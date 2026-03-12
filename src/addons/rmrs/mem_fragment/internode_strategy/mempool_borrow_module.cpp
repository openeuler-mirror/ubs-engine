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

#include "mempool_borrow_module.h"

#include <chrono>
#include <ctime>
#include <vector>
#include "mem_borrow_executor.h"
#include "mem_json_def.h"
#include "mem_manager.h"
#include "mempool_migrate_module.h"
#include "mp_default_struct.h"
#include "rmrs_serialize.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_mem_controller.h"
#include "ubse_node_controller.h"

namespace mempooling {
using namespace ubse::nodeController;
using namespace ubse::log;
using namespace ubse::com;
using namespace ubse::mem::controller;
using namespace mempooling::message;
using namespace rmrs::serialize;
using namespace turbo::rmrs;
using namespace ubse::election;

constexpr uint64_t FOUR_GB_128M = 32; //  4GB 转换为128M的个数
constexpr uint64_t KB_TO_BYTES = 1024;
constexpr uint64_t MB_TO_KB = 1024;
constexpr uint64_t MB_TO_BYTES = 1024 * 1024;
constexpr uint64_t HUGEPAGE_2M_KB = 2048;
constexpr uint64_t FOUR_GB = 4 * 1024 * 1024; // 4GB in KB
constexpr uint64_t HUGE_PAGE_FREE_NUM_TO_BYTES = 2 * 1024 * 1024;

#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)

MpResult MempoolBorrowModule::Init()
{
    return MEM_POOLING_OK;
}

void sortNumaMemInfo(NodeMemInfo &nodeMemInfo)
{
    std::sort(nodeMemInfo.localnumaMemInfo.begin(), nodeMemInfo.localnumaMemInfo.end(),
              [](const NumaMemInfo &a, const NumaMemInfo &b) { return a.borrowableMem > b.borrowableMem; });
}

MpResult GetBorrowedItemVec(const std::string nodeId, std::vector<RackMemNumaPair> &borrowedItemVec)
{
    MpResult ret = MEM_POOLING_OK;
    std::vector<UbseNumaMemoryDebtInfo> debtInfos;
    UbseResult errorCode = ubse::mem::controller::UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos);
    for (auto &debtInfo : debtInfos) {
        if (debtInfo.borrowNodeId == nodeId) {
            RackMemNumaPair rackMemNumaPair{debtInfo.lentNodeId, debtInfo.lentNumaIdList[0],
                                            debtInfo.lentNumaSizeList[0]};
            borrowedItemVec.push_back(rackMemNumaPair);
        }
    }
    return ret;
}

bool compareNodeMemInfo(const std::string nodeId, const std::pair<std::string, NodeMemInfo> &a,
                        const std::pair<std::string, NodeMemInfo> &b)
{
    // 获取已经借用过的节点列表
    std::vector<RackMemNumaPair> borrowedItemVec;
    MpResult ret = GetBorrowedItemVec(nodeId, borrowedItemVec);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] GetBorrowedItemVec of nodeId=" << nodeId << " failed.";
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] Get borrowed item vec failed.";
        return a.second.totalBorrowableMem > b.second.totalBorrowableMem;
    }
    // 将borrowedItem中的nodeId提取到unordered_set中，提高查找效率
    std::unordered_set<std::string> nodeIdSet;
    for (const auto &pair : borrowedItemVec) {
        (void)nodeIdSet.insert(pair.nodeId);
    }
    // 优先考虑a.first是否在nodeIdSet中
    if (nodeIdSet.find(a.first) != nodeIdSet.end() && nodeIdSet.find(b.first) == nodeIdSet.end()) {
        // 如果a.first在nodeIdSet中，b.first不在，返回a排在前面
        return true;
    } else if (nodeIdSet.find(b.first) != nodeIdSet.end() && nodeIdSet.find(a.first) == nodeIdSet.end()) {
        // 如果b.first在nodeIdSet中，a.first不在，返回b排在前面
        return false;
    }
    return a.second.totalBorrowableMem > b.second.totalBorrowableMem;
}

bool compareSocketMemInfo(const std::unordered_map<std::string, std::unordered_set<uint16_t>> &nodeIdToNumaIdSetMap,
                          const std::vector<RackMemNumaPair> &borrowedItemVec, const SrcMemoryBorrowParam &srcParam,
                          const std::pair<std::string, NodeMemInfo> &a, const std::pair<std::string, NodeMemInfo> &b)
{
    // 优先从同平面的平面借用内存
    bool aSamePlane = MemManager::Instance().JudgeSampPlane(srcParam.srcNid, srcParam.srcSocketId, a.first,
                                                            a.second.localnumaMemInfo[0].socketId);

    bool bSamePlane = MemManager::Instance().JudgeSampPlane(srcParam.srcNid, srcParam.srcSocketId, b.first,
                                                            b.second.localnumaMemInfo[0].socketId);
    if (aSamePlane != bSamePlane) {
        return aSamePlane; // 同平面的 (true) 排前面
    }
    // 将borrowedItem中的nodeId提取到unordered_set中，提高查找效率
    std::unordered_set<std::string> nodeIdSet;
    for (const auto &pair : borrowedItemVec) {
        if (!MpConfiguration::GetInstance().GetMustSamePlane()) {
            (void)nodeIdSet.insert(pair.nodeId);
            continue;
        }
        auto it = nodeIdToNumaIdSetMap.find(pair.nodeId);
        if (it != nodeIdToNumaIdSetMap.end() && it->second.count(pair.numaId)) {
            (void)nodeIdSet.insert(pair.nodeId);
        }
    }
    // 优先考虑a.first是否在nodeIdSet中
    if (nodeIdSet.find(a.first) != nodeIdSet.end() && nodeIdSet.find(b.first) == nodeIdSet.end()) {
        // 如果a.first在nodeIdSet中，b.first不在，返回a排在前面
        return true;
    } else if (nodeIdSet.find(b.first) != nodeIdSet.end() && nodeIdSet.find(a.first) == nodeIdSet.end()) {
        // 如果b.first在nodeIdSet中，a.first不在，返回b排在前面
        return false;
    }
    // 比较 localnumaMemInfo 中所有 borrowableMem 的和
    auto sumBorrowableMem = [](const NodeMemInfo &info) {
        uint64_t sum = 0;
        for (const auto &entry : info.localnumaMemInfo) {
            sum += entry.borrowableMem;
        }
        return sum;
    };
    return sumBorrowableMem(a.second) > sumBorrowableMem(b.second);
}

// 检查借用内存的大小是否合法
MpResult MempoolBorrowModule::ValidateBorrowSize(const uint64_t borrowSize)
{
    if (gBlockSize == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] The block size is 0, which is invalid.";
        return MEM_POOLING_ERROR;
    }
    if ((borrowSize % (gBlockSize * MB_TO_KB)) != 0) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] BorrowSize is not aligned to " << gBlockSize
            << " MB, round up automatically.";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

// 检查借用内存的大小是否合法
MpResult MempoolBorrowModule::ValidateSrcparam(const SrcMemoryBorrowParam &srcParam)
{
    std::map<std::string, std::map<int, uint16_t>> numaSocketMap;
    MpResult ret = MemManager::Instance().GenerateNumaSocketMap(numaSocketMap);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowStrategy] Get numaSocketMap failed.";
        return ret;
    }
    auto outerIt = numaSocketMap.find(srcParam.srcNid);
    if (outerIt == numaSocketMap.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] The input source nodeId = " << srcParam.srcNid << " does not exist.";
        return MEM_POOLING_ERROR;
    }
    const auto &innerMap = outerIt->second;
    auto innerIt = innerMap.find(srcParam.srcNumaId);
    if (innerIt == innerMap.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] The input numaId = " << srcParam.srcNumaId << " does not exist.";
        return MEM_POOLING_ERROR;
    }
    if (innerIt->second != srcParam.srcSocketId) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] The input socketId is wrong ,the numaId = " << srcParam.srcNumaId
            << " belong to socketId = " << innerIt->second << ".";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

void MempoolBorrowModule::UpdateNodeMemInfoWithNuma(std::unordered_map<std::string, NodeMemInfo> &nodeMemMap)
{
    for (auto &pair : nodeMemMap) {
        pair.second.totalBorrowableMem = 0;
        for (auto &numaInfo : pair.second.localnumaMemInfo) {
#ifdef UB_ENVIRONMENT
            numaInfo.borrowableMem = std::min(numaInfo.borrowableMem, numaInfo.vmMemFree);
            pair.second.totalBorrowableMem += numaInfo.borrowableMem;
#else
            numaInfo.borrowableMem = std::min(numaInfo.borrowableMem, numaInfo.memFree);
            pair.second.totalBorrowableMem += numaInfo.borrowableMem;
#endif
        }
    }
}

// 获取内存管理信息
MpResult MempoolBorrowModule::GetMemoryInfo(std::unordered_map<std::string, NodeMemInfo> &nodeMemMap,
                                            const SrcMemoryBorrowParam &srcParam,
                                            std::vector<std::string> &antiNodeMemVec)
{
    MpResult ret = MemManager::Instance().GetNodeMemMap(nodeMemMap);
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] Failed to get the memory ledger.";
        return ret;
    }
    UpdateNodeMemInfoWithNuma(nodeMemMap);
    for (auto &it : nodeMemMap) {
        for (auto &ix : it.second.localnumaMemInfo) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowStrategy] Node = " << it.first << ", Numa = " << ix.numaId
                << " , BorrowableMem = " << ix.borrowableMem << ".";
        }
    }
    ret = AntiNode::Instance().Query(srcParam.srcNid, antiNodeMemVec);
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] Failed to obtain the anti-affinity information of the current node.";
        return ret;
    }

    return MEM_POOLING_OK;
}

void MempoolBorrowModule::FilterNodesBySocketProximity(
    std::unordered_map<std::string, NodeMemInfo> &nodeMemMap, const std::vector<MemNodeData> &foundNodeData,
    std::unordered_map<std::string, std::unordered_set<uint16_t>> &nodeIdToNumaIdSetMap)
{
    std::unordered_map<std::string, std::unordered_set<std::string>> nodeToSocketSet;
    for (const auto &foundNode : foundNodeData) {
        (void)nodeToSocketSet[foundNode.nodeId].insert(foundNode.socket.socketId);
    }
    for (auto it = nodeMemMap.begin(); it != nodeMemMap.end();) {
        const std::string &nodeId = it->first;
        NodeMemInfo &nodeInfo = it->second;
        auto foundSocketIt = nodeToSocketSet.find(nodeId);
        if (foundSocketIt == nodeToSocketSet.end()) {
            it = nodeMemMap.erase(it);
            continue;
        }
        std::unordered_set<std::string> &validSocketIds = foundSocketIt->second;
        std::vector<NumaMemInfo> filteredNumaInfos;
        for (const auto &numaInfo : nodeInfo.localnumaMemInfo) {
            std::string socketIdStr = std::to_string(numaInfo.socketId);
            if (validSocketIds.count(socketIdStr) > 0) {
                filteredNumaInfos.push_back(numaInfo);
            }
        }
        if (filteredNumaInfos.empty()) {
            it = nodeMemMap.erase(it);
        } else {
            nodeInfo.localnumaMemInfo = std::move(filteredNumaInfos);
            ++it;
        }
    }
    for (const auto &[nodeId, info] : nodeMemMap) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] NodeId=" << nodeId << ", filtered info: " << info.ToString();
        for (const auto &numa : info.localnumaMemInfo) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowStrategy] numaInfo:    " << numa.ToString();
        }
    }
    for (const auto &foundNode : foundNodeData) {
        auto &numaSet = nodeIdToNumaIdSetMap[foundNode.nodeId];
        for (const auto &numa : foundNode.socket.numas) {
            try {
                uint16_t parsedNumaId = static_cast<uint16_t>(std::stoi(numa.numaId));
                (void)numaSet.insert(parsedNumaId);
            } catch (const std::invalid_argument &e) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "invalid argument=" << numa.numaId << ".";
            } catch (const std::out_of_range &e) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "numaId out of range, numaId=" << numa.numaId << ".";
            }
        }
    }
}

// 过滤并排序节点
void MempoolBorrowModule::FilterAndSortNodes(std::unordered_map<std::string, NodeMemInfo> &nodeMemMap,
                                             std::string srcNid, const std::vector<std::string> &antiNodeMemVec,
                                             std::vector<std::pair<std::string, NodeMemInfo>> &nodeVec)
{
    SrcMemoryBorrowParam param;
    param.srcNid = srcNid;
    auto ret = FilterBorrowableNodes(param, antiNodeMemVec, nodeMemMap);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] FilterBorrowableNodes failed.";
    }
    // 成环过滤
    std::vector<BorrowRecord> borrowRecords;
    ret = BorrowRecordHelper::Instance().CollectBorrowRecordsAll(borrowRecords, true, true);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] CollectBorrowRecords failed.";
    }
    for (auto &record : borrowRecords) {
        nodeMemMap.erase(record.borrowNode);
    }

    nodeVec = std::vector<std::pair<std::string, NodeMemInfo>>(nodeMemMap.begin(), nodeMemMap.end());
    for (auto &node : nodeVec) {
        sortNumaMemInfo(node.second);
    }
    std::sort(nodeVec.begin(), nodeVec.end(),
              [&srcNid](const std::pair<std::string, NodeMemInfo> &a, const std::pair<std::string, NodeMemInfo> &b) {
                  return compareNodeMemInfo(srcNid, a, b);
              });
}

MpResult MempoolBorrowModule::FilterBorrowableNodes(const SrcMemoryBorrowParam &srcParam,
                                                    const std::vector<std::string> &antiNodeMemVec,
                                                    std::unordered_map<std::string, NodeMemInfo> &nodeMemMap)
{
    for (auto &node : antiNodeMemVec) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowStrategy] antiNode = " << node;
        nodeMemMap.erase(node);
    }
    std::unordered_set<std::string> couldBorrowNodeSet;
    auto ret = MpParseGroupProviderConf::Instance().GetBorrowableList(srcParam.srcNid, couldBorrowNodeSet);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowStrategy] GetBorrowableList failed.";
        return MEM_POOLING_ERROR;
    }
    for (auto &borrowNode : couldBorrowNodeSet) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] borrowNode = " << borrowNode;
    }
    for (auto ix = nodeMemMap.begin(); ix != nodeMemMap.end();) {
        const auto &nodeId = ix->first;
        if (!couldBorrowNodeSet.count(nodeId)) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowStrategy] Node=" << nodeId << " is not in couldBorrowNodeSet, erase it.";
            ix = nodeMemMap.erase(ix);
        } else {
            ++ix;
        }
    }
    std::unordered_map<std::string, BorrowItem> borrowCacheAll;
    MemReturnManager::Instance().QueryAll(borrowCacheAll);
    for (auto &kv : borrowCacheAll) {
        if (kv.second.srcNid == srcParam.srcNid &&
            MemManager::Instance().JudgeSampPlane(srcParam.srcNid, srcParam.srcSocketId, kv.second.dstNid,
                                                  kv.second.dstSocketId)) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowStrategy] Node=" << kv.second.dstNid
                << " has return timeout records, erase it.";
            nodeMemMap.erase(kv.second.dstNid);
        }
    }
    return MEM_POOLING_OK;
}

// 过滤并排序节点
MpResult MempoolBorrowModule::FilterAndSortSockets(
    std::unordered_map<std::string, NodeMemInfo> &nodeMemMap, const SrcMemoryBorrowParam &srcParam,
    const std::vector<std::string> &antiNodeMemVec,
    std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology,
    std::vector<std::pair<std::string, NodeMemInfo>> &nodeVec)
{
    auto ret = FilterBorrowableNodes(srcParam, antiNodeMemVec, nodeMemMap);
    if (ret != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    std::vector<BorrowRecord> borrowRecords;
    ret = BorrowRecordHelper::Instance().CollectBorrowRecordsAll(borrowRecords, false, true);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] CollectBorrowRecords failed.";
        return MEM_POOLING_ERROR;
    }
    for (auto &record : borrowRecords) {
        nodeMemMap.erase(record.borrowNode);
    }
    std::string srcNidAndSocketId = srcParam.srcNid + "-" + std::to_string(srcParam.srcSocketId);
    auto it = nodeTopology.find(srcNidAndSocketId);
    if (it == nodeTopology.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] Can't find " << srcNidAndSocketId << " in nodeTopology";
        return MEM_POOLING_ERROR;
    }
    std::vector<MemNodeData> foundNodeData = it->second;
    std::unordered_map<std::string, std::unordered_set<uint16_t>> nodeIdToNumaIdSetMap;
    if (MpConfiguration::GetInstance().GetMustSamePlane()) {
        FilterNodesBySocketProximity(nodeMemMap, foundNodeData, nodeIdToNumaIdSetMap);
    }
    nodeVec = std::vector<std::pair<std::string, NodeMemInfo>>(nodeMemMap.begin(), nodeMemMap.end());
    if (!MpConfiguration::GetInstance().GetMustSamePlane()) {
        std::vector<std::pair<std::string, NodeMemInfo>> nodeVecSplit;
        for (const auto &[nodeId, nodeInfo] : nodeVec) {
            // 临时按 socketId 分组
            std::unordered_map<uint16_t, NodeMemInfo> socketGroup;
            for (const auto &numaInfo : nodeInfo.localnumaMemInfo) {
                socketGroup[numaInfo.socketId].localnumaMemInfo.push_back(numaInfo);
            }
            for (auto &[socketId, newInfo] : socketGroup) {
                (void)nodeVecSplit.emplace_back(nodeId, std::move(newInfo));
            }
        }
        nodeVec = std::move(nodeVecSplit);
    }
    for (auto &node : nodeVec) {
        sortNumaMemInfo(node.second);
    }
    NodeMemoryInfoWithReservedMem nodeMemoryInfoWithReservedMem{};
    ret = BorrowRecordHelper::Instance().CollectBorrowableInfo(srcParam.srcNid, nodeMemoryInfoWithReservedMem);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] CollectBorrowableInfo failed.";
        return MEM_POOLING_ERROR;
    }
    std::vector<RackMemNumaPair> borrowedItemVec;
    ret = GetBorrowedItemVec(srcParam.srcNid, borrowedItemVec);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] GetBorrowedItemVec of nodeId=" << srcParam.srcNid << " failed.";
        return MEM_POOLING_ERROR;
    }
    std::sort(nodeVec.begin(), nodeVec.end(),
              [&nodeIdToNumaIdSetMap, &borrowedItemVec, &srcParam](const std::pair<std::string, NodeMemInfo> &a,
                                                                   const std::pair<std::string, NodeMemInfo> &b) {
        return compareSocketMemInfo(nodeIdToNumaIdSetMap, borrowedItemVec, srcParam, a, b);
    });
    // 打印nodeVec
    for (const auto &[nodeId, info] : nodeVec) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] NodeId=" << nodeId << ", filtered info: " << info.ToString();
        for (const auto &numa : info.localnumaMemInfo) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowStrategy] numaInfo:    " << numa.ToString();
        }
    }
    return MEM_POOLING_OK;
}

// 处理每个节点的借用内存
MpResult MempoolBorrowModule::ProcessNodeMemBorrow(const std::pair<std::string, NodeMemInfo> &node,
                                                   uint32_t &needBorrowNum,
                                                   MemBorrowStrategyResult &borrowStrategyResult)
{
    std::map<int, uint64_t> socket2CurLeftMemSizeMap;
    if (GetSocket2CurMemSizeMap(node.first, socket2CurLeftMemSizeMap) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }

    for (auto &numamemInfo : node.second.localnumaMemInfo) {
        DestMemoryBorrowParam tempParam;
        tempParam.destNid = node.first;
        MpResult ret = GetSocketInfo(tempParam, numamemInfo.numaId);
        if (ret != MEM_POOLING_OK) {
            return ret;
        }
        uint32_t couldBorrowSize = 0;
        // tmp: 单位MB
        uint64_t tmp = numamemInfo.borrowableMem / MB_TO_BYTES;
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] The numamemInfo.numaId=" << numamemInfo.numaId
            << ", numamemInfo.borrowableMem=" << numamemInfo.borrowableMem << "BYTE";

        uint64_t newTmp = std::min(tmp, socket2CurLeftMemSizeMap[numamemInfo.socketId]);
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] Before calculate, couldBorrowSize=" << tmp
            << "MB,after calculate couldBorrowSize=" << newTmp << "MB.";

        ret = SafeUint64To32(couldBorrowSize, newTmp);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowStrategy] Conversion failed.";
            return MEM_POOLING_ERROR;
        }

        uint32_t FOUR_GB_BLOCKSIZE = FOUR_GB / (gBlockSize * MB_TO_KB);
        uint32_t couldBorrowNum = (couldBorrowSize * MB_TO_BYTES) / (gBlockSize * MB_TO_KB * KB_TO_BYTES);
        uint32_t haveFourGbNum = (couldBorrowSize * MB_TO_BYTES) / (FOUR_GB * KB_TO_BYTES);
        uint32_t moreBorrowNum = couldBorrowNum - haveFourGbNum * FOUR_GB_BLOCKSIZE;
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] The couldBorrowNum=" << couldBorrowNum
            << ", needBorrowNum=" << needBorrowNum << ".";
        if (couldBorrowNum <= needBorrowNum) {
            AddMemoryParamsToResult(haveFourGbNum, moreBorrowNum, tempParam, borrowStrategyResult);
            needBorrowNum -= couldBorrowNum;
            socket2CurLeftMemSizeMap[numamemInfo.socketId] -= couldBorrowNum * gBlockSize;
        } else {
            AddMemoryParamsToResult(needBorrowNum / FOUR_GB_BLOCKSIZE, needBorrowNum % FOUR_GB_BLOCKSIZE, tempParam,
                                    borrowStrategyResult);
            socket2CurLeftMemSizeMap[numamemInfo.socketId] -= needBorrowNum * gBlockSize;
            needBorrowNum = 0;
            break;
        }
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] After generate the MemBorrowStrategy,"
            << "numaId=" << numamemInfo.numaId << ", socketId=" << numamemInfo.socketId
            << ", curLeftMemSize=" << socket2CurLeftMemSizeMap[numamemInfo.socketId] << "MB.";
    }
    return MEM_POOLING_OK;
}

MpResult MempoolBorrowModule::GetNodeInfoByNodeId(const std::string &nodeId, UbseNodeInfo &nodeInfo)
{
    const auto allNodeInfo = UbseNodeController::GetInstance().GetAllNodes();
    auto it = allNodeInfo.find(nodeId);
    if (it == allNodeInfo.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] GetAllNodes failed, nodeId=" << nodeId << ".";
        return MEM_POOLING_ERROR;
    }

    nodeInfo = it->second;
    return MEM_POOLING_OK;
}

MpResult MempoolBorrowModule::GetSocket2CurMemSizeMap(const std::string &nodeId,
                                                      std::map<int, uint64_t> &socket2CurLeftMemSizeMap)
{
    // 获取socketId与对应当前剩余可借出内存大小的map（内存大小单位：MB）
    std::map<int, uint64_t> socket2CurUsedMemCountMap;
    // 获取借出节点NodeInfo信息
    UbseNodeInfo nodeInfo;
    if (GetNodeInfoByNodeId(nodeId, nodeInfo) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    // 节点上每个block块的大小
    uint32_t blockSize = nodeInfo.blockSize;
    if (blockSize != gBlockSize) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] BlockSize is not equal to gBlockSize, nodeId=" << nodeId << ".";
        return MEM_POOLING_ERROR;
    }
    // 节点socket对应的最大可借出mem块数量
    uint32_t exportTotalTimes = nodeInfo.exportTotalTimes;
    for (const auto &[numaLocation, numaInfo] : nodeInfo.numaInfos) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] SocketId=" << numaInfo.socketId << ", numaId=" << numaLocation.numaId
            << ", nodeId=" << nodeId << ", exportTotalTimes=" << exportTotalTimes << ".";
        socket2CurUsedMemCountMap[numaInfo.socketId] = 0;
    }

    // 获取节点账本信息，用于获取借出节点的所有memId
    std::vector<BorrowRecord> borrowRecords;
    auto ret = BorrowRecordHelper::Instance().CollectBorrowRecordsWithFault(nodeId, borrowRecords);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] Get BorrowRecords failed, nodeId=" << nodeId << ".";
        return MEM_POOLING_ERROR;
    }

    for (const auto &record : borrowRecords) {
        // 只获取借出本节点的memId
        if (record.lentNode != nodeId) {
            continue;
        }
        // 获取 借出节点的socketId 与 当前借出设备块数量 的map
        socket2CurUsedMemCountMap[record.lentSocketId] += record.lentMemId.size();
    }

    for (const auto &[socketId, curUsedMemCount] : socket2CurUsedMemCountMap) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] SocketId=" << socketId << ", curUsedMemCount=" << curUsedMemCount << ".";
        // 总的可借出块设备数量不能小于当前借出块设备数量
        if (exportTotalTimes < curUsedMemCount) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowStrategy] SocketId=" << socketId << ", curUsedMemCount=" << curUsedMemCount
                << "greater than exportTotalTimes=" << exportTotalTimes << ", which is invalid.";
            return MEM_POOLING_ERROR;
        }
        // 获取当前socketId与剩余可借出内存大小的map
        socket2CurLeftMemSizeMap[socketId] = (exportTotalTimes - curUsedMemCount) * gBlockSize;
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] SocketId=" << socketId
            << ", CurLeftMemSize=" << ((exportTotalTimes - curUsedMemCount) * gBlockSize) << "MB.";
    }

    return MEM_POOLING_OK;
}

// 获取Socket信息
MpResult MempoolBorrowModule::GetSocketInfo(DestMemoryBorrowParam &tempParam, uint32_t numaId)
{
    uint16_t socketId = 0;
    MpResult ret = MemManager::Instance().GetSocketId(tempParam.destNid, numaId, socketId);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowStrategy] Get destSocketId failed.";
        return ret;
    }
    tempParam.destSocketId = socketId;
    tempParam.destNumaNum = 1;
    tempParam.destNumaId.push_back(numaId);
    return MEM_POOLING_OK;
}

// 将内存参数添加到结果中
void MempoolBorrowModule::AddMemoryParamsToResult(uint32_t haveFourGbNum, uint32_t moreBorrowNum,
                                                  DestMemoryBorrowParam &tempParam,
                                                  MemBorrowStrategyResult &borrowStrategyResult)
{
    for (uint32_t i = 0; i < haveFourGbNum; i++) {
        DestMemoryBorrowParam tempParamPush = {
            tempParam.destNid, tempParam.destSocketId, tempParam.destNumaNum, {tempParam.destNumaId}, {FOUR_GB}};
        borrowStrategyResult.destParam.push_back(tempParamPush);
    }

    if (moreBorrowNum != 0) {
        DestMemoryBorrowParam tempParamPush = {tempParam.destNid,
                                               tempParam.destSocketId,
                                               tempParam.destNumaNum,
                                               {tempParam.destNumaId},
                                               {moreBorrowNum * (gBlockSize * MB_TO_KB)}};
        borrowStrategyResult.destParam.push_back(tempParamPush);
    }
}

MpResult MempoolBorrowModule::SafeUint64To32(uint32_t &targetNum, uint64_t tmp)
{
    if (tmp > std::numeric_limits<uint32_t>::max()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowStrategy] Param overflow.";
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] NeedBorrowNum overflow, original param=" << tmp << ".";
        return MEM_POOLING_ERROR;
    } else {
        targetNum = static_cast<uint32_t>(tmp);
    }
    return MEM_POOLING_OK;
}

MpResult MempoolBorrowModule::GetAndSetBlockSize(const std::string &nodeId)
{
    const auto allNodeInfo = UbseNodeController::GetInstance().GetAllNodes();
    auto it = allNodeInfo.find(nodeId);
    if (it == allNodeInfo.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowStrategy] GetAllNodes failed, nodeId=" << nodeId << ".";
        return MEM_POOLING_ERROR;
    }

    gBlockSize = it->second.blockSize;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemBorrow][MemBorrowStrategy] GetBlockSize Success, gBlockSize=" << gBlockSize << ", nodeId=" << nodeId
        << ".";
    return MEM_POOLING_OK;
}

MpResult MempoolBorrowModule::MemBorrowStrategy(const SrcMemoryBorrowParam &srcParam, const uint64_t borrowSize,
                                                MemBorrowStrategyResult &borrowStrategyResult)
{
    LOG_DEBUG << "[MemBorrow][MemBorrowStrategy] Start to invoke borrow strategy, input SrcMemoryBorrowParam = "
              << srcParam.ToString() << ".";
    if (GetAndSetBlockSize(srcParam.srcNid) != MEM_POOLING_OK)
        return MEM_POOLING_ERROR;
    uint64_t alignBorrowSize = borrowSize;
    if (ValidateBorrowSize(borrowSize) != MEM_POOLING_OK) {
        if (gBlockSize == 0) {
            return MEM_POOLING_ERROR;
        }
        alignBorrowSize =
            ((borrowSize + (gBlockSize * MB_TO_KB) - 1) / (gBlockSize * MB_TO_KB)) * (gBlockSize * MB_TO_KB);
    }

    if (ValidateSrcparam(srcParam) != MEM_POOLING_OK)
        return MEM_POOLING_ERROR;

    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    std::vector<std::string> antiNodeMemVec;
    MpResult ret = GetMemoryInfo(nodeMemMap, srcParam, antiNodeMemVec);
    if (ret != MEM_POOLING_OK)
        return ret;

    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    if (UbseMemGetTopologyInfo(nodeTopology) != 0) {
        LOG_ERROR << "[MemBorrow][MemBorrowStrategy] Get node topology from rack failed!";
        return MEM_POOLING_ERROR;
    }

    std::vector<std::pair<std::string, NodeMemInfo>> nodeVec;
    if (FilterAndSortSockets(nodeMemMap, srcParam, antiNodeMemVec, nodeTopology, nodeVec) != MEM_POOLING_OK)
        return MEM_POOLING_ERROR;

    borrowStrategyResult.srcParam = srcParam;
    borrowStrategyResult.borrowSize = alignBorrowSize;
    uint32_t needBorrowNum = 0;
    uint64_t tmp = alignBorrowSize / (gBlockSize * MB_TO_KB);
    ret = SafeUint64To32(needBorrowNum, tmp);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[MemBorrow][MemBorrowStrategy] Conversion failed.";
        return MEM_POOLING_ERROR;
    }

    for (auto &node : nodeVec) {
        if (needBorrowNum <= 0)
            break;
        if (node.first == srcParam.srcNid)
            continue;

        ret = ProcessNodeMemBorrow(node, needBorrowNum, borrowStrategyResult);
        if (ret != MEM_POOLING_OK)
            return ret;
    }

    if (needBorrowNum > 0) {
        LOG_ERROR << "[MemBorrow][MemBorrowStrategy] Not enough memory to borrow, left " << (needBorrowNum * gBlockSize)
                  << "MB can't be borrowed.";
        return MEM_POOLING_ERROR;
    }
    LOG_DEBUG << "[MemBorrow][MemBorrowStrategy] There is enough memory to borrow.";
    return MEM_POOLING_OK;
}

struct MultiBorrowStrategyParam {
    struct Numa {
        Numa(std::string nodeId, uint16_t numaId, uint16_t socketId, uint64_t freeMem, bool isSamePlane)
            : nodeId(nodeId),
              numaId(numaId),
              socketId(socketId),
              freeMem(freeMem),
              isSamePlane(isSamePlane)
        {
            return;
        }
        std::string nodeId;
        uint16_t numaId;
        uint16_t socketId;
        uint64_t freeMem;
        bool isSamePlane;
    };
    struct Node {
        Node(std::string nodeId, uint64_t available) : nodeId(nodeId), freeMem(freeMem)
        {
            return;
        }
        std::string nodeId;
        uint64_t freeMem;
        std::vector<Numa> numaList;
    };
    std::vector<Node> nodeList;
    std::vector<uint64_t> need;
    std::vector<Numa *> recordList;
    bool succeed{false};
    bool byNodeFault;
};

static void MultiBorrowDfs(MultiBorrowStrategyParam &param, unsigned int pos, MemBorrowStrategyMultiResult &result);

static void TryAssignWithPredicate(MultiBorrowStrategyParam &param, unsigned int pos,
                                   MemBorrowStrategyMultiResult &result,
                                   const std::function<bool(const MultiBorrowStrategyParam::Numa &)> &pred)
{
    for (auto &node : param.nodeList) {
        for (auto &numa : node.numaList) {
            if (!pred(numa)) {
                continue;
            }
            if (numa.freeMem < param.need[pos]) {
                continue;
            }

            numa.freeMem -= param.need[pos];
            node.freeMem -= param.need[pos];
            param.recordList[pos] = &numa;

            MultiBorrowDfs(param, pos + 1, result);

            node.freeMem += param.need[pos];
            numa.freeMem += param.need[pos];

            if (param.succeed) {
                return; // 提前结束递归搜索
            }
        }
    }
}

static void MultiBorrowDfs(MultiBorrowStrategyParam &param, unsigned int pos, MemBorrowStrategyMultiResult &result)
{
    if (param.succeed) {
        return;
    }

    if (pos == param.need.size()) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowStrategy] Found a valid strategy.";

        for (unsigned int i = 0; i < pos; i++) {
            DestMemoryBorrowParam dest;
            dest.destNid = param.recordList[i]->nodeId;
            dest.destNumaId.push_back(param.recordList[i]->numaId);
            dest.memSize.push_back(param.need[i]);
            dest.destSocketId = param.recordList[i]->socketId;

            result.destParam.push_back(dest);
            result.borrowSizes.push_back(param.need[i]);

            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowStrategy]"
                << "NodeId: " << dest.destNid << ", NumaId: " << dest.destNumaId[0]
                << ", Socket: " << dest.destSocketId;
        }

        param.succeed = true;
        return;
    }

    TryAssignWithPredicate(param, pos, result, [](const MultiBorrowStrategyParam::Numa &n) { return n.isSamePlane; });

    if (param.succeed) {
        return;
    }

    TryAssignWithPredicate(param, pos, result, [](const MultiBorrowStrategyParam::Numa &n) { return !n.isSamePlane; });
    return;
}

static void GetMemBorrowStrategyMultipleParam(const SrcMemoryBorrowParam &srcParam, MultiBorrowStrategyParam &param,
                                              std::string &destPreNid, uint16_t socketId,
                                              std::unordered_map<std::string, NodeMemInfo> &nodeMemMap)
{
    for (auto &pair : nodeMemMap) {
        unsigned int pos = 0;
        if (pair.first == srcParam.srcNid || (pair.first == destPreNid && param.byNodeFault)) {
            continue;
        }
        if (pair.first == destPreNid) {
            (void)param.nodeList.emplace(param.nodeList.begin(), pair.first, pair.second.totalBorrowableMem);
            pos = 0;
        } else {
            (void)param.nodeList.emplace_back(pair.first, pair.second.totalBorrowableMem);
            pos = param.nodeList.size() - 1;
        }
        for (auto &numa : pair.second.localnumaMemInfo) {
            // 只有同一平面的才可以借
            std::vector<MultiBorrowStrategyParam::Numa> tmp;
            if (MemManager::Instance().JudgeSampPlane(pair.first, numa.socketId, srcParam.srcNid,
                                                      srcParam.srcSocketId)) {
                (void)param.nodeList[pos].numaList.emplace_back(
                    pair.first, numa.numaId, numa.socketId,
                    std::min(numa.borrowableMem, numa.reservedMem - numa.borrowedMem), true);
            } else {
                (void)tmp.emplace_back(pair.first, numa.numaId, numa.socketId,
                                       std::min(numa.borrowableMem, numa.reservedMem - numa.borrowedMem), false);
            }
            if (!MpConfiguration::GetInstance().GetMustSamePlane()) {
                (void)param.nodeList[pos].numaList.insert(param.nodeList[pos].numaList.end(), tmp.begin(), tmp.end());
            }
        }
        if (pos != 0) {
            continue;
        }
        sort(param.nodeList[pos].numaList.begin(), param.nodeList[pos].numaList.end(),
             [](const MultiBorrowStrategyParam::Numa &x, const MultiBorrowStrategyParam::Numa &y) -> bool {
                 return x.freeMem > y.freeMem;
             });
    }
    return;
}

MpResult MempoolBorrowModule::MemBorrowStrategyMultipleUB(const SrcMemoryBorrowParam &srcParam,
                                                          const std::vector<uint64_t> &borrowSizes,
                                                          std::string &destPreNid, uint16_t socketId,
                                                          MemBorrowStrategyMultiResult &borrowStrategyResult)
{
    for (const auto &item : borrowSizes) {
        if (ValidateBorrowSize(item) != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowStrategy] Input size invalid " << item << ".";
            return MEM_POOLING_ERROR;
        }
    }
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    std::vector<std::string> antiNodeMemVec;
    auto ret = GetMemoryInfo(nodeMemMap, srcParam, antiNodeMemVec);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowStrategy] GetMemoryInfo failed.";
        return ret;
    }
    std::vector<std::pair<std::string, NodeMemInfo>> nodeVec;
    FilterAndSortNodes(nodeMemMap, srcParam.srcNid, antiNodeMemVec, nodeVec);
    for (auto &pair : nodeMemMap) {
        pair.second.totalReservedMem /= KB_TO_BYTES;
        pair.second.totalBorrowableMem /= KB_TO_BYTES;
        pair.second.totalLentMem /= KB_TO_BYTES;
        pair.second.totalBorrowedMem /= KB_TO_BYTES;
        for (auto &numa : pair.second.localnumaMemInfo) {
            numa.reservedMem /= KB_TO_BYTES;
            numa.lentMem /= KB_TO_BYTES;
            numa.borrowableMem /= KB_TO_BYTES;
            numa.borrowedMem /= KB_TO_BYTES;
        }
    }
    borrowStrategyResult.srcParam = srcParam;
    MultiBorrowStrategyParam param;
    param.byNodeFault = borrowStrategyResult.byNodeFault;
    param.need = borrowSizes;
    sort(param.need.begin(), param.need.end(), std::greater<uint64_t>());
    param.recordList.reserve(borrowSizes.size());
    GetMemBorrowStrategyMultipleParam(srcParam, param, destPreNid, socketId, nodeMemMap);
    MultiBorrowDfs(param, 0, borrowStrategyResult);
    return MEM_POOLING_OK;
}

MpResult MempoolBorrowModule::MemBorrowStrategyMultiple(const SrcMemoryBorrowParam &srcParam,
                                                        const std::vector<uint64_t> &borrowSizes,
                                                        std::string &destPreNid,
                                                        MemBorrowStrategyMultiResult &borrowStrategyResult)
{
    MpResult ret = MEM_POOLING_OK;
    for (const auto &item : borrowSizes) {
        ret = ValidateBorrowSize(item);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowStrategy] Input size invalid " << item << ".";
            return MEM_POOLING_ERROR;
        }
    }
    std::unordered_map<std::string, NodeMemInfo> nodeMemMap;
    std::vector<std::string> antiNodeMemVec;
    ret = GetMemoryInfo(nodeMemMap, srcParam, antiNodeMemVec);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowStrategy] GetMemoryInfo failed.";
        return ret;
    }
    std::vector<std::pair<std::string, NodeMemInfo>> nodeVec;
    FilterAndSortNodes(nodeMemMap, srcParam.srcNid, antiNodeMemVec, nodeVec);
    for (auto &pair : nodeMemMap) {
        pair.second.totalReservedMem /= KB_TO_BYTES;
        pair.second.totalBorrowableMem /= KB_TO_BYTES;
        pair.second.totalLentMem /= KB_TO_BYTES;
        pair.second.totalBorrowedMem /= KB_TO_BYTES;
        for (auto &numa : pair.second.localnumaMemInfo) {
            numa.reservedMem /= KB_TO_BYTES;
            numa.lentMem /= KB_TO_BYTES;
            numa.borrowableMem /= KB_TO_BYTES;
            numa.borrowedMem /= KB_TO_BYTES;
        }
    }
    borrowStrategyResult.srcParam = srcParam;
    MultiBorrowStrategyParam param;
    param.byNodeFault = borrowStrategyResult.byNodeFault;
    param.need = borrowSizes;
    sort(param.need.begin(), param.need.end(), std::greater<uint64_t>());
    param.recordList.reserve(borrowSizes.size());
    GetMemBorrowStrategyMultipleParam(srcParam, param, destPreNid, 0, nodeMemMap);
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemBorrow][MemBorrowStrategy] Start dfs." << param.nodeList.size();
    MultiBorrowDfs(param, 0, borrowStrategyResult);
    return MEM_POOLING_OK;
}

MpResult MempoolBorrowModule::MemFree(std::string nodeId)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemFree][MemFreeExecute] Master to invoke the slave memory free strategy and execute on nodeId=" << nodeId
        << ".";
    MigrateBackResult migrateBackResult;
    auto ret = message::MempoolingMessage::rmrsMigrateBack(migrateBackResult);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemFree][MemFreeExecute] MemFree strategy and execute failed, ubturbo frame error code=" << ret << ".";
        return MEM_POOLING_ERROR;
    }

    if (migrateBackResult.result != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemFree][MemFreeExecute] MemFree strategy and execute failed, ubturbo rmrs error code="
            << migrateBackResult.result << ".";
        if (migrateBackResult.result == MEM_POOLING_RMRS_MIGRATE_FAILED_VM_DELETED) {
            return MEM_POOLING_MIGRATE_FAILED_VM_DELETED;
        }
        return migrateBackResult.result;
    }

    for (auto numaId : migrateBackResult.NumaIds) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemFree][MemFreeExecute] MemFree execute numaId: " << numaId << ".";
        MpResult ret = MemBackExecute(nodeId, numaId);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemFree][MemFreeExecute] MemFree execute failed.";
            return ret;
        }
    }
    return MEM_POOLING_OK;
}

MpResult MempoolBorrowModule::MemBorrowFailedRollback(const MemBorrowExecuteResult &borrowExecuteResult)
{
    MpResult ret = MEM_POOLING_ERROR;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemBorrow][MemBorrowStrategy] Failed to borrow the execution action set. Rollback starts.";
    for (std::string borrowId : borrowExecuteResult.borrowIds) {
        ret = MemBorrowExecutor::Instance().MemFreeWithOps(borrowId, false, false);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowStrategy] Rollback failed";
            return ret;
        }
    }

    return MEM_POOLING_OK;
}

MpResult MempoolBorrowModule::ValidateBorrowExecuteParam(const DestMemoryBorrowParam &destParam, MemMallocAttr &memAttr,
                                                         const MemBorrowExecuteResult &borrowExecuteResult,
                                                         uint64_t &totalSize)
{
    if (GetAndSetBlockSize(destParam.destNid) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    bool enableBorrowSplit = MpConfiguration::GetInstance().GetEnableBorrowSplit();
    MpResult ret = MEM_POOLING_ERROR;
    for (size_t i = 0; i < destParam.destNumaId.size(); i++) {
        if ((destParam.memSize[i] % (gBlockSize * MB_TO_KB) != 0) ||
            (enableBorrowSplit && destParam.memSize[i] > FOUR_GB)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] Invalid memSize which = " << destParam.memSize[i]
                << ", the memSize should be an integer multiple of " << (gBlockSize * MB_TO_KB)
                << "KB and less than 4G.";
            // 执行到某一次借用失败了需要回滚恢复到借用之前的状态
            ret = MemBorrowFailedRollback(borrowExecuteResult);
            if (ret != MEM_POOLING_OK) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[MemBorrow][MemBorrowExecute] Mem borrow failed and rollback failed.";
                return ret;
            }
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] Mem borrow failed but rollback success.";

            return MEM_POOLING_ERROR;
        }
        if (destParam.destNumaId[i] < 0) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] DestNumaId is invalid, numaId should not be less than 0.";
            // 执行到某一次借用失败了需要回滚恢复到借用之前的状态
            ret = MemBorrowFailedRollback(borrowExecuteResult);
            if (ret != MEM_POOLING_OK) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                    << "[MemBorrow][MemBorrowExecute] Mem borrow failed and rollback failed.";
                return ret;
            }
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] Mem borrow failed but rollback success.";

            return MEM_POOLING_ERROR;
        }

        memAttr.lenderLocs.push_back({destParam.destNid, destParam.destSocketId, destParam.destNumaId[i]});
        (void)memAttr.lenderSizes.emplace_back(destParam.memSize[i] * KB_TO_BYTES);
        totalSize += destParam.memSize[i] * KB_TO_BYTES;
    }

    return MEM_POOLING_OK;
}

MpResult MempoolBorrowModule::ValidateBorrowParamSamePlane(const SrcMemoryBorrowParam &srcParam,
                                                           const std::vector<DestMemoryBorrowParam> &destParams)
{
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    MpResult ret = UbseMemGetTopologyInfo(nodeTopology);
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowExecute] Get topo from rack failed!";
        return MEM_POOLING_ERROR;
    }
    std::string srcNidAndSocketId = srcParam.srcNid + "-" + std::to_string(srcParam.srcSocketId);
    auto it = nodeTopology.find(srcNidAndSocketId);
    if (it == nodeTopology.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Can't find " << srcNidAndSocketId << " in nodeTopology";
        return MEM_POOLING_ERROR;
    }
    std::vector<MemNodeData> foundNodeData = it->second;
    for (const auto &destParam : destParams) {
        // 查看destParam.destSocketId是否在foundNodeData中
        std::string socketIdStr = std::to_string(destParam.destSocketId);
        auto it = std::find_if(foundNodeData.begin(), foundNodeData.end(), [&socketIdStr](const MemNodeData &nodeData) {
            return nodeData.socket.socketId == socketIdStr;
        });
        if (it == foundNodeData.end()) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] destNodeSocket=" << destParam.destNid << "." << destParam.destSocketId
                << " is not on the same plane with srcNodeSocket=" << srcParam.srcNid << "." << srcParam.srcSocketId;
            return MEM_POOLING_ERROR;
        }
    }
    return MEM_POOLING_OK;
}

MpResult MempoolBorrowModule::ValidateDestNids(const SrcMemoryBorrowParam &srcParam,
                                               const std::vector<DestMemoryBorrowParam> &destParams)
{
    std::unordered_set<std::string> couldBorrowNodeSet;
    auto ret = MpParseGroupProviderConf::Instance().GetBorrowableList(srcParam.srcNid, couldBorrowNodeSet);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowExecute] GetBorrowableList failed.";
        return MEM_POOLING_ERROR;
    }
    for (auto &destParam : destParams) {
        if (couldBorrowNodeSet.find(destParam.destNid) == couldBorrowNodeSet.end()) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] destNid=" << destParam.destNid
                << " is not borrowable for srcNid=" << srcParam.srcNid;
            return MEM_POOLING_ERROR;
        }
        if (destParam.destNumaNum != 1) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] destNumaNum=" << destParam.destNumaNum << " is not equal to 1.";
            return MEM_POOLING_ERROR;
        }
        if (destParam.destNumaId.size() != destParam.destNumaNum || destParam.memSize.size() != destParam.destNumaNum) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] destNumaId_size=" << destParam.destNumaId.size()
                << "or memSize_size=" << destParam.memSize.size()
                << "is not equal to destNumaNum=" << destParam.destNumaNum << ".";
            return MEM_POOLING_ERROR;
        }
    }

    return MEM_POOLING_OK;
}

MpResult MempoolBorrowModule::MemBorrowExecute(const SrcMemoryBorrowParam &srcParam,
                                               const std::vector<DestMemoryBorrowParam> &destParams,
                                               MemBorrowExecuteResult &borrowExecuteResult)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowExecute] MemBorrowExecute start.";

    MpResult ret = MEM_POOLING_ERROR;
    if (srcParam.srcNumaId < 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] SrcNumaId is invalid, numaId=" << srcParam.srcNumaId << " less than 0.";
        return MEM_POOLING_ERROR;
    }
    if (MpConfiguration::GetInstance().GetMustSamePlane()) {
        ret = ValidateBorrowParamSamePlane(srcParam, destParams);
        if (ret != MEM_POOLING_OK) {
            return ret;
        }
    }
    ret = ValidateDestNids(srcParam, destParams);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }
    for (const auto &destParam : destParams) {
        ret = ExecuteSingleBorrow(destParam, srcParam, borrowExecuteResult);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] ExecuteSingleBorrow failed, destParam=" << destParam.ToString()
                << ".";
            return MEM_POOLING_ERROR;
        }
    }

    std::vector<std::string> borrowIdsCompletedList;
    ret = BorrowIdsCompleted::Instance().Query(borrowIdsCompletedList);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] GetBorrowIdsCompletedList failed.";
    }

    for (const auto &borrowId : borrowIdsCompletedList) {
        ret = BorrowIdsCompleted::Instance().Remove(borrowId);
        if (ret != MEM_POOLING_OK) {
            // 可靠性保障，如果失败，不影响主功能
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] Remove borrowed borrowId failed, borrowId=" << borrowId << ".";
        }
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowExecute] MemBorrowExecute finished.";

    return MEM_POOLING_OK;
}

MpResult MempoolBorrowModule::ExecuteSingleBorrow(const DestMemoryBorrowParam &destParam,
                                                  const SrcMemoryBorrowParam &srcParam,
                                                  MemBorrowExecuteResult &borrowExecuteResult)
{
    if (destParam.destNumaId.size() != destParam.memSize.size()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowExecute] The size of destNumaId "
                                                             "list is not equal to the size of memSize list.";
        return MEM_POOLING_ERROR;
    }

    RackCreateResourceWaterBorrowAttr attr;
    MemMallocAttr memAttr;
    std::string name;
    int16_t presentNumaId;

    memAttr.srcNid = srcParam.srcNid;
    memAttr.srcSocket = srcParam.srcSocketId;
    memAttr.srcNuma = srcParam.srcNumaId;
    memAttr.uid = srcParam.uid;
    memAttr.username = srcParam.username;
    memAttr.dstNodeNum = 1; // 当前限制从单节点借用内存，且该参数对自定义借用无影响
    memAttr.lenderNumaSize = destParam.destNumaNum;

    MpResult ret = ValidateBorrowExecuteParam(destParam, memAttr, borrowExecuteResult, attr.size);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Invalid params exist in destParam: " << destParam.ToString() << ".";
        return ret;
    }
    attr.waterMallocAttr = memAttr;

    ret = MemBorrowExecutor::Instance().MemBorrow(srcParam.srcNid, attr, name, presentNumaId);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Mem borrow failed, borrowId = " << name << ".";
        ret = MemBorrowFailedRollback(borrowExecuteResult);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] Mem borrow failed and rollback failed.";
            return ret;
        }
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Mem borrow failed but rollback success.";
        return MEM_POOLING_ERROR;
    }

    (void)borrowExecuteResult.borrowIds.emplace_back(name);
    (void)borrowExecuteResult.presentNumaId.emplace_back(presentNumaId);

    return MEM_POOLING_OK;
}

std::vector<std::string> GenerateBorrowCandidateList(const SrcMemoryBorrowParam &srcParam)
{
    std::unordered_map<std::string, BorrowItem> timeoutNodeInfo;
    std::unordered_set<std::string> candidateNodeSet;
    MemReturnManager::Instance().QueryAll(timeoutNodeInfo);
    auto allNodeInfo = UbseNodeController::GetInstance().GetAllNodes();
    for (const auto &[nodeId, _] : allNodeInfo) {
        (void)candidateNodeSet.insert(nodeId);
    }
    for (const auto &[_, failItem] : timeoutNodeInfo) {
        if (failItem.srcNid == srcParam.srcNid &&
            (srcParam.srcSocketId == -1 ||
             MemManager::Instance().JudgeSampPlane(srcParam.srcNid, srcParam.srcSocketId, failItem.dstNid,
                                                   failItem.dstSocketId))) {
            candidateNodeSet.erase(failItem.dstNid);
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow]Mem borrow candidateNodeSet remove timeout node=" << failItem.dstNid << ".";
        }
    }
    return std::vector<std::string>(candidateNodeSet.begin(), candidateNodeSet.end());
}

MpResult MempoolBorrowModule::ProcessSingleBorrowInOverCommit(const SrcMemoryBorrowParam &srcParam,
                                                              const UbseMemNumaCandidateOpt &opt,
                                                              const bool &isFault,
                                                              UbseMemNumaDesc &desc)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "ProcessSingleBorrowInOverCommit start.";
    std::string name;
    std::string importNodeId = srcParam.srcNid;
    MemBorrowExecutor::Instance().GenerateUniqueId(importNodeId, name, isFault);
    if (isFault) {
        MpResult retBorrowIdInFaultProcess = BorrowIdInFaultProcess::Instance().Update(name);
        if (retBorrowIdInFaultProcess != MEM_POOLING_OK) {
            UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemReturn] Update SmapEnableCompleted numaId failed. ret=" << retBorrowIdInFaultProcess << ".";
            return MEM_POOLING_ERROR;
        }
    }
    UbseMemBorrower borrower{.nodeId = srcParam.srcNid,
                             .affinitySocketId = srcParam.srcSocketId,
                             .uid = srcParam.uid,
                             .username = srcParam.username};

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemBorrow] Borrow Start srcParam " << srcParam.ToString() << ".";
    auto ret = UbseMemNumaCreateWithCandidate(name, borrower, opt, desc);
    if (ret != UBSE_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow] Mem borrow failed, size=" << opt.size << ", name=" << name
            << ", res=" << static_cast<uint32_t>(ret) << ".";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "ProcessSingleBorrowInOverCommit end.";

    return MEM_POOLING_OK;
}

MpResult MempoolBorrowModule::MemBorrowExecuteInOverCommit(const SrcMemoryBorrowParam &srcParam,
                                                           const std::vector<uint64_t> &borrowSizes,
                                                           const WaterMark &waterMark,
                                                           MemBorrowExecuteResult &borrowExecuteResult,
                                                           const bool isFault)
{
    if (borrowSizes.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow] Borrow Size is Empty. srcParam: " << srcParam.ToString() << ".";
        return MEM_POOLING_OK;
    }
    std::vector<uint64_t> sortedSizes(borrowSizes.begin(), borrowSizes.end());
    std::sort(sortedSizes.begin(), sortedSizes.end(), [](uint64_t a, uint64_t b) { return a > b; });
    std::vector<std::string> candidateNodeList = GenerateBorrowCandidateList(srcParam);
    for (const auto &borrowSize : sortedSizes) {
        UbseMemNumaDesc desc;
        UbseMemNumaCandidateOpt opt;
        opt.slotIds = candidateNodeList;
        opt.size = borrowSize;
        opt.distance = ubse::mem::controller::MEM_DISTANCE_L0;
        opt.highWatermark = waterMark.highWaterMark;
        int16_t numaId = srcParam.srcNumaId;
        if (memcpy_s(opt.usrInfo, sizeof(numaId), &numaId, sizeof(numaId)) != EOK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow] SrcNumaId memcpy_s failed.";
        }
        UbseResult res = ProcessSingleBorrowInOverCommit(srcParam, opt, isFault, desc);
        if (res != UBSE_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow] ProcessSingleBorrow failed.";
            continue;
        }

        (void)borrowExecuteResult.borrowIds.emplace_back(desc.name);
        (void)borrowExecuteResult.presentNumaId.emplace_back(static_cast<uint16_t>(desc.numaId));
    }

    if (borrowExecuteResult.borrowIds.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow]All Mem borrow failed.";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult MempoolBorrowModule::MemBackExecute(std::string nodeId, uint16_t numaId)
{
    if (nodeId.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemFree][MemFreeExecute] Invalid node id or numa id.";
        return MEM_POOLING_ERROR;
    }
    std::vector<std::string> namesOfRemoteNuma;
    NodeMemInfo nodeMemInfo;

    MpResult ret = MemManager::Instance().GetNodeMemInfo(nodeId, nodeMemInfo);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemFree][MemFreeExecute] Get node mem info failed.";
        return MEM_POOLING_ERROR;
    }

    ret = BorrowRecordHelper::Instance().GetBorrowIdByNumaId(namesOfRemoteNuma, numaId, nodeId);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemFree][MemFreeExecute] Get borrowId by numaId failed.";
        return MEM_POOLING_ERROR;
    }
    for (auto &name : namesOfRemoteNuma) {
        ret = MemBorrowExecutor::Instance().MemFree(name);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemFree][MemFreeExecute] Memory free with not force delete failed.";
            return ret;
        }
    }
    return MEM_POOLING_OK;
}

// 从节点调用函数
uint32_t MigrateStrategyRecvHandler(const turbo::rmrs::MigrateStrategyParam &migrateStrategyParam,
                                    turbo::rmrs::MigrateStrategyResult &migrateStrategyResult)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][Strategy] IPC message sent started.";

    turbo::rmrs::MigrateStrategyParamRMRS migrateStrategyParamRMRS; // 转换为从节点和osturbo里面需要的入参
    auto retParam = ConvertMigrateStrategyParam(migrateStrategyParam, migrateStrategyParamRMRS);
    if (retParam != MEM_POOLING_OK) {
        migrateStrategyParamRMRS.vmInfoList.clear();
        migrateStrategyParamRMRS.borrowSize = 0;
        migrateStrategyParamRMRS.pidRemoteNumaMap.clear();
        migrateStrategyParamRMRS.timeOutNumas.clear();
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][Strategy] Convert Param failed.";
    }

    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MemMigrate][Strategy] IPC message vm.size() = " << migrateStrategyParamRMRS.vmInfoList.size() << ".";
    // 调用osturbo-rmrs的函数指针
    auto ret = MempoolingMessage::rmrsMigrateStrategy(migrateStrategyParamRMRS, migrateStrategyResult);
    if (ret != IPC_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemMigrate][Strategy] MigrateStrategyRecvHandler failed res: " << ret << ".";
        MpResult errCode = MEM_POOLING_ERROR;
        if (ret == IPC_BAD_CONNECT) {
            errCode = MEM_POOLING_TURBO_CONNECT_ERROR;
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemMigrate][Strategy] MemPooling Turbo connect ERROR.";
        }
        return errCode;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][Strategy] IPC message sent successfully.";
    return MEM_POOLING_OK;
}

// 获取 numaInfoMap 所有numa节点的索引对应的NUMA信息(numaID 大页总数量 大页空闲数量 是否本地 是否远端)
void DistributeNumaMemInfo(std::vector<mempooling::exportV2::NumaInfo> &numaInfos,
                           std::map<uint16_t, NumaHugePageInfo> &numaInfoMap,
                           std::vector<NumaHugePageInfo> &numaHugePageInfoSumList)
{
    for (mempooling::exportV2::NumaInfo numaInfo : numaInfos) {
        NumaHugePageInfo info;
        info.numaId = numaInfo.metaData.numaId;
        auto it = numaInfo.metaData.numaPageInfo.find(HUGEPAGE_2M_KB);
        if (it != numaInfo.metaData.numaPageInfo.end()) {
            info.hugePageFree = it->second.hugePageFree;
            info.hugePageTotal = it->second.hugePageTotal;
        }
        info.isLocal = numaInfo.metaData.isLocal;

        numaInfoMap[info.numaId] = info;
        numaHugePageInfoSumList.push_back(info);

        LOG_DEBUG << "[MemMigrate][Strategy][DistributeNumaMemInfo] NUMA node info: numaId = " << info.numaId
                  << ", hugePageFree = " << info.hugePageFree << ", hugePageTotal = " << info.hugePageTotal
                  << ", isLocal = " << info.isLocal << ", socketId = " << numaInfo.metaData.socketId << ".";
    }
}

// 获取远端numaID数组remoteNumaIdList
void GetRemoteNumaList(std::vector<NumaHugePageInfo> &numaHugePageInfoSumList, std::vector<uint16_t> &remoteNumaIdList)
{
    for (NumaHugePageInfo info : numaHugePageInfoSumList) {
        if (info.isLocal == false) {
            remoteNumaIdList.push_back(info.numaId);
            LOG_DEBUG << "[MemMigrate][Strategy] The remoteNumaIdList push_back " << info.numaId << ".";
        }
    }
    LOG_DEBUG << "[MemMigrate][Strategy] The remoteNumaIdList size() = " << remoteNumaIdList.size() << ".";
}

// 打印查询虚拟机信息
void GetLocalVmInfo(std::vector<VmNumaInfoBrr> &allVmNumaInfoInfoList, std::map<pid_t, VmNumaInfoBrr> &VmNumaInfoMap,
                    std::vector<mempooling::exportV2::VmDomainInfo> &vmDomainInfos)
{
    LOG_DEBUG << "[MemMigrate][Strategy] Enter GetLocalVmInfo.";

    if (vmDomainInfos.size() == 0) {
        LOG_ERROR << "[MemMigrate][Strategy] VM data is empty. Nothing to print.";
        return;
    }

    for (mempooling::exportV2::VmDomainInfo vmDomainInfo : vmDomainInfos) {
        VmNumaInfoBrr info;
        info.pid = vmDomainInfo.metaData.pid;
        for (const auto &[_, numaInfo] : vmDomainInfo.numaInfo) {
            if (numaInfo.isLocal) {
                // 本地 NUMA
                info.localNumaId = numaInfo.numaId;
                info.localUsedMem = numaInfo.usedMem;
            } else {
                // 远端 NUMA
                info.remoteNumaId = numaInfo.numaId;
                info.remoteUsedMem = numaInfo.usedMem;
            }
        }
        allVmNumaInfoInfoList.push_back(info);
        VmNumaInfoMap[info.pid] = info;
        LOG_DEBUG << "[MemMigrate][Strategy] VM info with PID = " << info.pid << ".";
        LOG_DEBUG << "[MemMigrate][Strategy] PID = " << info.pid << ", localNumaId = " << info.localNumaId << ".";
        LOG_DEBUG << "[MemMigrate][Strategy] PID = " << info.pid << ", remoteNumaId = " << info.remoteNumaId << ".";
        LOG_DEBUG << "[MemMigrate][Strategy] PID = " << info.pid << ", localUsedMem = " << info.localUsedMem << ".";
        LOG_DEBUG << "[MemMigrate][Strategy] PID = " << info.pid << ", remoteUsedMem = " << info.remoteUsedMem << ".";
    }
    LOG_DEBUG << "[MemMigrate][Strategy] Exit GetLocalVmInfo.";
}

// 获取NUMA数据
uint32_t GetNumaData(std::vector<mempooling::exportV2::NumaInfo> &numaInfos, std::vector<uint16_t> &remoteNumaIdList,
                     std::map<uint16_t, NumaHugePageInfo> &numaInfoMap,
                     std::vector<NumaHugePageInfo> &numaHugePageInfoSumList)
{
    // 采集当前从节点numa信息
    uint32_t queryNumaRes = mempooling::exportV2::Exporter::GetNumaInfoImmediately(numaInfos);
    if (MEM_POOLING_OK != queryNumaRes) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][Strategy] Failed to get NUMA info.";
        return MEM_POOLING_ERROR;
    }
    if (numaInfos.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemMigrate][Strategy] Current node has no NUMA data.";
        return MEM_POOLING_ERROR;
    }

    // 获取 numaInfoMap 所有numa节点的索引对应的NUMA信息(numaID 大页总数量 大页空闲数量 是否本地 是否远端)
    DistributeNumaMemInfo(numaInfos, numaInfoMap, numaHugePageInfoSumList);
    // 获取远端numaID数组remoteNumaIdList
    GetRemoteNumaList(numaHugePageInfoSumList, remoteNumaIdList);
    return MEM_POOLING_OK;
}

// 获取虚拟机信息
uint32_t GetVMData(std::vector<mempooling::exportV2::VmDomainInfo> &vmDomainInfos,
                   std::vector<VmNumaInfoBrr> &allVmNumaInfoInfoList, std::map<pid_t, VmNumaInfoBrr> &vmNumaInfoMap)
{
    // 采集当前从节点vm信息
    uint32_t retVmRes = mempooling::exportV2::Exporter::GetVmInfoImmediately(vmDomainInfos);
    if (retVmRes != MEM_POOLING_OK) {
        LOG_ERROR << "[MemMigrate][Strategy] Failed to get VM info.";
        return MEM_POOLING_ERROR;
    }
    if (vmDomainInfos.empty()) {
        LOG_INFO << "[MemMigrate][Strategy] Current node has no VM.";
        return MEM_POOLING_ERROR;
    }
    // 提取 allVmNumaInfoInfoList, vmNumaInfoMap数据
    GetLocalVmInfo(allVmNumaInfoInfoList, vmNumaInfoMap, vmDomainInfos); // 获取虚拟机信息
    return MEM_POOLING_OK;
}

bool IsSamePlaneBorrow(ConvertVmParam vmParam, uint16_t remoteNumaId,
                       const std::vector<mempooling::exportV2::NumaInfo> &numaInfos,
                       const std::vector<turbo::rmrs::RemoteNumaSocketInfo> &remoteNumaSocketInfo,
                       const std::unordered_map<std::string, std::vector<turbo::rmrs::MemNodeDataNew>> &nodeTopology)
{
    pid_t pid = vmParam.pid;
    uint16_t localNumaId = vmParam.localNumaId;
    std::string srcNid = vmParam.srcNid;
    uint16_t srcSocketId;

    std::string dstNid = "";
    uint16_t dstSocketId;

    // 从节点numa信息查询中获取本地numa对应的socket

    for (mempooling::exportV2::NumaInfo numaInfo : numaInfos) {
        LOG_DEBUG << "[MemMigrate][Strategy][Plane] Param, localNumaId=" << localNumaId
                  << ", numaId=" << numaInfo.metaData.numaId << ", srcSocketId=" << numaInfo.metaData.socketId;
        if (numaInfo.metaData.numaId == localNumaId) {
            srcSocketId = numaInfo.metaData.socketId;
            break;
        }
    }
    LOG_DEBUG << "[MemMigrate][Strategy] Compare same plane, pid=" << pid << ", srcNid=" << srcNid
              << ", srcSocketId=" << srcSocketId;

    // 从内存账本中获取远端NUMA对应的socket
    for (turbo::rmrs::RemoteNumaSocketInfo socketInfo : remoteNumaSocketInfo) {
        if (socketInfo.borrowRemoteNuma == remoteNumaId) {
            dstNid = socketInfo.lentNode;
            dstSocketId = socketInfo.lentSocketId;
            break;
        }
    }
    LOG_DEBUG << "[MemMigrate][Strategy] Compare same plane, remoteNumaId=" << remoteNumaId << ", dstNid=" << dstNid
              << ", dstSocketId=" << dstSocketId;

    // 从拓扑信息中获取，是否属于直连节点
    std::string key = srcNid + "-" + std::to_string(srcSocketId);

    for (const auto &pair : nodeTopology) {
        const std::string &host = pair.first;
        const auto &memNodeList = pair.second;
        if (host != key) {
            continue;
        }
        LOG_DEBUG << "[MemMigrate][Strategy] Host= " << host << ", Total MemNodeData count= " << memNodeList.size();
        for (size_t i = 0; i < memNodeList.size(); ++i) {
            const auto &memNode = memNodeList[i];
            LOG_DEBUG << "[MemMigrate][Strategy] Index= " << i << ", NodeId= " << memNode.nodeId
                      << ", socketId= " << memNode.socket.socketId;
            if (memNode.nodeId == dstNid && memNode.socket.socketId == std::to_string(dstSocketId)) {
                LOG_INFO << "[MemMigrate][Strategy] The pid=" << pid << ", remoteNumaId=" << remoteNumaId
                         << ", Same-plane memory borrow.";
                return true;
            }
        }
    }
    LOG_DEBUG << "[MemMigrate][Strategy] The pid=" << pid << ", remoteNumaId=" << remoteNumaId
              << ", Not a same-plane memory borrow.";
    LOG_ERROR << "[MemMigrate][Strategy] Not a same-plane memory borrow.";
    return false;
}

uint32_t ConvertMigrateStrategyParam(const turbo::rmrs::MigrateStrategyParam &migrateStrategyParam,
                                     turbo::rmrs::MigrateStrategyParamRMRS &migrateStrategyParamRMRS)
{
    migrateStrategyParamRMRS.vmInfoList = migrateStrategyParam.vmInfoList;
    migrateStrategyParamRMRS.borrowSize = migrateStrategyParam.borrowSize;
    migrateStrategyParamRMRS.timeOutNumas = migrateStrategyParam.timeOutNumas;

    NumaQueryInfo numaQueryInfo;
    VMQueryInfo vmQueryInfo;
    // 获取nmuma信息
    uint32_t ret = GetNumaData(numaQueryInfo.numaInfos, numaQueryInfo.remoteNumaIdList, numaQueryInfo.numaInfoMap,
                               numaQueryInfo.numaHugePageInfoSumList);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[MemMigrate][Strategy] Failed to get NUMA data.";
        return MEM_POOLING_ERROR;
    }

    // 获取虚拟机信息
    ret = GetVMData(vmQueryInfo.vmDomainInfos, vmQueryInfo.allVmNumaInfoInfoList, vmQueryInfo.vmNumaInfoMap);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[MemMigrate][Strategy] Failed to get VM data.";
        return MEM_POOLING_ERROR;
    }

    // 输入的所有虚拟机--以及对应本地numa
    for (size_t i = 0; i < migrateStrategyParam.vmInfoList.size(); i++) {
        pid_t pid = migrateStrategyParam.vmInfoList[i].pid;
        auto it = vmQueryInfo.vmNumaInfoMap.find(pid);
        uint16_t localNumaId;
        if (it != vmQueryInfo.vmNumaInfoMap.end()) {
            localNumaId = it->second.localNumaId;
        } else {
            LOG_DEBUG << "[MemMigrate][Strategy] Can not find pid=" << pid << ".";
            LOG_ERROR << "[MemMigrate][Strategy] Can not find pid in map.";
            return MEM_POOLING_ERROR;
        }

        migrateStrategyParamRMRS.pidRemoteNumaMap[pid] = std::vector<uint16_t>();
        std::vector<uint16_t> tmp;
        for (auto remoteNumaId : numaQueryInfo.remoteNumaIdList) {
            ConvertVmParam vmParam = {pid, localNumaId, migrateStrategyParam.nodeId};
            bool isSamePlane = IsSamePlaneBorrow(vmParam, remoteNumaId, numaQueryInfo.numaInfos,
                                                 migrateStrategyParam.remoteNumaInfo,
                                                 migrateStrategyParam.nodeTopology);
            // 精简账本和拓扑信息
            if (isSamePlane) {
                migrateStrategyParamRMRS.pidRemoteNumaMap[pid].push_back(remoteNumaId);
            } else {
                tmp.push_back(remoteNumaId);
            }
            if (!MpConfiguration::GetInstance().GetMustSamePlane()) {
                (void)migrateStrategyParamRMRS.pidRemoteNumaMap[pid].insert(
                    migrateStrategyParamRMRS.pidRemoteNumaMap[pid].end(), tmp.begin(), tmp.end());
            }
        }
    }

    LOG_DEBUG << "[MemMigrate][Strategy] Convert Migrate Strategy Param successfully.";
    return MEM_POOLING_OK;
}

} // namespace mempooling
