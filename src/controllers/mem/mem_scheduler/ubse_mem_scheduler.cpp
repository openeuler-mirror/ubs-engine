/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#include "ubse_mem_scheduler.h"

#include <shared_mutex>
#include <string>

#include "ubse_mgr_configuration.h"
#include "ubse_mem_account_helper.h"
#include "ubse_mem_algo_account.h"
#include "ubse_mem_meta_data.h"
#include "ubse_mem_strategy_helper.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mem_obj.h"
#include "ubse_node_topology.h"

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)
using namespace ubse::log;

namespace ubse::mem::scheduler {
std::shared_mutex nodeStatusMtx;
std::mutex schedulerMutex;

uint32_t Init()
{
    strategy::MemMgrConfiguration::GetInstance().Init();
    UbseClusterStateNotifyHandler handler = UbseMemNodeObjChangeHandler;
    auto ret = UbseNodeController::GetInstance().RegClusterStateNotifyHandler(handler);
    if (ret != UBSE_OK) {
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "Mem-scheduler inits successfully";
    return UBSE_OK;
}

static void GetNodeDataFromNodeInfo(const ubse::nodeController::UbseNodeInfo &nodeInfo, strategy::NodeData &nodeData)
{
    nodeData.nodeId = nodeInfo.nodeId;
    std::vector<strategy::SocketData> &socketDatas = nodeData.sockets;
    for (auto [numaLoc, numaInfo] : nodeInfo.numaInfos) {
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

uint32_t UbseMemNodeObjChangeHandler(const ubse::nodeController::UbseNodeInfo &nodeInfo)
{
    if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_WORKING) {
        strategy::NodeData nodeData{};
        GetNodeDataFromNodeInfo(nodeInfo, nodeData);
        UBSE_LOG_DEBUG << "UBSE_NODE_WORKING Happend, nodeId is" << nodeInfo.nodeId;
        auto nodeMap = UbseNodeController::GetInstance().GetAllNodes();
        std::vector<strategy::NodeDataWithNumaInfo> nodeDatas{};
        for (const auto &[nodeId, nodeInfo] : nodeMap) {
            if (nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_WORKING) {
                continue;
            }
            strategy::NodeDataWithNumaInfo nodeDataWithNumaInfo{};
            GetNodeDataFromNodeInfo(nodeInfo, nodeDataWithNumaInfo.nodeData);
            for (const auto &[loc, numaInfo] : nodeInfo.numaInfos) {
                nodeDataWithNumaInfo.numaInfo.push_back(numaInfo);
            }
            nodeDatas.push_back(nodeDataWithNumaInfo);
        }
        strategy::UbseMemTopologyInfoManager::GetInstance().NodesInit(nodeDatas);
        return UBSE_OK;
    }
    if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT ||
        nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_UNKNOWN ||
        nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
        UBSE_LOG_DEBUG << "UBSE_NODE_change Happend, state is " << static_cast<int>(nodeInfo.clusterState)
                    << "nodeId is" << nodeInfo.nodeId;
        auto nodeMap = UbseNodeController::GetInstance().GetAllNodes();
        std::vector<strategy::NodeDataWithNumaInfo> nodeDatas{};
        for (const auto &[nodeId, nodeInfo] : nodeMap) {
            if (nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_WORKING) {
                continue;
            }
            strategy::NodeDataWithNumaInfo nodeDataWithNumaInfo{};
            GetNodeDataFromNodeInfo(nodeInfo, nodeDataWithNumaInfo.nodeData);
            for (const auto &[loc, numaInfo] : nodeInfo.numaInfos) {
                nodeDataWithNumaInfo.numaInfo.push_back(numaInfo);
            }
            nodeDatas.push_back(nodeDataWithNumaInfo);
        }
        strategy::UbseMemTopologyInfoManager::GetInstance().NodesInit(nodeDatas);
    }
    return UBSE_OK;
}

uint32_t GetAttachNodeId(std::string &borrowNodeId, std::string &exportNodeId, int exportSocketId,
                         uint32_t &attachSocketId)
{
    UbseNodeMemCnaInfoInput input;
    input.borrowNodeId = borrowNodeId;
    input.exportNodeId = exportNodeId;
    input.exportSocketId = std::to_string(exportSocketId);
    UbseNodeMemCnaInfoOutput output;
    auto ret = UbseNodeMemGetTopologyCnaInfo(input, output);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Can not get cna info. BorrowNodeId: " << borrowNodeId << ", exportNodeId: " << exportNodeId
                    << ", exportSocketId:" << exportSocketId;
        return ret;
    }
    attachSocketId = static_cast<int>(atoi(output.borrowSocketId.c_str()));
    return UBSE_OK;
}

uint32_t UbseMemFdImportObjStateChangeHandler(UbseMemFdBorrowImportObj &importObj)
{
    std::unique_lock<std::mutex> guard(schedulerMutex);
    if (importObj.status.state == UBSE_MEM_SCHEDULING) {
        UBSE_LOG_INFO << "Name: " << importObj.req.name << "FD Req size: " << importObj.req.size;
        auto ret = strategy::UbseMemStrategyHelper::GetInstance().FdMemoryBorrow(importObj.req, importObj.algoResult);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Name: " << importObj.req.name << "FdMemoryBorrow failed";
            return ret;
        }
        ret = GetAttachNodeId(importObj.req.importNodeId, importObj.algoResult.exportNumaInfos[0].nodeId,
                              importObj.algoResult.exportNumaInfos[0].socketId, importObj.algoResult.attachSocketId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Name: " << importObj.req.name << "GetAttachNodeId failed";
            return ret;
        }
        try {
            std::shared_ptr<strategy::AlgoAccount> algoAccountPtr = std::make_shared<strategy::AlgoAccount>(
                importObj.req.name, importObj.algoResult.importNumaInfos, importObj.algoResult.exportNumaInfos,
                importObj.algoResult.attachSocketId, importObj.algoResult.exportNumaInfos[0].socketId,
                importObj.req.size);
            strategy::AlgoAccountManger::GetInstance().AddAlgoAccount(algoAccountPtr);
        } catch (...) {
            UBSE_LOG_ERROR << "Name: " << importObj.req.name << "make_shared failed";
            return UBSE_ERROR;
        }
        return ret;
    }

    if (importObj.status.state == UBSE_MEM_IMPORT_SUCCESS) {
        UBSE_LOG_DEBUG << "Fd BorrowSuccess, Begin change account";
        strategy::UbseMemAccountHelper::GetInstance().BorrowSuccess(importObj.req.name);
        return UBSE_OK;
    }

    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        auto algoAccountPtr = strategy::AlgoAccountManger::GetInstance().GetAlgoAccount(importObj.req.name);
        if (algoAccountPtr == nullptr) {
            UBSE_LOG_INFO << "algo Account has been deleted, name is " << importObj.req.name;
        }
        strategy::UbseMemAccountHelper::GetInstance().BorrowReturnSuccess(algoAccountPtr);
        strategy::AlgoAccountManger::GetInstance().DelAlgoAccount(importObj.req.name);
        return UBSE_OK;
    }

    // 回滚预占
    if (importObj.status.state == UBSE_MEM_STATE_FAILED) {
        strategy::UbseMemAccountHelper::GetInstance().BorrowFailed(importObj.req.name);
        return UBSE_OK;
    }
    return UBSE_OK;
}

uint32_t UbseMemFdExportObjStateChangeHandler(UbseMemFdBorrowExportObj &exportObj)
{
    std::unique_lock<std::mutex> guard(schedulerMutex);
    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        auto algoAccountPtr = strategy::AlgoAccountManger::GetInstance().GetAlgoAccount(exportObj.req.name);
        if (algoAccountPtr == nullptr) {
            UBSE_LOG_INFO << "algo Account has been deleted, name is " << exportObj.req.name;
        }
        strategy::UbseMemAccountHelper::GetInstance().BorrowReturnSuccess(algoAccountPtr);
        strategy::AlgoAccountManger::GetInstance().DelAlgoAccount(exportObj.req.name);
        return UBSE_OK;
    }
    if (exportObj.status.state == obj::UBSE_MEM_STATE_FAILED) {
        strategy::UbseMemAccountHelper::GetInstance().BorrowFailed(exportObj.req.name);
        return UBSE_OK;
    }
    return UBSE_OK;
}

uint32_t UbseMemNumaImportObjStateChangeHandler(UbseMemNumaBorrowImportObj &importObj)
{
    std::unique_lock<std::mutex> guard(schedulerMutex);
    if (importObj.status.state == UBSE_MEM_SCHEDULING) {
        UBSE_LOG_INFO << "Name: " << importObj.req.name << "NUMA Req size: " << importObj.req.size;
        auto ret = strategy::UbseMemStrategyHelper::GetInstance().NumaMemoryBorrow(importObj.req, importObj.algoResult);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Name: " << importObj.req.name << "NUMAMemoryBorrow failed";
            return ret;
        }
        ret = GetAttachNodeId(importObj.req.importNodeId, importObj.algoResult.exportNumaInfos[0].nodeId,
                              importObj.algoResult.exportNumaInfos[0].socketId, importObj.algoResult.attachSocketId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Name: " << importObj.req.name << "GetAttachNodeId failed";
            return ret;
        }
        try {
            std::shared_ptr<strategy::AlgoAccount> algoAccountPtr = std::make_shared<strategy::AlgoAccount>(
                importObj.req.name, importObj.algoResult.importNumaInfos, importObj.algoResult.exportNumaInfos,
                importObj.algoResult.attachSocketId, importObj.algoResult.exportNumaInfos[0].socketId,
                importObj.req.size);
            strategy::AlgoAccountManger::GetInstance().AddAlgoAccount(algoAccountPtr);
        } catch (...) {
            UBSE_LOG_ERROR << "Name: " << importObj.req.name << "make_shared failed";
            return UBSE_ERROR;
        }
        return ret;
    }

    if (importObj.status.state == UBSE_MEM_IMPORT_SUCCESS) {
        UBSE_LOG_DEBUG << "NUMA BorrowSuccess, Begin change account";
        strategy::UbseMemAccountHelper::GetInstance().BorrowSuccess(importObj.req.name);
        return UBSE_OK;
    }

    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        auto algoAccountPtr = strategy::AlgoAccountManger::GetInstance().GetAlgoAccount(importObj.req.name);
        if (algoAccountPtr == nullptr) {
            UBSE_LOG_INFO << "algo Account has been deleted, name is " << importObj.req.name;
        }
        strategy::UbseMemAccountHelper::GetInstance().BorrowReturnSuccess(algoAccountPtr);
        strategy::AlgoAccountManger::GetInstance().DelAlgoAccount(importObj.req.name);
        return UBSE_OK;
    }

    // 回滚预占
    if (importObj.status.state == UBSE_MEM_STATE_FAILED) {
        strategy::UbseMemAccountHelper::GetInstance().BorrowFailed(importObj.req.name);
        return UBSE_OK;
    }

    return UBSE_OK;
}

uint32_t UbseMemNumaExportObjStateChangeHandler(UbseMemNumaBorrowExportObj &exportObj)
{
    std::unique_lock<std::mutex> guard(schedulerMutex);
    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        auto algoAccountPtr = strategy::AlgoAccountManger::GetInstance().GetAlgoAccount(exportObj.req.name);
        if (algoAccountPtr == nullptr) {
            UBSE_LOG_INFO << "algo Account has been deleted, name is " << exportObj.req.name;
        }
        strategy::UbseMemAccountHelper::GetInstance().BorrowReturnSuccess(algoAccountPtr);
        strategy::AlgoAccountManger::GetInstance().DelAlgoAccount(exportObj.req.name);
        return UBSE_OK;
    }

    if (exportObj.status.state == obj::UBSE_MEM_STATE_FAILED) {
        strategy::UbseMemAccountHelper::GetInstance().BorrowFailed(exportObj.req.name);
        return UBSE_OK;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::scheduler