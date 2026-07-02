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
#include <condition_variable>
#include <mutex>
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
#include "ubse_ras_handler.h"
#include "ubse_serial_util.h"
#include "ubse_timer.h"

const uint32_t HA_SEQUENCE_ID = 101; // todo 待链路合并后下线，需要确保在节点建链后触发，节点建链优先级100。

const uint32_t UBSE_COLLECT_TOPOLOGY_RETRY_INTERVAL = 400; // 节点上线，主动采集失败重试周期，单位豪秒
const uint32_t UBSE_COLLECT_TOPOLOGY_RETRY_TIMES = 5;      // 节点上线，主动采集失败重试次数
const uint32_t UBSE_NODE_LEDGER_INTERVAL = 300;            // 中心侧主动向各节点对账周期；单位秒
const uint32_t UBSE_REPORT_LOG_INTERVAL = 60;              // 中心侧收到各节点上报日志打印周期，单位秒
const std::string UBSE_NODE_MASTER_LEDGER_TIMER = "UbseNodeLedger";
const std::string UBSE_NODE_MASTER_ONLINE = "UbseMasterOnLine";
const std::string UBSE_NODE_NODE_UP = "UbseNodeUp";
const std::string UBSE_NODE_NODE_DOWN = "UbseNodeDown";
constexpr int UBSE_RPC_TIMEOUT_MS = 60000; // 5秒超时
constexpr UbseResult UBSE_ERROR_TIMEOUT = 0x80000001;

std::string LCNE_CHANGE_REPORT_EVENT = UBSE_EVENT_CLUSTER_TOPOLOGY_CHANGE;

UBSE_DEFINE_THIS_MODULE("ubse");
namespace ubse::nodeController {
using namespace ubse::context;
using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::ras;
using namespace ubse::event;
using namespace ubse::timer;
using namespace ubse::serial;

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

UbseResult UbseNodeControllerMaster::UbseMasterOnlineHandler(const std::string &nodeId)
{
    if (nodeId != UbseNodeController::GetInstance().GetCurrentNodeId()) {
        UBSE_LOG_INFO << "master online, current nodeId=" << UbseNodeController::GetInstance().GetCurrentNodeId()
                      << " not master, master nodeId=" << nodeId << ", start to clean context.";
        UbseNodeCleanAfterSwitchStandby();
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "master online, current nodeId=" << nodeId << " is master.";
    auto role = GetClosRole();
    UBSE_LOG_INFO << "[CLOS_ROLE] refresh role on master online, currentNodeId="
                  << UbseNodeController::GetInstance().GetCurrentNodeId() << ", role=" << static_cast<uint32_t>(role);
    std::string selfnodeId = nodeId;
    auto ret = UbsePubEvent(UBSE_EVENT_NODE_JOIN, selfnodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbsePubEvent " << UBSE_EVENT_NODE_JOIN << " failed on master node";
        return ret;
    }
    std::lock_guard<std::mutex> lock(taskExecMutex_);
    if (taskExecutor_ == nullptr || !taskExecutor_->Start()) {
        taskExecutor_ = UbseTaskExecutor::Create("UbseNodeMaster", NO_16, NO_1024);
        if (taskExecutor_ == nullptr || !taskExecutor_->Start()) {
            UBSE_LOG_ERROR << "master online, current nodeId=" << nodeId << " start task thread pool failed";
            taskExecutor_ = nullptr;
            return UBSE_ERROR_NULLPTR;
        }
    }
    // 本节点对账
    taskExecutor_->Execute([this]() -> void {
        auto selfNodeId = UbseNodeController::GetInstance().GetCurrentNodeId();

        UbseNodeLedger(selfNodeId);

        auto selfNodeInfo = UbseNodeController::GetInstance().GetNodeById(selfNodeId);
        UBSE_LOG_INFO << "[CLOS_REPORT] master self state change report trigger, nodeId=" << selfNodeId
                      << ", groupId=" << selfNodeInfo.groupId
                      << ", clusterState=" << static_cast<uint32_t>(selfNodeInfo.clusterState);

        auto reportRet = ReportSingleNodeChangeToPrev(selfNodeId, "master self ledger");
        if (reportRet != UBSE_OK) {
            UBSE_LOG_WARN << "[CLOS_STATE_REPORT] report master self change failed, nodeId=" << selfNodeId << ", "
                          << FormatRetCode(reportRet);
        }
    });
    // 周期对账，注册对账定时器回调并启动定时器
    UbseTimerHandlerRegister(
        UBSE_NODE_MASTER_LEDGER_TIMER,
        [this]() -> UbseResult {
            UbseNodeLedgerTimerHandler();
            return UBSE_OK;
        },
        UBSE_NODE_LEDGER_INTERVAL);

    isLogAggregationRunning_.store(true);
    taskExecutor_->Execute([this]() -> void { ReportAggregation(); });
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
    UbseRoleInfo masterInfo{};
    auto ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ubse get master node failed, skip smoothing.";
        return;
    }
    // 若当前节点不是主节点，不执行smoothing
    if (masterInfo.nodeId != UbseNodeController::GetInstance().GetCurrentNodeId()) {
        UBSE_LOG_INFO << "current node not master, skip smoothing.";
        return;
    }
    std::vector<UbseRoleInfo> roleInfos{};
    ret = UbseNodeGetLinkUpNodes(roleInfos);
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
    // 检查节点当前状态
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(nodeId);
    // 只有故障状态才切换到smoothing
    if (nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_FAULT) {
        return;
    }

    UBSE_LOG_INFO << "Node " << nodeId << " switching from fault to smoothing state";

    // 直接调用UbseNodeLedger函数来处理完整的平滑过程
    UbseNodeLedger(nodeId);
}

UbseResult UbseNodeControllerMaster::UbseNodeReportHandler(const UbseNodeInfo &nodeInfo)
{
    // 参数校验
    if (nodeInfo.nodeId.empty()) {
        return SER_INVALID_PARAM;
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

UbseResult UbseNodeControllerMaster::UbseSingleNodeReportHandler(const UbseNodeInfo &info)
{
    if (info.nodeId.empty()) {
        return SER_INVALID_PARAM;
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

    std::string prevNodeId;
    auto prevRet = GetPrevReportNodeId(prevNodeId);
    if (prevRet != UBSE_OK) {
        UBSE_LOG_WARN << "[CLOS_REPORT] pd master get global prev failed, nodeId=" << nodeInfo.nodeId << ", "
                      << FormatRetCode(prevRet);
        return prevRet;
    }

    if (prevNodeId == UbseNodeController::GetInstance().GetCurrentNodeId()) {
        return UBSE_OK;
    }

    UBSE_LOG_INFO << "[CLOS_REPORT] pd master forward single node to global, nodeId=" << nodeInfo.nodeId
                  << ", groupId=" << nodeInfo.groupId << ", clusterState=" << static_cast<uint32_t>(nodeInfo.clusterState)
                  << ", globalNodeId=" << prevNodeId;

    return UbseGlobalReportSingleNode(prevNodeId, nodeInfo);
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

    std::string prevNodeId;
    auto ret = GetPrevReportNodeId(prevNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[CLOS_REPORT] get prev node failed, nodeId=" << nodeId << ", reason=" << reason << ", "
                      << FormatRetCode(ret);
        return ret;
    }

    if (prevNodeId == UbseNodeController::GetInstance().GetCurrentNodeId()) {
        UBSE_LOG_INFO << "[CLOS_REPORT] skip report to self, nodeId=" << nodeId << ", reason=" << reason
                      << ", prevNodeId=" << prevNodeId;
        return UBSE_OK;
    }

    UBSE_LOG_INFO << "[CLOS_REPORT] report single node change to prev, nodeId=" << nodeId
                  << ", groupId=" << nodeInfo.groupId << ", clusterState=" << static_cast<uint32_t>(nodeInfo.clusterState)
                  << ", reason=" << reason << ", prevNodeId=" << prevNodeId << ", role=" << static_cast<uint32_t>(role);

    if (role == UbseClosNodeRole::PD_MASTER) {
        return UbseGlobalReportSingleNode(prevNodeId, nodeInfo);
    }

    return UbseCabinetReportSingleNode(prevNodeId, nodeInfo);
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
        return SER_INVALID_PARAM;
    }

    // 如果需要，创建临时变量
    UbseNodeInfo nodeInfoCopy = nodeInfo;
    UbseNodeController::GetInstance().UpdateNodeInfo(nodeInfoCopy.nodeId, nodeInfoCopy);
    UbseNodeController::GetInstance().UpdateDevDirConnectInfo();

    // 创建临时变量给UbsePubEvent
    std::string eventMessageCopy = nodeInfo.eventMessage;
    auto ret = UbsePubEvent(LCNE_CHANGE_REPORT_EVENT, eventMessageCopy);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbsePubEvent failed";
        return ret;
    }
    UbseMasterNotifyAllAgentsAction(nodeInfoCopy.nodeId, UBSE_EVENT_NODE_TOPO_LINK_CHANGE);

    return UBSE_OK;
}

void UbseNodeControllerMaster::UbseNodeUpHandlerExec(const std::string &nodeId)
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
    if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " node up, state=smoothing, skip ledger";
        UbseNodeControllerLockMgr::WriteUnLock(nodeId);
        return;
    }
    UbseNodeLedger(nodeId);
    UbseNodeControllerLockMgr::WriteUnLock(nodeId);
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
        taskExecutor_->Execute([this, nodeId]() -> void {
            UbseNodeLedger(nodeId);

            auto reportRet = ReportSingleNodeChangeToPrev(nodeId, "ras pre fault fail");
            if (reportRet != UBSE_OK) {
                UBSE_LOG_WARN << "[CLOS_REPORT] report ras pre fault fail change failed, nodeId=" << nodeId << ", "
                              << FormatRetCode(reportRet);
            }
        });
    }

    return UBSE_OK;
}

UbseResult UbseNodeControllerMaster::UbseNodeRasFaultHandler(const std::string &nodeId)
{
    UBSE_LOG_INFO << "nodeId=" << nodeId << ", start fault";

    // 清除旧的故障计数器
    ClearFaultCounter(nodeId);

    // 初始化故障计数器
    {
        std::lock_guard<std::mutex> lock(faultCountersMutex_);
        faultReportCounters_[nodeId] = 0;
        UBSE_LOG_DEBUG << "Initialize fault counter to 0 for node: " << nodeId;
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
        taskExecutor_->Execute([this, nodeId]() -> void {
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

// 处理节点信息请求的通用函数
static UbseResult ProcessNodeRequest(const UbseByteBuffer &req, UbseByteBuffer &resp,
                                     const std::function<UbseResult(UbseNodeInfo &)> &handler)
{
    // 参数验证
    if (req.data == nullptr && req.len > 0) {
        UBSE_LOG_ERROR << "Invalid request: data is null but len=" << req.len;
        return CreateErrorResponse(SER_INVALID_PARAM, resp);
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
        return CreateErrorResponse(SER_INVALID_PARAM, resp);
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
        UBSE_LOG_INFO << "Processing LCNE change for node: " << info.nodeId;
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
    auto nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    std::vector<UbseNodeInfo> infos{};
    for (auto iter : nodeInfos) {
        infos.push_back(iter.second);
    }

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
    return ret;
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

void UbseNodeControllerMaster::UbseMasterNotifyAllAgentsAction(const std::string &nodeId, std::string action)
{
    UBSE_LOG_INFO << "master notify action " << action << " to all nodes";
    auto nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    for (auto &node : nodeInfos) {
        const ubse::com::UbseComEndpoint endpoint{
            .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
            .serviceId = static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_NODE_CHANGE),
            .address = node.first,
        };

        UbseSerialization outStream;
        outStream << nodeId << action;
        if (!outStream.Check()) {
            UBSE_LOG_ERROR << "Failed to serialize node ID: " << nodeId << " and action: " << action;
            continue;
        }
        size_t size = outStream.GetLength();
        uint8_t *buffer = outStream.GetBuffer(true);
        UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t *p) noexcept {
                                     SafeDeleteArray(p, size);
                                 }};

        auto dummyHandler = [](void *ctx, const UbseByteBuffer &buf, uint32_t ret) {
            UBSE_LOG_INFO << "Received ack for notification agent";
        };

        UBSE_LOG_INFO << "master notify action " << action << " to node " << node.first;
        auto ret = UbseRpcAsyncSend(endpoint, reqBuffer, nullptr, dummyHandler);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "Failed to notify node " << node.first << ", error: " << FormatRetCode(ret);
            continue;
        }
    }
}
} // namespace ubse::nodeController