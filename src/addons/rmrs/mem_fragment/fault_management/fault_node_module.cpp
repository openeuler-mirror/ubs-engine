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
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <set>
#include <thread>
#include <unordered_map>

#include "ubse_com.h"
#include "ubse_error.h"
#include "ubse_mem_controller.h"
#include "ubse_pointer_process.h"
#include "export_type.h"
#include "exporter.h"
#include "fault_memid_helper.h"
#include "mem_borrow_executor.h"
#include "mem_manager.h"
#include "mempooling_message.h"
#include "mp_configuration.h"
#include "mp_json_util.h"
#include "over_commit_fault_memid_module.h"
#include "rmrs_serialize.h"

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
constexpr uint64_t BLOCK_SIZE_KB = 128 * KB_1024;
constexpr size_t RPC_RESP_LENGTH = 2;

#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)

uint64_t FaultNodeModule::sBlockSizeKb = BLOCK_SIZE_KB; // 静态成员初始化

void FaultNodeModule::SetBlockSizeKB(const std::string& nodeId)
{
    const auto allNodeInfo = UbseNodeController::GetInstance().GetAllNodes();
    auto it = allNodeInfo.find(nodeId);
    if (it == allNodeInfo.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] GetAllNodes failed, nodeId=" << nodeId << ".";
        sBlockSizeKb = BLOCK_SIZE_KB;
        return;
    }

    // blockSize 单位是 MB，转换为 KB
    sBlockSizeKb = static_cast<uint64_t>(it->second.blockSize) * KB_1024;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[SetBlockSizeKB] Set BlockSizeKB=" << sBlockSizeKb << ".";
}

uint64_t FaultNodeModule::GetBlockSizeKB()
{
    if (sBlockSizeKb == 0) {
        // 未被初始化时，使用一个安全默认值（128MB）
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[GetBlockSizeKB] BlockSize not set, using default 128MB.";
        return BLOCK_SIZE_KB;
    }
    return sBlockSizeKb;
}

MpResult FaultNodeModule::DetermineNodeTypeOverCommit(const std::string nodeId, NodeType& nodeType)
{
    MpResult ret = MEM_POOLING_OK;
    std::vector<UbseNumaMemoryDebtInfo> debtInfos;
    UbseResult retErrorCode = UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos);
    if (retErrorCode != UBSE_OK && retErrorCode != UBSE_MEMCONTROLLER_ERROR_PAR_SUCCESS) {
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

MpResult FaultNodeModule::DetermineNodeTypeFragment(const std::string nodeId, NodeType& nodeType)
{
    MpResult ret = MEM_POOLING_OK;
    std::vector<BorrowRecord> fragMentFaultBorrowRecords;
    UbseResult retErrorCode =
        BorrowRecordHelper::Instance().GetFragmentFaultBorrowRecords(nodeId, fragMentFaultBorrowRecords);
    if (retErrorCode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] GetFragmentFaultBorrowRecords failed.";
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
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] Node:" << nodeId << " NodeType: BORROW_IN.";
        nodeType = NodeType::BORROW_IN;
    }
    return ret;
}

MpResult FaultNodeModule::DetermineNodeType(const std::string nodeId, NodeType& nodeType)
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

bool FaultNodeModule::QueryNumaAllPid(uint16_t numaId, std::vector<pid_t>& pidList)
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
    for (const auto& vmDomainInfo : vmDomainInfos) {
        // 查找远端 NUMA
        for (const auto& kv : vmDomainInfo.numaInfo) {
            const mempooling::exportV2::VmDomainNumaInfo& numa = kv.second;
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

bool FaultNodeModule::GenerateMigrateNumaMsgList(NumaReplaceReturnMsg rpcMsg, std::vector<MigrateNumaMsg>& msgList)
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

MpResult FaultNodeModule::GetBorrowNodeInfo(std::string nodeId, std::vector<BorrowRecord>& borrowRecords)
{
    UbseResult res = BorrowRecordHelper::Instance().GetFragmentFaultBorrowRecords(nodeId, borrowRecords);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] [FaultLentNode] GetFragmentFaultBorrowRecords failed, nodeId=" << nodeId;
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult FaultNodeModule::GetBorrowAbleNodeIdList(std::string curDealNodeId,
                                                  std::vector<std::string>& borrowAbleNodeIdList)
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
    std::vector<std::string> borrowAbleNodeIdList, std::vector<NodeMemoryInfoWithReservedMem>& ableNodeMemInfoList)
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
    for (NodeMemoryInfoWithReservedMem& record : ableNodeMemInfoList) {
        // 也按照借用大小从大到小排序
        std::sort(record.numaMemInfo.begin(), record.numaMemInfo.end(),
                  [](RackNumaMemInfo l1, RackNumaMemInfo l2) { return l1.canBorrowMem > l2.canBorrowMem; });
    }
    return MEM_POOLING_OK;
}

void FaultNodeModule::MergeBorrowRecords(std::vector<BorrowRecord> borrowRecordList,
                                         std::vector<NodeBorrowRecord>& nodeBorrowRecordList)
{
#ifdef UB_ENVIRONMENT
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] [FaultLentNode] UB MergeBorrowRecords";
    std::unordered_map<std::string, NodeBorrowRecord> borrowRecordGroupByNodeIdMap;
    for (BorrowRecord record : borrowRecordList) {
        std::string key = record.borrowNode + "_" + record.lentNode + "_" + std::to_string(record.lentSocketId);
        uint16_t borrowSocketId{0};
        MemManager::Instance().GetSocketId(record.borrowNode, record.borrowLocalNuma, borrowSocketId);
        OnceBorrowRecord onceBorrowRecord{record.name,
                                          record.size,
                                          record.borrowMemId,
                                          static_cast<uint16_t>(record.borrowRemoteNuma),
                                          static_cast<uint16_t>(record.borrowLocalNuma),
                                          record.uid,
                                          record.username};
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
    for (NodeBorrowRecord& record : nodeBorrowRecordList) {
        // socket内也按照借用大小从大到小排序
        std::sort(record.onceBorrowRecordList.begin(), record.onceBorrowRecordList.end(),
                  [](OnceBorrowRecord l1, OnceBorrowRecord l2) { return l1.borrowSize > l2.borrowSize; });
    }
}

bool FaultNodeModule::MayBorrowFromNuma(NodeBorrowRecord nodeBorrowRecord,
                                        NodeMemoryInfoWithReservedMem& originNodeMemInfoItem,
                                        NodeMemoryInfoWithReservedMem nodeMemInfoItem,
                                        std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList)
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
                                                     std::vector<NodeMemoryInfoWithReservedMem>& ableNodeMemInfoList,
                                                     std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList)
{
    std::unordered_set<std::string> couldBorrowNodeSet;
    auto ret = MpParseGroupProviderConf::Instance().GetBorrowableList(nodeBorrowRecord.nodeId, couldBorrowNodeSet);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultManager] [FaultLentNode] GetBorrowableList failed.";
        return MEM_POOLING_ERROR;
    }
    std::vector<std::string> antiNodeMemVec;
    AntiNode::Instance().Query(nodeBorrowRecord.nodeId, antiNodeMemVec);
    for (const auto& node : antiNodeMemVec) {
        couldBorrowNodeSet.erase(node);
    }
    bool mustSamePlane = MpConfiguration::GetInstance().GetMustSamePlane();
    std::unordered_map<std::string, BorrowItem> borrowCacheAll;
    MemReturnManager::Instance().QueryAll(borrowCacheAll);
    for (auto& kv : borrowCacheAll) {
        if (kv.second.srcNid == nodeBorrowRecord.nodeId &&
            (!mustSamePlane ||
             MemManager::Instance().JudgeSampPlane(nodeBorrowRecord.nodeId, nodeBorrowRecord.borrowSocketId,
                                                   kv.second.dstNid, kv.second.dstSocketId))) {
            LOG_DEBUG << "[FaultManager] Node=" << kv.second.dstNid << " has return timeout records, erase it.";
            couldBorrowNodeSet.erase(kv.second.dstNid);
        }
    }
    for (NodeMemoryInfoWithReservedMem& nodeMemInfoItem : ableNodeMemInfoList) {
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
                                                 std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList,
                                                 std::vector<ForwardMemIdParam>& forwardMemIdParamList)
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
            [&nodeBorrowRecordItem](const NodeMemoryInfoWithReservedMem& l1, const NodeMemoryInfoWithReservedMem& l2) {
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

        for (auto& nodeMemInfo : ableNodeMemInfoList) {
            LOG_DEBUG << "NodeId = " << nodeMemInfo.nodeId << " , socketId = " << nodeMemInfo.socketId
                      << " , canBorrowMem = " << nodeMemInfo.canBorrowMem;
            for (auto& numaInfo : nodeMemInfo.numaMemInfo) {
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

MpResult FaultNodeModule::FillBorrowExecuteParam(std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList)
{
    if (borrowExecuteParamCollectList.empty()) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] [FaultLentNode] Execute list empty.";
        return MEM_POOLING_OK;
    }
    for (BorrowExecuteParam& param : borrowExecuteParamCollectList) {
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

bool FaultNodeModule::DealBorrowResult(std::vector<BorrowExecuteParam>& borrowExecuteParamList,
                                       std::vector<BorrowExecuteParam>& successExecuteParamCollectList,
                                       std::vector<MemBorrowExecuteResult> resultList)
{
    for (size_t i = 0; i < borrowExecuteParamList.size(); i++) {
        borrowExecuteParamList[i].newNumaId = resultList[i].presentNumaId[0];
        borrowExecuteParamList[i].newBorrowId = resultList[i].borrowIds[0];
        successExecuteParamCollectList.push_back(borrowExecuteParamList[i]);
    }
    return true;
}

void FaultNodeModule::DoExecuteBorrow(std::vector<BorrowExecuteParam>& successExecuteParamCollectList,
                                      std::pair<std::string, std::vector<BorrowExecuteParam>> nodeBorrowExecuteParam,
                                      std::vector<ForwardMemIdParam>& forwardMemIdParamList)
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

void FaultNodeModule::ExecuteBorrow(std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList,
                                    std::vector<BorrowExecuteParam>& successExecuteParamCollectList,
                                    std::vector<ForwardMemIdParam>& forwardMemIdParamList)
{
    if (borrowExecuteParamCollectList.empty()) {
        return;
    }
    // 按照接入节点分组 如果有一个执行失败就转为memId级故障处理
    std::unordered_map<std::string, std::vector<BorrowExecuteParam>> nodeBorrowExecuteParamMap;
    for (BorrowExecuteParam param : borrowExecuteParamCollectList) {
        nodeBorrowExecuteParamMap[param.borrowNodeId].push_back(param);
    }
    for (auto& nodeBorrowExecuteParam : nodeBorrowExecuteParamMap) {
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

MpResult FaultNodeModule::ExecuteNumaReplaceAndReturn(std::vector<BorrowExecuteParam>& borrowExecuteParamCollectList,
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

bool FaultNodeModule::CheckUBTurboIsAliveRpc(std::string nodeId)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] Master to invoke the slave CheckUBTurboIsAlive, nodeId=" << nodeId << ".";
    UbseComEndpoint endpoint = {
        .moduleId = MP_MODULE_CODE, .serviceId = message::OPCODE_CHECK_UBTURBO_IS_ALIVE, .address = nodeId};
    RmrsOutStream builder;
    builder << nodeId;
    UbseByteBuffer reqData = {
        .data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = [](uint8_t* data) {
            delete[] data;
        }};
    bool isAlive = false;
    auto ret = UbseRpcSend(endpoint, reqData, &isAlive, CheckUBTurboIsAliveResHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] CheckUBTurboIsAlive failed, ret = " << ret;
        return false;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] CheckUBTurboIsAlive success.";
    return isAlive;
}

uint32_t CheckUBTurboIsAliveHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] CheckUBTurboIsAliveHandler start.";
    resp.len = 1;
    resp.data = new (std::nothrow) uint8_t[resp.len]{};
    if (resp.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] Failed to allocate memory, size=" << resp.len << ".";
        return MEM_POOLING_ERROR;
    }
    resp.freeFunc = [](uint8_t* p) {
        if (p != nullptr) {
            delete[] p;
        }
    };
    turbo::rmrs::PidNumaInfoCollectParam pidNumaInfoCollectParam;
    turbo::rmrs::PidNumaInfoCollectResult pidNumaInfoCollectResult;
    auto ret = MempoolingMessage::rmrsPidNumaInfoCollect(pidNumaInfoCollectParam, pidNumaInfoCollectResult);
    if (ret == IPC_BAD_SOCKET || ret == IPC_BAD_CONNECT) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] UBTurbo is not alive, ret=" << ret << ".";
        resp.data[0] = static_cast<uint8_t>(0);
        return ret;
    } else {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] UBTurbo is alive, ret=" << ret << ".";
        resp.data[0] = static_cast<uint8_t>(1);
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] CheckUBTurboIsAliveHandler end.";
    return ret;
}

void CheckUBTurboIsAliveResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] CheckUBTurboIsAliveResHandler resCode=" << resCode;
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] Ctx or respData is null.";
        return;
    }
    auto* result = static_cast<bool*>(ctx);
    if (resCode != MEM_POOLING_OK || respData.data[0] != 1) {
        *result = false;
        return;
    }
    *result = true;
}

MpResult FaultNodeModule::FragmentHandleFault(std::string nodeId)
{
    faultHandleCurRound++;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] FragmentHandleFault round " << faultHandleCurRound << " start.";

    // =========基于不信任原则，获取账本并筛选合法条目===========
    // =========仅处理合法条目，处理完后返回失败，利用UBSE故障重试机制继续处理===========
    MpResult res = BorrowRecordHelper::Instance().UpdateBorrowRecordsWithFragmentFault(nodeId);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] UpdateBorrowRecordsWithFragmentFault failed.";
        return res;
    }

    NodeType nodeType = NodeType::ABNORMAL;
    res = FaultNodeModule::Instance().DetermineNodeTypeFragment(nodeId, nodeType);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] DetermineNodeType failed.";
        return MEM_POOLING_ERROR;
    }
    if (nodeType == NodeType::BORROW_IN) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] BORROW_IN Fault is handled by ubse.";
    } else if (nodeType == NodeType::BORROW_OUT) {
        res = FaultNodeModule::Instance().ProcessBorrowOutNodeFaultParallel(nodeId, true);
        if (res != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager] Process BORROW_OUT node fault failed.";
        }
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] FragmentHandleFault round " << faultHandleCurRound << " end.";
    // 处理完后返回失败，利用UBSE故障重试机制继续处理
    return MEM_POOLING_ERROR;
}

static void PrintCollectedInfos(const std::vector<BorrowGroupResult>& borrowGroups,
                                const std::vector<ClusterSnapshotItem>& baseSnapshot)
{
    // 打印 borrowGroups 详情
    for (const auto& group : borrowGroups) {
        // 构建虚机信息字符串
        std::string vmInfoStr;
        for (const auto& vm : group.vmInfos) {
            if (!vmInfoStr.empty())
                vmInfoStr += ", ";
            vmInfoStr +=
                "{pid:" + std::to_string(vm.pid) + ",remoteNumaUsedMem:" + std::to_string(vm.remoteNumaUsedMem) + "KB}";
        }
        // 构建账本信息字符串 —— 直接使用 BorrowRecord 的 ToString()
        std::string recordStr;
        for (const auto& rec : group.records) {
            if (!recordStr.empty())
                recordStr += ", ";
            recordStr += rec.ToString();
        }
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel][FaultHandleInfosCollect] BorrowGroup: borrowNode=" << group.borrowNodeId
            << ", faultNuma=" << group.remoteNumaId << ", borrowSocket=" << group.borrowSocketId
            << ", totalSize=" << group.totalSize << "KB"
            << ", totalUsedSize=" << group.totalUsedSize << "KB"
            << ", vmInfos=[" << vmInfoStr << "]"
            << ", borrowRecords=[" << recordStr << "]"
            << ", strategy=" << static_cast<int>(group.strategyType) << ".";
    }

    // 打印集群快照 baseSnapshot
    for (const auto& item : baseSnapshot) {
        std::string numaDetails;
        for (const auto& [numaId, blocks] : item.numaCanLentMap) {
            if (!numaDetails.empty())
                numaDetails += ", ";
            numaDetails += "numa" + std::to_string(numaId) + ":" +
                           std::to_string(blocks * FaultNodeModule::GetBlockSizeKB()) + "KB";
        }
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel][FaultHandleInfosCollect] ClusterSnapshot: node=" << item.nodeId
            << ", socket=" << item.socketId << ", canLentTotal=" << item.canLentMemSize << "KB"
            << ", numaDetails=[" << numaDetails << "]";
    }
}

// 按故障Numa（即borrowNodeId+remoteNumaId）分组
std::vector<BorrowGroupResult> FaultNodeModule::GroupBorrowRecordsByNuma(const std::vector<BorrowRecord>& records)
{
    std::unordered_map<std::string, BorrowGroupResult> groupMap;
    for (const auto& rec : records) {
        std::string key = rec.borrowNode + "_" + std::to_string(rec.borrowRemoteNuma);
        auto& group = groupMap[key];
        if (group.borrowNodeId.empty()) {
            group.borrowNodeId = rec.borrowNode;
            group.remoteNumaId = rec.borrowRemoteNuma;
            group.borrowSocketId = rec.borrowSocketId;
        }
        group.totalSize += rec.size;
        group.records.push_back(rec);
    }
    std::vector<BorrowGroupResult> groups;
    for (auto& [_, g] : groupMap) {
        std::sort(g.records.begin(), g.records.end(),
                  [](const BorrowRecord& a, const BorrowRecord& b) { return a.size > b.size; });
        groups.push_back(std::move(g));
    }
    std::sort(groups.begin(), groups.end(),
              [](const BorrowGroupResult& a, const BorrowGroupResult& b) { return a.totalSize > b.totalSize; });
    return groups;
}

static void ConvertToClusterSnapshot(const std::vector<NodeMemoryInfoWithReservedMem>& src,
                                     std::vector<ClusterSnapshotItem>& dst)
{
    const uint64_t blockSize = FaultNodeModule::GetBlockSizeKB();
    for (const auto& nodeInfo : src) {
        ClusterSnapshotItem item;
        item.nodeId = nodeInfo.nodeId;
        item.socketId = nodeInfo.socketId;
        item.freeMemSize = nodeInfo.canBorrowMem; // 原始未对齐大小
        item.totalBlocks = 0;
        for (const auto& numa : nodeInfo.numaMemInfo) {
            item.numaIds.push_back(numa.numaId);
            uint64_t blocks = numa.canBorrowMem / blockSize;
            if (blocks > 0) {
                item.numaCanLentMap[numa.numaId] = blocks;
                item.totalBlocks += blocks;
            }
        }
        item.canLentMemSize = item.totalBlocks * blockSize; // 按 block 对齐后的总大小
        dst.push_back(item);
    }
    std::sort(dst.begin(), dst.end(), [](const ClusterSnapshotItem& a, const ClusterSnapshotItem& b) {
        return a.canLentMemSize < b.canLentMemSize;
    });
}

// 向借入节点发起RPC，获取占用指定远端Numa的所有虚机信息
MpResult FaultNodeModule::GetVmOccupancyForGroup(const std::string& borrowNodeId, uint16_t remoteNumaId,
                                                 std::vector<FaultNumaVmInfo>& vmInfos)
{
    // 复用 OverCommitMsg::GetVmNumaInfoMapRpc，但该接口根据本地 numa 获取虚机，需要二次筛选
    std::vector<mempooling::VmNumaInfoWithSocket> allVms;
    MpResult ret = OverCommitFaultMemIdModule::Instance().GetVmNumaInfoMapRpc(borrowNodeId, allVms, remoteNumaId);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandlePara] GetVmNumaInfoMapRpc failed, borrowNodeId=" << borrowNodeId << ".";
        return ret;
    }
    for (const auto& v : allVms) {
        // 只保留占用了该远端 numa 的虚机（即remoteNumaId匹配）
        if (v.remoteNumaId == remoteNumaId && v.remoteUsedMem > 0) {
            FaultNumaVmInfo info;
            info.pid = v.pid;
            info.localNumaId = v.localNumaId;
            info.remoteNumaId = v.remoteNumaId;
            info.remoteNumaUsedMem = v.remoteUsedMem;
            vmInfos.push_back(info);
        }
    }

    if (vmInfos.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandlePara] GetVmInfos for Group failed, vmInfos empty, borrowNodeId=" << borrowNodeId
            << ", remoteNumaId=" << remoteNumaId << ".";
        return MEM_POOLING_OK;
    }
    std::sort(vmInfos.begin(), vmInfos.end(),
              [](const FaultNumaVmInfo& a, const FaultNumaVmInfo& b) {
                  return a.remoteNumaUsedMem > b.remoteNumaUsedMem;
              });
    return MEM_POOLING_OK;
}

// 从候选列表中 Best-Fit 分配 needBlocks 个block
// samePlaneOnly: true 只考虑同平面；false 优先同平面，若同平面无可用则考虑其他平面
// 成功返回true，result.selected:指向选中的节点；result.allocatedNumas:记录每个numa分配了多少block
bool FaultNodeModule::TryBestFitAllocate(const BorrowGroupResult& group,
                                         const std::vector<ClusterSnapshotItem*>& candidates, uint64_t needBlocks,
                                         bool samePlaneOnly,
                                         AllocResult& result) // 输出参数
{
    result.allocatedNumas.clear();
    if (candidates.empty()) {
        return false;
    }

    // 复制候选列表并按 socket 的 canLentMemSize 升序排序（Best‑Fit 外层顺序）
    std::vector<ClusterSnapshotItem*> sortedSockets = candidates;
    std::sort(sortedSockets.begin(), sortedSockets.end(),
              [](const ClusterSnapshotItem* a, const ClusterSnapshotItem* b) {
                  return a->canLentMemSize < b->canLentMemSize;
              });

    for (auto* socket : sortedSockets) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel] Try lentNodeId=" << socket->nodeId << ", socketId=" << socket->socketId << ".";
        // 同平面过滤（如果需要）
        if (samePlaneOnly) {
            bool isSamePlane = MemManager::Instance().JudgeSampPlane(socket->nodeId, socket->socketId,
                                                                     group.borrowNodeId, group.borrowSocketId);
            if (!isSamePlane) {
                UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[FaultHandleParallel] Not samePlane,skip. lentNodeId=" << socket->nodeId
                    << ", socketId=" << socket->socketId << ".";
                continue;
            }
        }

        // 将当前 socket 下的所有 numa 按可用 block 数升序排序（实现numa级别的Best‑Fit）
        std::vector<std::pair<uint16_t, uint64_t>> numaList(socket->numaCanLentMap.begin(),
                                                            socket->numaCanLentMap.end());
        std::sort(numaList.begin(), numaList.end(),
                  [](const auto& a, const auto& b) { return a.second < b.second; }); // 升序

        for (auto& [numaId, blocks] : numaList) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultHandleParallel] Try lentNodeId=" << socket->nodeId << ", socketId=" << socket->socketId
                << ", numaId=" << numaId << ",blocks=" << blocks << ", needBlocks=" << needBlocks << ".";
            if (blocks >= needBlocks) {
                // 找到合适的 numa，执行分配
                result.selected = socket;
                // 从该 numa 扣减
                socket->numaCanLentMap[numaId] = blocks - needBlocks;
                result.allocatedNumas.emplace_back(numaId, needBlocks); // 记录分配
                // 更新 socket 级别的总 block 数
                socket->totalBlocks -= needBlocks;
                socket->canLentMemSize = socket->totalBlocks * sBlockSizeKb;
                UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[FaultHandleParallel] Find success. lentNodeId=" << socket->nodeId
                    << ", socketId=" << socket->socketId << ", numaId=" << numaId << ",blocks=" << blocks
                    << ", needBlocks=" << needBlocks << ".";
                return true;
            }
        }
    }
    return false;
}

// 获取基础集群快照：只包含健康、非借入节点，按 socket 聚合，block 对齐，升序排序
MpResult FaultNodeModule::GetBaseClusterSnapshot(const std::string& faultNodeId,
                                                 std::vector<ClusterSnapshotItem>& clusterInfos)
{
    const uint64_t blockSize = sBlockSizeKb;

    // 1. 获取初始可借节点列表（基础过滤：工作状态、非借入节点等）
    std::vector<std::string> borrowAbleNodeIdList;
    MpResult res = GetBorrowAbleNodeIdList(faultNodeId, borrowAbleNodeIdList);
    if (res != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    if (borrowAbleNodeIdList.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[GetBaseClusterSnapshot] No candidate nodes for fault node " << faultNodeId;
        return MEM_POOLING_OK;
    }

    // 2. 获取这些节点的内存信息（按socket聚合）
    std::vector<NodeMemoryInfoWithReservedMem> ableNodeMemInfoList;
    res = GetBorrowAbleNodeInfoSortByMemSize(borrowAbleNodeIdList, ableNodeMemInfoList);
    if (res != MEM_POOLING_OK || ableNodeMemInfoList.empty()) {
        return MEM_POOLING_ERROR;
    }

    // 3. 转换为 ClusterSnapshotItem，并按 blockSize 对齐
    for (const auto& nodeInfo : ableNodeMemInfoList) {
        ClusterSnapshotItem item;
        item.nodeId = nodeInfo.nodeId;
        item.socketId = nodeInfo.socketId;
        item.freeMemSize = nodeInfo.canBorrowMem;
        item.totalBlocks = 0;
        for (const auto& numa : nodeInfo.numaMemInfo) {
            item.numaIds.push_back(numa.numaId);
            uint64_t blocks = numa.canBorrowMem / blockSize;
            if (blocks > 0) {
                item.numaCanLentMap[numa.numaId] = blocks;
                item.totalBlocks += blocks;
            }
        }
        item.canLentMemSize = item.totalBlocks * blockSize;
        if (item.totalBlocks > 0) {
            clusterInfos.push_back(item);
        }
    }

    // 4. 按 canLentMemSize 升序排序
    std::sort(clusterInfos.begin(), clusterInfos.end(), [](const ClusterSnapshotItem& a, const ClusterSnapshotItem& b) {
        return a.canLentMemSize < b.canLentMemSize;
    });

    return MEM_POOLING_OK;
}

// 根据借入节点ID和socketId对集群快照进行动态过滤，返回候选节点指针列表
std::vector<ClusterSnapshotItem*> FaultNodeModule::FilterSnapshotByBorrowNode(
    const std::string& borrowNodeId, uint16_t borrowSocketId, std::vector<ClusterSnapshotItem>& clusterInfos)
{
    std::vector<ClusterSnapshotItem*> candidates;

    // 1. 获取白名单
    std::unordered_set<std::string> couldBorrowNodeSet;
    MpResult ret = MpParseGroupProviderConf::Instance().GetBorrowableList(borrowNodeId, couldBorrowNodeSet);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FilterSnapshot] GetBorrowableList failed for node " << borrowNodeId << ".";
        return candidates;
    }

    // 2. 反亲和节点
    std::vector<std::string> antiNodeVec;
    AntiNode::Instance().Query(borrowNodeId, antiNodeVec);
    for (const auto& antiNode : antiNodeVec) {
        couldBorrowNodeSet.erase(antiNode);
    }

    // 3. 成环过滤：排除自身是借入节点的节点（即不能向也借入了内存的节点借用）
    std::vector<BorrowRecord> borrowRecords;
    ret = BorrowRecordHelper::Instance().CollectBorrowRecordsAll(borrowRecords, true, true);
    if (ret == MEM_POOLING_OK) {
        for (const auto& record : borrowRecords) {
            couldBorrowNodeSet.erase(record.borrowNode);
        }
    }

    // 4. 归还超时记录过滤
    std::unordered_map<std::string, BorrowItem> borrowCacheAll;
    MemReturnManager::Instance().QueryAll(borrowCacheAll);
    for (const auto& kv : borrowCacheAll) {
        if (kv.second.srcNid == borrowNodeId &&
            MemManager::Instance().JudgeSampPlane(borrowNodeId, borrowSocketId, kv.second.dstNid,
                                                  kv.second.dstSocketId)) {
            couldBorrowNodeSet.erase(kv.second.dstNid);
        }
    }

    // 5. 筛选clusterInfos中满足白名单的节点
    for (auto& item : clusterInfos) {
        if (couldBorrowNodeSet.find(item.nodeId) != couldBorrowNodeSet.end()) {
            candidates.push_back(&item);
        }
    }
    return candidates;
}

void FaultNodeModule::NumaLevelDecisionFill(BorrowGroupResult& group, ClusterSnapshotItem& best, uint64_t needBlocks,
                                            std::vector<std::pair<uint16_t, uint64_t>>& allocatedNumas)
{
    NumaLevelDecision decision;
    for (const auto& record : group.records) {
        decision.oldNames.push_back(record.name);
    }
    decision.borrowNumaId = group.records[0].borrowLocalNuma;
    decision.borrowSocketId = group.records[0].borrowSocketId;
    decision.lentNodeId = best.nodeId;
    decision.lentSocketId = best.socketId;
    decision.borrowSize = needBlocks * sBlockSizeKb;
    for (auto [numaId, blocks] : allocatedNumas) {
        decision.lentNumaId = numaId;
        decision.lentMemSize = blocks * sBlockSizeKb;
        decision.lentNumaIdAndSizeMap[numaId] = blocks * sBlockSizeKb;
    }
    for (const auto& vm : group.vmInfos) {
        decision.pids.push_back(vm.pid);
    }
    RemoveMigratedPidsFromVmInfos(group.vmInfos, decision.pids);
    if (!group.records.empty()) {
        decision.uid = group.records[0].uid;
        decision.username = group.records[0].username;
    }
    group.numaDecision = decision;
}

static void printNumaLevelDecisions(const std::vector<BorrowGroupResult>& borrowGroups, int successCnt)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[NumaLevelDecision] Total groups=" << borrowGroups.size()
                                                     << ", numa-level strategy success count=" << successCnt << ".";

    for (const auto& group : borrowGroups) {
        if (group.strategyType != BorrowStrategyType::NUMA_LEVEL_STRATEGY) {
            continue;
        }

        const auto& decision = group.numaDecision;
        std::string oldNamesStr;
        std::string pidsStr;
        for (size_t i = 0; i < decision.oldNames.size(); ++i) {
            if (i != 0)
                oldNamesStr += ",";
            oldNamesStr += decision.oldNames[i];
        }

        if (decision.isReturnDirectly == true) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[NumaLevelDecision] BorrowGroup borrowNodeId=" << group.borrowNodeId
                << ", faultNuma=" << group.remoteNumaId << " generate numa-level decision success. "
                << "Decide to return directly oldNames=[" << oldNamesStr << "].";
            continue;
        }

        for (size_t i = 0; i < decision.pids.size(); ++i) {
            if (i != 0)
                pidsStr += ",";
            pidsStr += std::to_string(decision.pids[i]);
        }
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[NumaLevelDecision] BorrowGroup borrowNodeId=" << group.borrowNodeId
            << ", faultNuma=" << group.remoteNumaId << " generate numa-level decision success. "
            << "Decision: borrow from lentNodeId=" << decision.lentNodeId << ", lentSocketId=" << decision.lentSocketId
            << ", lentNumaId=" << decision.lentNumaId << ", lentMemSize=" << decision.lentMemSize << "KB"
            << ", migrate these pids=[" << pidsStr << "]"
            << ", return oldNames=[" << oldNamesStr << "].";
    }
}

void FaultNodeModule::GenerateNumaLevelDecision(std::vector<BorrowGroupResult>& borrowGroups,
                                                std::vector<ClusterSnapshotItem>& baseSnapshot, bool mustSamePlane)
{
    int successCnt = 0;
    for (auto& group : borrowGroups) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel] start to generate numa-level decision for borrowGroup which's borrowNodeId="
            << group.borrowNodeId << ", faultNumaId=" << group.remoteNumaId << ".";
        if (group.vmInfos.empty()) {
            group.strategyType = BorrowStrategyType::NUMA_LEVEL_STRATEGY;
            group.numaDecision.isReturnDirectly = true;
            for (const auto& record : group.records) {
                group.numaDecision.oldNames.push_back(record.name);
            }
            successCnt++;
            continue;
        }

        // 1. 动态过滤，得到该borrowNode的候选节点列表
        auto candidates = FilterSnapshotByBorrowNode(group.borrowNodeId, group.borrowSocketId, baseSnapshot);
        if (candidates.empty()) {
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[NumaLevelDecision] No candidates for borrowNode " << group.borrowNodeId << ".";
            continue;
        }

        // 2. 使用 group.totalSize 计算 needBlocks（账本总借用大小）
        uint64_t needBlocks = (group.totalSize + GetBlockSizeKB() - 1) / GetBlockSizeKB();
        AllocResult allocResult;
        bool allocateSuccess = false;

        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel] borrowGroup which's borrowNodeId=" << group.borrowNodeId
            << ", faultNumaId=" << group.remoteNumaId << ", needBlocks=" << needBlocks << ".";

        // 同平面决策
        if (TryBestFitAllocate(group, candidates, needBlocks, true, allocResult)) {
            allocateSuccess = true;
        }
        // 不限制同平面 且 同平面下借用决策失败，尝试非同平面借用决策
        if (!mustSamePlane && !allocateSuccess) {
            if (TryBestFitAllocate(group, candidates, needBlocks, false, allocResult)) {
                allocateSuccess = true;
            }
        }
        if (!allocateSuccess) {
            // 决策失败则跳过该故障numa到下一个，该故障numa继续再borrowId-level进行借用决策
            continue;
        }

        // 填充决策结果
        group.strategyType = BorrowStrategyType::NUMA_LEVEL_STRATEGY;
        successCnt++;
        NumaLevelDecisionFill(group, *allocResult.selected, needBlocks, allocResult.allocatedNumas);
    }

    printNumaLevelDecisions(borrowGroups, successCnt);
}

void FaultNodeModule::RemoveMigratedPidsFromVmInfos(std::vector<FaultNumaVmInfo>& vmInfos,
                                                    const std::vector<pid_t>& pids)
{
    for (pid_t pid : pids) {
        auto it =
            std::find_if(vmInfos.begin(), vmInfos.end(), [pid](const FaultNumaVmInfo& vm) { return vm.pid == pid; });
        if (it != vmInfos.end()) {
            vmInfos.erase(it);
        }
    }
}

void FaultNodeModule::GenerateBorrowIdLevelDecision(std::vector<BorrowGroupResult>& borrowGroups,
                                                    std::vector<ClusterSnapshotItem>& baseSnapshot, bool mustSamePlane)
{
    for (auto& group : borrowGroups) {
        if (group.strategyType == BorrowStrategyType::NUMA_LEVEL_STRATEGY) {
            continue;
        }

        // 动态过滤
        auto candidates = FilterSnapshotByBorrowNode(group.borrowNodeId, group.borrowSocketId, baseSnapshot);
        if (candidates.empty()) {
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[BorrowIdLevelDecision] No candidates for borrowNode " << group.borrowNodeId << ".";
            continue;
        }

        int errCount = 0;
        std::vector<BorrowIdLevelDecision> tmpDecisions;
        for (const auto& record : group.records) {
            BorrowIdLevelDecision decision;
            decision.oldName = record.name;
            // 当故障numa上没有虚机占用，且前面的决策没有失败的情况下，直接归还该oldBorrowId
            if (group.vmInfos.empty() && errCount == 0) {
                decision.isReturnDirectly = true;
                tmpDecisions.push_back(decision);
                continue;
            }

            if (!group.records.empty()) {
                decision.uid = group.records[0].uid;
                decision.username = group.records[0].username;
            }

            // 转换虚机信息格式
            std::vector<VmNumaInfo> tmp;
            for (const auto& vm : group.vmInfos) {
                tmp.push_back({vm.pid, vm.localNumaId, vm.remoteNumaId, 0, vm.remoteNumaUsedMem});
            }
            std::vector<pid_t> pids;
            uint64_t needBorrowSize = 0;
            MpResult ret =
                FaultMemIdModule::Instance().FindClosestVmForMemAlloc(tmp, record.size, pids, needBorrowSize);
            if (ret != MEM_POOLING_OK || needBorrowSize == 0) {
                continue;
            }

            uint64_t needBlocks = (needBorrowSize + sBlockSizeKb - 1) / sBlockSizeKb;
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultHandleParallel] borrowGroup which's borrowNodeId=" << group.borrowNodeId
                << ", faultNumaId=" << group.remoteNumaId << ", needBorrowSize=" << needBorrowSize
                << ", needBlocks=" << needBlocks << ".";
            needBorrowSize = needBlocks * sBlockSizeKb;

            AllocResult allocResult;
            bool allocateSuccess = false;
            if (TryBestFitAllocate(group, candidates, needBlocks, true, allocResult)) {
                allocateSuccess = true;
            }
            if (!mustSamePlane && !allocateSuccess) {
                if (TryBestFitAllocate(group, candidates, needBlocks, false, allocResult)) {
                    allocateSuccess = true;
                }
            }
            if (!allocateSuccess) {
                errCount++;
                continue;
            }
            decision.oldName = record.name;
            decision.borrowNumaId = record.borrowLocalNuma;
            decision.borrowSocketId = record.borrowSocketId;
            decision.needBorrowSize = needBorrowSize;
            decision.pids = pids;
            decision.lentNodeId = allocResult.selected->nodeId;
            decision.lentSocketId = allocResult.selected->socketId;
            for (auto [numaId, block] : allocResult.allocatedNumas) {
                decision.lentNumaId = numaId;
                decision.lentMemSize = block * sBlockSizeKb;
                decision.lentNumaIdAndSizeMap.emplace(numaId, block * sBlockSizeKb);
            }
            tmpDecisions.push_back(decision);
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultHandleParallel] borrowIdDecision tmp generate success. {oldName=" << decision.oldName
                << " borrow from lentNodeId=" << decision.lentNodeId << ", lentSocketId=" << decision.lentSocketId
                << ", lentNumaId=" << decision.lentNumaId << ", lentMemSize=" << decision.lentMemSize << "KB}";

            RemoveMigratedPidsFromVmInfos(group.vmInfos, decision.pids);
        }
        group.borrowIdDecisions = std::move(tmpDecisions);
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel] group.borrowIdDecisions.size=" << group.borrowIdDecisions.size() << ".";
        group.strategyType = (group.borrowIdDecisions.empty()) ? BorrowStrategyType::STRATEGY_FAILED :
                                                                 BorrowStrategyType::BORROW_ID_LEVEL_STRATEGY;
    }
}

static void printBorrowIdLevelDecisions(const std::vector<BorrowGroupResult>& borrowGroups)
{
    int borrowIdCount = 0;
    for (const auto& group : borrowGroups) {
        if (group.strategyType == BorrowStrategyType::BORROW_ID_LEVEL_STRATEGY) {
            borrowIdCount++;
        }
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultHandleParallel] borrowIdDecisionGroup count=" << borrowIdCount << ".";

    for (const auto& group : borrowGroups) {
        if (group.strategyType != BorrowStrategyType::BORROW_ID_LEVEL_STRATEGY) {
            continue;
        }

        std::string decisionsStr;
        for (const auto& decision : group.borrowIdDecisions) {
            if (!decisionsStr.empty()) {
                decisionsStr += ", ";
            }
            if (decision.isReturnDirectly) {
                decisionsStr += "{oldName=" + decision.oldName + " : isReturnDirectly=true}";
            } else {
                decisionsStr += "{oldName=" + decision.oldName + " borrow from lentNodeId=" + decision.lentNodeId +
                                ", lentSocketId=" + std::to_string(decision.lentSocketId) +
                                ", lentNumaId=" + std::to_string(decision.lentNumaId) +
                                ", lentMemSize=" + std::to_string(decision.lentMemSize) + "KB}";
            }
        }

        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel][BorrowIdLevelDecision] BorrowGroup: borrowNodeId=" << group.borrowNodeId
            << ", faultNuma=" << group.remoteNumaId << ", totalRecords=" << group.records.size()
            << ", decisionsGenerated=" << group.borrowIdDecisions.size() << ", Decisions:[" << decisionsStr << "].";
    }
}

MpResult FaultNodeModule::FaultHandleBorrowStrategy(std::vector<BorrowGroupResult>& borrowGroups,
                                                    std::vector<ClusterSnapshotItem>& baseSnapshot)
{
    // 确保基础快照升序（已在 GetBaseClusterSnapshot 中完成，这里再确保一次）
    bool mustSamePlane = MpConfiguration::GetInstance().GetMustSamePlane();

    // 1. NUMA 级别决策
    GenerateNumaLevelDecision(borrowGroups, baseSnapshot, mustSamePlane);
    PrintCollectedInfos(borrowGroups, baseSnapshot);

    // 2. BorrowId 级别决策
    GenerateBorrowIdLevelDecision(borrowGroups, baseSnapshot, mustSamePlane);
    // 打印 BorrowId 级别决策
    printBorrowIdLevelDecisions(borrowGroups);
    PrintCollectedInfos(borrowGroups, baseSnapshot);

    // 统计完全失败的组数
    int totalFailed = 0;
    for (const auto& group : borrowGroups) {
        if (group.strategyType == BorrowStrategyType::STRATEGY_FAILED) {
            totalFailed++;
        }
    }
    if (totalFailed == (int)borrowGroups.size()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel][FaultHandleBorrowStrategy] All BorrowGroups generate strategy failed.";
        return MEM_POOLING_ERROR;
    }

    if (totalFailed > 0) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel][FaultHandleBorrowStrategy] totalFailed count=" << totalFailed << " out of "
            << borrowGroups.size() << " groups.";
        return MEM_POOLING_PARTIAL_OK;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultHandleParallel][FaultHandleBorrowStrategy] All BorrowGroups generate strategy successfully.";
    return MEM_POOLING_OK;
}

MpResult FaultNodeModule::FaultHandleInfosCollect(const std::string& faultNodeId,
                                                  std::vector<BorrowGroupResult>& borrowGroups,
                                                  std::vector<ClusterSnapshotItem>& baseSnapshot)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultHandleInfosCollect] Faulthandle parallel collect infos start.";
    // 1. 获取账本并分组
    std::vector<BorrowRecord> borrowRecords;
    MpResult res = GetBorrowNodeInfo(faultNodeId, borrowRecords);
    if (res != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    if (borrowRecords.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultHandleInfosCollect] Get borrowRecords are empty.";
        return MEM_POOLING_ERROR;
    }
    // 初始化块大小（根据故障节点配置）
    SetBlockSizeKB(borrowRecords[0].borrowNode);

    borrowGroups = GroupBorrowRecordsByNuma(borrowRecords);
    if (borrowGroups.empty()) {
        return MEM_POOLING_ERROR;
    }

    // 2. 采集虚机信息，并剔除采集失败的组
    for (auto it = borrowGroups.begin(); it != borrowGroups.end();) {
        std::vector<FaultNumaVmInfo> vmInfos;
        if (GetVmOccupancyForGroup(it->borrowNodeId, it->remoteNumaId, vmInfos) != MEM_POOLING_OK) {
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultHandleInfosCollect] Failed to get VM info for (borrowNodeId="
                << it->borrowNodeId << ", faultNuma=" << it->remoteNumaId << "), remove this group.";
            it = borrowGroups.erase(it);   // 采集失败，删除当前组，iterator指向下一个
            continue;
        }
        it->vmInfos = std::move(vmInfos);
        it->totalUsedSize = 0;
        for (const auto& vmInfo : it->vmInfos) {
            it->totalUsedSize += vmInfo.remoteNumaUsedMem;
        }
        ++it;
    }

    // 如果所有组都被剔除，则无需继续
    if (borrowGroups.empty()) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleInfosCollect] No valid borrow groups after VM info collection, skip.";
        return MEM_POOLING_ERROR;
    }

    // 3. 获取基础集群快照
    res = GetBaseClusterSnapshot(faultNodeId, baseSnapshot);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Get cluster sapShot failed, ret=" << res << ".";
        baseSnapshot.clear();
    }

    // 打印采集结果
    PrintCollectedInfos(borrowGroups, baseSnapshot);
    return MEM_POOLING_OK;
}

MpResult FaultNodeModule::SendNumaLevelExecuteRpc(std::string& nodeId, BorrowGroupResult& group, MpResult& outResult)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] Master to invoke the slave SendNumaLevelExecuteRpc, node=" << nodeId << ".";
    UbseComEndpoint endpoint = {
        .moduleId = MP_MODULE_CODE, .serviceId = message::OPCODE_NUMA_LEVEL_EXECUTE, .address = nodeId};
    RmrsOutStream builder;
    builder << group; // 直接序列化整个 BorrowGroupResult
    UbseByteBuffer reqData = {
        .data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = [](uint8_t* data) {
            delete[] data;
        }};
    MpResult ret = MEM_POOLING_ERROR;
    uint32_t rpcRet = UbseRpcSend(endpoint, reqData, &ret, NumaLevelExecuteResHandler);
    if (rpcRet != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "SendNumaLevelExecuteRpc RPC send failed, ret=" << rpcRet;
        return MEM_POOLING_ERROR;
    }
    outResult = ret;
    return MEM_POOLING_OK;
}

void NumaLevelExecuteResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) 
            << "[FaultHandleParallel] NumaLevelExecuteResHandler invalid param.";
        return;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultHandleParallel] resCode=" << resCode << ".";
    MpResult* result = static_cast<MpResult*>(ctx);
    if (resCode != MEM_POOLING_OK) {
        *result = MEM_POOLING_ERROR;
        return;
    }
    if (respData.len != sizeof(MpResult)) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "NumaLevelExecuteResHandler unexpected resp len=" << respData.len;
        *result = MEM_POOLING_ERROR;
        return;
    }
    *result = *reinterpret_cast<const MpResult*>(respData.data);
}

MpResult FaultNodeModule::SendBorrowIdExecuteRpc(std::string& nodeId, BorrowGroupResult& group, MpResult& outResult)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultHandleParallel][FaultManager] SendBorrowIdExecuteRpc Master to invoke slave node=" << nodeId << ".";

    UbseComEndpoint endpoint = {
        .moduleId = MP_MODULE_CODE, .serviceId = message::OPCODE_BORROW_ID_LEVEL_EXECUTE, .address = nodeId};
    RmrsOutStream builder;
    builder << group;
    UbseByteBuffer reqData = {
        .data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = [](uint8_t* data) {
            delete[] data;
        }};
    MpResult ret = MEM_POOLING_ERROR;
    uint32_t rpcRet = UbseRpcSend(endpoint, reqData, &ret, BorrowIdLevelExecuteResHandler);
    if (rpcRet != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "SendBorrowIdExecuteRpc RPC send failed, ret=" << rpcRet << ".";
        return MEM_POOLING_ERROR;
    }
    outResult = ret;
    return MEM_POOLING_OK;
}

void BorrowIdLevelExecuteResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel] BorrowIdLevelExecuteResHandler invalid param.";
        return;
    }
    auto* result = static_cast<MpResult*>(ctx);
    if (resCode != MEM_POOLING_OK) {
        *result = MEM_POOLING_ERROR;
        return;
    }
    if (respData.len != sizeof(MpResult)) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "BorrowIdLevelExecuteResHandler unexpected resp len=" << respData.len << ".";
        *result = MEM_POOLING_ERROR;
        return;
    }
    *result = *reinterpret_cast<const MpResult*>(respData.data);
}

MpResult FaultNodeModule::FaultHandleExecuteParallel(std::vector<BorrowGroupResult>& borrowGroups)
{
    std::vector<std::future<MpResult>> futures;
    // 1. 处理 NUMA 级别决策的组
    for (auto& group : borrowGroups) {
        if (group.strategyType != BorrowStrategyType::NUMA_LEVEL_STRATEGY) {
            continue;
        }

        futures.push_back(std::async(std::launch::async, [this, &group]() -> MpResult {
            MpResult result;
            MpResult sendRet = SendNumaLevelExecuteRpc(group.borrowNodeId, group, result);
            if (sendRet != MEM_POOLING_OK) {
                return MEM_POOLING_ERROR;
            }
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultHandleParallel] borrowNodeId=" << group.borrowNodeId << ", faultNuma=" << group.remoteNumaId
                << ",sendRet=" << sendRet << ", result=" << result << ".";
            return result;
        }));
    }

    // 2. 处理 BorrowId 级别决策的组
    for (auto& group : borrowGroups) {
        if (group.strategyType != BorrowStrategyType::BORROW_ID_LEVEL_STRATEGY) {
            continue;
        }

        futures.push_back(std::async(std::launch::async, [this, &group]() -> MpResult {
            MpResult result;
            MpResult sendRet = SendBorrowIdExecuteRpc(group.borrowNodeId, group, result);
            if (sendRet != MEM_POOLING_OK) return MEM_POOLING_ERROR;
            return result;
        }));
    }

    // 3. 等待所有任务完成并统计失败数
    int failCount = 0;
    int successCnt = 0;
    for (auto& f : futures) {
        MpResult res = f.get();
        if (res != MEM_POOLING_OK) {
            failCount++;
        } else {
            successCnt++;
        }
    }
    UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultHandleParallel] Parallel execute end, success count="
        << successCnt << ", fail count=" << failCount << ".";

    if (failCount == 0) {
        return MEM_POOLING_OK;
    }
    if (successCnt == 0) {
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_PARTIAL_OK;
}

MpResult FaultNodeModule::FaultHandleMigrate(uint16_t presentNumaId, uint16_t faultNumaId,
                                             std::vector<pid_t>& pids, uint64_t borrowMemSize)
{
    std::string pidStr{};
    for (auto pid : pids) {
        pidStr += std::to_string(pid) + ",";
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultHandleParallel][FaultHandleMigrate] FaultHandleMigrate start, presentNumaId=" << presentNumaId
        << ", faultNumaId=" << faultNumaId << ", borrowMemSize=" << borrowMemSize << ", pids=[" << pidStr << "].";

    // 分配大页
    auto res = FaultMemIdExecute::Instance().EchoHugepages(presentNumaId, borrowMemSize);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel][FaultHandleMigrate] Echo Hugepages failed.";
        return MEM_POOLING_ERROR;
    }
    
    // 远端迁移
    std::string borrowId{};
    res = FaultMemIdExecute::Instance().VmsMigrateOtherRemoteNuma(pids, faultNumaId, presentNumaId, borrowId, false);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel][FaultHandleMigrate] VmsMigrateOtherRemoteNuma failed. oldNumaId=" << faultNumaId
            << ", newNumaId=" << presentNumaId << ".";
        // 远端迁移失败，回滚释放此次分配的大页
        FaultMemIdExecute::Instance().RollBackHugepages(presentNumaId, borrowMemSize);
        return MEM_POOLING_ERROR;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultHandleParallel][FaultHandleMigrate] FaultHandleMigrate success."
    return MEM_POOLING_OK;
}

MpResult FaultNodeModule::BorrowIdLevelMemBorrow(const BorrowGroupResult& group, BorrowIdLevelDecision decision)
{
    // 1 借用新内存
    SrcMemoryBorrowParam srcParam{
        group.borrowNodeId, static_cast<int16_t>(group.borrowSocketId),
        static_cast<int16_t>(decision.borrowNumaId), decision.uid, decision.username};
    MemBorrowExecuteResult borrowExecuteResult;
    std::vector<DestMemoryBorrowParam> destParamList;
    uint16_t destNumaNum = 1;
    std::vector<int> destNumaIdList{decision.lentNumaId};
    std::vector<uint64_t> memSizeList{decision.lentMemSize};
    DestMemoryBorrowParam destMemoryBorrowParam{decision.lentNodeId, decision.lentSocketId,
                                                destNumaNum, destNumaIdList, memSizeList};
    destParamList.push_back(destMemoryBorrowParam);
    MpResult res = MempoolBorrowModule::Instance().MemBorrowExecute(srcParam, destParamList, borrowExecuteResult);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] [NumaIdLevelExecute] Borrow mem error srcNode=" << group.borrowNodeId
            << ",destNode=" << decision.lentNodeId << ",size=" << decision.lentMemSize << ".";
        return MEM_POOLING_ERROR;
    }
}

MpResult FaultNodeModule::BorrowIdLevelExecute(const BorrowGroupResult& group, BorrowIdLevelDecision decision)
{
    // 1 借用新内存
    SrcMemoryBorrowParam srcParam{
        group.borrowNodeId, static_cast<int16_t>(group.borrowSocketId),
        static_cast<int16_t>(decision.borrowNumaId), decision.uid, decision.username};
    MemBorrowExecuteResult borrowExecuteResult;
    std::vector<DestMemoryBorrowParam> destParamList;
    uint16_t destNumaNum = 1;
    std::vector<int> destNumaIdList{decision.lentNumaId};
    std::vector<uint64_t> memSizeList{decision.lentMemSize};
    DestMemoryBorrowParam destMemoryBorrowParam{decision.lentNodeId, decision.lentSocketId,
                                                destNumaNum, destNumaIdList, memSizeList};
    destParamList.push_back(destMemoryBorrowParam);
    MpResult res = MempoolBorrowModule::Instance().MemBorrowExecute(srcParam, destParamList, borrowExecuteResult);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] [NumaIdLevelExecute] Borrow mem error srcNode=" << group.borrowNodeId
            << ",destNode=" << decision.lentNodeId << ",size=" << decision.lentMemSize << ".";
        return MEM_POOLING_ERROR;
    }

    // 2 分配大页+远端迁移
    res = FaultNodeModule::FaultHandleMigrate(borrowExecuteResult.presentNumaId[0], group.remoteNumaId,
                                              decision.pids, decision.lentMemSize);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] [BorrowIdLevelExecute] FaultHandleMigrate error oldNumaId=" << group.remoteNumaId
            << ", newNumaId=" << borrowExecuteResult.presentNumaId[0] << ".";
        // 迁移失败需将此次借用记录存储到数据库中，以便下次故障处理时直接使用此次借过来的内存；
        // borrowIdLevel只需要borrowExecuteResult+BorrowIdLevelDecision(pids+memSize++oldName)+faultNuma
        // 形式可以是Map<borrowGroup+oldName : borrowExecuteResult+BorrowIdLevelDecision>
        return MEM_POOLING_ERROR;
    }

    // 3 归还旧内存
    res = MemBorrowExecutor::Instance().MemFreeWithOps(decision.oldName, false, true, true);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] [FaultLentNode] MemFreeWithOps error oldName=" << decision.oldName << ".";
        return MEM_POOLING_ERROR;
    }
    // 4 归还成功后更新借用描述符的重定向关系表
    MpResult retBorrId = BorrowIdRedirection::Instance().Update(oldName, borrowExecuteResult.borrowIds[0]);
    if (retBorrId != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel][NumaIdLevelExecute] Update borrowId redirection failed. oldName="
            << decision.oldName << ", newBorrowId=" << borrowExecuteResult.borrowIds[0] << ".";
    }

    return MEM_POOLING_OK;
}

void BorrowIdLevelReturnDirectly(const BorrowIdLevelDecision& decision, int errCount)
{
    if (decision.isReturnDirectly && errCount > 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[BorrowIdLevelExecuteHandler] borrowId=" << decision.oldName
            << " should be directly return, but there are execute error before.";
        return;
    }

    if (decision.isReturnDirectly && errCount == 0) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[BorrowIdLevelExecuteHandler] Start to free directly for oldName=" << decision.oldName << ".";
        auto ret = MemBorrowExecutor::Instance().MemFreeWithOps(decision.oldName, false, true, true);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[BorrowIdLevelExecuteHandler] direct free oldName=" << decision.oldName << "failed. ret=" << ret
                << ".";
        } else {
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[BorrowIdLevelExecuteHandler] direct free oldName=" << decision.oldName << " success.";
        }
        return;
    }
}

uint32_t BorrowIdLevelExecuteHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultNode] BorrowIdLevelExecuteHandler start.";
    BorrowGroupResult group;
    RmrsInStream inBuilder(req.data, req.len);
    inBuilder >> group;
    int errCount = 0;
    MpResult ret;
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[BorrowIdLevelExecuteHandler] BorrowNodeId=" << group.borrowNodeId << ", faultNumaId=" << group.remoteNumaId
        << " start to execute fault handle";

    for (const auto& decision : group.borrowIdDecisions) {
        if (decision.isReturnDirectly) {
            BorrowIdLevelReturnDirectly(decision, errCount);
            continue;
        }

        // 决策结果执行函数（不包括直接归还决策）
        ret = FaultNodeModule::Instance().BorrowIdLevelExecute(group, decision);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[BorrowIdLevelExecuteHandler] BorrowIdLevelExecute failed. ret=" << ret << ".";
            errCount++;
        }
        continue;
    }

    resp.freeFunc = [](uint8_t* data) {
        delete[] data;
    };

    if (errCount > 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel][BorrowIdLevelExecuteHandler] BorrowIdLevelExecute errCount=" << errCount << ".";
        resp.len = RPC_RESP_LENGTH;
        ret = MEM_POOLING_ERROR;
    } else {
        resp.len = sizeof(MpResult);
        ret = MEM_POOLING_OK;
    }

    resp.data = new (std::nothrow) uint8_t[resp.len];
    if (resp.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel][BorrowIdLevelExecuteHandler] NumaLevelExecute failed, size=" << resp.len << ".";
        return MEM_POOLING_ERROR;
    }
    *reinterpret_cast<MpResult*>(resp.data) = ret;

    return ret;
}

MpResult FaultNodeModule::NumaLevelMemBorrow(const BorrowGroupResult& group, NumaLevelDecision decision,
                                             std::map<std::string, MemBorrowExecuteResult>& tmpRedirectionMap,
                                             uint16_t& presentNumaId)
{
    // numa级别借用，新借内存的账本数量和Size应和group中的records记录保持一致
    SrcMemoryBorrowParam srcParam{
        group.borrowNodeId, static_cast<int16_t>(group.borrowSocketId),
        static_cast<int16_t>(decision.borrowNumaId), decision.uid, decision.username};
    MemBorrowExecuteResult borrowExecuteResult;
    uint64_t totalBorrowSize{};

    // 遍历group中的records，对每个record进行相应的borrow
    for (auto& record : group.records) {
        std::vector<DestMemoryBorrowParam> destParamList;
        uint16_t destNumaNum = 1;
        std::vector<int> destNumaIdList{decision.lentNumaId};
        std::vector<uint64_t> memSizeList{record.size};     // 对应该账本的size
        DestMemoryBorrowParam destMemoryBorrowParam{decision.lentNodeId, decision.lentSocketId,
                                                    destNumaNum, destNumaIdList, memSizeList};
        destParamList.push_back(destMemoryBorrowParam);
        MpResult res = MempoolBorrowModule::Instance().MemBorrowExecute(srcParam, destParamList, borrowExecuteResult);
        if (res != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultHandleParallel][NumaIdLevelExecute] FaultHandle borrow mem for oldName=" << record.name
                << " failed, destNode=" << decision.lentNodeId << ", borrowSize=" << record.size << ".";
            // 如果有任何一个borrow失败，需要回滚归还之前的所有借用的内存
            for (auto &[_, borrowExecuteResult] : tmpRedirectionMap) {
                // 第三个参数smapBack设为false，不走smapMigrateBack逻辑，直接进行ubse归还
                auto newName = borrowExecuteResult.borrowIds[0];
                auto ret = MemBorrowExecutor::Instance().MemFreeWithOps(newName, false, false, true);
                if (ret != MEM_POOLING_OK) {
                    UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultHandleParallel][NumaIdLevelExecute]"
                        << "Cause MemBorrow failed, need to free borrowRecord which successed before, borrowId="
                        << newName << ", but memFree failed, ret=" << ret << ".";
                }
            }
            return MEM_POOLING_ERROR;
        }
        tmpRedirectionMap[record.name] = borrowExecuteResult;
        presentNumaId = borrowExecuteResult.presentNumaId[0];
        totalBorrowSize += record.size;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultHandleParallel][NumaIdLevelExecute]"<< "NumaLevelMemBorrow success, presentNumaId=" << presentNumaId
        << ", borrowCount=" << tmpRedirectionMap.size() << ", totalBorrowSize=" << totalBorrowSize << "."
    return MEM_POOLING_OK;
}

MpResult FaultNodeModule::NumaLevelExecute(const BorrowGroupResult& group, NumaLevelDecision decision,
                                           std::map<std::string, MemBorrowExecuteResult>& tmpRedirectionMap)
{
    if (decision.isReturnDirectly) {
        // 直接归还旧内存
        for (auto oldName : decision.oldNames) {
            auto ret = MemBorrowExecutor::Instance().MemFreeWithOps(oldName, false, true, true);
            if (ret != MEM_POOLING_OK) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[FaultHandleParallel][NumaIdLevelExecute] MemFreeWithOps error oldName " << oldName << ".";
                return MEM_POOLING_ERROR;
            }
        }
        return MEM_POOLING_OK;
    }

    // 1 借用新内存
    uint16_t presentNumaId{};
    std::string borrowId{};
    MpResult res = NumaLevelMemBorrow(group, decision, tmpRedirectionMap, presentNumaId);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel][NumaIdLevelExecute] NumaLevelMemBorrow failed.";
        return MEM_POOLING_ERROR;
    }

    // 2 远端迁移
    res = FaultHandleMigrate(presentNumaId, group.remoteNumaId, decision.pids, decision.lentMemSize);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultHandleParallel][NumaIdLevelExecute] FaultHandleMigrate failed. oldNumaId=" << group.remoteNumaId
            << ", newNumaId=" << presentNumaId << ".";
        return MEM_POOLING_ERROR;
    }

    // 3 归还旧内存
    for (auto [oldName, borrowResult] : tmpRedirectionMap) {
        res = MemBorrowExecutor::Instance().MemFreeWithOps(oldName, false, true, true);
        if (res != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultHandleParallel][NumaIdLevelExecute] MemFreeWithOps error oldName " << oldName << ".";
            return MEM_POOLING_ERROR;
        }

        MpResult retBorrId = BorrowIdRedirection::Instance().Update(oldName, borrowResult.borrowIds[0]);
        if (retBorrId != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[FaultHandleParallel][NumaIdLevelExecute] Update borrowId redirection failed. oldName=" << oldName
                << ", newBorrowId=" << borrowResult.borrowIds[0] << ".";
        }
    }

    return MEM_POOLING_OK;
}

uint32_t NumaLevelExecuteHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultHandleParallel] NumaLevelExecuteHandler start.";
    BorrowGroupResult group;
    RmrsInStream inBuilder(req.data, req.len);
    inBuilder >> group;
    MpResult res;
    NumaLevelDecision numaDecision = group.numaDecision;
    resp.freeFunc = [](uint8_t* data) {
        delete[] data;
    };
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[NumaLevelExecuteHandler] BorrowNodeId="
        << group.borrowNodeId << ", faultNumaId=" << group.remoteNumaId << " start to execute fault handle";
    std::map<std::string, MemBorrowExecuteResult> tmpRedirectionMap;    // key为oldName，value为该oldName对应的新借用记录
    res = FaultNodeModule::Instance().NumaLevelExecute(group, numaDecision, tmpRedirectionMap);
    if (MEM_POOLING_OK != res) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[NumaLevelExecuteHandler] Recv NumaLevelExecute res=" << res << ".";
        resp.len = RPC_RESP_LENGTH;
    } else {
        resp.len = sizeof(MpResult);
    }

    resp.data = new (std::nothrow) uint8_t[resp.len];
    if (resp.data == nullptr) {
        LOG_ERROR << "[NumaLevelExecuteHandler] NumaLevelExecute failed, size=" << resp.len << ".";
        return MEM_POOLING_ERROR;
    }
    *reinterpret_cast<MpResult*>(resp.data) = res;

    return res;
}

MpResult FaultNodeModule::ProcessBorrowOutNodeFaultParallel(const std::string nodeId, bool forceDeleteMem)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager][FaultLentNode] Borrow out faulthandle parallel start.";
    // 采集阶段
    std::vector<BorrowGroupResult> borrowGroups;
    std::vector<ClusterSnapshotItem> clusterInfos;
    auto ret = FaultHandleInfosCollect(nodeId, borrowGroups, clusterInfos);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager][FaultLentNode] Fault handle cluster infos collect failed.";
        return MEM_POOLING_ERROR;
    }

    // 故障处理借用决策阶段
    auto strategyRes = FaultHandleBorrowStrategy(borrowGroups, clusterInfos);
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager][FaultLentNode] FaultHandle strategyRet=" << strategyRes << ".";
    if (strategyRes == MEM_POOLING_ERROR) {
        // 只有决策返回error时表示所有故障numa决策全部失败，直接return
        // 返回完全成功MEM_POOLING_OK或部分成功MEM_POOLING_PARTIAL_OK都需进入到执行阶段
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager][FaultLentNode] Fault handle borrow strategy generate failed.";
        return MEM_POOLING_ERROR;
    }

    // 故障处理执行阶段
    auto execRes = FaultHandleExecuteParallel(borrowGroups);
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager][FaultLentNode] FaultHandle execute ret=" << execRes << ".";

    if (execRes == MEM_POOLING_OK && strategyRes == MEM_POOLING_OK) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager][FaultLentNode] FaultHandle success.";
        return MEM_POOLING_OK;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager][FaultLentNode] FaultHandle finished.";
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
        if (!CheckUBTurboIsAliveRpc(nodeBorrowRecordItem.nodeId)) {
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

void GetPidListAndHugePageMemSize(const NumaReplaceReturnMsg& rpcMsg, std::vector<pid_t>& destPidList,
                                  uint64_t& hugePageMemSize)
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

void NodeNumaReplaceReturnHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
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
        resp.freeFunc = [](uint8_t* data) {
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
    resp.freeFunc = [](uint8_t* data) {
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

void NodeNumaReplaceReturnResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[FaultManager][FaultLentNode] ResHandler ctx is null.";
        return;
    }
    MpResult* res = static_cast<MpResult*>(ctx);
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[FaultManager] [FaultLentNode] NodeNumaReplaceReturn res: " << respData.data[0] << ".";
    if (resCode != MEM_POOLING_OK || respData.data[0] != MEM_POOLING_OK) {
        *res = MEM_POOLING_ERROR;
        return;
    }
    *res = resCode;
}

} // namespace mempooling