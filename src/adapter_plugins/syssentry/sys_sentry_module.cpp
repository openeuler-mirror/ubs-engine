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

#include "sys_sentry_module.h"
#include <algorithm>
#include <cctype>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_map>
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_event_module.h"
#include "ubse_logger.h"
#include "ubse_module.h"
#include "ubse_node_mgr.h"
#include "ubse_node_mgr_module.h"
#include "ubse_os_util.h"
#include "ubse_ras_module.h"
#include "ubse_str_util.h"
#include "ubse_thread_pool_module.h"
#include "ubse_timer.h"
#include "adapter_plugins/mti/ubse_mti_eid_interface.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "adapter_plugins/mti/ubse_smbios.h"
#include "sentry_observer.h"
#include "src/adapter_plugins/mti/ubse_lcne_module.h"

namespace syssentry {
using namespace ubse::log;
using namespace ubse::adapter_plugins::mti;
using namespace ubse::adapter_plugins::smbios;
using namespace ubse::context;
using namespace ubse::common::def;
using namespace ubse::module;
using namespace ubse::task_executor;
OPTIONAL_MODULE_IMPL(SysSentryModule, ubse::ras::UbseRasModule, ubse::nodeMgr::UbseNodeMgrModule);
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult SysSentryModule::Initialize()
{
    auto taskExecutor = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    // 缺少任务执行器时无法承载配置和动态刷新任务，初始化直接失败。
    if (taskExecutor == nullptr) {
        UBSE_LOG_ERROR << "Initialize sysSentry module failed because TaskExecutorModule is unavailable";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return taskExecutor->Create(UBSE_RAS_TASK_NAME, NO_1, NO_128);
}

void SysSentryModule::UnInitialize()
{
    auto taskExecutor = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    // 模块卸载顺序异常时无法删除执行器，记录后按幂等方式退出。
    if (taskExecutor == nullptr) {
        UBSE_LOG_WARN << "TaskExecutorModule is null";
        return;
    }
    taskExecutor->Remove(UBSE_RAS_TASK_NAME);
}

UbseResult SysSentryModule::Start()
{
    auto& observer = UbseRasObserver::GetInstance();
    auto ret = SubscribeBroadcastDomainEvents();
    if (ret != UBSE_OK) {
        // 此时 observer 尚未启动，直接返回并保留后续重试能力。
        UBSE_LOG_ERROR << "Start sysSentry module failed while subscribing broadcast events, " << FormatRetCode(ret);
        return ret;
    }
    // 注册定时器，每隔一段时间调用sentryctl命令查询sentry_msg_monitor 运行状态
    observer.RegQueryMsgMonitorTimer();
    observer.UbseConfigSysSentryWithRetry(); // 不校验返回值，sysSentry未就绪时不影响ubse其它功能
    ret = observer.Start();
    if (ret != UBSE_OK) {
        // observer 启动失败时撤销节点发现订阅，避免遗留无消费者的回调。
        UBSE_LOG_ERROR << "Start observer failed, " << FormatRetCode(ret);
        UnsubscribeBroadcastDomainEvents();
        return ret;
    }
    return UBSE_OK;
}

void SysSentryModule::Stop()
{
    UnsubscribeBroadcastDomainEvents();
    UbseRasObserver::GetInstance().Stop();
}

void LinkStrings(std::string& result, const std::string linkSymbol, const std::vector<std::string> strings)
{
    for (const auto& item : strings) {
        if (!result.empty()) {
            result += linkSymbol;
        }
        result += item;
    }
}

std::vector<std::string> SplitString(const std::string& str, char delimiter)
{
    std::vector<std::string> result;
    std::istringstream iss(str);
    std::string token;

    while (std::getline(iss, token, delimiter)) {
        result.push_back(token);
    }

    return result;
}

UbseResult ProcessEids(const std::map<UbseMtiIouInfo, UbseMtiEidGroup>& allSocketComEid, const std::string& nodeId,
                       std::unordered_map<std::string, std::vector<std::string>>& eids,
                       std::vector<std::string>& eidGroup)
{
    for (const auto& info : allSocketComEid) {
        eids[info.first.slotId].emplace_back(info.second.primaryEid);
    }
    for (auto& e : eids[nodeId]) {
        eidGroup.emplace_back(e);
    }
    for (const auto& eidPair : eids) {
        for (size_t i = 0; i < eidPair.second.size(); i++) {
            if (eidPair.second[i].empty()) {
                continue;
            }
            if (std::find(eids[nodeId].begin(), eids[nodeId].end(), eidPair.second[i]) != eids[nodeId].end()) {
                continue;
            }
            if (i >= eidGroup.size()) {
                // 当前节点具有的eid数量与其它节点不同，记录警告日志并添加默认值
                UBSE_LOG_WARN << "Node eid count mismatch, adding default eid";
                eidGroup.emplace_back("");
            }
            if (!eidGroup[i].empty()) {
                eidGroup[i] += ",";
            }
            eidGroup[i] += eidPair.second[i];
        }
    }
    return UBSE_OK;
}

UbseResult GetEids(std::string& clientEid, std::string& serverEids)
{
    // CLOS组网下EID信息应从节点发现获得
    std::map<UbseMtiIouInfo, UbseMtiEidGroup> comUrmaInfoMap{};
    auto result = UbseMtiInterface::GetInstance().GetMtiComEid(comUrmaInfoMap);
    if (result != UBSE_OK) {
        UBSE_LOG_WARN << "Get all socket eid failed, " << ubse::log::FormatRetCode(result);
        return result;
    }
    UbseMtiNodeInfo localNodeInfo;
    result = UbseMtiInterface::GetInstance().GetLocalNodeInfo(localNodeInfo);
    if (result != UBSE_OK || localNodeInfo.nodeId.empty()) {
        UBSE_LOG_WARN << "Get local node info failed, " << ubse::log::FormatRetCode(result);
        return result;
    }

    std::unordered_map<std::string, std::vector<std::string>> eids{};
    std::vector<std::string> eidGroup;
    if (auto ret = ProcessEids(comUrmaInfoMap, localNodeInfo.nodeId, eids, eidGroup); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to process eids, local nodeId=" << localNodeInfo.nodeId;
        return ret;
    }
    LinkStrings(serverEids, ";", eidGroup);

    if (eids.find(localNodeInfo.nodeId) == eids.end()) {
        UBSE_LOG_ERROR << "Cannot find local node eid which nodeId=" << localNodeInfo.nodeId;
        return UBSE_ERROR_CONF_INVALID;
    }

    LinkStrings(clientEid, ";", eids[localNodeInfo.nodeId]);
    return UBSE_OK;
}

// 对动态参数转义, 用引号把数据“包裹”起来，shell 不会解析内部的 ;、`
std::string ShellEscape(const std::string& str)
{
    if (str.empty()) {
        return "''";
    }
    std::string result;
    result += '\'';
    for (char c : str) {
        if (c == '\'') {
            result += "'\\''";
        } else {
            result += c;
        }
    }
    result += '\'';
    return result;
}

std::optional<std::vector<uint32_t>> g_lastSuccessfulPeers;

using CommandDescList = std::vector<std::pair<std::string, std::string>>;

bool HasInvalidCommandValue(const SysSentryBroadcastDomain& domain);
CommandDescList BuildBroadcastCommands(const SysSentryBroadcastDomain& domain);
UbseResult ExecuteSentryCommands(const CommandDescList& commands);

UbseResult SetSysSentryFaultReporter()
{
    SysSentryBroadcastDomain domain;
    auto ret = BuildSysSentryBroadcastDomain(domain);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to build sysSentry broadcast domain, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    if (HasInvalidCommandValue(domain)) {
        UBSE_LOG_ERROR << "sysSentry configuration contains invalid double quote";
        return UBSE_ERROR_INVAL;
    }

    CommandDescList tasks = {
        {"sentryctl set sentry_remote_reporter --eid=" + ShellEscape(domain.clientEid) + " 2>&1", "setClientEid"},
    };
    // 1D 未取得有效本机 CNA 时只保留原有 EID 配置，不下发空 CNA。
    if (!domain.clientCna.empty()) {
        tasks.emplace_back("sentryctl set sentry_remote_reporter --cna=" + ShellEscape(domain.clientCna) + " 2>&1",
                           "setClientCna");
    }
    auto broadcastTasks = BuildBroadcastCommands(domain);
    tasks.insert(tasks.end(), broadcastTasks.begin(), broadcastTasks.end());
    ret = ExecuteSentryCommands(tasks);
    if (ret != UBSE_OK) {
        // 具体失败命令由执行函数记录，此处补充完整配置阶段上下文。
        UBSE_LOG_WARN << "Failed to apply complete sysSentry configuration, " << FormatRetCode(ret);
        return ret;
    }
    if (domain.clientCna.empty()) {
        UBSE_LOG_WARN << "No local CNA available, skip CNA configuration";
    } else if (domain.serverCnas.empty()) {
        UBSE_LOG_WARN << "No peer CNA available, skip server_cna configuration";
    }
    g_lastSuccessfulPeers = domain.peerNodeIds;
    return UBSE_OK;
}

bool ParseDecimalUint32(const std::string& value, uint32_t& parsed)
{
    // 通用转换函数不打印日志，由调用点补充 nodeId、dieId 等字段语义。
    if (value.empty() ||
        !std::all_of(value.begin(), value.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
        return false;
    }
    return ubse::utils::ConvertStrToUint32(value, parsed) == UBSE_OK;
}

UbseResult NormalizeNodeIds(const std::vector<ubse::nodeMgr::UbseNodeStaticInfo>& nodes, const std::string& localNodeId,
                            std::vector<uint32_t>& nodeIds, size_t& localNodeIndex)
{
    nodeIds.clear();
    localNodeIndex = 0;
    uint32_t localNodeIdValue = 0;
    if (!ParseDecimalUint32(localNodeId, localNodeIdValue)) {
        UBSE_LOG_ERROR << "Invalid current nodeId, localNodeId=" << localNodeId;
        return UBSE_ERROR_CONF_INVALID;
    }
    if (nodes.empty()) {
        UBSE_LOG_ERROR << "Specified-root node list is empty";
        return UBSE_ERROR_CONF_INVALID;
    }

    std::set<uint32_t> uniqueNodeIds;
    for (const auto& node : nodes) {
        uint32_t nodeId = 0;
        if (!ParseDecimalUint32(node.nodeId, nodeId)) {
            UBSE_LOG_WARN << "Invalid specified-root candidate nodeId=" << node.nodeId;
            continue;
        }
        if (!uniqueNodeIds.emplace(nodeId).second) {
            UBSE_LOG_WARN << "Duplicate numeric specified-root candidate nodeId=" << node.nodeId;
            continue;
        }
    }
    auto localIt = uniqueNodeIds.find(localNodeIdValue);
    if (localIt == uniqueNodeIds.end()) {
        UBSE_LOG_ERROR << "Current node is absent from specified-root nodes, localNodeId=" << localNodeId
                       << ", nodes.size=" << nodes.size();
        return UBSE_ERROR_CONF_INVALID;
    }

    nodeIds.assign(uniqueNodeIds.begin(), uniqueNodeIds.end());
    localNodeIndex = static_cast<size_t>(std::distance(uniqueNodeIds.begin(), localIt));
    return UBSE_OK;
}

UbseResult SelectRootPeerNodeIds(const std::vector<ubse::nodeMgr::UbseNodeStaticInfo>& nodes,
                                 const std::string& localNodeId, std::vector<uint32_t>& peers)
{
    constexpr size_t PEERS_PER_SIDE = 4;
    constexpr size_t MAX_PEERS = PEERS_PER_SIDE * 2;
    std::vector<uint32_t> nodeIds;
    size_t localNodeIndex = 0;
    auto ret = NormalizeNodeIds(nodes, localNodeId, nodeIds, localNodeIndex);
    if (ret != UBSE_OK) {
        // 规范化函数已记录具体候选节点，调用层只补充选点阶段上下文。
        UBSE_LOG_DEBUG << "Stop selecting specified-root peers because node normalization failed, "
                       << FormatRetCode(ret);
        peers.clear();
        return ret;
    }

    const size_t leftAvailable = localNodeIndex;
    const size_t rightAvailable = nodeIds.size() - localNodeIndex - 1;
    size_t leftCount = std::min(PEERS_PER_SIDE, leftAvailable);
    size_t rightCount = std::min(PEERS_PER_SIDE, rightAvailable);
    const size_t selectedCount = leftCount + rightCount;
    if (selectedCount < MAX_PEERS) {
        size_t remaining = MAX_PEERS - selectedCount;
        const size_t extraLeft = std::min(remaining, leftAvailable - leftCount);
        leftCount += extraLeft;
        remaining -= extraLeft;
        rightCount += std::min(remaining, rightAvailable - rightCount);
    }

    peers.clear();
    // 按 nodeId 顺序追加左右两侧的相邻节点。
    for (size_t index = localNodeIndex - leftCount; index < localNodeIndex; ++index) {
        peers.push_back(nodeIds[index]);
    }
    for (size_t index = localNodeIndex + 1; index < localNodeIndex + 1 + rightCount; ++index) {
        peers.push_back(nodeIds[index]);
    }
    return UBSE_OK;
}

UbseResult GetLocalDieEids(const std::string& localNodeId, const std::map<UbseMtiIouInfo, UbseMtiEidGroup>& allEids,
                           std::vector<std::pair<uint32_t, std::string>>& localDieEids)
{
    uint32_t localNodeIdValue = 0;
    if (!ParseDecimalUint32(localNodeId, localNodeIdValue)) {
        UBSE_LOG_ERROR << "Invalid local nodeId for local DIE EIDs, localNodeId=" << localNodeId;
        return UBSE_ERROR_CONF_INVALID;
    }

    std::map<uint32_t, std::string> sortedDieEids;
    for (const auto& [iouInfo, eidGroup] : allEids) {
        uint32_t nodeId = 0;
        if (!ParseDecimalUint32(iouInfo.slotId, nodeId)) {
            UBSE_LOG_WARN << "Invalid EID slotId, slotId=" << iouInfo.slotId << ", ubpuId=" << iouInfo.ubpuId
                          << ", primaryEid=" << eidGroup.primaryEid;
            continue;
        }
        // 这里只收集本节点 EID，其他节点条目由目标 nodeId 推算，不直接使用。
        if (nodeId != localNodeIdValue) {
            continue;
        }
        uint32_t dieId = 0;
        if (!ParseDecimalUint32(iouInfo.ubpuId, dieId)) {
            UBSE_LOG_ERROR << "Invalid local DIE ubpuId, slotId=" << iouInfo.slotId << ", ubpuId=" << iouInfo.ubpuId
                           << ", primaryEid=" << eidGroup.primaryEid;
            return UBSE_ERROR_CONF_INVALID;
        }
        if (eidGroup.primaryEid.empty()) {
            UBSE_LOG_WARN << "Empty local DIE primaryEid, slotId=" << iouInfo.slotId << ", ubpuId=" << iouInfo.ubpuId
                          << ", dieId=" << dieId << ", primaryEid=" << eidGroup.primaryEid;
            continue;
        }
        if (!sortedDieEids.emplace(dieId, eidGroup.primaryEid).second) {
            UBSE_LOG_ERROR << "Duplicate numeric local DIE dieId, slotId=" << iouInfo.slotId
                           << ", ubpuId=" << iouInfo.ubpuId << ", dieId=" << dieId
                           << ", primaryEid=" << eidGroup.primaryEid;
            return UBSE_ERROR_CONF_INVALID;
        }
    }
    if (sortedDieEids.empty()) {
        UBSE_LOG_ERROR << "No local DIE EIDs found, localNodeId=" << localNodeId << ", allEids.size=" << allEids.size();
        return UBSE_ERROR_CONF_INVALID;
    }

    localDieEids.assign(sortedDieEids.begin(), sortedDieEids.end());
    return UBSE_OK;
}

namespace {
using LocalDieEids = std::vector<std::pair<uint32_t, std::string>>;
using ServerEidGroups = std::vector<std::vector<std::string>>;
using DieCnas = std::map<uint32_t, std::vector<uint32_t>>;
using NodeCnas = std::map<uint32_t, DieCnas>;
constexpr uint32_t MAX_CNA = 0xFFFFFFU;

UbseResult InitializeClosEndpoints(const LocalDieEids& localDieEids, SysSentryBroadcastDomain& domain,
                                   ServerEidGroups& serverEidGroups)
{
    std::vector<std::string> clientEids;
    clientEids.reserve(localDieEids.size());
    serverEidGroups.reserve(localDieEids.size());
    for (const auto& [dieId, localEid] : localDieEids) {
        clientEids.push_back(localEid);
        serverEidGroups.push_back({localEid});
    }
    const auto& [referenceDieId, referenceEid] = localDieEids.front();
    uint32_t localCna = 0;
    if (ubse::utils::ParseCnaValueFromEid(referenceEid, localCna) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse local reference CNA, dieId=" << referenceDieId
                       << ", localEid=" << referenceEid;
        return UBSE_ERROR_CONF_INVALID;
    }
    LinkStrings(domain.clientEid, ";", clientEids);
    domain.clientCna = std::to_string(localCna);
    return UBSE_OK;
}

UbseResult AppendClosPeerEndpoints(const std::vector<uint32_t>& peers, const LocalDieEids& localDieEids,
                                   ServerEidGroups& serverEidGroups, std::vector<std::string>& peerCnas)
{
    for (auto peerNodeId : peers) {
        if (peerNodeId == 0) {
            UBSE_LOG_ERROR << "Failed to overwrite peer EID because peerNodeId is zero";
            return UBSE_ERROR_CONF_INVALID;
        }
        // NodeMgr 的 nodeId 从 1 开始，OverwriteEid 使用从 0 开始的 serverIdx。
        const uint32_t serverIdx = peerNodeId - 1;
        for (size_t dieIndex = 0; dieIndex < localDieEids.size(); ++dieIndex) {
            const auto& [dieId, localEid] = localDieEids[dieIndex];
            std::string peerEid;
            if (ubse::utils::OverwriteEid(serverIdx, localEid, peerEid) != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to overwrite peer EID, peerNodeId=" << peerNodeId << ", sourceDieId=" << dieId
                               << ", localEid=" << localEid;
                return UBSE_ERROR;
            }
            serverEidGroups[dieIndex].push_back(peerEid);
            // CNA 只从参考 DIE 推算，其余 DIE 仅追加 server_eid。
            if (dieIndex != 0) {
                continue;
            }
            uint32_t peerCna = 0;
            if (ubse::utils::ParseCnaValueFromEid(peerEid, peerCna) != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to parse peer CNA, peerNodeId=" << peerNodeId << ", dieId=" << dieId
                               << ", peerEid=" << peerEid;
                return UBSE_ERROR;
            }
            peerCnas.push_back(std::to_string(peerCna));
        }
    }
    return UBSE_OK;
}

void JoinClosServerEndpoints(const ServerEidGroups& serverEidGroups, const std::vector<std::string>& peerCnas,
                             SysSentryBroadcastDomain& domain)
{
    std::vector<std::string> serverEids;
    serverEids.reserve(serverEidGroups.size());
    for (const auto& eidGroup : serverEidGroups) {
        std::string group;
        LinkStrings(group, ",", eidGroup);
        serverEids.push_back(std::move(group));
    }
    LinkStrings(domain.serverEids, ";", serverEids);
    LinkStrings(domain.serverCnas, ";", peerCnas);
}

void Collect1dNodeCnas(const UbseMtiCpuTopoInfoMap& topology, NodeCnas& nodeCnas)
{
    for (const auto& [devName, cpuTopo] : topology) {
        std::string nodeIdText;
        std::string dieIdText;
        uint32_t nodeId = 0;
        uint32_t dieId = 0;
        if (devName.GetNodeIdAndChipId(nodeIdText, dieIdText) != UBSE_OK || !ParseDecimalUint32(nodeIdText, nodeId) ||
            !ParseDecimalUint32(dieIdText, dieId) || cpuTopo.nodeId != nodeId) {
            UBSE_LOG_WARN << "Skip invalid CPU topology entry, devName=" << devName.devName;
            continue;
        }
        nodeCnas[nodeId][dieId].push_back(cpuTopo.busNodeCna);
    }
}

UbseResult SetLocalReferenceCna(uint32_t localNodeId, const NodeCnas& nodeCnas, uint32_t& referenceDie,
                                std::string& clientCna)
{
    const auto localIt = nodeCnas.find(localNodeId);
    if (localIt == nodeCnas.end() || localIt->second.empty()) {
        UBSE_LOG_ERROR << "Missing local CNA data, localNodeId=" << localNodeId
                       << ", nodeCnas.size=" << nodeCnas.size();
        return UBSE_ERROR_CONF_INVALID;
    }
    // 以本节点最小 DIE 作为 CNA 参考 DIE。
    referenceDie = localIt->second.begin()->first;
    const auto& localCnas = localIt->second.begin()->second;
    if (localCnas.size() == 1 && localCnas.front() <= MAX_CNA) {
        clientCna = std::to_string(localCnas.front());
        return UBSE_OK;
    }
    // 本机参考 DIE 没有唯一有效 CNA 时，不构造任何 1D CNA 配置。
    UBSE_LOG_WARN << "Invalid local reference DIE CNA, localNodeId=" << localNodeId << ", dieId=" << referenceDie
                  << ", cnaCount=" << localCnas.size();
    return UBSE_ERROR_CONF_INVALID;
}

void Collect1dPeerCnas(uint32_t localNodeId, uint32_t referenceDie, const NodeCnas& nodeCnas,
                       std::vector<std::string>& peerCnas)
{
    for (const auto& [nodeId, dieCnas] : nodeCnas) {
        // 本机 CNA 只用于 client_cna，不加入对端 server_cna 列表。
        if (nodeId == localNodeId) {
            continue;
        }
        const auto referenceIt = dieCnas.find(referenceDie);
        if (referenceIt == dieCnas.end() || referenceIt->second.size() != 1 || referenceIt->second.front() > MAX_CNA) {
            UBSE_LOG_WARN << "Skip invalid peer reference DIE CNA, nodeId=" << nodeId;
            continue;
        }
        peerCnas.push_back(std::to_string(referenceIt->second.front()));
    }
}
} // namespace

UbseResult BuildClosBroadcastDomain(const std::string& localNodeId, const std::vector<uint32_t>& peerNodeIds,
                                    const std::map<UbseMtiIouInfo, UbseMtiEidGroup>& allEids,
                                    SysSentryBroadcastDomain& domain)
{
    domain = {};
    auto sortedPeers = peerNodeIds;
    std::sort(sortedPeers.begin(), sortedPeers.end());

    std::vector<std::pair<uint32_t, std::string>> localDieEids;
    auto ret = GetLocalDieEids(localNodeId, allEids, localDieEids);
    if (ret != UBSE_OK) {
        // 叶子函数已记录 nodeId、DIE 和 EID，此处补充构建阶段上下文。
        UBSE_LOG_DEBUG << "Stop building CLOS broadcast domain because local EID collection failed, "
                       << FormatRetCode(ret);
        return ret;
    }

    ServerEidGroups serverEidGroups;
    ret = InitializeClosEndpoints(localDieEids, domain, serverEidGroups);
    if (ret != UBSE_OK) {
        // 初始化函数已记录参考 DIE 和完整 EID，此处只透传错误码。
        UBSE_LOG_DEBUG << "Stop building CLOS broadcast domain because local endpoint initialization failed, "
                       << FormatRetCode(ret);
        return ret;
    }
    std::vector<std::string> peerCnas;
    ret = AppendClosPeerEndpoints(sortedPeers, localDieEids, serverEidGroups, peerCnas);
    if (ret != UBSE_OK) {
        // 推算函数已记录目标节点及 EID，此处补充广播域构建上下文。
        UBSE_LOG_DEBUG << "Stop building CLOS broadcast domain because peer endpoint generation failed, "
                       << FormatRetCode(ret);
        return ret;
    }
    JoinClosServerEndpoints(serverEidGroups, peerCnas, domain);
    domain.peerNodeIds = std::move(sortedPeers);
    return UBSE_OK;
}

UbseResult Build1dCnaConfig(const std::string& localNodeId, const UbseMtiCpuTopoInfoMap& topology,
                            std::string& clientCna, std::string& serverCnas)
{
    clientCna.clear();
    serverCnas.clear();
    uint32_t localNodeIdValue = 0;
    if (!ParseDecimalUint32(localNodeId, localNodeIdValue)) {
        UBSE_LOG_ERROR << "Invalid local nodeId for 1D CNA configuration, localNodeId=" << localNodeId;
        return UBSE_ERROR_CONF_INVALID;
    }
    NodeCnas nodeCnas;
    Collect1dNodeCnas(topology, nodeCnas);
    uint32_t referenceDie = 0;
    auto ret = SetLocalReferenceCna(localNodeIdValue, nodeCnas, referenceDie, clientCna);
    if (ret != UBSE_OK) {
        // 叶子函数已记录本机 nodeId 和 CNA 数据规模，此处只补充 1D 构建上下文。
        UBSE_LOG_DEBUG << "Stop building 1D CNA configuration because local reference CNA is unavailable, "
                       << FormatRetCode(ret);
        return ret;
    }
    std::vector<std::string> peerCnas;
    Collect1dPeerCnas(localNodeIdValue, referenceDie, nodeCnas, peerCnas);
    LinkStrings(serverCnas, ";", peerCnas);
    return UBSE_OK;
}

UbseResult SysSentryModule::SubscribeBroadcastDomainEvents()
{
    // 重复启动时保持订阅幂等，不重复注册同一事件回调。
    if (nodeDiscoverySubscribed_) {
        UBSE_LOG_DEBUG << "Skip subscribing node discovery because the handler is already subscribed";
        return UBSE_OK;
    }
    // 1D 广播域静态构建，不需要节点发现驱动刷新。
    if (!UbseSmbios::GetInstance().IsClosType()) {
        UBSE_LOG_DEBUG << "Skip subscribing node discovery for 1D topology";
        return UBSE_OK;
    }
    // CLOS 分层选举广播域只取当前组，不响应指定根节点的动态发现事件。
    if (ubse::nodeMgr::GetRootIpList().empty()) {
        UBSE_LOG_DEBUG << "Skip subscribing node discovery for hierarchical CLOS topology";
        return UBSE_OK;
    }
    auto eventModule = UbseContext::GetInstance().GetModule<ubse::event::UbseEventModule>();
    if (eventModule == nullptr) {
        UBSE_LOG_ERROR << "Get event module failed while subscribing node discovery";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto ret = eventModule->UbseSubEvent(UBSE_EVENT_NODE_DISCOVERY, HandleSysSentryNodeDiscoveryEvent);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Subscribe node discovery failed, " << FormatRetCode(ret);
        return ret;
    }
    nodeDiscoverySubscribed_ = true;
    return UBSE_OK;
}

void SysSentryModule::UnsubscribeBroadcastDomainEvents()
{
    // 未订阅时按幂等成功处理，避免重复调用事件模块。
    if (!nodeDiscoverySubscribed_) {
        UBSE_LOG_DEBUG << "Skip unsubscribing node discovery because the handler is not subscribed";
        return;
    }
    auto eventModule = UbseContext::GetInstance().GetModule<ubse::event::UbseEventModule>();
    // 事件模块已先卸载时无法调用反订阅接口，但仍需清理本地订阅标志。
    if (eventModule == nullptr) {
        UBSE_LOG_WARN << "Get event module failed while unsubscribing node discovery";
        nodeDiscoverySubscribed_ = false;
        return;
    }
    auto ret = eventModule->UbseUnSubEvent(UBSE_EVENT_NODE_DISCOVERY, HandleSysSentryNodeDiscoveryEvent);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unsubscribe node discovery failed, " << FormatRetCode(ret);
    }
    nodeDiscoverySubscribed_ = false;
}

bool HasInvalidCommandValue(const SysSentryBroadcastDomain& domain)
{
    const std::vector<std::string> values = {domain.clientEid, domain.clientCna, domain.serverEids, domain.serverCnas};
    return std::any_of(values.begin(), values.end(),
                       [](const std::string& value) { return value.find('"') != std::string::npos; });
}

CommandDescList BuildBroadcastCommands(const SysSentryBroadcastDomain& domain)
{
    CommandDescList commands = {
        {"sentryctl set sentry_urma_comm --server_eid=" + ShellEscape(domain.serverEids) +
             " --client_jetty_id=1000 2>&1",
         "setServerEid"},
    };
    // 缺少本机 CNA 时不下发对端 CNA，避免形成不完整的 CNA 广播配置。
    if (!domain.clientCna.empty() && !domain.serverCnas.empty()) {
        commands.emplace_back("sentryctl set sentry_uvb_comm --server_cna=" + ShellEscape(domain.serverCnas) + " 2>&1",
                              "setServerCna");
    }
    return commands;
}

UbseResult ExecuteSentryCommands(const CommandDescList& commands)
{
    for (const auto& [command, desc] : commands) {
        std::string commandResult;
        if (ubse::utils::UbseOsUtil::Exec(command, commandResult) != UBSE_OK) {
            UBSE_LOG_DEBUG << "Failed to execute: " << desc;
            return UBSE_RAS_ERROR_SET_SENTRY_REPORTER;
        }
    }
    return UBSE_OK;
}

UbseResult RefreshSysSentryFaultBroadcastDomain()
{
    SysSentryBroadcastDomain domain;
    auto ret = BuildSysSentryBroadcastDomain(domain);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to rebuild sysSentry broadcast domain, " << FormatRetCode(ret);
        return ret;
    }
    if (HasInvalidCommandValue(domain)) {
        UBSE_LOG_ERROR << "sysSentry broadcast configuration contains an invalid double quote";
        return UBSE_ERROR_INVAL;
    }
    // 对端集合未变化时保留现有配置，避免重复执行 sentryctl。
    if (g_lastSuccessfulPeers.has_value() && *g_lastSuccessfulPeers == domain.peerNodeIds) {
        UBSE_LOG_DEBUG << "Skip sysSentry broadcast refresh because peer nodes are unchanged, peerCount="
                       << domain.peerNodeIds.size();
        return UBSE_OK;
    }

    ret = ExecuteSentryCommands(BuildBroadcastCommands(domain));
    if (ret != UBSE_OK) {
        // 执行函数已记录失败命令，此处补充动态刷新阶段上下文。
        UBSE_LOG_WARN << "Failed to apply sysSentry broadcast refresh, " << FormatRetCode(ret);
        return ret;
    }
    if (domain.serverCnas.empty()) {
        UBSE_LOG_WARN << "No peer CNA available, skip server_cna configuration";
    }
    g_lastSuccessfulPeers = domain.peerNodeIds;
    return UBSE_OK;
}

void ResetSysSentryFaultBroadcastDomain()
{
    g_lastSuccessfulPeers.reset();
}

namespace {
UbseResult Build1dBroadcastDomain(SysSentryBroadcastDomain& domain)
{
    domain = {};
    auto ret = GetEids(domain.clientEid, domain.serverEids);
    if (ret != UBSE_OK) {
        // GetEids 已记录具体数据源失败，此处补充 1D 广播域上下文。
        UBSE_LOG_DEBUG << "Stop building 1D broadcast domain because legacy EID collection failed, "
                       << FormatRetCode(ret);
        return ret;
    }

    // 1D 保留原有 EID 获取方式；CNA 不可用时仅跳过 CNA，不阻断 EID 下发。
    UbseMtiCpuTopoInfoMap topology;
    ret = UbseMtiInterface::GetInstance().GetClusterCpuTopo(topology);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get CPU topology failed, skip 1D CNA configuration";
        return UBSE_OK;
    }
    UbseMtiNodeInfo localNodeInfo;
    ret = UbseMtiInterface::GetInstance().GetLocalNodeInfo(localNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get local node info failed for 1D CNA configuration, " << FormatRetCode(ret);
        return UBSE_OK;
    }
    if (localNodeInfo.nodeId.empty()) {
        UBSE_LOG_WARN << "Local nodeId is empty for 1D CNA configuration";
        return UBSE_OK;
    }
    if (Build1dCnaConfig(localNodeInfo.nodeId, topology, domain.clientCna, domain.serverCnas) != UBSE_OK) {
        // 本机 CNA 不可用时清空 CNA 结果，完整配置只下发原有 EID。
        UBSE_LOG_WARN << "Build 1D CNA configuration failed, skip CNA configuration";
        domain.clientCna.clear();
        domain.serverCnas.clear();
    }
    return UBSE_OK;
}

UbseResult BuildClosBroadcastDomainFromPeers(const std::string& localNodeId, const std::vector<uint32_t>& peerNodeIds,
                                             SysSentryBroadcastDomain& domain)
{
    // 两种 CLOS 场景仅选点规则不同，EID/CNA 的组装过程保持一致。
    std::map<UbseMtiIouInfo, UbseMtiEidGroup> allEids;
    auto ret = UbseMtiInterface::GetInstance().GetMtiComEid(allEids);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get local communication EIDs failed, " << FormatRetCode(ret);
        return ret;
    }
    return BuildClosBroadcastDomain(localNodeId, peerNodeIds, allEids, domain);
}

UbseResult BuildHierarchicalClosBroadcastDomain(const ubse::nodeMgr::UbseNodeStaticInfo& currentNode,
                                                SysSentryBroadcastDomain& domain)
{
    // 分层 CLOS 使用当前组返回的所有其他节点。
    const auto groupNodes = ubse::nodeMgr::GetNodesByGroupId(currentNode.groupId);
    UBSE_LOG_INFO << "Build hierarchical CLOS broadcast domain for node=" << currentNode.nodeId
                  << ", groupSize=" << groupNodes.size();
    std::vector<uint32_t> peerNodeIds;
    for (const auto& node : groupNodes) {
        // 广播域对端不包含本节点。
        if (node.nodeId == currentNode.nodeId) {
            continue;
        }
        UBSE_LOG_DEBUG << "Add hierarchical CLOS peer node=" << node.nodeId;
        uint32_t peerNodeId = 0;
        if (!ParseDecimalUint32(node.nodeId, peerNodeId)) {
            UBSE_LOG_ERROR << "Invalid hierarchical CLOS peer nodeId=" << node.nodeId;
            return UBSE_ERROR_CONF_INVALID;
        }
        peerNodeIds.push_back(peerNodeId);
    }
    if (peerNodeIds.empty()) {
        // 当前组暂无对端时仍下发本机身份，后续节点发现不影响本次启动。
        UBSE_LOG_WARN << "No hierarchical CLOS peer node available, configure local endpoints only";
    }
    return BuildClosBroadcastDomainFromPeers(currentNode.nodeId, peerNodeIds, domain);
}

UbseResult BuildSpecifiedRootClosBroadcastDomain(const ubse::nodeMgr::UbseNodeStaticInfo& currentNode,
                                                 SysSentryBroadcastDomain& domain)
{
    // 指定根节点场景按 nodeId 排序，选择本节点相邻的八个节点。
    const auto allNodes = ubse::nodeMgr::GetAllNodes();
    std::vector<uint32_t> peerNodeIds;
    auto ret = SelectRootPeerNodeIds(allNodes, currentNode.nodeId, peerNodeIds);
    if (ret != UBSE_OK) {
        // 选点函数已记录候选 nodeId 根因，此处补充指定根场景上下文。
        UBSE_LOG_DEBUG << "Stop building specified-root CLOS broadcast domain because peer selection failed, "
                       << FormatRetCode(ret);
        return ret;
    }
    return BuildClosBroadcastDomainFromPeers(currentNode.nodeId, peerNodeIds, domain);
}
} // namespace

UbseResult BuildSysSentryBroadcastDomain(SysSentryBroadcastDomain& domain)
{
    // 非 CLOS 组网沿用 1D EID 构建路径，并补充同 DIE CNA。
    if (!UbseSmbios::GetInstance().IsClosType()) {
        UBSE_LOG_DEBUG << "Build sysSentry broadcast domain for 1D topology";
        return Build1dBroadcastDomain(domain);
    }

    const auto currentNode = ubse::nodeMgr::GetCurrentNode();
    // 无指定根列表时使用当前组作为分层 CLOS 广播域。
    if (ubse::nodeMgr::GetRootIpList().empty()) {
        UBSE_LOG_DEBUG << "Build sysSentry broadcast domain for hierarchical CLOS topology";
        return BuildHierarchicalClosBroadcastDomain(currentNode, domain);
    }
    // 指定根场景从当前全部节点中选择相邻八个对端。
    UBSE_LOG_DEBUG << "Build sysSentry broadcast domain for specified-root CLOS topology";
    return BuildSpecifiedRootClosBroadcastDomain(currentNode, domain);
}
} // namespace syssentry
