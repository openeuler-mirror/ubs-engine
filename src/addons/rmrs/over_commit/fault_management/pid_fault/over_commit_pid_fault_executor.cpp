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

#include "over_commit_pid_fault_executor.h"
#include <algorithm>
#include <future>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include "ubse_com.h"
#include "ubse_logger.h"
#include "mempool_borrow_module.h"
#include "mempooling_message.h"
#include "over_commit_fault_memid_module.h"
#include "over_commit_pid_fault_error_util.h"
#include "over_commit_pid_fault_common.h"
#include "over_commit_pid_fault_state_store.h"
#include "rmrs_serialize.h"

namespace mempooling {

using namespace ubse::com;
using rmrs::serialize::RmrsInStream;
using rmrs::serialize::RmrsOutStream;

static const std::string TAG = "[OverCommit][PidFault][Executor] ";
#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << TAG

// UBSE单笔借用下限4MB: 实际借用量=max(需求,4MB)，与借用层LiftFaultBorrowSizeKB口径一致，
// 随task下发供借入节点设置smap远端numa借用信息（免重复查账本）
static constexpr uint64_t MIN_BORROW_SIZE_KB = 4 * 1024;


// RPC响应回调
static void FaultPidExecuteResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t retCode)
{
    if (ctx == nullptr) {
        return;
    }
    auto* result = static_cast<FaultPidExecuteResponse*>(ctx);
    result->retCode = retCode;
    if (retCode != MEM_POOLING_OK || respData.data == nullptr || respData.len == 0) {
        return;
    }
    RmrsInStream in(respData.data, respData.len);
    if (FaultPidExecuteResponseDeserialization(in, *result) != MEM_POOLING_OK) {
        // 响应截断/损坏: 丢弃部分填充数据，按失败处理等下轮重试
        LOG_ERROR << "FaultPidExecuteResHandler deserialization incomplete, drop partial data.";
        *result = FaultPidExecuteResponse{};
        result->retCode = MEM_POOLING_ERROR;
    }
}

MpResult PidFaultExecutor::BorrowForGroup(const FaultExecutePlan& plan, const PlanBorrowGroup& group,
                                          std::string& newBorrowId, uint16_t& newRemoteNumaId,
                                          uint64_t& newBorrowSizeKB)
{
    // srcNid=借入节点（借用主体是借入节点，master仅代为执行）；约束粒度=单次借用，socket取自本组；
    // srcNumaId=组归属本地numa，借用层写入usrInfo前2字节，virt_agent据此归属水线归还
    SrcMemoryBorrowParam srcParam;
    srcParam.srcNid = plan.borrowInNodeId;
    srcParam.srcSocketId = group.hasSameSocketConstraint ? group.constraintSocketId : plan.borrowUser.socketId;
    srcParam.srcNumaId = static_cast<int16_t>(group.localNumaId);
    srcParam.uid = plan.borrowUser.uid;
    srcParam.username = plan.borrowUser.username;
    LOG_DEBUG << "BorrowForGroup: plan=" << plan.planId << ", srcNid=" << srcParam.srcNid
              << ", srcSocketId=" << srcParam.srcSocketId << ", srcNumaId=" << srcParam.srcNumaId
              << ", constrained=" << group.hasSameSocketConstraint << ", demandKB=" << group.demandKB
              << ", uid=" << srcParam.uid << ", candidateLenderNodes=" << JoinToString(group.candidateLenderNodes)
              << ".";

    // 故障借用层入参单位KB（低于4MB下限的取整与KB→字节换算由借用层统一处理）
    std::vector<uint64_t> borrowSizes{group.demandKB};

    WaterMark waterMark;
    if (OverCommitFaultMemIdModule::Instance().GetWaterMark(waterMark) != MEM_POOLING_OK) {
        LOG_ERROR << "GetWaterMark failed.";
        return MEM_POOLING_FAULT_RESOURCE_COLLECT_ERROR;
    }

    // 优先用BFD预分配的借出节点（slotIds钉住），失败回退全候选重试一次；
    // PID专用借用函数: usrInfo写本地numaId（容器/虚机协议），不用裸机process_mem协议
    MemBorrowExecuteResult borrowResult;
    auto ret = MempoolBorrowModule::MemBorrowExecuteForPidFaultInOverCommit(
        srcParam, borrowSizes,
        mempooling::WaterMark({.highWaterMark = waterMark.highWaterMark, .lowWaterMark = waterMark.lowWaterMark}),
        borrowResult, group.candidateLenderNodes);
    if ((ret != MEM_POOLING_OK || borrowResult.borrowIds.empty() || borrowResult.presentNumaId.empty()) &&
        !group.candidateLenderNodes.empty()) {
        LOG_WARN << "Borrow with BFD candidate failed for plan " << plan.planId << ", fallback to full candidates.";
        borrowResult = MemBorrowExecuteResult();
        ret = MempoolBorrowModule::MemBorrowExecuteForPidFaultInOverCommit(
            srcParam, borrowSizes,
            mempooling::WaterMark({.highWaterMark = waterMark.highWaterMark, .lowWaterMark = waterMark.lowWaterMark}),
            borrowResult);
    }
    if (ret != MEM_POOLING_OK || borrowResult.borrowIds.empty() || borrowResult.presentNumaId.empty()) {
        LOG_ERROR << "MemBorrowExecuteForPidFaultInOverCommit failed for plan " << plan.planId
                  << ", group socket=" << group.constraintSocketId << ", reason=" << static_cast<uint32_t>(ret) << ".";
        // 透传借用层失败原因: 内存不足(n=4)/借用执行异常(n=6)；ret为OK但结果为空时归为借用执行异常
        return ret != MEM_POOLING_OK ? ret : MEM_POOLING_FAULT_BORROW_MEM_ERROR;
    }

    newBorrowId = borrowResult.borrowIds[0];
    newRemoteNumaId = borrowResult.presentNumaId[0];
    // 实际借用量以借用层返回为准（UBSE按block粒度取整可能大于需求量）；缺失时兜底需求量取整口径
    newBorrowSizeKB = borrowResult.borrowedSizesKB.empty() ?
                          std::max(group.demandKB, static_cast<uint64_t>(MIN_BORROW_SIZE_KB)) :
                          borrowResult.borrowedSizesKB[0];
    LOG_INFO << "Borrow success for plan " << plan.planId << " group(socket=" << group.constraintSocketId
             << "): newBorrowId=" << newBorrowId << ", newRemoteNumaId=" << newRemoteNumaId
             << ", demandKB=" << group.demandKB << ", actualBorrowKB=" << newBorrowSizeKB << ".";
    return MEM_POOLING_OK;
}

void PidFaultExecutor::SerialBorrowOnMaster(const std::string& faultNodeId, std::vector<FaultExecutePlan>& plans,
                                            std::set<MpResult>& failCodes)
{
    // 以借用组为单元按需求降序串行借用（与决策层BFD分配次序一致，大需求优先拿大块）；
    // 同节点跨socket约束的task分属不同组，各自独立借一笔，互不影响
    struct GroupRef {
        size_t planIdx;
        size_t groupIdx;
    };
    std::vector<GroupRef> borrowOrder;
    for (size_t i = 0; i < plans.size(); ++i) {
        if (plans[i].planType != PlanType::EXECUTE) {
            LOG_DEBUG << "SerialBorrow skip plan " << plans[i].planId << ": type=DEFER.";
            continue;
        }
        for (size_t g = 0; g < plans[i].borrowGroups.size(); ++g) {
            if (plans[i].borrowGroups[g].demandKB > 0 && !plans[i].borrowGroups[g].taskIds.empty()) {
                borrowOrder.push_back({i, g});
            } else {
                // 需求为0/无成员: 决策阶段未分配到容量被清空的组，本轮不借
                LOG_DEBUG << "SerialBorrow skip group of " << plans[i].planId
                          << ": demandKB=" << plans[i].borrowGroups[g].demandKB
                          << ", taskIds=" << plans[i].borrowGroups[g].taskIds.size() << ".";
            }
        }
    }
    std::sort(borrowOrder.begin(), borrowOrder.end(), [&plans](const GroupRef& a, const GroupRef& b) {
        return plans[a.planIdx].borrowGroups[a.groupIdx].demandKB > plans[b.planIdx].borrowGroups[b.groupIdx].demandKB;
    });
    LOG_DEBUG << "SerialBorrowOnMaster: borrowOrder groups=" << borrowOrder.size() << ".";

    for (const auto& ref : borrowOrder) {
        auto& plan = plans[ref.planIdx];
        auto& group = plan.borrowGroups[ref.groupIdx];
        std::unordered_set<std::string> groupIdSet(group.taskIds.begin(), group.taskIds.end());

        std::string newBorrowId;
        uint16_t newRemoteNumaId = 0;
        uint64_t borrowedKB = 0;
        MpResult borrowRet = BorrowForGroup(plan, group, newBorrowId, newRemoteNumaId, borrowedKB);
        if (borrowRet != MEM_POOLING_OK) {
            // 借用失败: 仅剔除本组NEW task等下轮重试，其他组/RESUME/直接归还照常下发；失败码记入集合供出口聚合
            (void)failCodes.insert(borrowRet);
            plan.tasks.erase(std::remove_if(plan.tasks.begin(), plan.tasks.end(),
                                            [&groupIdSet](const MigrationTask& t) {
                                                return t.phase == TaskPhase::NONE && groupIdSet.count(t.taskId) > 0;
                                            }),
                             plan.tasks.end());
            LOG_WARN << "Plan " << plan.planId << " group(socket=" << group.constraintSocketId
                     << ") borrow failed, ret=" << static_cast<uint32_t>(borrowRet) << ", NEW tasks deferred.";
            continue;
        }

        // 借用结果写回本组全部NEW task并持久化BORROWED（RPC丢失响应时下轮可RESUME，避免借用泄漏）；
        // borrowedKB为实际借用量（BorrowForGroup输出），分大页/smap账目据此设置才能覆盖全部借入内存
        for (auto& task : plan.tasks) {
            if (task.phase != TaskPhase::NONE || groupIdSet.count(task.taskId) == 0) {
                continue; // 非本组或RESUME task不共享本笔借用
            }
            task.phase = TaskPhase::BORROWED;
            task.newBorrowId = newBorrowId;
            task.newRemoteNumaId = newRemoteNumaId;
            task.newBorrowSizeKB = borrowedKB;
            LOG_DEBUG << "Task " << task.taskId << " -> BORROWED, newBorrowId=" << newBorrowId
                      << ", newRemoteNumaId=" << newRemoteNumaId << ", persisting.";

            TaskPersistState persistState;
            persistState.taskId = task.taskId;
            persistState.borrowInNodeId = plan.borrowInNodeId;
            persistState.phase = TaskPhase::BORROWED;
            persistState.newBorrowId = newBorrowId;
            persistState.newRemoteNumaId = newRemoteNumaId;
            persistState.newBorrowSizeKB = borrowedKB;
            persistState.oldBorrowIds = task.relatedBorrowIds;
            persistState.pids = task.pids;
            persistState.migrationSizeKB = task.migrationSizeKB;
            if (PidFaultStateStore::Instance().SaveTaskState(faultNodeId, persistState) != MEM_POOLING_OK) {
                // 持久化失败: 数据一致性问题（借用已成功但状态未落盘），归为归还内存类错误码
                LOG_ERROR << "SaveTaskState failed for task " << task.taskId << ".";
                (void)failCodes.insert(MEM_POOLING_FAULT_RETURN_MEM_ERROR);
            }
        }
    }
}

NodeExecuteResult PidFaultExecutor::DispatchNodePlan(const std::string& faultNodeId, const FaultExecutePlan& plan)
{
    NodeExecuteResult result;
    result.borrowInNodeId = plan.borrowInNodeId;
    result.success = false;

    LOG_INFO << "DispatchNodePlan: node=" << plan.borrowInNodeId << ", tasks=" << plan.tasks.size()
             << ", directReturns=" << plan.directReturnBorrowIds.size() << ".";

    // 任务组一次RPC下发，借入节点上自发并行执行
    FaultPidExecuteRequest request;
    request.faultNodeId = faultNodeId;
    request.tasks = plan.tasks;
    request.directReturnBorrowIds = plan.directReturnBorrowIds;
    for (const auto& task : request.tasks) {
        LOG_DEBUG << "Dispatch task to " << plan.borrowInNodeId << ": taskId=" << task.taskId
                  << ", phase=" << static_cast<uint32_t>(task.phase) << ", newBorrowId=" << task.newBorrowId
                  << ", newRemoteNumaId=" << task.newRemoteNumaId << ", pids=" << JoinToString(task.pids) << ".";
    }

    RmrsOutStream builder;
    FaultPidExecuteRequestSerialization(builder, request);

    UbseComEndpoint endpoint = {.moduleId = MP_MODULE_CODE,
                                .serviceId = message::OPCODE_OVER_COMMIT_FAULT_PID_EXECUTE,
                                .address = plan.borrowInNodeId};

    UbseByteBuffer reqData = {
        .data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = [](uint8_t* data) {
            delete[] data;
        }};

    FaultPidExecuteResponse response;
    uint32_t rpcRet = UbseRpcSend(endpoint, reqData, &response, FaultPidExecuteResHandler);
    if (rpcRet != MEM_POOLING_OK) {
        // RPC失败: 已持久化的BORROWED状态保留，下轮RESUME；通信面异常记IPC错误码
        LOG_ERROR << "RPC to " << plan.borrowInNodeId << " failed, ret=" << rpcRet << ".";
        result.errorCode = MEM_POOLING_FAULT_IPC_ERROR;
        return result;
    }

    result.taskResults = response.taskResults;
    result.freedBorrowIds = response.freedBorrowIds;
    result.success = (response.retCode == MEM_POOLING_OK);
    result.errorCode = response.retCode; // 对端已按优先级聚合本轮借入节点侧失败码
    LOG_INFO << "DispatchNodePlan done: node=" << plan.borrowInNodeId << ", retCode=" << response.retCode
             << ", taskResults=" << result.taskResults.size() << ", freed=" << result.freedBorrowIds.size() << ".";
    return result;
}

void PidFaultExecutor::ProcessNodeResult(const std::string& faultNodeId, const FaultExecutePlan& plan,
                                         const NodeExecuteResult& result, std::set<MpResult>& failCodes)
{
    // taskId → task索引（响应只带taskId，需回查下发时的task完整信息才能持久化）
    std::unordered_map<std::string, const MigrationTask*> taskMap;
    for (const auto& task : plan.tasks) {
        taskMap[task.taskId] = &task;
    }
    std::unordered_set<std::string> freedIdSet(result.freedBorrowIds.begin(), result.freedBorrowIds.end());
    LOG_DEBUG << "ProcessNodeResult: node=" << plan.borrowInNodeId << ", taskResults=" << result.taskResults.size()
              << ", freedBorrowIds=" << JoinToString(result.freedBorrowIds) << ".";

    for (const auto& taskResult : result.taskResults) {
        auto it = taskMap.find(taskResult.taskId);
        if (it == taskMap.end()) {
            LOG_WARN << "Unknown taskId in response: " << taskResult.taskId << ".";
            continue;
        }
        const MigrationTask& task = *it->second;

        // per-task失败码记入集合（迁移失败/归还失败等，由借入节点侧按步骤回填），供出口聚合
        if (taskResult.retCode != MEM_POOLING_OK) {
            (void)failCodes.insert(taskResult.retCode);
        }

        if (taskResult.completedPhase == TaskPhase::COMPLETED) {
            PidFaultStateStore::Instance().RemoveCompletedTask(faultNodeId, task.taskId);
            LOG_INFO << "Task " << task.taskId << " COMPLETED, state removed.";
            continue;
        }
        if (taskResult.completedPhase >= TaskPhase::BORROWED) {
            // 未走完链路: 更新持久化状态（含迁移完成但归还未完的MIGRATED），下轮RESUME继续推进
            TaskPersistState persistState;
            persistState.taskId = task.taskId;
            persistState.borrowInNodeId = plan.borrowInNodeId;
            persistState.phase = taskResult.completedPhase;
            persistState.newBorrowId = task.newBorrowId;
            persistState.newRemoteNumaId = task.newRemoteNumaId;
            persistState.newBorrowSizeKB = task.newBorrowSizeKB;
            persistState.oldBorrowIds = task.relatedBorrowIds;
            persistState.pids = task.pids;
            persistState.migrationSizeKB = task.migrationSizeKB;
            for (const auto& borrowId : task.relatedBorrowIds) {
                if (freedIdSet.count(borrowId) > 0) {
                    // 记录已归还的旧borrowId，下轮条件归还不再重复操作
                    persistState.freedOldBorrowIds.insert(borrowId);
                }
            }
            if (PidFaultStateStore::Instance().SaveTaskState(faultNodeId, persistState) != MEM_POOLING_OK) {
                // 持久化失败: 数据一致性问题，归为归还内存类错误码
                LOG_ERROR << "SaveTaskState failed for task " << task.taskId << ".";
                (void)failCodes.insert(MEM_POOLING_FAULT_RETURN_MEM_ERROR);
            }
            LOG_DEBUG << "Task " << task.taskId
                      << " persisted at phase=" << static_cast<uint32_t>(taskResult.completedPhase)
                      << ", freedOldBorrowIds=" << persistState.freedOldBorrowIds.size() << ".";
        } else {
            // NONE: 借用前失败（本轮未借用），无状态可存，下轮作为NEW任务重新进入
            LOG_DEBUG << "Task " << task.taskId << " stays NONE, will retry as NEW next round.";
        }
        LOG_INFO << "Task " << task.taskId << " retCode=" << taskResult.retCode
                 << ", phase=" << static_cast<uint32_t>(taskResult.completedPhase) << ".";
    }
}

MpResult PidFaultExecutor::ExecuteAll(const std::string& faultNodeId, std::vector<FaultExecutePlan>& plans)
{
    LOG_INFO << "ExecuteAll start, plans=" << plans.size() << ".";

    // 本轮失败码集合: 各阶段失败原因都记入，出口按排障优先级聚合为一个码上报
    std::set<MpResult> failCodes;

    // Phase A: master串行借用（避免多借入节点并行借用导致碎片化）
    SerialBorrowOnMaster(faultNodeId, plans, failCodes);

    // Phase B: 过滤可下发计划（DEFER/空计划跳过）
    std::vector<const FaultExecutePlan*> dispatchablePlans;
    for (const auto& plan : plans) {
        if (plan.planType == PlanType::DEFER) {
            LOG_DEBUG << "Plan " << plan.planId << " DEFER (unreachable), skip dispatch.";
            continue;
        }
        if (plan.tasks.empty() && plan.directReturnBorrowIds.empty()) {
            // 借用全部失败且无直接归还时可能出现空计划
            LOG_DEBUG << "Plan " << plan.planId << " empty after borrow, skip dispatch.";
            continue;
        }
        dispatchablePlans.push_back(&plan);
    }

    if (dispatchablePlans.empty()) {
        // 无可下发计划: 借用失败码已记入failCodes；若仅剩DEFER（节点不可达）则归为IPC错误
        if (failCodes.empty()) {
            (void)failCodes.insert(MEM_POOLING_FAULT_IPC_ERROR);
        }
        LOG_ERROR << "No dispatchable plans, failCodes=" << JoinFaultErrorCodes(failCodes) << ".";
        return AggregateFaultErrorCodes(failCodes);
    }

    // 按借入节点并行下发（每节点单次RPC）
    std::vector<std::future<NodeExecuteResult>> futures;
    for (const auto* plan : dispatchablePlans) {
        futures.push_back(std::async(std::launch::async,
                                     [this, &faultNodeId, plan]() { return DispatchNodePlan(faultNodeId, *plan); }));
    }

    // 等待所有完成，汇总结果并持久化
    int successCount = 0;
    int failCount = 0;
    for (size_t i = 0; i < futures.size(); ++i) {
        NodeExecuteResult result = futures[i].get();
        ProcessNodeResult(faultNodeId, *dispatchablePlans[i], result, failCodes);
        if (result.success) {
            successCount++;
        } else {
            failCount++;
            // 节点级失败码（RPC失败=IPC错误；否则为对端聚合码）；非故障码的异常统一归为借用执行异常
            MpResult nodeCode = result.errorCode;
            if (GetFaultErrorCodePriority(nodeCode) == 0) {
                nodeCode = MEM_POOLING_FAULT_BORROW_MEM_ERROR;
            }
            (void)failCodes.insert(nodeCode);
            LOG_WARN << "Node " << result.borrowInNodeId << " execution failed, code=" << nodeCode << ".";
        }
    }

    // 有DEFER计划（不可达节点）视为未收敛，记IPC错误码触发上层下轮重试
    for (const auto& plan : plans) {
        if (plan.planType == PlanType::DEFER) {
            LOG_DEBUG << "Plan " << plan.planId << " was DEFER, mark overall result as partial fail.";
            (void)failCodes.insert(MEM_POOLING_FAULT_IPC_ERROR);
        }
    }

    MpResult aggregated = AggregateFaultErrorCodes(failCodes);
    LOG_INFO << "ExecuteAll end, success=" << successCount << ", fail=" << failCount
             << ", failCodes=" << JoinFaultErrorCodes(failCodes) << ", aggregated=" << aggregated << ".";
    return aggregated;
}

} // namespace mempooling
