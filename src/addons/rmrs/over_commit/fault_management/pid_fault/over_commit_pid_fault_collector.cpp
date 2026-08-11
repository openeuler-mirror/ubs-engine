/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "over_commit_pid_fault_collector.h"
#include <algorithm>
#include <set>
#include "ubse_com.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_node_controller.h"
#include "mem_borrow_executor.h"
#include "mempooling_message.h"
#include "mp_configuration.h"
#include "over_commit_pid_fault_state_store.h"
#include "over_commit_storage.h"
#include "rmrs_serialize.h"

namespace mempooling {

using namespace ubse::com;
using namespace ubse::mem::controller;
using namespace ubse::nodeController;
using rmrs::serialize::RmrsInStream;
using rmrs::serialize::RmrsOutStream;

static const std::string TAG = "[OverCommit][PidFault][Collector] ";
#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << TAG

MpResult PidFaultCollector::Collect(const std::string& faultNodeId, OverCommitFaultContext& context)
{
    LOG_INFO << "Collect start, faultNodeId=" << faultNodeId << ".";
    context.faultNodeId = faultNodeId;

    // 1a. 查账本
    MpResult ret = CollectDebtInfo(faultNodeId, context);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "CollectDebtInfo failed.";
        return ret;
    }

    if (context.affectedBorrowInNodeIds.empty()) {
        LOG_INFO << "No affected borrow-in nodes, nothing to do.";
        return MEM_POOLING_OK;
    }

    // 1c. RPC查询PID内存分布（同时作为可达性探测）
    ret = QueryPidMemDistribution(faultNodeId, context);
    if (ret == MEM_POOLING_FAULT_IPC_ERROR) {
        // 全部节点RPC失败: 通信面异常，本轮无法推进，上报IPC错误等上层下轮重试
        LOG_ERROR << "QueryPidMemDistribution failed on all nodes, IPC error.";
        return ret;
    }
    if (ret != MEM_POOLING_OK) {
        LOG_WARN << "QueryPidMemDistribution partially failed, continuing with available data.";
    }

    // 1d. 场景识别与约束标记
    MarkSocketConstraints(context);

    // 统计per-node故障numa总数
    size_t totalFaultNumas = 0;
    for (const auto& [nodeId, numaIds] : context.nodeToFaultNumaIds) {
        totalFaultNumas += numaIds.size();
    }
    LOG_INFO << "Collect end, affectedNodes=" << context.affectedBorrowInNodeIds.size()
             << ", faultNumas=" << totalFaultNumas << ".";
    return MEM_POOLING_OK;
}

// 打印账本采集详细信息
static void PrintDebtCollectDetail(const OverCommitFaultContext& context)
{
    size_t totalFaultNumas = 0;
    for (const auto& [nodeId, numaIds] : context.nodeToFaultNumaIds) {
        totalFaultNumas += numaIds.size();
    }
    LOG_DEBUG << "DebtInfo collected: borrowInNodes=" << context.affectedBorrowInNodeIds.size()
              << ", faultNumas=" << totalFaultNumas << ".";

    for (const auto& [nodeId, numaIds] : context.nodeToFaultNumaIds) {
        std::string numaListStr;
        for (const auto& numaId : numaIds) {
            numaListStr += std::to_string(numaId) + " ";
        }
        std::string borrowIdStr;
        auto it = context.nodeToBorrowIds.find(nodeId);
        if (it != context.nodeToBorrowIds.end()) {
            for (const auto& borrowId : it->second) {
                borrowIdStr += borrowId + " ";
            }
        }
        LOG_DEBUG << "BorrowInNode " << nodeId << ": faultNumaIds=[ " << numaListStr << "], borrowIds=[ " << borrowIdStr
                  << "].";
    }
}

MpResult PidFaultCollector::CollectDebtInfo(const std::string& faultNodeId, OverCommitFaultContext& context)
{
    std::vector<UbseNumaMemoryDebtInfo> debtInfos;

    // 查全集群账本（内置重试，账本在故障中可能暂时不可用）；一次查询两用：
    // 1) 筛与故障节点相关的记录做受影响分析；
    // 2) 全量记录收集实际借入节点集合，供Phase 3容量快照排除防借用成环（故障处理只减不增借入节点）
    if (MemBorrowExecutor::GetDebtInfosWithRetry(debtInfos) != MEM_POOLING_OK) {
        LOG_ERROR << "GetDebtInfosWithRetry failed.";
        return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
    }

    // 实际借入节点集合（已借入节点不能再当借出节点，否则借用成环）；
    // 全量账本记录都计入，不做场景专属过滤（成环防护口径宁多勿漏）
    for (const auto& debt : debtInfos) {
        context.actualBorrowInNodeIds.insert(debt.borrowNodeId);
    }
    LOG_INFO << "ActualBorrowInNodes collected from ledger: count=" << context.actualBorrowInNodeIds.size() << ".";

    if (debtInfos.empty()) {
        LOG_INFO << "No debt info for faultNodeId=" << faultNodeId << ".";
        return MEM_POOLING_OK;
    }

    // 提取受影响的借入节点和故障numa（per-node跟踪）
    std::set<std::string> borrowInNodeSet;
    std::unordered_map<std::string, std::set<uint16_t>> nodeFaultNumaMap;

    for (const auto& debt : debtInfos) {
        if (debt.lentNodeId != faultNodeId) {
            // 只关心借出方=故障节点的账本记录，其余记录与本次故障无关
            LOG_DEBUG << "Skip debt borrowId=" << debt.name << ": lentNodeId=" << debt.lentNodeId
                      << " is not fault node.";
            continue;
        }
        // 半边账本: remoteNumaId无效时跳过+告警
        if (debt.remoteNumaId <= 0) {
            LOG_WARN << "Half-ledger: remoteNumaId invalid=" << debt.remoteNumaId << ", borrowId=" << debt.name
                     << ", skip.";
            continue;
        }
        // 节点状态过滤: 仅采信WORKING状态的借入节点
        auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(debt.borrowNodeId);
        if (nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_WORKING &&
            nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_FAULT) {
            LOG_WARN << "BorrowInNode " << debt.borrowNodeId
                     << " state=" << static_cast<uint32_t>(nodeInfo.clusterState) << ", not WORKING, skip.";
            continue;
        }
        borrowInNodeSet.insert(debt.borrowNodeId);
        nodeFaultNumaMap[debt.borrowNodeId].insert(static_cast<uint16_t>(debt.remoteNumaId));
        LOG_DEBUG << "Accept debt: borrowId=" << debt.name << ", borrowNode=" << debt.borrowNodeId
                  << ", remoteNumaId=" << debt.remoteNumaId << ", uid=" << debt.uid << ".";

        // 按借入节点分组记录borrowId（用于后续归还），同时按故障numa分组（直接归还按numa精确归还）
        context.nodeToBorrowIds[debt.borrowNodeId].push_back(debt.name);
        context.nodeToNumaBorrowIds[debt.borrowNodeId][static_cast<uint16_t>(debt.remoteNumaId)].push_back(debt.name);

        // 记录借用方用户信息（master代借时使用，取该节点第一条有效记录）
        if (context.nodeToBorrowUser.find(debt.borrowNodeId) == context.nodeToBorrowUser.end()) {
            BorrowUserInfo userInfo;
            userInfo.uid = debt.uid;
            userInfo.username = debt.username;
            if (!debt.borrowSocketIdList.empty()) {
                userInfo.socketId = static_cast<int16_t>(debt.borrowSocketIdList[0]);
            }
            context.nodeToBorrowUser[debt.borrowNodeId] = std::move(userInfo);
        }
    }

    context.affectedBorrowInNodeIds.assign(borrowInNodeSet.begin(), borrowInNodeSet.end());
    // 写入per-node faultNumaIds
    for (const auto& [nodeId, numaSet] : nodeFaultNumaMap) {
        context.nodeToFaultNumaIds[nodeId].assign(numaSet.begin(), numaSet.end());
    }

    PrintDebtCollectDetail(context);
    return MEM_POOLING_OK;
}

// RPC响应回调
static void FaultPidQueryResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t retCode)
{
    if (ctx == nullptr) {
        return;
    }
    auto* result = static_cast<FaultPidQueryResponse*>(ctx);
    result->retCode = retCode;
    if (retCode != MEM_POOLING_OK || respData.data == nullptr || respData.len == 0) {
        return;
    }
    RmrsInStream in(respData.data, respData.len);
    FaultPidQueryResponseDeserialization(in, *result);
}

MpResult PidFaultCollector::QueryPidMemDistribution(const std::string& faultNodeId, OverCommitFaultContext& context)
{
    MpResult overallRet = MEM_POOLING_OK;
    size_t successCount = 0;

    for (const auto& borrowInNodeId : context.affectedBorrowInNodeIds) {
        // 构造RPC请求（携带场景类型 + per-node故障numa列表），由借入节点自行采集PID内存分布
        FaultPidQueryRequest request;
        request.sceneType = MpConfiguration::GetInstance().GetMpSceneType();
        auto it = context.nodeToFaultNumaIds.find(borrowInNodeId);
        if (it != context.nodeToFaultNumaIds.end()) {
            request.faultNumaIds = it->second;
        }
        std::string numaListStr;
        for (const auto& numaId : request.faultNumaIds) {
            numaListStr += std::to_string(numaId) + " ";
        }
        LOG_DEBUG << "Send PID query to node " << borrowInNodeId
                  << ": sceneType=" << static_cast<uint32_t>(request.sceneType) << ", faultNumaIds=[ " << numaListStr
                  << "].";

        RmrsOutStream builder;
        FaultPidQueryRequestSerialization(builder, request);

        UbseComEndpoint endpoint = {.moduleId = MP_MODULE_CODE,
                                    .serviceId = message::OPCODE_OVER_COMMIT_FAULT_PID_QUERY,
                                    .address = borrowInNodeId};

        UbseByteBuffer reqData = {
            .data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = [](uint8_t* data) {
                delete[] data;
            }};

        FaultPidQueryResponse response;
        uint32_t rpcRet = UbseRpcSend(endpoint, reqData, &response, FaultPidQueryResHandler);

        // 更新可达性: RPC成功即视为ubse可达（ubturbo/smap探活后移至执行阶段迁移前检查），
        // 不可达节点的任务后续在决策阶段会被标为DEFER延后处理
        BorrowNodeReachability reachability;
        reachability.nodeId = borrowInNodeId;
        reachability.ubseReachable = (rpcRet == MEM_POOLING_OK && response.retCode == MEM_POOLING_OK);
        context.nodeReachability[borrowInNodeId] = reachability;
        LOG_DEBUG << "Node " << borrowInNodeId
                  << " reachability: ubseReachable=" << (reachability.ubseReachable ? "true" : "false") << ".";

        if (rpcRet != MEM_POOLING_OK || response.retCode != MEM_POOLING_OK) {
            LOG_WARN << "PID query to node " << borrowInNodeId << " failed, rpcRet=" << rpcRet
                     << ", respRet=" << response.retCode << ".";
            overallRet = MEM_POOLING_ERROR;
            continue;
        }

        // 存储PID内存分布
        context.nodeToPidMemInfos[borrowInNodeId] = response.pidMemDistribution;
        // 待恢复故障numa（smap纳管查询失败且采集占用非0）: task_builder本轮对其不建task不归还
        if (!response.pendingFaultNumaIds.empty()) {
            context.nodeToPendingFaultNumaIds[borrowInNodeId].insert(response.pendingFaultNumaIds.begin(),
                                                                     response.pendingFaultNumaIds.end());
            LOG_WARN << "Node " << borrowInNodeId << " has " << response.pendingFaultNumaIds.size()
                     << " pending fault numa(s), deferred to next round.";
        }
        successCount++;
        LOG_INFO << "PID query to node " << borrowInNodeId
                 << " success, pidCount=" << response.pidMemDistribution.size() << ".";
    }

    if (successCount == 0 && !context.affectedBorrowInNodeIds.empty()) {
        // 全部节点失败（区别于部分失败降级）: 通信面异常，升级为IPC错误
        overallRet = MEM_POOLING_FAULT_IPC_ERROR;
    }
    return overallRet;
}

void PidFaultCollector::MarkSocketConstraints(OverCommitFaultContext& context)
{
    MpSceneType sceneType = MpConfiguration::GetInstance().GetMpSceneType();
    LOG_DEBUG << "MarkSocketConstraints start: sceneType=" << static_cast<uint32_t>(sceneType) << ".";

    for (auto& [nodeId, pidMemInfos] : context.nodeToPidMemInfos) {
        for (auto& info : pidMemInfos) {
            if (sceneType == MpSceneType::CONTAINER_SCENE) {
                // 容器场景：查询绑定类型（同socket约束由bindType推导: BIND_MULTIPLE=无约束）
                NumaBindType bindType = NumaBindType::BIND_SINGLE;
                OverCommitStorage::Instance().GetNumaBindType(nodeId, bindType);
                info.bindType = bindType;
            } else {
                // 虚机场景：始终有同socket约束
                info.bindType = NumaBindType::BIND_SINGLE;
            }
            LOG_DEBUG << "Mark constraint: node=" << nodeId << ", pid=" << info.pid
                      << ", bindType=" << static_cast<uint32_t>(info.bindType) << ", socketId=" << info.socketId << ".";
        }
    }
}

} // namespace mempooling
