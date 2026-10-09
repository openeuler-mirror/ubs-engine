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

#include "ubse_node_controller_agent.h"

#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>

#include "ubse_common_def.h"
#include "ubse_election.h"
#include "ubse_election_module.h"
#include "ubse_error.h"
#include "ubse_event.h"
#include "ubse_logger.h"
#include "ubse_node_com_urma_collector.h"
#include "ubse_node_controller_collector.h"
#include "ubse_node_controller_util.h"
#include "ubse_node_mgr.h"
#include "ubse_serial_util.h"
#include "ubse_timer.h"
#include "adapter_plugins/mti/ubse_smbios.h"

const uint32_t UBSE_NODE_COLLECT_RETRY_INTERVAL = 2;  // 节点侧采集失败重试周期，单位/s
const uint32_t UBSE_NODE_REPORT_INTERVAL = 2;         // 节点侧主动向中心侧上报节点内存，拓扑周期；单位秒
constexpr int UBSE_RPC_TIMEOUT_MS = 60000;            // 60秒超时

const std::string UBSE_NODE_CABINET_REPORT_TIMER = "UbseCabinetReport";
const std::string UBSE_NODE_GLOBAL_REPORT_TIMER = "UbseGlobalReport";


UBSE_DEFINE_THIS_MODULE("ubse");
namespace ubse::nodeController {
using namespace ubse::context;
using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::event;
using namespace ubse::timer;
using namespace ubse::common::def;
using namespace ubse::com;
using namespace ubse::serial;
using namespace ubse::adapter_plugins::smbios;
using namespace ubse::task_executor;

constexpr UbseResult UBSE_ERROR_TIMEOUT = 0x80000001; // 超时错误码
const std::string UBSE_NODE_AGENT_REPORT_TIMER = "UbseNodeReport";
std::string UBSE_TOPOLOGY_CHANGE_EVENT = UBSE_EVENT_TOPOLOGY_CHANGE;
// 备侧同步事件处理器序列号（相同序号但事件类型不同且互斥：CHANGE_TO_STANDBY/CHANGE_TO_GLOBAL_STANDBY/CHANGE_TO_AGENT）
const uint32_t AGENT_STANDBY_PULL_SEQUENCE = 102;
const std::string UBSE_NODE_STANDBY_PULL_HANDLER = "UbseStandbyPullNodeInfo";
const std::string UBSE_NODE_AGENT_CLEAR_MIRROR_HANDLER = "UbseAgentClearMirror";
const std::string UBSE_NODE_GLOBAL_STANDBY_PULL_HANDLER = "UbseGlobalStandbyPullNodeInfo";
// 全量快照节点数上限（防恶意/异常报文导致 reserve 异常），与 node_mgr 集群容量口径联动，避免超大 CLOS 拓扑被拒绝
constexpr size_t MAX_MIRROR_NODE_NUM = nodeMgr::MAX_CLUSTER_SIZE;

// Agent端消息处理注册
UbseResult RegAgentMsgHandler()
{
    const ubse::com::UbseComEndpoint collectEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER),
        static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_COLLECT)};

    const ubse::com::UbseComEndpoint nodeChangeEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER),
        static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_NODE_CHANGE)};

    const ubse::com::UbseComEndpoint nodeInfoSyncEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER),
        static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_NODE_INFO_SYNC)};

    const ubse::com::UbseComEndpoint nodeInfoSyncFullEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER),
        static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_NODE_INFO_SYNC_FULL)};

    auto ret = UbseRegRpcService(nodeInfoSyncEndpoint, NodeInfoSyncHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register node info sync endpoint failed";
        return ret;
    }

    ret = UbseRegRpcService(nodeInfoSyncFullEndpoint, NodeInfoSyncFullHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register node info sync full endpoint failed";
        return ret;
    }

    ret = UbseRegRpcService(collectEndpoint, CollectNodeInfoHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register collect endpoint failed";
        return ret;
    }

    ret = UbseRegRpcService(nodeChangeEndpoint, nodeChangeHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register join endpoint failed";
        return ret;
    }

    UBSE_LOG_INFO << "Agent message handler registered successfully";
    return UBSE_OK;
}

UbseResult UbseNodeControllerAgent::Initialize()
{
    UBSE_LOG_INFO << "UbseNodeControllerAgent init";
    // 创建任务执行器
    taskExecutor_ = UbseTaskExecutor::Create("UbseNodeAgent", NO_1, NO_1024);
    if (taskExecutor_ == nullptr || !taskExecutor_->Start()) {
        UBSE_LOG_ERROR << "Create agent task executor failed";
        return UBSE_ERROR_NULLPTR;
    }

    GetClosRole();

    // 升备：主动拉取全量快照重建镜像（唯一拉取触发点，覆盖委派新备/主降备/重启即备/从转备）
    UbseElectionHandlerBuilder standbyBuilder;
    standbyBuilder.SetHandler([this](UbseElectionEventType, UBSE_ID_TYPE) -> UbseResult {
        if (taskExecutor_ != nullptr) {
            taskExecutor_->Execute([this]() -> void { PullNodeInfoFromMaster(); });
        }
        return UBSE_OK;
    });
    standbyBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    standbyBuilder.SetSequenceId(AGENT_STANDBY_PULL_SEQUENCE);
    standbyBuilder.SetType(UbseElectionEventType::CHANGE_TO_STANDBY);
    standbyBuilder.SetName(UBSE_NODE_STANDBY_PULL_HANDLER);
    UbseElectionChangeAttachHandler(standbyBuilder.Build());

    UbseElectionHandlerBuilder globalStandbyBuilder;
    globalStandbyBuilder.SetHandler([this](UbseElectionEventType, UBSE_ID_TYPE) -> UbseResult {
        if (taskExecutor_ != nullptr) {
            taskExecutor_->Execute([this]() -> void { PullNodeInfoFromMaster(); });
        }
        return UBSE_OK;
    });
    globalStandbyBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    globalStandbyBuilder.SetSequenceId(AGENT_STANDBY_PULL_SEQUENCE);
    globalStandbyBuilder.SetType(UbseElectionEventType::CHANGE_TO_GLOBAL_STANDBY);
    globalStandbyBuilder.SetName(UBSE_NODE_GLOBAL_STANDBY_PULL_HANDLER);
    UbseElectionChangeAttachHandler(globalStandbyBuilder.Build());

    UbseElectionHandlerBuilder agentBuilder;
    agentBuilder.SetHandler([this](UbseElectionEventType, UBSE_ID_TYPE) -> UbseResult {
        ClearMirror();
        return UBSE_OK;
    });
    agentBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    agentBuilder.SetSequenceId(AGENT_STANDBY_PULL_SEQUENCE);
    agentBuilder.SetType(UbseElectionEventType::CHANGE_TO_AGENT);
    agentBuilder.SetName(UBSE_NODE_AGENT_CLEAR_MIRROR_HANDLER);
    UbseElectionChangeAttachHandler(agentBuilder.Build());

    UBSE_LOG_INFO << "UbseNodeControllerAgent initialized successfully";
    return UBSE_OK;
}

void CollectBaseInfo(UbseNodeInfo& info)
{
    while (!g_globalStop.load()) {
        auto ret = CollectNodeBaseInfo(info);
        if (ret == UBSE_OK) {
            return;
        }
        UBSE_LOG_ERROR << "collect node base info failed, " << FormatRetCode(ret);
        sleep(UBSE_NODE_COLLECT_RETRY_INTERVAL);
    }
}

void CollectTopology(UbseNodeInfo& info)
{
    while (!g_globalStop.load()) {
        auto ret = CollectNodeTopology(info);
        if (ret == UBSE_OK) {
            return;
        }
        UBSE_LOG_ERROR << "collect node topology failed, " << FormatRetCode(ret);
        sleep(UBSE_NODE_COLLECT_RETRY_INTERVAL);
    }
}

UbseNodeInfo UbseNodeInfoCollect()
{
    UbseNodeInfo info{};
    auto ret = CollectNodeBaseInfo(info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << UbseNodeController::GetInstance().GetCurrentNodeId()
                       << "collect base info failed, " << FormatRetCode(ret) << ", will return last collect info";
        return UbseNodeController::GetInstance().GetCurNode();
    }
    ret = CollectNodeTopology(info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << UbseNodeController::GetInstance().GetCurrentNodeId()
                       << "collect topology info failed, " << FormatRetCode(ret) << ", will return last collect info";
        return UbseNodeController::GetInstance().GetCurNode();
    }
    ret = CollectSysSentryState(info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << UbseNodeController::GetInstance().GetCurrentNodeId()
                       << "collect sentry info failed, " << FormatRetCode(ret) << ", will return last collect info";
        return UbseNodeController::GetInstance().GetCurNode();
    }
    ret = CollectObmmKernelState(info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << UbseNodeController::GetInstance().GetCurrentNodeId()
                       << "collect obmm kernel info failed, " << FormatRetCode(ret)
                       << ", will return last collect info";
        return UbseNodeController::GetInstance().GetCurNode();
    }

    UBSE_LOG_INFO << "[MEM_COLLECT] success, nodeId=" << info.nodeId << ", groupId=" << info.groupId
                  << ", numaCount=" << info.numaInfos.size() << ", cpuCount=" << info.cpuInfos.size();

    ret = UbseNodeController::GetInstance().UpdateNodeInfo(info.nodeId, info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << info.nodeId << " update node info failed, " << FormatRetCode(ret);
        return UbseNodeController::GetInstance().GetCurNode();
    }

    UbseNodeController::GetInstance().UpdateDevDirConnectInfo();
    return info;
}

static void FillCachedNodeState(UbseNodeInfo &info)
{
    auto cachedInfo = UbseNodeController::GetInstance().GetNodeById(info.nodeId);
    if (cachedInfo.nodeId.empty()) {
        return;
    }

    info.localState = cachedInfo.localState;
    info.clusterState = cachedInfo.clusterState;
}

UbseResult UbseNodeInfoReport()
{
    UbseNodeInfo info = UbseNodeInfoCollect();
    FillCachedNodeState(info);

    if (info.nodeId.empty()) {
        UBSE_LOG_ERROR << "[CLOS_REPORT] skip report, node id is empty";
        return UBSE_ERROR;
    }

    auto& ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[CLOS_REPORT] election module is nullptr";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    Node masterInfo{};
    auto ret = electionModule->GetLocalMasterNode(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ubse get master node failed, skip report.";
        return ret;
    }

    if (masterInfo.id == UbseNodeController::GetInstance().GetCurrentNodeId()) {
        return UBSE_OK;
    }

    UBSE_LOG_INFO << "[CLOS_REPORT] fallback report self node to master, nodeId=" << info.nodeId
                  << ", groupId=" << info.groupId << ", masterNodeId=" << masterInfo.id
                  << ", clusterState=" << static_cast<uint32_t>(info.clusterState);

    return UbseNodeReportNodeInfo(masterInfo.id, info);
}

UbseResult UbseNodeControllerAgent::UbseNodeInfoReportTimerHandler()
{
    // 节点上报，部分场景下可能通信超时，放线程池处理不阻塞定时器
    taskExecutor_->Execute([]() { UbseNodeInfoReport(); });
    return UBSE_OK;
}

UbseResult UbseNodeControllerAgent::UbseCabinetInfoReportTimerHandler()
{
    UBSE_LOG_DEBUG << "timer cabinet trigger.";
    if (!IsCabinetMaster()) {
        UBSE_LOG_DEBUG << "timer cabinet trigger, current role=" << static_cast<uint32_t>(GetClosRole());
        return UBSE_OK;
    }
    UBSE_LOG_DEBUG << "timer cabinet trigger, current role is master.";

    taskExecutor_->Execute([this]() {
        auto ret = ReportCabinetFullInfo();
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "report cabinet full info failed, " << FormatRetCode(ret);
        }
    });
    return UBSE_OK;
}

UbseResult UbseNodeControllerAgent::UbseGlobalInfoReportTimerHandler()
{
    if (!IsPdMaster()) {
        return UBSE_OK;
    }

    taskExecutor_->Execute([this]() {
        auto ret = ForwardCabinetFullToPrev();
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "forward full info to prev failed, " << FormatRetCode(ret);
        }
    });
    return UBSE_OK;
}

void UbseNodeControllerAgent::StartExec()
{
    UbseNodeInfo info{};
    CollectBaseInfo(info);
    CollectTopology(info);
    UbseNodeController::GetInstance().SetCurrentNodeId(info.nodeId);
    // 将节点刷新至内存，mem ctl从obmm采集账本；海量账本场景下，采集时间较长；需要交由子线程处理

    UbseNodeController::GetInstance().UpdateNodeInfo(info.nodeId, info);
    // 更新 link id
    UbseNodeController::GetInstance().UpdateDevDirConnectInfo();
    // 将节点本地状态刷新至 ready，加入集群选主
    UbseNodeController::GetInstance().UpdateNodeInfoLocalState(UbseNodeLocalState::UBSE_NODE_READY);
    // 注册采集定时器并启动
    UbseTimerHandlerRegister(
        UBSE_NODE_AGENT_REPORT_TIMER, [this]() -> UbseResult { return UbseNodeInfoReportTimerHandler(); },
        UBSE_NODE_REPORT_INTERVAL);

    auto role = GetClosRole();
    UBSE_LOG_INFO << "[CLOS_ROLE] agent start, nodeId=" << info.nodeId << ", role=" << static_cast<uint32_t>(role);

    UbseTimerHandlerRegister(
        UBSE_NODE_CABINET_REPORT_TIMER, [this]() -> UbseResult { return UbseCabinetInfoReportTimerHandler(); },
        UBSE_NODE_REPORT_INTERVAL);

    UbseTimerHandlerRegister(
        UBSE_NODE_GLOBAL_REPORT_TIMER, [this]() -> UbseResult { return UbseGlobalInfoReportTimerHandler(); },
        UBSE_NODE_REPORT_INTERVAL);

    // 注册LCNE变更回调
    UbseSubEvent(UBSE_TOPOLOGY_CHANGE_EVENT, UbseNodeInfoLcneNotifyHandler);
}

UbseResult UbseNodeControllerAgent::Start()
{
    // 注册消息处理器
    auto ret = RegAgentMsgHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register agent message handler failed, " << FormatRetCode(ret);
        return ret;
    }
    taskExecutor_->Execute([this]() -> void {
        StartExec();
    });
    return UBSE_OK;
}

UbseResult UbseNodeControllerAgent::UbseNodeInfoLcneNotifyHandler(std::string&, std::string& eventMsg)
{
    UBSE_LOG_INFO << "lcne change, start to collect";
    UbseNodeInfo info = UbseNodeInfoCollect();
    FillCachedNodeState(info);
    info.eventMessage = eventMsg;
    UbseNodeController::GetInstance().UpdateNodeInfo(info.nodeId, info);
    UbseNodeController::GetInstance().UpdateDevDirConnectInfo();

    if (GetClosRole() != UbseClosNodeRole::UNKNOWN && !IsGlobalMaster()) {
        std::string prevNodeId;
        auto ret = GetPrevReportNodeId(prevNodeId);
        if (ret == UBSE_OK) {
            return LcneChangeReportNodeInfo(prevNodeId, info);
        }
        UBSE_LOG_WARN << "get prev node failed for lcne report, fallback to single master, " << FormatRetCode(ret);
    }

    UbseRoleInfo masterInfo{};
    auto ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "ubse get master node failed, skip report.";
        return ret;
    }
    return LcneChangeReportNodeInfo(masterInfo.nodeId, info);
}

void UbseNodeControllerAgent::UnInitialize() {}

void UbseNodeControllerAgent::Stop()
{
    UBSE_LOG_INFO << "ubse node agent start to stop executor.";
    if (taskExecutor_ != nullptr) {
        taskExecutor_->Stop();
    }
    UBSE_LOG_INFO << "ubse node agent start to stop report timer.";
    // 停止 内存&拓扑上报定时器
    UbseTimerHandlerUnregister(UBSE_NODE_AGENT_REPORT_TIMER);

    UbseTimerHandlerUnregister(UBSE_NODE_CABINET_REPORT_TIMER);

    UbseTimerHandlerUnregister(UBSE_NODE_GLOBAL_REPORT_TIMER);

    // 解除注册 LCNE 变更回调
    UbseUnSubEvent(UBSE_TOPOLOGY_CHANGE_EVENT, UbseNodeInfoLcneNotifyHandler);
    UBSE_LOG_INFO << "ubse node agent stopped.";
}

// ====== 主备数据同步（备侧）实现 ======
UbseSyncStandbyRole UbseNodeControllerAgent::GetSyncStandbyRole() const
{
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        return UbseSyncStandbyRole::NONE;
    }
    ubse::election::HaTopologyInfo topology{};
    if (module->GetCurNodeGlobalTopoInfo(topology) != UBSE_OK) {
        return UbseSyncStandbyRole::NONE;
    }
    // 全局备同步所有机柜数据；普通组内备只同步本机柜数据
    if (topology.currentNode.globalRole == ubse::election::GlobalRoleType::GLOBAL_STANDBY) {
        return UbseSyncStandbyRole::GLOBAL_STANDBY;
    }
    if (topology.currentNode.groupRole == ubse::election::RoleType::STANDBY) {
        return UbseSyncStandbyRole::CABINET_STANDBY;
    }
    return UbseSyncStandbyRole::NONE;
}

bool UbseNodeControllerAgent::IsStandbyNode() const
{
    return GetSyncStandbyRole() != UbseSyncStandbyRole::NONE;
}

bool UbseNodeControllerAgent::GetMasterNodeForStandby(ubse::election::Node& masterNode) const
{
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        return false;
    }
    if (GetSyncStandbyRole() == UbseSyncStandbyRole::GLOBAL_STANDBY) {
        return module->UbseGetMasterNode(masterNode) == UBSE_OK && !masterNode.id.empty();
    }
    // 柜内备取本机柜主；全局主所在机柜的柜内备也从全局主拉取，但靠ShouldKeepNodeInMirror只保留本机柜
    return module->GetLocalMasterNode(masterNode) == UBSE_OK && !masterNode.id.empty();
}

bool UbseNodeControllerAgent::ShouldKeepNodeInMirror(const UbseNodeInfo& info) const
{
    if (GetSyncStandbyRole() == UbseSyncStandbyRole::CABINET_STANDBY) {
        return info.groupId == GetGroupId();
    }
    return true;
}

void UbseNodeControllerAgent::ClearMirror()
{
    std::unique_lock<std::shared_mutex> lock(mirrorMutex_);
    nodeInfoMirror_.clear();
    faultProtectMirror_.clear();
    lastSyncSeq_ = 0;
    UBSE_LOG_INFO << "node info mirror cleared";
}

void UbseNodeControllerAgent::RemoveMirrorNode(const std::string& nodeId)
{
    std::unique_lock<std::shared_mutex> lock(mirrorMutex_);
    nodeInfoMirror_.erase(nodeId);
    faultProtectMirror_.erase(nodeId);
    if (nodeInfoMirror_.empty()) {
        faultProtectMirror_.clear();
        lastSyncSeq_ = 0;
        UBSE_LOG_INFO << "node info mirror empty after remove, cleared";
    }
}

void UbseNodeControllerAgent::GetMirrorSnapshot(std::unordered_map<std::string, UbseNodeInfo>& mirror,
                                                std::unordered_map<std::string, uint64_t>& faultProtect)
{
    std::shared_lock<std::shared_mutex> lock(mirrorMutex_);
    mirror = nodeInfoMirror_;
    faultProtect = faultProtectMirror_;
}

UbseResult NodeInfoSyncHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    return UbseNodeControllerAgent::GetInstance().HandleNodeInfoSync(req, resp);
}

UbseResult NodeInfoSyncFullHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    return UbseNodeControllerAgent::GetInstance().HandleNodeInfoSyncFull(req, resp);
}

UbseResult UbseNodeControllerAgent::HandleNodeInfoSync(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    if (!IsStandbyNode()) {
        UBSE_LOG_INFO << "current node not standby, ignore node info sync";
        return UBSE_OK;
    }
    if (req.data == nullptr || req.len == 0) {
        return UBSE_ERROR;
    }

    UbseDeSerialization inStream(req.data, req.len);
    uint64_t syncSeq = 0;
    uint64_t faultUpdateTimeMs = 0;
    UbseNodeInfo nodeInfo{};
    inStream >> syncSeq >> faultUpdateTimeMs;
    UbseDeSerialization item;
    inStream >> item;
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "deserialize node info sync head failed";
        return UBSE_ERROR;
    }
    auto ret = ParseNodeInfo(nodeInfo, item);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "parse node info sync node failed, " << FormatRetCode(ret);
        return ret;
    }

    ubse::election::Node masterNode{};
    (void)GetMasterNodeForStandby(masterNode);
    auto selfId = UbseNodeController::GetInstance().GetCurrentNodeId();
    if (nodeInfo.nodeId == masterNode.id || nodeInfo.nodeId == selfId) {
        UBSE_LOG_INFO << "nodeId=" << nodeInfo.nodeId << " is master/self, skip mirror";
        return UBSE_OK;
    }
    if (!ShouldKeepNodeInMirror(nodeInfo)) {
        UBSE_LOG_INFO << "nodeId=" << nodeInfo.nodeId << " groupId=" << nodeInfo.groupId
                      << " not in current cabinet, skip mirror";
        return UBSE_OK;
    }

    std::unique_lock<std::shared_mutex> lock(mirrorMutex_);
    if (syncSeq <= lastSyncSeq_) {
        UBSE_LOG_DEBUG << "node info sync out of order, syncSeq=" << syncSeq << ", lastSyncSeq=" << lastSyncSeq_;
        return UBSE_OK;
    }
    nodeInfoMirror_[nodeInfo.nodeId] = nodeInfo;
    if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT && faultUpdateTimeMs != 0) {
        faultProtectMirror_[nodeInfo.nodeId] = faultUpdateTimeMs;
    } else if (nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_FAULT) {
        faultProtectMirror_.erase(nodeInfo.nodeId);
    }
    lastSyncSeq_ = syncSeq;
    UBSE_LOG_INFO << "[NODE_SYNC] standby upsert mirror, nodeId=" << nodeInfo.nodeId
                  << ", state=" << static_cast<uint32_t>(nodeInfo.clusterState)
                  << ", faultUpdateTimeMs=" << faultUpdateTimeMs << ", syncSeq=" << syncSeq;
    return UBSE_OK;
}

UbseResult UbseNodeControllerAgent::HandleNodeInfoSyncFull(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    if (req.data == nullptr || req.len == 0) {
        return UBSE_ERROR;
    }
    return ProcessNodeInfoSyncFull(req.data, req.len, false);
}

UbseResult UbseNodeControllerAgent::ProcessNodeInfoSyncFull(const uint8_t* data, uint32_t len, bool isPullReply)
{
    if (!IsStandbyNode()) {
        UBSE_LOG_INFO << "current node not standby, ignore node info sync full";
        return UBSE_OK;
    }

    UbseDeSerialization inStream(data, len);
    uint64_t syncSeq = 0;
    size_t num = 0;
    inStream >> syncSeq >> num;
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "deserialize node info sync full head failed";
        return UBSE_ERROR;
    }
    if (num > MAX_MIRROR_NODE_NUM) {
        UBSE_LOG_ERROR << "node info sync full node num too large, num=" << num;
        return UBSE_ERROR;
    }

    std::vector<UbseNodeInfo> nodeList;
    nodeList.reserve(num);
    for (size_t i = 0; i < num; i++) {
        UbseDeSerialization item;
        inStream >> item;
        if (!inStream.Check()) {
            UBSE_LOG_ERROR << "deserialize node info sync full item failed, index=" << i;
            return UBSE_ERROR;
        }
        UbseNodeInfo info{};
        auto ret = ParseNodeInfo(info, item);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "parse node info sync full item failed, index=" << i << ", " << FormatRetCode(ret);
            return ret;
        }
        nodeList.push_back(info);
    }
    std::map<std::string, uint64_t> faultProtectMap;
    inStream >> faultProtectMap;
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "deserialize node info sync full fault map failed";
        return UBSE_ERROR;
    }

    ubse::election::Node masterNode{};
    (void)GetMasterNodeForStandby(masterNode);
    auto selfId = UbseNodeController::GetInstance().GetCurrentNodeId();
    std::unordered_map<std::string, UbseNodeInfo> filteredMap;
    std::unordered_map<std::string, uint64_t> filteredFaultMap;
    for (const auto& node : nodeList) {
        if (node.nodeId == masterNode.id || node.nodeId == selfId) {
            continue;
        }
        if (!ShouldKeepNodeInMirror(node)) {
            continue;
        }
        filteredMap[node.nodeId] = node;
        auto iter = faultProtectMap.find(node.nodeId);
        if (iter != faultProtectMap.end()) {
            filteredFaultMap[node.nodeId] = iter->second;
        }
    }

    std::unique_lock<std::shared_mutex> lock(mirrorMutex_);
    // 拉取回包是备侧主动发起的权威全量快照，无条件应用；收包路径按 seq 做乱序过滤
    if (!isPullReply && syncSeq <= lastSyncSeq_) {
        UBSE_LOG_DEBUG << "node info sync full out of order, syncSeq=" << syncSeq << ", lastSyncSeq=" << lastSyncSeq_;
        return UBSE_OK;
    }
    nodeInfoMirror_ = std::move(filteredMap);
    faultProtectMirror_ = std::move(filteredFaultMap);
    lastSyncSeq_ = std::max(syncSeq, lastSyncSeq_);
    UBSE_LOG_INFO << "[NODE_SYNC_FULL] standby apply full snapshot, syncSeq=" << syncSeq
                  << ", mirrorSize=" << nodeInfoMirror_.size();
    return UBSE_OK;
}

void UbseNodeControllerAgent::PullNodeInfoFromMaster()
{
    if (!IsStandbyNode()) {
        UBSE_LOG_INFO << "current node not standby, skip pull node info";
        return;
    }
    ubse::election::Node masterNode{};
    if (!GetMasterNodeForStandby(masterNode)) {
        UBSE_LOG_WARN << "get master node failed, skip pull node info";
        return;
    }

    ClearMirror();

    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_NODE_INFO_SYNC_REQ),
        .address = masterNode.id,
    };

    UbseSerialization outStream;
    outStream << UbseNodeController::GetInstance().GetCurrentNodeId();
    if (!outStream.Check()) {
        UBSE_LOG_ERROR << "serialize node info sync req failed";
        return;
    }
    size_t size = outStream.GetLength();
    uint8_t* buffer = outStream.GetBuffer(true);
    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t* p) noexcept {
                                 SafeDeleteArray(p, size);
                             }};

    UbseResult pullRet = UBSE_OK;
    bool callbackCalled = false;
    std::mutex mtx;
    std::condition_variable cv;

    auto ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                           [this, &pullRet, &callbackCalled, &mtx, &cv](void* ctx, const UbseByteBuffer& respData,
                                                                       uint32_t resCode) {
                               if (resCode != UBSE_OK) {
                                   UBSE_LOG_WARN << "pull node info from master failed, " << FormatRetCode(resCode);
                                   pullRet = resCode;
                               } else if (respData.data != nullptr && respData.len != 0) {
                                   pullRet = ProcessNodeInfoSyncFull(respData.data, respData.len, true);
                               } else {
                                   pullRet = UBSE_ERROR_NULLPTR;
                               }
                               {
                                   std::lock_guard<std::mutex> lock(mtx);
                                   callbackCalled = true;
                               }
                               cv.notify_one();
                           });
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "send node info sync req failed, " << FormatRetCode(ret);
        return;
    }
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&callbackCalled] { return callbackCalled; });
    }
    if (pullRet != UBSE_OK) {
        UBSE_LOG_WARN << "pull node info from master failed, " << FormatRetCode(pullRet);
        return;
    }
    UBSE_LOG_INFO << "[NODE_SYNC_PULL] standby pull full snapshot success";
}

// 安全的序列化辅助函数
static UbseResult SafeSerializeUbseNode(const UbseNodeInfo &info, UbseByteBuffer &buffer)
{
    uint8_t *data = nullptr;
    size_t size = 0;
    auto ret = SerializeUbseNode(info, data, size);
    if (ret != UBSE_OK) {
        if (data != nullptr) {
            SafeDeleteArray(data, size);
        }
        return ret;
    }

    buffer = {data, size, [size](uint8_t *p) noexcept {
        SafeDeleteArray(p, size);
    }};
    return UBSE_OK;
}

static UbseResult SafeSerializeUbseNodeList(const std::vector<UbseNodeInfo> &infos, UbseByteBuffer &buffer)
{
    uint8_t *data = nullptr;
    size_t size = 0;
    auto ret = SerializeUbseNodeList(infos, data, size);
    if (ret != UBSE_OK) {
        if (data != nullptr) {
            SafeDeleteArray(data, size);
        }
        return ret;
    }

    buffer = {data, size, [size](uint8_t *p) noexcept {
        SafeDeleteArray(p, size);
    }};
    return UBSE_OK;
}

static UbseResult SendSingleNodeReportByOpCode(const std::string &nodeId, const UbseNodeInfo &info,
                                               UbseNodeControllerOpCode opCode, const std::string &action)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(opCode),
        .address = nodeId,
    };

    struct SyncData {
        UbseResult reportRet = UBSE_OK;
        bool callbackCalled = false;
        std::mutex mtx;
        std::condition_variable cv;
    };

    auto syncData = std::make_shared<SyncData>();

    UbseByteBuffer reqBuffer;
    auto ret = SafeSerializeUbseNode(info, reqBuffer);
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                      [syncData, nodeId, action](void *, const UbseByteBuffer &, uint32_t resCode) -> void {
        if (resCode != UBSE_OK) {
            UBSE_LOG_ERROR << action << " nodeId=" << nodeId << " failed, " << FormatRetCode(resCode);
            syncData->reportRet = resCode;
        }
        {
            std::lock_guard<std::mutex> lock(syncData->mtx);
            syncData->callbackCalled = true;
        }
        syncData->cv.notify_one();
    });

if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send " << action << " nodeId=" << nodeId << " failed, " << FormatRetCode(ret);
        return ret;
    }

    std::unique_lock<std::mutex> lock(syncData->mtx);
    auto timeout = std::chrono::milliseconds(UBSE_RPC_TIMEOUT_MS);
    if (!syncData->cv.wait_for(lock, timeout, [syncData]() { return syncData->callbackCalled; })) {
        UBSE_LOG_ERROR << action << " nodeId=" << nodeId << " timeout after " << UBSE_RPC_TIMEOUT_MS << "ms";
        return UBSE_ERROR_TIMEOUT;
    }

    return syncData->reportRet;
}

static UbseResult SendNodeListReportByOpCode(const std::string &nodeId, const std::vector<UbseNodeInfo> &infos,
                                             UbseNodeControllerOpCode opCode, const std::string &action)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(opCode),
        .address = nodeId,
    };

    struct SyncData {
        UbseResult reportRet = UBSE_OK;
        bool callbackCalled = false;
        std::mutex mtx;
        std::condition_variable cv;
    };

    auto syncData = std::make_shared<SyncData>();

    UbseByteBuffer reqBuffer;
    auto ret = SafeSerializeUbseNodeList(infos, reqBuffer);
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                      [syncData, nodeId, action](void *, const UbseByteBuffer &, uint32_t resCode) -> void {
        if (resCode != UBSE_OK) {
            UBSE_LOG_ERROR << action << " nodeId=" << nodeId << " failed, " << FormatRetCode(resCode);
            syncData->reportRet = resCode;
        }

        {
            std::lock_guard<std::mutex> lock(syncData->mtx);
            syncData->callbackCalled = true;
        }
        syncData->cv.notify_one();
    });

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send " << action << " nodeId=" << nodeId << " failed, " << FormatRetCode(ret);
        return ret;
    }

    std::unique_lock<std::mutex> lock(syncData->mtx);
    auto timeout = std::chrono::milliseconds(UBSE_RPC_TIMEOUT_MS);
    if (!syncData->cv.wait_for(lock, timeout, [syncData]() { return syncData->callbackCalled; })) {
        UBSE_LOG_ERROR << action << " nodeId=" << nodeId << " timeout after " << UBSE_RPC_TIMEOUT_MS << "ms";
        return UBSE_ERROR_TIMEOUT;
    }

    return syncData->reportRet;
}

// Agent向Master周期上报节点信息
UbseResult UbseNodeReportNodeInfo(const std::string& nodeId, const UbseNodeInfo& info)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_REPORT),
        .address = nodeId,
    };

    // 使用智能指针管理同步对象
    struct SyncData {
        UbseResult reportRet = UBSE_OK;
        bool callbackCalled = false;
        std::mutex mtx;
        std::condition_variable cv;
    };

    auto syncData = std::make_shared<SyncData>();

    // 使用辅助函数序列化
    UbseByteBuffer reqBuffer;
    auto ret = SafeSerializeUbseNode(info, reqBuffer);
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                      [syncData, nodeId](void* ctx, const UbseByteBuffer& respData, uint32_t resCode) -> void {
                          if (resCode != UBSE_OK) {
                              UBSE_LOG_ERROR << "report node to nodeId=" << nodeId << " failed, "
                                             << FormatRetCode(resCode);
                              syncData->reportRet = resCode;
                          }

                          {
                              std::lock_guard<std::mutex> lock(syncData->mtx);
                              syncData->callbackCalled = true;
                          }
                          syncData->cv.notify_one();
                      });

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send report nodeId=" << nodeId << " msg failed, " << FormatRetCode(ret);
        return ret;
    }

    // 等待回调完成
    {
        std::unique_lock<std::mutex> lock(syncData->mtx);
        auto timeout = std::chrono::milliseconds(UBSE_RPC_TIMEOUT_MS);
        if (!syncData->cv.wait_for(lock, timeout, [syncData] { return syncData->callbackCalled; })) {
            UBSE_LOG_ERROR << "report node to " << nodeId << " timeout after " << UBSE_RPC_TIMEOUT_MS << "ms";
            return UBSE_ERROR_TIMEOUT;
        }
    }

    return syncData->reportRet;
}

// Agent向Master上报LCNE拓扑变化
UbseResult LcneChangeReportNodeInfo(const std::string& nodeId, const UbseNodeInfo& info)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_LCNE_CHANGE_REPORT_TOPOLOGY),
        .address = nodeId,
    };

    // 使用智能指针
    struct SyncData {
        UbseResult reportRet = UBSE_OK;
        bool callbackCalled = false;
        std::mutex mtx;
        std::condition_variable cv;
    };

    auto syncData = std::make_shared<SyncData>();

    // 使用辅助函数序列化
    UbseByteBuffer reqBuffer;
    auto ret = SafeSerializeUbseNode(info, reqBuffer);
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                      [syncData, nodeId](void* ctx, const UbseByteBuffer& respData, uint32_t resCode) -> void {
                          if (resCode != UBSE_OK) {
                              UBSE_LOG_ERROR << "lcne, report node to nodeId=" << nodeId << " failed, "
                                             << FormatRetCode(resCode);
                              syncData->reportRet = resCode;
                          }

                          {
                              std::lock_guard<std::mutex> lock(syncData->mtx);
                              syncData->callbackCalled = true;
                          }
                          syncData->cv.notify_one();
                      });

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "lcne change, send report nodeId=" << nodeId << " msg failed, " << FormatRetCode(ret);
        return ret;
    }

    // 等待回调完成
    {
        std::unique_lock<std::mutex> lock(syncData->mtx);
        auto timeout = std::chrono::milliseconds(UBSE_RPC_TIMEOUT_MS);
        if (!syncData->cv.wait_for(lock, timeout, [syncData] { return syncData->callbackCalled; })) {
            UBSE_LOG_ERROR << "lcne change report to " << nodeId << " timeout after " << UBSE_RPC_TIMEOUT_MS << "ms";
            return UBSE_ERROR_TIMEOUT;
        }
    }

    return syncData->reportRet;
}

UbseResult UbseCabinetReportSingleNode(const std::string &nodeId, const UbseNodeInfo &info)
{
    return SendSingleNodeReportByOpCode(nodeId, info, UbseNodeControllerOpCode::NODE_CONTROLLER_SINGLE_NODE_REPORT,
                                        "cabinet single report");
}

UbseResult UbseReportCasCadeNodeDown(const std::string& nodeId, const UbseNodeInfo& info)
{
    return SendSingleNodeReportByOpCode(nodeId, info, UbseNodeControllerOpCode::NODE_CONTROLLER_CASCADE_NODE_REPORT,
                                        "cascade node down report");
}

UbseResult UbseCabinetReportFullInfo(const std::string &nodeId, const std::vector<UbseNodeInfo> &infos)
{
    return SendNodeListReportByOpCode(nodeId, infos, UbseNodeControllerOpCode::NODE_CONTROLLER_CABINET_FULL_REPORT,
                                      "cabinet full report");
}

UbseResult UbseGlobalReportSingleNode(const std::string &nodeId, const UbseNodeInfo &info)
{
    return SendSingleNodeReportByOpCode(nodeId, info, UbseNodeControllerOpCode::NODE_CONTROLLER_SINGLE_NODE_REPORT,
                                        "global single report");
}

UbseResult UbseGlobalReportFullInfo(const std::string &nodeId, const std::vector<UbseNodeInfo> &infos)
{
    return SendNodeListReportByOpCode(nodeId, infos, UbseNodeControllerOpCode::NODE_CONTROLLER_GLOBAL_FULL_REPORT,
                                      "global full report");
}

// 创建错误响应
static UbseResult CreateErrorResponse(UbseResult errorCode, UbseByteBuffer &resp)
{
    uint8_t *errorBuffer = new (std::nothrow) uint8_t[4];
    if (errorBuffer != nullptr) {
        *reinterpret_cast<uint32_t*>(errorBuffer) =
            static_cast<uint32_t>(errorCode); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        resp = {errorBuffer, 4, [](uint8_t* p) noexcept {
                    delete[] p;
                }};
        return errorCode;
    } else {
        resp = {nullptr, 0, nullptr}; // 内存分配失败，只能返回空
        return UBSE_ERROR_NULLPTR;
    }
}

// Agent处理Master的采集请求
UbseResult CollectNodeInfoHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UbseNodeInfo info = UbseNodeController::GetInstance().GetCurNode();

    uint8_t *buffer = nullptr;
    size_t size = 0;
    auto ret = SerializeUbseNode(info, buffer, size);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SerializeUbseNode failed: " << FormatRetCode(ret);

        if (buffer != nullptr) {
            SafeDeleteArray(buffer, size);
        }

        return CreateErrorResponse(ret, resp);
    }

    resp = {buffer, size, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    return ret;
}

// GetAllNodeInfoFromRemote的辅助函数
static void GetAllNodeInfoFromRemoteRespHandler(const std::string& nodeId, const UbseByteBuffer& respData,
                                                uint32_t resCode, std::vector<UbseNodeInfo>& infos, UbseResult& getRet)
{
    if (resCode != UBSE_OK) {
        UBSE_LOG_ERROR << "get all node info failed, " << FormatRetCode(resCode);
        getRet = resCode;
        return;
    }
    if (respData.data == nullptr || respData.len == 0) {
        UBSE_LOG_ERROR << "get all node resp null";
        getRet = UBSE_ERROR_NULLPTR;
        return;
    }
    getRet = DeSerializeUbseNodeList(infos, respData.data, respData.len);
    if (getRet != UBSE_OK) {
        UBSE_LOG_ERROR << "get all node deserialize failed, " << FormatRetCode(getRet);
    }
}

UbseResult nodeChangeHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOG_INFO << "Receive node change req from master";

    uint8_t* buffer = nullptr;
    size_t size = 0;
    resp = {buffer, size, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};

    UbseDeSerialization inStream(req.data, req.len);
    std::string node;
    std::string action;
    inStream >> node >> action;

    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize node join response";
        return CreateErrorResponse(UBSE_ERROR, resp);
    }
    UbseResult ret = UBSE_OK;
    if (action == UBSE_EVENT_NODE_TOPO_LINK_CHANGE || action == UBSE_EVENT_NODE_JOIN) {
        ret = SetUrmaUvs();
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "SetUrmaUvs failed: " << FormatRetCode(ret);
        }
        ret = PubNodeUrmaChange(node, action);
    }

    return CreateErrorResponse(ret, resp);
}

// 向Master节点请求全量节点列表
UbseResult GetAllNodeInfoFromRemote(const std::string& nodeId, std::vector<UbseNodeInfo>& infos)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_ALL_NODE),
        .address = nodeId,
    };

    // 同步机制
    UbseResult getRet = UBSE_OK;
    bool callbackCalled = false;
    std::mutex mtx;
    std::condition_variable cv;

    uint8_t* buffer = nullptr;
    size_t size = 0;
    auto ret = SerializeUbseNodeList(std::vector<UbseNodeInfo>{}, buffer, size);
    if (ret != UBSE_OK) {
        // 错误路径：如果buffer已被分配，需要释放
        if (buffer != nullptr) {
            SafeDeleteArray(buffer, size);
        }
        return ret;
    }

    // 只有成功时，用UbseByteBuffer管理buffer
    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t* p) noexcept {
                                 SafeDeleteArray(p, size);
                             }};

    ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                      [&infos, &getRet, &callbackCalled, &mtx, &cv, nodeId](void* ctx, const UbseByteBuffer& respData,
                                                                            uint32_t resCode) -> void {
                          GetAllNodeInfoFromRemoteRespHandler(nodeId, respData, resCode, infos, getRet);
                          {
                              std::lock_guard<std::mutex> lock(mtx);
                              callbackCalled = true;
                          }
                          cv.notify_one();
                      });

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send get all node msg failed, " << FormatRetCode(ret);
        return ret;
    }

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&callbackCalled] { return callbackCalled; });
    }

    return getRet;
}

// 处理获取链路信息的回调
static void HandleGetDirConnectInfoCallback(const std::string& nodeId, const UbseByteBuffer& respData, uint32_t resCode,
                                            std::map<std::string, PhysicalLink>& devDirConnectInfoRemote,
                                            UbseResult& getRet, bool& callbackCalled, std::mutex& mtx,
                                            std::condition_variable& cv)
{
    if (resCode != UBSE_OK) {
        UBSE_LOG_ERROR << "get all node info failed, " << FormatRetCode(resCode);
        getRet = resCode;
    } else if (respData.data == nullptr || respData.len == 0) {
        UBSE_LOG_ERROR << "get all node resp null";
        getRet = UBSE_ERROR_NULLPTR;
    } else {
        getRet = DeSerializeDevDirConnectInfo(devDirConnectInfoRemote, respData.data, respData.len);
        if (getRet != UBSE_OK) {
            UBSE_LOG_ERROR << "get devDirConnectInfo deserialize failed, " << FormatRetCode(getRet);
        }
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        callbackCalled = true;
    }
    cv.notify_one();
}

// 向Master节点请求全量链路信息
UbseResult UbseGetDirConnectInfoFromRemote(const std::string& nodeId,
                                           std::map<std::string, PhysicalLink>& devDirConnectInfoRemote)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_GET_DEV_CONNECT),
        .address = nodeId,
    };

    UbseResult getRet = UBSE_OK;
    bool callbackCalled = false;
    std::mutex mtx;
    std::condition_variable cv;

    uint8_t* buffer = new (std::nothrow) uint8_t[1]; // com不允许空请求
    if (buffer == nullptr) {
        UBSE_LOG_ERROR << "Memory allocation failed.";
        return UBSE_ERROR_NULLPTR;
    }
    size_t size = 1;
    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t* p) noexcept {
                                 SafeDeleteArray(p, size);
                             }};

    auto ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                           [&devDirConnectInfoRemote, &getRet, &callbackCalled, &mtx, &cv,
                            nodeId](void* ctx, const UbseByteBuffer& respData, uint32_t resCode) -> void {
                               HandleGetDirConnectInfoCallback(nodeId, respData, resCode, devDirConnectInfoRemote,
                                                               getRet, callbackCalled, mtx, cv);
                           });

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send get all node msg failed, " << FormatRetCode(ret);
        return ret;
    }

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&callbackCalled] { return callbackCalled; });
    }

    return getRet;
}

UbseResult UbseNodeControllerAgent::ReportSingleNodeToPrev(const UbseNodeInfo &info)
{
    std::string prevNodeId;
    auto ret = GetPrevReportNodeId(prevNodeId);
    if (ret != UBSE_OK) {
        return ret;
    }

    if (prevNodeId == info.nodeId) {
        return UBSE_OK;
    }

    return UbseCabinetReportSingleNode(prevNodeId, info);
}

UbseResult UbseNodeControllerAgent::ReportCabinetFullInfo()
{
    UBSE_LOG_INFO << "timer cabinet trigger, start to report cabinet info.";
    auto allNodes = UbseNodeController::GetInstance().GetLocalNodeInfos();

    std::vector<UbseNodeInfo> infos{};
    infos.reserve(allNodes.size());
    for (const auto& [_, info] : allNodes) {
        if (info.groupId != nodeMgr::GetCurrentNode().groupId) {
            continue;
        }
        infos.push_back(info);
    }
    if (infos.empty()) {
        return UBSE_OK;
    }

    UbseRoleInfo globalMasterInfo;
    auto ret = UbseGetMasterInfo(globalMasterInfo);
    if (ret != UBSE_OK) {
        return ret;
    }

    if (globalMasterInfo.nodeId == UbseNodeController::GetInstance().GetCurrentNodeId()) {
        UBSE_LOG_INFO << "[CLOS_REPORT] skip cabinet full report to self, currentNodeId="
                      << UbseNodeController::GetInstance().GetCurrentNodeId() << ", nodeCount=" << infos.size()
                      << ", role=" << static_cast<uint32_t>(GetClosRole());
        return UBSE_OK;
    }

    std::stringstream nodeList;
    for (const auto &info : infos) {
        nodeList << info.nodeId << "(group=" << info.groupId << ",state=" << static_cast<uint32_t>(info.clusterState)
                 << "), ";
    }

    UBSE_LOG_INFO << "[CLOS_REPORT] cabinet full report node list, currentNodeId="
                  << UbseNodeController::GetInstance().GetCurrentNodeId() << ", prevNodeId=" << globalMasterInfo.nodeId
                  << ", nodeCount=" << infos.size() << ", nodes=" << nodeList.str();

    return UbseCabinetReportFullInfo(globalMasterInfo.nodeId, infos);
}

UbseResult UbseNodeControllerAgent::ForwardSingleNodeToPrev(const UbseNodeInfo &info)
{
    std::string prevNodeId;
    auto ret = GetPrevReportNodeId(prevNodeId);
    if (ret != UBSE_OK) {
        return ret;
    }

    return UbseGlobalReportSingleNode(prevNodeId, info);
}

UbseResult UbseNodeControllerAgent::ForwardCabinetFullToPrev()
{
    auto allNodes = UbseNodeController::GetInstance().GetLocalNodeInfos();

    std::vector<UbseNodeInfo> infos{};
    infos.reserve(allNodes.size());
    for (const auto& [_, info] : allNodes) {
        if (info.groupId != nodeMgr::GetCurrentNode().groupId) {
            continue;
        }
        infos.push_back(info);
    }
    if (infos.empty()) {
        return UBSE_OK;
    }

    std::string prevNodeId;
    auto ret = GetPrevReportNodeId(prevNodeId);
    if (ret != UBSE_OK) {
        return ret;
    }

    std::stringstream nodeList;
    for (const auto &info : infos) {
        nodeList << info.nodeId << "(group=" << info.groupId << ",state=" << static_cast<uint32_t>(info.clusterState)
                 << "), ";
    }

    UBSE_LOG_INFO << "[CLOS_REPORT] global full report node list, currentNodeId="
                  << UbseNodeController::GetInstance().GetCurrentNodeId() << ", globalNodeId=" << prevNodeId
                  << ", nodeCount=" << infos.size() << ", nodes=" << nodeList.str();

    return UbseGlobalReportFullInfo(prevNodeId, infos);
}

UbseResult SetUrmaUvs()
{
    std::vector<PhysicalLink> links;

    if (!UbseSmbios::GetInstance().IsClosType()) {
        std::map<std::string, PhysicalLink> connectInfo = UbseNodeController::GetInstance().UbseGetDirConnectInfo();
        if (connectInfo.empty()) {
            UBSE_LOG_WARN << "get cur node link size = 0";
        }

        for (const auto &entry : connectInfo) {
            links.push_back(entry.second);
        }

        UBSE_LOG_INFO << "get cur node topo success, update urma uvs";
    }

    auto ret = UbseNodeComUrmaCollector::GetInstance().SetComUrma(links, false);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "set to urma uvs failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult PubNodeUrmaChange(std::string& nodeId, std::string action)
{
    if (action != UBSE_EVENT_NODE_TOPO_LINK_CHANGE && action != UBSE_EVENT_NODE_JOIN) {
        UBSE_LOG_ERROR << "PubEvent " << action << " is not supported.";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "PubEvent " << action << " to urma when change";
    auto ret = UbsePubEvent(action, nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to PubEvent" << action << ", nodeId = " << nodeId << "," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}
} // namespace ubse::nodeController