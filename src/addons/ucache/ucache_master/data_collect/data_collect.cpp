/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "data_collect.h"
#include <regex>
#include "ubse_node.h"
#include "ucache_config.h"
#include "ucache_error.h"
#include "ucache_string_util.h"

namespace ucache {

namespace data_collect {
using namespace ubse::log;
using namespace ucache::deserialize;

std::map<std::string, PhyNodeStat> DataCollect::phyNodeStatMap;
std::vector<BorrowStrategyRawData> DataCollect::borrowStrategyRawData;
std::map<std::string, std::map<int, uint64_t>> DataCollect::loanableTotalBorrowMemMap;
std::map<std::string, std::vector<std::string>> DataCollect::physicalTopo;
std::map<std::string, std::map<int, int>> DataCollect::numaSocketMap;
std::map<std::string, std::vector<NodeMemBorrowInfo>> DataCollect::lendMemMap;
std::map<std::string, std::vector<NodeMemBorrowInfo>> DataCollect::borrowMemMap;
std::map<std::string, NodeMemoryInfo> DataCollect::nodeMemInfos;
std::map<std::string, std::map<std::string, CgroupInfo>> DataCollect::cgroupInfo;
std::queue<std::map<std::string, std::map<std::string, CgroupInfos>>> DataCollect::cgroupInfosQueue;
std::map<std::string, std::map<std::string, NodeInfo>> DataCollect::nodeInfos;
std::map<std::string, uint64_t> DataCollect::memWaterMarkInfos;
std::queue<std::map<std::string, uint64_t>> DataCollect::timeStampsCgroup;
std::queue<std::map<std::string, uint64_t>> DataCollect::timeStampsNode;

const int PAGE_SIZE = 4; // 单位为KB
const int KB_TO_BYTE = 1024;
const uint64_t GB_TO_BYTE = 1024 * 1024 * 1024;
const uint64_t MB_TO_BYTE = 1024 * 1024;
const int MAX_FAILED_TIMES = 10;

using FuncPtrType = uint32_t (*)();
struct Task {
    FuncPtrType func;
    const char* errMsg;
};
inline uint32_t CheckAndRun(FuncPtrType func, const std::string& message)
{
    uint32_t result = func();
    if (result != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << message;
        return result;
    }
    return result;
}

uint32_t DataCollect::GenerateBorrowStrategyRawData()
{
    borrowStrategyRawData.clear();
    for (const auto& pair1 : nodeInfos) {
        BorrowStrategyRawData tmpRawData = {};
        std::string nodeId = pair1.first;
        tmpRawData.nodeId = nodeId;
        if (memWaterMarkInfos.find(nodeId) == memWaterMarkInfos.end()) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "nodeId" << nodeId << "not found in memWaterMarkInfos!";
            return UCACHE_ERR;
        }
        tmpRawData.freeMemMin = memWaterMarkInfos[nodeId];
        std::map<std::string, NodeInfo> nodeInfo = pair1.second;
        for (const auto& pair2 : nodeInfo) {
            std::regex rgx("\\d+");
            std::smatch match;
            int numaId = 0;
            if (!std::regex_search(pair2.first, match, rgx)) {
                continue;
            }

            uint32_t ret = SafeStoInt(match.str(), numaId);
            if (ret != UCACHE_OK) {
                UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                    << "Convert numaId to int failed! ret is " << ret;
                return ret;
            }

            NodeInfo nodeInfo = pair2.second;
            if (!nodeInfo.isRemote) {
                tmpRawData.localMemInfo.total += nodeInfo.memTotalBytes;
                tmpRawData.localMemInfo.available += nodeInfo.memFreeBytes;
                tmpRawData.localMemInfo.used += nodeInfo.memUsedBytes;
                tmpRawData.localMemInfo.pagecache += (nodeInfo.activeFileBytes + nodeInfo.inactiveFileBytes);
            } else {
                MemInfo tmpInfo;
                tmpInfo.total = nodeInfo.memTotalBytes;
                tmpInfo.available = nodeInfo.memFreeBytes;
                tmpInfo.used = nodeInfo.memUsedBytes;
                tmpInfo.pagecache = nodeInfo.activeFileBytes + nodeInfo.inactiveFileBytes;
                tmpRawData.remoteNumaMemInfo[numaId] = tmpInfo;
            }
        }
        borrowStrategyRawData.emplace_back(tmpRawData);
    }
    return UCACHE_OK;
}

uint32_t DataCollect::GenerateLoanableTotalBorrowMemMap()
{
    loanableTotalBorrowMemMap.clear();
    std::vector<UbseNodeNumaInfo> numaNodeInfoList{};
    uint32_t ret = Deserialize::GetNodeMemInfo(numaNodeInfoList);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Get node mem info from deserialize failed!";
        return UCACHE_ERR;
    }
    for (const auto& numaInfo : numaNodeInfoList) {
        std::string nodeId = numaInfo.nodeId;
        uint32_t numaId = numaInfo.numaId;
        uint64_t reservedMem = (numaInfo.memTotal * numaInfo.mReservedMemRatio) / 100;
        uint64_t lentMem = numaInfo.memLent;
        uint64_t sharedMem = numaInfo.memShared;
        uint64_t loanableSize = 0;
        if (reservedMem < lentMem + sharedMem) {
            UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "ReservedMem is less than lentMem and sharedMem. "
                << "nodeId=" << nodeId << ", numaId=" << numaId << ", reservedMem=" << reservedMem
                << ", lentMem=" << lentMem << ", sharedMem=" << sharedMem;
        } else {
            loanableSize = reservedMem - lentMem - sharedMem;
        }
        loanableTotalBorrowMemMap[nodeId][numaId] = loanableSize;
    }
    return UCACHE_OK;
}

uint32_t ParseRackNodeId(const std::string& originNodeId, std::string& nodeId)
{
    size_t pos = originNodeId.rfind("-");
    if (pos == std::string::npos) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "NodeId=" << originNodeId << "not found - !";
        return UCACHE_ERR;
    }
    nodeId = originNodeId.substr(0, pos);
    return UCACHE_OK;
}

void PrintPhysicalTopo(const std::map<std::string, std::vector<std::string>>& physicalTopo)
{
    for (auto& pair : physicalTopo) {
        for (const auto& nodeId : pair.second) {
            UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "NodeId=" << pair.first << ", which is linked to nodeId= " << nodeId;
        }
    }
}
static void SetPhyNodeStatMap()
{
    std::vector<ubse::nodeController::UbseNodeTopology> topologies;
    uint32_t ret = ubse::nodeController::UbseGetNodeTopology(topologies);
    if (ret != 0) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Get topologies from rack failed! ret is " << ret;
        return;
    }
    for (const auto& topo : topologies) {
        // 如果发生过故障，未上电是恢复状态，上电后置位活跃的正常状态
        if (topo.state != ubse::nodeController::UbseNodeState::UP &&
            DataCollect::phyNodeStatMap[topo.nodeId] == PhyNodeStat::FAULT) {
            DataCollect::phyNodeStatMap[topo.nodeId] = PhyNodeStat::RECOVER;
        }
        if (topo.state == ubse::nodeController::UbseNodeState::UP &&
            DataCollect::phyNodeStatMap[topo.nodeId] == PhyNodeStat::RECOVER) {
            DataCollect::phyNodeStatMap[topo.nodeId] = PhyNodeStat::ACTIVE;
        }
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << topo.nodeId << " state is " << static_cast<long>(DataCollect::phyNodeStatMap[topo.nodeId]);
    }
}

static uint32_t CheckNodeStat(const ubse::nodeController::MemNodeData& node)
{
    // 节点未连接RM，排除
    if (!node.isRegisterRm) {
        return true;
    }
    // 节点故障未激活，排除
    auto it = DataCollect::phyNodeStatMap.find(node.nodeId);
    if (it != DataCollect::phyNodeStatMap.end() && it->second != PhyNodeStat::ACTIVE) {
        return true;
    }
    return false;
}

uint32_t DataCollect::GeneratePhysicalTopo()
{
    physicalTopo.clear();
    std::unordered_map<std::string, std::vector<ubse::nodeController::MemNodeData>> nodeTopology;
    uint32_t ret = ubse::nodeController::UbseMemGetTopologyInfo(nodeTopology);
    if (ret != 0) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Get topo from rack failed! ret is " << ret;
        return UCACHE_ERR;
    }
    std::vector<std::string> allNodeIds;
    for (const auto& pair : nodeTopology) {
        std::string nodeId;
        if (ParseRackNodeId(pair.first, nodeId) != UCACHE_OK) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Parse rack node id failed!";
            return UCACHE_ERR;
        }
        if (std::find(allNodeIds.begin(), allNodeIds.end(), nodeId) == allNodeIds.end()) {
            allNodeIds.emplace_back(nodeId);
        }
        if (physicalTopo.find(nodeId) != physicalTopo.end()) {
            continue;
        }
        std::vector<std::string> connectedNodeIds;
        for (const auto& node : pair.second) {
            if (CheckNodeStat(node)) {
                continue;
            }
            if (std::find(connectedNodeIds.begin(), connectedNodeIds.end(), node.nodeId) == connectedNodeIds.end()) {
                connectedNodeIds.emplace_back(node.nodeId);
            }
        }
        physicalTopo[nodeId] = connectedNodeIds;
    }
    for (auto nodeId : allNodeIds) {
        // 初始化phyNodeStatMap
        if (phyNodeStatMap.find(nodeId) == phyNodeStatMap.end()) {
            phyNodeStatMap[nodeId] = PhyNodeStat::ACTIVE;
        }
        UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "New node id=" << nodeId;
    }
    UcacheConfig::GetInstance().SetNodeIds(allNodeIds);
    SetPhyNodeStatMap();
    PrintPhysicalTopo(physicalTopo);
    return UCACHE_OK;
}
uint32_t GeneratePerNodeNumaSocketMap(const std::vector<ubse::nodeController::MemNodeData>& memNodeDataVec,
                                      std::map<std::string, std::map<int, int>>& numaSocketMap)
{
    for (const auto& memNodeData : memNodeDataVec) {
        int socketId = 0;
        uint32_t ret = SafeStoInt(memNodeData.socket.socketId, socketId);
        if (ret != UCACHE_OK) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Convert socketId to int failed! ret is " << ret;
            return ret;
        }

        for (const auto& numa : memNodeData.socket.numas) {
            int numaId = 0;
            ret = SafeStoInt(numa.numaId, numaId);
            if (ret != UCACHE_OK) {
                UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                    << "Convert numaId to int failed! ret is " << ret;
                return ret;
            }

            numaSocketMap[memNodeData.nodeId][numaId] = socketId;
        }
    }
    return UCACHE_OK;
}

void PrintNumaSocketMap(const std::map<std::string, std::map<int, int>>& numaSocketMap)
{
    for (auto& pair1 : numaSocketMap) {
        for (const auto& pair2 : pair1.second) {
            UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "NodeId=" << pair1.first << " numaId=" << pair2.first << " socketId=" << pair2.second;
        }
    }
}

uint32_t DataCollect::GenerateNumaSocketMap()
{
    numaSocketMap.clear();
    std::unordered_map<std::string, std::vector<ubse::nodeController::MemNodeData>> nodeTopology;
    uint32_t ret = ubse::nodeController::UbseMemGetTopologyInfo(nodeTopology);
    if (ret != 0) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Get topo from rack failed!";
        return UCACHE_ERR;
    }
    for (const auto& pair : nodeTopology) {
        ret = GeneratePerNodeNumaSocketMap(pair.second, numaSocketMap);
        if (ret != UCACHE_OK) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Generate per node numa socket map failed!";
            return UCACHE_ERR;
        }
    }
    PrintNumaSocketMap(numaSocketMap);
    return UCACHE_OK;
}

void DataCollect::GenerateLendMap(const BorrowMemInfo& borrowMemInfo, const std::string& lendId,
                                  std::map<std::string, std::vector<NodeMemBorrowInfo>>& MemMap)
{
    std::string name = borrowMemInfo.name;
    std::string destNodeId = borrowMemInfo.borrowNodeId;
    std::string srcNodeId = borrowMemInfo.lentNodeId;
    int srcNumaId = borrowMemInfo.lentNumaInfos[0].numaId;
    int dstNumaId = borrowMemInfo.borrowRemoteNuma >= 0 ? borrowMemInfo.borrowRemoteNuma :
                                                          borrowMemInfo.borrowLocalNuma;
    uint64_t lentSize = static_cast<uint64_t>(borrowMemInfo.lentNumaInfos[0].lentSize);

    if (MemMap.find(lendId) == MemMap.end()) {
        std::vector<NodeMemBorrowInfo> nodeMemBorrowInfos;
        NodeMemBorrowInfo nodeMemBorrowInfo;
        nodeMemBorrowInfo.srcNodeId = srcNodeId;
        nodeMemBorrowInfo.destNodeId = destNodeId;
        nodeMemBorrowInfo.totalSize = borrowMemInfo.size;
        nodeMemBorrowInfo.dstNumaId = dstNumaId;
        nodeMemBorrowInfo.numaNodeBorrowSize[srcNumaId][name] = lentSize;
        nodeMemBorrowInfos.emplace_back(nodeMemBorrowInfo);
        MemMap[lendId] = nodeMemBorrowInfos;
        return;
    }

    auto it = std::find_if(MemMap[lendId].begin(), MemMap[lendId].end(),
                           [destNodeId, dstNumaId](const NodeMemBorrowInfo& item) {
                               return item.destNodeId == destNodeId && item.dstNumaId == dstNumaId;
                           });
    if (it != MemMap[lendId].end()) {
        it->totalSize += borrowMemInfo.size;
        it->numaNodeBorrowSize[srcNumaId][name] = lentSize;
    } else {
        NodeMemBorrowInfo nodeMemBorrowInfo;
        nodeMemBorrowInfo.srcNodeId = srcNodeId;
        nodeMemBorrowInfo.destNodeId = destNodeId;
        nodeMemBorrowInfo.totalSize = borrowMemInfo.size;
        nodeMemBorrowInfo.dstNumaId = dstNumaId;
        nodeMemBorrowInfo.numaNodeBorrowSize[srcNumaId][name] = lentSize;
        MemMap[lendId].emplace_back(nodeMemBorrowInfo);
    }
}

void DataCollect::GenerateBorrowMap(const BorrowMemInfo& borrowMemInfo, const std::string& borrowId,
                                    std::map<std::string, std::vector<NodeMemBorrowInfo>>& MemMap)
{
    std::string name = borrowMemInfo.name;
    std::string destNodeId = borrowMemInfo.borrowNodeId;
    std::string srcNodeId = borrowMemInfo.lentNodeId;
    int srcNumaId = borrowMemInfo.lentNumaInfos[0].numaId;
    int dstNumaId = borrowMemInfo.borrowRemoteNuma >= 0 ? borrowMemInfo.borrowRemoteNuma :
                                                          borrowMemInfo.borrowLocalNuma;
    uint64_t lentSize = static_cast<uint64_t>(borrowMemInfo.lentNumaInfos[0].lentSize);

    if (MemMap.find(borrowId) == MemMap.end()) {
        std::vector<NodeMemBorrowInfo> nodeMemBorrowInfos;
        NodeMemBorrowInfo nodeMemBorrowInfo;
        nodeMemBorrowInfo.srcNodeId = srcNodeId;
        nodeMemBorrowInfo.destNodeId = destNodeId;
        nodeMemBorrowInfo.totalSize = borrowMemInfo.size;
        nodeMemBorrowInfo.dstNumaId = dstNumaId;
        nodeMemBorrowInfo.numaNodeBorrowSize[srcNumaId][name] = lentSize;
        nodeMemBorrowInfos.emplace_back(nodeMemBorrowInfo);
        MemMap[borrowId] = nodeMemBorrowInfos;
        return;
    }

    auto it = std::find_if(MemMap[borrowId].begin(), MemMap[borrowId].end(),
                           [srcNodeId, dstNumaId](const NodeMemBorrowInfo& item) {
                               return item.srcNodeId == srcNodeId && item.dstNumaId == dstNumaId;
                           });
    if (it != MemMap[borrowId].end()) {
        it->totalSize += borrowMemInfo.size;
        it->numaNodeBorrowSize[srcNumaId][name] = lentSize;
    } else {
        NodeMemBorrowInfo nodeMemBorrowInfo;
        nodeMemBorrowInfo.srcNodeId = srcNodeId;
        nodeMemBorrowInfo.destNodeId = destNodeId;
        nodeMemBorrowInfo.totalSize = borrowMemInfo.size;
        nodeMemBorrowInfo.dstNumaId = dstNumaId;
        nodeMemBorrowInfo.numaNodeBorrowSize[srcNumaId][name] = lentSize;
        MemMap[borrowId].emplace_back(nodeMemBorrowInfo);
    }
}

uint32_t DataCollect::GeneratelendMemMap()
{
    lendMemMap.clear();
    std::vector<BorrowMemInfo> borrowMemInfos;
    uint32_t ret = Deserialize::GetBorrowMemInfo("", borrowMemInfos);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Get borrow mem info from deserialize failed!";
        return UCACHE_ERR;
    }

    for (const auto& borrowMemInfo : borrowMemInfos) {
        std::string keyId = borrowMemInfo.lentNodeId;
        GenerateLendMap(borrowMemInfo, keyId, lendMemMap);
    }
    return UCACHE_OK;
}

uint32_t DataCollect::GenerateborrowMemMap()
{
    borrowMemMap.clear();
    std::vector<BorrowMemInfo> borrowMemInfos;
    uint32_t ret = Deserialize::GetBorrowMemInfo("", borrowMemInfos);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Get borrow mem info from deserialize failed!";
        return UCACHE_ERR;
    }

    for (const auto& borrowMemInfo : borrowMemInfos) {
        std::string keyId = borrowMemInfo.borrowNodeId;
        GenerateBorrowMap(borrowMemInfo, keyId, borrowMemMap);
    }
    return UCACHE_OK;
}

uint32_t DataCollect::GenerateNodeMemoryInfo()
{
    nodeMemInfos.clear();
    for (const auto& pair1 : nodeInfos) {
        NodeMemoryInfo tmpNodeInfo = {};
        std::string nodeId = pair1.first;
        if (memWaterMarkInfos.find(nodeId) == memWaterMarkInfos.end()) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "nodeId" << nodeId << "not found in memWaterMarkInfos!";
            return UCACHE_ERR;
        }
        tmpNodeInfo.minFreeKbytes = memWaterMarkInfos[nodeId];
        std::map<std::string, NodeInfo> nodeInfo = pair1.second;
        for (const auto& pair2 : nodeInfo) {
            NodeInfo nodeInfo = pair2.second;
            if (!nodeInfo.isRemote) {
                tmpNodeInfo.totalMemory += nodeInfo.memTotalBytes;
                tmpNodeInfo.freeMemory += nodeInfo.memFreeBytes;
                tmpNodeInfo.usedMemory += nodeInfo.memUsedBytes;
                tmpNodeInfo.pageCacheMemory += (nodeInfo.activeFileBytes + nodeInfo.inactiveFileBytes);
            }
        }
        nodeMemInfos[nodeId] = tmpNodeInfo;
    }
    return UCACHE_OK;
}

void SetCgroupInfoDefault(CgroupInfo& cgInfo)
{
    cgInfo.ioReadBandwidth = 0;
    cgInfo.pageCacheIn = 0;
}

CgroupInfo CalculateSingleCgroupRate(const CgroupInfos& curr, const CgroupInfos* prev, uint64_t interval)
{
    CgroupInfo cgInfo{};
    if (!prev || interval == 0) {
        return cgInfo;
    }
    if (curr.pageCacheInfo.pageCacheIn >= prev->pageCacheInfo.pageCacheIn) {
        cgInfo.pageCacheIn = (curr.pageCacheInfo.pageCacheIn - prev->pageCacheInfo.pageCacheIn) * PAGE_SIZE / interval;
    }
    if (curr.ioInfo.ioReadBytes >= prev->ioInfo.ioReadBytes) {
        cgInfo.ioReadBandwidth = (curr.ioInfo.ioReadBytes - prev->ioInfo.ioReadBytes) / (KB_TO_BYTE * interval);
    }
    return cgInfo;
}

uint32_t DataCollect::GenerateCgroupInfo()
{
    if (cgroupInfosQueue.empty()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "cgroupInfosQueue is empty!";
        return UCACHE_ERR;
    }

    const NodeRawMap& latestNodes = cgroupInfosQueue.back();
    const NodeRawMap* oldNodes = (cgroupInfosQueue.size() > 1) ? &cgroupInfosQueue.front() : nullptr;

    for (const auto& nodePair : latestNodes) {
        std::string nodeId = nodePair.first;
        const DockerRawMap& latestDockers = nodePair.second;

        DockerResultMap resultDockerMap;

        const DockerRawMap* oldDockers = (oldNodes && oldNodes->count(nodeId)) ? &oldNodes->at(nodeId) : nullptr;

        uint64_t interval = 0;
        if (oldDockers) {
            uint64_t tOld = timeStampsCgroup.front().count(nodeId) ? timeStampsCgroup.front().at(nodeId) : 0;
            uint64_t tNew = timeStampsCgroup.back().count(nodeId) ? timeStampsCgroup.back().at(nodeId) : 0;
            if (tNew > tOld) {
                interval = tNew - tOld;
            }
        }

        for (const auto& dockerPair : latestDockers) {
            std::string dockerId = dockerPair.first;
            const CgroupInfos& currData = dockerPair.second;
            const CgroupInfos* prevData = (oldDockers && oldDockers->count(dockerId)) ? &oldDockers->at(dockerId) :
                                                                                        nullptr;
            resultDockerMap[dockerId] = CalculateSingleCgroupRate(currData, prevData, interval);
        }
        cgroupInfo[nodeId] = resultDockerMap;
    }
    return UCACHE_OK;
}

uint32_t DataCollect::GenerateBorrow()
{
    uint32_t ret;
    struct Task tasks[] = {
        {GenerateBorrowStrategyRawData, "Set borrow strategy raw data failed!"},
        {GenerateLoanableTotalBorrowMemMap, "Set loanable total borrow mem map failed!"},
        {GenerateNumaSocketMap, "Set numa socket map failed!"},
        {GeneratelendMemMap, "Set lend mem map failed!"},
        {GenerateborrowMemMap, "Set borrow mem map failed!"},
    };

    for (uint32_t i = 0; i < sizeof(tasks) / sizeof(tasks[0]); i++) {
        ret = CheckAndRun(tasks[i].func, tasks[i].errMsg);
        if (ret != UCACHE_OK) {
            return ret;
        }
    }

    return UCACHE_OK;
}

uint32_t DataCollect::GetResourceExportData()
{
    uint32_t ret = Deserialize::GetCgroupInfos(timeStampsCgroup, cgroupInfosQueue);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Deserialize cgroup infos failed!";
        return ret;
    }

    ret = Deserialize::GetNumaInfos(nodeInfos);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Deserialize numa infos failed!";
        return ret;
    }

    ret = Deserialize::GetMemWaterMark(memWaterMarkInfos);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Deserialize mem wartermark failed!";
        return ret;
    }

    return UCACHE_OK;
}

uint32_t DataCollect::CollectData()
{
    uint32_t ret;
    struct Task tasks[] = {
        {GeneratePhysicalTopo, "Set physical topo failed!"},
        {GetResourceExportData, "Get resource export data failed!"},
        {GenerateBorrow, "Set borrow data failed!"},
        {GenerateNodeMemoryInfo, "Set node memory info failed!"},
        {GenerateCgroupInfo, "Set cgroup info failed!"},
    };

    for (uint32_t i = 0; i < sizeof(tasks) / sizeof(tasks[0]); i++) {
        ret = CheckAndRun(tasks[i].func, tasks[i].errMsg);
        if (ret != UCACHE_OK) {
            return ret;
        }
    }

    return UCACHE_OK;
}

std::string DataCollect::PrintCgroupInfo(const std::map<std::string, std::map<std::string, CgroupInfo>>& cgroupInfo)
{
    std::ostringstream oss;
    oss << "{\n";
    for (const auto& outerPair : cgroupInfo) {
        const std::string& rackNodeId = outerPair.first;
        oss << "  rack_node_id: \"" << rackNodeId << "\",\n";

        for (const auto& innerPair : outerPair.second) {
            const std::string& dockerId = innerPair.first;
            const auto& info = innerPair.second;
            oss << "    docker_id: \"" << dockerId << "\",\n";
            oss << "    {\n";
            oss << "      pageCacheIn: " << info.pageCacheIn << ",\n";
            oss << "      ioReadBandwidth: " << info.ioReadBandwidth << "\n";
            oss << "    },\n";
        }
        oss << "  },\n";
    }
    oss << "}\n";
    return oss.str();
}

void DataCollect::GetBorrowStrategyRawData(std::vector<BorrowStrategyRawData>& rawData)
{
    rawData = borrowStrategyRawData;
}

void DataCollect::GetLoanableTotalBorrowMemMap(std::map<std::string, std::map<int, uint64_t>>& rawMap)
{
    rawMap = loanableTotalBorrowMemMap;
}

void DataCollect::GetPhysicalTopo(std::map<std::string, std::vector<std::string>>& topo)
{
    topo = physicalTopo;
}

void DataCollect::GetNumaSocketMap(std::map<std::string, std::map<int, int>>& socketMap)
{
    socketMap = numaSocketMap;
}

void DataCollect::GetlendMemMap(std::map<std::string, std::vector<NodeMemBorrowInfo>>& lendMap)
{
    lendMap = lendMemMap;
}

void DataCollect::GetborrowMemMap(std::map<std::string, std::vector<NodeMemBorrowInfo>>& borrowMap)
{
    borrowMap = borrowMemMap;
}

void DataCollect::GetNodeMemoryInfo(std::map<std::string, NodeMemoryInfo>& nodes)
{
    nodes = nodeMemInfos;
}

void DataCollect::GetCgroupInfo(std::map<std::string, std::map<std::string, CgroupInfo>>& cgInfos)
{
    cgInfos = cgroupInfo;
}

void DataCollect::SetBorrowStrategyRawData(std::vector<BorrowStrategyRawData>& rawData)
{
    borrowStrategyRawData = rawData;
}

void DataCollect::SetLoanableTotalBorrowMemMap(std::map<std::string, std::map<int, uint64_t>>& rawMap)
{
    loanableTotalBorrowMemMap = rawMap;
}

void DataCollect::SetPhysicalTopo(std::map<std::string, std::vector<std::string>>& topo)
{
    physicalTopo = topo;
}

void DataCollect::SetNumaSocketMap(std::map<std::string, std::map<int, int>>& socketMap)
{
    numaSocketMap = socketMap;
}

void DataCollect::SetlendMemMap(std::map<std::string, std::vector<NodeMemBorrowInfo>>& lendMap)
{
    lendMemMap = lendMap;
}

void DataCollect::SetborrowMemMap(std::map<std::string, std::vector<NodeMemBorrowInfo>>& borrowMap)
{
    borrowMemMap = borrowMap;
}

void DataCollect::SetNodeMemoryInfo(std::map<std::string, NodeMemoryInfo>& nodes)
{
    nodeMemInfos = nodes;
}

void DataCollect::SetCgroupInfo(std::map<std::string, std::map<std::string, CgroupInfo>>& cgInfos)
{
    cgroupInfo = cgInfos;
}
} // end namespace data_collect
} // end namespace ucache
