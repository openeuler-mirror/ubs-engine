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

#include "fault_node_module.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <set>
#include <unordered_map>
#include <chrono>
#include <thread>

#include "exporter.h"
#include "export_type.h"
#include "fault_memid_helper.h"
#include "mem_borrow_executor.h"
#include "mem_manager.h"
#include "mempooling_message.h"
#include "mp_configuration.h"
#include "mp_json_util.h"
#include "rmrs_serialize.h"
#include "ubse_com.h"
#include "ubse_mem_controller.h"
#include "ubse_error.h"
#include "ubse_pointer_process.h"

namespace mempooling {
using namespace mempooling::smap;
using namespace ubse::log;
using namespace ubse::com;
using namespace ubse::mem::controller;
using namespace mempooling::message;
using namespace rmrs::serialize;

constexpr int SMAP_OK = 0;
constexpr int SMAP_ERROR = 1;
constexpr auto DELETERE_KIND = "MEM";
constexpr uint64_t KB_1024 = 1024ULL;

#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)

MpResult FaultNodeModule::DetermineNodeTypeOverCommit(const std::string nodeId, NodeType &nodeType)
{
    MpResult ret = MEM_POOLING_OK;
    std::vector<UbseNumaMemoryDebtInfo> debtInfos;
    UbseResult retErrorCode = UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos);
    if (retErrorCode != UBSE_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager][OverCommit] Call UbseGetNumaMemDebtInfoWithNode failed.";
        nodeType = NodeType::ABNORMAL;
        return MEM_POOLING_ERROR;
    }
    if (debtInfos.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager][OverCommit] Node:" << nodeId << " has no debt infos, ret=" << ret << ".";
        nodeType = NodeType::NO_RECORD;
        return ret;
    }
    // nodeId与记录中借出节点相同
    if (debtInfos[0].lentNodeId == nodeId) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager][OverCommit] Node:" << nodeId << " NodeType: BORROW_OUT.";
        nodeType = NodeType::BORROW_OUT;
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager][OverCommit] Node:" << nodeId << " NodeType: BORROW_IN.";
        nodeType = NodeType::BORROW_IN;
    }
    return ret;
}

MpResult FaultNodeModule::DetermineNodeTypeFragment(const std::string nodeId, NodeType &nodeType)
{
    MpResult ret = MEM_POOLING_OK;
    std::vector<BorrowRecord> fragMentFaultBorrowRecords;
    UbseResult retErrorCode =
        BorrowRecordHelper::Instance().GetFragMentFaultBorrowRecords(nodeId, fragMentFaultBorrowRecords);
    if (retErrorCode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] GetFragMentFaultBorrowRecords failed.";
        nodeType = NodeType::ABNORMAL;
        return MEM_POOLING_ERROR;
    }
    if (fragMentFaultBorrowRecords.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] Node:" << nodeId << " has no debt infos, ret=" << ret << ".";
        nodeType = NodeType::NO_RECORD;
        return ret;
    }
    // nodeId与记录中借出节点相同
    if (fragMentFaultBorrowRecords[0].lentNode == nodeId) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] Node:" << nodeId << " NodeType: BORROW_OUT.";
        nodeType = NodeType::BORROW_OUT;
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] Node:" << nodeId << " NodeType: BORROW_IN.";
        nodeType = NodeType::BORROW_IN;
    }
    return ret;
}

MpResult FaultNodeModule::DetermineNodeType(const std::string nodeId, NodeType &nodeType)
{
    std::vector<BorrowRecord> borrowRecords;
    auto ret = BorrowRecordHelper::Instance().CollectBorrowRecords(nodeId, borrowRecords);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] Call QueryMemRecoreds failed.";
        nodeType = NodeType::ABNORMAL;
        return ret;
    }
    if (borrowRecords.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] Node:" << nodeId << "has no borrow records.";
        nodeType = NodeType::NO_RECORD;
        return MEM_POOLING_OK;
    }
    // nodeId与记录中借出节点相同
    if (borrowRecords[0].lentNode == nodeId) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] Node:" << nodeId << " NodeType: BORROW_OUT.";
        nodeType = NodeType::BORROW_OUT;
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] Node:" << nodeId << " NodeType: BORROW_IN.";
        nodeType = NodeType::BORROW_IN;
    }
    return ret;
}

bool FaultNodeModule::QueryNumaAllPid(uint16_t numaId, std::vector<pid_t> &pidList)
{
    std::vector<mempooling::exportV2::VmDomainInfo> vmDomainInfos;
    uint32_t queryVmRes = mempooling::exportV2::Exporter::GetVmInfoImmediately(vmDomainInfos);
    if (queryVmRes != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] [FaultLentNode] CurNode query vm error.";
        return false;
    }
    if (vmDomainInfos.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] [FaultLentNode] CurNode vm empty.";
        return false;
    }
    for (const auto &vmDomainInfo : vmDomainInfos) {
        // 查找远端 NUMA
        for (const auto &kv : vmDomainInfo.numaInfo) {
            const mempooling::exportV2::VmDomainNumaInfo &numa = kv.second;
            if (!numa.isLocal && numa.numaId == numaId) {
                pidList.push_back(vmDomainInfo.metaData.pid);
                break; // 一个 VM 最多只有一个远端 NUMA
            }
        }
    }
    if (pidList.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] [FaultLentNode] PID empty.";
        return false;
    }
    return true;
}

bool FaultNodeModule::AllocateHugePage(uint16_t numaId, uint64_t hugePageMemSize)
{
    std::vector<uint64_t> numaIdList{static_cast<uint64_t>(numaId)};
    std::vector<uint64_t> borrowSizeList{hugePageMemSize};
    if (MpSmapHelper::GetInstance().AllocateHugePages(numaIdList, borrowSizeList)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] [FaultLentNode] No vm AllocateHugePages failed.";
        return false;
    }
    return true;
}

bool FaultNodeModule::SwitchMigrateForNumaVm(std::vector<pid_t> pidList, int enable)
{
    // 预留字段暂无意义
    int flags = 0;
    // 关闭pid级别冷热迁移
    int res = MpSmapHelper::SmapEnableProcessMigrateHelper(pidList.data(), pidList.size(), enable, flags);
    if (res != SMAP_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] [FaultLentNode] SmapEnableProcessMigrateHelper failed.";
        return false;
    }
    return true;
}

bool FaultNodeModule::GenerateMigrateNumaMsgList(NumaReplaceReturnMsg rpcMsg, std::vector<MigrateNumaMsg> &msgList)
{
    for (MigrateBorrowRecord record : rpcMsg.migrateBorrowRecordList) {
        if (record.memIdList.size() > MAX_NR_MIGNUMA) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] [FaultLentNode] The size of record is invalid.";
            return false;
        }
        MigrateNumaMsg msg;
        MigrateNumaPayload payloadList;
        for (size_t i = 0; i < record.memIdList.size(); i++) {
            msg.memids[i] = record.memIdList[i];
        }
        msg.srcNid = rpcMsg.srcNid;
        msg.destNid = rpcMsg.destNid;
        msg.count = static_cast<int>(record.memIdList.size());
        msgList.push_back(msg);
    }
    return true;
}

bool FaultNodeModule::ExecMigrateRemoteNumaToNuma(NumaReplaceReturnMsg rpcMsg, std::vector<MigrateNumaMsg> msgList)
{
    if (msgList.size() == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] [FaultLentNode] InPutParam empty.";
        return false;
    }
    // 单远端numa
    std::vector<uint64_t> numaIdList{static_cast<uint64_t>(msgList[0].destNid)};
    // 查看需要分配大页数量
    if (rpcMsg.destNid != msgList[0].destNid) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] [FaultLentNode] Remote NUMAID ERROR.";
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] [FaultLentNode] Param destNid=" << rpcMsg.destNid
            << ", msgList[0].destNid=" << msgList[0].destNid << ".";
        return false;
    }

    // 计算需要分配大页的内存大小，单位b
    uint64_t hugePageMemSize = 0;
    for (size_t i = 0; i < rpcMsg.BorrowExecuteParamVec.size(); i++) {
        hugePageMemSize += rpcMsg.BorrowExecuteParamVec[i].memSize;
    }
    hugePageMemSize = hugePageMemSize * KB_1024;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] [FaultLentNode] Param hugePageMemSize=" << hugePageMemSize << "B, srcNid=" << rpcMsg.srcNid
        << ", destNid=" << rpcMsg.destNid << ".";

    // 全量分大页
    std::vector<uint64_t> borrowSizeList{hugePageMemSize};
    MpResult res = MpSmapHelper::GetInstance().AllocateHugePages(numaIdList, borrowSizeList);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] [FaultLentNode] AllocateHugePages failed.";
        return false;
    }
    for (MigrateNumaMsg msg : msgList) {
        res = MpSmapHelper::SmapMigrateRemoteNuma(msg);
        if (res != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] [FaultLentNode] SmapMigrateRemoteNuma failed, srcNumaId " << msg.srcNid
                << ", destNumaId " << msg.destNid;
            return false;
        }
    }
    return true;
}

MpResult FaultNodeModule::GetBorrowNodeInfo(std::string nodeId, std::vector<BorrowRecord> &borrowRecords)
{
    UbseResult res = BorrowRecordHelper::Instance().GetFragMentFaultBorrowRecords(nodeId, borrowRecords);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] [FaultLentNode] GetFragMentFaultBorrowRecords failed, nodeId=" << nodeId;
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult FaultNodeModule::GetBorrowAbleNodeIdList(std::string curDealNodeId,
                                                  std::vector<std::string> &borrowAbleNodeIdList)
{
    // 目前架构能查到的所有的节点都可以借用 查询所有节点
    std::vector<std::string> allNodeIdList = MpConfiguration::GetInstance().GetNodeIds();
    if (allNodeIdList.empty()) {
        return MEM_POOLING_ERROR;
    }
    // 去除掉故障节点、已经是借入节点的节点(自己也是)
    for (std::string nodeId : allNodeIdList) {
        NodeType nodeType = NodeType::ABNORMAL;
        FaultNodeModule::Instance().DetermineNodeTypeFragment(nodeId, nodeType);
        ubse::nodeController::UbseNodeInfo info = UbseNodeController::GetInstance().GetNodeById(nodeId);
        if (info.clusterState == ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING &&
            nodeId != curDealNodeId && (nodeType == NodeType::BORROW_OUT || nodeType == NodeType::NO_RECORD)) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] [FaultLentNode] Borrow able nodeId " << nodeId << ".";
            borrowAbleNodeIdList.push_back(nodeId);
        }
    }
    if (borrowAbleNodeIdList.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] [FaultLentNode] No borrowable nodeId " << curDealNodeId << ".";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult FaultNodeModule::GetBorrowAbleNodeInfoSortByMemSize(
    std::vector<std::string> borrowAbleNodeIdList, std::vector<NodeMemoryInfoWithReservedMem> &ableNodeMemInfoList)
{
    // 获取可借用节点的 节点-numa 内存余量
    MpResult res = BorrowRecordHelper::Instance().CollectBorrowableInfoList(borrowAbleNodeIdList, ableNodeMemInfoList);
    if (res != MEM_POOLING_OK || ableNodeMemInfoList.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] [FaultLentNode] CollectBorrowableInfoList failed.";
        return MEM_POOLING_ERROR;
    }
    // 节点可借内存从大到小排序
    std::sort(ableNodeMemInfoList.begin(), ableNodeMemInfoList.end(),
              [](NodeMemoryInfoWithReservedMem l1, NodeMemoryInfoWithReservedMem l2) {
                  return l1.canBorrowMem > l2.canBorrowMem;
              });
    // 节点内numa可借内存从大到小排序
    for (NodeMemoryInfoWithReservedMem &record : ableNodeMemInfoList) {
        // 也按照借用大小从大到小排序
        std::sort(record.numaMemInfo.begin(), record.numaMemInfo.end(),
                  [](RackNumaMemInfo l1, RackNumaMemInfo l2) { return l1.canBorrowMem > l2.canBorrowMem; });
    }
    return MEM_POOLING_OK;
}

void FaultNodeModule::MergeBorrowRecords(std::vector<BorrowRecord> borrowRecordList,
                                         std::vector<NodeBorrowRecord> &nodeBorrowRecordList)
{
#ifdef UB_ENVIRONMENT
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] [FaultLentNode] UB MergeBorrowRecords";
    std::unordered_map<std::string, NodeBorrowRecord> borrowRecordGroupByNodeIdMap;
    for (BorrowRecord record : borrowRecordList) {
        std::string key = record.borrowNode + "_" + record.lentNode + "_" + std::to_string(record.lentSocketId);
        uint16_t borrowSocketId{0};
        MemManager::Instance().GetSocketId(record.borrowNode, record.borrowLocalNuma, borrowSocketId);
        OnceBorrowRecord onceBorrowRecord{record.name, record.size, record.borrowMemId,
                                          static_cast<uint16_t>(record.borrowRemoteNuma),
                                          static_cast<uint16_t>(record.borrowLocalNuma), record.uid, record.username};
        if (borrowRecordGroupByNodeIdMap.find(key) != borrowRecordGroupByNodeIdMap.end()) {
            NodeBorrowRecord borrowRecord = borrowRecordGroupByNodeIdMap[key];
            borrowRecord.borrowMemSize += record.size;
            borrowRecord.onceBorrowRecordList.push_back(onceBorrowRecord);
            borrowRecordGroupByNodeIdMap[key] = borrowRecord;
        } else {
            NodeBorrowRecord nodeBorrowRecord{record.borrowNode, borrowSocketId, record.size, {onceBorrowRecord}};
            borrowRecordGroupByNodeIdMap[key] = nodeBorrowRecord;
        }
    }
    for (auto borrowRecordEntry : borrowRecordGroupByNodeIdMap) {
        nodeBorrowRecordList.push_back(borrowRecordEntry.second);
    }
#else
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] [FaultLentNode] HCCS MergeBorrowRecords";
    std::unordered_map<std::string, std::vector<BorrowRecord>> borrowRecordGroupByNodeIdMap;
    for (BorrowRecord record : borrowRecordList) {
        borrowRecordGroupByNodeIdMap[record.borrowNode].push_back(record);
    }
    for (auto borrowRecordEntry : borrowRecordGroupByNodeIdMap) {
        NodeBorrowRecord tmpNodeBorrowRecord;
        tmpNodeBorrowRecord.nodeId = borrowRecordEntry.first;
        for (BorrowRecord recordItem : borrowRecordEntry.second) {
            tmpNodeBorrowRecord.borrowMemSize += recordItem.size;
            OnceBorrowRecord onceBorrowRecord{recordItem.name, recordItem.size, recordItem.borrowMemId,
                                              static_cast<uint16_t>(recordItem.borrowRemoteNuma),
                                              static_cast<uint16_t>(recordItem.borrowLocalNuma)};
            tmpNodeBorrowRecord.onceBorrowRecordList.push_back(onceBorrowRecord);
        }
        nodeBorrowRecordList.push_back(tmpNodeBorrowRecord);
    }
#endif
    // socket级别记录按借用大小从大到小排序
    std::sort(nodeBorrowRecordList.begin(), nodeBorrowRecordList.end(),
              [](NodeBorrowRecord r1, NodeBorrowRecord r2) { return r1.borrowMemSize > r2.borrowMemSize; });
    // 每一个节点内部的借用记录
    for (NodeBorrowRecord &record : nodeBorrowRecordList) {
        // socket内也按照借用大小从大到小排序
        std::sort(record.onceBorrowRecordList.begin(), record.onceBorrowRecordList.end(),
                  [](OnceBorrowRecord l1, OnceBorrowRecord l2) { return l1.borrowSize > l2.borrowSize; });
    }
}

bool FaultNodeModule::MayBorrowFromNuma(NodeBorrowRecord nodeBorrowRecord,
                                        NodeMemoryInfoWithReservedMem &originNodeMemInfoItem,
                                        NodeMemoryInfoWithReservedMem nodeMemInfoItem,
                                        std::vector<BorrowExecuteParam> &borrowExecuteParamCollectList)
{
    std::vector<BorrowExecuteParam> tmpList;
    std::map<uint16_t, uint64_t> sizeRecordMap;
    for (OnceBorrowRecord record : nodeBorrowRecord.onceBorrowRecordList) {
        RackNumaMemInfo item = nodeMemInfoItem.numaMemInfo[0];
        BorrowExecuteParam param;
        if (record.borrowSize > item.canBorrowMem) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] [FaultLentNode] Numa mem no enough "
                << " needBorrowSize " << record.borrowSize << " numaCanBorrowSize " << item.canBorrowMem << " nodeId "
                << nodeMemInfoItem.nodeId << " numaId " << item.numaId << ".";
            return false;
        }
        param.borrowNodeId = nodeBorrowRecord.nodeId;
        param.borrowNumaId = record.localNumaId;
        param.oldNumaId = record.numaId;
        param.lentNodeId = nodeMemInfoItem.nodeId;
        param.lentNumaId = item.numaId;
        param.memSize = record.borrowSize;
        param.memIdList = record.memIdList;
        param.oldBorrowId = record.borrowId;
        param.uid = record.uid;
        param.username = record.username;
        tmpList.push_back(param);
        nodeMemInfoItem.canBorrowMem -= record.borrowSize;
        nodeMemInfoItem.numaMemInfo[0].canBorrowMem -= record.borrowSize;
        std::sort(nodeMemInfoItem.numaMemInfo.begin(), nodeMemInfoItem.numaMemInfo.end(),
                  [](RackNumaMemInfo l1, RackNumaMemInfo l2) { return l1.canBorrowMem > l2.canBorrowMem; });
    }
    originNodeMemInfoItem = nodeMemInfoItem;
    (void)borrowExecuteParamCollectList.insert(borrowExecuteParamCollectList.end(), tmpList.begin(), tmpList.end());
    for (BorrowExecuteParam borrowExecuteParamItem : borrowExecuteParamCollectList) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] [FaultLentNode] Borrow execute param collect " << borrowExecuteParamItem.ToString()
            << ".";
    }
    return true;
}

MpResult FaultNodeModule::NodeMayBorrowFromOtherNode(NodeBorrowRecord nodeBorrowRecord,
                                                     std::vector<NodeMemoryInfoWithReservedMem> &ableNodeMemInfoList,
                                                     std::vector<BorrowExecuteParam> &borrowExecuteParamCollectList)
{
    std::unordered_set<std::string> couldBorrowNodeSet;
    auto ret = MpParseGroupProviderConf::Instance().GetBorrowableList(nodeBorrowRecord.nodeId, couldBorrowNodeSet);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager] [FaultLentNode] GetBorrowableList failed.";
        return MEM_POOLING_ERROR;
    }
    std::vector<std::string> antiNodeMemVec;
    AntiNode::Instance().Query(nodeBorrowRecord.nodeId, antiNodeMemVec);
    for (const auto &node : antiNodeMemVec) {
        couldBorrowNodeSet.erase(node);
    }
    bool mustSamePlane = MpConfiguration::GetInstance().GetMustSamePlane();
    std::unordered_map<std::string, BorrowItem> borrowCacheAll;
    MemReturnManager::Instance().QueryAll(borrowCacheAll);
    for (auto &kv : borrowCacheAll) {
        if (kv.second.srcNid == nodeBorrowRecord.nodeId &&
            (!mustSamePlane ||
             MemManager::Instance().JudgeSampPlane(nodeBorrowRecord.nodeId, nodeBorrowRecord.borrowSocketId,
                                                   kv.second.dstNid, kv.second.dstSocketId))) {
            LOG_DEBUG << "[FaultManager] Node=" << kv.second.dstNid << " has return timeout records, erase it.";
            couldBorrowNodeSet.erase(kv.second.dstNid);
        }
    }
    for (NodeMemoryInfoWithReservedMem &nodeMemInfoItem : ableNodeMemInfoList) {
        if (mustSamePlane &&
            !MemManager::Instance().JudgeSampPlane(nodeMemInfoItem.nodeId, nodeMemInfoItem.socketId,
                                                   nodeBorrowRecord.nodeId, nodeBorrowRecord.borrowSocketId)) {
            continue;
        }
        if (couldBorrowNodeSet.find(nodeMemInfoItem.nodeId) == couldBorrowNodeSet.end()) {
            continue;
        }
        if (nodeMemInfoItem.canBorrowMem < nodeBorrowRecord.borrowMemSize) {
            // 可借用节点中最大的可借用内存都不够借用 直接给到memId级别处理
            LOG_ERROR << "[FaultManager] [FaultLentNode] No node can borrow"
                      << "nodeId " << nodeBorrowRecord.nodeId << " needBorrowMemSize " << nodeBorrowRecord.borrowMemSize
                      << " canBorrowMemSize " << nodeMemInfoItem.canBorrowMem << ".";
            return MEM_POOLING_ERROR;
        }
        if (MayBorrowFromNuma(nodeBorrowRecord, nodeMemInfoItem, nodeMemInfoItem, borrowExecuteParamCollectList)) {
            return MEM_POOLING_OK;
        }
        LOG_WARN << "[FaultManager] [FaultLentNode] No MayBorrowFromNuma"
                 << "nodeId " << nodeBorrowRecord.nodeId << " needBorrowMemSize " << nodeBorrowRecord.borrowMemSize;
    }
    return MEM_POOLING_ERROR;
}

MpResult FaultNodeModule::MayBorrowFromOtherNode(std::string curDealNodeId, std::vector<BorrowRecord> borrowRecords,
                                                 std::vector<BorrowExecuteParam> &borrowExecuteParamCollectList,
                                                 std::vector<ForwardMemIdParam> &forwardMemIdParamList)
{
    std::vector<std::string> borrowAbleNodeIdList;
    MpResult res = GetBorrowAbleNodeIdList(curDealNodeId, borrowAbleNodeIdList);
    if (res != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    std::vector<NodeMemoryInfoWithReservedMem> ableNodeMemInfoList;
    res = GetBorrowAbleNodeInfoSortByMemSize(borrowAbleNodeIdList, ableNodeMemInfoList);
    if (res != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    // nodeId维度合并借入故障节点的记录
    std::vector<NodeBorrowRecord> nodeBorrowRecordList;
    MergeBorrowRecords(borrowRecords, nodeBorrowRecordList);
    std::vector<NodeBorrowRecord> failedNodeBorrowRecordList;
    for (NodeBorrowRecord nodeBorrowRecordItem : nodeBorrowRecordList) {
        // 判断单个借入节点能否从所有可借用节点借用到足够的内存 可以借用到就直接更新可借用节点的可借用内存信息
        std::sort(
            ableNodeMemInfoList.begin(), ableNodeMemInfoList.end(),
            [&nodeBorrowRecordItem](const NodeMemoryInfoWithReservedMem &l1, const NodeMemoryInfoWithReservedMem &l2) {
                bool l1enough = l1.canBorrowMem >= nodeBorrowRecordItem.borrowMemSize;
                bool l2enough = l2.canBorrowMem >= nodeBorrowRecordItem.borrowMemSize;
                if (l1enough != l2enough) {
                    return l1enough;
                }
                bool l1SamePlane = MemManager::Instance().JudgeSampPlane(
                    nodeBorrowRecordItem.nodeId, nodeBorrowRecordItem.borrowSocketId, l1.nodeId, l1.socketId);
                bool l2SamePlane = MemManager::Instance().JudgeSampPlane(
                    nodeBorrowRecordItem.nodeId, nodeBorrowRecordItem.borrowSocketId, l2.nodeId, l2.socketId);
                if (l1SamePlane != l2SamePlane) {
                    return l1SamePlane; // true 会排前面
                }
                return l1.canBorrowMem > l2.canBorrowMem;
            });

        for (auto &nodeMemInfo : ableNodeMemInfoList) {
            LOG_DEBUG << "NodeId = " << nodeMemInfo.nodeId << " , socketId = " << nodeMemInfo.socketId
                      << " , canBorrowMem = " << nodeMemInfo.canBorrowMem;
            for (auto &numaInfo : nodeMemInfo.numaMemInfo) {
                LOG_DEBUG << "numaInfo = " << numaInfo.ToString();
            }
        }
        // 有不能借用的全部降为memId级故障处理
        res = NodeMayBorrowFromOtherNode(nodeBorrowRecordItem, ableNodeMemInfoList, borrowExecuteParamCollectList);
        if (res != MEM_POOLING_OK) {
            failedNodeBorrowRecordList.push_back(nodeBorrowRecordItem);
            continue;
        }
    }
    for (NodeBorrowRecord nodeBorrowRecordItem : failedNodeBorrowRecordList) {
        std::string importNodeId = nodeBorrowRecordItem.nodeId;
        for (OnceBorrowRecord onceBorrowRecord : nodeBorrowRecordItem.onceBorrowRecordList) {
            ForwardMemIdParam param{importNodeId, onceBorrowRecord.memIdList, onceBorrowRecord.uid,
                                    onceBorrowRecord.username};
            forwardMemIdParamList.push_back(param);
        }
    }
    return MEM_POOLING_OK;
}

MpResult FaultNodeModule::FillBorrowExecuteParam(std::vector<BorrowExecuteParam> &borrowExecuteParamCollectList)
{
    if (borrowExecuteParamCollectList.empty()) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] [FaultLentNode] Execute list empty.";
        return MEM_POOLING_OK;
    }
    for (BorrowExecuteParam &param : borrowExecuteParamCollectList) {
        uint16_t borrowSocketId;
        uint16_t lentSocketId;
        MpResult ret = MemManager::Instance().GetSocketId(param.borrowNodeId, param.borrowNumaId, borrowSocketId);
        ret |= MemManager::Instance().GetSocketId(param.lentNodeId, param.lentNumaId, lentSocketId);
        if (ret) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] [FaultLentNode] Can't find numaId form socket.";
            return MEM_POOLING_ERROR;
        }
        param.borrowSocketId = static_cast<uint16_t>(borrowSocketId);
        param.lentSocketId = static_cast<uint16_t>(lentSocketId);
    }
    return MEM_POOLING_OK;
}

bool FaultNodeModule::DealBorrowResult(std::vector<BorrowExecuteParam> &borrowExecuteParamList,
                                       std::vector<BorrowExecuteParam> &successExecuteParamCollectList,
                                       std::vector<MemBorrowExecuteResult> resultList)
{
    for (size_t i = 0; i < borrowExecuteParamList.size(); i++) {
        borrowExecuteParamList[i].newNumaId = resultList[i].presentNumaId[0];
        borrowExecuteParamList[i].newBorrowId = resultList[i].borrowIds[0];
        successExecuteParamCollectList.push_back(borrowExecuteParamList[i]);
    }
    return true;
}

void FaultNodeModule::DoExecuteBorrow(std::vector<BorrowExecuteParam> &successExecuteParamCollectList,
                                      std::pair<std::string, std::vector<BorrowExecuteParam>> nodeBorrowExecuteParam,
                                      std::vector<ForwardMemIdParam> &forwardMemIdParamList)
{
    std::vector<MemBorrowExecuteResult> resultList;
    for (BorrowExecuteParam borrowExecuteParam : nodeBorrowExecuteParam.second) {
        SrcMemoryBorrowParam srcParam{
            borrowExecuteParam.borrowNodeId, static_cast<int16_t>(borrowExecuteParam.borrowSocketId),
            static_cast<int16_t>(borrowExecuteParam.borrowNumaId), borrowExecuteParam.uid, borrowExecuteParam.username};
        MemBorrowExecuteResult borrowExecuteResult;
        std::vector<DestMemoryBorrowParam> destParamList;
        uint16_t destNumaNum = 1; // 当前限制为1
        std::vector<int> destNumaIdList{borrowExecuteParam.lentNumaId};
        std::vector<uint64_t> memSizeList{borrowExecuteParam.memSize};
        DestMemoryBorrowParam destMemoryBorrowParam{borrowExecuteParam.lentNodeId, borrowExecuteParam.lentSocketId,
                                                    destNumaNum, destNumaIdList, memSizeList};
        destParamList.push_back(destMemoryBorrowParam);
        MpResult res = MempoolBorrowModule::Instance().MemBorrowExecute(srcParam, destParamList, borrowExecuteResult);
        if (res) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] [FaultLentNode] Borrow mem error srcNode " << borrowExecuteParam.borrowNodeId
                << " destNode " << borrowExecuteParam.lentNodeId << " size " << borrowExecuteParam.memSize << ".";
            break;
        }
        resultList.push_back(borrowExecuteResult);
    }
    // 执行结果和执行集的数量不等说明有失败的
    if (resultList.size() == nodeBorrowExecuteParam.second.size()) {
        DealBorrowResult(nodeBorrowExecuteParam.second, successExecuteParamCollectList, resultList);
    } else {
        for (BorrowExecuteParam borrowExecuteParam : nodeBorrowExecuteParam.second) {
            ForwardMemIdParam param{borrowExecuteParam.borrowNodeId, borrowExecuteParam.memIdList,
                                    borrowExecuteParam.uid, borrowExecuteParam.username};
            forwardMemIdParamList.push_back(param);
        }
    }
}

void FaultNodeModule::ExecuteBorrow(std::vector<BorrowExecuteParam> &borrowExecuteParamCollectList,
                                    std::vector<BorrowExecuteParam> &successExecuteParamCollectList,
                                    std::vector<ForwardMemIdParam> &forwardMemIdParamList)
{
    if (borrowExecuteParamCollectList.empty()) {
        return;
    }
    // 按照接入节点分组 如果有一个执行失败就转为memId级故障处理
    std::unordered_map<std::string, std::vector<BorrowExecuteParam>> nodeBorrowExecuteParamMap;
    for (BorrowExecuteParam param : borrowExecuteParamCollectList) {
        nodeBorrowExecuteParamMap[param.borrowNodeId].push_back(param);
    }
    for (auto &nodeBorrowExecuteParam : nodeBorrowExecuteParamMap) {
        DoExecuteBorrow(successExecuteParamCollectList, nodeBorrowExecuteParam, forwardMemIdParamList);
    }
}

MpResult FaultNodeModule::DealRes(NumaReplaceReturnMsg msg)
{
    std::set<std::string> failSet;
    // 删除掉老的borrowId
    for (BorrowExecuteParam param : msg.BorrowExecuteParamVec) {
        if (mempooling::MemBorrowExecutor::Instance().MemFreeWithOps(param.oldBorrowId, false, false, true)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] [FaultLentNode] MemFree borrowId failed, nodeId " << msg.faultNuma << " borrowId"
                << param.oldBorrowId << ".";
            (void)failSet.insert(msg.faultNuma);
        }
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] [FaultLentNode] Update borrow map oldBorrowId " << param.oldBorrowId << " newBorrowId "
            << param.newBorrowId << ".";
        if (BorrowIdRedirection::Instance().Update(param.oldBorrowId, param.newBorrowId)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] [FaultLentNode] UpdateBorrowIdRedirection failed.";
            (void)failSet.insert(msg.faultNuma);
        }
    }

    return failSet.empty() ? MEM_POOLING_OK : MEM_POOLING_ERROR;
}

MpResult FaultNodeModule::ExecuteNumaReplaceAndReturn(std::vector<BorrowExecuteParam> &borrowExecuteParamCollectList,
                                                      bool failFlag, bool forceDeleteMem)
{
    if (borrowExecuteParamCollectList.empty()) {
        return MEM_POOLING_OK;
    }
    std::unordered_map<std::string, std::vector<BorrowExecuteParam>> nodeBorrowExecuteParamMap;
    for (BorrowExecuteParam borrowExecuteParam : borrowExecuteParamCollectList) {
        std::string key = borrowExecuteParam.borrowNodeId + "_" + std::to_string(borrowExecuteParam.oldNumaId);
        nodeBorrowExecuteParamMap[key].push_back(borrowExecuteParam);
    }
    for (const auto nodeBorrowExecuteParamEntry : nodeBorrowExecuteParamMap) {
        NumaReplaceReturnMsg msg;
        std::string tmpNodeId;
        for (BorrowExecuteParam param : nodeBorrowExecuteParamEntry.second) {
            tmpNodeId = param.borrowNodeId;
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] [FaultLentNode] Before rpc replace, param " << param.ToString() << ".";
            msg.srcNid = param.oldNumaId;
            msg.destNid = param.newNumaId;
            MigrateBorrowRecord migrateBorrowRecord{param.memIdList, param.memSize * MEM_SYS_NUM};
            msg.migrateBorrowRecordList.push_back(migrateBorrowRecord);
        }
        msg.BorrowExecuteParamVec = nodeBorrowExecuteParamEntry.second;
        RmrsOutStream builder;
        builder << msg;
        UbseByteBuffer reqData = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
        UbseComEndpoint endpoint{MP_MODULE_CODE, OPCODE_FAULT_NODE_NUMA_REPLACE_RETURN, tmpNodeId};
        MpResult res = MEM_POOLING_ERROR;
        UbseRpcSend(endpoint, reqData, &res, NodeNumaReplaceReturnResHandler);
        delete[] reqData.data;
        if (res == MEM_POOLING_ERROR) {
            failFlag = true;
        }
    }
    return MEM_POOLING_OK;
}

MpResult FaultNodeModule::ForwardMemIdFaultDeal(std::vector<ForwardMemIdParam> forwardMemIdParamList,
                                                bool forceDeleteMem)
{
    MpResult res = MEM_POOLING_OK;
    for (ForwardMemIdParam param : forwardMemIdParamList) {
        if (param.memIdList.empty()) {
            res = MEM_POOLING_ERROR;
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] [FaultLentNode] Forward memId deal but memId empty, nodeId " << param.importNodeId
                << ".";
            continue;
        }
        if (FaultMemIdHelper::Instance().FaultMemIdManageHelper(param.importNodeId, param.memIdList[0], forceDeleteMem,
                                                                true)) {
            res = MEM_POOLING_ERROR;
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] [FaultLentNode] Turn to memId fault deal error, "
                << "importNodeId=" << param.importNodeId << ", importMemId=" << param.memIdList[0] << ", uid"
                << param.uid << ", username=" << param.username << ".";
        }
    }
    return res;
}

bool FaultNodeModule::CheckUBTurboIsAliveRpc(std::string nodeId) {
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) 
        << "[FaultManager] Master to invoke the slave CheckUBTurboIsAlive.";
    UbseComEndpoint endpoint = {
        .moduleId = MP_MODULE_CODE, .serviceId = message::OPCODE_CHECK_UBTURBO_IS_ALIVE, .address = nodeId};
    RmrsOutStream builder;
    UbseByteBuffer reqData = {
        .data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = [](uint8_t *data) { delete[] data; }};
    bool isAlive = false;
    UbseRpcSend(endpoint, reqData, &isAlive, CheckUBTurboIsAliveResHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] CheckUBTurboIsAlive failed, ret = " << ret;
        return false;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] CheckUBTurboIsAlive success.";
    return isAlive;
}

uint32_t FaultNodeModule::CheckUBTurboIsAliveHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] CheckUBTurboIsAliveHandler start.";
    resp.len = 1;
    resp.data = new (std::nothrow) uint8_t[resp.len]{};
    if (resp.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] Failed to allocate memory, size=" << resp.len << ".";
        return MEM_POOLING_ERROR;
    }
    resp.freeFunc = [](uint8_t *p) {
        if (p != nullptr) {
            delete[] p;
        }
    };
    turbo::rmrs::PidNumaInfoCollectParam pidNumaInfoCollectParam;
    turbo::rmrs::PidNumaInfoCollectResult pidNumaInfoCollectResult;
    auto ret = MempoolingMessage::rmrsPidNumaInfoCollect(pidNumaInfoCollectParam, pidNumaInfoCollectResult);
    if (ret == IPC_BAD_SOCKET || ret == IPC_BAD_CONNECT) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] UBTurbo is not alive, ret=" << ret << ".";
        resp.data[0] = static_cast<uint8_t>(0);
        return ret;
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] UBTurbo is alive, ret=" << ret << ".";
        resp.data[0] = static_cast<uint8_t>(1);
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] CheckUBTurboIsAliveHandler end.";
    return ret;
}

void FaultNodeModule::CheckUBTurboIsAliveResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] CheckUBTurboIsAliveResHandler resCode=" 
        << resCode;
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] Ctx or respData is null.";
        return;
    }
    auto *result = static_cast<bool *>(ctx);
    if (resCode != MEM_POOLING_OK || respData.data[0] != 1) {
        *result = false;
        return;
    }
    *result = true;
}

MpResult FaultNodeModule::FragmentHandleFault(std::string nodeId)
{
    faultHandleCurRound++;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] FragmentHandleFault round "
                                                      << faultHandleCurRound << " start.";

    // =========基于不信任原则，获取账本并筛选合法条目===========
    // =========仅处理合法条目，处理完后返回失败，利用UBSE故障重试机制继续处理===========
    MpResult res =
        BorrowRecordHelper::Instance().UpdateBorrowRecordsWithFragMentFault(nodeId);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] UpdateBorrowRecordsWithFragMentFault failed.";
        return res;
    }

    NodeType nodeType = NodeType::ABNORMAL;
    res = FaultNodeModule::Instance().DetermineNodeTypeFragment(nodeId, nodeType);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] DetermineNodeType failed.";
        return MEM_POOLING_ERROR;
    }
    if (nodeType == NodeType::BORROW_IN) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] BORROW_IN Fault is handled by MXE.";
    } else if (nodeType == NodeType::BORROW_OUT) {
        res = FaultNodeModule::Instance().ProcessBorrowOutNodeFault(nodeId, true);
        if (res != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] Process BORROW_OUT node fault failed.";
        }
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] FragmentHandleFault round "
                                                      << faultHandleCurRound << " end.";
    // 处理完后返回失败，利用UBSE故障重试机制继续处理
    return MEM_POOLING_ERROR;
}

MpResult FaultNodeModule::ProcessBorrowOutNodeFault(const std::string nodeId, bool forceDeleteMem)
{
    // 根据内存账本获取所有借入节点信息
    std::vector<BorrowRecord> borrowRecords;
    MpResult res = GetBorrowNodeInfo(nodeId, borrowRecords);
    if (res != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    // 如果借用记录是空的 说明不需要做任何处理 直接返回
    if (borrowRecords.empty()) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] [FaultLentNode] BorrowRecords empty, do not any deal.";
        return MEM_POOLING_OK;
    }
    std::vector<NodeBorrowRecord> nodeBorrowRecordList;
    MergeBorrowRecords(borrowRecords, nodeBorrowRecordList);
    for (NodeBorrowRecord nodeBorrowRecordItem : nodeBorrowRecordList) {
        // 如果ubturbo不存活，故障处理直接失败重试
        if(!CheckUBTurboIsAliveRpc(nodeBorrowRecordItem.nodeId)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultManager] UBTurbo is not alive, retry fragment handling.";
            return MEM_POOLING_ERROR;
        }
    }
    // 收集需要转memId故障处理的参数集
    std::vector<ForwardMemIdParam> forwardMemIdParamList;
    // 分析每一个借入节点是否能从其他节点借同样次数、每次借相同大小的内存 获取到分析的借用动作集合
    std::vector<BorrowExecuteParam> borrowExecuteParamCollectList;
    res |= MayBorrowFromOtherNode(nodeId, borrowRecords, borrowExecuteParamCollectList, forwardMemIdParamList);
    if (res != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    // 填充执行借用的其余参数
    res |= FillBorrowExecuteParam(borrowExecuteParamCollectList);
    if (res != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    // 执行借用 借用失败的交给memId级别故障处理
    std::vector<BorrowExecuteParam> successExecuteParamCollectList;
    ExecuteBorrow(borrowExecuteParamCollectList, successExecuteParamCollectList, forwardMemIdParamList);
    // 借入节点的待销毁远端numa处理 替换&归还 发rpc
    bool isNodeProcessFailed = false;
    ExecuteNumaReplaceAndReturn(successExecuteParamCollectList, isNodeProcessFailed, forceDeleteMem);
    if (ForwardMemIdFaultDeal(forwardMemIdParamList, forceDeleteMem) == MEM_POOLING_OK && !isNodeProcessFailed) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] [FaultLentNode] Node fault deal success.";
        return MEM_POOLING_OK;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] [FaultLentNode] Node fault deal exist error.";
    return MEM_POOLING_ERROR;
}

void GetPidListAndHugePageMemSize(const NumaReplaceReturnMsg &rpcMsg, std::vector<pid_t> &destPidList,
                                  uint64_t &hugePageMemSize)
{
    if (!FaultNodeModule::Instance().QueryNumaAllPid(rpcMsg.destNid, destPidList)) {
        LOG_INFO << "[FaultManager] [FaultLentNode] DestPidList may empty.";
    }
    // 计算需要分配大页的内存大小，单位b
    for (size_t i = 0; i < rpcMsg.BorrowExecuteParamVec.size(); i++) {
        hugePageMemSize += rpcMsg.BorrowExecuteParamVec[i].memSize;
    }
    hugePageMemSize = hugePageMemSize * KB_1024;
    LOG_DEBUG << "[FaultManager] [FaultLentNode] Param hugePageMemSize=" << hugePageMemSize
              << "B, srcNid=" << rpcMsg.srcNid << ", destNid=" << rpcMsg.destNid << ".";
}

void NodeNumaReplaceReturnHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    NumaReplaceReturnMsg rpcMsg;
    RmrsInStream builder(req.data, req.len);
    builder >> rpcMsg;
    std::vector<pid_t> pidList;
    std::vector<MigrateNumaMsg> msgList;
    std::vector<uint8_t> resList;
    std::vector<pid_t> destPidList;
    uint64_t hugePageMemSize = 0;
    GetPidListAndHugePageMemSize(rpcMsg, destPidList, hugePageMemSize);
    // 查询numa上所有pid
    // 禁用源端所有的pid冷热流动 enable 0禁用 1启用
    // 禁用目的端所有的pid冷热流动 enable 0禁用 1启用
    // 合并、分片需要迁移的所有物理地址段
    // 远端numa迁远端numa 完成替换
    // 开启所有pid冷热流动
    if (((FaultNodeModule::Instance().QueryNumaAllPid(rpcMsg.srcNid, pidList) &&
         FaultNodeModule::Instance().SwitchMigrateForNumaVm(pidList, 0) &&
         (destPidList.empty() ? true : FaultNodeModule::Instance().SwitchMigrateForNumaVm(destPidList, 0)) &&
         FaultNodeModule::Instance().GenerateMigrateNumaMsgList(rpcMsg, msgList) &&
         FaultNodeModule::Instance().ExecMigrateRemoteNumaToNuma(rpcMsg, msgList) &&
         FaultNodeModule::Instance().SwitchMigrateForNumaVm(pidList, 1)) &&
         (destPidList.empty() ? true : FaultNodeModule::Instance().SwitchMigrateForNumaVm(destPidList, 1)) ||
         (pidList.empty() && FaultNodeModule::Instance().AllocateHugePage(rpcMsg.destNid, hugePageMemSize))) &&
        FaultNodeModule::Instance().DealRes(rpcMsg) == MEM_POOLING_OK) {
        resList.push_back(MEM_POOLING_OK);
        resp.len = resList.size();
        resp.data = new (std::nothrow) uint8_t[resp.len];
        resp.freeFunc = [](uint8_t *data) {
            delete[] data;
        };
        if (resp.data == nullptr) {
            LOG_ERROR << "[FaultManager] [FaultLentNode] Failed to allocate memory, size=" << resp.len << ".";
            return;
        }
        if (memcpy_s(resp.data, resp.len, resList.data(), resp.len) != EOK) {
            LOG_ERROR << "[FaultManager] [FaultLentNode] memcpy_s failed";
            return;
        }
        LOG_DEBUG << "[FaultManager] [FaultLentNode] NodeNumaReplaceReturn handle success.";
        return;
    }
    LOG_ERROR << "[FaultManager] [FaultLentNode] Execute numa replace return failed " << rpcMsg.faultNuma << ".";
    resList.push_back(MEM_POOLING_ERROR);
    resp.len = resList.size();
    resp.data = new (std::nothrow) uint8_t[resp.len];
    resp.freeFunc = [](uint8_t *data) {
        delete[] data;
    };
    if (resp.data == nullptr) {
        LOG_ERROR << "[FaultManager] [FaultLentNode] Failed to allocate memory, size=" << resp.len << ".";
        return;
    }
    if (memcpy_s(resp.data, resp.len, resList.data(), resp.len) != EOK) {
        LOG_ERROR << "[FaultManager] [FaultLentNode] memcpy_s failed";
        return;
    }
}

void NodeNumaReplaceReturnResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager][FaultLentNode] ResHandler ctx is null.";
        return;
    }
    MpResult *res = static_cast<MpResult *>(ctx);
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] [FaultLentNode] NodeNumaReplaceReturn res: " << respData.data[0] << ".";
    if (resCode != MEM_POOLING_OK || respData.data[0] != MEM_POOLING_OK) {
        *res = MEM_POOLING_ERROR;
        return;
    }
    *res = resCode;
}

} // namespace mempooling