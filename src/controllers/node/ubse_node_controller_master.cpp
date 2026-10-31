/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_node_controller_master.h"

#include <unistd.h>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <unordered_set>

#include "ubse_common_def.h"
#include "ubse_election.h"
#include "ubse_election_module.h"
#include "ubse_error.h"
#include "ubse_event.h"
#include "ubse_logger.h"
#include "ubse_node.h"
#include "ubse_node_controller_agent.h"
#include "ubse_node_controller_util.h"
#include "ubse_node_info_serialize.h"
#include "ubse_node_mgr.h"
#include "ubse_ras_handler.h"
#include "ubse_serial_util.h"
#include "ubse_str_util.h"
#include "ubse_timer.h"
#include "adapter_plugins/mti/ubse_smbios.h"

const uint32_t HA_SEQUENCE_ID = 101; // todo 待链路合并后下线，需要确保在节点建链后触发，节点建链优先级100。

const uint32_t UBSE_COLLECT_TOPOLOGY_RETRY_INTERVAL = 400; // 节点上线，主动采集失败重试周期，单位豪秒
const uint32_t UBSE_COLLECT_TOPOLOGY_RETRY_TIMES = 5;      // 节点上线，主动采集失败重试次数
const uint32_t UBSE_NODE_LEDGER_INTERVAL = 300;            // 中心侧主动向各节点对账周期；单位秒
const uint32_t UBSE_NODE_LEDGER_RETRY_INTERVAL = 60;       // 中心侧对账失败重试周期；单位秒
const uint32_t UBSE_REPORT_LOG_INTERVAL = 60;              // 中心侧收到各节点上报日志打印周期，单位秒
const uint32_t UBSE_NODE_SYNC_INTERVAL = 60;               // 主备全量快照周期，单位秒
const std::string UBSE_NODE_MASTER_LEDGER_TIMER = "UbseNodeLedger";
const std::string UBSE_NODE_SYNC_FULL_TIMER = "UbseNodeSyncFull";
const std::string UBSE_NODE_MASTER_LEDGER_RETRY_PREFIX = "UbseNodeLedgerRetry_";
const std::string UBSE_NODE_MASTER_ONLINE = "UbseMasterOnLine";
const std::string UBSE_NODE_NODE_UP = "UbseNodeUp";
const std::string UBSE_NODE_NODE_DOWN = "UbseNodeDown";
const std::string UBSE_CASCADE_NODE_NODE_DOWN = "UbseCascadeNodeDown";
const std::string UBSE_GLOBAL_NODE_DOWN = "UbseGlobalNodeDown";
const std::string UBSE_NODE_GLOBAL_MASTER_ONLINE = "UbseGlobalMasterOnLine";
constexpr int UBSE_RPC_TIMEOUT_MS = 60000; // 5秒超时

UBSE_DEFINE_THIS_MODULE("ubse");
namespace ubse::nodeController {
using namespace ubse::context;
using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::ras;
using namespace ubse::event;
using namespace ubse::timer;
using namespace ubse::serial;
using namespace ubse::common::def;
using namespace ubse::task_executor;
using namespace ubse::com;

constexpr UbseResult UBSE_ERROR_TIMEOUT = 0x80000001;
std::string LCNE_CHANGE_REPORT_EVENT = UBSE_EVENT_CLUSTER_TOPOLOGY_CHANGE;
const std::string UBSE_NODE_RAS_FAULT_NOTIFY = "ras fault notify";

static bool IsNodeInGroup(const ubse::election::GroupTopology &group, const std::string &nodeId)
{
    if (group.groupMasterId == nodeId || group.groupStandbyId == nodeId) {
        return true;
    }

    for (const auto &node : group.groupNodes) {
        if (node == nodeId) {
            return true;
        }
    }

    return false;
}

static const ubse::election::GroupTopology *FindGroupByNodeId(
    const std::vector<ubse::election::GroupTopology> &groups, const std::string &nodeId)
{
    for (const auto &group : groups) {
        if (IsNodeInGroup(group, nodeId)) {
            return &group;
        }
    }

    return nullptr;
}

static void CollectGroupNodeIds(const ubse::election::GroupTopology &group, bool includeMountedGroups,
                                const std::vector<ubse::election::GroupTopology> &groups,
                                std::unordered_set<std::string> &nodeIds)
{
    if (!group.groupMasterId.empty()) {
        nodeIds.insert(group.groupMasterId);
    }

    if (!group.groupStandbyId.empty()) {
        nodeIds.insert(group.groupStandbyId);
    }

    for (const auto &node : group.groupNodes) {
        if (!node.empty()) {
            nodeIds.insert(node);
        }
    }

    if (!includeMountedGroups) {
        return;
    }

    for (const auto &mountedGroup : groups) {
        CollectGroupNodeIds(mountedGroup, false, {}, nodeIds);
    }
}

static bool HasSubHealthPortUpChange(const UbseNodeInfo& oldInfo, const UbseNodeInfo& newInfo)
{
    for (const auto& [location, newCpuInfo] : newInfo.cpuInfos) {
        auto oldCpuIter = oldInfo.cpuInfos.find(location);

        for (const auto& [portId, newPortInfo] : newCpuInfo.portInfos) {
            if (newPortInfo.portStatus != PortStatus::UP) {
                continue;
            }

            if (oldCpuIter == oldInfo.cpuInfos.end()) {
                return true;
            }

            auto oldPortIter = oldCpuIter->second.portInfos.find(portId);
            if (oldPortIter == oldCpuIter->second.portInfos.end() ||
                oldPortIter->second.portStatus != PortStatus::UP) {
                return true;
            }
        }
    }

    return false;
}

UbseResult ClusterInfoQueryHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

template <typename Handler>
static UbseResult RegisterMasterRpcService(UbseNodeControllerOpCode opCode, Handler handler,
                                           const char *errorMessage)
{
    const ubse::com::UbseComEndpoint endpoint = {
        static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER),
        static_cast<uint32_t>(opCode)};

    auto ret = UbseRegRpcService(endpoint, handler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << errorMessage;
    }
    return ret;
}

// Master端消息处理注册
UbseResult RegMasterMsgHandler()
{
    auto ret = RegisterMasterRpcService(UbseNodeControllerOpCode::NODE_CONTROLLER_ALL_NODE,
                                        GetAllNodeInfoFromRemoteHandler, "Register all node endpoint failed");
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = RegisterMasterRpcService(UbseNodeControllerOpCode::NODE_CONTROLLER_LCNE_CHANGE_REPORT_TOPOLOGY,
                                   LcneChangeNodeInfoHandler, "Register lcne report endpoint failed");
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = RegisterMasterRpcService(UbseNodeControllerOpCode::NODE_CONTROLLER_GET_DEV_CONNECT,
                                   UbseGetDirConnectInfoFromRemoteHandler, "Register get dev connect endpoint failed");
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = RegisterMasterRpcService(UbseNodeControllerOpCode::NODE_CONTROLLER_REPORT,
                                   UbseNodeReportNodeInfoHandler, "Register report endpoint failed");
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = RegisterMasterRpcService(UbseNodeControllerOpCode::NODE_CONTROLLER_CABINET_FULL_REPORT,
                                   CabinetNodeReportHandler, "Register cabinet full report endpoint failed");
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = RegisterMasterRpcService(UbseNodeControllerOpCode::NODE_CONTROLLER_GLOBAL_FULL_REPORT,
                                   GlobalNodeReportHandler, "Register global full report endpoint failed");
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = RegisterMasterRpcService(UbseNodeControllerOpCode::NODE_CONTROLLER_SINGLE_NODE_REPORT,
                                   SingleNodeReportHandler, "Register single node report endpoint failed");
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = RegisterMasterRpcService(UbseNodeControllerOpCode::NODE_CONTROLLER_CLUSTER_INFO,
                                   ClusterInfoQueryHandler, "Register cluster info endpoint failed");
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = RegisterMasterRpcService(UbseNodeControllerOpCode::NODE_CONTROLLER_CASCADE_NODE_REPORT,
                                   UbseRemoteCasCadeNodeDownHandler, "Register cascade down endpoint failed");
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = RegisterMasterRpcService(UbseNodeControllerOpCode::NODE_CONTROLLER_NODE_INFO_SYNC_REQ,
                                   NodeInfoSyncReqHandler, "Register node info sync req endpoint failed");
    if (ret != UBSE_OK) {
        return ret;
    }

    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "get com module failed";
        return UBSE_ERROR_NULLPTR;
    }

    UbseComBaseMessageHandlerPtr lcneTopologyReportHandler = new (std::nothrow) UbseLcneTopologyMessageChangeHandler();
    if (lcneTopologyReportHandler == nullptr) {
        UBSE_LOG_ERROR << "get lcne topology report failed";
        return UBSE_ERROR_NULLPTR;
    }

    ret = comModule->RegRpcService<UbseNodeInfoSerialize, UbseNodeInfoSerialize>(lcneTopologyReportHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register lcne topology report failed";
        return ret;
    }

    UBSE_LOG_INFO << "Master message handlers registered successfully";
    return UBSE_OK;
}

UbseResult UbseNodeControllerMaster::Initialize()
{
    // 主上线；若当前为主节点，启动周期对账；否则清理资源
    UbseElectionHandlerBuilder Builder;
    Builder.SetHandler([this](UbseElectionEventType, UBSE_ID_TYPE nodeId) { return UbseMasterOnlineHandler(nodeId); });
    Builder.SetPriority(UbseElectionHandlerPriority::HIGH);
    Builder.SetSequenceId(HA_SEQUENCE_ID);
    Builder.SetType(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION);
    Builder.SetName(UBSE_NODE_MASTER_ONLINE);
    UbseElectionChangeAttachHandler(Builder.Build());

    // 节点上线，主节点主动触发一次采集；然后启动对账
    UbseElectionHandlerBuilder NodeUpBuilder;
    NodeUpBuilder.SetHandler([this](UbseElectionEventType, UBSE_ID_TYPE nodeId) { return UbseNodeUpHandler(nodeId); });
    NodeUpBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeUpBuilder.SetSequenceId(HA_SEQUENCE_ID);
    NodeUpBuilder.SetType(UbseElectionEventType::NODE_UP);
    NodeUpBuilder.SetName(UBSE_NODE_NODE_UP);
    UbseElectionChangeAttachHandler(NodeUpBuilder.Build());

    // 节点下线，将状态设置为 unknown
    UbseElectionHandlerBuilder NodeDownBuilder;
    NodeDownBuilder.SetHandler(
        [this](UbseElectionEventType, UBSE_ID_TYPE nodeId) { return UbseNodeDownHandler(nodeId); });
    NodeDownBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeDownBuilder.SetSequenceId(HA_SEQUENCE_ID);
    NodeDownBuilder.SetType(UbseElectionEventType::NODE_DOWN);
    NodeDownBuilder.SetName(UBSE_NODE_NODE_DOWN);
    UbseElectionChangeAttachHandler(NodeDownBuilder.Build());

    // 级联机柜主下线
    UbseElectionHandlerBuilder CasCadeNodeDownBuilder;
    CasCadeNodeDownBuilder.SetHandler(
        [this](UbseElectionEventType, UBSE_ID_TYPE nodeId) { return UbseCasCadeNodeDownHandler(nodeId); });
    CasCadeNodeDownBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    CasCadeNodeDownBuilder.SetSequenceId(HA_SEQUENCE_ID);
    CasCadeNodeDownBuilder.SetType(UbseElectionEventType::GLOBAL_CASCADE_NODE_DOWN);
    CasCadeNodeDownBuilder.SetName(UBSE_CASCADE_NODE_NODE_DOWN);
    UbseElectionChangeAttachHandler(CasCadeNodeDownBuilder.Build());

    // PD机柜主下线
    UbseElectionHandlerBuilder GlobalNodeDownBuilder;
    GlobalNodeDownBuilder.SetHandler(
        [this](UbseElectionEventType, UBSE_ID_TYPE nodeId) { return UbseGlobalNodeDownHandler(nodeId); });
    GlobalNodeDownBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    GlobalNodeDownBuilder.SetSequenceId(HA_SEQUENCE_ID);
    GlobalNodeDownBuilder.SetType(UbseElectionEventType::GLOBAL_NODE_DOWN);
    GlobalNodeDownBuilder.SetName(UBSE_GLOBAL_NODE_DOWN);
    UbseElectionChangeAttachHandler(GlobalNodeDownBuilder.Build());

    // 全局主上线通知：机柜主收到后重置 globalState + 触发摘要上报
    UbseElectionHandlerBuilder globalMasterOnlineBuilder;
    globalMasterOnlineBuilder.SetHandler(
        [this](UbseElectionEventType, UBSE_ID_TYPE globalMasterId) {
            return UbseGlobalMasterOnlineHandler(globalMasterId);
        });
    globalMasterOnlineBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    globalMasterOnlineBuilder.SetSequenceId(HA_SEQUENCE_ID);
    globalMasterOnlineBuilder.SetType(UbseElectionEventType::GLOBAL_MASTER_ONLINE_NOTIFICATION);
    globalMasterOnlineBuilder.SetName(UBSE_NODE_GLOBAL_MASTER_ONLINE);
    UbseElectionChangeAttachHandler(globalMasterOnlineBuilder.Build());

    // prebmc 故障回调；仅允许连通节点：smoothing，working状态节点进入prebmc状态
    UbseRasHandler::GetInstance().RegisterNodeHandler(
        NodeHandlerType::PRE_FAULT_STATE_HANDLER_TYPE,
        [this](const std::string &nodeId) -> UbseResult { return UbseNodeRasPreFaultHandler(nodeId); });

    // bmc下电失败，节点进入 smoothing状态
    UbseRasHandler::GetInstance().RegisterNodeHandler(
        NodeHandlerType::PRE_FAULT_STATE_FAIL_HANDLER_TYPE,
        [this](const std::string &nodeId) -> UbseResult { return UbseNodeRasPreFaultFailHandler(nodeId); });

    // panic，节点重启，bmc下电成功回调，节点状态置为 fault
    UbseRasHandler::GetInstance().RegisterNodeHandler(
        NodeHandlerType::NODE_FAULT_STATE_HANDLER_TYPE,
        [this](const std::string &nodeId) -> UbseResult { return UbseNodeRasFaultHandler(nodeId); });

    // 节点故障清除回调
    UbseRasHandler::GetInstance().RegisterNodeHandler(
        NodeHandlerType::NODE_FAULT_STATE_CLEAR_HANDLER_TYPE,
        [this](const std::string &nodeId) -> UbseResult { return UbseNodeRasAfterFaultClearHandler(nodeId); });

    return UBSE_OK;
}

UbseResult UbseNodeControllerMaster::UbseGlobalMasterOnlineHandler(const std::string &globalMasterId)
{
    const std::string currentNodeId = UbseNodeController::GetInstance().GetCurrentNodeId();
    UBSE_LOG_INFO << "[CLOS_EVENT] global master online notification, globalMasterId=" << globalMasterId
                  << ", currentNodeId=" << currentNodeId
                  << ", role=" << static_cast<uint32_t>(GetClosRole());

    UbseNodeController::GetInstance().CleanAfterGlobalMasterSwitchRole();
    UBSE_LOG_INFO << "[CLOS_STATE] all nodes globalState reset to GLOBAL_INIT on global master online";

    // 升全局主：消费全局备镜像重建"非本机柜"全局视图（本机柜节点仍由柜内主管理），
    // 状态原样继承，避免倒换后等待各机柜重新上报导致全局视图短暂缺失。
    if (!globalMasterId.empty() && globalMasterId == currentNodeId) {
        InitSyncSeq();
        ConsumeMirrorOnPromote(true);
    }

    std::string eventMessage = globalMasterId;
    auto pubRet = UbsePubEvent(UBSE_EVENT_GLOBAL_MASTER_ONLINE, eventMessage);
    if (pubRet != UBSE_OK) {
        UBSE_LOG_WARN << "[CLOS_EVENT] publish global master online event failed, globalMasterId=" << globalMasterId
                      << ", " << FormatRetCode(pubRet);
    }

    return UBSE_OK;
}

UbseResult UbseNodeControllerMaster::UbseMasterOnlineHandler(const std::string &nodeId)
{
    auto currentNodeId = UbseNodeController::GetInstance().GetCurrentNodeId();
    const bool isClosType =
        ubse::adapter_plugins::smbios::UbseSmbios::GetInstance().IsClosType();

    if (isClosType) {
        auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
        if (module == nullptr) {
            UBSE_LOG_ERROR << "election module not load";
            return UBSE_ERROR_MODULE_LOAD_FAILED;
        }

        Node localMasterNode{};
        auto ret = module->GetLocalMasterNode(localMasterNode);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "get local master failed on master online, "
                          << FormatRetCode(ret);
            return ret;
        }

        if (localMasterNode.id.empty()) {
            UBSE_LOG_WARN << "local master id is empty on master online";
            return UBSE_ERROR;
        }

        // 其他组主上线不影响当前组
        if (nodeId != localMasterNode.id) {
            UBSE_LOG_INFO << "[CLOS_EVENT] ignore unrelated group master online"
                          << ", currentNodeId=" << currentNodeId
                          << ", eventMasterId=" << nodeId
                          << ", localMasterId=" << localMasterNode.id;
            return UBSE_OK;
        }
    }

    if (nodeId != currentNodeId) {
        UBSE_LOG_INFO << "master online, current nodeId=" << currentNodeId
                      << " not master, master nodeId=" << nodeId
                      << ", start to clean context.";
        UbseNodeCleanAfterSwitchStandby();
        return UBSE_OK;
    }

    UBSE_LOG_INFO << "master online, current nodeId=" << nodeId << " is master.";
    auto role = GetClosRole();
    UBSE_LOG_INFO << "[CLOS_ROLE] refresh role on master online, currentNodeId="
                  << currentNodeId << ", role=" << static_cast<uint32_t>(role);

    // 升主即以 epoch 毫秒为起点重置同步序号，先于任何推送，避免进程重启后备侧 lastSyncSeq_ 残留高位丢弃新推送
    InitSyncSeq();

    std::string selfnodeId = nodeId;
    auto ret = UbsePubEvent(UBSE_EVENT_NODE_JOIN, selfnodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbsePubEvent " << UBSE_EVENT_NODE_JOIN
                       << " failed on master node";
        return ret;
    }

    std::lock_guard<std::mutex> lock(taskExecMutex_);
    if (taskExecutor_ == nullptr || !taskExecutor_->Start()) {
        taskExecutor_ = UbseTaskExecutor::Create("UbseNodeMaster", NO_16, NO_1024);
        if (taskExecutor_ == nullptr || !taskExecutor_->Start()) {
            UBSE_LOG_ERROR << "master online, current nodeId=" << nodeId
                           << " start task thread pool failed";
            taskExecutor_ = nullptr;
            return UBSE_ERROR_NULLPTR;
        }
    }

    // 本节点对账（纳入per-node锁纪律，与镜像消费/周期/重试/NODE_UP对账路径互斥）
    taskExecutor_->Execute([this]() -> void {
        auto selfNodeId = UbseNodeController::GetInstance().GetCurrentNodeId();
        UbseNodeControllerLockMgr::WriteLock(selfNodeId);

        UbseNodeLedger(selfNodeId);

        auto selfNodeInfo = UbseNodeController::GetInstance().GetNodeById(selfNodeId);
        UBSE_LOG_INFO << "[CLOS_REPORT] master self state change report trigger, nodeId="
                      << selfNodeId << ", groupId=" << selfNodeInfo.groupId
                      << ", clusterState="
                      << static_cast<uint32_t>(selfNodeInfo.clusterState);

        auto reportRet = ReportSingleNodeChangeToPrev(selfNodeId, "master self ledger");
        if (reportRet != UBSE_OK) {
            UBSE_LOG_WARN << "[CLOS_STATE_REPORT] report master self change failed, nodeId="
                          << selfNodeId << ", " << FormatRetCode(reportRet);
        }
        UbseNodeControllerLockMgr::WriteUnLock(selfNodeId);
    });

    // 周期对账，注册对账定时器回调并启动定时器
    UbseTimerHandlerRegister(
        UBSE_NODE_MASTER_LEDGER_TIMER,
        [this]() -> UbseResult {
            UbseNodeLedgerTimerHandler();
            return UBSE_OK;
        },
        UBSE_NODE_LEDGER_INTERVAL);

    // 主备全量快照定时器（仅主内执行，降主后靠GetSyncTargets角色短路跳过）
    UbseTimerHandlerRegister(
        UBSE_NODE_SYNC_FULL_TIMER,
        [this]() -> UbseResult {
            UbseNodeSyncFullTimerHandler();
            return UBSE_OK;
        },
        UBSE_NODE_SYNC_INTERVAL);

    isLogAggregationRunning_.store(true);
    taskExecutor_->Execute([this]() -> void {
        ReportAggregation();
    });
    ConsumeMirrorOnPromote();
    return UBSE_OK;
}

UbseResult UbseNodeControllerMaster::Start()
{
    // 注册消息处理器
    auto ret = RegMasterMsgHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register master message handler failed, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

void PrintAllLinkNodes(const std::vector<UbseRoleInfo> &roleInfos)
{
    std::stringstream oss;
    for (auto &node : roleInfos) {
        oss << node.nodeId << ", ";
    }
    UBSE_LOG_INFO << "cluster link up nodes=" << oss.str();
}

void UbseNodeControllerMaster::UbseNodeLedgerTimerHandler()
{
    UBSE_LOG_INFO << "cycle reconciliation";

    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "election module not load, skip smoothing.";
        return;
    }

    // 仅当前选主组的主节点执行本组周期对账
    if (!module->IsLeader()) {
        UBSE_LOG_INFO << "current node not local master, skip smoothing.";
        return;
    }

    std::vector<UbseRoleInfo> roleInfos{};
    auto ret = UbseNodeGetLinkUpNodes(roleInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get all link nodes failed, " << FormatRetCode(ret);
        return;
    }
    PrintAllLinkNodes(roleInfos);
    for (auto &node : roleInfos) {
        std::string nodeId = node.nodeId;
        taskExecMutex_.lock();
        if (taskExecutor_ != nullptr) {
            taskExecutor_->Execute([this, nodeId]() -> void { UbseNodeCycleLedger(nodeId); });
        }
        taskExecMutex_.unlock();
    }
}

void UbseNodeControllerMaster::UbseNodeCycleLedger(const std::string &nodeId)
{
    UbseNodeControllerLockMgr::WriteLock(nodeId);
    UBSE_LOG_INFO << "nodeId=" << nodeId << " start to collect ledger.";
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " not report, will skip";
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    UBSE_LOG_INFO << "nodeId=" << nodeId
                  << " before collect ledger current state=" << static_cast<uint32_t>(nodeInfo.clusterState);
    // 预下电，故障，断连等异常场景不进行对账；
    // smoothing 表示节点已经在对账流程中，不对账；
    // init为初始静态数据 或者 节点首次上报，等待节点上线事件触发后启动对账
    if (nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_WORKING) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " state=" << static_cast<uint32_t>(nodeInfo.clusterState)
                      << ", can not ledger";
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    UbseNodeLedger(nodeId);
    UbseNodeControllerLockMgr::WriteUnLock(nodeId);
}

// 重试定时器记账集合: 注册时入集/注销时出集, 清理时按快照精确遍历。
// 旧清理实现按本group节点枚举拼名字, 跨柜/已移除节点的重试项永远对不上名字, 导致timer残留引发退出coredump
static std::mutex g_ledgerRetryTimerMutex;
static std::unordered_set<std::string> g_ledgerRetryTimers;

static void UbseUnregisterLedgerRetryTimer(const std::string& timerName)
{
    UbseTimerHandlerUnregister(timerName);
    std::lock_guard<std::mutex> lock(g_ledgerRetryTimerMutex);
    g_ledgerRetryTimers.erase(timerName);
}

// 对账失败时注册重试定时器: 含leader检查(降备清理后不再新增注册)、
// 同临界区注册与记账(防"已注册未记账"残留)、退出窗口自反清理护栏
void UbseNodeControllerMaster::UbseRegisterLedgerRetryTimer(const std::string& nodeId)
{
    // 非leader(降备清理后)不再注册重试定时器: 在途对账在清理后新增注册会形成残留
    auto electionModule = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (electionModule == nullptr || !electionModule->IsLeader()) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " not leader anymore, skip retry timer register";
        return;
    }
    std::string retryTimerName = UBSE_NODE_MASTER_LEDGER_RETRY_PREFIX + nodeId;
    // 注册与记账必须在同一临界区: 防止与清理路径(快照+clear集合)交错, 产生"已注册未记账"的残留
    std::lock_guard<std::mutex> retryBookkeepingLock(g_ledgerRetryTimerMutex);
    auto registerRet = UbseTimerHandlerRegister(
        retryTimerName,
        [this, nodeId]() -> UbseResult {
            // 进程退出窗口短路(不执行对账, 亦不自注销): 退出窗口无需再对账,
            // 残留timer由CleanAfterSwitchStandby(降备/退出共用清理)的记账集合兜底注销
            // (timer框架已支持池线程内安全注销最后一个handler, 拆除由专用线程异步执行)
            if (g_globalStop.load()) {
                return UBSE_OK;
            }
            UbseNodeRetryLedger(nodeId);
            return UBSE_OK;
        },
        UBSE_NODE_LEDGER_RETRY_INTERVAL);
    if (registerRet != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << nodeId << " register retry timer failed, " << FormatRetCode(registerRet);
        return;
    }
    g_ledgerRetryTimers.insert(retryTimerName);
}

void UbseNodeControllerMaster::UbseNodeRetryLedger(const std::string& nodeId)
{
    // 获取节点锁
    UbseNodeControllerLockMgr::WriteLock(nodeId);
    UBSE_LOG_INFO << "nodeId=" << nodeId << " start retry ledger";
    std::string timerName = UBSE_NODE_MASTER_LEDGER_RETRY_PREFIX + nodeId;
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr || !module->IsLeader()) {
        // 降备短路(不自注销): 降备清理函数即将按记账集合注销本timer, 无需重复
        UBSE_LOG_WARN << "nodeId=" << nodeId << " current node not local master, skip retry ledger";
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    // 检查节点是否存在
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " not report, cancel retry timer";
        UbseUnregisterLedgerRetryTimer(timerName);
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    // 判断节点是否处于故障状态
    if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT ||
        nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_PRE_BMC ||
        nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_UNKNOWN) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " is in fault state, cancel retry timer";
        // 取消对账定时器
        UbseUnregisterLedgerRetryTimer(timerName);
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    // 对账 = 切换到SMOOTHING
    UbseResult ret =
        UbseNodeController::GetInstance().UpdateNodeInfoClusterState(nodeId, UbseNodeClusterState::UBSE_NODE_SMOOTHING);
    // 获取切换后的状态
    nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    // 检查节点是否处于故障状态
    if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT ||
        nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_PRE_BMC ||
        nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_UNKNOWN) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " changed to fault state after ledger, cancel retry timer";
        // 取消对账定时器
        UbseUnregisterLedgerRetryTimer(timerName);
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    // 判断对账执行结果
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " failed to switch to smoothing, will retry in next cycle";
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    // 检查状态是否为 SMOOTHING
    if (nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " expected UBSE_NODE_SMOOTHING, but current state="
                      << static_cast<uint32_t>(nodeInfo.clusterState);
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    // 对账成功，取消对账定时器
    auto workRet =
        UbseNodeController::GetInstance().UpdateNodeInfoClusterState(nodeId, UbseNodeClusterState::UBSE_NODE_WORKING);
    if (workRet != UBSE_OK) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " switch to working failed, keep retry timer, ret="
                      << FormatRetCode(workRet);
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    auto reportAfterRet = ReportSingleNodeChangeToPrev(nodeId, "ledger after working");
    if (reportAfterRet != UBSE_OK) {
        UBSE_LOG_WARN << "report after working failed, nodeId=" << nodeId << ", "
                      << FormatRetCode(reportAfterRet);
    }
    // 对账成功，取消对账定时器
    UbseUnregisterLedgerRetryTimer(timerName);
    UBSE_LOG_INFO << "nodeId=" << nodeId << " retry ledger success, timer cleaned";
    // 释放锁
    UbseNodeControllerLockMgr::WriteUnLock(nodeId);
}

void UbseNodeControllerMaster::UbseNodeLedger(const std::string &nodeId)
{
    UBSE_LOG_INFO << "nodeId=" << nodeId << " start to collect reconciliation";

    auto beforeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    UBSE_LOG_INFO << "[CLOS_STATE] before set smoothing, nodeId=" << nodeId
                  << ", clusterState=" << static_cast<uint32_t>(beforeInfo.clusterState);

    auto ret =
        UbseNodeController::GetInstance().UpdateNodeInfoClusterState(nodeId, UbseNodeClusterState::UBSE_NODE_SMOOTHING);

    auto smoothingInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    UBSE_LOG_INFO << "[CLOS_STATE] after set smoothing, nodeId=" << nodeId << ", ret=" << FormatRetCode(ret)
                  << ", clusterState=" << static_cast<uint32_t>(smoothingInfo.clusterState);

    // 对账前同步 SMOOTHING 状态到全局主，确保对账期间全局主不调度跨柜任务到该节点
    auto reportBeforeRet = ReportSingleNodeChangeToPrev(nodeId, "ledger before smoothing");
    if (reportBeforeRet != UBSE_OK) {
        UBSE_LOG_WARN << "[CLOS_REPORT] report before smoothing failed, nodeId=" << nodeId << ", "
                      << FormatRetCode(reportBeforeRet);
    }

    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " smoothing failed, will retry, ret=" << FormatRetCode(ret);
        UbseRegisterLedgerRetryTimer(nodeId); // 内部含leader检查/同临界区记账/退出护栏
        return;
    }

    if (smoothingInfo.clusterState != UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
        // 若对账期间，节点故障或者断连，故障模块会将节点状态修改为fault，unknown；
        // 在部分不断链的故障场景下，例如BMC下电失败，reboot -f等，若对账完毕发现状态非smoothing，不刷新状态。
        UBSE_LOG_INFO << "nodeId=" << nodeId << " not in smoothing after smoothing, stop update state, current state="
                      << static_cast<uint32_t>(smoothingInfo.clusterState);
        return;
    }

    ret = UbseNodeController::GetInstance().UpdateNodeInfoClusterState(nodeId, UbseNodeClusterState::UBSE_NODE_WORKING);

    auto workingInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    UBSE_LOG_INFO << "[CLOS_STATE] after set working, nodeId=" << nodeId << ", ret=" << FormatRetCode(ret)
                  << ", clusterState=" << static_cast<uint32_t>(workingInfo.clusterState);

    // 对账后同步 WORKING 状态到全局主
    auto reportAfterRet = ReportSingleNodeChangeToPrev(nodeId, "ledger after working");
    if (reportAfterRet != UBSE_OK) {
        UBSE_LOG_WARN << "[CLOS_REPORT] report after working failed, nodeId=" << nodeId << ", "
                      << FormatRetCode(reportAfterRet);
    }

    UBSE_LOG_INFO << "nodeId=" << nodeId << " collect ledger success.";
}

void UbseNodeControllerMaster::ReportAggregation()
{
    std::unique_lock<std::mutex> cvLock(cvMutex_);
    while (!g_globalStop.load() && isLogAggregationRunning_.load()) {
        cv_.wait_for(cvLock, std::chrono::seconds(UBSE_REPORT_LOG_INTERVAL));
        if (g_globalStop.load()) {
            UBSE_LOG_WARN << "ubse process stop, stop log aggregation.";
            break;
        }
        if (!isLogAggregationRunning_.load()) {
            UBSE_LOG_WARN << "current node not master, stop log aggregation";
            break;
        }
        std::unique_lock<std::shared_mutex> reportLock(rwReportMutex_);
        std::stringstream buffer;
        buffer << "ubse node last 1min report summary:";
        for (auto &[id, count] : reportCounters_) {
            int reportCount = count;
            buffer << "nodeId=" << id << " report=" << reportCount << " times, ";
            count = 0;
        }
        UBSE_LOG_INFO << buffer.str();
    }
}

// 清除故障计数器
void UbseNodeControllerMaster::ClearFaultCounter(const std::string &nodeId)
{
    std::lock_guard<std::mutex> lock(faultCountersMutex_);
    auto it = faultReportCounters_.find(nodeId);
    if (it != faultReportCounters_.end()) {
        faultReportCounters_.erase(it);
        UBSE_LOG_DEBUG << "Cleared fault counter for node: " << nodeId;
    }
}

// 执行节点平滑处理
void UbseNodeControllerMaster::ExecuteNodeSmoothing(const std::string &nodeId)
{
    // 与镜像消费/NODE_UP对账路径共用per-node写锁，FAULT→SMOOTHING迁移原子化
    UbseNodeControllerLockMgr::WriteLock(nodeId);
    // 检查节点当前状态
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    // 只有故障状态才切换到smoothing
    if (nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_FAULT) {
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }

    UBSE_LOG_INFO << "Node " << nodeId << " switching from fault to smoothing state";

    // 直接调用UbseNodeLedger函数来处理完整的平滑过程
    UbseNodeLedger(nodeId);
    UbseNodeControllerLockMgr::WriteUnLock(nodeId);
}

UbseResult UbseNodeControllerMaster::UbseNodeReportHandler(const UbseNodeInfo &nodeInfo)
{
    // 参数校验
    if (nodeInfo.nodeId.empty()) {
        return UBSE_ERROR_INVAL;
    }

    UbseNodeInfo nodeInfoCopy = nodeInfo;
    auto ret = UbseNodeController::GetInstance().UpdateNodeInfo(nodeInfoCopy.nodeId, nodeInfoCopy);
    if (ret == UBSE_OK) {
        (void)ProcessGlobalStateAfterReport(nodeInfoCopy.nodeId);
    }
    UbseNodeController::GetInstance().UpdateDevDirConnectInfo();

    auto cachedNodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeInfoCopy.nodeId);

    UBSE_LOG_INFO << "[CLOS_RECV] receive local node report, nodeId=" << nodeInfo.nodeId << ", groupId=" << nodeInfo.groupId
                  << ", currentNodeId=" << UbseNodeController::GetInstance().GetCurrentNodeId()
                  << ", role=" << static_cast<uint32_t>(GetClosRole())
                  << ", reportClusterState=" << static_cast<uint32_t>(nodeInfo.clusterState)
                  << ", cachedClusterState=" << static_cast<uint32_t>(cachedNodeInfo.clusterState);

    // 上报统计
    {
        std::unique_lock<std::shared_mutex> lock(rwReportMutex_);
        ++reportCounters_[nodeInfoCopy.nodeId];
    }

    // 如果不是故障状态，直接返回
    auto localNodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeInfo.nodeId);
    if (localNodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_FAULT) {
        return UBSE_OK;
    }

    // 处理故障节点的计数器
    int currentCount = ProcessFaultCounter(nodeInfo.nodeId);
    if (currentCount < 0) { // 计数器不存在
        return UBSE_OK;
    }

    // 检查是否需要平滑处理
    if (currentCount < FAULT_REPORT_THRESHOLD) {
        return UBSE_OK;
    }

    // 触发平滑处理
    UBSE_LOG_INFO << "Node " << nodeInfo.nodeId << " reached 150 fault reports, switching to smoothing";

    std::lock_guard<std::mutex> lock(taskExecMutex_);
    if (taskExecutor_ == nullptr) {
        UBSE_LOG_WARN << "Task executor not available for node smoothing";
        return UBSE_OK;
    }

    taskExecutor_->Execute([this, nodeId = nodeInfo.nodeId]() -> void { ExecuteNodeSmoothing(nodeId); });

    return UBSE_OK;
}

UbseResult UbseNodeControllerMaster::UbseCabinetReportHandler(const std::vector<UbseNodeInfo> &infos)
{
    UBSE_LOG_INFO << "[CLOS_RECV] receive cabinet full report, currentNodeId="
                  << UbseNodeController::GetInstance().GetCurrentNodeId()
                  << ", role=" << static_cast<uint32_t>(GetClosRole()) << ", nodeCount=" << infos.size();

    for (const auto &info : infos) {
        if (info.nodeId.empty()) {
            continue;
        }

        UbseNodeInfo nodeInfo = info;
        auto ret = UbseNodeController::GetInstance().UpdateNodeInfo(nodeInfo.nodeId, nodeInfo);
        if (ret == UBSE_OK) {
            (void)ProcessGlobalStateAfterReport(nodeInfo.nodeId);
        }

        UBSE_LOG_INFO << "[CLOS_RECV] update node from cabinet full report, nodeId=" << nodeInfo.nodeId
                      << ", groupId=" << nodeInfo.groupId
                      << ", reportClusterState=" << static_cast<uint32_t>(info.clusterState)
                      << ", cachedClusterState=" << static_cast<uint32_t>(nodeInfo.clusterState);

        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "[CLOS_RECV] update node from cabinet full report failed, nodeId=" << info.nodeId << ", "
                          << FormatRetCode(ret);
        }
    }

    return UBSE_OK;
}

UbseResult UbseNodeControllerMaster::UbseGlobalReportHandler(const std::vector<UbseNodeInfo> &infos)
{
    UBSE_LOG_INFO << "[CLOS_RECV] receive global full report, currentNodeId="
                  << UbseNodeController::GetInstance().GetCurrentNodeId()
                  << ", role=" << static_cast<uint32_t>(GetClosRole()) << ", nodeCount=" << infos.size();

    for (const auto &info : infos) {
        if (info.nodeId.empty()) {
            continue;
        }

        UbseNodeInfo nodeInfo = info;
        auto ret = UbseNodeController::GetInstance().UpdateNodeInfo(nodeInfo.nodeId, nodeInfo);
        if (ret == UBSE_OK) {
            (void)ProcessGlobalStateAfterReport(nodeInfo.nodeId);
        }

        UBSE_LOG_INFO << "[CLOS_RECV] update node from global full report, nodeId=" << nodeInfo.nodeId
                      << ", groupId=" << nodeInfo.groupId
                      << ", reportClusterState=" << static_cast<uint32_t>(info.clusterState)
                      << ", cachedClusterState=" << static_cast<uint32_t>(nodeInfo.clusterState);

        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "[CLOS_RECV] update node from global full report failed, nodeId=" << info.nodeId << ", "
                          << FormatRetCode(ret);
        }
    }

    return UBSE_OK;
}

UbseResult UbseNodeControllerMaster::UbseSingleNodeReportHandler(const UbseNodeInfo& info)
{
    if (info.nodeId.empty()) {
        return UBSE_ERROR_INVAL;
    }

    // 故障通知直接交给本机柜主处理，不将通知报文写入节点缓存。
    if (info.eventMessage == UBSE_NODE_RAS_FAULT_NOTIFY) {
        if (!IsHierarchicalElection()) {
            UBSE_LOG_WARN << "[RAS_FAULT_NOTIFY] reject fault notification in non-hierarchical election, nodeId="
                          << info.nodeId;
            return UBSE_ERROR_INVAL;
        }

        auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
        if (module == nullptr) {
            return UBSE_ERROR_MODULE_LOAD_FAILED;
        }
        if (!module->IsLeader()) {
            UBSE_LOG_WARN << "[RAS_FAULT_NOTIFY] current node is not local master";
            return UBSE_ERROR;
        }

        auto faultNode = nodeMgr::GetUbseNodeById(info.nodeId);
        auto currentNode = nodeMgr::GetCurrentNode();
        if (faultNode.nodeId.empty() || currentNode.nodeId.empty()) {
            UBSE_LOG_WARN << "[RAS_FAULT_NOTIFY] static node info not found, nodeId=" << info.nodeId;
            return UBSE_ERROR_NULLPTR;
        }
        if (info.clusterState != UbseNodeClusterState::UBSE_NODE_FAULT ||
            faultNode.groupId != currentNode.groupId || info.groupId != currentNode.groupId) {
            UBSE_LOG_WARN << "[RAS_FAULT_NOTIFY] invalid fault notification, nodeId=" << info.nodeId;
            return UBSE_ERROR_INVAL;
        }

        UBSE_LOG_INFO << "[RAS_FAULT_NOTIFY] receive fault notification, nodeId=" << info.nodeId
                      << ", currentNodeId=" << currentNode.nodeId;
        return UbseNodeRasFaultHandler(info.nodeId);
    }

    auto role = GetClosRole();

    UbseNodeInfo nodeInfo = info;
    auto ret = UbseNodeController::GetInstance().UpdateNodeInfo(nodeInfo.nodeId, nodeInfo);
    if (ret == UBSE_OK) {
        (void)ProcessGlobalStateAfterReport(nodeInfo.nodeId);
    }

    UBSE_LOG_INFO << "[CLOS_RECV] receive single node change, currentNodeId="
                  << UbseNodeController::GetInstance().GetCurrentNodeId() << ", role=" << static_cast<uint32_t>(role)
                  << ", nodeId=" << nodeInfo.nodeId << ", groupId=" << nodeInfo.groupId
                  << ", reportClusterState=" << static_cast<uint32_t>(info.clusterState)
                  << ", cachedClusterState=" << static_cast<uint32_t>(nodeInfo.clusterState);

    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[CLOS_RECV] update single node failed, nodeId=" << info.nodeId << ", " << FormatRetCode(ret);
        return ret;
    }

    if (role != UbseClosNodeRole::PD_MASTER) {
        return UBSE_OK;
    }

    UbseRoleInfo roleInfo{};
    ret =  UbseGetMasterInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[CLOS_REPORT] pd master get global prev failed, nodeId=" << nodeInfo.nodeId << ", "
                      << FormatRetCode(ret);
        return ret;
    }
    std::string globalMasterNodeId = roleInfo.nodeId;

    if (globalMasterNodeId == UbseNodeController::GetInstance().GetCurrentNodeId()) {
        return UBSE_OK;
    }

    UBSE_LOG_INFO << "[CLOS_REPORT] pd master forward single node to global, nodeId=" << nodeInfo.nodeId
                  << ", groupId=" << nodeInfo.groupId << ", clusterState=" << static_cast<uint32_t>(nodeInfo.clusterState)
                  << ", globalMasterNodeId=" << globalMasterNodeId;

    return UbseGlobalReportSingleNode(globalMasterNodeId, nodeInfo);
}

UbseResult UbseNodeControllerMaster::ProcessGlobalStateAfterReport(const std::string &nodeId)
{
    if (!IsGlobalMaster()) {
        return UBSE_OK;
    }

    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_WARN << "[GLOBAL_STATE] node not found, skip process global state, nodeId=" << nodeId;
        return UBSE_ERROR_NULLPTR;
    }

    if (nodeInfo.globalState == UbseNodeGlobalState::UBSE_NODE_GLOBAL_READY) {
        return UBSE_OK;
    }

    auto curNodeInfo = UbseNodeController::GetInstance().GetCurNode();
    if (curNodeInfo.nodeId.empty()) {
        UBSE_LOG_WARN << "[GLOBAL_STATE] current node info is empty, skip process global state, nodeId=" << nodeId;
        return UBSE_ERROR_NULLPTR;
    }

    if (nodeInfo.groupId == curNodeInfo.groupId) {
        UBSE_LOG_INFO << "[GLOBAL_STATE] node is in the same cabinet with global master, set ready directly, nodeId="
                      << nodeId << ", groupId=" << nodeInfo.groupId;
        return UbseNodeController::GetInstance().UpdateNodeInfoGlobalState(nodeId,
                                                                           UbseNodeGlobalState::UBSE_NODE_GLOBAL_READY);
    }

    UBSE_LOG_INFO << "[GLOBAL_STATE] node global state init, start recovery callback, nodeId=" << nodeId
                  << ", groupId=" << nodeInfo.groupId << ", globalMasterGroupId=" << curNodeInfo.groupId;

    auto ret = UbseNodeController::GetInstance().ExecGlobalStateNotifyHandler(nodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[GLOBAL_STATE] recovery callback failed, keep init, nodeId=" << nodeId << ", "
                      << FormatRetCode(ret);
        return ret;
    }

    UBSE_LOG_INFO << "[GLOBAL_STATE] recovery callback success, set ready, nodeId=" << nodeId;
    return UbseNodeController::GetInstance().UpdateNodeInfoGlobalState(nodeId,
                                                                       UbseNodeGlobalState::UBSE_NODE_GLOBAL_READY);
}

UbseResult UbseNodeControllerMaster::ReportSingleNodeChangeToPrev(const std::string &nodeId, const std::string &reason)
{
    auto role = GetClosRole();
    if (role == UbseClosNodeRole::GLOBAL_MASTER) {
        UBSE_LOG_INFO << "[CLOS_REPORT] skip report to prev on global master, nodeId=" << nodeId
                      << ", reason=" << reason;
        return UBSE_OK;
    }

    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_WARN << "[CLOS_REPORT] node not found, skip single node report, nodeId=" << nodeId
                      << ", reason=" << reason;
        return UBSE_ERROR_NULLPTR;
    }

    UbseRoleInfo roleInfo{};
    auto ret =  UbseGetMasterInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[CLOS_REPORT] get prev node failed, nodeId=" << nodeId << ", reason=" << reason << ", "
                      << FormatRetCode(ret);
        return ret;
    }
    std::string globalMasterNodeId = roleInfo.nodeId;

    if (globalMasterNodeId == UbseNodeController::GetInstance().GetCurrentNodeId()) {
        UBSE_LOG_INFO << "[CLOS_REPORT] skip report to self, nodeId=" << nodeId << ", reason=" << reason
                      << ", globalMasterNodeId=" << globalMasterNodeId;
        return UBSE_OK;
    }

    UBSE_LOG_INFO << "[CLOS_REPORT] report single node change to prev, nodeId=" << nodeId
                  << ", groupId=" << nodeInfo.groupId << ", clusterState=" << static_cast<uint32_t>(nodeInfo.clusterState)
                  << ", reason=" << reason << ", globalMasterNodeId=" << globalMasterNodeId << ", role=" << static_cast<uint32_t>(role);

    if (role == UbseClosNodeRole::PD_MASTER) {
        return UbseGlobalReportSingleNode(globalMasterNodeId, nodeInfo);
    }

    return UbseCabinetReportSingleNode(globalMasterNodeId, nodeInfo);
}

// 处理故障计数器的辅助函数
int UbseNodeControllerMaster::ProcessFaultCounter(const std::string &nodeId)
{
    std::lock_guard<std::mutex> lock(faultCountersMutex_);
    auto it = faultReportCounters_.find(nodeId);
    if (it == faultReportCounters_.end()) {
        return -1; // 计数器不存在
    }

    // 增加计数
    int newCount = ++it->second;

    // 达到阈值时清除计数器
    if (newCount >= FAULT_REPORT_THRESHOLD) {
        faultReportCounters_.erase(it);
    }

    return newCount;
}

UbseResult UbseNodeControllerMaster::UbseLcneTopologyChangeHandler(const UbseNodeInfo &nodeInfo)
{
    UBSE_LOG_INFO << "nodeId=" << nodeInfo.nodeId << ", lcne topology change, msg=" << nodeInfo.eventMessage;

    if (nodeInfo.nodeId.empty()) {
        return UBSE_ERROR_INVAL;
    }

    auto oldNodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeInfo.nodeId);
    bool hasPortUp = HasSubHealthPortUpChange(oldNodeInfo, nodeInfo);

    // 如果需要，创建临时变量
    UbseNodeInfo nodeInfoCopy = nodeInfo;
    auto ret = UbseNodeController::GetInstance().UpdateNodeInfo(nodeInfoCopy.nodeId, nodeInfoCopy);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << nodeInfo.nodeId << " update lcne topology failed, " << FormatRetCode(ret);
        return ret;
    }

    UbseNodeController::GetInstance().UpdateDevDirConnectInfo();

    if (hasPortUp) {
        UBSE_LOG_INFO << "[SUB_HEALTH] lcne topology up, trigger refresh, nodeId=" << nodeInfo.nodeId;
        UbseNodeController::GetInstance().TriggerSubHealthRefresh();
    }

    // 创建临时变量给UbsePubEvent
    std::string eventMessageCopy = nodeInfo.eventMessage;
    ret = UbsePubEvent(LCNE_CHANGE_REPORT_EVENT, eventMessageCopy);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbsePubEvent failed";
        return ret;
    }
    UbseMasterNotifyAllAgentsAction(nodeInfoCopy.nodeId, UBSE_EVENT_NODE_TOPO_LINK_CHANGE);

    return UBSE_OK;
}

void UbseNodeControllerMaster::UbseNodeUpHandlerExec(const std::string& nodeId)
{
    UBSE_LOG_INFO << "nodeId=" << nodeId << " node up, start to collect topology.";
    UbseNodeInfo info{};
    UbseResult ret = UBSE_OK;
    // 主节点采集拓扑成功后，才启动对账。
    for (int i = 0; i < UBSE_COLLECT_TOPOLOGY_RETRY_TIMES; i++) {
        ret = CollectRemoteNodeInfo(nodeId, info);
        if (ret == UBSE_OK) {
            UbseNodeController::GetInstance().UpdateNodeInfo(nodeId, info);
            UbseNodeController::GetInstance().UpdateDevDirConnectInfo();
            break;
        }
        UBSE_LOG_WARN << "nodeId=" << nodeId << " collect topology failed, will retry, " << FormatRetCode(ret);
        std::this_thread::sleep_for(std::chrono::milliseconds(UBSE_COLLECT_TOPOLOGY_RETRY_INTERVAL));
    }
    UbseMasterNotifyAllAgentsAction(nodeId, UBSE_EVENT_NODE_JOIN);

    UbseNodeUpLedger(nodeId);

    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    UBSE_LOG_INFO << "[CLOS_REPORT] node up report trigger, nodeId=" << nodeId << ", groupId=" << nodeInfo.groupId
                  << ", clusterState=" << static_cast<uint32_t>(nodeInfo.clusterState);

    auto reportRet = ReportSingleNodeChangeToPrev(nodeId, "node up");
    if (reportRet != UBSE_OK) {
        UBSE_LOG_WARN << "[CLOS_REPORT] report node up change failed, nodeId=" << nodeId << ", "
                      << FormatRetCode(reportRet);
    }
}

UbseResult UbseNodeControllerMaster::UbseNodeUpHandler(const std::string &nodeId)
{
    // 持续等待直到 taskExecutor_ 初始化完成
    while (taskExecutor_ == nullptr) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    // taskExecutor_ 初始化完成
    std::lock_guard<std::mutex> lock(taskExecMutex_);
    UBSE_LOG_INFO << "taskExecutor_ initialized successfully, processing node up, nodeId=" << nodeId;
    taskExecutor_->Execute([this, nodeId]() -> void { UbseNodeUpHandlerExec(nodeId); });
    return UBSE_OK;
}

void UbseNodeControllerMaster::UbseNodeUpLedger(const std::string &nodeId)
{
    UbseNodeControllerLockMgr::WriteLock(nodeId);
    UBSE_LOG_INFO << "nodeId=" << nodeId << " node up, start to collect ledger.";
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " not report, will skip";
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    UBSE_LOG_INFO << "nodeId=" << nodeId
                  << " node up, before collect ledger current state=" << static_cast<uint32_t>(nodeInfo.clusterState);
    // 预下电，故障，断连等异常场景不进行对账；
    // smoothing 表示节点已经在对账流程中，不对账；
    // init为初始静态数据 或者 节点首次上报，等待节点上线事件触发后启动对账
    // 已进入故障上报计数流程的节点，等待150次上报，避免上线事件提前触发对账。
    bool waitingFaultReports = false;
    if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
        std::lock_guard<std::mutex> lock(faultCountersMutex_);
        waitingFaultReports = faultReportCounters_.find(nodeId) != faultReportCounters_.end();
    }

    if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_SMOOTHING || waitingFaultReports) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " node up, smoothing or waiting fault reports, skip ledger";
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    UbseNodeLedger(nodeId);
    UbseNodeControllerLockMgr::WriteUnLock(nodeId);
}

UbseResult FindCasCadeGroupId(uint32_t groupSize, uint32_t pdGroupId, uint32_t &cascadeGroupId) {
    if (pdGroupId == 0 || pdGroupId > groupSize) {
        return UBSE_ERROR;
    }
    uint32_t half = groupSize / 2;
    if (pdGroupId > half) {
        return UBSE_ERROR;
    }
    cascadeGroupId = pdGroupId + (groupSize - half);
    return UBSE_OK;
}

UbseResult UbseNodeControllerMaster::UbseCasCadeNodeDownHandler(const std::string& nodeId)
{
    UBSE_LOG_INFO << "[CLOS_EVENT] cascade node down, cascade nodeId=" << nodeId;
    UbseRoleInfo globalMasterInfo{};
    auto ret = UbseGetMasterInfo(globalMasterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[CLOS_EVENT] get global master failed, " << FormatRetCode(ret);
        return ret;
    }
    if (globalMasterInfo.nodeId == UbseNodeController::GetInstance().GetCurrentNodeId()) {
        auto cascadeNode = nodeMgr::GetUbseNodeById(nodeId);
        if (cascadeNode.nodeId.empty()) {
            UBSE_LOG_INFO << "[CLOS_EVENT] cascade node down, cascade nodeId=" << nodeId << " not found.";
            return UBSE_ERROR_NULLPTR;
        }
        auto nodes = nodeMgr::GetNodesByGroupId(cascadeNode.groupId);
        for (const auto& node : nodes) {
            UBSE_LOG_INFO << "[CLOS_EVENT] set cascade cabinet down, cascade nodeId=" << node.nodeId;
            // FAULT 节点不允许直接迁到 UNKNOWN，会被状态机拒绝；这是预期行为（FAULT 比 UNKNOWN 更严重）
            ret = UbseNodeController::GetInstance().UpdateNodeInfoClusterState(node.nodeId,
                                                                               UbseNodeClusterState::UBSE_NODE_UNKNOWN);
            if (ret != UBSE_OK) {
                UBSE_LOG_INFO << "[CLOS_EVENT] set UNKNOWN rejected (state machine), nodeId=" << node.nodeId << ", "
                              << FormatRetCode(ret);
            }
        }
        return UBSE_OK;
    }
    return UbseReportCasCadeNodeDown(globalMasterInfo.nodeId, UbseNodeInfo{.nodeId = nodeId});
}

UbseResult UbseNodeControllerMaster::UbseGlobalNodeDownHandler(const std::string& nodeId)
{
    UBSE_LOG_INFO << "[CLOS_EVENT] global pd node down, pd nodeId=" << nodeId;
    UbseRoleInfo globalMasterInfo{};
    auto ret = UbseGetMasterInfo(globalMasterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[CLOS_EVENT] get global master failed, " << FormatRetCode(ret);
        return ret;
    }
    if (globalMasterInfo.nodeId == UbseNodeController::GetInstance().GetCurrentNodeId()) {
        auto pdNode = nodeMgr::GetUbseNodeById(nodeId);
        if (pdNode.nodeId.empty()) {
            UBSE_LOG_INFO << "[CLOS_EVENT] pd node down, pd nodeId=" << nodeId << " not found.";
            return UBSE_ERROR_NULLPTR;
        }
        auto nodes = nodeMgr::GetNodesByGroupId(pdNode.groupId);
        for (const auto& node : nodes) {
            UBSE_LOG_INFO << "[CLOS_EVENT] set pd cabinet down, pd nodeId=" << node.nodeId;
            UbseNodeController::GetInstance().UpdateNodeInfoClusterState(node.nodeId,
                                                                         UbseNodeClusterState::UBSE_NODE_UNKNOWN);
        }
        uint32_t cascadeGroupId = 0;
        ret = FindCasCadeGroupId(nodeMgr::GetAllNodesStoredByGroup().size(), pdNode.groupId, cascadeGroupId);
        if (ret != UBSE_OK) {
            UBSE_LOG_INFO << "[CLOS_EVENT] pd group=" << pdNode.groupId << " has no cascade group";
            return UBSE_OK;
        }
        nodes = nodeMgr::GetNodesByGroupId(cascadeGroupId);
        for (const auto& node : nodes) {
            UBSE_LOG_INFO << "[CLOS_EVENT] set cascade cabinet down, cascade nodeId=" << node.nodeId;
            UbseNodeController::GetInstance().UpdateNodeInfoClusterState(node.nodeId,
                                                                         UbseNodeClusterState::UBSE_NODE_UNKNOWN);
        }
    }
    return UBSE_OK;
}

UbseResult UbseNodeControllerMaster::UbseNodeDownHandler(const std::string &nodeId)
{
    UBSE_LOG_INFO << "[CLOS_EVENT] node down, nodeId=" << nodeId;

    std::unordered_set<std::string> affectedNodeIds{};
    affectedNodeIds.insert(nodeId);

    ubse::election::HaTopologyInfo topology{};
    auto topologyRet = GetClosHaTopology(topology);
    if (topologyRet != UBSE_OK) {
        UBSE_LOG_WARN << "[CLOS_EVENT] get ha topology failed, only update down node, nodeId=" << nodeId << ", "
                      << FormatRetCode(topologyRet);
    } else {
        auto group = FindGroupByNodeId(topology.groups, nodeId);
        if (group == nullptr) {
            UBSE_LOG_WARN << "[CLOS_EVENT] down node group not found, only update down node, nodeId=" << nodeId;
        } else if (group->groupMasterId == nodeId) {
            if (group->isManagingGroup) {
                UBSE_LOG_INFO << "[CLOS_EVENT] pd master down, update managing group and mounted groups, nodeId="
                              << nodeId << ", groupId=" << group->groupId;
                CollectGroupNodeIds(*group, true, topology.groups, affectedNodeIds);
            } else {
                UBSE_LOG_INFO << "[CLOS_EVENT] cabinet master down, update cabinet group, nodeId=" << nodeId
                              << ", groupId=" << group->groupId;
                CollectGroupNodeIds(*group, false, topology.groups, affectedNodeIds);
            }
        } else {
            UBSE_LOG_INFO << "[CLOS_EVENT] ordinary node down, only update node, nodeId=" << nodeId
                          << ", groupId=" << group->groupId;
        }
    }

    UbseResult finalRet = UBSE_OK;
    for (const auto &affectedNodeId : affectedNodeIds) {
        auto ret = UbseNodeController::GetInstance().UpdateNodeInfoClusterState(
            affectedNodeId, UbseNodeClusterState::UBSE_NODE_UNKNOWN);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "[CLOS_EVENT] update node down state failed, nodeId=" << affectedNodeId << ", "
                          << FormatRetCode(ret);
            finalRet = ret;
            continue;
        }

        auto reportRet = ReportSingleNodeChangeToPrev(affectedNodeId, "node down");
        if (reportRet != UBSE_OK) {
            UBSE_LOG_WARN << "[CLOS_REPORT] report node down change failed, nodeId=" << affectedNodeId << ", "
                          << FormatRetCode(reportRet);
            finalRet = reportRet;
        }
    }

    return finalRet;
}

UbseResult UbseNodeControllerMaster::UbseNodeRasPreFaultHandler(const std::string &nodeId)
{
    UBSE_LOG_INFO << "nodeId=" << nodeId << ", start pre bmc";
    // 预下电必须保证节点连通；当前仅smoothing和working状态支持切换到预下电状态
    auto ret =
        UbseNodeController::GetInstance().UpdateNodeInfoClusterState(nodeId, UbseNodeClusterState::UBSE_NODE_PRE_BMC);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[CLOS_REPORT] update ras pre fault state failed, nodeId=" << nodeId << ", "
                      << FormatRetCode(ret);
        return ret;
    }

    auto reportRet = ReportSingleNodeChangeToPrev(nodeId, "ras pre fault");
    if (reportRet != UBSE_OK) {
        UBSE_LOG_WARN << "[CLOS_REPORT] report ras pre fault change failed, nodeId=" << nodeId << ", "
                      << FormatRetCode(reportRet);
    }

    return reportRet;
}

UbseResult UbseNodeControllerMaster::UbseNodeRasPreFaultFailHandler(const std::string &nodeId)
{
    if (UbseNodeController::GetInstance().GetNodeById(nodeId).clusterState != UbseNodeClusterState::UBSE_NODE_PRE_BMC) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " is not in pre fault, cannot smoothing.";
        return UBSE_OK;
    }

    UBSE_LOG_INFO << "nodeId=" << nodeId << ", pre bmc fail, start to smoothing.";
    std::lock_guard<std::mutex> lock(taskExecMutex_);
    if (taskExecutor_ != nullptr) {
        // 纳入per-node锁纪律，与镜像消费/周期/重试/NODE_UP对账路径互斥
        taskExecutor_->Execute([this, nodeId]() -> void {
            UbseNodeControllerLockMgr::WriteLock(nodeId);
            UbseNodeLedger(nodeId);

            auto reportRet = ReportSingleNodeChangeToPrev(nodeId, "ras pre fault fail");
            if (reportRet != UBSE_OK) {
                UBSE_LOG_WARN << "[CLOS_REPORT] report ras pre fault fail change failed, nodeId=" << nodeId << ", "
                              << FormatRetCode(reportRet);
            }
            UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        });
    }

    return UBSE_OK;
}

UbseResult UbseNodeControllerMaster::UbseNodeRasFaultHandler(const std::string &nodeId)
{
    if (nodeId.empty()) {
        return UBSE_ERROR_INVAL;
    }

    // 全局主收到其他机柜节点的故障事件时，通知对应机柜主。
    if (IsHierarchicalElection() && GetClosRole() == UbseClosNodeRole::GLOBAL_MASTER) {
        auto faultNode = nodeMgr::GetUbseNodeById(nodeId);
        auto currentNode = nodeMgr::GetCurrentNode();
        if (faultNode.nodeId.empty() || currentNode.nodeId.empty()) {
            UBSE_LOG_WARN << "[RAS_FAULT_NOTIFY] static node info not found, nodeId=" << nodeId;
            return UBSE_ERROR_NULLPTR;
        }

        if (faultNode.groupId != currentNode.groupId) {
            HaTopologyInfo topology{};
            auto topologyRet = GetClosHaTopology(topology);
            if (topologyRet != UBSE_OK) {
                UBSE_LOG_WARN << "[RAS_FAULT_NOTIFY] get topology failed, nodeId=" << nodeId
                              << ", " << FormatRetCode(topologyRet);
                return topologyRet;
            }

            // 静态节点的groupId是整数，选主拓扑的groupId是字符串。
            // 按机柜号查找，节点故障后即使已从选主成员列表移除，也可以定位机柜主。
            const std::string groupId = std::to_string(faultNode.groupId);
            const GroupTopology *targetGroup = nullptr;
            if (topology.currentGroup.groupId == groupId) {
                targetGroup = &topology.currentGroup;
            } else {
                for (const auto &group : topology.groups) {
                    if (group.groupId == groupId) {
                        targetGroup = &group;
                        break;
                    }
                }
            }

            if (targetGroup == nullptr || targetGroup->groupMasterId.empty()) {
                UBSE_LOG_WARN << "[RAS_FAULT_NOTIFY] cabinet master not found, nodeId=" << nodeId
                              << ", groupId=" << groupId;
                return UBSE_ERROR;
            }
            if (targetGroup->groupMasterId == currentNode.nodeId) {
                UBSE_LOG_WARN << "[RAS_FAULT_NOTIFY] unexpected cabinet master, nodeId=" << nodeId
                              << ", groupId=" << groupId;
                return UBSE_ERROR;
            }

            UbseNodeInfo faultInfo{};
            faultInfo.nodeId = nodeId;
            faultInfo.groupId = faultNode.groupId;
            faultInfo.clusterState = UbseNodeClusterState::UBSE_NODE_FAULT;
            faultInfo.eventMessage = UBSE_NODE_RAS_FAULT_NOTIFY;

            UBSE_LOG_INFO << "[RAS_FAULT_NOTIFY] send fault notification, nodeId=" << nodeId
                          << ", cabinetMasterId=" << targetGroup->groupMasterId
                          << ", groupId=" << groupId;

            auto notifyRet = UbseCabinetReportSingleNode(targetGroup->groupMasterId, faultInfo);
            if (notifyRet != UBSE_OK) {
                UBSE_LOG_WARN << "[RAS_FAULT_NOTIFY] send fault notification failed, nodeId=" << nodeId
                              << ", " << FormatRetCode(notifyRet);
            }
            return notifyRet;
        }
    }

    UBSE_LOG_INFO << "nodeId=" << nodeId << ", start fault";

    // 清除旧的故障计数器
    ClearFaultCounter(nodeId);

    // 初始化故障计数器，由本机柜主累计该节点后续的150次周期上报。
    {
        std::lock_guard<std::mutex> lock(faultCountersMutex_);
        faultReportCounters_[nodeId] = 0;
        UBSE_LOG_INFO << "[RAS_FAULT_NOTIFY] initialize fault counter, nodeId=" << nodeId
                      << ", currentNodeId=" << UbseNodeController::GetInstance().GetCurrentNodeId();
    }

    auto ret =
        UbseNodeController::GetInstance().UpdateNodeInfoClusterState(nodeId, UbseNodeClusterState::UBSE_NODE_FAULT);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[CLOS_REPORT] update ras fault state failed, nodeId=" << nodeId << ", " << FormatRetCode(ret);
        return ret;
    }

    auto reportRet = ReportSingleNodeChangeToPrev(nodeId, "ras fault");
    if (reportRet != UBSE_OK) {
        UBSE_LOG_WARN << "[CLOS_REPORT] report ras fault change failed, nodeId=" << nodeId << ", "
                      << FormatRetCode(reportRet);
    }

    return reportRet;
}

UbseResult UbseNodeControllerMaster::UbseNodeRasAfterFaultClearHandler(const std::string &nodeId)
{
    UBSE_LOG_INFO << "nodeId=" << nodeId << ", after fault, start to ledger";

    std::lock_guard<std::mutex> lock(taskExecMutex_);
    if (taskExecutor_ != nullptr) {
        // 纳入per-node锁纪律，与镜像消费/周期/重试/NODE_UP对账路径互斥
        taskExecutor_->Execute([this, nodeId]() -> void {
            UbseNodeControllerLockMgr::WriteLock(nodeId);
            UbseNodeLedger(nodeId);

            auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
            UBSE_LOG_INFO << "[CLOS_REPORT] ras fault clear report trigger, nodeId=" << nodeId
                          << ", groupId=" << nodeInfo.groupId
                          << ", clusterState=" << static_cast<uint32_t>(nodeInfo.clusterState);

            auto reportRet = ReportSingleNodeChangeToPrev(nodeId, "ras fault clear");
            if (reportRet != UBSE_OK) {
                UBSE_LOG_WARN << "[CLOS_REPORT] report ras fault clear change failed, nodeId=" << nodeId << ", "
                              << FormatRetCode(reportRet);
            }
            UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        });
    }

    return UBSE_OK;
}

void UbseNodeControllerMaster::UbseNodeCleanAfterSwitchStandby()
{
    isLogAggregationRunning_.store(false);
    cv_.notify_all();
    // 停止定时器
    UBSE_LOG_INFO << "ubse node master start to stop ledger timer.";
    UbseTimerHandlerUnregister(UBSE_NODE_MASTER_LEDGER_TIMER);
    // 主备全量快照定时器: 降备/退出必须解除注册。残留会使timer框架活到静态析构,
    // check线程joinable析构触发std::terminate/coredump, 且fire会访问已析构对象
    UbseTimerHandlerUnregister(UBSE_NODE_SYNC_FULL_TIMER);
    // 重试定时器按记账集合精确清理(覆盖任意group的节点, 消除枚举拼名对不上的残留)
    std::vector<std::string> retryTimers;
    {
        std::lock_guard<std::mutex> lock(g_ledgerRetryTimerMutex);
        retryTimers.assign(g_ledgerRetryTimers.begin(), g_ledgerRetryTimers.end());
        g_ledgerRetryTimers.clear();
    }
    for (const auto& retryTimerName : retryTimers) {
        UbseTimerHandlerUnregister(retryTimerName);
    }
    UBSE_LOG_INFO << "ubse node master start to stop executor.";
    std::lock_guard<std::mutex> lock(taskExecMutex_);
    if (taskExecutor_ != nullptr) {
        taskExecutor_->Stop();
    }
    // 清理内存中其余节点的信息
    UBSE_LOG_INFO << "ubse node master start to clean context.";
    UbseNodeController::GetInstance().CleanAfterMasterSwitchRole();
    UBSE_LOG_INFO << "ubse node master clean done.";
}

void UbseNodeControllerMaster::Stop()
{
    UbseNodeCleanAfterSwitchStandby();
}

void UbseNodeControllerMaster::UnInitialize()
{
    // 解注册主上线回调
    UbseElectionHandlerBuilder Builder;
    Builder.SetHandler([this](UbseElectionEventType, UBSE_ID_TYPE nodeId) { return UbseMasterOnlineHandler(nodeId); });
    Builder.SetPriority(UbseElectionHandlerPriority::HIGH);
    Builder.SetSequenceId(HA_SEQUENCE_ID); // 需要确保在节点建链后触发，节点建链优先级100
    Builder.SetType(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION);
    Builder.SetName(UBSE_NODE_MASTER_ONLINE);
    UbseElectionChangeDeAttachHandler(Builder.Build());
    // 解注册节点上线回调
    UbseElectionHandlerBuilder NodeUpBuilder;
    NodeUpBuilder.SetHandler([this](UbseElectionEventType, UBSE_ID_TYPE nodeId) { return UbseNodeUpHandler(nodeId); });
    NodeUpBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeUpBuilder.SetSequenceId(HA_SEQUENCE_ID); // 需要确保在节点建链后触发，节点建链优先级100
    NodeUpBuilder.SetType(UbseElectionEventType::NODE_UP);
    NodeUpBuilder.SetName(UBSE_NODE_NODE_UP);
    UbseElectionChangeDeAttachHandler(NodeUpBuilder.Build());

    // 解注册节点下线回调
    UbseElectionHandlerBuilder NodeDownBuilder;
    NodeDownBuilder.SetHandler(
        [this](UbseElectionEventType, UBSE_ID_TYPE nodeId) { return UbseNodeDownHandler(nodeId); });
    NodeDownBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeDownBuilder.SetSequenceId(HA_SEQUENCE_ID); // 需要确保在节点建链后触发，节点建链优先级100
    NodeDownBuilder.SetType(UbseElectionEventType::NODE_DOWN);
    NodeDownBuilder.SetName(UBSE_NODE_NODE_DOWN);
    UbseElectionChangeDeAttachHandler(NodeDownBuilder.Build());

    UbseElectionHandlerBuilder CasCadeNodeDownBuilder;
    CasCadeNodeDownBuilder.SetHandler(
        [this](UbseElectionEventType, UBSE_ID_TYPE nodeId) { return UbseCasCadeNodeDownHandler(nodeId); });
    CasCadeNodeDownBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    CasCadeNodeDownBuilder.SetSequenceId(HA_SEQUENCE_ID);
    CasCadeNodeDownBuilder.SetType(UbseElectionEventType::GLOBAL_CASCADE_NODE_DOWN);
    CasCadeNodeDownBuilder.SetName(UBSE_CASCADE_NODE_NODE_DOWN);
    UbseElectionChangeDeAttachHandler(CasCadeNodeDownBuilder.Build());

    UbseElectionHandlerBuilder GlobalNodeDownBuilder;
    GlobalNodeDownBuilder.SetHandler(
        [this](UbseElectionEventType, UBSE_ID_TYPE nodeId) { return UbseGlobalNodeDownHandler(nodeId); });
    GlobalNodeDownBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    GlobalNodeDownBuilder.SetSequenceId(HA_SEQUENCE_ID);
    GlobalNodeDownBuilder.SetType(UbseElectionEventType::GLOBAL_NODE_DOWN);
    GlobalNodeDownBuilder.SetName(UBSE_GLOBAL_NODE_DOWN);
    UbseElectionChangeDeAttachHandler(GlobalNodeDownBuilder.Build());

    UbseElectionHandlerBuilder GlobalMasterOnlineBuilder;
    GlobalMasterOnlineBuilder.SetHandler(
        [this](UbseElectionEventType, UBSE_ID_TYPE globalMasterId) {
            return UbseGlobalMasterOnlineHandler(globalMasterId);
        });
    GlobalMasterOnlineBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    GlobalMasterOnlineBuilder.SetSequenceId(HA_SEQUENCE_ID);
    GlobalMasterOnlineBuilder.SetType(UbseElectionEventType::GLOBAL_MASTER_ONLINE_NOTIFICATION);
    GlobalMasterOnlineBuilder.SetName(UBSE_NODE_GLOBAL_MASTER_ONLINE);
    UbseElectionChangeDeAttachHandler(GlobalMasterOnlineBuilder.Build());
}

// 创建错误响应
static UbseResult CreateErrorResponse(UbseResult errorCode, UbseByteBuffer &resp)
{
    uint8_t *errorBuffer = new (std::nothrow) uint8_t[4];
    if (errorBuffer != nullptr) {
        *reinterpret_cast<uint32_t *>(errorBuffer) = static_cast<uint32_t>(errorCode);
        resp = {errorBuffer, 4, [](uint8_t *p) noexcept {
                    delete[] p;
                }};
        return errorCode;
    } else {
        resp = {nullptr, 0, nullptr}; // 内存分配失败，只能返回空
        return UBSE_ERROR_NULLPTR;
    }
}

struct ClusterDisplayNode {
    UbseNodeInfo nodeInfo;
    std::string role;
};

static bool IsClusterNodeOnline(const UbseNodeInfo& nodeInfo)
{
    return nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_SMOOTHING ||
           nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_WORKING;
}

static void SortClusterDisplayNodes(std::vector<ClusterDisplayNode>& displayNodes)
{
    std::sort(displayNodes.begin(), displayNodes.end(),
              [](const ClusterDisplayNode& left, const ClusterDisplayNode& right) {
                  return left.nodeInfo.slotId < right.nodeInfo.slotId;
              });
}

static UbseResult BuildLocalClusterDisplayNodes(std::vector<ClusterDisplayNode>& displayNodes)
{
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "[CLUSTER_QUERY] election module not load";
        return UBSE_ERROR_NULLPTR;
    }

    Node localMaster{};
    auto ret = module->GetLocalMasterNode(localMaster);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[CLUSTER_QUERY] get local master failed, " << FormatRetCode(ret);
        return ret;
    }

    Node localStandby{};
    ret = module->GetLocalStandbyNode(localStandby);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[CLUSTER_QUERY] get local standby failed, " << FormatRetCode(ret);
        localStandby.id.clear();
    }

    uint32_t currentGroupId = GetGroupId();
    auto nodeInfos = UbseNodeController::GetInstance().GetLocalNodeInfos();

    for (const auto& [_, nodeInfo] : nodeInfos) {
        if (nodeInfo.groupId != currentGroupId) {
            continue;
        }

        std::string role = UBSE_ROLE_AGENT;
        if (nodeInfo.nodeId == localMaster.id) {
            role = UBSE_ROLE_MASTER;
        } else if (!localStandby.id.empty() && nodeInfo.nodeId == localStandby.id) {
            role = UBSE_ROLE_SLAVE;
        }

        displayNodes.push_back({nodeInfo, role});
    }

    // 补充静态节点信息：配置存在但尚未上报到内存的本组节点（nodeMgr 按 groupId 直接获取，避免全量 static info 再过滤）
    auto staticNodes = nodeMgr::GetNodesByGroupId(static_cast<uint16_t>(currentGroupId));
    for (const auto& staticNode : staticNodes) {
        UBSE_LOG_INFO << "[CLUSTER_QUERY] static node=" << staticNode.nodeId;
        if (nodeInfos.find(staticNode.nodeId) != nodeInfos.end()) {
            continue;
        }
        UbseNodeInfo nodeInfo{staticNode.nodeId};
        nodeInfo.groupId = staticNode.groupId;
        (void)ubse::utils::ConvertStrToUint32(staticNode.nodeId, nodeInfo.slotId);
        (void)strcpy_s(nodeInfo.bondingEid, sizeof(nodeInfo.bondingEid), staticNode.bonding0Eid.c_str());
        displayNodes.push_back({nodeInfo, UBSE_ROLE_AGENT});
    }

    SortClusterDisplayNodes(displayNodes);
    return UBSE_OK;
}

static void AddGroupMasterId(const GroupTopology& group, std::unordered_set<std::string>& groupMasterIds)
{
    if (!group.groupMasterId.empty()) {
        groupMasterIds.insert(group.groupMasterId);
    }
}

static UbseResult BuildGlobalClusterDisplayNodes(std::vector<ClusterDisplayNode>& displayNodes)
{
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "[CLUSTER_QUERY] election module not load";
        return UBSE_ERROR_NULLPTR;
    }

    HaTopologyInfo topology{};
    auto ret = module->GetCurNodeGlobalTopoInfo(topology);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[CLUSTER_QUERY] get global topology failed, " << FormatRetCode(ret);
        return ret;
    }

    Node globalMaster{};
    ret = module->UbseGetMasterNode(globalMaster);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[CLUSTER_QUERY] get global master failed, " << FormatRetCode(ret);
        return ret;
    }

    Node globalStandby{};
    ret = module->UbseGetStandbyNode(globalStandby);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[CLUSTER_QUERY] get global standby failed, " << FormatRetCode(ret);
        globalStandby.id.clear();
    }

    std::unordered_map<std::string, bool> groupMasters{};

    auto addGroupMaster = [&groupMasters](const GroupTopology& group) {
        if (group.groupMasterId.empty()) {
            return;
        }

        auto iter = groupMasters.find(group.groupMasterId);
        if (iter == groupMasters.end()) {
            groupMasters.emplace(group.groupMasterId, group.isManagingGroup);
            return;
        }

        // 同一节点重复出现时，管理组属性优先保留
        iter->second = iter->second || group.isManagingGroup;
    };

    addGroupMaster(topology.currentGroup);
    for (const auto& group : topology.groups) {
        addGroupMaster(group);
    }

    auto nodeInfos = UbseNodeController::GetInstance().GetLocalNodeInfos();
    for (const auto& [groupMasterId, isManagingGroup] : groupMasters) {
        auto iter = nodeInfos.find(groupMasterId);
        if (iter == nodeInfos.end()) {
            UBSE_LOG_WARN << "[CLUSTER_QUERY] group master not found in node cache, nodeId=" << groupMasterId;
            continue;
        }

        std::string role;
        if (groupMasterId == globalMaster.id) {
            role = "global master";
        } else if (!globalStandby.id.empty() && groupMasterId == globalStandby.id) {
            role = "global standby";
        } else if (isManagingGroup) {
            role = "global agent";
        } else {
            role = "global cascade";
        }

        displayNodes.push_back({iter->second, role});
    }

    SortClusterDisplayNodes(displayNodes);
    return UBSE_OK;
}

static UbseResult SerializeClusterDisplayNodes(const std::vector<ClusterDisplayNode>& displayNodes,
                                               UbseByteBuffer& resp)
{
    UbseSerialization outStream;
    outStream << right_v<size_t>(displayNodes.size());

    for (const auto& displayNode : displayNodes) {
        const auto& nodeInfo = displayNode.nodeInfo;
        bool isOnline = IsClusterNodeOnline(nodeInfo);
        std::string slotId = std::to_string(nodeInfo.slotId);
        std::string nodeName = (!isOnline || nodeInfo.hostName.empty() ? "-" : nodeInfo.hostName) + "(" + slotId + ")";
        std::string role = isOnline ? displayNode.role : "-";
        std::string guid = (!isOnline || nodeInfo.guid.empty()) ? "-" : nodeInfo.guid;

        outStream << nodeName << role << (!nodeMgr::IsApplyUrmaDev() ? "-" : nodeInfo.bondingEid) << guid;
    }

    if (!outStream.Check()) {
        UBSE_LOG_ERROR << "[CLUSTER_QUERY] serialize cluster display nodes failed";
        return UBSE_ERROR;
    }

    size_t size = outStream.GetLength();
    uint8_t* buffer = outStream.GetBuffer(true);
    resp = {buffer, size, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    return UBSE_OK;
}

UbseResult ClusterInfoQueryHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    if (req.data == nullptr || req.len == 0) {
        UBSE_LOG_ERROR << "[CLUSTER_QUERY] request is empty";
        return CreateErrorResponse(UBSE_ERROR_INVAL, resp);
    }

    uint8_t globalQuery = 0;
    UbseDeSerialization inStream(req.data, req.len);
    inStream >> globalQuery;
    if (!inStream.Check() || globalQuery > 1) {
        UBSE_LOG_ERROR << "[CLUSTER_QUERY] deserialize query type failed";
        return CreateErrorResponse(UBSE_ERROR_INVAL, resp);
    }

    std::vector<ClusterDisplayNode> displayNodes{};
    UbseResult ret = UBSE_OK;
    if (globalQuery == 1) {
        ret = BuildGlobalClusterDisplayNodes(displayNodes);
    } else {
        ret = BuildLocalClusterDisplayNodes(displayNodes);
    }

    if (ret != UBSE_OK) {
        return CreateErrorResponse(ret, resp);
    }

    return SerializeClusterDisplayNodes(displayNodes, resp);
}

// 处理节点信息请求的通用函数
static UbseResult ProcessNodeRequest(const UbseByteBuffer &req, UbseByteBuffer &resp,
                                     const std::function<UbseResult(UbseNodeInfo &)> &handler)
{
    // 参数验证
    if (req.data == nullptr && req.len > 0) {
        UBSE_LOG_ERROR << "Invalid request: data is null but len=" << req.len;
        return CreateErrorResponse(UBSE_ERROR_INVAL, resp);
    }

    // 反序列化
    UbseNodeInfo info{};
    auto ret = DeSerializeUbseNode(info, req.data, req.len);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "DeSerialize ubse node failed: " << FormatRetCode(ret);
        return CreateErrorResponse(ret, resp);
    }

    // 调用处理器
    ret = handler(info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Handler failed: " << FormatRetCode(ret);
        return CreateErrorResponse(ret, resp);
    }

    // 创建成功响应（空节点信息）
    uint8_t *responseBuffer = nullptr;
    size_t responseSize = 0;
    ret = SerializeUbseNode(UbseNodeInfo{}, responseBuffer, responseSize);
    if (ret != UBSE_OK) {
        if (responseBuffer != nullptr) {
            SafeDeleteArray(responseBuffer, responseSize);
        }
        return CreateErrorResponse(ret, resp);
    }

    resp = {responseBuffer, responseSize, [responseSize](uint8_t *p) noexcept {
                SafeDeleteArray(p, responseSize);
            }};
    return UBSE_OK;
}

// 处理需要序列化响应的节点信息请求
static UbseResult ProcessNodeRequestWithResponse(const UbseByteBuffer &req, UbseByteBuffer &resp,
                                                 const std::function<UbseResult(UbseNodeInfo &)> &handler)
{
    // 参数验证
    if (req.data == nullptr && req.len > 0) {
        UBSE_LOG_ERROR << "Invalid request: data is null but len=" << req.len;
        return CreateErrorResponse(UBSE_ERROR_INVAL, resp);
    }

    // 反序列化
    UbseNodeInfo info{};
    auto ret = DeSerializeUbseNode(info, req.data, req.len);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "DeSerialize ubse node failed: " << FormatRetCode(ret);
        return CreateErrorResponse(ret, resp);
    }

    // 调用处理器
    ret = handler(info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Handler failed: " << FormatRetCode(ret);
        return CreateErrorResponse(ret, resp);
    }

    // 创建序列化响应
    uint8_t *responseBuffer = nullptr;
    size_t responseSize = 0;
    ret = SerializeUbseNode(info, responseBuffer, responseSize);
    if (ret != UBSE_OK) {
        if (responseBuffer != nullptr) {
            SafeDeleteArray(responseBuffer, responseSize);
        }
        return CreateErrorResponse(ret, resp);
    }

    resp = {responseBuffer, responseSize, [responseSize](uint8_t *p) noexcept {
                SafeDeleteArray(p, responseSize);
            }};
    return UBSE_OK;
}

static UbseResult ProcessNodeListRequest(const UbseByteBuffer &req, UbseByteBuffer &resp,
                                         const std::function<UbseResult(std::vector<UbseNodeInfo> &)> &handler)
{
    std::vector<UbseNodeInfo> infos{};
    auto ret = DeSerializeUbseNodeList(infos, req.data, req.len);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "DeSerialize ubse node list failed, " << FormatRetCode(ret);
        return CreateErrorResponse(ret, resp);
    }

    ret = handler(infos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "node list handler failed, " << FormatRetCode(ret);
        return CreateErrorResponse(ret, resp);
    }

    return CreateErrorResponse(UBSE_OK, resp);
}

// 处理LCNE变化上报
UbseResult LcneChangeNodeInfoHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOG_DEBUG << "LcneChangeNodeInfoHandler received request";
    return ProcessNodeRequest(req, resp, [](UbseNodeInfo &info) -> UbseResult {
        UBSE_LOG_INFO << "Processing LCNE change for node=" << info.nodeId;
        return UbseNodeControllerMaster::GetInstance().UbseLcneTopologyChangeHandler(info);
    });
}

// 处理节点信息上报
UbseResult UbseNodeReportNodeInfoHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    return ProcessNodeRequestWithResponse(req, resp, [](UbseNodeInfo &info) -> UbseResult {
        return UbseNodeControllerMaster::GetInstance().UbseNodeReportHandler(info);
    });
}

UbseResult CabinetNodeReportHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    return ProcessNodeListRequest(req, resp, [](std::vector<UbseNodeInfo> &infos) -> UbseResult {
        return UbseNodeControllerMaster::GetInstance().UbseCabinetReportHandler(infos);
    });
}

UbseResult GlobalNodeReportHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    return ProcessNodeListRequest(req, resp, [](std::vector<UbseNodeInfo> &infos) -> UbseResult {
        return UbseNodeControllerMaster::GetInstance().UbseGlobalReportHandler(infos);
    });
}

UbseResult SingleNodeReportHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    return ProcessNodeRequest(req, resp, [](UbseNodeInfo &info) -> UbseResult {
        return UbseNodeControllerMaster::GetInstance().UbseSingleNodeReportHandler(info);
    });
}

// 处理agent查询全量节点列表
UbseResult GetAllNodeInfoFromRemoteHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    (void)req;

    auto nodeInfos = UbseNodeController::GetInstance().GetLocalNodeInfos();

    std::vector<UbseNodeInfo> infos{};
    infos.reserve(nodeInfos.size());

    std::stringstream nodeList;
    for (const auto &[nodeId, nodeInfo] : nodeInfos) {
        infos.push_back(nodeInfo);
        nodeList << nodeId
                 << "(group=" << nodeInfo.groupId
                 << ",state=" << static_cast<uint32_t>(nodeInfo.clusterState)
                 << "), ";
    }

    UBSE_LOG_INFO << "[GET_ALL_SERVER] currentNodeId="
                  << UbseNodeController::GetInstance().GetCurrentNodeId()
                  << ", nodeCount=" << nodeInfos.size()
                  << ", nodes=[" << nodeList.str() << "]";

    uint8_t *buffer = nullptr;
    size_t size = 0;
    auto ret = SerializeUbseNodeList(infos, buffer, size);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SerializeUbseNodeList failed, " << FormatRetCode(ret);
        if (buffer != nullptr) {
            SafeDeleteArray(buffer, size);
        }
        return CreateErrorResponse(ret, resp);
    }

    resp = {buffer, size, [size](uint8_t *p) noexcept {
        SafeDeleteArray(p, size);
    }};
    return UBSE_OK;
}

// 处理agent查询全量链路信息
UbseResult UbseGetDirConnectInfoFromRemoteHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOG_INFO << "UbseGetDirConnectInfoFromRemoteHandler init";

    auto devDirConnectInfoRemote = UbseNodeController::GetInstance().UbseGetDirConnectInfo();

    uint8_t *buffer = nullptr;
    size_t size = 0;
    auto ret = SerializeDevDirConnectInfo(devDirConnectInfoRemote, buffer, size);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SerializeDevDirConnectInfo failed, " << FormatRetCode(ret);
        if (buffer != nullptr) {
            SafeDeleteArray(buffer, size);
        }
        return CreateErrorResponse(ret, resp);
    }

    resp = {buffer, size, [size](uint8_t *p) noexcept {
                SafeDeleteArray(p, size);
            }};
    return ret;
}

UbseResult UbseRemoteCasCadeNodeDownHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOG_INFO << "UbseRemoteCasCadeNodeDownHandler init";
    return ProcessNodeRequestWithResponse(req, resp, [](UbseNodeInfo &info) -> UbseResult {
        return UbseNodeControllerMaster::GetInstance().UbseCasCadeNodeDownHandler(info.nodeId);
    });
}

void CollectRemoteNodeInfoRespHandler(const std::string &nodeId, const UbseByteBuffer &respData, uint32_t resCode,
                                      UbseNodeInfo &info, UbseResult &collectRet)
{
    if (resCode != UBSE_OK) {
        UBSE_LOG_ERROR << "collect nodeId=" << nodeId << " node info failed, " << FormatRetCode(resCode);
        collectRet = resCode;
        return;
    }
    if (respData.data == nullptr || respData.len == 0) {
        UBSE_LOG_ERROR << "collect nodeId=" << nodeId << " resp null";
        collectRet = UBSE_ERROR_NULLPTR;
        return;
    }
    collectRet = DeSerializeUbseNode(info, respData.data, respData.len);
    if (collectRet != UBSE_OK) {
        UBSE_LOG_ERROR << "collect nodeId=" << nodeId << " deserialize failed, " << FormatRetCode(collectRet);
    }
}

// 从远程节点采集节点信息
UbseResult CollectRemoteNodeInfo(const std::string &nodeId, UbseNodeInfo &info)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_COLLECT),
        .address = nodeId,
    };

    // 使用shared_ptr管理同步对象
    struct SyncData {
        UbseResult collectRet = UBSE_OK;
        bool callbackCalled = false;
        std::mutex mtx;
        std::condition_variable cv;
    };

    auto syncData = std::make_shared<SyncData>();

    uint8_t *buffer = nullptr;
    size_t size = 0;
    auto ret = SerializeUbseNode(UbseNodeInfo{}, buffer, size);
    if (ret != UBSE_OK) {
        // 检查并释放buffer
        if (buffer != nullptr) {
            SafeDeleteArray(buffer, size);
        }
        return ret;
    }

    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t *p) noexcept {
                                 SafeDeleteArray(p, size);
                             }};

    ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                      [&info, syncData, nodeId](void *ctx, const UbseByteBuffer &respData, uint32_t resCode) -> void {
                          CollectRemoteNodeInfoRespHandler(nodeId, respData, resCode, info, syncData->collectRet);
                          {
                              std::lock_guard<std::mutex> lock(syncData->mtx);
                              syncData->callbackCalled = true;
                          }
                          syncData->cv.notify_one();
                      });

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send collect nodeId=" << nodeId << " msg failed, " << FormatRetCode(ret);
        return ret;
    }

    // 等待回调完成
    {
        std::unique_lock<std::mutex> lock(syncData->mtx);
        auto timeout = std::chrono::milliseconds(UBSE_RPC_TIMEOUT_MS);
        if (!syncData->cv.wait_for(lock, timeout, [syncData] { return syncData->callbackCalled; })) {
            UBSE_LOG_ERROR << "collect nodeId=" << nodeId << " timeout after " << UBSE_RPC_TIMEOUT_MS << "ms";
            return UBSE_ERROR_TIMEOUT;
        }
    }

    return syncData->collectRet;
}

namespace {
void NotifyNodeChangeAction(const std::string &destNodeId, const std::string &nodeId, const std::string &action)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_NODE_CHANGE),
        .address = destNodeId,
    };

    UbseSerialization outStream;
    outStream << nodeId << action;
    if (!outStream.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize node ID: " << nodeId << " and action: " << action;
        return;
    }
    size_t size = outStream.GetLength();
    uint8_t *buffer = outStream.GetBuffer(true);
    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t *p) noexcept {
                                 SafeDeleteArray(p, size);
                             }};

    auto dummyHandler = [](void *ctx, const UbseByteBuffer &buf, uint32_t ret) {
        UBSE_LOG_INFO << "Received ack for notification agent";
    };

    UBSE_LOG_INFO << "master notify action " << action << " to node " << destNodeId;
    auto ret = UbseRpcAsyncSend(endpoint, reqBuffer, nullptr, dummyHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to notify node " << destNodeId << ", error: " << FormatRetCode(ret);
    }
}
} // namespace

void UbseNodeControllerMaster::UbseMasterNotifyAllAgentsAction(const std::string &nodeId, std::string action)
{
    UBSE_LOG_INFO << "master notify action " << action << " to all nodes";
    auto nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    for (auto &node : nodeInfos) {
        NotifyNodeChangeAction(node.first, nodeId, action);
    }
}

UbseResult UbseLcneTopologyMessageChangeHandler::Handle(const ubse::message::UbseBaseMessagePtr& req,
                                                        const ubse::message::UbseBaseMessagePtr& rsp,
                                                        ubse::com::UbseComBaseMessageHandlerCtxPtr ctx)
{
    if (g_globalStop) {
        UBSE_LOG_INFO << "ubse is stopped, ignore msg";
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "Handling lcne topology change report.";
    auto request = UbseBaseMessage::DeConvert<UbseNodeInfoSerialize>(req);
    if (request == nullptr || rsp == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert rpc message";
        return UBSE_ERROR;
    }
    auto ret = request->Deserialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "DeSerialize ubse node failed: " << FormatRetCode(ret);
        rsp->SetErrCode(ret);
        return ret;
    }
    UbseNodeInfo nodeInfo = request->GetUbseNodeInfo();
    if (ctx->GetDstId() != nodeInfo.nodeId) {
        UBSE_LOG_ERROR << "receive wrong lcne topology change report, requestId=" << ctx->GetDstId()
                       << ", nodeId=" << nodeInfo.nodeId;
        rsp->SetErrCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    ret = UbseNodeControllerMaster::GetInstance().UbseLcneTopologyChangeHandler(nodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Handler failed: " << FormatRetCode(ret);
        rsp->SetErrCode(ret);
        return ret;
    }
    rsp->SetErrCode(UBSE_OK);
    return UBSE_OK;
}

uint16_t UbseLcneTopologyMessageChangeHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER);
}

uint16_t UbseLcneTopologyMessageChangeHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_LCNE_CHANGE_REPORT_TOPOLOGY);
}

// ====== 主备数据同步（主侧）实现 ======
// 同步推送目标集合：区分「全局备」（收所有机柜）与「柜内备」（只收本机柜），全量定时器据此构造不同粒度快照
struct SyncTargetNodes {
    std::vector<ubse::election::Node> globalStandbys;
    std::vector<ubse::election::Node> localStandbys;
};

// 收集主节点需要推送同步数据的备节点列表：
//  - 全局主：全局备（同步所有机柜） + 本机柜柜内备（若与全局备同一节点则只发一次）
//  - 柜内主/PD主：本机柜柜内备；非主节点：空（短路）
//  nodeId 非空表示单点推送：非本机柜节点不推柜内备（柜内备只需本机柜数据，源头省冗余）
static void GetSyncTargetNodes(SyncTargetNodes& targets, const std::string& nodeId = "")
{
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        return;
    }

    if (!IsHierarchicalElection()) {
        if (!module->IsLeader()) {
            return;
        }
        ubse::election::Node standby{};
        if (module->UbseGetStandbyNode(standby) == UBSE_OK && !standby.id.empty()) {
            targets.localStandbys.push_back(standby);
        }
        return;
    }

    auto role = GetClosRole();
    if (role == UbseClosNodeRole::GLOBAL_MASTER) {
        ubse::election::Node globalStandby{};
        if (module->UbseGetStandbyNode(globalStandby) == UBSE_OK && !globalStandby.id.empty()) {
            targets.globalStandbys.push_back(globalStandby);
        }
        bool pushLocalStandby = true;
        if (!nodeId.empty()) {
            auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
            pushLocalStandby = nodeInfo.groupId == GetGroupId();
        }
        if (pushLocalStandby) {
            ubse::election::Node localStandby{};
            if (module->GetLocalStandbyNode(localStandby) == UBSE_OK && !localStandby.id.empty() &&
                localStandby.id != globalStandby.id) {
                targets.localStandbys.push_back(localStandby);
            }
        }
    } else if (role == UbseClosNodeRole::PD_MASTER || role == UbseClosNodeRole::CABINET_MASTER) {
        ubse::election::Node localStandby{};
        if (module->GetLocalStandbyNode(localStandby) == UBSE_OK && !localStandby.id.empty()) {
            targets.localStandbys.push_back(localStandby);
        }
    }
}

// seq + 节点列表(嵌套流) + FAULT保护登记时刻map
static UbseResult BuildNodeInfoSyncFullPayload(uint64_t syncSeq, const std::vector<UbseNodeInfo>& nodeList,
                                               const std::map<std::string, uint64_t>& faultProtectMap,
                                               uint8_t*& buffer, size_t& size)
{
    UbseSerialization outStream;
    outStream << syncSeq;
    outStream << (right_v<size_t>(nodeList.size()));
    for (const auto& node : nodeList) {
        UbseSerialization item;
        auto ret = GetUbseNodeInfoOffset(node, item);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "ubse serialize node info sync full item failed, " << FormatRetCode(ret);
            return ret;
        }
        outStream << item;
        if (!outStream.Check()) {
            UBSE_LOG_ERROR << "ubse serialize node info sync full item failed";
            return UBSE_ERROR;
        }
    }
    outStream << faultProtectMap;
    if (!outStream.Check()) {
        UBSE_LOG_ERROR << "ubse serialize node info sync full failed";
        return UBSE_ERROR;
    }
    size = outStream.GetLength();
    buffer = outStream.GetBuffer(true);
    return UBSE_OK;
}

// 备节点主动拉取请求处理：回复全量快照（seq取主侧最新）
UbseResult NodeInfoSyncReqHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    if (g_globalStop.load()) {
        UBSE_LOG_WARN << "ubse is stopping, reject node info sync req";
        return CreateErrorResponse(UBSE_ERROR, resp);
    }
    if (req.data == nullptr || req.len == 0) {
        UBSE_LOG_ERROR << "node info sync req payload empty";
        return CreateErrorResponse(UBSE_ERROR, resp);
    }
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "election module not load";
        return CreateErrorResponse(UBSE_ERROR_MODULE_LOAD_FAILED, resp);
    }
    if (!module->IsLeader()) {
        UBSE_LOG_WARN << "current node is not leader, reject node info sync req";
        return CreateErrorResponse(UBSE_ERROR, resp);
    }

    std::string requestorNodeId;
    {
        UbseDeSerialization inStream(req.data, req.len);
        inStream >> requestorNodeId;
        if (!inStream.Check()) {
            UBSE_LOG_ERROR << "deserialize node info sync req failed";
            return CreateErrorResponse(UBSE_ERROR, resp);
        }
    }
    UBSE_LOG_INFO << "node info sync req from nodeId=" << requestorNodeId;

    // 快照seq在读取nodeInfos之前捕获，防止seq超前于快照内数据（备侧误用旧快照覆盖新推送）
    auto syncSeq = UbseNodeControllerMaster::GetInstance().GetNextSyncSeq();

    ubse::election::Node globalStandby{};
    bool requestorIsGlobalStandby = module->UbseGetStandbyNode(globalStandby) == UBSE_OK &&
                                    !globalStandby.id.empty() && globalStandby.id == requestorNodeId;
    ubse::election::Node localStandby{};
    bool isLocalStandby = module->GetLocalStandbyNode(localStandby) == UBSE_OK &&
                          !localStandby.id.empty() && localStandby.id == requestorNodeId;
    if (!requestorIsGlobalStandby && !isLocalStandby) {
        UBSE_LOG_WARN << "node info sync req from non-standby nodeId=" << requestorNodeId;
    }

    auto nodeInfos = requestorIsGlobalStandby ? UbseNodeController::GetInstance().GetAllNodes()
                                              : UbseNodeController::GetInstance().GetLocalNodeInfos();
    uint32_t selfGroupId = GetGroupId();
    std::vector<UbseNodeInfo> nodeList;
    nodeList.reserve(nodeInfos.size());
    for (const auto& iter : nodeInfos) {
        if (requestorIsGlobalStandby || iter.second.groupId == selfGroupId) {
            nodeList.push_back(iter.second);
        }
    }
    auto faultMap = UbseNodeController::GetInstance().GetFaultUpdateTimeSysMs();
    std::map<std::string, uint64_t> orderedFaultMap;
    for (const auto& kv : faultMap) {
        auto it = nodeInfos.find(kv.first);
        if (it != nodeInfos.end() && (requestorIsGlobalStandby || it->second.groupId == selfGroupId)) {
            orderedFaultMap.emplace(kv.first, kv.second);
        }
    }

    uint8_t* buffer = nullptr;
    size_t size = 0;
    auto ret = BuildNodeInfoSyncFullPayload(syncSeq, nodeList, orderedFaultMap, buffer, size);
    if (ret != UBSE_OK) {
        if (buffer != nullptr) {
            SafeDeleteArray(buffer, size);
        }
        return CreateErrorResponse(ret, resp);
    }
    resp = {buffer, size, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    UBSE_LOG_INFO << "[NODE_SYNC_REQ] master reply snapshot to standby, requestorId=" << requestorNodeId
                  << ", scope=" << (requestorIsGlobalStandby ? "global" : "local")
                  << ", syncSeq=" << syncSeq << ", nodeNum=" << nodeList.size()
                  << ", faultNodeNum=" << orderedFaultMap.size();
    return ret;
}

uint64_t UbseNodeControllerMaster::GetNextSyncSeq()
{
    return ++syncSeq_;
}

void UbseNodeControllerMaster::InitSyncSeq()
{
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    syncSeq_.store(static_cast<uint64_t>(nowMs));
}

void UbseNodeControllerMaster::SyncPushNodeToStandby(const std::string& nodeId)
{
    SyncTargetNodes syncTargets;
    GetSyncTargetNodes(syncTargets, nodeId);
    std::vector<ubse::election::Node> targets;
    targets.reserve(syncTargets.globalStandbys.size() + syncTargets.localStandbys.size());
    targets.insert(targets.end(), syncTargets.globalStandbys.begin(), syncTargets.globalStandbys.end());
    targets.insert(targets.end(), syncTargets.localStandbys.begin(), syncTargets.localStandbys.end());
    if (targets.empty()) {
        return;
    }

    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_DEBUG << "nodeId=" << nodeId << " not found, skip node info sync push";
        return;
    }

    uint64_t faultUpdateTimeMs = 0;
    if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
        auto faultTimes = UbseNodeController::GetInstance().GetFaultUpdateTimeSysMs();
        auto iter = faultTimes.find(nodeId);
        if (iter != faultTimes.end()) {
            faultUpdateTimeMs = iter->second;
        }
    }

    auto syncSeq = GetNextSyncSeq();
    for (const auto& standby : targets) {
        UbseSerialization outStream;
        outStream << syncSeq << faultUpdateTimeMs;
        UbseSerialization item;
        auto ret = GetUbseNodeInfoOffset(nodeInfo, item);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " serialize node info sync failed, " << FormatRetCode(ret);
            continue;
        }
        outStream << item;
        if (!outStream.Check()) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " serialize node info sync failed";
            continue;
        }
        size_t size = outStream.GetLength();
        uint8_t* buffer = outStream.GetBuffer(true);
        UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t* p) noexcept {
                                     SafeDeleteArray(p, size);
                                 }};

        const ubse::com::UbseComEndpoint endpoint{
            .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
            .serviceId = static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_NODE_INFO_SYNC),
            .address = standby.id,
        };
        auto sendRet = UbseRpcAsyncSend(endpoint, reqBuffer, nullptr, [nodeId, syncSeq](void*, const UbseByteBuffer&,
                                                                                       uint32_t retCode) {
            if (retCode != UBSE_OK) {
                UBSE_LOG_WARN << "node info sync push to standby failed, nodeId=" << nodeId << ", syncSeq=" << syncSeq
                              << ", ret=" << FormatRetCode(retCode);
            }
        });
        if (sendRet != UBSE_OK) {
            UBSE_LOG_WARN << "node info sync push to standby send failed, nodeId=" << nodeId
                          << ", syncSeq=" << syncSeq << ", ret=" << FormatRetCode(sendRet);
        } else {
            UBSE_LOG_INFO << "[NODE_SYNC] master push node to standby, standbyId=" << standby.id
                          << ", nodeId=" << nodeId << ", state=" << static_cast<uint32_t>(nodeInfo.clusterState)
                          << ", faultUpdateTimeMs=" << faultUpdateTimeMs << ", syncSeq=" << syncSeq;
        }
    }
}

void UbseNodeControllerMaster::UbseNodeSyncFullTimerHandler()
{
    SyncTargetNodes targets;
    GetSyncTargetNodes(targets);
    if (targets.globalStandbys.empty() && targets.localStandbys.empty()) {
        return;
    }

    auto syncSeq = GetNextSyncSeq();
    // 全局主读全量缓存，柜内主/PD主直读本地缓存，避免每 60s 跨柜 RPC
    bool isGlobalMaster = IsHierarchicalElection() && GetClosRole() == UbseClosNodeRole::GLOBAL_MASTER;
    auto nodeInfos = isGlobalMaster ? UbseNodeController::GetInstance().GetAllNodes()
                                    : UbseNodeController::GetInstance().GetLocalNodeInfos();
    if (nodeInfos.empty()) {
        UBSE_LOG_WARN << "node info sync full snapshot empty, skip push this round";
        return;
    }
    std::vector<UbseNodeInfo> nodeList;
    nodeList.reserve(nodeInfos.size());
    for (const auto& iter : nodeInfos) {
        nodeList.push_back(iter.second);
    }
    auto faultMap = UbseNodeController::GetInstance().GetFaultUpdateTimeSysMs();
    std::map<std::string, uint64_t> orderedFaultMap(faultMap.begin(), faultMap.end());

    uint32_t selfGroupId = GetGroupId();
    std::vector<UbseNodeInfo> localNodeList;
    localNodeList.reserve(nodeList.size());
    std::map<std::string, uint64_t> localFaultMap;
    for (const auto& info : nodeList) {
        if (info.groupId == selfGroupId) {
            localNodeList.push_back(info);
            auto fIt = orderedFaultMap.find(info.nodeId);
            if (fIt != orderedFaultMap.end()) {
                localFaultMap.emplace(fIt->first, fIt->second);
            }
        }
    }

    auto pushTo = [syncSeq](const std::vector<ubse::election::Node>& standbys,
                            const std::vector<UbseNodeInfo>& nodeListToPush,
                            const std::map<std::string, uint64_t>& faultMapToPush) {
        for (const auto& standby : standbys) {
            uint8_t* buffer = nullptr;
            size_t size = 0;
            auto ret = BuildNodeInfoSyncFullPayload(syncSeq, nodeListToPush, faultMapToPush, buffer, size);
            if (ret != UBSE_OK) {
                if (buffer != nullptr) {
                    SafeDeleteArray(buffer, size);
                }
                UBSE_LOG_ERROR << "node info sync full serialize failed, " << FormatRetCode(ret);
                continue;
            }
            UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t* p) noexcept {
                                         SafeDeleteArray(p, size);
                                     }};

            const ubse::com::UbseComEndpoint endpoint{
                .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
                .serviceId = static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_NODE_INFO_SYNC_FULL),
                .address = standby.id,
            };
            auto sendRet = UbseRpcAsyncSend(endpoint, reqBuffer, nullptr, [syncSeq](void*, const UbseByteBuffer&,
                                                                                    uint32_t retCode) {
                if (retCode != UBSE_OK) {
                    UBSE_LOG_WARN << "node info sync full push failed, syncSeq=" << syncSeq
                                  << ", ret=" << FormatRetCode(retCode);
                }
            });
            if (sendRet != UBSE_OK) {
                UBSE_LOG_WARN << "node info sync full push send failed, syncSeq=" << syncSeq
                              << ", ret=" << FormatRetCode(sendRet);
            } else {
                UBSE_LOG_INFO << "[NODE_SYNC_FULL] master periodic full snapshot push, standbyId=" << standby.id
                              << ", syncSeq=" << syncSeq << ", nodeNum=" << nodeListToPush.size();
            }
        }
    };

    pushTo(targets.globalStandbys, nodeList, orderedFaultMap);
    pushTo(targets.localStandbys, localNodeList, localFaultMap);
}

void UbseNodeControllerMaster::ConsumeMirrorOnPromote(bool globalScope)
{
    std::unordered_map<std::string, UbseNodeInfo> mirror;
    std::unordered_map<std::string, uint64_t> faultProtect;
    UbseNodeControllerAgent::GetInstance().GetMirrorSnapshot(mirror, faultProtect);
    if (mirror.empty()) {
        UBSE_LOG_INFO << "[NODE_SYNC] promote consume mirror empty, skip restore";
        return;
    }
    // 调用方(UbseMasterOnlineHandler)已持有taskExecMutex_，此处直接使用taskExecutor_（勿重复加锁）
    uint32_t selfGroupId = GetGroupId();
    for (auto& item : mirror) {
        const std::string& nodeId = item.first;
        UbseNodeInfo info = item.second;
        // 按角色范围分区消费：柜内升主只消费本机柜、全局升主只消费非本机柜；
        // 被本路径跳过的条目保留给另一消费路径，使同一节点双角色升主的两个消费路径顺序无关。
        bool isLocalNode = info.groupId == selfGroupId;
        if (globalScope == isLocalNode) {
            continue;
        }
        ConsumeMirrorNode(nodeId, info, faultProtect, globalScope);
    }
}

void UbseNodeControllerMaster::ConsumeMirrorNode(const std::string& nodeId, const UbseNodeInfo& mirrorInfo,
                                                 const std::unordered_map<std::string, uint64_t>& faultProtect,
                                                 bool globalScope)
{
    UbseNodeInfo info = mirrorInfo;
    // 与NODE_UP/周期/重试对账路径持有同一把per-node写锁，"检查内存现状+恢复镜像"原子执行
    UbseNodeControllerLockMgr::WriteLock(nodeId);
    if (!globalScope && IsNodeUpTakenOver(nodeId)) {
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        // 只移除已消费条目；镜像被本/他路径消费为空时，RemoveMirrorNode 内部整体清理并复位 lastSyncSeq_
        UbseNodeControllerAgent::GetInstance().RemoveMirrorNode(nodeId);
        return;
    }
    // 全局主只消费非本机柜节点：globalState 不继承旧 READY，WORKING/SMOOTHING 重置 INIT 等待重新上报
    if (globalScope) {
        info.globalState = UbseNodeGlobalState::UBSE_NODE_GLOBAL_INIT;
        if (info.clusterState == UbseNodeClusterState::UBSE_NODE_WORKING ||
            info.clusterState == UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
            info.clusterState = UbseNodeClusterState::UBSE_NODE_INIT;
        }
    } else if (info.clusterState == UbseNodeClusterState::UBSE_NODE_WORKING ||
               info.clusterState == UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
        // 柜内主消费：WORKING/SMOOTHING 不可直接继承，转INIT等升主后NODE_UP事件重对账
        // （升主后选举主角色活跃集合清空重建，可达成员必然补发NODE_UP驱动SMOOTHING→WORKING；
        // 不可达成员停留INIT，消除残影节点立即对账失败后永久卡死SMOOTHING且display显示在线的问题）
        UBSE_LOG_INFO << "[NODE_SYNC] promote consume: nodeId=" << nodeId << " state="
                      << static_cast<uint32_t>(info.clusterState) << " not inheritable, set INIT and wait node-up";
        info.clusterState = UbseNodeClusterState::UBSE_NODE_INIT;
    }
    uint64_t faultTimeMs = 0;
    auto faultIter = faultProtect.find(nodeId);
    if (faultIter != faultProtect.end()) {
        faultTimeMs = faultIter->second;
    }
    UBSE_LOG_INFO << "[NODE_SYNC] restore from mirror: nodeId=" << nodeId
                  << " state=" << static_cast<uint32_t>(info.clusterState);
    UbseNodeController::GetInstance().RestoreNodeInfoFromMirror(info, faultTimeMs);
    // 柜内升主：FAULT节点重建故障上报计数器（计数器为内存态、不随镜像同步），
    // 从0重新累计150次上报后触发平滑对账，避免倒换后FAULT节点无法进入平滑流程。
    if (!globalScope && info.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
        std::lock_guard<std::mutex> lock(faultCountersMutex_);
        faultReportCounters_[nodeId] = 0;
        UBSE_LOG_INFO << "[NODE_SYNC] rebuild fault report counter after promote, nodeId=" << nodeId;
    }
    UbseNodeControllerLockMgr::WriteUnLock(nodeId);
    // 只移除已消费条目；镜像被本/他路径消费为空时，RemoveMirrorNode 内部整体清理并复位 lastSyncSeq_
    UbseNodeControllerAgent::GetInstance().RemoveMirrorNode(nodeId);
}

bool UbseNodeControllerMaster::IsNodeUpTakenOver(const std::string& nodeId)
{
    // NODE_UP对账在途(SMOOTHING)或已收敛(WORKING)视为NODE_UP已接管：内存信息已由NODE_UP采集刷新，
    // 若回置INIT，NODE_UP已消费完且周期对账跳过INIT，节点将永久卡死INIT，须跳过镜像恢复
    // （升主时内存无全集群数据属正常情况，用静默查询避免对每个镜像节点打ERROR误导运维）
    auto curNode = UbseNodeController::GetInstance().GetNodeByIdSilent(nodeId);
    if (curNode.nodeId.empty() ||
        (curNode.clusterState != UbseNodeClusterState::UBSE_NODE_SMOOTHING &&
         curNode.clusterState != UbseNodeClusterState::UBSE_NODE_WORKING)) {
        return false;
    }
    UBSE_LOG_INFO << "[NODE_SYNC] promote consume: nodeId=" << nodeId << " state="
                  << static_cast<uint32_t>(curNode.clusterState) << " taken over by node-up flow, skip mirror restore";
    return true;
}
} // namespace ubse::nodeController
