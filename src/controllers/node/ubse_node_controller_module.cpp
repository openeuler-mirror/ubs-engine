/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_node_controller_module.h"
#include <fstream>
#include <regex>
#include "ubse_election_module.h"
#include "ubse_node_controller_collector.h"
#include "ubse_node_controller_msg.h"
#include "ubse_node_controller_util.h"
#include "ubse_node_module.h"
#include "ubse_ras_handler.h"

namespace ubse::nodeController {
using namespace ubse::election;
using namespace ubse::node;
using namespace ubse::ras;

DYNAMIC_CREATE(UbseNodeControllerModule);
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_NODE_CONTROLLER_MID)

const uint32_t LOCAL_COLLECTOR_DURATION = 2000;
const uint32_t CLUSTER_COLLECTOR_DURATION = 300000;
const uint32_t CLUSTER_TOPOLOGY_DURATION = 2000;
const uint32_t HA_SEQUENCE_ID = 101; // 需要确保在节点建链后触发，节点建链优先级100

const uint32_t COLLECT_TOPOLOGY_RETRY_TIMES = 3;
const uint32_t COLLECT_TOPOLOGY_RETRY_DURATION = 2;

UbseResult UbseNodeControllerModule::Initialize()
{
    // 注册消息处理函数
    RegNodeControllerHandler();

    // 监听主上线事件，启动拓扑采集和周期平滑流程
    UbseElectionHandlerBuilder Builder;
    Builder.SetHandler([this](UbseElectionEventType, UBSE_ID_TYPE nodeId) { return MasterOnlineHandler(nodeId); });
    Builder.SetPriority(UbseElectionHandlerPriority::HIGH);
    Builder.SetSequenceId(HA_SEQUENCE_ID); // 需要确保在节点建链后触发，节点建链优先级100
    Builder.SetType(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION);
    Builder.SetName("UbseNodeControllerMasterOnLine");
    UbseElectionChangeAttachHandler(Builder.Build());

    // 监听节点上线事件，触发采集和平滑流程
    UbseElectionHandlerBuilder NodeUpBuilder;
    NodeUpBuilder.SetHandler([this](UbseElectionEventType, UBSE_ID_TYPE nodeId) {
        NodeUpHandler(nodeId);
        return UBSE_OK;
    });
    NodeUpBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeUpBuilder.SetSequenceId(HA_SEQUENCE_ID); // 需要确保在节点建链后触发，节点建链优先级100
    NodeUpBuilder.SetType(UbseElectionEventType::NODE_UP);
    NodeUpBuilder.SetName("UbseNodeControllerNodeUp");
    UbseElectionChangeAttachHandler(NodeUpBuilder.Build());

    // 监听节点下线事件，将节点状态置为 Unknown
    UbseElectionHandlerBuilder NodeDownBuilder;
    NodeDownBuilder.SetHandler([this](UbseElectionEventType, UBSE_ID_TYPE nodeId) {
        NodeDownHandler(nodeId);
        return UBSE_OK;
    });
    NodeDownBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeDownBuilder.SetSequenceId(HA_SEQUENCE_ID); // 需要确保在节点建链后触发，节点建链优先级100
    NodeDownBuilder.SetType(UbseElectionEventType::NODE_DOWN);
    NodeDownBuilder.SetName("UbseNodeControllerNodeDown");
    UbseElectionChangeAttachHandler(NodeDownBuilder.Build());

    // 监听故障事件，将节点状态置为 Fault
    UbseRasHandler::GetInstance().SetNodeStateHandler(
        [this](const std::string &faultInfo) { NodeFaultHandler(faultInfo); });
    return UBSE_OK;
}

void CollectBaseInfo(UbseNodeInfo &info)
{
    while (true) {
        auto ret = CollectNodeBaseInfo(info);
        if (ret == UBSE_OK) {
            return;
        }
        UBSE_LOG_ERROR << "collect node base info failed, " << FormatRetCode(ret);
        sleep(COLLECT_TOPOLOGY_RETRY_DURATION);
    }
}

void CollectTopology(UbseNodeInfo &info)
{
    while (true) {
        auto ret = CollectNodeTopology(info);
        if (ret == UBSE_OK) {
            return;
        }
        UBSE_LOG_ERROR << "collect node topology failed, " << FormatRetCode(ret);
        sleep(COLLECT_TOPOLOGY_RETRY_DURATION);
    }
}

void UbseNodeControllerModule::StartExec()
{
    // 采集节点基本信息
    UbseNodeInfo info{};
    CollectBaseInfo(info);
    UbseNodeController::GetInstance().SetCurrentNodeId(info.nodeId);
    // 将节点本地状态置为restore，触发账本平滑流程，直到成功为止。
    UbseNodeController::GetInstance().UpdateNodeInfo(info.nodeId, info);
    // 采集节点拓扑信息
    CollectTopology(info);
    UbseNodeController::GetInstance().UpdateNodeInfo(info.nodeId, info);
    UbseNodeController::GetInstance().UpdateNodeInfoLocalState(UbseNodeLocalState::UBSE_NODE_READY);
    // 待平滑后再启动定期numa采集
    localCollectTimer.Start(LOCAL_COLLECTOR_DURATION, []() -> UbseResult {
        UbseNodeInfo localInfo{};
        CollectBaseInfo(localInfo);
        // 定期采集numa的信息更新至节点
        auto collectRet = CollectNodeTopology(localInfo);
        if (collectRet != UBSE_OK) {
            UBSE_LOG_ERROR << "collect node info failed, " << FormatRetCode(collectRet);
            return collectRet;
        }
        UbseNodeController::GetInstance().UpdateNodeInfo(localInfo.nodeId, localInfo);
        return UBSE_OK;
    });
    std::string ubseTopologyChangeEvent = "UbseTopologyChangeEvent";
    UbseSubEvent(ubseTopologyChangeEvent, CollectWhenLcneChange);
}

UbseResult UbseNodeControllerModule::Start()
{
    try {
        std::thread([this]() -> void { StartExec(); }).detach();
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "create thread failed: " << e.what() << ", " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseNodeControllerModule::MasterOnlineHandler(const std::string &nodeId)
{
    std::unique_lock<std::shared_mutex> lock(rwMutex);
    UBSE_LOG_INFO << "node controller master online, master nodeId=" << nodeId;
    auto electionModule = UbseContext::GetInstance().GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "election module not load.";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    if (!electionModule->IsLeader()) {
        // 主变更后且本节点非主，停止已有的对账定时器(若存在).
        clusterCollectTimer.Stop();
        clusterNodeTopologyTimer.Stop();
        UbseNodeController::GetInstance().CleanAfterMasterSwitchRole();
        return UBSE_OK;
    }
    // 定时器是等待间隔后触发，先主动触发一次拓扑采集和对账
    ClusterCollectNodeTopology();
    MasterOnLineClusterCollector();
    clusterCollectTimer.Start(CLUSTER_COLLECTOR_DURATION, [this]() -> UbseResult {
        CycleCollectLedger();
        return UBSE_OK;
    });
    clusterNodeTopologyTimer.Start(CLUSTER_TOPOLOGY_DURATION, [this]() -> UbseResult {
        ClusterCollectNodeTopology();
        return UBSE_OK;
    });
    return UBSE_OK;
}

void PrintAllLinkNodes(const std::vector<UbseRoleInfo> &roleInfos)
{
    for (auto node : roleInfos) {
        UBSE_LOG_INFO << "nodeId=" << node.nodeId;
    }
}

void UbseNodeControllerModule::ClusterCollectNodeTopology()
{
    UBSE_LOG_INFO << "Start to collect node topology";
    std::vector<UbseRoleInfo> roleInfos{};
    UbseResult ret = UbseNodeGetLinkUpNodes(roleInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get all link nodes failed, " << FormatRetCode(ret);
        return;
    }
    UBSE_LOG_INFO << "collect node topology, print all link up nodes";
    PrintAllLinkNodes(roleInfos);
    for (auto node : roleInfos) {
        if (UbseNodeController::GetInstance().currentNodeId == node.nodeId) {
            UBSE_LOG_INFO << "skip current nodeId=" << node.nodeId;
            continue;
        }
        UbseNodeInfo info{};
        UBSE_LOG_INFO << "nodeId=" << node.nodeId << " start to collect topology";
        auto collectRet = CollectRemoteNodeInfo(node.nodeId, info);
        if (collectRet != UBSE_OK) {
            UBSE_LOG_ERROR << "collect remote nodeId=" << node.nodeId << " topology failed, "
                           << FormatRetCode(collectRet);
            continue;
        }
        UbseNodeController::GetInstance().UpdateNodeInfo(node.nodeId, info);
    }
}

void UbseNodeControllerModule::MasterOnLineClusterCollector()
{
    UBSE_LOG_INFO << "master oneline, start to reconciliation";
    std::vector<UbseRoleInfo> roleInfos{};
    UbseResult ret = UbseNodeGetLinkUpNodes(roleInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get all link nodes failed, " << FormatRetCode(ret);
        return;
    }
    UBSE_LOG_INFO << "master oneline ledger, print all link up nodes";
    PrintAllLinkNodes(roleInfos);
    for (auto node : roleInfos) {
        try {
            std::thread([this, node]() -> void { CollectNode(node.nodeId); }).detach();
        } catch (const std::system_error &) {
            UBSE_LOG_ERROR << "Failed to create thread.";
            return;
        }
    }
}

void UbseNodeControllerModule::CollectNode(const std::string &nodeId)
{
    UbseNodeControllerLockMgr::WriteLock(nodeId);
    UBSE_LOG_INFO << "nodeId=" << nodeId << " start to collect ledger.";
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " not collect, will skip";
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " is smoothing, will skip";
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    CollectLedger(nodeId);
    UbseNodeControllerLockMgr::WriteUnLock(nodeId);
}

void UbseNodeControllerModule::CycleCollectNode(const std::string &nodeId)
{
    UbseNodeControllerLockMgr::WriteLock(nodeId);
    UBSE_LOG_INFO << "nodeId=" << nodeId << " start to collect ledger.";
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " not collect, will skip";
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_SMOOTHING ||
        nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT ||
        nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_UNKNOWN) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " state=" << static_cast<uint32_t>(nodeInfo.clusterState)
                      << ", can not ledger";
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    CollectLedger(nodeId);
    UbseNodeControllerLockMgr::WriteUnLock(nodeId);
}

void UbseNodeControllerModule::CycleCollectLedger()
{
    UBSE_LOG_INFO << "cycle reconciliation";
    std::vector<UbseRoleInfo> roleInfos{};
    UbseResult ret = UbseNodeGetLinkUpNodes(roleInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get all link nodes failed, " << FormatRetCode(ret);
        return;
    }
    UBSE_LOG_INFO << "cycle reconciliation, print all link up nodes";
    PrintAllLinkNodes(roleInfos);
    for (auto node : roleInfos) {
        try {
            std::thread([this, node]() -> void { CycleCollectNode(node.nodeId); }).detach();
        } catch (const std::system_error &) {
            UBSE_LOG_ERROR << "Failed to create thread.";
            return;
        }
    }
}

void UbseNodeControllerModule::CollectLedger(const std::string &nodeId)
{
    UBSE_LOG_INFO << "nodeId=" << nodeId << " start to collect reconciliation";
    auto ret =
        UbseNodeController::GetInstance().UpdateNodeInfoClusterState(nodeId, UbseNodeClusterState::UBSE_NODE_SMOOTHING);
    if (UbseNodeController::GetInstance().GetNodeById(nodeId).clusterState !=
        UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
        // 若对账期间，节点故障，故障模块会将节点状态修改为fault；
        // 在部分不断链的故障场景下，例如BMC下电失败，reboot -f等，若对账完毕发现状态非smoothing，不刷新状态。
        UBSE_LOG_INFO << "nodeId=" << nodeId << " not in smoothing after smoothing, stop update state, current state="
                      << static_cast<uint32_t>(UbseNodeController::GetInstance().GetNodeById(nodeId).clusterState);
        return;
    }
    if (ret == UBSE_OK) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " collect success, set to working";
        UbseNodeController::GetInstance().UpdateNodeInfoClusterState(nodeId, UbseNodeClusterState::UBSE_NODE_WORKING);
    } else {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " collect failed, " << FormatRetCode(ret) << " set to unknown";
        UbseNodeController::GetInstance().UpdateNodeInfoClusterState(nodeId, UbseNodeClusterState::UBSE_NODE_UNKNOWN);
    }
    UBSE_LOG_INFO << "nodeId=" << nodeId << " collect ledger success.";
}

void UbseNodeControllerModule::NodeUpHandler(const std::string &nodeId)
{
    UbseNodeInfo info{};
    UBSE_LOG_INFO << "nodeId=" << nodeId << " node up, start to collect topology";
    UbseResult ret = UBSE_OK;
    for (uint32_t i = 0; i < COLLECT_TOPOLOGY_RETRY_TIMES; i++) {
        ret = CollectRemoteNodeInfo(nodeId, info);
        if (ret == UBSE_OK) {
            break;
        }
        sleep(COLLECT_TOPOLOGY_RETRY_DURATION);
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "collect remote nodeId=" << nodeId << " topology failed, " << FormatRetCode(ret)
                       << " stop ledger.";
        return;
    }
    UbseNodeController::GetInstance().UpdateNodeInfo(nodeId, info);
    try {
        std::thread([this, nodeId]() -> void { CollectNode(nodeId); }).detach();
    } catch (const std::system_error &) {
        UBSE_LOG_ERROR << "Failed to create thread.";
        return;
    }
}

void UbseNodeControllerModule::NodeDownHandler(const std::string &nodeId)
{
    UBSE_LOG_INFO << "nodeId=" << nodeId << " down, set state to unknown";
    UbseNodeController::GetInstance().UpdateNodeInfoClusterState(nodeId, UbseNodeClusterState::UBSE_NODE_UNKNOWN);
}

void UbseNodeControllerModule::NodeFaultHandler(const std::string &nodeId)
{
    UBSE_LOG_INFO << "nodeId=" << nodeId << " fault, set state to fault";
    UbseNodeController::GetInstance().UpdateNodeInfoClusterState(nodeId, UbseNodeClusterState::UBSE_NODE_FAULT);
}

void UbseNodeControllerModule::UnInitialize()
{
    localCollectTimer.Stop();
    clusterCollectTimer.Stop();

    // 监听主上线时间，启动拓扑采集和周期平滑流程
    UbseElectionHandlerBuilder Builder;
    Builder.SetHandler([this](UbseElectionEventType, UBSE_ID_TYPE nodeId) { return MasterOnlineHandler(nodeId); });
    Builder.SetPriority(UbseElectionHandlerPriority::HIGH);
    Builder.SetSequenceId(HA_SEQUENCE_ID); // 需要确保在节点建链后触发，节点建链优先级100
    Builder.SetType(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION);
    Builder.SetName("UbseNodeControllerMasterOnLine");
    UbseElectionChangeDeAttachHandler(Builder.Build());

    // 监听节点上线事件，触发采集和平滑流程
    UbseElectionHandlerBuilder NodeUpBuilder;
    NodeUpBuilder.SetHandler([this](UbseElectionEventType, UBSE_ID_TYPE nodeId) {
        NodeUpHandler(nodeId);
        return UBSE_OK;
    });
    NodeUpBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeUpBuilder.SetSequenceId(HA_SEQUENCE_ID); // 需要确保在节点建链后触发，节点建链优先级100
    NodeUpBuilder.SetType(UbseElectionEventType::NODE_UP);
    NodeUpBuilder.SetName("UbseNodeControllerNodeUp");
    UbseElectionChangeDeAttachHandler(NodeUpBuilder.Build());

    // 监听节点下线事件，将节点状态置为 Unknown
    UbseElectionHandlerBuilder NodeDownBuilder;
    NodeDownBuilder.SetHandler([this](UbseElectionEventType, UBSE_ID_TYPE nodeId) {
        NodeDownHandler(nodeId);
        return UBSE_OK;
    });
    NodeDownBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeDownBuilder.SetSequenceId(HA_SEQUENCE_ID); // 需要确保在节点建链后触发，节点建链优先级100
    NodeDownBuilder.SetType(UbseElectionEventType::NODE_DOWN);
    NodeDownBuilder.SetName("UbseNodeControllerNodeDown");
    UbseElectionChangeDeAttachHandler(NodeDownBuilder.Build());
    UbseRasHandler::GetInstance().SetNodeStateHandler(nullptr);

    std::string ubseTopologyChangeEvent = "UbseTopologyChangeEvent";
    UbseUnSubEvent(ubseTopologyChangeEvent, CollectWhenLcneChange);
}

void UbseNodeControllerModule::Stop() {}

UbseResult UbseNodeControllerModule::CollectWhenLcneChange(std::string &, std::string &)
{
    UBSE_LOG_INFO << "lcne change, start to collect";
    UbseNodeInfo info{};
    CollectBaseInfo(info);
    info.nodeId = UbseNodeController::GetInstance().GetCurrentNodeId();
    CollectTopology(info);
    UbseNodeController::GetInstance().UpdateNodeInfo(info.nodeId, info);
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "elc module not load.";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    if (module->IsLeader()) {
        // 发布事件
        std::string pubId = "UbseClusterTopologyChangeEvent";
        UbsePubEvent(pubId, info.nodeId);
        return UBSE_OK;
    }
    Node node{};
    auto ret = module->UbseGetMasterNode(node);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get master node failed, " << FormatRetCode(ret);
        return ret;
    }
    return ReportUbseNodeInfo(node.id, info);
}

uint32_t UbseNodeControllerModule::UbseNodeGetLinkUpNodes(std::vector<UbseRoleInfo> &roleInfos)
{
    auto elcmodule = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (elcmodule == nullptr) {
        UBSE_LOG_ERROR << "election module not init";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    if (!elcmodule->IsLeader()) {
        UBSE_LOG_ERROR << "current node not master";
        return UBSE_ERROR;
    }
    roleInfos = UbseNodeModule::GetLinkUpNodes();
    return UBSE_OK;
}
} // namespace ubse::nodeController