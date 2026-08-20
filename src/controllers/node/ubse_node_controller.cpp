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

#include "ubse_node_controller.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <fstream>
#include <queue>
#include <regex>
#include <set>

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/istreamwrapper.h"

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_event.h"
#include "ubse_net_util.h"
#include "ubse_node.h"
#include "ubse_node_com_urma_collector.h"
#include "ubse_node_controller_agent.h"
#include "ubse_node_controller_collector.h"
#include "ubse_node_controller_util.h"
#include "ubse_node_mgr.h"
#include "ubse_serial_util.h"
#include "ubse_str_util.h"
#include "ubse_timer.h"
#include "adapter_plugins/mti/ubse_mti_def.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "adapter_plugins/mti/ubse_smbios.h"
#include "securec.h"

namespace ubse::nodeController {
using namespace ubse::context;
using namespace ubse::election;
using namespace ubse::serial;
using namespace ubse::config;
using namespace ubse::adapter_plugins::mti;
using namespace ubse::adapter_plugins::smbios;
using namespace ubse::log;
using namespace ubse::common::def;
using namespace ubse::utils;
UBSE_DEFINE_THIS_MODULE("ubse");

const uint32_t LOCAL_HANDLER_RETRY_DURATION = 2;
const uint32_t IPV4_LENGTH = 4;
const uint32_t IPV6_LENGTH = 16;
const size_t MAX_HOSTNAME_LENGTH = 63;
constexpr size_t MAX_IP_ADDR_NUM = 1024;
constexpr uint32_t FAULT_STATE_PROTECT_SECONDS = 60;
const std::string UBSE_NODE_SUB_HEALTH_TIMER = "UbseNodeSubHealth";
const std::string UBSE_PORT_UP_DOWN_EVENT = "topology.port.linkupdown";
const std::string SUB_HEALTH_CONF_SECTION = "ubse.memory";
const std::string SUB_HEALTH_ENABLED_KEY = "subHealthPenaltyEnabled";
const std::string SUB_HEALTH_REFRESH_INTERVAL_KEY = "subHealthRefreshInterval";
constexpr uint32_t SUB_HEALTH_DEFAULT_REFRESH_INTERVAL = 60;
const std::string SUB_HEALTH_DETECTION_FILE = "/var/log/ubse/detection.json";  

struct SubHealthPortLocation {
    std::string nodeId;
    uint32_t socketId;
    bool available;
};

std::string NormalizeSubHealthEid(std::string eid)
{
    std::transform(eid.begin(), eid.end(), eid.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return eid;
}

bool IsSubHealthNodeAvailable(const UbseNodeInfo& nodeInfo)
{
    return nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_UNKNOWN &&
           nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_FAULT;
}

size_t UbseNodeController::SubHealthSocketPairHash::operator()(const SubHealthSocketPair& key) const
{
    size_t hash = std::hash<std::string>{}(key.importNodeId);
    hash ^= std::hash<std::string>{}(key.exportNodeId) << 1;
    hash ^= std::hash<uint32_t>{}(key.importSocketId) << 2;
    hash ^= std::hash<uint32_t>{}(key.exportSocketId) << 3;
    return hash;
}

size_t UbseNodeController::SubHealthHostExportHash::operator()(const SubHealthHostExport& key) const
{
    size_t hash = std::hash<std::string>{}(key.importNodeId);
    hash ^= std::hash<std::string>{}(key.exportNodeId) << 1;
    hash ^= std::hash<uint32_t>{}(key.exportSocketId) << 2;
    return hash;
}

/**
 * 从 LCNE 模块获取全量静态节点列表，用于选主模块查询全量节点列表，做选主操作
 * @return
 */
std::vector<UbseNodeInfo> UbseNodeController::GetStaticNodeInfo()
{
    std::vector<UbseNodeInfo> nodeInfos{};
    std::vector<UbseMtiNodeInfo> ubseNodeInfos{};
    auto ret = UbseMtiInterface::GetInstance().GetClusterNodeInfoList(ubseNodeInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get all node infos from lcne failed, " << FormatRetCode(ret);
        return {};
    }

    const bool isClosType = UbseSmbios::GetInstance().IsClosType();
    std::string currentNodeId;
    if (isClosType) {
        currentNodeId = GetCurNode().nodeId;
    }

    nodeInfos.reserve(ubseNodeInfos.size());
    for (const auto& node : ubseNodeInfos) {
        if (isClosType && node.nodeId != currentNodeId) {
            continue;
        }
        UbseNodeInfo ubseNodeInfo{node.nodeId};
        auto cpyRet = strcpy_s(ubseNodeInfo.bondingEid, sizeof(ubseNodeInfo.bondingEid), node.eid.c_str());
        if (cpyRet != EOK) {
            UBSE_LOG_ERROR << "nodeId=" << node.nodeId << " copy eid failed," << FormatRetCode(ret);
            continue;
        }
        nodeInfos.push_back(ubseNodeInfo);
    }
    return nodeInfos;
}

/**
 * 获取全量节点列表，agent节点向主节点请求
 * @return
 */
std::unordered_map<std::string, UbseNodeInfo> UbseNodeController::GetAllNodes()
{
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "election module not load";
        return {};
    }

    auto getCachedNodes = [this]() {
        std::shared_lock<std::shared_mutex> lock(rwMutex);
        return nodeInfos;
    };

    const bool isClosType = UbseSmbios::GetInstance().IsClosType();

    // 非CLOS场景，当前主节点直接返回本地全量缓存
    if (!isClosType && module->IsLeader()) {
        auto cachedNodes = getCachedNodes();

        std::stringstream nodeList;
        for (const auto& [nodeId, nodeInfo] : cachedNodes) {
            nodeList << nodeId << "(group=" << nodeInfo.groupId
                     << ",state=" << static_cast<uint32_t>(nodeInfo.clusterState) << "), ";
        }

        UBSE_LOG_INFO << "[GET_ALL_RESULT] source=local"
                      << ", reason=non-clos leader"
                      << ", currentNodeId=" << currentNodeId << ", nodeCount=" << cachedNodes.size() << ", nodes=["
                      << nodeList.str() << "]";
        return cachedNodes;
    }

    Node masterNode{};
    auto ret = module->UbseGetMasterNode(masterNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get master node failed, " << FormatRetCode(ret);
        return {};
    }

    if (masterNode.id.empty()) {
        UBSE_LOG_ERROR << "master node id is empty";
        return {};
    }

    UBSE_LOG_INFO << "[GET_ALL_ROUTE] currentNodeId=" << currentNodeId << ", isClosType=" << isClosType
                  << ", targetMasterId=" << masterNode.id;

    // CLOS场景下，UbseGetMasterNode返回全局主
    if (masterNode.id == currentNodeId) {
        auto cachedNodes = getCachedNodes();

        std::stringstream nodeList;
        for (const auto& [nodeId, nodeInfo] : cachedNodes) {
            nodeList << nodeId << "(group=" << nodeInfo.groupId
                     << ",state=" << static_cast<uint32_t>(nodeInfo.clusterState) << "), ";
        }

        UBSE_LOG_INFO << "[GET_ALL_RESULT] source=local"
                      << ", reason=target is current node"
                      << ", currentNodeId=" << currentNodeId << ", targetMasterId=" << masterNode.id
                      << ", nodeCount=" << cachedNodes.size() << ", nodes=[" << nodeList.str() << "]";
        return cachedNodes;
    }

    std::vector<UbseNodeInfo> infos{};
    ret = GetAllNodeInfoFromRemote(masterNode.id, infos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get all node from master=" << masterNode.id << " failed, " << FormatRetCode(ret);
        return {};
    }

    std::unordered_map<std::string, UbseNodeInfo> maps{};
    maps.reserve(infos.size());

    std::stringstream nodeList;
    for (const auto& info : infos) {
        maps[info.nodeId] = info;
        nodeList << info.nodeId << "(group=" << info.groupId << ",state=" << static_cast<uint32_t>(info.clusterState)
                 << "), ";
    }

    UBSE_LOG_INFO << "[GET_ALL_RESULT] source=remote"
                  << ", currentNodeId=" << currentNodeId << ", targetMasterId=" << masterNode.id
                  << ", nodeCount=" << maps.size() << ", nodes=[" << nodeList.str() << "]";

    return maps;
}

std::unordered_map<std::string, UbseNodeInfo> UbseNodeController::GetLocalNodeInfos()
{
    std::shared_lock<std::shared_mutex> lock(rwMutex);
    return nodeInfos;
}

UbseNodeInfo UbseNodeController::GetCurNode()
{
    if (currentNodeId.empty() || nodeInfos.find(currentNodeId) == nodeInfos.end()) {
        UbseNodeInfo info{};
        GetCurNodeInfo(info);
        currentNodeId = info.nodeId;
        return info;
    }
    return GetNodeById(currentNodeId);
}

UbseNodeInfo UbseNodeController::GetNodeById(const std::string& nodeId)
{
    std::shared_lock<std::shared_mutex> lock(rwMutex);
    auto iter = nodeInfos.find(nodeId);
    if (iter == nodeInfos.end()) {
        UBSE_LOG_ERROR << "nodeId=" << nodeId << " not found";
        return UbseNodeInfo{};
    }
    return iter->second;
}

UbseNodeInfo UbseNodeController::GetNodeBySlotId(uint32_t slotId)
{
    std::shared_lock<std::shared_mutex> lock(rwMutex);
    for (auto& iter : nodeInfos) {
        if (iter.second.slotId != slotId) {
            continue;
        }
        return iter.second;
    }
    UBSE_LOG_ERROR << "slotId=" << slotId << " not found";
    return {};
}

uint32_t UbseNodeController::GetLocalEidBySocket(const uint32_t& socketId, uint32_t& eid)
{
    auto node = GetNodeById(currentNodeId);
    for (const auto& [_, cpuInfo] : node.cpuInfos) {
        if (cpuInfo.socketId == socketId) {
            UBSE_LOG_INFO << "nodeId=" << currentNodeId << ", socketId=" << socketId << " , eid=" << cpuInfo.eid;
            return ConvertStrToUint32(cpuInfo.eid, eid, NO_16);
        }
    }
    UBSE_LOG_ERROR << "nodeId=" << currentNodeId << ", socketId=" << socketId << " not found";
    return UBSE_ERROR;
}

uint32_t UbseNodeController::GetEid(const std::string& nodeId, const uint32_t& socketId, uint32_t& eid)
{
    if (currentNodeId == nodeId) {
        return GetLocalEidBySocket(socketId, eid);
    }
    auto nodes = GetAllNodes();
    auto iter = nodes.find(nodeId);
    if (iter == nodes.end()) {
        UBSE_LOG_ERROR << "nodeId=" << nodeId << " not found";
        return UBSE_ERROR;
    }
    auto node = iter->second;
    for (const auto& [_, cpuInfo] : node.cpuInfos) {
        if (cpuInfo.socketId == socketId) {
            UBSE_LOG_INFO << "nodeId=" << nodeId << ", socketId=" << socketId << " , eid=" << cpuInfo.eid;
            return ConvertStrToUint32(cpuInfo.eid, eid, NO_16);
        }
    }
    UBSE_LOG_ERROR << "nodeId=" << nodeId << ", socketId=" << socketId << " not found";
    return UBSE_ERROR;
}

UbseResult CheckHostNameCharacters(std::string hostName)
{
    for (size_t i = 0; i < hostName.size(); i++) {
        if (!isdigit(hostName[i]) && !islower(hostName[i]) && !isupper(hostName[i]) && hostName[i] != '-') {
            UBSE_LOG_WARN << "The hostname=" << hostName << "has illegal characters.";
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

UbseResult CheckHostName(std::string hostName)
{
    if (hostName.size() > MAX_HOSTNAME_LENGTH) {
        UBSE_LOG_WARN << "The length of the hostname=" << hostName << " exceeds 63 characters.";
        return UBSE_ERROR;
    }
    if (hostName.empty()) {
        UBSE_LOG_WARN << "The hostname=" << hostName << " is empty.";
        return UBSE_ERROR;
    }
    if (CheckHostNameCharacters(hostName) != UBSE_OK) {
        return UBSE_ERROR;
    }
    if (hostName[0] == '-' || hostName[hostName.size() - 1] == '-') {
        UBSE_LOG_WARN << "The hostname=" << hostName << " contains '-' at the beginning or end.";
        return UBSE_ERROR;
    }
    if (isdigit(hostName[0])) {
        UBSE_LOG_WARN << "The hostname=" << hostName << " starts with a number.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult CheckGroupList(std::vector<std::vector<std::string>>& groupListVec, UbseMemGroupNodeList& groupList)
{
    std::unordered_set<std::string> globalSeen;
    std::unordered_map<std::string, UbseNodeInfo> nodesMap = UbseNodeController::GetInstance().GetAllNodes();
    std::unordered_map<std::string, UbseNodeInfo> hostnameMap{};
    for (auto& kv : nodesMap) {
        hostnameMap[kv.second.hostName] = kv.second;
    }
    size_t totalCount =
        std::accumulate(groupListVec.begin(), groupListVec.end(), size_t(0),
                        [](size_t sum, const std::vector<std::string>& group) { return sum + group.size(); });
    if (totalCount != nodesMap.size()) {
        UBSE_LOG_ERROR << "the number of group list hosts does not match lcne static node number.";
        return UBSE_ERROR_CONF_INVALID;
    }
    for (auto& group : groupListVec) {
        std::vector<UbseNodeInfo> groupNodeInfo;
        for (auto& hostname : group) {
            if (globalSeen.count(hostname)) {
                continue; // 过滤重复的hostname
            }
            // 检查hostname长度、字符是否有效
            if (CheckHostName(hostname) != UBSE_OK) {
                continue;
            }
            // 检验hostname是否有效
            auto it = hostnameMap.find(hostname);
            if (it == hostnameMap.end()) {
                UBSE_LOG_WARN << "hostname:" << hostname << " is invalid";
                continue;
            }
            groupNodeInfo.push_back(it->second);
            globalSeen.insert(hostname);
        }
        groupList.push_back(groupNodeInfo);
    }
    return UBSE_OK;
}

uint32_t UbseNodeController::GetMemGroupNodeList(UbseMemGroupNodeList& groupList)
{
    // 1.读配置
    auto confModule = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (confModule == nullptr) {
        UBSE_LOG_ERROR << "conf module not init";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    std::string grouListConf;
    auto ret = confModule->GetConf<std::string>("ubse.memory", "group", grouListConf);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "cannot access the group conf. Use the default set." << FormatRetCode(ret);
        return UBSE_OK;
    }
    // 2.check配置
    std::vector<std::string> groupListConfVec;
    std::vector<std::vector<std::string>> groupListVec;
    Split(grouListConf, ";", groupListConfVec);
    for (auto& groupConf : groupListConfVec) {
        std::vector<std::string> groups;
        Split(groupConf, ",", groups);
        groupListVec.push_back(groups);
    }
    ret = CheckGroupList(groupListVec, groupList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "the group conf is invalid" << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

UbseResult CheckProviderList(std::vector<std::string>& providerListConfVec, UbseMemProviderNodeList& providerList)
{
    std::unordered_set<std::string> globalSeen;
    std::unordered_map<std::string, UbseNodeInfo> nodesMap = UbseNodeController::GetInstance().GetAllNodes();
    std::unordered_map<std::string, UbseNodeInfo> hostnameMap{};
    for (auto& kv : nodesMap) {
        hostnameMap[kv.second.hostName] = kv.second;
    }
    // 总个数不大于静态规划节点数
    if (providerListConfVec.size() >= nodesMap.size()) {
        UBSE_LOG_ERROR << "the number of provider is more than lcne static node number.";
        return UBSE_ERROR_CONF_INVALID;
    }
    for (auto& hostname : providerListConfVec) {
        if (globalSeen.count(hostname)) {
            continue; // 过滤重复的hostname
        }
        // 检查hostname长度、字符是否有效
        if (CheckHostName(hostname) != UBSE_OK) {
            continue;
        }
        // 检验hostname是否有效
        auto it = hostnameMap.find(hostname);
        if (it == hostnameMap.end()) {
            UBSE_LOG_WARN << "hostname:" << hostname << " is invalid";
            continue;
        }
        providerList.push_back(it->second);
        globalSeen.insert(hostname);
    }
    return UBSE_OK;
}

uint32_t UbseNodeController::GetMemProviderNodeList(UbseMemProviderNodeList& providerList)
{
    auto confModule = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (confModule == nullptr) {
        UBSE_LOG_ERROR << "conf module not init";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    std::string providerConf;
    auto ret = confModule->GetConf<std::string>("ubse.memory", "provider", providerConf);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "cannot access the provider conf. Use the default set." << FormatRetCode(ret);
        return UBSE_OK;
    }
    std::vector<std::string> providerListConfVec;
    Split(providerConf, ",", providerListConfVec);
    ret = CheckProviderList(providerListConfVec, providerList);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "the provider conf is invalid." << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

// 注册本节点状态变更回调
uint32_t UbseNodeController::RegLocalStateNotifyHandler(const UbseLocalStateNotifyHandler& handler)
{
    std::unique_lock<std::shared_mutex> lock(rwMutex);
    UBSE_LOG_INFO << "register node local state";
    localNotifyHandlers.push_back(handler);
    return UBSE_OK;
}

// 注册中心侧节点状态变更回调
uint32_t UbseNodeController::RegClusterStateNotifyHandler(const UbseClusterStateNotifyHandler& handler)
{
    if (handler == nullptr) {
        UBSE_LOG_ERROR << "register cluster state notify handler failed, handler is null";
        return UBSE_ERROR_NULLPTR;
    }
    std::vector<UbseNodeInfo> nodeInfoList;
    {
        std::unique_lock<std::shared_mutex> lock(rwMutex);
        clusterNotifyHandlers.push_back(handler);
        nodeInfoList.reserve(nodeInfos.size());
        for (const auto& [_, nodeInfo] : nodeInfos) {
            nodeInfoList.push_back(nodeInfo);
        }
    }
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_WARN << "election module not load, skip sync current cluster state";
        return UBSE_OK;
    }
    if (!module->IsLeader()) {
        return UBSE_OK;
    }
    for (const auto& nodeInfo : nodeInfoList) {
        auto ret = handler(nodeInfo);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "nodeId=" << nodeInfo.nodeId
                           << " sync current cluster state=" << static_cast<uint32_t>(nodeInfo.clusterState)
                           << " failed, " << FormatRetCode(ret);
            continue;
        }
        UBSE_LOG_INFO << "nodeId=" << nodeInfo.nodeId
                      << " sync current cluster state=" << static_cast<uint32_t>(nodeInfo.clusterState) << " success";
    }
    return UBSE_OK;
}

uint32_t UbseNodeController::RegGlobalStateNotifyHandler(const UbseGlobalStateNotifyHandler& handler)
{
    std::unique_lock<std::shared_mutex> lock(rwMutex);
    globalNotifyHandlers.push_back(handler);
    return UBSE_OK;
}

uint32_t ParseSubHealthLinkArray(
    const rapidjson::Value& result, const std::string& srcEid, const SubHealthPortLocation& srcLocation,
    const std::unordered_map<std::string, SubHealthPortLocation>& portLocations,
    UbseSubHealthFlagMap& subHealthFlags, size_t& linkCount)
{
    const char* dstEidsKey = "sub_health_dst_eids";
    const char* latenciesKey = "sub_health_latencies";

    if (!result.HasMember(dstEidsKey) || !result[dstEidsKey].IsArray() ||
        !result.HasMember(latenciesKey) || !result[latenciesKey].IsArray()) {
        UBSE_LOG_ERROR << "[SUB_HEALTH] invalid detection result"
                       << ", srcEid=" << srcEid;
        return UBSE_ERROR_INVAL;
    }

    const auto& dstEids = result[dstEidsKey];
    const auto& latencies = result[latenciesKey];

    if (dstEids.Size() != latencies.Size()) {
        UBSE_LOG_ERROR << "[SUB_HEALTH] dst eid and latency size mismatch"
                       << ", srcEid=" << srcEid
                       << ", dstCount=" << dstEids.Size()
                       << ", latencyCount=" << latencies.Size();
        return UBSE_ERROR_INVAL;
    }

    for (rapidjson::SizeType i = 0; i < dstEids.Size(); ++i) {
        if (!dstEids[i].IsString() || !latencies[i].IsNumber()) {
            UBSE_LOG_ERROR << "[SUB_HEALTH] invalid link item"
                           << ", srcEid=" << srcEid
                           << ", index=" << i;
            return UBSE_ERROR_INVAL;
        }

        std::string dstEid = dstEids[i].GetString();
        auto dstIter = portLocations.find(NormalizeSubHealthEid(dstEid));
        if (dstIter == portLocations.end()) {
            UBSE_LOG_WARN << "[SUB_HEALTH] destination primary eid not found in topology"
                          << ", srcEid=" << srcEid
                          << ", dstEid=" << dstEid
                          << ", keep old cache";
            return UBSE_ERROR;
        }

        const auto& dstLocation = dstIter->second;
        double latency = latencies[i].GetDouble();

        // detection.json中记录的链路已经由hikptool判定为亚健康
        bool isSubHealthy = srcLocation.available && dstLocation.available;

        // 正向：src -> dst
        UbseSubHealthLinkKey linkKey{
            {srcLocation.nodeId, srcLocation.socketId},
            {dstLocation.nodeId, dstLocation.socketId},
        };

        auto flagIter = subHealthFlags.find(linkKey);
        if (flagIter == subHealthFlags.end()) {
            subHealthFlags[linkKey] = isSubHealthy;
        } else {
            // 同一个SocketPair可能对应多条链路，任意链路亚健康则SocketPair亚健康
            flagIter->second = flagIter->second || isSubHealthy;
        }

        // 反向：dst -> src，亚健康链路按双向缓存
        UbseSubHealthLinkKey reverseLinkKey{
            {dstLocation.nodeId, dstLocation.socketId},
            {srcLocation.nodeId, srcLocation.socketId},
        };

        auto reverseIter = subHealthFlags.find(reverseLinkKey);
        if (reverseIter == subHealthFlags.end()) {
            subHealthFlags[reverseLinkKey] = isSubHealthy;
        } else {
            reverseIter->second = reverseIter->second || isSubHealthy;
        }

        ++linkCount;

        if (isSubHealthy) {
            UBSE_LOG_INFO << "[SUB_HEALTH] sub healthy link detected"
                          << ", srcEid=" << srcEid
                          << ", dstEid=" << dstEid
                          << ", latency=" << latency << "ms"
                          << ", nodeId=" << srcLocation.nodeId
                          << ", socketId=" << srcLocation.socketId
                          << ", peerNodeId=" << dstLocation.nodeId
                          << ", peerSocketId=" << dstLocation.socketId
                          << ", cacheDirection=bidirectional";
        }
    }

    return UBSE_OK;
}

uint32_t UbseNodeController::LoadSubHealthDetection(UbseSubHealthFlagMap& subHealthFlags)
{
    std::ifstream file(SUB_HEALTH_DETECTION_FILE);
    if (!file.is_open()) {
        UBSE_LOG_WARN << "[SUB_HEALTH] open detection file failed"
                      << ", file=" << SUB_HEALTH_DETECTION_FILE
                      << ", keep old cache";
        return UBSE_ERROR;
    }

    rapidjson::IStreamWrapper stream(file);
    rapidjson::Document document;
    document.ParseStream(stream);

    if (document.HasParseError()) {
        UBSE_LOG_ERROR << "[SUB_HEALTH] parse detection file failed"
                       << ", file=" << SUB_HEALTH_DETECTION_FILE
                       << ", offset=" << document.GetErrorOffset()
                       << ", error=" << rapidjson::GetParseError_En(document.GetParseError())
                       << ", keep old cache";
        return UBSE_ERROR_INVAL;
    }

    if (!document.IsObject()) {
        UBSE_LOG_ERROR << "[SUB_HEALTH] detection root is not object"
                       << ", file=" << SUB_HEALTH_DETECTION_FILE
                       << ", keep old cache";
        return UBSE_ERROR_INVAL;
    }

    if (document.ObjectEmpty()) {
        UBSE_LOG_INFO << "[SUB_HEALTH] detection result is empty, no sub healthy link"
                      << ", file=" << SUB_HEALTH_DETECTION_FILE;
        return UBSE_OK;
    }

    auto allNodes = GetAllNodes();
    if (allNodes.empty()) {
        UBSE_LOG_WARN << "[SUB_HEALTH] get all nodes failed, keep old cache";
        return UBSE_ERROR;
    }

    // 构建Primary EID到nodeId/socketId的映射
    std::unordered_map<std::string, SubHealthPortLocation> portLocations;

    for (const auto& [nodeId, nodeInfo] : allNodes) {
        bool available = IsSubHealthNodeAvailable(nodeInfo);

        for (const auto& cpuItem : nodeInfo.cpuInfos) {
            const auto& cpuInfo = cpuItem.second;

            std::string primaryEid = NormalizeSubHealthEid(cpuInfo.primaryEid);
            if (primaryEid.empty()) {
                continue;
            }

            auto iter = portLocations.find(primaryEid);
            if (iter != portLocations.end()) {
                if (iter->second.nodeId != nodeId || iter->second.socketId != cpuInfo.socketId) {
                    UBSE_LOG_ERROR << "[SUB_HEALTH] duplicated primary eid"
                                   << ", eid=" << cpuInfo.primaryEid
                                   << ", oldNodeId=" << iter->second.nodeId
                                   << ", oldSocketId=" << iter->second.socketId
                                   << ", newNodeId=" << nodeId
                                   << ", newSocketId=" << cpuInfo.socketId
                                   << ", keep old cache";
                    return UBSE_ERROR;
                }
                continue;
            }

            portLocations[primaryEid] = {
                nodeId,
                cpuInfo.socketId,
                available,
            };

            UBSE_LOG_INFO << "[SUB_HEALTH] primary eid mapping"
                          << ", nodeId=" << nodeId
                          << ", socketId=" << cpuInfo.socketId
                          << ", primaryEid=" << cpuInfo.primaryEid;
        }
    }

    if (portLocations.empty()) {
        UBSE_LOG_WARN << "[SUB_HEALTH] primary eid mapping is empty, keep old cache";
        return UBSE_ERROR;
    }

    UBSE_LOG_INFO << "[SUB_HEALTH] build primary eid mapping success"
                  << ", nodeCount=" << allNodes.size()
                  << ", eidCount=" << portLocations.size();

    size_t sourceCount = 0;
    size_t linkCount = 0;

    for (auto hostIter = document.MemberBegin(); hostIter != document.MemberEnd(); ++hostIter) {
        std::string host = hostIter->name.GetString();

        if (!hostIter->value.IsObject()) {
            UBSE_LOG_ERROR << "[SUB_HEALTH] host detection result is not object"
                           << ", host=" << host
                           << ", keep old cache";
            return UBSE_ERROR_INVAL;
        }

        const auto& socketResults = hostIter->value;

        for (auto socketIter = socketResults.MemberBegin(); socketIter != socketResults.MemberEnd(); ++socketIter) {
            if (!socketIter->value.IsObject()) {
                UBSE_LOG_ERROR << "[SUB_HEALTH] socket detection result is not object"
                               << ", host=" << host
                               << ", socket=" << socketIter->name.GetString()
                               << ", keep old cache";
                return UBSE_ERROR_INVAL;
            }

            const auto& result = socketIter->value;

            if (!result.HasMember("src_eid") || !result["src_eid"].IsString()) {
                UBSE_LOG_ERROR << "[SUB_HEALTH] src_eid is invalid"
                               << ", host=" << host
                               << ", socket=" << socketIter->name.GetString()
                               << ", keep old cache";
                return UBSE_ERROR_INVAL;
            }

            std::string srcEid = result["src_eid"].GetString();

            auto srcIter = portLocations.find(NormalizeSubHealthEid(srcEid));
            if (srcIter == portLocations.end()) {
                UBSE_LOG_WARN << "[SUB_HEALTH] source primary eid not found in topology"
                              << ", host=" << host
                              << ", socket=" << socketIter->name.GetString()
                              << ", srcEid=" << srcEid
                              << ", keep old cache";
                return UBSE_ERROR;
            }

            ++sourceCount;

            auto ret = ParseSubHealthLinkArray(result, srcEid, srcIter->second, portLocations, subHealthFlags,
                                               linkCount);
            if (ret != UBSE_OK) {
                return ret;
            }
        }
    }

    size_t subHealthyCount = 0;
    for (const auto& item : subHealthFlags) {
        if (item.second) {
            ++subHealthyCount;
        }
    }

    UBSE_LOG_INFO << "[SUB_HEALTH] load detection success"
                  << ", file=" << SUB_HEALTH_DETECTION_FILE
                  << ", sourceCount=" << sourceCount
                  << ", linkCount=" << linkCount
                  << ", socketPairCount=" << subHealthFlags.size()
                  << ", subHealthyPairCount=" << subHealthyCount;

    return UBSE_OK;
}

uint32_t UbseNodeController::UpdateSubHealthCache(const UbseSubHealthFlagMap& subHealthFlags)
{
    std::unordered_set<SubHealthSocketPair, SubHealthSocketPairHash> newSocketPairCache;
    std::unordered_set<SubHealthHostExport, SubHealthHostExportHash> newHostExportCache;

    for (const auto& [linkKey, isSubHealthy] : subHealthFlags) {
        if (!isSubHealthy) {
            continue;
        }

        const auto& importEndpoint = linkKey.first;
        const auto& exportEndpoint = linkKey.second;

        if (importEndpoint.first.empty() || exportEndpoint.first.empty()) {
            UBSE_LOG_WARN << "[SUB_HEALTH] node id is empty, keep old cache";
            return UBSE_ERROR_INVAL;
        }

        SubHealthSocketPair socketPair{
            importEndpoint.first,
            exportEndpoint.first,
            importEndpoint.second,
            exportEndpoint.second,
        };

        newSocketPairCache.insert(socketPair);

        newHostExportCache.insert({
            socketPair.importNodeId,
            socketPair.exportNodeId,
            socketPair.exportSocketId,
        });
    }
    size_t subHealthyCount = 0;
    {
        std::unique_lock<std::shared_mutex> lock(subHealthMutex_);

        for (const auto& item : newSocketPairCache) {
            if (subHealthSocketPairCache_.find(item) == subHealthSocketPairCache_.end()) {
                UBSE_LOG_INFO << "[SUB_HEALTH] link becomes sub healthy"
                              << ", importNodeId=" << item.importNodeId << ", importSocketId=" << item.importSocketId
                              << ", exportNodeId=" << item.exportNodeId << ", exportSocketId=" << item.exportSocketId;
            }
        }

        for (const auto& item : subHealthSocketPairCache_) {
            if (newSocketPairCache.find(item) == newSocketPairCache.end()) {
                UBSE_LOG_INFO << "[SUB_HEALTH] link recovers healthy"
                              << ", importNodeId=" << item.importNodeId << ", importSocketId=" << item.importSocketId
                              << ", exportNodeId=" << item.exportNodeId << ", exportSocketId=" << item.exportSocketId;
            }
        }

        subHealthSocketPairCache_ = std::move(newSocketPairCache);
        subHealthHostExportCache_ = std::move(newHostExportCache);
        subHealthyCount = subHealthSocketPairCache_.size();
    }

    UBSE_LOG_INFO << "[SUB_HEALTH] cache refresh success"
                  << ", socketPairCount=" << subHealthFlags.size() << ", subHealthyPairCount=" << subHealthyCount;

    return UBSE_OK;
}

uint32_t UbseNodeController::RefreshSubHealthCache()
{
    if (!subHealthEnabled_.load()) {
        return UBSE_OK;
    }

    UBSE_LOG_INFO << "[SUB_HEALTH] start refresh"
                  << ", file=" << SUB_HEALTH_DETECTION_FILE;

    UbseSubHealthFlagMap subHealthFlags;
    auto ret = LoadSubHealthDetection(subHealthFlags);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[SUB_HEALTH] load detection failed, keep old cache, "
                      << FormatRetCode(ret);
        return ret;
    }

    ret = UpdateSubHealthCache(subHealthFlags);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[SUB_HEALTH] update cache failed, keep old cache, "
                      << FormatRetCode(ret);
        return ret;
    }

    return UBSE_OK;
}

void UbseNodeController::TriggerSubHealthRefresh()
{
    if (!subHealthEnabled_.load()) {
        return;
    }

    bool expected = false;
    if (!subHealthRefreshPending_.compare_exchange_strong(expected, true)) {
        return;
    }

    std::lock_guard<std::mutex> lock(subHealthExecutorMutex_);
    if (subHealthExecutor_ == nullptr) {
        subHealthRefreshPending_.store(false);
        UBSE_LOG_WARN << "[SUB_HEALTH] executor is null, skip refresh";
        return;
    }

    subHealthExecutor_->Execute([this]() {
        // 当前刷新任务已开始，允许后续变化再合并提交一个刷新任务
        subHealthRefreshPending_.store(false);

        auto ret = RefreshSubHealthCache();
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "[SUB_HEALTH] refresh failed, " << FormatRetCode(ret);
        }
    });
}

void UbseNodeController::RebuildHostExportSubHealthCacheLocked()
{
    subHealthHostExportCache_.clear();

    for (const auto& item : subHealthSocketPairCache_) {
        subHealthHostExportCache_.insert({
            item.importNodeId,
            item.exportNodeId,
            item.exportSocketId,
        });
    }
}

void UbseNodeController::ClearSubHealthCacheForNode(const std::string& nodeId)
{
    if (nodeId.empty()) {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(subHealthMutex_);

    size_t removed = 0;
    for (auto iter = subHealthSocketPairCache_.begin(); iter != subHealthSocketPairCache_.end();) {
        if (iter->importNodeId == nodeId || iter->exportNodeId == nodeId) {
            iter = subHealthSocketPairCache_.erase(iter);
            ++removed;
            continue;
        }
        ++iter;
    }

    if (removed == 0) {
        return;
    }

    RebuildHostExportSubHealthCacheLocked();

    UBSE_LOG_INFO << "[SUB_HEALTH] clear node cache, nodeId=" << nodeId << ", removed=" << removed;
}

bool UbseNodeController::IsSocketPairSubHealthy(const std::string& importNodeId, const std::string& exportNodeId,
                                                uint32_t importSocketId, uint32_t exportSocketId) const
{
    if (!subHealthEnabled_.load()) {
        return false;
    }

    std::shared_lock<std::shared_mutex> lock(subHealthMutex_);
    return subHealthSocketPairCache_.find({importNodeId, exportNodeId, importSocketId, exportSocketId}) !=
           subHealthSocketPairCache_.end();
}

bool UbseNodeController::IsHostExportSubHealthy(const std::string& importNodeId, const std::string& exportNodeId,
                                                uint32_t exportSocketId) const
{
    if (!subHealthEnabled_.load()) {
        return false;
    }

    std::shared_lock<std::shared_mutex> lock(subHealthMutex_);
    return subHealthHostExportCache_.find({importNodeId, exportNodeId, exportSocketId}) !=
           subHealthHostExportCache_.end();
}

uint32_t UbseNodeController::SubHealthPortChangeHandler(std::string&, std::string& eventMessage)
{
    if (eventMessage.rfind("UP;", 0) != 0) {
        return UBSE_OK;
    }

    UBSE_LOG_INFO << "[SUB_HEALTH] port up, trigger refresh, eventMessage=" << eventMessage;
    UbseNodeController::GetInstance().TriggerSubHealthRefresh();
    return UBSE_OK;
}

uint32_t UbseNodeController::StartSubHealth()
{
    if (UbseSmbios::GetInstance().IsClosType()) {
        UBSE_LOG_INFO << "[SUB_HEALTH] clos scenario is not supported, skip start";
        return UBSE_OK;
    }

    auto confModule = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (confModule == nullptr) {
        UBSE_LOG_ERROR << "[SUB_HEALTH] conf module not init";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    bool enabled = false;
    auto ret = confModule->GetConf<bool>(SUB_HEALTH_CONF_SECTION, SUB_HEALTH_ENABLED_KEY, enabled);
    if (ret != UBSE_OK || !enabled) {
        UBSE_LOG_INFO << "[SUB_HEALTH] disabled";
        return UBSE_OK;
    }

    uint32_t refreshInterval = SUB_HEALTH_DEFAULT_REFRESH_INTERVAL;
    ret = confModule->GetConf<uint32_t>(SUB_HEALTH_CONF_SECTION, SUB_HEALTH_REFRESH_INTERVAL_KEY, refreshInterval);
    if (ret != UBSE_OK || refreshInterval == 0) {
        UBSE_LOG_WARN << "[SUB_HEALTH] invalid refresh interval, use default=" << SUB_HEALTH_DEFAULT_REFRESH_INTERVAL
                      << "s";
        refreshInterval = SUB_HEALTH_DEFAULT_REFRESH_INTERVAL;
    }

    {
        std::lock_guard<std::mutex> lock(subHealthExecutorMutex_);
        subHealthExecutor_ = ubse::task_executor::UbseTaskExecutor::Create("UbseNodeSubHealth", NO_1, NO_1024);
        if (subHealthExecutor_ == nullptr || !subHealthExecutor_->Start()) {
            UBSE_LOG_ERROR << "[SUB_HEALTH] start executor failed";
            subHealthExecutor_ = nullptr;
            return UBSE_ERROR_NULLPTR;
        }
    }

    subHealthEnabled_.store(true);

    ret = ubse::timer::UbseTimerHandlerRegister(
        UBSE_NODE_SUB_HEALTH_TIMER,
        [this]() -> uint32_t {
            TriggerSubHealthRefresh();
            return UBSE_OK;
        },
        refreshInterval);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[SUB_HEALTH] register refresh timer failed, " << FormatRetCode(ret);
        StopSubHealth();
        return ret;
    }

    std::string eventId = UBSE_PORT_UP_DOWN_EVENT;
    ret = ubse::event::UbseSubEvent(eventId, SubHealthPortChangeHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[SUB_HEALTH] subscribe port event failed, " << FormatRetCode(ret);
        StopSubHealth();
        return ret;
    }

    UBSE_LOG_INFO << "[SUB_HEALTH] start success, refreshInterval=" << refreshInterval << "s";
    return UBSE_OK;
}

void UbseNodeController::StopSubHealth()
{
    if (!subHealthEnabled_.exchange(false)) {
        return;
    }

    ubse::timer::UbseTimerHandlerUnregister(UBSE_NODE_SUB_HEALTH_TIMER);

    std::string eventId = UBSE_PORT_UP_DOWN_EVENT;
    auto ret = ubse::event::UbseUnSubEvent(eventId, SubHealthPortChangeHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[SUB_HEALTH] unsubscribe port event failed, " << FormatRetCode(ret);
    }

    {
        std::lock_guard<std::mutex> lock(subHealthExecutorMutex_);
        if (subHealthExecutor_ != nullptr) {
            subHealthExecutor_->Stop();
            subHealthExecutor_ = nullptr;
        }
    }

    subHealthRefreshPending_.store(false);

    {
        std::unique_lock<std::shared_mutex> lock(subHealthMutex_);
        subHealthSocketPairCache_.clear();
        subHealthHostExportCache_.clear();
    }

    UBSE_LOG_INFO << "[SUB_HEALTH] stop success";
}

uint32_t UbseNodeController::ExecGlobalStateNotifyHandler(const UbseNodeInfo& node)
{
    std::vector<UbseGlobalStateNotifyHandler> handlers;
    {
        std::shared_lock<std::shared_mutex> lock(rwMutex);
        handlers = globalNotifyHandlers;
    }

    UbseResult ret = UBSE_OK;
    for (auto handler : handlers) {
        if (handler == nullptr) {
            continue;
        }
        ret |= handler(node);
    }

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << node.nodeId << " exec global state notify failed, " << FormatRetCode(ret);
    }
    return ret;
}

void ExecLocalStateHandler(const UbseNodeInfo& nodeInfo, const std::vector<UbseLocalStateNotifyHandler>& handlers)
{
    for (auto handler : handlers) {
        while (true) {
            if (handler == nullptr) {
                break;
            }
            auto ret = handler(nodeInfo);
            if (ret == UBSE_OK) {
                break;
            }
            UBSE_LOG_WARN << "local node exec handler failed, when update local state to "
                          << static_cast<uint32_t>(nodeInfo.localState);
            sleep(LOCAL_HANDLER_RETRY_DURATION);
        }
    }
}

UbseResult ExecClusterStateHandler(const UbseNodeInfo& nodeInfo,
                                   const std::vector<UbseClusterStateNotifyHandler>& handlers)
{
    UbseResult ret = UBSE_OK;
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "election module not load";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    if (!module->IsLeader()) {
        UBSE_LOG_WARN << "current node not master, skip exec cluster state notify";
        return UBSE_OK;
    }
    for (auto handler : handlers) {
        if (handler == nullptr) {
            continue;
        }
        ret |= handler(nodeInfo);
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << nodeInfo.nodeId
                       << " update state=" << static_cast<uint32_t>(nodeInfo.clusterState) << " exec handler failed, "
                       << FormatRetCode(ret);
    }
    return ret;
}

bool CanUpdateNodeClusterState(UbseNodeClusterState curState, UbseNodeClusterState updateState)
{
    switch (curState) {
        case UbseNodeClusterState::UBSE_NODE_INIT:
            return updateState == UbseNodeClusterState::UBSE_NODE_SMOOTHING ||
                   updateState == UbseNodeClusterState::UBSE_NODE_UNKNOWN ||
                   updateState == UbseNodeClusterState::UBSE_NODE_FAULT;
        case UbseNodeClusterState::UBSE_NODE_SMOOTHING:
            return updateState == UbseNodeClusterState::UBSE_NODE_WORKING ||
                   updateState == UbseNodeClusterState::UBSE_NODE_UNKNOWN ||
                   updateState == UbseNodeClusterState::UBSE_NODE_FAULT ||
                   updateState == UbseNodeClusterState::UBSE_NODE_PRE_BMC ||
                   updateState == UbseNodeClusterState::UBSE_NODE_SMOOTHING;
        case UbseNodeClusterState::UBSE_NODE_WORKING:
            return updateState == UbseNodeClusterState::UBSE_NODE_SMOOTHING ||
                   updateState == UbseNodeClusterState::UBSE_NODE_UNKNOWN ||
                   updateState == UbseNodeClusterState::UBSE_NODE_FAULT ||
                   updateState == UbseNodeClusterState::UBSE_NODE_PRE_BMC;
        case UbseNodeClusterState::UBSE_NODE_UNKNOWN:
            return updateState == UbseNodeClusterState::UBSE_NODE_SMOOTHING ||
                   updateState == UbseNodeClusterState::UBSE_NODE_FAULT ||
                   updateState == UbseNodeClusterState::UBSE_NODE_UNKNOWN;
        case UbseNodeClusterState::UBSE_NODE_FAULT:
            return updateState == UbseNodeClusterState::UBSE_NODE_SMOOTHING ||
                   updateState == UbseNodeClusterState::UBSE_NODE_FAULT;
        case UbseNodeClusterState::UBSE_NODE_PRE_BMC:
            return updateState == UbseNodeClusterState::UBSE_NODE_FAULT ||
                   updateState == UbseNodeClusterState::UBSE_NODE_WORKING ||
                   updateState == UbseNodeClusterState::UBSE_NODE_SMOOTHING;
        default: {
            UBSE_LOG_ERROR << "unknown current state: " << static_cast<uint32_t>(curState);
            return false;
        }
    }
}

void UbseNodeController::InitHierarchical()
{
    this->isHierarchical = nodeMgr::GetRootIpList().size() == 0 && nodeMgr::GetGroupSize() > 1;
}

bool UbseNodeController::IsHierarchical() const
{
    return this->isHierarchical;
}

bool UbseNodeController::CanUpdateClusterStateForReport(const UbseNodeInfo& reportNodeInfo,
                                                        UbseNodeClusterState oldState, UbseNodeClusterState mergedState,
                                                        bool isExisting, bool& needRecovery)
{
    needRecovery = false;

    auto curIter = nodeInfos.find(currentNodeId);
    if (curIter == nodeInfos.end()) {
        UBSE_LOG_WARN << "[CLUSTER_STATE_CHECK] current node not in cache, reject update, nodeId="
                      << reportNodeInfo.nodeId;
        return false;
    }
    uint32_t curGroupId = curIter->second.groupId;
    bool sameGroup = (reportNodeInfo.groupId == curGroupId);

    if (sameGroup) {
        if (isExisting) {
            // 后续添加同组：按状态机迁移规则校验
            return CanUpdateNodeClusterState(oldState, mergedState);
        }
        // 首次添加同组：直接允许
        return true;
    }

    // 跨机柜
    if (mergedState != UbseNodeClusterState::UBSE_NODE_WORKING) {
        return true;
    }

    // 跨机柜 + WORKING
    if (isExisting) {
        // 后续添加：取本节点内存里记录的该节点 globalState 判断
        auto nodeIter = nodeInfos.find(reportNodeInfo.nodeId);
        if (nodeIter != nodeInfos.end() &&
            nodeIter->second.globalState == UbseNodeGlobalState::UBSE_NODE_GLOBAL_READY) {
            return true;
        }
        needRecovery = true;
        return false;
    }

    // 首次添加：跨机柜 + WORKING，需先恢复
    needRecovery = true;
    return false;
}

uint32_t UbseNodeController::UpdateNodeInfo(const std::string& nodeId, UbseNodeInfo& info)
{
    std::string nodeIdStr = nodeId;
    if (isHierarchical) {
        return UpdateClosHierarchicalNodeInfo(nodeIdStr, info);
    }
    UbseResult ret = UBSE_OK;
    std::unique_lock<std::shared_mutex> lk(rwMutex);
    if (nodeInfos.find(nodeIdStr) == nodeInfos.end()) {
        UBSE_LOG_INFO << "nodeId=" << nodeIdStr
                      << " first add, update node info, current nodeId=" << GetCurrentNodeId();
        info.clusterState = UbseNodeClusterState::UBSE_NODE_INIT;
        nodeInfos[nodeIdStr] = info;
        // 使用numaInfos更新拓扑数据中的本端信息
        UbseSocketIdChange(nodeIdStr);
        auto notifyInfo = nodeInfos[nodeIdStr];
        lk.unlock();
        if (nodeIdStr == currentNodeId) {
            ExecLocalStateHandler(notifyInfo, localNotifyHandlers);
        }
        ret = ExecClusterStateHandler(notifyInfo, clusterNotifyHandlers);
        // lk 已 unlock，函数返回时无需再操作
        return ret;
    }
    auto& existing = nodeInfos[nodeIdStr];
    info.clusterState = existing.clusterState;
    info.localState = existing.localState;
    if (info.guid.empty() && !existing.guid.empty()) {
        info.guid = existing.guid;
    }
    existing = info;
    UbseSocketIdChange(nodeIdStr);
    lk.unlock();
    return UBSE_OK;
}

uint32_t UbseNodeController::UpdateClosHierarchicalNodeInfo(const std::string& nodeId, UbseNodeInfo& info)
{
    if (nodeId.empty() || info.nodeId.empty()) {
        UBSE_LOG_ERROR << "update node info failed, node id is empty";
        return UBSE_ERROR;
    }
    if (nodeId != info.nodeId) {
        UBSE_LOG_ERROR << "update node info failed, node id mismatch, nodeId=" << nodeId
                       << ", info.nodeId=" << info.nodeId;
        return UBSE_ERROR;
    }
    rwMutex.lock();
    auto iter = nodeInfos.find(nodeId);
    if (iter == nodeInfos.end()) {
        return UpdateNodeInfoFirstAdd(nodeId, info);
    }

    auto& existing = iter->second;
    auto oldClusterState = existing.clusterState;
    auto reportClusterState = info.clusterState;
    info.localState = existing.localState;
    info.globalState = existing.globalState;
    if (info.guid.empty() && !existing.guid.empty()) {
        info.guid = existing.guid;
    }

    UBSE_LOG_INFO << "[CLUSTER_STATE_UPDATE] nodeId=" << nodeId
                  << ", oldState=" << static_cast<uint32_t>(oldClusterState)
                  << ", reportState=" << static_cast<uint32_t>(reportClusterState)
                  << ", newState=" << static_cast<uint32_t>(info.clusterState);

    bool needRecovery = false;
    bool allowed = CanUpdateClusterStateForReport(info, oldClusterState, info.clusterState, true, needRecovery);
    if (!allowed && !needRecovery) {
        // 同组状态机校验失败：拒绝更新，保留旧状态
        rwMutex.unlock();
        UBSE_LOG_WARN << "[CLUSTER_STATE_UPDATE] reject, state machine check failed, nodeId=" << nodeId
                      << ", oldState=" << static_cast<uint32_t>(oldClusterState)
                      << ", newState=" << static_cast<uint32_t>(info.clusterState);
        return UBSE_OK;
    }
    if (!allowed && needRecovery) {
        return UpdateNodeInfoExistingRecovery(nodeId, info, oldClusterState);
    }
    return UpdateNodeInfoExistingNormal(nodeId, info, oldClusterState, existing);
}

UbseResult UbseNodeController::UpdateNodeInfoFirstAdd(const std::string& nodeId, UbseNodeInfo& info)
{
    UBSE_LOG_INFO << "[FIRST_ADD] nodeId=" << nodeId << ", curNodeId=" << GetCurrentNodeId()
                  << ", reportState=" << static_cast<uint32_t>(info.clusterState);
    if (info.nodeId != currentNodeId && nodeInfos.find(currentNodeId) == nodeInfos.end()) {
        UBSE_LOG_WARN << "[CLUSTER_STATE_CHECK] current node not in cache, not allow update, nodeId=" << info.nodeId;
        rwMutex.unlock();
        return UBSE_ERROR_NULLPTR;
    }
    bool needRecovery = false;
    CanUpdateClusterStateForReport(info, info.clusterState, info.clusterState, false, needRecovery);

    UbseResult recoveryRet = UBSE_OK;
    if (needRecovery) {
        rwMutex.unlock();
        UbseNodeInfo nodeForRecovery = info;
        recoveryRet = ExecGlobalStateNotifyHandler(nodeForRecovery);
        if (recoveryRet != UBSE_OK) {
            UBSE_LOG_WARN << "[FIRST_ADD] recovery failed, downgrade to INIT, nodeId=" << nodeId << ", "
                          << FormatRetCode(recoveryRet);
            info.clusterState = UbseNodeClusterState::UBSE_NODE_INIT;
            // 状态与全局恢复状态一致降级，避免残留 READY 绕过后续恢复校验
            info.globalState = UbseNodeGlobalState::UBSE_NODE_GLOBAL_INIT;
        }
        rwMutex.lock();
    }

    if (currentNodeId != nodeId && nodeInfos.find(currentNodeId) == nodeInfos.end()) {
        rwMutex.unlock();
        UBSE_LOG_WARN << "[FIRST_ADD] current node removed during recovery, abort, nodeId=" << nodeId;
        return UBSE_ERROR_NULLPTR;
    }
    if (nodeInfos.find(nodeId) != nodeInfos.end()) {
        // 期间已被其他线程插入，放弃本次写入由该路径负责
        rwMutex.unlock();
        UBSE_LOG_INFO << "[FIRST_ADD] node already added during recovery, abort, nodeId=" << nodeId;
        return UBSE_OK;
    }
    nodeInfos[nodeId] = info;
    if (recoveryRet == UBSE_OK && info.clusterState == UbseNodeClusterState::UBSE_NODE_WORKING) {
        nodeInfos[nodeId].globalState = UbseNodeGlobalState::UBSE_NODE_GLOBAL_READY;
    }
    UbseSocketIdChange(nodeId);
    auto nodeInfoCopy = nodeInfos[nodeId];
    auto localHandlers = localNotifyHandlers;
    auto clusterHandlers = clusterNotifyHandlers;
    rwMutex.unlock();
    UBSE_LOG_INFO << "[FIRST_ADD] write success, nodeId=" << nodeId
                  << ", state=" << static_cast<uint32_t>(nodeInfoCopy.clusterState)
                  << ", global=" << static_cast<uint32_t>(nodeInfoCopy.globalState);
    if (nodeId == currentNodeId) {
        ExecLocalStateHandler(nodeInfoCopy, localHandlers);
    }
    auto ret = ExecClusterStateHandler(nodeInfoCopy, clusterHandlers);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[FIRST_ADD] notify failed, nodeId=" << nodeId << ", " << FormatRetCode(ret);
    } else {
        UBSE_LOG_INFO << "[FIRST_ADD] notify success, nodeId=" << nodeId;
    }
    return ret;
}

UbseResult UbseNodeController::UpdateNodeInfoExistingRecovery(const std::string& nodeId, UbseNodeInfo& info,
                                                              UbseNodeClusterState oldClusterState)
{
    // 跨机柜 + WORKING + 未 READY：释放锁，在锁外执行数据恢复
    rwMutex.unlock();
    UbseNodeInfo nodeForRecovery = info;
    auto recoveryRet = ExecGlobalStateNotifyHandler(nodeForRecovery);
    if (recoveryRet != UBSE_OK) {
        UBSE_LOG_WARN << "[RECOVERY] recovery failed, reject update, nodeId=" << nodeId << ", "
                      << FormatRetCode(recoveryRet);
        return UBSE_OK;
    }
    // 恢复成功：重新加锁写回
    rwMutex.lock();
    if (nodeInfos.find(currentNodeId) == nodeInfos.end()) {
        rwMutex.unlock();
        UBSE_LOG_WARN << "[CLUSTER_STATE_CHECK] current node not in cache, reject update, nodeId=" << nodeId;
        return UBSE_ERROR_NULLPTR;
    }
    auto reIter = nodeInfos.find(nodeId);
    if (reIter == nodeInfos.end()) {
        rwMutex.unlock();
        UBSE_LOG_WARN << "[RECOVERY] node removed during recovery, nodeId=" << nodeId;
        return UBSE_OK;
    }
    reIter->second = info;
    reIter->second.globalState = UbseNodeGlobalState::UBSE_NODE_GLOBAL_READY;
    UbseSocketIdChange(nodeId);
    auto nodeInfoCopy = reIter->second;
    auto handlers = clusterNotifyHandlers;
    rwMutex.unlock();
    UBSE_LOG_INFO << "[RECOVERY] write success, nodeId=" << nodeId << ", old=" << static_cast<uint32_t>(oldClusterState)
                  << ", new=" << static_cast<uint32_t>(nodeInfoCopy.clusterState) << ", global=READY";
    if (oldClusterState == nodeInfoCopy.clusterState) {
        UBSE_LOG_INFO << "[RECOVERY] state unchanged, skip notify, nodeId=" << nodeId;
        return UBSE_OK;
    }
    auto ret = ExecClusterStateHandler(nodeInfoCopy, handlers);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[RECOVERY] notify failed, nodeId=" << nodeId << ", " << FormatRetCode(ret);
    } else {
        UBSE_LOG_INFO << "[RECOVERY] notify success, nodeId=" << nodeId;
    }
    return ret;
}

UbseResult UbseNodeController::UpdateNodeInfoExistingNormal(const std::string& nodeId, UbseNodeInfo& info,
                                                            UbseNodeClusterState oldClusterState,
                                                            UbseNodeInfo& existing)
{
    existing = info;
    UbseSocketIdChange(nodeId);
    auto nodeInfoCopy = existing;
    auto handlers = clusterNotifyHandlers;
    // 持锁期间读取当前节点信息，避免解锁后数据竞争
    auto curIter = nodeInfos.find(currentNodeId);
    bool curNodeInCache = (curIter != nodeInfos.end());
    uint32_t curGroupId = curNodeInCache ? curIter->second.groupId : 0;
    rwMutex.unlock();

    UBSE_LOG_INFO << "[UPDATE] write success, nodeId=" << nodeId
                  << ", oldState=" << static_cast<uint32_t>(oldClusterState)
                  << ", newState=" << static_cast<uint32_t>(nodeInfoCopy.clusterState);

    if (oldClusterState == nodeInfoCopy.clusterState) {
        UBSE_LOG_INFO << "[UPDATE] state unchanged, skip notify, nodeId=" << nodeId;
        return UBSE_OK;
    }
    if (!curNodeInCache) {
        UBSE_LOG_WARN << "[CLUSTER_STATE_CHECK] current node not in cache, reject update, nodeId=" << info.nodeId;
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = ExecClusterStateHandler(nodeInfoCopy, handlers);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[UPDATE] notify cluster state failed, nodeId=" << nodeId
                       << ", oldState=" << static_cast<uint32_t>(oldClusterState)
                       << ", newState=" << static_cast<uint32_t>(nodeInfoCopy.clusterState) << ", "
                       << FormatRetCode(ret);
    } else {
        UBSE_LOG_INFO << "[UPDATE] notify cluster state success, nodeId=" << nodeId
                      << ", oldState=" << static_cast<uint32_t>(oldClusterState)
                      << ", newState=" << static_cast<uint32_t>(nodeInfoCopy.clusterState);
    }
    return ret;
}

void LogOnSocketIdMismatch(const std::set<uint32_t>& lcneChipIdSet, const std::set<uint32_t>& osSocketIdSet)
{
    if (lcneChipIdSet.size() != osSocketIdSet.size()) {
        UBSE_LOG_WARN << "Mismatch in sockets. lcne reports " << lcneChipIdSet.size() << " CPUs, while OS reports "
                      << osSocketIdSet.size() << " sockets.";
        std::ostringstream oss;
        oss << "os_socket_ids include";
        std::for_each(osSocketIdSet.begin(), osSocketIdSet.end(),
                      [&oss](const uint32_t& osSocketId) { oss << " " << osSocketId; });
        oss << ". lcne_chip_ids include";
        std::for_each(lcneChipIdSet.begin(), lcneChipIdSet.end(),
                      [&oss](const uint32_t& lcneChipId) { oss << " " << lcneChipId; });
        UBSE_LOG_WARN << oss.str();
    }
}

void UbseNodeController::UbseSocketIdChange(const std::string& nodeId)
{
    std::unordered_map<uint32_t, uint32_t> socketIdMap;
    std::set<uint32_t> osSocketIdSet; // 本节点OS-socketId
    std::set<uint32_t> lcneChipIdSet; // 本节点chipId
    // 排序全量SocketID
    for (auto& numa : nodeInfos[nodeId].numaInfos) {
        osSocketIdSet.insert(numa.second.socketId);
    }
    // 排序全量chipID
    for (auto& cpu : nodeInfos[nodeId].cpuInfos) {
        uint32_t chipId;
        ubse::utils::ConvertStrToUint32(cpu.second.chipId, chipId);
        lcneChipIdSet.insert(chipId);
    }
    // 构建映射关系
    LogOnSocketIdMismatch(lcneChipIdSet, osSocketIdSet);
    auto it1 = lcneChipIdSet.begin();
    auto it2 = osSocketIdSet.begin();
    for (size_t i = 0; i < lcneChipIdSet.size() && i < osSocketIdSet.size(); i++) {
        socketIdMap[*it1] = *it2;
        ++it1;
        ++it2;
    }
    // 进行更新,暂不改动key,若两边数据的cpu数量不符，此处只能保证数组不越界
    for (auto& cpu : nodeInfos[nodeId].cpuInfos) {
        uint32_t chipId;
        ubse::utils::ConvertStrToUint32(cpu.second.chipId, chipId);
        auto it = socketIdMap.find(chipId);
        if (it != socketIdMap.end()) {
            cpu.second.socketId = it->second;
        }
    }
}

std::string CreateLinkIdAndPhysicalLink(const LinkInfo& linkInfo, PhysicalLink& physicalLink)
{
    // lcne保证数据可正常转换
    try {
        uint32_t slotId = std::stoi(linkInfo.slotId);
        uint32_t socketId = std::stoi(linkInfo.socketId);
        uint32_t portId = std::stoi(linkInfo.portId);
        uint32_t peerSlotId = std::stoi(linkInfo.peerSlotId);
        uint32_t peerSocketId = std::stoi(linkInfo.peerSocketId);
        uint32_t peerPortId = std::stoi(linkInfo.peerPortId);
        // 小的slotid在前
        if (linkInfo.slotId < linkInfo.peerSlotId) {
            physicalLink = {slotId,     socketId,     portId,     linkInfo.interfaceName,
                            peerSlotId, peerSocketId, peerPortId, linkInfo.peerInterfaceName};
            return linkInfo.slotId + "/" + linkInfo.socketId + "/" + linkInfo.portId + "-" + linkInfo.peerSlotId + "/" +
                   linkInfo.peerSocketId + "/" + linkInfo.peerPortId;
        } else {
            physicalLink = {peerSlotId, peerSocketId, peerPortId, linkInfo.peerInterfaceName,
                            slotId,     socketId,     portId,     linkInfo.interfaceName};
            return linkInfo.peerSlotId + "/" + linkInfo.peerSocketId + "/" + linkInfo.peerPortId + "-" +
                   linkInfo.slotId + "/" + linkInfo.socketId + "/" + linkInfo.portId;
        }
    } catch (const std::exception& e) {
        UBSE_LOG_WARN << "LCNE provides data that cannot be converted to uint32, with the specific data being: "
                      << "slotId=" << linkInfo.slotId << ", socketId=" << linkInfo.socketId
                      << ", portId=" << linkInfo.portId << ", peerSlotId=" << linkInfo.peerSlotId
                      << ", peerSocketId=" << linkInfo.peerSocketId << ", peerPortId=" << linkInfo.peerPortId;
    }
    return "ERROR-LINK";
}

void UbseNodeController::UpdateConnect(PhysicalLink& physicalLink, std::string& linkId)
{
    auto it = devDirConnectInfo.find(linkId);
    if (it == devDirConnectInfo.end()) {
        // 第一次看到, 标记为冲突
        physicalLink.linkStatus = LinkStatus::conflict;
        devDirConnectInfo[linkId] = physicalLink;
    } else {
        // 已存在, 更新为可用, 并合并信息
        PhysicalLink& existing = it->second;
        // 合并接口信息
        if (existing.interfaceName.empty() && !physicalLink.interfaceName.empty()) {
            existing.interfaceName = physicalLink.interfaceName;
        }
        if (existing.peerInterfaceName.empty() && !physicalLink.peerInterfaceName.empty()) {
            existing.peerInterfaceName = physicalLink.peerInterfaceName;
        }
        // 状态更新为可用
        existing.linkStatus = LinkStatus::available;
    }
}

void UbseNodeController::PrintDevDirConnectInfo()
{
    std::stringstream oss;
    oss << "------ DevDirConnectInfo INFO ------\n";
    for (auto& connect : devDirConnectInfo) {
        oss << "LinkId=" << connect.first << ", slotId=" << connect.second.slotId
            << ", chipId=" << connect.second.chipId << ", portId=" << connect.second.portId
            << ", peerSlotId=" << connect.second.peerSlotId << ", peerChipId=" << connect.second.peerChipId
            << ", peerPortId=" << connect.second.peerPortId;
        if (connect.second.linkStatus == LinkStatus::conflict) {
            oss << ", status=conflict";
        }
        oss << "\n";
    }
    UBSE_LOG_INFO << oss.str();
}

void UbseNodeController::UpdateDevDirConnectInfo()
{
    // 遍历该节点链接,每个链接获取linkid
    std::unique_lock<std::shared_mutex> lock(devDirMutex);
    std::unordered_map<std::string, UbseNodeInfo> localNodeInfos;
    {
        std::shared_lock<std::shared_mutex> lockNodeInfos(rwMutex);
        localNodeInfos = nodeInfos;
    }
    devDirConnectInfo.clear();
    for (auto& nodeInfo : localNodeInfos) {
        for (auto& topoInfo : nodeInfo.second.cpuInfos) {
            CreateAndUpdateInfo(topoInfo);
        }
    }
}

void UbseNodeController::CreateAndUpdateInfo(std::pair<const UbseCpuLocation, UbseCpuInfo> topoInfo)
{
    if (UbseSmbios::GetInstance().IsClosType()) {
        return;
    }
    std::string slotId = topoInfo.first.nodeId;
    std::string chipId = topoInfo.second.chipId;

    for (auto& portInfo : topoInfo.second.portInfos) {
        if (portInfo.second.portStatus == PortStatus::DOWN) {
            continue;
        }
        std::string portId = portInfo.first;
        std::string interfaceName = portInfo.second.ifName;
        // 对端
        std::string remoteSlotId = portInfo.second.remoteSlotId;
        std::string remoteChipId = portInfo.second.remoteChipId;
        std::string remotePortId = portInfo.second.remotePortId;
        std::string peerInterfaceName = portInfo.second.remoteIfName;
        // 检查是否有对端信息
        if (remoteSlotId.empty() || remoteChipId.empty() || remotePortId.empty()) {
            continue;
        }
        // 生成linkid 和 要填入的数据
        LinkInfo linkInfo{slotId,       chipId,       portId,       interfaceName,
                          remoteSlotId, remoteChipId, remotePortId, peerInterfaceName};
        PhysicalLink physicalLink{};
        std::string linkId = CreateLinkIdAndPhysicalLink(linkInfo, physicalLink);
        // 更新具体信息
        if (linkId == "ERROR-LINK") { // 异常数据，跳过
            continue;
        }
        UpdateConnect(physicalLink, linkId);
    }
}

void UbseNodeController::UpdateNodeInfoLocalState(UbseNodeLocalState state)
{
    UBSE_LOG_INFO << "nodeId=" << currentNodeId << " start to update local state=" << static_cast<uint32_t>(state);
    rwMutex.lock();
    if (nodeInfos.find(currentNodeId) == nodeInfos.end()) {
        rwMutex.unlock();
        UBSE_LOG_ERROR << "nodeId=" << currentNodeId << " local node info not collect, skip update local state.";
        return;
    }
    nodeInfos[currentNodeId].localState = state;
    auto nodeInfo = nodeInfos[currentNodeId];
    auto handlers = localNotifyHandlers;
    rwMutex.unlock();
    // local 状态的变更，restore需要重试直到平滑成功;
    ExecLocalStateHandler(nodeInfo, handlers);
    UBSE_LOG_INFO << "local node update local state to " << static_cast<uint32_t>(state);
}

uint32_t GenerateFaultUbseNode(const std::string& nodeId, UbseNodeInfo& faultNodeInfo)
{
    faultNodeInfo.nodeId = nodeId;
    auto ret = ConvertStrToUint32(faultNodeInfo.nodeId, faultNodeInfo.slotId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "generate fault nodeId=" << nodeId << ", convert slot id failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "generate fault nodeId=" << nodeId << ", slotId=" << faultNodeInfo.slotId;
    faultNodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_FAULT;
    ret = CollectCpuInfo(faultNodeInfo, nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "generate fault nodeId=" << nodeId << ", cpu info failed, " << FormatRetCode(ret);
        return ret;
    }
    std::vector<UbseMtiNodeInfo> ubseNodeInfos{};
    ret = UbseMtiInterface::GetInstance().GetClusterNodeInfoList(ubseNodeInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "generate fault nodeId=" << nodeId << ", get all nodes failed, " << FormatRetCode(ret);
        return {};
    }
    for (auto& node : ubseNodeInfos) {
        if (nodeId != node.nodeId) {
            continue;
        }
        auto cpyRet = strcpy_s(faultNodeInfo.bondingEid, sizeof(faultNodeInfo.bondingEid), node.eid.c_str());
        if (cpyRet != EOK) {
            UBSE_LOG_ERROR << "generate fault nodeId=" << nodeId << ", copy eid failed, " << FormatRetCode(ret);
        }
        break;
    }
    return UBSE_OK;
}

uint32_t UbseNodeController::UpdateNodeInfoGlobalState(const std::string& nodeId, UbseNodeGlobalState state)
{
    std::unique_lock<std::shared_mutex> lock(rwMutex);
    auto iter = nodeInfos.find(nodeId);
    if (iter == nodeInfos.end()) {
        UBSE_LOG_ERROR << "nodeId=" << nodeId
                       << " global node info not found, skip update global state=" << static_cast<uint32_t>(state);
        return UBSE_ERROR_NULLPTR;
    }

    if (iter->second.globalState == UbseNodeGlobalState::UBSE_NODE_GLOBAL_READY) {
        UBSE_LOG_INFO << "nodeId=" << nodeId
                      << " global state already ready, skip update state=" << static_cast<uint32_t>(state);
        return UBSE_OK;
    }

    UBSE_LOG_INFO << "nodeId=" << nodeId
                  << " update global state, current state=" << static_cast<uint32_t>(iter->second.globalState)
                  << ", update state=" << static_cast<uint32_t>(state);
    iter->second.globalState = state;
    return UBSE_OK;
}

void UbseNodeController::ResetAllGlobalStates()
{
    std::unique_lock<std::shared_mutex> lock(rwMutex);
    for (auto& [nodeId, info] : nodeInfos) {
        info.globalState = UbseNodeGlobalState::UBSE_NODE_GLOBAL_INIT;
    }
    UBSE_LOG_INFO << "all nodes globalState reset to GLOBAL_INIT";
}

uint32_t UbseNodeController::UpdateNodeInfoClusterState(const std::string& nodeId, UbseNodeClusterState state)
{
    UBSE_LOG_INFO << "nodeId=" << nodeId << " start to update cluster state=" << static_cast<uint32_t>(state);
    UbseResult ret = UBSE_OK;
    rwMutex.lock();
    if (nodeInfos.find(nodeId) == nodeInfos.end()) {
        if (state != UbseNodeClusterState::UBSE_NODE_FAULT) {
            rwMutex.unlock();
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " cluster node info not collect, skip update cluster state="
                           << static_cast<uint32_t>(state);
            return UBSE_ERROR_NULLPTR;
        }
        UbseNodeInfo faultNodeInfo{};
        (void)GenerateFaultUbseNode(nodeId, faultNodeInfo);
        nodeInfos[nodeId] = faultNodeInfo;
        faultUpdateTimes[nodeId] = std::chrono::steady_clock::now();
        rwMutex.unlock();
        ClearSubHealthCacheForNode(nodeId);
        UBSE_LOG_WARN << "nodeId=" << nodeId << " cluster node info not collect, set default item.";
        return UBSE_OK;
    }

    auto curState = nodeInfos[nodeId].clusterState;
    if (curState == UbseNodeClusterState::UBSE_NODE_FAULT && state != UbseNodeClusterState::UBSE_NODE_FAULT) {
        auto iter = faultUpdateTimes.find(nodeId);
        if (iter != faultUpdateTimes.end()) {
            auto elapsed =
                std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - iter->second)
                    .count();
            if (elapsed < FAULT_STATE_PROTECT_SECONDS) {
                rwMutex.unlock();
                UBSE_LOG_WARN << "nodeId=" << nodeId << " is in fault protect period, skip update cluster state="
                              << static_cast<uint32_t>(state) << ", elapsed=" << elapsed << "s";
                return UBSE_OK;
            }
        }
    }

    if (!CanUpdateNodeClusterState(curState, state)) {
        rwMutex.unlock();
        UBSE_LOG_ERROR << "nodeId=" << nodeId
                       << " can not cluster local state, current state=" << static_cast<uint32_t>(curState)
                       << ", update state=" << static_cast<uint32_t>(state);
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "nodeId=" << nodeId << " update cluster state, current state=" << static_cast<uint32_t>(curState)
                  << ", update state=" << static_cast<uint32_t>(state);

    if (curState != UbseNodeClusterState::UBSE_NODE_FAULT && state == UbseNodeClusterState::UBSE_NODE_FAULT) {
        faultUpdateTimes[nodeId] = std::chrono::steady_clock::now();
    } else if (state != UbseNodeClusterState::UBSE_NODE_FAULT) {
        faultUpdateTimes.erase(nodeId);
    }

    nodeInfos[nodeId].clusterState = state;
    rwMutex.unlock();

    if (state == UbseNodeClusterState::UBSE_NODE_UNKNOWN || state == UbseNodeClusterState::UBSE_NODE_FAULT) {
        ClearSubHealthCacheForNode(nodeId);
    } else if (state == UbseNodeClusterState::UBSE_NODE_WORKING) {
        TriggerSubHealthRefresh();
    }
    rwMutex.lock_shared();
    if (nodeInfos.find(nodeId) == nodeInfos.end()) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " not exists.";
        rwMutex.unlock_shared();
        return UBSE_OK;
    }

    auto nodeInfoCopy = nodeInfos[nodeId];
    rwMutex.unlock_shared();

    ret = ExecClusterStateHandler(nodeInfoCopy, clusterNotifyHandlers);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << nodeId << " update state=" << static_cast<uint32_t>(state)
                       << " exec handler failed, " << FormatRetCode(ret);
    }
    return ret;
}

void UbseNodeController::SetCurrentNodeId(const std::string& nodeId)
{
    currentNodeId = nodeId;
}

std::string UbseNodeController::GetCurrentNodeId()
{
    return currentNodeId;
}

void UbseNodeController::CleanAfterMasterSwitchRole()
{
    std::unique_lock<std::shared_mutex> lock(rwMutex);
    for (auto it = nodeInfos.begin(); it != nodeInfos.end();) {
        if (it->first != currentNodeId) {
            it = nodeInfos.erase(it);
        } else {
            it->second.globalState = UbseNodeGlobalState::UBSE_NODE_GLOBAL_INIT;
            ++it;
        }
    }
}

void UbseNodeController::CleanAfterGlobalMasterSwitchRole()
{
    std::unique_lock<std::shared_mutex> lock(rwMutex);
    for (auto it = nodeInfos.begin(); it != nodeInfos.end();) {
        if (it->second.groupId != nodeMgr::GetCurrentNode().groupId) {
            it = nodeInfos.erase(it);
        } else {
            it->second.globalState = UbseNodeGlobalState::UBSE_NODE_GLOBAL_INIT;
            ++it;
        }
    }
}

std::map<std::string, PhysicalLink> UbseNodeController::UbseGetDirConnectInfo()
{
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "election module not load";
        return {};
    }
    if (module->IsLeader()) {
        std::shared_lock<std::shared_mutex> lock(devDirMutex);
        PrintDevDirConnectInfo();
        return devDirConnectInfo;
    }
    // 远程获取
    Node masterNode{};
    auto ret = module->UbseGetMasterNode(masterNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get master node failed, " << FormatRetCode(ret);
        return {};
    }
    std::map<std::string, PhysicalLink> devDirConnectInfoRemote;
    ret = UbseGetDirConnectInfoFromRemote(masterNode.id, devDirConnectInfoRemote);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get devDirConnectInfo form master=" << masterNode.id << " failed, " << FormatRetCode(ret);
        return {};
    }
    return devDirConnectInfoRemote;
}

std::set<uint32_t> UbseNodeController::UbseGetAllDeployedNode()
{
    std::map<std::string, PhysicalLink> connectInfo = UbseNodeController::GetInstance().UbseGetDirConnectInfo();
    std::set<uint32_t> deployedNode;
    for (auto& connect : connectInfo) {
        deployedNode.insert(connect.second.slotId);
        deployedNode.insert(connect.second.peerSlotId);
    }
    std::ostringstream oss;
    std::for_each(deployedNode.begin(), deployedNode.end(),
                  [&oss](const uint32_t& slotId) { oss << std::to_string(slotId) << ","; });
    UBSE_LOG_INFO << "[node_controller] UbseGetAllDeployedNode slotId are:" << oss.str();
    return deployedNode;
}

UbseResult GetUbseIpAddrVecOffset(const std::vector<UbseIpAddr>& ipList, UbseSerialization& outStream)
{
    // 数组类型先存入长度
    outStream << (right_v<size_t>(ipList.size()));
    for (size_t i = 0; i < ipList.size(); i++) {
        UbseSerialization item;
        // 根据ip_list的元素拼装UbseIpAddr
        auto type = ipList[i].type;
        item << enum_v(type);
        std::vector<uint8_t> ipv6Vec{};
        std::vector<uint8_t> ipv4Vec{};
        if (type == UbseIpType::UBSE_IP_V4) {
            for (size_t j = 0; j < IPV4_LENGTH; j++) { // 4个字符存储ipv4地址
                ipv4Vec.push_back(ipList[i].ipv4.addr[j]);
            }
            item << ipv4Vec;
        } else {
            for (size_t j = 0; j < IPV6_LENGTH; j++) { // 16个字符存储ipv6地址
                ipv6Vec.push_back(ipList[i].ipv6.addr[j]);
            }
            item << ipv6Vec;
        }
        outStream << item;
        if (!outStream.Check()) {
            UBSE_LOG_ERROR << "Ubse serialize ip addr vec failed";
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

UbseResult GetUbseNumaInfoVecOffset(const std::unordered_map<UbseNumaLocation, UbseNumaInfo, UbseNumaLocation::Hash,
                                                             UbseNumaLocation::Equal>& numaInfos,
                                    UbseSerialization& outStream)
{
    outStream << (right_v<size_t>(numaInfos.size()));
    for (auto numa : numaInfos) {
        UbseSerialization item;
        item << numa.first.nodeId << numa.first.numaId << numa.second.socketId << numa.second.bindCore
             << numa.second.size << numa.second.freeSize << numa.second.nr_hugepages_2M << numa.second.free_hugepages_2M
             << numa.second.timestamp << numa.second.nr_hugepages_1G << numa.second.free_hugepages_1G
             << numa.second.mempool_total << numa.second.mempool_used << numa.second.mempool_available_cleared
             << numa.second.mempool_available_uncleared << numa.second.nr_hugepages_512M
             << numa.second.free_hugepages_512M;
        outStream << item;
        if (!outStream.Check()) {
            UBSE_LOG_ERROR << "Ubse serialize numa info vec failed";
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

UbseResult GetUbseCpuInfoOffset(
    std::unordered_map<UbseCpuLocation, UbseCpuInfo, UbseCpuLocation::Hash, UbseCpuLocation::Equal> cpuInfos,
    UbseSerialization& outStream)
{
    outStream << (right_v<size_t>(cpuInfos.size()));
    for (auto cpu : cpuInfos) {
        UbseSerialization item;
        item << cpu.first.nodeId << cpu.first.chipId << cpu.second.slotId << cpu.second.socketId
             << cpu.second.primaryEid << cpu.second.chipId << cpu.second.cardId << cpu.second.eid << cpu.second.guid
             << cpu.second.busNodeCna;
        item << (right_v<size_t>(cpu.second.portInfos.size()));
        for (auto portInfo : cpu.second.portInfos) {
            UbseSerialization portItem;
            portItem << portInfo.first << portInfo.second.ifName << portInfo.second.portRole
                     << enum_v(portInfo.second.portStatus) << portInfo.second.portCna << portInfo.second.urmaEid
                     << portInfo.second.remoteSlotId << portInfo.second.remoteChipId << portInfo.second.remoteCardId
                     << portInfo.second.remoteIfName << portInfo.second.remotePortId;
            item << portItem;
        }
        outStream << item;
        if (!outStream.Check()) {
            UBSE_LOG_ERROR << "Ubse serialize cpu info vec failed";
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

UbseResult GetUbseNodeInfoOffset(UbseNodeInfo info, UbseSerialization& outStream)
{
    outStream << info.nodeId << info.slotId << info.bondingEid << info.hostName << info.comIp << info.guid
              << enum_v(info.allocator) << info.pmdMapping << info.blockSize << info.groupId;

    UbseSerialization ipAddrOffset;
    auto ret = GetUbseIpAddrVecOffset(info.ipList, ipAddrOffset);

    UbseSerialization numaInfoOffset;
    ret |= GetUbseNumaInfoVecOffset(info.numaInfos, numaInfoOffset);

    UbseSerialization cpuInfoOffset;
    ret |= GetUbseCpuInfoOffset(info.cpuInfos, cpuInfoOffset);

    outStream << ipAddrOffset << numaInfoOffset << cpuInfoOffset << enum_v(info.localState) << enum_v(info.clusterState)
              << enum_v(info.globalState) << info.eventMessage << info.isLender << enum_v(info.sysSentryState)
              << enum_v(info.obmmState);

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Ubse serialize node info data failed, " << FormatRetCode(ret);
        return ret;
    }
    if (!outStream.Check()) {
        UBSE_LOG_ERROR << "Ubse serialize node info check failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t SerializeUbseNode(UbseNodeInfo info, uint8_t*& buffer, size_t& size)
{
    UbseSerialization outStream;
    auto ret = GetUbseNodeInfoOffset(info, outStream);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Ubse serialize node info data failed";
        return ret;
    }
    size = outStream.GetLength();
    buffer = outStream.GetBuffer(true);
    return UBSE_OK;
}

uint32_t SerializeUbseNodeList(std::vector<UbseNodeInfo> infos, uint8_t*& buffer, size_t& size)
{
    UbseResult ret = UBSE_OK;
    UbseSerialization outStream;
    outStream << (right_v<size_t>(infos.size()));
    for (size_t i = 0; i < infos.size(); i++) {
        UbseSerialization item;
        ret = GetUbseNodeInfoOffset(infos[i], item);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Ubse serialize node info list[" << i << "]  failed, " << FormatRetCode(ret);
            return ret;
        }
        outStream << item;
        if (!outStream.Check()) {
            UBSE_LOG_ERROR << "Ubse serialize node info list[" << i << "]  failed, " << FormatRetCode(ret);
            return UBSE_ERROR;
        }
    }
    size = outStream.GetLength();
    buffer = outStream.GetBuffer(true);
    return UBSE_OK;
}

uint32_t SerializeDevDirConnectInfo(std::map<std::string, PhysicalLink>& devDirConnectInfo, uint8_t*& buffer,
                                    size_t& size)
{
    UbseResult ret = UBSE_OK;
    UbseSerialization outStream;
    outStream << (right_v<size_t>(devDirConnectInfo.size()));
    for (auto& physicalLinkInfo : devDirConnectInfo) {
        std::string linkId = physicalLinkInfo.first;
        PhysicalLink physicalLink = physicalLinkInfo.second;
        outStream << linkId << physicalLink.slotId << physicalLink.chipId << physicalLink.portId
                  << physicalLink.interfaceName << physicalLink.peerSlotId << physicalLink.peerChipId
                  << physicalLink.peerPortId << physicalLink.peerInterfaceName << enum_v(physicalLink.linkStatus);
        if (!outStream.Check()) {
            UBSE_LOG_ERROR << "Ubse serialize devDirConnectInfo failed, " << FormatRetCode(ret);
            return UBSE_ERROR;
        }
    }
    size = outStream.GetLength();
    buffer = outStream.GetBuffer(true);
    return UBSE_OK;
}

// 通用IP数据验证和复制函数
template <typename IpAddrType>
UbseResult ValidateAndCopyIpData(const std::vector<uint8_t>& srcData, IpAddrType& destAddr, size_t expectedSize,
                                 const char* ipTypeName)
{
    if (srcData.size() != expectedSize) {
        UBSE_LOG_ERROR << ipTypeName << " address size mismatch. Expected: " << expectedSize
                       << ", Got: " << srcData.size();
        return UBSE_ERROR;
    }

    std::copy(srcData.begin(), srcData.end(), destAddr.addr);
    return UBSE_OK;
}

UbseResult SetUbseIpAddr(UbseDeSerialization& inStream, UbseIpAddr& addr)
{
    std::vector<uint8_t> ipData{};

    // 读取IP类型
    inStream >> enum_v(addr.type);
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize IP type.";
        return UBSE_ERROR;
    }

    // 读取IP数据
    inStream >> ipData;
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize IP address data.";
        return UBSE_ERROR;
    }

    // 根据类型验证和复制数据
    switch (addr.type) {
        case UbseIpType::UBSE_IP_V4:
            return ValidateAndCopyIpData(ipData, addr.ipv4, IPV4_LENGTH, "IPv4");

        case UbseIpType::UBSE_IP_V6:
            return ValidateAndCopyIpData(ipData, addr.ipv6, IPV6_LENGTH, "IPv6");

        default:
            UBSE_LOG_ERROR << "Unsupported IP type: " << static_cast<int>(addr.type);
            return UBSE_ERROR;
    }
}

UbseResult ParseIpList(UbseDeSerialization& inStream, UbseNodeInfo& info)
{
    // 数组类型simpo先获取元素个数
    size_t itemNum = 0;
    inStream >> itemNum;
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "Ubse deserialize itemNum failed";
        return UBSE_ERROR;
    }
    if (itemNum > MAX_IP_ADDR_NUM) {
        UBSE_LOG_ERROR << "Ubse deserialize ip addr num exceed limit, itemNum=" << itemNum;
        return UBSE_ERROR_INVAL;
    }

    UbseResult ret = UBSE_OK;
    std::vector<UbseIpAddr> ipAddrVec{};
    for (size_t i = 0; i < itemNum; i++) {
        // 设置子元素
        UbseDeSerialization item;
        inStream >> item;
        UbseIpAddr ipAddr{};
        ret = SetUbseIpAddr(item, ipAddr);
        if (!inStream.Check() || ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Ubse deserialize ubse ip addr vec failed, " << FormatRetCode(ret);
            return ret;
        }
        ipAddrVec.push_back(ipAddr);
    }
    info.ipList = ipAddrVec;
    return UBSE_OK;
}

uint32_t ParseNumaInfo(UbseDeSerialization& inStream, UbseNodeInfo& nodeInfo)
{
    size_t itemNum = 0;
    inStream >> itemNum;
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "Ubse deserialize itemNum failed";
        return UBSE_ERROR;
    }
    for (size_t i = 0; i < itemNum; i++) {
        UbseDeSerialization item;
        inStream >> item;
        UbseNumaLocation location{};
        item >> location.nodeId >> location.numaId;
        if (!item.Check()) {
            UBSE_LOG_ERROR << "Ubse deserialize numa location failed";
            return UBSE_ERROR;
        }
        UbseNumaInfo info{};
        info.location = location;
        item >> info.socketId >> info.bindCore >> info.size >> info.freeSize >> info.nr_hugepages_2M >>
            info.free_hugepages_2M >> info.timestamp >> info.nr_hugepages_1G >> info.free_hugepages_1G >>
            info.mempool_total >> info.mempool_used >> info.mempool_available_cleared >>
            info.mempool_available_uncleared >> info.nr_hugepages_512M >> info.free_hugepages_512M;
        if (!item.Check()) {
            UBSE_LOG_ERROR << "Ubse deserialize numa info vec failed";
            return UBSE_ERROR;
        }
        nodeInfo.numaInfos[location] = info;
    }
    return UBSE_OK;
}

uint32_t ParseCpuInfo(UbseDeSerialization& inStream, UbseNodeInfo& nodeInfo)
{
    errno_t cpyRet = 0;
    size_t itemNum = 0;
    inStream >> itemNum;
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "Ubse deserialize itemNum failed";
        return UBSE_ERROR;
    }
    for (size_t i = 0; i < itemNum; i++) {
        UbseDeSerialization item;
        inStream >> item;
        UbseCpuLocation location{};
        item >> location.nodeId >> location.chipId;
        UbseCpuInfo info{};
        item >> info.slotId;
        item >> info.socketId; // key里面为lcne的chipid，值里面有socketid也有lcne的chipid
        std::string primaryEid;
        item >> primaryEid;
        cpyRet = strcpy_s(info.primaryEid, sizeof(info.primaryEid), primaryEid.c_str());
        if (cpyRet != EOK) {
            UBSE_LOG_ERROR << "cpy primary eid failed, ErrorCode=" << cpyRet;
            return cpyRet;
        }
        item >> info.chipId >> info.cardId >> info.eid >> info.guid >> info.busNodeCna;
        size_t portNum = 0;
        item >> portNum;
        if (!inStream.Check()) {
            UBSE_LOG_ERROR << "Ubse deserialize cpu info vec failed";
            return UBSE_ERROR;
        }
        for (size_t j = 0; j < portNum; j++) {
            UbseDeSerialization port;
            item >> port;
            UbsePortInfo portInfo{};
            port >> portInfo.portId >> portInfo.ifName >> portInfo.portRole >> enum_v(portInfo.portStatus) >>
                portInfo.portCna >> portInfo.urmaEid >> portInfo.remoteSlotId >> portInfo.remoteChipId >>
                portInfo.remoteCardId >> portInfo.remoteIfName >> portInfo.remotePortId;
            if (!inStream.Check()) {
                UBSE_LOG_ERROR << "Ubse deserialize portInfo failed";
                return UBSE_ERROR;
            }
            info.portInfos[portInfo.portId] = portInfo;
        }
        nodeInfo.cpuInfos[location] = info;
    }
    return UBSE_OK;
}

uint32_t ParseNodeInfo(UbseNodeInfo& info, UbseDeSerialization& inStream)
{
    inStream >> info.nodeId >> info.slotId;
    std::string bondingEid;
    inStream >> bondingEid;
    auto cpyRet = strcpy_s(info.bondingEid, sizeof(info.bondingEid), bondingEid.c_str());
    if (cpyRet != EOK) {
        UBSE_LOG_ERROR << "cpy bonding eid failed, ErrorCode=" << cpyRet;
        return cpyRet;
    }
    inStream >> info.hostName;
    inStream >> info.comIp;
    inStream >> info.guid;
    inStream >> enum_v(info.allocator);
    inStream >> info.pmdMapping;
    inStream >> info.blockSize;
    inStream >> info.groupId;
    UbseDeSerialization ipAddrOffset;
    UbseDeSerialization numaInfoOffset;
    UbseDeSerialization cpuInfoOffset;
    inStream >> ipAddrOffset >> numaInfoOffset >> cpuInfoOffset;
    inStream >> enum_v(info.localState) >> enum_v(info.clusterState) >> enum_v(info.globalState);
    inStream >> info.eventMessage >> info.isLender >> enum_v(info.sysSentryState) >> enum_v(info.obmmState);
    auto ret = ParseIpList(ipAddrOffset, info);
    ret |= ParseNumaInfo(numaInfoOffset, info);
    ret |= ParseCpuInfo(cpuInfoOffset, info);
    return ret;
}

uint32_t DeSerializeUbseNode(UbseNodeInfo& info, uint8_t* buffer, size_t size)
{
    UbseDeSerialization inStream(buffer, size);
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "Deserialize mem node info failed";
        return UBSE_ERROR;
    }
    return ParseNodeInfo(info, inStream);
}

uint32_t DeSerializeUbseNodeList(std::vector<UbseNodeInfo>& infos, uint8_t* buffer, size_t size)
{
    UbseResult ret = UBSE_OK;
    UbseDeSerialization inStream(buffer, size);
    size_t num = 0;
    inStream >> num;
    infos.reserve(num);
    if (!inStream.Check()) {
        return UBSE_ERROR;
    }
    for (size_t i = 0; i < num; i++) {
        UbseDeSerialization item;
        inStream >> item;
        UbseNodeInfo info{};
        ret = ParseNodeInfo(info, item);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Ubse deserialize node info list[" << i << "]  failed, " << FormatRetCode(ret);
            return ret;
        }
        infos.push_back(info);
        if (!inStream.Check()) {
            UBSE_LOG_ERROR << "Ubse deserialize node info list[" << i << "]  failed, " << FormatRetCode(ret);
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

uint32_t DeSerializeDevDirConnectInfo(std::map<std::string, PhysicalLink>& devDirConnectInfo, uint8_t* buffer,
                                      size_t size)
{
    UbseResult ret = UBSE_OK;
    UbseDeSerialization inStream(buffer, size);
    // map大小
    size_t num = 0;
    inStream >> num;
    if (!inStream.Check()) {
        return UBSE_ERROR;
    }
    for (size_t i = 0; i < num; i++) {
        std::string linkId;
        inStream >> linkId;
        if (!inStream.Check()) {
            return UBSE_ERROR;
        }
        auto& physicalLink = devDirConnectInfo[linkId];
        inStream >> physicalLink.slotId >> physicalLink.chipId >> physicalLink.portId >> physicalLink.interfaceName >>
            physicalLink.peerSlotId >> physicalLink.peerChipId >> physicalLink.peerPortId >>
            physicalLink.peerInterfaceName;
        inStream >> enum_v(physicalLink.linkStatus);
        if (!inStream.Check()) {
            UBSE_LOG_ERROR << "Ubse deserialize node info list[" << i << "]  failed, " << FormatRetCode(ret);
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

uint32_t UbseGetNodeInfos(std::vector<NodeInfo>& nodeInfos)
{
    auto nodeInfoMap = UbseNodeController::GetInstance().GetAllNodes();
    if (nodeInfoMap.empty()) {
        return UBSE_ERROR;
    }
    nodeInfos.reserve(nodeInfoMap.size());
    for (const auto& [_, ubseNodeInfo] : nodeInfoMap) {
        NodeInfo nodeInfo{};
        nodeInfo.nodeId = ubseNodeInfo.nodeId;
        nodeInfo.hostName = ubseNodeInfo.hostName;
        for (const auto& ip : ubseNodeInfo.ipList) {
            if (ip.type == UbseIpType::UBSE_IP_V4) {
                nodeInfo.ipList.emplace_back(UbseNetUtil::Ipv4ArrToString(ip.ipv4.addr));
            } else {
                nodeInfo.ipList.emplace_back(UbseNetUtil::Ipv6ArrToString(ip.ipv6.addr));
            }
        }
        nodeInfos.emplace_back(nodeInfo);
    }
    return UBSE_OK;
}

uint32_t UbseNodeGetNodeIdByHostname(const std::string& hostname, std::string& nodeId)
{
    auto nodeInfoMap = UbseNodeController::GetInstance().GetAllNodes();
    for (const auto& [_, nodeInfo] : nodeInfoMap) {
        if (nodeInfo.hostName == hostname) {
            nodeId = nodeInfo.nodeId;
            return UBSE_OK;
        }
    }
    return UBSE_ERROR;
}

// 辅助函数：解析字符串IP为UbseIpAddr结构
bool parseIpString(const std::string& ipStr, UbseIpAddr& out)
{
    // 尝试解析为IPv4
    in_addr ipv4{};
    if (inet_pton(AF_INET, ipStr.c_str(), &ipv4) == 1) {
        out.type = UbseIpType::UBSE_IP_V4;
        auto err = memcpy_s(out.ipv4.addr, sizeof(out.ipv4.addr), &ipv4, NO_4); // ipv4
        if (err != EOK) {
            UBSE_LOG_ERROR << "Mem copy failed, errno_t=" << err << ".";
            return false;
        }
        return true;
    }

    // 尝试解析为IPv6
    in6_addr ipv6{};
    if (inet_pton(AF_INET6, ipStr.c_str(), &ipv6) == 1) {
        out.type = UbseIpType::UBSE_IP_V6;
        auto err = memcpy_s(out.ipv6.addr, sizeof(out.ipv6.addr), &ipv6, NO_16);
        if (err != EOK) {
            UBSE_LOG_ERROR << "Mem copy failed, errno_t=" << err << ".";
            return false;
        }
        return true;
    }
    return false; // 无效IP格式
}

bool isIpInList(const std::string& ipStr, const std::vector<UbseIpAddr>& ipList)
{
    UbseIpAddr target{};
    if (!parseIpString(ipStr, target)) {
        return false; // 无效IP字符串，直接返回false
    }
    for (const auto& ip : ipList) {
        // 类型不匹配则跳过
        if (ip.type != target.type) {
            continue;
        }

        // 根据类型比较二进制数据
        if (ip.type == UbseIpType::UBSE_IP_V4) {
            if (std::memcmp(ip.ipv4.addr, target.ipv4.addr, NO_4) == 0) {
                return true;
            }
        } else { // IPv6
            if (std::memcmp(ip.ipv6.addr, target.ipv6.addr, NO_16) == 0) {
                return true;
            }
        }
    }
    return false;
}

bool CheckGuid(const std::string& value, const std::pair<const std::string, UbseNodeInfo>& nodeInfo)
{
    for (const auto& nodeData : nodeInfo.second.cpuInfos) {
        if (value == nodeData.second.guid) {
            return true;
        }
    }
    return false;
}

uint32_t UbseGetNodeIdByAttrValue(const NodeAttr& attr, const std::string& value, uint32_t& nodeId)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    for (const auto& nodeInfo : nodeInfos) {
        // 节点信息
        if (attr == NodeAttr::Ip && isIpInList(value, nodeInfo.second.ipList)) {
            nodeId = nodeInfo.second.slotId;
            return UBSE_OK;
        }
        if (attr == NodeAttr::hostName && value == nodeInfo.second.hostName) {
            nodeId = nodeInfo.second.slotId;
            return UBSE_OK;
        }
        if (attr == NodeAttr::guid && CheckGuid(value, nodeInfo)) {
            nodeId = nodeInfo.second.slotId;
            return UBSE_OK;
        }
    }
    UBSE_LOG_ERROR << "Failed to get node id. " << FormatRetCode(UBSE_ERROR);
    nodeId = 0xFFFFFFFF;
    return UBSE_ERROR;
}

uint32_t UbseNodeGetLinkUpNodes(std::vector<UbseRoleInfo>& roleInfos)
{
    return UbseGetAllNodeInfos(roleInfos);
}

void UbseNodeController::RegisterHostBonding()
{
    isHostUrmaDevOccupied = true;
}
bool UbseNodeController::IsHostBondingRegistered() const
{
    return isHostUrmaDevOccupied;
}

UbseResult UbseNodeController::GetPlanningHostBondingByNodeId(const std::string& nodeId,
                                                              std::vector<UbseUrmaUvsNodeInfo>& hostUrmaInfos)
{
    return UbseNodeComUrmaCollector::GetInstance().GetPlanningHostBondingByNodeId(nodeId, hostUrmaInfos);
}
} // namespace ubse::nodeController