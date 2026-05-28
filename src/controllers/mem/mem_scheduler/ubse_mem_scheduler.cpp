/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#include "ubse_mem_scheduler.h"

#include <shared_mutex>
#include <string>

#include "src/controllers/mem/mem_controller/ubse_mem_controller_pre_online.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mem_account_helper.h"
#include "ubse_mem_algo_account.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_meta_data.h"
#include "ubse_mem_strategy_helper.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_mem_validator.h"
#include "ubse_mmi_interface.h"
#include "ubse_node.h"
#include "ubse_node_controller.h"

namespace ubse::mem::scheduler {
std::mutex schedulerMutex;
using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::mem::strategy;
using namespace ubse::nodeController;
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
uint32_t Init()
{
    UbseClusterStateNotifyHandler handler = UbseMemNodeObjChangeHandler;
    auto ret = UbseNodeController::GetInstance().RegClusterStateNotifyHandler(handler);
    if (ret != UBSE_OK) {
        return UBSE_ERROR;
    }
    UbseMemConfiguration::GetInstance().Init();
    UBSE_LOG_INFO << "Mem-scheduler inits successfully";
    return UBSE_OK;
}

namespace {
void GetNodeDataFromNodeInfo(const ubse::nodeController::UbseNodeInfo &nodeInfo, strategy::NodeData &nodeData)
{
    nodeData.nodeId = nodeInfo.nodeId;
    std::vector<strategy::SocketData> &socketDatas = nodeData.sockets;
    for (const auto &[numaLoc, numaInfo] : nodeInfo.numaInfos) {
        strategy::NumaData data{numaLoc.nodeId, static_cast<int>(numaInfo.socketId), numaLoc.numaId};
        auto it = std::find_if(socketDatas.begin(), socketDatas.end(), [data](strategy::SocketData &socketData) {
            return socketData.socketId == data.socketId;
        });
        if (it != socketDatas.end()) {
            it->numas.push_back(data);
        } else {
            strategy::SocketData newSocketData{data.socketId, data.nodeId};
            newSocketData.numas.push_back(data);
            socketDatas.push_back(newSocketData);
        }
    }
}

void UpdateNumaInfo(const std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> &nodeMap)
{
    std::vector<strategy::NodeDataWithNumaInfo> nodeDatas{};
    for (const auto &[nodeId, mapNodeInfo] : nodeMap) {
        strategy::NodeDataWithNumaInfo nodeDataWithNumaInfo{};
        GetNodeDataFromNodeInfo(mapNodeInfo, nodeDataWithNumaInfo.nodeData);
        for (const auto &[loc, numaInfo] : mapNodeInfo.numaInfos) {
            nodeDataWithNumaInfo.numaInfo.push_back(numaInfo);
        }
        nodeDatas.push_back(nodeDataWithNumaInfo);
    }
    UbseMemTopologyInfoManager::GetInstance().UpdateNodeMesgInfo(nodeDatas);
}

bool UpdateCacheNodeState(const std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> &nodeMap)
{
    auto flag = false;
    for (const auto &[nodeId, nodeInfo] : nodeMap) {
        if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
            flag = true;
        }
        bool status = (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_WORKING &&
                       nodeInfo.globalState == UbseNodeGlobalState::UBSE_NODE_GLOBAL_READY);
        UbseMemTopologyInfoManager::GetInstance().ChangeNodeStatus(nodeId, status);
        UBSE_LOG_INFO << "when allocate begin, node id=" << nodeInfo.nodeId
                      << ", clusterState=" << static_cast<uint32_t>(nodeInfo.clusterState)
                      << ", globalState=" << static_cast<uint8_t>(nodeInfo.globalState);
    }
    return flag;
}

void FillterNodeOnline()
{
    auto nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    for (const auto &[nodeId, nodeInfo] : nodeInfos) {
        if (!ubse::mem::controller::IsNodeOnLine(nodeId)) {
            UBSE_LOG_WARN << "enable pre Online, but nodeId=" << nodeId << " is not online";
            UbseMemTopologyInfoManager::GetInstance().ChangeNodeStatus(nodeId, false);
        }
    }
}

const size_t MAX_HIGH_WATERMARK = 100;
uint32_t CheckNumaReqParam(const UbseMemNumaBorrowReq &numaReq)
{
    if (numaReq.highWatermark < 0 || numaReq.highWatermark > MAX_HIGH_WATERMARK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

bool CheckNodeLink(uint32_t node1, uint32_t node2,
                   const std::unordered_map<std::uint32_t, std::unordered_set<std::uint32_t>> &nodeConnectInfo)
{
    if ((nodeConnectInfo.find(node1) != nodeConnectInfo.end()) && (nodeConnectInfo.at(node1).count(node2) != 0)) {
        return true;
    }
    if ((nodeConnectInfo.find(node2) != nodeConnectInfo.end()) && (nodeConnectInfo.at(node2).count(node1) != 0)) {
        return true;
    }
    UBSE_LOG_ERROR << "nodeId:" << node1 << " is not connect with "
                   << "nodeId:" << node2;
    return false;
}

uint32_t CheckRegionIsFullMesh(
    const std::vector<ubse::adapter_plugins::mmi::UbseNodeInfo> &nodelist,
    const std::unordered_map<std::uint32_t, std::unordered_set<std::uint32_t>> &nodeConnectInfo)
{
    for (size_t i = 0; i < nodelist.size(); ++i) {
        uint32_t preNode{};
        auto res = utils::ConvertStrToUint32(nodelist[i].nodeId, preNode);
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "ConvertStrToUint32 failed, nodeId is " << nodelist[i].nodeId;
            return res;
        }

        for (size_t j = 0; j < nodelist.size(); ++j) {
            uint32_t sufNode{};
            auto res = utils::ConvertStrToUint32(nodelist[j].nodeId, sufNode);
            if (res != UBSE_OK) {
                UBSE_LOG_ERROR << "ConvertStrToUint32 failed, nodeId is " << nodelist[j].nodeId;
                return res;
            }
            if (j == i) {
                continue;
            }
            if (!CheckNodeLink(preNode, sufNode, nodeConnectInfo)) {
                UbseMemTopologyInfoManager::GetInstance().ChangeNodeStatus(std::to_string(preNode), false);
            }
        }
    }

    return UBSE_OK;
}

uint32_t CheckShareReq(const UbseMemShareBorrowReq &borrowReq)
{
    // 判断共享域内的节点都是一跳直连的
    auto linkInfos = nodeController::UbseNodeController::GetInstance().UbseGetDirConnectInfo();
    std::unordered_map<std::uint32_t, std::unordered_set<std::uint32_t>> nodeConnectInfo{};
    for (const auto &[key, linkInfo] : linkInfos) {
        nodeConnectInfo[linkInfo.slotId].insert(linkInfo.peerSlotId);
    }

    return CheckRegionIsFullMesh(borrowReq.shmRegion.nodelist, nodeConnectInfo);
}

uint64_t GetCheckMaskCodeByFdReq(const UbseMemFdBorrowReq &fdReq)
{
    uint64_t checkMaskCode = CHECK_MEMORY_CONFIG_VALID | CHECK_BORROW_SIZE_MEET_LIMIT | FILTER_LEND_NODE_HAS_BORROWED |
                             CHECK_BORROW_NODE_HAS_LENT | FILTER_NODE_IS_LENDER | FILTER_NODE_BY_GROUP |
                             FILTER_NODE_IS_DOWN | FILTER_LEND_TIME_OUT | FILTER_LINK_PORT_DOWN;
    if (!fdReq.candidateNodeList.empty()) {
        checkMaskCode |= FILTER_CANDIDATE_NODE_LIST;
    }

    return checkMaskCode;
}

uint64_t GetCheckMaskCodeByNumaReq(const UbseMemNumaBorrowReq &numaReq)
{
    uint64_t checkMaskCode = CHECK_MEMORY_CONFIG_VALID | CHECK_BORROW_SIZE_MEET_LIMIT | FILTER_LEND_NODE_HAS_BORROWED |
                             CHECK_BORROW_NODE_HAS_LENT | FILTER_NODE_IS_LENDER | FILTER_NODE_BY_GROUP |
                             FILTER_NODE_IS_DOWN | FILTER_LEND_TIME_OUT | FILTER_LINK_PORT_DOWN;

    if (numaReq.srcSocket != -1) {
        checkMaskCode |= FILTER_NUMA_BY_SAME_PLANE;
    }

    if (!numaReq.candidateNodeList.empty()) {
        checkMaskCode |= FILTER_CANDIDATE_NODE_LIST;
    }

    if (numaReq.linkInfo.lenderSocketId != -1) {
        checkMaskCode |= FILTER_NUMA_BY_LEND_SOCKET;
    }

    return checkMaskCode;
}

template <class ReqType>
uint64_t GetCheckMaskCodeByUserReq(const ReqType &userReq)
{
    uint64_t checkMaskCode = CHECK_MEMORY_CONFIG_VALID | CHECK_BORROW_SIZE_MEET_LIMIT | CHECK_BORROW_NODE_HAS_LENT |
                             CHECK_LEND_NODE_HAS_BORROWED | CHECK_LEND_NODE_IS_IN_GROUP | CHECK_LEND_NODE_IS_LENDER |
                             CHECK_NODE_IS_DOWN | CHECK_LEND_NUMA_IS_ENOUGH;

    if (!userReq.candidateNodeList.empty()) {
        checkMaskCode |= CHECK_LEND_NODE_IS_IN_CANDIDATELIST;
    }
    return checkMaskCode;
}

uint64_t GetCheckMaskCodeByAddrReq(const UbseMemAddrBorrowReq &addrReq)
{
    uint64_t checkMaskCode = CHECK_BORROW_NODE_HAS_LENT | CHECK_LEND_NODE_HAS_BORROWED | CHECK_NODE_IS_DOWN;

    return checkMaskCode;
}

uint64_t GetCheckMaskCodeByShareReq(const UbseMemShareBorrowReq &shareReq)
{
    uint64_t checkMaskCode = CHECK_MEMORY_CONFIG_VALID | FILTER_NODE_IS_LENDER | FILTER_NODE_IS_DOWN |
                             FILTER_LEND_TIME_OUT | FILTER_LINK_PORT_DOWN;
    if (!shareReq.providerList.empty()) {
        checkMaskCode |= FILTER_SHARE_NODE_LIST;
    }
    if (shareReq.withAffinity.enableCreateWithAffinity) {
        checkMaskCode |= FILTER_SHARE_BY_SAME_PLANE;
    }
    if (shareReq.lenderInfo.lender_size != 0) {
        checkMaskCode |= FILTER_SHARE_BY_LENDER;
    }
    return checkMaskCode;
}

uint64_t GetShareCheckMaskCodeByUserReq(const UbseMemShareBorrowReq &shareReq)
{
    uint64_t checkMaskCode = CHECK_MEMORY_CONFIG_VALID | CHECK_LEND_NODE_IS_LENDER | CHECK_NODE_IS_DOWN |
                             CHECK_LEND_NUMA_IS_ENOUGH;
    if (!shareReq.providerList.empty()) {
        checkMaskCode |= CHECK_LEND_NODE_IS_IN_CANDIDATELIST;
    }
    return checkMaskCode;
}
} // namespace

uint32_t UbseMemNodeObjChangeHandler(const ubse::nodeController::UbseNodeInfo &nodeInfo)
{
    UBSE_LOG_INFO << "Node state change, NodeId=" << nodeInfo.nodeId
                  << ", State=" << static_cast<int>(nodeInfo.clusterState);
    std::unique_lock<std::mutex> guard(schedulerMutex);
    auto nodeMap = UbseNodeController::GetInstance().GetAllNodes();
    auto &config = UbseMemConfiguration::GetInstance();
    config.SetConfig(nodeMap);
    if (nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_INIT &&
        nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
        return UBSE_OK;
    }
    if (!UbseMemTopologyInfoManager::GetInstance().CheckIsNewNode(nodeInfo.nodeId)) {
        return UBSE_OK;
    }

    UBSE_LOG_INFO << "Node working event detected, NodeId=" << nodeInfo.nodeId;
    std::vector<strategy::NodeDataWithNumaInfo> nodeDatas{};
    for (const auto &[nodeId, mapNodeInfo] : nodeMap) {
        if (mapNodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
            UBSE_LOG_WARN << "nodeId=" << mapNodeInfo.nodeId << " state is fault";
            continue;
        }
        strategy::NodeDataWithNumaInfo nodeDataWithNumaInfo{};
        GetNodeDataFromNodeInfo(mapNodeInfo, nodeDataWithNumaInfo.nodeData);
        for (const auto &[loc, numaInfo] : mapNodeInfo.numaInfos) {
            nodeDataWithNumaInfo.numaInfo.push_back(numaInfo);
        }
        nodeDatas.push_back(nodeDataWithNumaInfo);
    }

    return strategy::UbseMemTopologyInfoManager::GetInstance().NodesInit(nodeDatas);
}

uint32_t UbseMemFdImportObjStateChangeHandler(UbseMemFdBorrowImportObj &importObj)
{
    if (importObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_STATE_FAILED) {
        importObj.status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    }
    std::unique_lock<std::mutex> guard(schedulerMutex);
    if (importObj.status.state == UBSE_MEM_SCHEDULING) {
        UBSE_LOG_INFO << "Name=" << importObj.req.name << ", Fd req node=" << importObj.req.importNodeId
                      << ", Fd req size=" << importObj.req.size;
        auto nodeMap = UbseNodeController::GetInstance().GetAllNodes();
        UpdateNumaInfo(nodeMap);
        auto isNeedRetry = UpdateCacheNodeState(nodeMap);
        UbseResult ret = UBSE_OK;
        if (!importObj.req.lenderLocs.empty()) {
            auto checkMaskCode = GetCheckMaskCodeByUserReq(importObj.req);
            ret = strategy::UbseMemStrategyHelper::GetInstance().MemoryBorrowAccordingToUserRequest(
                importObj.req, importObj.algoResult, checkMaskCode);
            if (ret != UBSE_OK) {
                isNeedRetry ? ret = UBSE_SCHEDULER_ERROR_NODE_RECONCILE : ret;
                UBSE_LOG_ERROR << "Name=" << importObj.req.name << " Fd MemoryBorrow failed";
                return ret;
            }
        } else {
            auto checkMaskCode = GetCheckMaskCodeByFdReq(importObj.req);
            ret = strategy::UbseMemStrategyHelper::GetInstance().FdMemoryBorrow(importObj.req, importObj.algoResult,
                                                                                checkMaskCode);
            if (ret != UBSE_OK) {
                isNeedRetry ? ret = UBSE_SCHEDULER_ERROR_NODE_RECONCILE : ret;
                UBSE_LOG_ERROR << "Name=" << importObj.req.name << " Fd MemoryBorrow failed";
                return ret;
            }
        }
    }
    UbseMemAccountHelper::UpdateAlgoAccountState(importObj.req.name, importObj.status.state, importObj.algoResult,
                                                 BorrowedType::FD);
    return UBSE_OK;
}

uint32_t UbseMemFdExportObjStateChangeHandler(UbseMemFdBorrowExportObj &exportObj)
{
    if (exportObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_STATE_FAILED) {
        exportObj.status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    }
    std::unique_lock<std::mutex> guard(schedulerMutex);
    UbseMemAccountHelper::UpdateAlgoAccountState(exportObj.req.name, exportObj.status.state, exportObj.algoResult,
                                                 BorrowedType::FD);
    return UBSE_OK;
}

uint32_t UbseMemNumaImportObjStateChangeHandler(UbseMemNumaBorrowImportObj &importObj)
{
    if (importObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_STATE_FAILED) {
        importObj.status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    }
    std::unique_lock<std::mutex> guard(schedulerMutex);
    if (importObj.status.state == UBSE_MEM_SCHEDULING) {
        UBSE_LOG_INFO << "Name=" << importObj.req.name << ", Numa req node=" << importObj.req.importNodeId
                      << ", Numa req size=" << importObj.req.size;
        auto nodeMap = UbseNodeController::GetInstance().GetAllNodes();
        auto ret = CheckNumaReqParam(importObj.req);
        if (ret != UBSE_OK) {
            return ret;
        }
        UpdateNumaInfo(nodeMap);
        auto isNeedRetry = UpdateCacheNodeState(nodeMap); // 如果存在对账节点数量不为0，且算法决策失败，则会触发重试
        if (ubse::mem::controller::IsPreOnLineEnable()) {
            if (!ubse::mem::controller::IsNodeOnLine(importObj.req.importNodeId)) {
                UBSE_LOG_ERROR << "enable pre online, but nodeId=" << importObj.req.importNodeId << " not online";
                return UBSE_ERROR;
            }
        }

        if (!importObj.req.lenderLocs.empty()) {
            auto checkMaskCode = GetCheckMaskCodeByUserReq(importObj.req);
            ret = strategy::UbseMemStrategyHelper::GetInstance().MemoryBorrowAccordingToUserRequest(
                importObj.req, importObj.algoResult, checkMaskCode, importObj.req.srcSocket, importObj.req.srcNuma);
            if (ret != UBSE_OK) {
                isNeedRetry ? ret = UBSE_SCHEDULER_ERROR_NODE_RECONCILE : ret;
                UBSE_LOG_ERROR << "Name=" << importObj.req.name << " Numa MemoryBorrow failed";
                return ret;
            }
        } else {
            auto checkMaskCode = GetCheckMaskCodeByNumaReq(importObj.req);
            ret = strategy::UbseMemStrategyHelper::GetInstance().NumaMemoryBorrow(importObj.req, importObj.algoResult,
                                                                                  checkMaskCode);
            if (ret != UBSE_OK) {
                isNeedRetry ? ret = UBSE_SCHEDULER_ERROR_NODE_RECONCILE : ret;
                UBSE_LOG_ERROR << "Name=" << importObj.req.name << " Numa MemoryBorrow failed";
                return ret;
            }
        }
    }
    UbseMemAccountHelper::UpdateAlgoAccountState(importObj.req.name, importObj.status.state, importObj.algoResult,
                                                 BorrowedType::NUMA);
    return UBSE_OK;
}

uint32_t UbseMemNumaExportObjStateChangeHandler(UbseMemNumaBorrowExportObj &exportObj)
{
    if (exportObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_STATE_FAILED) {
        exportObj.status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    }
    std::unique_lock<std::mutex> guard(schedulerMutex);
    UbseMemAccountHelper::UpdateAlgoAccountState(exportObj.req.name, exportObj.status.state, exportObj.algoResult,
                                                 BorrowedType::NUMA);
    return UBSE_OK;
}

uint32_t UbseMemShmImportObjStateChangeHandler(UbseMemShareBorrowImportObj &importObj)
{
    if (importObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_STATE_FAILED) {
        importObj.status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    }
    std::unique_lock<std::mutex> guard(schedulerMutex);
    UbseMemAccountHelper::UpdateAlgoAccountState(importObj.req.name, importObj.status.state, importObj.algoResult,
                                                 BorrowedType::SHM);
    return UBSE_OK;
}

uint32_t UbseMemShmExportObjStateChangeHandler(UbseMemShareBorrowExportObj &exportObj)
{
    if (exportObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_STATE_FAILED) {
        exportObj.status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    }
    std::unique_lock<std::mutex> guard(schedulerMutex);
    if (exportObj.status.state == UBSE_MEM_SCHEDULING) {
        UBSE_LOG_INFO << "Name=" << exportObj.req.name << ", Shm Req size=" << exportObj.req.size;
        auto nodeMap = UbseNodeController::GetInstance().GetAllNodes();

        UpdateNumaInfo(nodeMap);
        auto isNeedRetry = UpdateCacheNodeState(nodeMap);
        UbseResult ret = UBSE_OK;
        ret = CheckShareReq(exportObj.req);
        if (ret != UBSE_OK) {
            return ret;
        }
        auto checkMaskCode = GetCheckMaskCodeByShareReq(exportObj.req);
        ret = strategy::UbseMemStrategyHelper::GetInstance().ShareMemoryBorrow(exportObj.req, exportObj.algoResult,
                                                                               checkMaskCode);
        if (ret != UBSE_OK) {
            isNeedRetry ? ret = UBSE_SCHEDULER_ERROR_NODE_RECONCILE : ret;
            UBSE_LOG_ERROR << "Name=" << exportObj.req.name << " Shm MemoryBorrow failed";
            return ret;
        }
    }
    UbseMemAccountHelper::UpdateAlgoAccountState(exportObj.req.name, exportObj.status.state, exportObj.algoResult,
                                                 BorrowedType::SHM);
    return UBSE_OK;
}

uint32_t UbseMemAddrImportObjStateChangeHandler(UbseMemAddrBorrowImportObj &importObj)
{
    if (importObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_STATE_FAILED) {
        importObj.status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    }
    std::unique_lock<std::mutex> guard(schedulerMutex);
    if (importObj.status.state == UBSE_MEM_SCHEDULING) {
        auto ret = UBSE_OK;
        auto nodeMap = UbseNodeController::GetInstance().GetAllNodes();
        UpdateNumaInfo(nodeMap);
        UpdateCacheNodeState(nodeMap);
        ret = strategy::UbseMemStrategyHelper::GetInstance().AddrMemoryBorrow(importObj.req, importObj.algoResult,
                                                                              GetCheckMaskCodeByAddrReq(importObj.req));
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Name=" << importObj.req.name << " Addr MemoryBorrow failed";
            return ret;
        }
    }
    UbseMemAccountHelper::UpdateAlgoAccountState(importObj.req.name, importObj.status.state, importObj.algoResult,
                                                 BorrowedType::ADDR);
    return UBSE_OK;
}

uint32_t UbseMemAddrExportObjStateChangeHandler(UbseMemAddrBorrowExportObj &exportObj)
{
    if (exportObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_STATE_FAILED) {
        exportObj.status.state = ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_DESTROYED;
    }
    std::unique_lock<std::mutex> guard(schedulerMutex);
    UbseMemAccountHelper::UpdateAlgoAccountState(exportObj.req.name, exportObj.status.state, exportObj.algoResult,
                                                 BorrowedType::ADDR);
    return UBSE_OK;
}

void ClearCacheValue()
{
    std::unique_lock<std::mutex> guard(schedulerMutex);
    UbseMemTopologyInfoManager::GetInstance().Clear();
    UbseMemStrategyHelper::GetInstance().Clear();
    AlgoAccountManger::GetInstance().Clear();
}
} // namespace ubse::mem::scheduler