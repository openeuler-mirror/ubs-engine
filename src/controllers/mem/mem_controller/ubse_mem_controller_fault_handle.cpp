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

#include "ubse_mem_controller_fault_handle.h"

#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <set>
#include <unordered_map>

#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_str_util.h"

#include "ubse_api_server_module.h"
#include "ubse_conf_module.h"
#include "ubse_election_module.h"
#include "ubse_election_node_mgr.h"
#include "ubse_logger.h"
#include "ubse_mmi_interface.h"

#include "ubse_com_op_code.h"
#include "ubse_event.h"
#include "ubse_ipc_common_def.h"
#include "ubse_ipc_utils.h"
#include "ubse_json_util.h"
#include "ubse_mem_controller_share_api.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_mem_single_import_message.h"
#include "ubse_serial_util.h"
#include "ubse_node_mgr.h"
#include "ubse_smbios.h"
#include "ubse_timer.h"

namespace ubse::mem::controller {
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::election;
using namespace ubse::context;
using namespace ubse::ipc;
using namespace ::api::server;
using namespace ubse::config;
using namespace ubse::com;
using namespace ubse::serial;
using namespace ubse::ras;
using namespace ubse::common::def;
using namespace ubse::task_executor;
using namespace ubse::nodeMgr;
using adapter_plugins::smbios::UbseSmbios;

UBSE_DEFINE_THIS_MODULE("ubse");

const std::string MEM_FAULT_ALAUBSE_NAME = "MemFaultAlarm";
const std::string BMC_FAULT_ALARM_NAME = "BmcFaultAlarm";
const std::string CLOS_IMPORT_MEM_FAULT_ALARM_NAME = "ClosImportMemFaultAlarm";
const std::string MEM_FAULT_DELIVER_TASK_PREFIX = "MemFaultDeliver";
const std::string MEM_FAULT_INFO_JSON_MEM_ID = "memid";
const std::string MEM_FAULT_INFO_JSON_RAS_TYPE = "raw_ubus_mem_err_type";
const std::string PANIC_REBOOT_FAULT_LOCAL_EVENT_ID = "UbsePanicAndRebootFaultLocalEvent";
// 故障转发定时器名称：BMC + Panic/Reboot 共用
const std::string FAULT_DELIVER_TIMER_NAME = "FaultDeliverTimer";
const std::string PORT_UP_DOWN_EVENT_ID = "topology.port.linkupdown";
// 故障转发链路（BMC + Panic/Reboot）共用：最大重试次数、超时时间、定时器周期
const uint32_t FAULT_DELIVER_MAX_RETRY_COUNT = 5;
const uint32_t FAULT_DELIVER_TIMEOUT_SECONDS = 60;
const uint32_t FAULT_DELIVER_TIMER_INTERVAL_SECONDS = 5;

static std::unordered_map<std::string, std::unordered_set<std::string>> g_portDownRecords;

static std::mutex g_portDownMutex;

static std::string MakePortDownKey(const std::string& slotId, const std::string& chipId)
{
    return slotId + ":" + chipId;
}

// 原子地检查端口是否已 Down，若否则记录并返回 true；已 Down 则返回 false
static bool TryAddPortDown(const PortEventInfo& info)
{
    std::scoped_lock lock(g_portDownMutex);
    auto key = MakePortDownKey(info.slotId, info.chipId);
    auto [it, inserted] = g_portDownRecords.try_emplace(key);
    if (it->second.find(info.portId) != it->second.end()) {
        return false; // 已 Down，非新增
    }
    it->second.insert(info.portId);
    if (inserted) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] New port down record created, slotId=" << info.slotId
                      << ", chipId=" << info.chipId << ", portId=" << info.portId << ".";
    } else {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] Port down recorded, slotId=" << info.slotId << ", chipId=" << info.chipId
                      << ", portId=" << info.portId << ".";
    }
    return true;
}

// 原子地检查端口是否已 Down，若是则移除并返回 true；否则返回 false
static bool TryErasePortDown(const PortEventInfo& info)
{
    std::scoped_lock lock(g_portDownMutex);
    auto key = MakePortDownKey(info.slotId, info.chipId);
    auto it = g_portDownRecords.find(key);
    if (it == g_portDownRecords.end()) {
        return false; // 不在记录中
    }
    auto portIt = it->second.find(info.portId);
    if (portIt == it->second.end()) {
        return false; // 该端口未 Down
    }
    it->second.erase(portIt);
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Port up removed from down record, slotId=" << info.slotId
                  << ", chipId=" << info.chipId << ", portId=" << info.portId << ".";
    if (it->second.empty()) {
        g_portDownRecords.erase(it);
        UBSE_LOG_INFO << "[MEM_CONTROLLER] All ports up, removed down record, slotId=" << info.slotId
                      << ", chipId=" << info.chipId << ".";
    }
    return true;
}

// 待转发的故障事件（BMC + Panic/Reboot 共用）
struct MemFaultEvent {
    std::string faultNodeId;
    uint32_t faultTypeValue{};
    std::set<std::string> sentNodeIds;
    uint32_t retryCount{};
    std::chrono::steady_clock::time_point createTime;
};

// 故障类型转人类可读名称，便于日志追溯（避免仅出现 1003/1007/1009 等数字码）
static std::string FaultTypeName(uint32_t faultTypeValue)
{
    switch (faultTypeValue) {
        case static_cast<uint32_t>(ALARM_REBOOT_EVENT):
            return "BMC_POWER_OFF";
        case static_cast<uint32_t>(ALARM_PANIC_EVENT):
            return "PANIC";
        case static_cast<uint32_t>(ALARM_KERNEL_REBOOT_EVENT):
            return "KERNEL_REBOOT";
        default:
            return "UNKNOWN";
    }
}

// 待转发故障事件队列（BMC + Panic/Reboot 共用），主节点按节点 ID 去重
static std::unordered_map<std::string, MemFaultEvent> g_pendingFaultEvents;
static std::mutex g_faultMutex;

// 组内待转发队列（Clos 双层场景，组长节点转发给组内 agent）
static std::unordered_map<std::string, MemFaultEvent> g_pendingGroupForwardEvents;
static std::mutex g_groupForwardMutex;

UbseTaskExecutorPtr UbseMemFaultManager::executorPtr = nullptr;

static UbseResult GetCurrentNodeId(std::string& nodeId)
{
    UbseRoleInfo curNodeInfo{};
    auto ret = UbseGetCurrentNodeInfo(curNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get the information of current node.";
        return UBSE_ERROR;
    }
    nodeId = curNodeInfo.nodeId;
    return ret;
}

static UbseComEndpoint GetMemFaultComEndpoint(uint16_t opCode, const std::string& nodeId)
{
    return UbseComEndpoint{
        .moduleId = static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_FAULT),
        .serviceId = static_cast<uint32_t>(opCode),
        .address = nodeId,
    };
}

static bool IsHierarchicalMode()
{
    auto rootList = ubse::nodeMgr::GetRootIpList();
    auto nodes = ubse::nodeMgr::GetAllNodes();
    return rootList.empty() && nodes.size() > 1;
}

// SRP: 场景判定单一职责，集中在一处
enum class FaultMode {
    CLOS_DOUBLE_LAYER, // Clos双层: 双层选主
    CLOS_SINGLE_LAYER, // Clos单层: Clos硬件 + 单层选主
    NON_CLOS            // 非Clos: 非Clos硬件 + 单层选主
};

static FaultMode DetectFaultMode()
{
    if (IsHierarchicalMode()) {
        return FaultMode::CLOS_DOUBLE_LAYER;
    }
    if (UbseSmbios::GetInstance().IsClosType()) {
        return FaultMode::CLOS_SINGLE_LAYER;
    }
    return FaultMode::NON_CLOS;
}

static bool IsGlobalMasterNode()
{
    auto& ctxRef = UbseContext::GetInstance();
    auto electionModule = ctxRef.GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] IsGlobalMasterNode election module is null.";
        return false;
    }
    HaTopologyInfo topoInfo;
    if (electionModule->GetCurNodeGlobalTopoInfo(topoInfo) != UBSE_OK) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] IsGlobalMasterNode failed to get current node global topo info.";
        return false;
    }
    bool isGlobalMaster = topoInfo.currentNode.globalRole == GlobalRoleType::GLOBAL_MASTER;
    UBSE_LOG_DEBUG << "[MEM_CONTROLLER] IsGlobalMasterNode currentNodeId=" << topoInfo.currentNode.nodeId
                   << ", globalRole=" << static_cast<uint32_t>(topoInfo.currentNode.globalRole)
                   << ", isGlobalMaster=" << isGlobalMaster;
    return isGlobalMaster;
}

static bool IsLocalMasterNode()
{
    UbseRoleInfo curNodeInfo{};
    if (UbseGetCurrentNodeInfo(curNodeInfo) != UBSE_OK) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] IsLocalMasterNode failed to get current node info.";
        return false;
    }
    bool isLocalMaster = curNodeInfo.nodeRole == ELECTION_ROLE_MASTER;
    UBSE_LOG_DEBUG << "[MEM_CONTROLLER] IsLocalMasterNode currentNodeId=" << curNodeInfo.nodeId
                   << ", role=" << curNodeInfo.nodeRole << ", isLocalMaster=" << isLocalMaster;
    return isLocalMaster;
}

// 将故障事件压入待转发队列（BMC + Panic/Reboot 共用），按 faultNodeId 去重
static void PushFaultEvent(const std::string& faultNodeId, uint32_t faultTypeValue)
{
    std::scoped_lock lock(g_faultMutex);

    auto it = g_pendingFaultEvents.find(faultNodeId);
    if (it != g_pendingFaultEvents.end()) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] PushFaultEvent event already exists, faultNodeId=" << faultNodeId
                      << ", faultType=" << faultTypeValue << "(" << FaultTypeName(faultTypeValue)
                      << "), refreshing createTime.";
        it->second.createTime = std::chrono::steady_clock::now();
        it->second.retryCount = 0;
        return;
    }

    MemFaultEvent event;
    event.faultNodeId = faultNodeId;
    event.faultTypeValue = faultTypeValue;
    event.retryCount = 0;
    event.createTime = std::chrono::steady_clock::now();

    g_pendingFaultEvents[faultNodeId] = event;
    UBSE_LOG_INFO << "[MEM_CONTROLLER] PushFaultEvent pushed to pending queue, faultNodeId=" << faultNodeId
                  << ", faultType=" << faultTypeValue << "(" << FaultTypeName(faultTypeValue)
                  << "), pendingCount=" << g_pendingFaultEvents.size();
}

static UbseResult SendFaultToNode(const std::string& nodeId, const std::string& faultNodeId, uint32_t faultTypeValue)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] SendFaultToNode sending to nodeId=" << nodeId
                  << ", faultNodeId=" << faultNodeId << ", faultType=" << faultTypeValue
                  << "(" << FaultTypeName(faultTypeValue) << ").";

    UbseSerialization output;
    output << faultNodeId << faultTypeValue;
    if (!output.Check()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] SendFaultToNode failed to serialize, nodeId=" << nodeId
                       << ", faultNodeId=" << faultNodeId << ", faultType=" << faultTypeValue
                       << "(" << FaultTypeName(faultTypeValue) << ").";
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    // 异步回调不处理
    auto respHandler = [](void*, const UbseByteBuffer&, uint32_t statusCode) -> void {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] SendFaultToNode response, statusCode=" << statusCode;
    };
    auto endpoint =
        GetMemFaultComEndpoint(static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_MEM_FAULT_BMC_AGENTS), nodeId);
    UbseByteBuffer request{.data = output.GetBuffer(), .len = output.GetLength(), .freeFunc = nullptr};
    return UbseRpcAsyncSend(endpoint, request, nullptr, respHandler);
}

static bool GetAllManagingGroupMasterIds(std::set<std::string>& masterIds)
{
    auto& ctxRef = UbseContext::GetInstance();
    auto electionModule = ctxRef.GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get election module.";
        return false;
    }
    HaTopologyInfo topoInfo;
    if (electionModule->GetCurNodeGlobalTopoInfo(topoInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get global topology info.";
        return false;
    }
    if (!topoInfo.currentGroup.groupMasterId.empty()) {
        masterIds.insert(topoInfo.currentGroup.groupMasterId);
    }
    for (const auto& group : topoInfo.groups) {
        if (!group.groupMasterId.empty()) {
            masterIds.insert(group.groupMasterId);
        }
    }
    UBSE_LOG_DEBUG << "[MEM_CONTROLLER] GetAllManagingGroupMasterIds success, count=" << masterIds.size();
    return true;
}

// SRP: 从 UbseGetAllNodeInfos 获取全部节点ID，独立出来
static bool GetAllNodeIdsFromRoleInfos(std::set<std::string>& allNodeIds)
{
    std::vector<UbseRoleInfo> roleInfos;
    if (UbseGetAllNodeInfos(roleInfos) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] GetAllNodeIds failed to get all node infos.";
        return false;
    }
    for (const auto& node : roleInfos) {
        if (!node.nodeId.empty()) {
            allNodeIds.insert(node.nodeId);
        }
    }
    UBSE_LOG_DEBUG << "[MEM_CONTROLLER] GetAllNodeIds success, nodeCount=" << allNodeIds.size();
    return true;
}

// DIP/OCP: 策略接口，handler 依赖此抽象，新增场景只需新增子类
// 拆分语义：BMC 故障与 Panic 事件分发模型不同，分别建模
//   - BMC 故障：所有场景下只有主节点收到，由主清理本节点债务并转发给其他节点
//   - Panic/Reboot 事件：非 Clos 下所有节点都收到（各自清理，无需转发）；
//                        Clos 下只有主收到（主清理并转发给其他节点）
class FaultModeStrategy {
public:
    virtual ~FaultModeStrategy() = default;
    virtual std::string Name() const = 0;
    // BMC 故障入口：本节点是否处理 BMC 告警（主收到 → 转发给其他节点）
    virtual bool ShouldHandleBmc() const = 0;
    // 故障转发队列：本节点是否驱动待转发队列（主节点职责）
    virtual bool ShouldProcessFaultQueue() const = 0;
    // Panic：本节点是否需要清理本节点导入债务
    virtual bool ShouldHandlePanicLocal() const = 0;
    // Panic：是否需要转发给其他节点（其他节点收不到原始事件）
    virtual bool ShouldForwardPanic() const = 0;
    // 是否继续转发给组内节点（clos 双层）
    virtual bool ShouldForwardToGroupNodes() const = 0;
    virtual bool GetTargetNodeIds(std::set<std::string>& ids) const = 0;
};

class ClosDoubleLayerStrategy : public FaultModeStrategy {
public:
    std::string Name() const override { return "CLOS_DOUBLE_LAYER"; }
    bool ShouldHandleBmc() const override { return IsGlobalMasterNode(); }
    bool ShouldProcessFaultQueue() const override { return IsGlobalMasterNode(); }
    // Clos 双层：全局主只转发给组长，不清理本节点债务，由组长/agent 通过 FaultAgentsNotifyHandler 清理
    bool ShouldHandlePanicLocal() const override { return false; }
    bool ShouldForwardPanic() const override { return IsGlobalMasterNode(); }
    bool ShouldForwardToGroupNodes() const override { return true; }
    bool GetTargetNodeIds(std::set<std::string>& ids) const override
    {
        return GetAllManagingGroupMasterIds(ids);
    }
};

class ClosSingleLayerStrategy : public FaultModeStrategy {
public:
    std::string Name() const override { return "CLOS_SINGLE_LAYER"; }
    bool ShouldHandleBmc() const override { return IsLocalMasterNode(); }
    bool ShouldProcessFaultQueue() const override { return IsLocalMasterNode(); }
    // Clos 单层：本地主只转发给其他节点，不清理本节点债务，由其他节点通过 FaultAgentsNotifyHandler 清理
    bool ShouldHandlePanicLocal() const override { return false; }
    bool ShouldForwardPanic() const override { return IsLocalMasterNode(); }
    bool ShouldForwardToGroupNodes() const override { return false; }
    bool GetTargetNodeIds(std::set<std::string>& ids) const override
    {
        return GetAllNodeIdsFromRoleInfos(ids);
    }
};

class NonClosStrategy : public FaultModeStrategy {
public:
    std::string Name() const override { return "NON_CLOS"; }
    bool ShouldHandleBmc() const override { return IsLocalMasterNode(); }
    bool ShouldProcessFaultQueue() const override { return IsLocalMasterNode(); }
    // 非 Clos：所有节点都收到 Panic，都需清理本节点导入债务
    bool ShouldHandlePanicLocal() const override { return true; }
    // 非 Clos：所有节点都收到，无需转发
    bool ShouldForwardPanic() const override { return false; }
    bool ShouldForwardToGroupNodes() const override { return false; }
    bool GetTargetNodeIds(std::set<std::string>& ids) const override
    {
        return GetAllNodeIdsFromRoleInfos(ids);
    }
};

static const FaultModeStrategy& GetStrategy()
{
    static const ClosDoubleLayerStrategy sDoubleLayer;
    static const ClosSingleLayerStrategy sSingleLayer;
    static const NonClosStrategy sNonClos;
    switch (DetectFaultMode()) {
        case FaultMode::CLOS_DOUBLE_LAYER:
            return sDoubleLayer;
        case FaultMode::CLOS_SINGLE_LAYER:
            return sSingleLayer;
        case FaultMode::NON_CLOS:
            return sNonClos;
    }
    return sNonClos;
}

// Clos 双层：组长节点将故障转发给组内 agent（BMC + Panic/Reboot 共用）
static void ForwardFaultToGroupNodes(const std::string& faultNodeId, uint32_t faultTypeValue)
{
    UbseRoleInfo curNodeInfo{};
    if (UbseGetCurrentNodeInfo(curNodeInfo) != UBSE_OK || curNodeInfo.nodeRole != ELECTION_ROLE_MASTER) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] ForwardFaultToGroupNodes skip, current node is not local master,"
                      << " faultNodeId=" << faultNodeId << ", faultType=" << faultTypeValue
                      << "(" << FaultTypeName(faultTypeValue) << ").";
        return;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] ForwardFaultToGroupNodes pushing to group forward queue,"
                  << " faultNodeId=" << faultNodeId << ", faultType=" << faultTypeValue
                  << "(" << FaultTypeName(faultTypeValue) << ").";

    MemFaultEvent event;
    event.faultNodeId = faultNodeId;
    event.faultTypeValue = faultTypeValue;
    event.retryCount = 0;
    event.createTime = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(g_groupForwardMutex);
    auto it = g_pendingGroupForwardEvents.find(faultNodeId);
    if (it != g_pendingGroupForwardEvents.end()) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] ForwardFaultToGroupNodes event already exists, refreshing,"
                      << " faultNodeId=" << faultNodeId << ", faultType=" << faultTypeValue
                      << "(" << FaultTypeName(faultTypeValue) << ").";
        it->second.createTime = std::chrono::steady_clock::now();
        it->second.retryCount = 0;
        return;
    }
    g_pendingGroupForwardEvents[faultNodeId] = event;
    UBSE_LOG_INFO << "[MEM_CONTROLLER] ForwardFaultToGroupNodes pushed, pendingCount="
                  << g_pendingGroupForwardEvents.size() << ", faultNodeId=" << faultNodeId
                  << ", faultType=" << faultTypeValue << "(" << FaultTypeName(faultTypeValue) << ").";
}

static bool GetGroupNodeIds(std::set<std::string>& groupNodeIds)
{
    UbseRoleInfo curNodeInfo{};
    if (UbseGetCurrentNodeInfo(curNodeInfo) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] GetGroupNodeIds failed to get current node info.";
        return false;
    }
    auto& ctxRef = UbseContext::GetInstance();
    auto electionModule = ctxRef.GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] GetGroupNodeIds failed to get election module.";
        return false;
    }
    Node masterNode, standbyNode;
    std::vector<Node> agentNodes;
    if (electionModule->UbseGetAllNodes(masterNode, standbyNode, agentNodes) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] GetGroupNodeIds failed to get all nodes.";
        return false;
    }
    if (!standbyNode.id.empty() && standbyNode.id != curNodeInfo.nodeId) {
        groupNodeIds.insert(standbyNode.id);
    }
    for (const auto& agent : agentNodes) {
        if (!agent.id.empty() && agent.id != curNodeInfo.nodeId) {
            groupNodeIds.insert(agent.id);
        }
    }
    UBSE_LOG_DEBUG << "[MEM_CONTROLLER] GetGroupNodeIds success, count=" << groupNodeIds.size()
                   << ", currentNodeId=" << curNodeInfo.nodeId;
    return true;
}

static void SendFaultToPendingNodes(const std::string& faultNodeId, MemFaultEvent& event,
                                    const std::set<std::string>& allNodeIds);

static void ProcessGroupForwardEvents()
{
    if (!GetStrategy().ShouldForwardToGroupNodes()) {
        return;
    }
    UbseRoleInfo curNodeInfo{};
    if (UbseGetCurrentNodeInfo(curNodeInfo) != UBSE_OK || curNodeInfo.nodeRole != ELECTION_ROLE_MASTER) {
        std::scoped_lock lock(g_groupForwardMutex);
        if (!g_pendingGroupForwardEvents.empty()) {
            UBSE_LOG_INFO << "[MEM_CONTROLLER] ProcessGroupForwardEvents current node is not local master,"
                          << " clearing pending events, count=" << g_pendingGroupForwardEvents.size();
            g_pendingGroupForwardEvents.clear();
        }
        return;
    }

    std::set<std::string> groupNodeIds;
    if (!GetGroupNodeIds(groupNodeIds)) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] ProcessGroupForwardEvents failed to resolve group node ids, skip.";
        return;
    }
    if (groupNodeIds.empty()) {
        std::scoped_lock lock(g_groupForwardMutex);
        UBSE_LOG_INFO << "[MEM_CONTROLLER] ProcessGroupForwardEvents group node ids empty, clearing pending,"
                      << " count=" << g_pendingGroupForwardEvents.size();
        g_pendingGroupForwardEvents.clear();
        return;
    }

    std::vector<std::string> eventsToRemove;
    auto now = std::chrono::steady_clock::now();

    {
        std::scoped_lock lock(g_groupForwardMutex);
        if (g_pendingGroupForwardEvents.empty()) {
            return;
        }
        UBSE_LOG_INFO << "[MEM_CONTROLLER] ProcessGroupForwardEvents start, pendingCount="
                       << g_pendingGroupForwardEvents.size() << ", targetCount=" << groupNodeIds.size();
        for (auto& [faultNodeId, event] : g_pendingGroupForwardEvents) {
            SendFaultToPendingNodes(faultNodeId, event, groupNodeIds);
            auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now - event.createTime).count();
            if (event.sentNodeIds.size() >= groupNodeIds.size() ||
                event.retryCount >= FAULT_DELIVER_MAX_RETRY_COUNT ||
                elapsedSec > FAULT_DELIVER_TIMEOUT_SECONDS) {
                eventsToRemove.push_back(faultNodeId);
                UBSE_LOG_INFO << "[MEM_CONTROLLER] ProcessGroupForwardEvents event done, faultNodeId="
                              << faultNodeId << ", faultType=" << event.faultTypeValue
                              << "(" << FaultTypeName(event.faultTypeValue) << "), sentCount="
                              << event.sentNodeIds.size() << ", retryCount=" << event.retryCount
                              << ", elapsed=" << elapsedSec << "s.";
            }
            event.retryCount++;
        }
        for (const auto& faultNodeId : eventsToRemove) {
            g_pendingGroupForwardEvents.erase(faultNodeId);
            UBSE_LOG_WARN << "[MEM_CONTROLLER] ProcessGroupForwardEvents removed completed event, faultNodeId="
                          << faultNodeId;
        }
    }
}

static void SendFaultToPendingNodes(const std::string& faultNodeId, MemFaultEvent& event,
                                    const std::set<std::string>& allNodeIds)
{
    uint32_t sentCount = 0;
    uint32_t failedCount = 0;
    for (const auto& nodeId : allNodeIds) {
        if (event.sentNodeIds.find(nodeId) != event.sentNodeIds.end()) {
            continue;
        }
        auto ret = SendFaultToNode(nodeId, faultNodeId, event.faultTypeValue);
        if (ret == UBSE_OK) {
            event.sentNodeIds.insert(nodeId);
            sentCount++;
            UBSE_LOG_INFO << "[MEM_CONTROLLER] SendFaultToPendingNodes sent to nodeId=" << nodeId
                          << ", faultNodeId=" << faultNodeId << ", faultType=" << event.faultTypeValue
                          << "(" << FaultTypeName(event.faultTypeValue) << ").";
        } else {
            failedCount++;
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] SendFaultToPendingNodes failed to send to nodeId=" << nodeId
                           << ", faultNodeId=" << faultNodeId << ", faultType=" << event.faultTypeValue
                           << "(" << FaultTypeName(event.faultTypeValue) << ").";
        }
    }
    if (sentCount > 0 || failedCount > 0) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] SendFaultToPendingNodes done, faultNodeId=" << faultNodeId
                      << ", faultType=" << event.faultTypeValue << "(" << FaultTypeName(event.faultTypeValue)
                      << "), sentCount=" << sentCount << ", failedCount=" << failedCount;
    }
}

static bool ProcessSingleFaultEvent(const std::string& faultNodeId, MemFaultEvent& event,
                                    const std::set<std::string>& allNodeIds,
                                    const std::chrono::steady_clock::time_point& now)
{
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - event.createTime).count();
    if (elapsed > FAULT_DELIVER_TIMEOUT_SECONDS || event.retryCount >= FAULT_DELIVER_MAX_RETRY_COUNT) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] ProcessSingleFaultEvent timeout or max retry reached, faultNodeId="
                      << faultNodeId << ", faultType=" << event.faultTypeValue
                      << "(" << FaultTypeName(event.faultTypeValue) << "), elapsed=" << elapsed
                      << "s, retryCount=" << event.retryCount << ", sentNodeCount=" << event.sentNodeIds.size();
        return true; // 防止主备倒换集群节点不全，结束条件为达到最大次数，或者超时
    }

    SendFaultToPendingNodes(faultNodeId, event, allNodeIds);

    event.retryCount++;
    UBSE_LOG_INFO << "[MEM_CONTROLLER] ProcessSingleFaultEvent processed, faultNodeId=" << faultNodeId
                  << ", faultType=" << event.faultTypeValue << "(" << FaultTypeName(event.faultTypeValue)
                  << "), sentCount=" << event.sentNodeIds.size() << ", totalNodeCount=" << allNodeIds.size()
                  << ", retryCount=" << event.retryCount;
    return false;
}

// 主节点处理待转发故障事件队列（BMC + Panic/Reboot 共用）
static void ProcessFaultEvents()
{
    UbseRoleInfo curNodeInfo{};
    auto ret = UbseGetCurrentNodeInfo(curNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] ProcessFaultEvents failed to get current node info.";
        return;
    }

    if (!GetStrategy().ShouldProcessFaultQueue()) {
        std::scoped_lock lock(g_faultMutex);
        if (!g_pendingFaultEvents.empty()) {
            UBSE_LOG_INFO << "[MEM_CONTROLLER] ProcessFaultEvents current node is not fault handler"
                          << " (nodeId=" << curNodeInfo.nodeId << ", role=" << curNodeInfo.nodeRole
                          << "), clearing pending events, count=" << g_pendingFaultEvents.size();
            g_pendingFaultEvents.clear();
        }
        return;
    }

    std::set<std::string> allNodeIds;
    if (!GetStrategy().GetTargetNodeIds(allNodeIds)) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] ProcessFaultEvents failed to get all node ids, skip.";
        return;
    }

    std::vector<std::string> eventsToRemove;
    auto now = std::chrono::steady_clock::now();

    {
        std::scoped_lock lock(g_faultMutex);
        if (g_pendingFaultEvents.empty()) {
            return;
        }
        UBSE_LOG_INFO << "[MEM_CONTROLLER] ProcessFaultEvents start, pendingCount="
                      << g_pendingFaultEvents.size() << ", targetCount=" << allNodeIds.size()
                      << ", currentNodeId=" << curNodeInfo.nodeId;
        for (auto& [faultNodeId, event] : g_pendingFaultEvents) {
            if (ProcessSingleFaultEvent(faultNodeId, event, allNodeIds, now)) {
                eventsToRemove.push_back(faultNodeId);
            }
        }
        for (const auto& faultNodeId : eventsToRemove) {
            g_pendingFaultEvents.erase(faultNodeId);
            UBSE_LOG_WARN << "[MEM_CONTROLLER] ProcessFaultEvents removed completed event, faultNodeId="
                          << faultNodeId;
        }
    }
}

template <uint32_t OpCode, ubse_ipc_module_code_t ModuleCode>
static UbseResult SendSingleFaultMemBlockMessage(const std::string& handleType, const UbseMemFault& faultMem,
                                                 const UbseUdsInfo& udsInfo)
{
    auto& ctxRef = UbseContext::GetInstance();
    std::shared_ptr<UbseApiServerModule> apiServerPtr = ctxRef.GetModule<UbseApiServerModule>();
    if (apiServerPtr == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get api server module when sending " << handleType
                       << " fault message, memName=" << faultMem.memName << ", handleId=" << faultMem.handleId << "."
                       << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    uint8_t* buffer = nullptr;
    size_t size = 0;
    auto ret = SerializeMemFault(faultMem, buffer, size);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to serialize " << handleType
                       << " fault message, memName=" << faultMem.memName << ", handleId=" << faultMem.handleId << "."
                       << FormatRetCode(ret);
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbseRequestMessage longLinkReq{};
    longLinkReq.header.opCode = OpCode;
    longLinkReq.header.moduleCode = ModuleCode;
    longLinkReq.body = buffer;
    longLinkReq.header.bodyLen = size;
    std::vector<uint64_t> reqList;
    const UbseAsyncResponseHandler respHandler = [](void*, const UbseResponseMessage& resp) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] Received long link response, retCode=" << resp.header.statusCode;
    };
    UbseClientInfo clientInfo{.uid = udsInfo.uid, .gid = udsInfo.gid, .pid = udsInfo.pid};
    ret = apiServerPtr->AsyncSendLongLink(longLinkReq, clientInfo, nullptr, respHandler, reqList);
    SafeDelete(buffer);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to send " << handleType
                       << " fault long link message, memName=" << faultMem.memName << ", handleId=" << faultMem.handleId
                       << "." << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

template <uint32_t OpCode, ubse_ipc_module_code_t ModuleCode, UbMemFaultType FaultType = MEM_EXPORT_FAULT,
          typename HandleInfoVec, typename IdGetter>
static UbseResult ExtractFaultMemBlockInVecAndCallSengFunc(const HandleInfoVec& handleInfo,
                                                           const std::string& handleType, IdGetter&& getIdList)
{
    uint32_t sentCount = 0;
    for (const auto& info : handleInfo) {
        for (const auto& id : getIdList(info)) {
            UbseMemFault fault{
                .memName = info.name,
                .handleId = static_cast<uint64_t>(id),
                .type = static_cast<UbseIpcMemFaultType>(FaultType),
            };
            auto ret = SendSingleFaultMemBlockMessage<OpCode, ModuleCode>(handleType, fault, info.udsInfo);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to send " << handleType
                               << " fault message at memName=" << info.name << ", handleId=" << id
                               << ", sentCount=" << sentCount << "." << FormatRetCode(ret);
                continue;
            }
            sentCount++;
        }
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Successfully sent " << handleType << " fault messages, total=" << sentCount
                  << ".";
    return UBSE_OK;
}

template <uint32_t OpCode, ubse_ipc_module_code_t ModuleCode, typename HandleInfoVec, typename QueryFunc,
          typename IdGetter>
static UbseResult ReportPanicRebootBmcHandles(const std::string& faultId, const std::string& currentNodeId,
                                              const std::string& handleType, QueryFunc queryFunc, IdGetter getIdList)
{
    HandleInfoVec handleInfo;
    auto ret = queryFunc(currentNodeId, faultId, handleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to query " << handleType << " handle info, faultId=" << faultId
                       << ", exportNodeId=" << currentNodeId << "." << FormatRetCode(ret);
        return ret;
    }

    return ExtractFaultMemBlockInVecAndCallSengFunc<OpCode, ModuleCode>(handleInfo, handleType, getIdList);
}

void SubmitMemReportTaskWhenNodeStopsMemoryService(const std::string& faultId)
{
    std::string currentNodeId;
    auto ret = UbseGetCurrentNodeId(currentNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get current node id for faultId=" << faultId << "."
                       << FormatRetCode(ret);
        return;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Start fault handle report, faultId=" << faultId
                  << ", currentNodeId=" << currentNodeId << ".";
    ret = ReportPanicRebootBmcHandles<UBSE_LONGLINK_FAULT_SHM, UBSE_LONG_LINK_REGISTER, debt::ShareHandleInfoVec>(
        faultId, currentNodeId, "share", debt::UbseQueryShareImportHandleByExportNodeId,
        [](const auto& info) -> const auto& { return info.memIds; });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to report panic reboot share handles, faultId=" << faultId << "."
                       << FormatRetCode(ret);
    }
    ret = ReportPanicRebootBmcHandles<UBSE_LONGLINK_FAULT_NUMA, UBSE_LONG_LINK_REGISTER, debt::NumaHandleInfoVec>(
        faultId, currentNodeId, "numa", debt::UbseQueryNumaImportHandleByExportNodeId,
        [](const auto& info) -> const auto& { return info.numaIds; });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to report panic reboot numa handles, faultId=" << faultId << "."
                       << FormatRetCode(ret);
    }
    ret = ReportPanicRebootBmcHandles<UBSE_LONGLINK_FAULT_FD, UBSE_LONG_LINK_REGISTER, debt::FdHandleInfoVec>(
        faultId, currentNodeId, "fd", debt::UbseQueryFdImportHandleByExportNodeId,
        [](const auto& info) -> const auto& { return info.memIds; });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to report panic reboot fd handles, faultId=" << faultId << "."
                       << FormatRetCode(ret);
    }
}

UbseResult UbseMemFaultManager::CreateTaskExecutor(const std::string& name)
{
    auto& ctxRef = UbseContext::GetInstance();
    auto taskExec = ctxRef.GetModule<UbseTaskExecutorModule>();
    if (taskExec == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get task executor module, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }

    auto ret = taskExec->Create(name, NO_1, NO_1000);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to create executor tasks which memName=" << name << "."
                       << FormatRetCode(ret);
        return ret;
    }

    executorPtr = taskExec->Get(name);
    return ret;
}

UbseResult UbseMemFaultManager::RemoveTaskExecutor(const std::string& name)
{
    auto& ctxRef = UbseContext::GetInstance();
    auto taskExec = ctxRef.GetModule<UbseTaskExecutorModule>();
    if (taskExec == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get task executor module, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }

    taskExec->Remove(name);
    return UBSE_OK;
}

UbseResult UbseMemFaultManager::InitMemFaultManager()
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Started to register mem fault alarm.";
    auto ret = CreateTaskExecutor(MEM_FAULT_DELIVER_TASK_PREFIX);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to create task executors. " << FormatRetCode(ret);
        return ret;
    }

    std::vector<UbseResult> regRets;
    // Agent 节点接收主节点转发的故障通知（BMC + Panic/Reboot 共用）
    regRets.emplace_back(UbseRegRpcService(
        GetMemFaultComEndpoint(static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_MEM_FAULT_BMC_AGENTS), ""),
        FaultAgentsNotifyHandler));
    // 单导入债务通知处理函数（agent节点接收主节点通知）
    regRets.emplace_back(UbseRegRpcService(
        GetMemFaultComEndpoint(static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_SINGLE_IMPORT_DEBT_NOTIFY), ""),
        SingleImportDebtNotifyHandler));
    for (auto regRet : regRets) {
        if (regRet != UBSE_OK) {
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to register handlers. " << FormatRetCode(regRet);
            return regRet;
        }
    }

    ret = RegisterAlarmFaultHandler(ALARM_MEM_FAULT, MEM_FAULT_ALAUBSE_NAME, MemFaultHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to register mem fault alarm. " << FormatRetCode(ret);
        return ret;
    }

    ret = RegisterAlarmFaultHandler(ALARM_REBOOT_EVENT, BMC_FAULT_ALARM_NAME, BmcFaultHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to register BMC fault alarm. " << FormatRetCode(ret);
        return ret;
    }

    std::string eventId = PANIC_REBOOT_FAULT_LOCAL_EVENT_ID;
    ret = event::UbseSubEvent(eventId, PanicRebootFaultEventHandler, event::HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to subscribe panic reboot fault event. " << FormatRetCode(ret);
        return ret;
    }
    eventId = PORT_UP_DOWN_EVENT_ID;
    ret = event::UbseSubEvent(eventId, PortDownUpEventHandle, event::HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to subscribe port down or up event. " << FormatRetCode(ret);
        return ret;
    }
    ret = timer::UbseTimerHandlerRegister(FAULT_DELIVER_TIMER_NAME, FaultDeliverTimerHandler,
                                          FAULT_DELIVER_TIMER_INTERVAL_SECONDS);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to register fault deliver timer. " << FormatRetCode(ret);
        return ret;
    }

    if (UbseSmbios::GetInstance().IsClosType()) {
        ret = RegisterAlarmFaultHandler(ALARM_PANIC_EVENT, CLOS_IMPORT_MEM_FAULT_ALARM_NAME,
                                         ClosImportMemFaultHandler);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to register clos import mem fault alarm (panic). "
                           << FormatRetCode(ret);
            return ret;
        }

        ret = RegisterAlarmFaultHandler(ALARM_KERNEL_REBOOT_EVENT, CLOS_IMPORT_MEM_FAULT_ALARM_NAME,
                                         ClosImportMemFaultHandler);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to register clos import mem fault alarm (kernel reboot). "
                           << FormatRetCode(ret);
            return ret;
        }
    }

    UBSE_LOG_INFO << "[MEM_CONTROLLER] Succeed to register mem fault alarm.";
    return ret;
}

UbseResult UbseMemFaultManager::DeInitMemFaultManager()
{
    timer::UbseTimerHandlerUnregister(FAULT_DELIVER_TIMER_NAME);

    {
        std::scoped_lock lock(g_faultMutex);
        g_pendingFaultEvents.clear();
    }

    {
        std::scoped_lock lock(g_groupForwardMutex);
        g_pendingGroupForwardEvents.clear();
    }

    std::string alarmName = MEM_FAULT_ALAUBSE_NAME;
    auto ret = UnRegisterAlarmFaultHandler(ALARM_MEM_FAULT, alarmName);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to unregister mem fault alarm. " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    alarmName = BMC_FAULT_ALARM_NAME;
    ret = UnRegisterAlarmFaultHandler(ALARM_REBOOT_EVENT, alarmName);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to unregister BMC fault alarm. " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    if (UbseSmbios::GetInstance().IsClosType()) {
        alarmName = CLOS_IMPORT_MEM_FAULT_ALARM_NAME;
        ret = UnRegisterAlarmFaultHandler(ALARM_PANIC_EVENT, alarmName);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to unregister clos import mem fault alarm (panic). "
                           << FormatRetCode(ret);
            return UBSE_ERROR;
        }
        ret = UnRegisterAlarmFaultHandler(ALARM_KERNEL_REBOOT_EVENT, alarmName);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to unregister clos import mem fault alarm (kernel reboot). "
                           << FormatRetCode(ret);
            return UBSE_ERROR;
        }
    }

    std::string eventId = PANIC_REBOOT_FAULT_LOCAL_EVENT_ID;
    ret = event::UbseUnSubEvent(eventId, PanicRebootFaultEventHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to unsubscribe panic reboot fault event. " << FormatRetCode(ret);
        return ret;
    }

    eventId = PORT_UP_DOWN_EVENT_ID;
    ret = event::UbseUnSubEvent(eventId, PortDownUpEventHandle);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to unsubscribe port down or up event. " << FormatRetCode(ret);
        return ret;
    }

    ret = RemoveTaskExecutor(MEM_FAULT_DELIVER_TASK_PREFIX);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to remove task executors. " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Succeed to unregister mem fault alarm.";
    return ret;
}

uint32_t UbseMemFaultManager::PanicRebootFaultEventHandler(std::string& eventId, std::string& eventMessage)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] PanicRebootFaultEventHandler received, eventId=" << eventId
                  << ", eventMessage=" << eventMessage;
    if (eventMessage.empty()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] PanicRebootFaultEventHandler event message is empty.";
        return UBSE_ERROR_INVAL;
    }

    // SRP: 输入校验独立于场景判断，格式错误始终返回 INVAL
    size_t pos = eventMessage.find('_');
    if (pos == std::string::npos) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] PanicRebootFaultEventHandler invalid event message format,"
                       << " expected 'nodeId_faultType', eventMessage=" << eventMessage;
        return UBSE_ERROR_INVAL;
    }

    std::string faultNodeId = eventMessage.substr(0, pos);
    std::string faultTypeStr = eventMessage.substr(pos + 1);

    uint32_t faultTypeValue = 0;
    if (utils::ConvertStrToUint32(faultTypeStr, faultTypeValue) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] PanicRebootFaultEventHandler failed to parse fault type,"
                       << " faultTypeStr=" << faultTypeStr;
        return UBSE_ERROR_INVAL;
    }

    auto& strategy = GetStrategy();
    ALARM_FAULT_TYPE faultType = static_cast<ALARM_FAULT_TYPE>(faultTypeValue);
    UBSE_LOG_INFO << "[MEM_CONTROLLER] PanicRebootFaultEventHandler parsed, faultNodeId=" << faultNodeId
                  << ", faultType=" << faultTypeValue << "(" << FaultTypeName(faultTypeValue)
                  << "), mode=" << strategy.Name() << ".";

    // 本节点清理：根据策略决定是否清理本节点导入债务
    // 非 Clos 下所有节点都收到 Panic，都需清理；Clos 下只有主收到并清理
    if (strategy.ShouldHandlePanicLocal()) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] PanicRebootFaultEventHandler handling local cleanup, mode="
                      << strategy.Name() << ", faultNodeId=" << faultNodeId << ".";
        auto ret = MemReportWhenExportNodeOnFault(faultType, faultNodeId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] PanicRebootFaultEventHandler local cleanup failed, mode="
                           << strategy.Name() << ", faultNodeId=" << faultNodeId << "." << FormatRetCode(ret);
            return ret;
        }
    } else {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] PanicRebootFaultEventHandler skip local cleanup, mode="
                      << strategy.Name() << ", eventMessage=" << eventMessage << ".";
    }

    // 转发：根据策略决定是否转发给其他节点
    // 非 Clos 下所有节点都收到，无需转发；Clos 下只有主收到，需转发给其他节点
    if (strategy.ShouldForwardPanic()) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] PanicRebootFaultEventHandler forwarding, mode=" << strategy.Name()
                      << ", faultNodeId=" << faultNodeId << ", faultType=" << faultTypeValue
                      << "(" << FaultTypeName(faultTypeValue) << ").";
        PushFaultEvent(faultNodeId, faultTypeValue);
        ProcessFaultEvents();
    } else {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] PanicRebootFaultEventHandler skip forward, mode=" << strategy.Name()
                      << ", faultNodeId=" << faultNodeId << ", faultType=" << faultTypeValue
                      << "(" << FaultTypeName(faultTypeValue) << ").";
    }
    return UBSE_OK;
}

uint32_t UbseMemFaultManager::BmcFaultHandler(ALARM_FAULT_TYPE alarmFaultEvent, const std::string& faultInfo)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] BmcFaultHandler received, faultInfo=" << faultInfo
                  << ", faultType=" << static_cast<uint32_t>(alarmFaultEvent)
                  << "(" << FaultTypeName(static_cast<uint32_t>(alarmFaultEvent)) << ").";

    UbseRoleInfo curNodeInfo{};
    auto ret = UbseGetCurrentNodeInfo(curNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] BmcFaultHandler failed to get current node info. " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    auto& strategy = GetStrategy();
    if (!strategy.ShouldHandleBmc()) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] BmcFaultHandler skip, mode=" << strategy.Name()
                      << " (nodeId=" << curNodeInfo.nodeId << ", role=" << curNodeInfo.nodeRole << ").";
        return UBSE_OK;
    }

    std::string faultNodeId = faultInfo;
    UBSE_LOG_INFO << "[MEM_CONTROLLER] BmcFaultHandler handling, mode=" << strategy.Name()
                  << ", faultNodeId=" << faultNodeId << ", faultType=" << static_cast<uint32_t>(alarmFaultEvent)
                  << "(" << FaultTypeName(static_cast<uint32_t>(alarmFaultEvent)) << ").";

    PushFaultEvent(faultNodeId, static_cast<uint32_t>(alarmFaultEvent));

    ProcessFaultEvents();
    return UBSE_OK;
}

uint32_t UbseMemFaultManager::FaultDeliverTimerHandler()
{
    if (UbseMemFaultManager::executorPtr != nullptr) {
        UbseMemFaultManager::executorPtr->Execute([] {
            UBSE_LOG_DEBUG << "[MEM_CONTROLLER] FaultDeliverTimerHandler tick, mode=" << GetStrategy().Name();
            ProcessFaultEvents();
            ProcessGroupForwardEvents();
        });
    } else {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] FaultDeliverTimerHandler executorPtr is null.";
    }
    return UBSE_OK;
}

void UbseMemFaultManager::FaultAgentsNotifyHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] FaultAgentsNotifyHandler received fault notify from master.";

    UbseDeSerialization input(req.data, req.len);
    std::string faultNodeId;
    uint32_t faultTypeValue = 0;
    input >> faultNodeId >> faultTypeValue;
    if (!input.Check()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] FaultAgentsNotifyHandler failed to deserialize fault notify message.";
        return;
    }

    ALARM_FAULT_TYPE faultType = static_cast<ALARM_FAULT_TYPE>(faultTypeValue);
    auto& strategy = GetStrategy();
    UBSE_LOG_INFO << "[MEM_CONTROLLER] FaultAgentsNotifyHandler processing, mode=" << strategy.Name()
                  << ", faultNodeId=" << faultNodeId << ", faultType=" << faultTypeValue
                  << "(" << FaultTypeName(faultTypeValue) << ").";

    MemReportWhenExportNodeOnFault(faultType, faultNodeId);

    if (strategy.ShouldForwardToGroupNodes()) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] FaultAgentsNotifyHandler forwarding to local group nodes, mode="
                      << strategy.Name() << ", faultNodeId=" << faultNodeId << ", faultType=" << faultTypeValue
                      << "(" << FaultTypeName(faultTypeValue) << ").";
        ForwardFaultToGroupNodes(faultNodeId, faultTypeValue);
    }
}

UbseResult UbseMemFaultManager::MemReportWhenExportNodeOnFault(ALARM_FAULT_TYPE faultType, std::string& faultId)
{
    if (faultId.empty()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] faultNodeId is empty..";
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Starting to handle mem reports caused by export node failures, faultId="
                  << faultId << ".";
    if (executorPtr == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] executorPtr is null, cannot submit task.";
        return UBSE_ERROR_NULLPTR;
    }
    std::string faultIdCopy = faultId;
    executorPtr->Execute([faultIdCopy] { SubmitMemReportTaskWhenNodeStopsMemoryService(faultIdCopy); });
    return UBSE_OK;
}

uint32_t UbseMemFaultManager::ClosImportMemFaultHandler(ALARM_FAULT_TYPE alarmFaultEvent,
                                                              const std::string& faultNodeId)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] ClosImportMemFaultHandler triggered, "
                  << "faultType=" << static_cast<uint32_t>(alarmFaultEvent) << ", faultNodeId=" << faultNodeId;
    return DeleteShareImportDebtInfoByNodeId(faultNodeId);
}

UbseResult UbseMemFaultManager::ReportSingleImportDebt(const std::string& targetNodeId,
                                                       const def::ShareHandleInfoVec& shareHandleInfoVec,
                                                       const def::NumaHandleInfoVec& numaHandleInfoVec,
                                                       const def::FdHandleInfoVec& fdHandleInfoVec)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] ReportSingleImportDebt called, targetNodeId=" << targetNodeId
                  << ", shareCount=" << shareHandleInfoVec.size() << ", numaCount=" << numaHandleInfoVec.size()
                  << ", fdCount=" << fdHandleInfoVec.size();

    if (shareHandleInfoVec.empty() && numaHandleInfoVec.empty() && fdHandleInfoVec.empty()) {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] No single import debt to report for targetNodeId=" << targetNodeId;
        return UBSE_OK;
    }

    message::UbseMemSingleImportMessage notifyMsg;
    notifyMsg.SetShareHandleInfoVec(shareHandleInfoVec);
    notifyMsg.SetNumaHandleInfoVec(numaHandleInfoVec);
    notifyMsg.SetFdHandleInfoVec(fdHandleInfoVec);
    auto ret = notifyMsg.Serialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to serialize single import debt message for targetNodeId="
                       << targetNodeId << "." << FormatRetCode(ret);
        return ret;
    }

    const UbseByteBuffer request{
        .data = notifyMsg.SerializedData(), .len = notifyMsg.SerializedDataSize(), .freeFunc = [](uint8_t*) -> void {
        }};
    auto respHandler = [](void*, const UbseByteBuffer&, uint32_t) -> void {
    };
    ret = UbseRpcAsyncSend(
        GetMemFaultComEndpoint(static_cast<uint16_t>(UbseMemFaultOpCode::UBSE_SINGLE_IMPORT_DEBT_NOTIFY), targetNodeId),
        request, nullptr, respHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to send single import debt notify to targetNodeId=" << targetNodeId
                       << "." << FormatRetCode(ret);
    } else {
        UBSE_LOG_INFO << "[MEM_CONTROLLER] Sent single import debt notify to targetNodeId=" << targetNodeId
                      << ", shareCount=" << shareHandleInfoVec.size() << ", numaCount=" << numaHandleInfoVec.size()
                      << ", fdCount=" << fdHandleInfoVec.size();
    }
    return ret;
}

void UbseMemFaultManager::SingleImportDebtNotifyHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Received single import debt notify from master.";

    message::UbseMemSingleImportMessage simpo{req.data, static_cast<uint32_t>(req.len)};
    auto ret = simpo.Deserialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to deserialize single import debt message." << FormatRetCode(ret);
        return;
    }

    auto shareHandleInfoVec = simpo.GetShareHandleInfoVec();
    auto numaHandleInfoVec = simpo.GetNumaHandleInfoVec();
    auto fdHandleInfoVec = simpo.GetFdHandleInfoVec();

    UBSE_LOG_INFO << "[MEM_CONTROLLER] Processing single import debt, shareCount=" << shareHandleInfoVec.size()
                  << ", numaCount=" << numaHandleInfoVec.size() << ", fdCount=" << fdHandleInfoVec.size();

    if (executorPtr == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] executorPtr is null, cannot submit SingleImportDebt task.";
        return;
    }
    executorPtr->Execute([shareHandleInfoVec, numaHandleInfoVec, fdHandleInfoVec] {
        ExtractFaultMemBlockInVecAndCallSengFunc<UBSE_LONGLINK_FAULT_SHM, UBSE_LONG_LINK_REGISTER>(
            shareHandleInfoVec, "share", [](const auto& info) -> const auto& { return info.memIds; });
        ExtractFaultMemBlockInVecAndCallSengFunc<UBSE_LONGLINK_FAULT_NUMA, UBSE_LONG_LINK_REGISTER>(
            numaHandleInfoVec, "numa", [](const auto& info) -> const auto& { return info.numaIds; });
        ExtractFaultMemBlockInVecAndCallSengFunc<UBSE_LONGLINK_FAULT_FD, UBSE_LONG_LINK_REGISTER>(
            fdHandleInfoVec, "fd", [](const auto& info) -> const auto& { return info.memIds; });
    });
}

template <typename ImportObjType>
static bool DoFindInImportMap(debt::UbseMemTypeDebtMap<ImportObjType>& importMap, const std::string& nodeId,
                              uint64_t memId, std::string& memName, UbseUdsInfo& udsInfo, uint64_t& handleId,
                              const std::string& memTypeStr)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Searching in " << memTypeStr << " import map for memId=" << memId;

    auto nodeMap = importMap.FindNodeMap(nodeId);
    if (!nodeMap) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] No " << memTypeStr << " import map found for nodeId=" << nodeId;
        return false;
    }

    for (const auto& [name, objPtr] : nodeMap->GetAll()) {
        if (!objPtr) {
            continue;
        }
        auto obj = *objPtr;
        for (auto& info : obj.status.importResults) {
            if (info.memId == memId) {
                importMap.PutResource(nodeId, name, obj);
                memName = name;
                udsInfo = obj.req.udsInfo;
                handleId = (memTypeStr == "numa") ? static_cast<uint64_t>(info.numaId) : info.memId;
                UBSE_LOG_INFO << "[MEM_CONTROLLER] Found and updated memName=" << memName << " from " << memTypeStr
                              << " import by memId=" << memId << ", handleId=" << handleId;
                return true;
            }
        }
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] memId=" << memId << " not found in " << memTypeStr << " import map.";
    return false;
}

UbseResult FindNameByMemIdInImportObj(uint64_t memId, std::string& memName, std::string& memType, UbseUdsInfo& udsInfo,
                                      uint64_t& handleId)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Finding import object by memId=" << memId;

    std::string nodeId;
    auto ret = GetCurrentNodeId(nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get current node id.";
        return ret;
    }

    auto& ledger = debt::UbseMemDebtLedger::GetInstance();
    if (DoFindInImportMap(ledger.GetDebtMap<UbseMemShareBorrowImportObj>(), nodeId, memId, memName, udsInfo, handleId,
                          "share")) {
        memType = "share";
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Not found in share import, trying fd import. memId=" << memId;

    if (DoFindInImportMap(ledger.GetDebtMap<UbseMemFdBorrowImportObj>(), nodeId, memId, memName, udsInfo, handleId,
                          "fd")) {
        memType = "fd";
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Not found in fd import, trying numa import. memId=" << memId;

    if (DoFindInImportMap(ledger.GetDebtMap<UbseMemNumaBorrowImportObj>(), nodeId, memId, memName, udsInfo, handleId,
                          "numa")) {
        memType = "numa";
        return UBSE_OK;
    }

    UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to find import object by memId=" << memId << " in all import types.";
    memType = "unknown";
    return UBSE_ERROR;
}

static UbseResult ParseFaultInfo(const std::string& faultInfo, uint64_t& memId, UbMemFaultType& type)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Parsing fault info.";

    rapidjson::Document doc(rapidjson::kObjectType);
    doc.Parse(faultInfo.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to parse alarm info to json string.";
        return UBSE_ERROR;
    }

    auto ret = UbseJsonUtil::GetUint64FromJsonPtr(doc, MEM_FAULT_INFO_JSON_MEM_ID, memId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get memId from json, content is [" << faultInfo << "].";
        return ret;
    }

    uint32_t faultType = UB_MEM_HEALTHY;
    ret = UbseJsonUtil::GetUintFromJsonPtr(doc, MEM_FAULT_INFO_JSON_RAS_TYPE, faultType);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to get fault type from json, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }

    type = static_cast<UbMemFaultType>(faultType);
    UBSE_LOG_DEBUG << "[MEM_CONTROLLER] Parsed fault info success. memId=" << memId << ", faultType=" << faultType;
    return UBSE_OK;
}

uint32_t UbseMemFaultManager::MemFaultHandler(ubse::ras::ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Started to handle ras fault report. faultInfo=" << faultInfo << ".";

    uint64_t memId = 0;
    UbMemFaultType type;
    auto ret = ParseFaultInfo(faultInfo, memId, type);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to parse fault info.";
        return ret;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Parsed fault info: memId=" << memId
                  << ", faultType=" << static_cast<uint16_t>(type);

    std::string memName;
    std::string memType;
    UbseUdsInfo udsInfo;
    uint64_t handleId = 0;
    ret = FindNameByMemIdInImportObj(memId, memName, memType, udsInfo, handleId);
    if (ret != UBSE_OK || memName.empty()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to find and update import object by memId=" << memId << ".";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Found import object: memName=" << memName << ", memType=" << memType
                  << ", handleId=" << handleId;

    ret = SendMemFaultMessageByType(memType, handleId, memName, udsInfo, type);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to send fault message. memId=" << memId << ", memName=" << memName;
        return ret;
    }

    UBSE_LOG_INFO << "[MEM_CONTROLLER] Successfully handled memory fault. memId=" << memId << ", memName=" << memName
                  << ", memType=" << memType << ", handleId=" << handleId
                  << ", faultType=" << static_cast<uint16_t>(type);
    return UBSE_OK;
}

UbseResult UbseMemFaultManager::SendMemFaultMessageByType(const std::string& memType, uint64_t handleId,
                                                          const std::string& memName, const UbseUdsInfo& udsInfo,
                                                          ubse::adapter_plugins::mmi::UbMemFaultType type)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Sending fault message. memType=" << memType << ", handleId=" << handleId
                  << ", memName=" << memName;

    if (UbseMemFaultManager::executorPtr == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] executorPtr is null, cannot submit task.";
        return UBSE_ERROR_NULLPTR;
    }

    UbseMemFault fault{
        .memName = memName,
        .handleId = handleId,
        .type = static_cast<UbseIpcMemFaultType>(type),
    };

    UbseMemFaultManager::executorPtr->Execute([memType, fault, udsInfo] {
        UbseResult ret;
        if (memType == "share") {
            ret = SendSingleFaultMemBlockMessage<UBSE_LONGLINK_FAULT_SHM, UBSE_LONG_LINK_REGISTER>("share", fault,
                                                                                                   udsInfo);
        } else if (memType == "fd") {
            ret = SendSingleFaultMemBlockMessage<UBSE_LONGLINK_FAULT_FD, UBSE_LONG_LINK_REGISTER>("fd", fault, udsInfo);
        } else if (memType == "numa") {
            ret = SendSingleFaultMemBlockMessage<UBSE_LONGLINK_FAULT_NUMA, UBSE_LONG_LINK_REGISTER>("numa", fault,
                                                                                                    udsInfo);
        } else {
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] Unknown mem type=" << memType << " for memId=" << fault.handleId;
            return;
        }

        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to send fault message, memName=" << fault.memName
                           << ", handleId=" << fault.handleId << "." << FormatRetCode(ret);
            return;
        }
        UBSE_LOG_INFO << "[MEM_CONTROLLER] Successfully sent fault message. memType=" << memType
                      << ", memId=" << fault.handleId;
    });
    return UBSE_OK;
}

static UbseResult ParsePortDownUpEventMsg(const std::string& eventMsg, PortEventInfo& info)

{
    size_t semiPos = eventMsg.find(';');
    if (semiPos == std::string::npos) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Invalid eventMsg format, expected 'STATUS;slotId:chipId:portId'.";
        return UBSE_ERROR_INVAL;
    }

    info.status = eventMsg.substr(0, semiPos);
    if (info.status != "DOWN" && info.status != "UP") {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Invalid port status=" << info.status << ", expected 'DOWN' or 'UP'.";
        return UBSE_ERROR_INVAL;
    }

    std::string locationInfo = eventMsg.substr(semiPos + 1);
    size_t pos1 = locationInfo.find(':');
    if (pos1 == std::string::npos) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Invalid location info format, missing slotId:chipId:portId.";
        return UBSE_ERROR_INVAL;
    }
    size_t pos2 = locationInfo.find(':', pos1 + 1);
    if (pos2 == std::string::npos) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Invalid location info format, missing chipId:portId.";
        return UBSE_ERROR_INVAL;
    }
    size_t pos3 = locationInfo.find(':', pos2 + 1);
    if (pos3 == std::string::npos) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Invalid location info format, missing portId.";
        return UBSE_ERROR_INVAL;
    }

    info.slotId = locationInfo.substr(0, pos1);
    info.chipId = locationInfo.substr(pos1 + 1, pos2 - pos1 - 1);
    info.portId = locationInfo.substr(pos2 + 1, pos3 - pos2 - 1);

    if (info.slotId.empty() || info.chipId.empty() || info.portId.empty()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Parsed empty field from eventMsg, slotId=" << info.slotId
                       << ", chipId=" << info.chipId << ", portId=" << info.portId << ".";
        return UBSE_ERROR_INVAL;
    }

    return UBSE_OK;
}

UbseResult UbseMemFaultManager::PortDownUpEventHandle(std::string&, std::string& eventMsg)
{
    if (eventMsg.empty()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] PortDownUpEventHandle eventMsg is empty.";
        return UBSE_ERROR_INVAL;
    }
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Starting to handle topology port down or up event, eventMsg=" << eventMsg << ".";

    PortEventInfo info;
    auto ret = ParsePortDownUpEventMsg(eventMsg, info);
    if (ret != UBSE_OK) {
        return ret;
    }

    UBSE_LOG_INFO << "[MEM_CONTROLLER] Parsed port event: status=" << info.status << ", slotId=" << info.slotId
                  << ", chipId=" << info.chipId << ", portId=" << info.portId << ".";

    bool isDown = (info.status == "DOWN");
    if (isDown) {
        if (!TryAddPortDown(info)) {
            UBSE_LOG_WARN << "[MEM_CONTROLLER] Port already down, slotId=" << info.slotId << ", chipId=" << info.chipId
                          << ", portId=" << info.portId << ".";
            return UBSE_OK;
        }
        OnePortDownHandle(info);
        return UBSE_OK;
    }

    if (!TryErasePortDown(info)) {
        UBSE_LOG_WARN << "[MEM_CONTROLLER] Port up but not in down record, slotId=" << info.slotId
                      << ", chipId=" << info.chipId << ", portId=" << info.portId << ".";
        return UBSE_OK;
    }

    OnePortUpHandle(info);
    return UBSE_OK;
}
template <uint32_t OpCode, ubse_ipc_module_code_t ModuleCode, UbMemFaultType FaultType, typename HandleInfoVec,
          typename QueryFunc, typename IdGetter>
static UbseResult ReportPortFaultHandles(const PortEventInfo& info, const std::string& handleType, QueryFunc queryFunc,
                                         IdGetter getIdList)
{
    HandleInfoVec handleInfo;
    auto ret = queryFunc(info.slotId, info.chipId, info.portId, handleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to query " << handleType
                       << " port fault handle info, nodeId=" << info.slotId << ", chipId=" << info.chipId
                       << ", portId=" << info.portId << "." << FormatRetCode(ret);
        return ret;
    }
    return ExtractFaultMemBlockInVecAndCallSengFunc<OpCode, ModuleCode, FaultType>(handleInfo, handleType, getIdList);
}

template <UbMemFaultType FaultType>
static void ReportAllPortFaultHandles(const PortEventInfo& info, const std::string& action)
{
    auto ret =
        ReportPortFaultHandles<UBSE_LONGLINK_FAULT_SHM, UBSE_LONG_LINK_REGISTER, FaultType, debt::ShareHandleInfoVec>(
            info, "share", debt::UbseQuerySharePortFaultHandleInfo,
            [](const auto& h) -> const auto& { return h.memIds; });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] " << action << " share fault report failed, slotId=" << info.slotId
                       << ", chipId=" << info.chipId << ", portId=" << info.portId << "." << FormatRetCode(ret);
    }
    ret = ReportPortFaultHandles<UBSE_LONGLINK_FAULT_NUMA, UBSE_LONG_LINK_REGISTER, FaultType, debt::NumaHandleInfoVec>(
        info, "numa", debt::UbseQueryNumaPortFaultHandleInfo, [](const auto& h) -> const auto& { return h.numaIds; });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] " << action << " numa fault report failed, slotId=" << info.slotId
                       << ", chipId=" << info.chipId << ", portId=" << info.portId << "." << FormatRetCode(ret);
    }
    ret = ReportPortFaultHandles<UBSE_LONGLINK_FAULT_FD, UBSE_LONG_LINK_REGISTER, FaultType, debt::FdHandleInfoVec>(
        info, "fd", debt::UbseQueryFdPortFaultHandleInfo, [](const auto& h) -> const auto& { return h.memIds; });
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] " << action << " fd fault report failed, slotId=" << info.slotId
                       << ", chipId=" << info.chipId << ", portId=" << info.portId << "." << FormatRetCode(ret);
    }
}

UbseResult UbseMemFaultManager::OnePortDownHandle(const PortEventInfo& info)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Handling new port down event, slotId=" << info.slotId
                  << ", chipId=" << info.chipId << ", portId=" << info.portId << ".";
    if (executorPtr == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] executorPtr is null, cannot submit OnPortDown task.";
        return UBSE_ERROR_NULLPTR;
    }
    executorPtr->Execute([info] { ReportAllPortFaultHandles<MEM_LINK_DOWN>(info, "OnPortDown"); });
    return UBSE_OK;
}

UbseResult UbseMemFaultManager::OnePortUpHandle(const PortEventInfo& info)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Handling port up event (was down), slotId=" << info.slotId

                  << ", chipId=" << info.chipId << ", portId=" << info.portId << ".";
    if (executorPtr == nullptr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] executorPtr is null, cannot submit OnPortUp task.";
        return UBSE_ERROR_NULLPTR;
    }
    executorPtr->Execute([info] { ReportAllPortFaultHandles<MEM_LINK_UP>(info, "OnPortUp"); });
    return UBSE_OK;
}
} // namespace ubse::mem::controller