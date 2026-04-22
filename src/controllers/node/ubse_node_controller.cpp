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
#include <queue>
#include <regex>
#include <set>
#include <src/framework/serde/ubse_serial_util.h>

#include "adapter_plugins/mti/ubse_mti_def.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "securec.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_net_util.h"
#include "ubse_node.h"
#include "ubse_node_controller_agent.h"
#include "ubse_node_controller_collector.h"
#include "ubse_node_controller_util.h"
#include "ubse_serial_util.h"
#include "ubse_str_util.h"

namespace ubse::nodeController {
using namespace ubse::context;
using namespace ubse::election;
using namespace ubse::serial;
using namespace ubse::config;
using namespace ubse::adapter_plugins::mti;
UBSE_DEFINE_THIS_MODULE("ubse");

const uint32_t LOCAL_HANDLER_RETRY_DURATION = 2;
const uint32_t IPV4_LENGTH = 4;
const uint32_t IPV6_LENGTH = 16;
const size_t MAX_HOSTNAME_LENGTH = 63;

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
    for (const auto& node : ubseNodeInfos) {
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
    if (module->IsLeader()) {
        rwMutex.lock_shared();
        auto nodes = nodeInfos;
        rwMutex.unlock_shared();
        return nodes;
    }
    Node masterNode{};
    auto ret = module->UbseGetMasterNode(masterNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get master node failed, " << FormatRetCode(ret);
        return {};
    }
    std::vector<UbseNodeInfo> infos{};
    ret = GetAllNodeInfoFromRemote(masterNode.id, infos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get all node from master=" << masterNode.id << " failed, " << FormatRetCode(ret);
        return {};
    }
    std::unordered_map<std::string, UbseNodeInfo> maps{};
    for (const auto& info : infos) {
        maps[info.nodeId] = info;
    }
    return maps;
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

UbseNodeInfo UbseNodeController::GetNodeById(const std::string &nodeId)
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

uint32_t UbseNodeController::GetLocalEidBySocket(const uint32_t &socketId, uint32_t &eid)
{
    auto node = GetNodeById(currentNodeId);
    for (const auto &[_, cpuInfo] : node.cpuInfos) {
        if (cpuInfo.socketId == socketId) {
            UBSE_LOG_INFO << "nodeId=" << currentNodeId << ", socketId=" << socketId << " , eid=" << cpuInfo.eid;
            return ConvertStrToUint32(cpuInfo.eid, eid, NO_16);
        }
    }
    UBSE_LOG_ERROR << "nodeId=" << currentNodeId << ", socketId=" << socketId << " not found";
    return UBSE_ERROR;
}

uint32_t UbseNodeController::GetEid(const std::string &nodeId, const uint32_t &socketId, uint32_t &eid)
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
    for (const auto &[_, cpuInfo] : node.cpuInfos) {
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

UbseResult CheckGroupList(std::vector<std::vector<std::string>> &groupListVec, UbseMemGroupNodeList &groupList)
{
    std::unordered_set<std::string> globalSeen;
    std::unordered_map<std::string, UbseNodeInfo> nodesMap = UbseNodeController::GetInstance().GetAllNodes();
    std::unordered_map<std::string, UbseNodeInfo> hostnameMap{};
    for (auto &kv : nodesMap) {
        hostnameMap[kv.second.hostName] = kv.second;
    }
    size_t totalCount =
        std::accumulate(groupListVec.begin(), groupListVec.end(), size_t(0),
                        [](size_t sum, const std::vector<std::string> &group) { return sum + group.size(); });
    if (totalCount != nodesMap.size()) {
        UBSE_LOG_ERROR << "the number of group list hosts does not match lcne static node number.";
        return UBSE_ERROR_CONF_INVALID;
    }
    for (auto &group : groupListVec) {
        std::vector<UbseNodeInfo> groupNodeInfo;
        for (auto &hostname : group) {
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

uint32_t UbseNodeController::GetMemGroupNodeList(UbseMemGroupNodeList &groupList)
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
    for (auto &groupConf : groupListConfVec) {
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

UbseResult CheckProviderList(std::vector<std::string> &providerListConfVec, UbseMemProviderNodeList &providerList)
{
    std::unordered_set<std::string> globalSeen;
    std::unordered_map<std::string, UbseNodeInfo> nodesMap = UbseNodeController::GetInstance().GetAllNodes();
    std::unordered_map<std::string, UbseNodeInfo> hostnameMap{};
    for (auto &kv : nodesMap) {
        hostnameMap[kv.second.hostName] = kv.second;
    }
    // 总个数不大于静态规划节点数
    if (providerListConfVec.size() >= nodesMap.size()) {
        UBSE_LOG_ERROR << "the number of provider is more than lcne static node number.";
        return UBSE_ERROR_CONF_INVALID;
    }
    for (auto &hostname : providerListConfVec) {
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

uint32_t UbseNodeController::GetMemProviderNodeList(UbseMemProviderNodeList &providerList)
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
uint32_t UbseNodeController::RegLocalStateNotifyHandler(const UbseLocalStateNotifyHandler &handler)
{
    std::unique_lock<std::shared_mutex> lock(rwMutex);
    UBSE_LOG_INFO << "register node local state";
    localNotifyHandlers.push_back(handler);
    return UBSE_OK;
}
// 注册中心侧节点状态变更回调
uint32_t UbseNodeController::RegClusterStateNotifyHandler(const UbseClusterStateNotifyHandler &handler)
{
    std::unique_lock<std::shared_mutex> lock(rwMutex);
    clusterNotifyHandlers.push_back(handler);
    return UBSE_OK;
}

void ExecLocalStateHandler(const UbseNodeInfo &nodeInfo, const std::vector<UbseLocalStateNotifyHandler> &handlers)
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

UbseResult ExecClusterStateHandler(const UbseNodeInfo &nodeInfo,
                                   const std::vector<UbseClusterStateNotifyHandler> &handlers)
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

    UBSE_LOG_INFO << "ExecClusterStateHandler start, nodeId=" << nodeInfo.nodeId
                  << ", clusterState=" << static_cast<uint32_t>(nodeInfo.clusterState)
                  << ", handlers count=" << handlers.size();
    int failedCount = 0;
    for (auto handler : handlers) {
        if (handler == nullptr) {
            continue;
        }
        UbseResult handlerRet = handler(nodeInfo);
        if (handlerRet != UBSE_OK) {
            UBSE_LOG_ERROR << "nodeId=" << nodeInfo.nodeId
                           << " handler failed, ret=" << FormatRetCode(handlerRet);
            failedCount++;
        }
        ret |= handlerRet;
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << nodeInfo.nodeId
                       << " update state=" << static_cast<uint32_t>(nodeInfo.clusterState)
                       << " exec handler failed, total ret=" << FormatRetCode(ret)
                       << ", failedCount=" << failedCount << "/" << handlers.size();
    } else {
        UBSE_LOG_INFO << "nodeId=" << nodeInfo.nodeId
                      << " update state=" << static_cast<uint32_t>(nodeInfo.clusterState)
                      << " exec all handlers success, total=" << handlers.size();
    }
    return ret;
}

uint32_t UbseNodeController::UpdateNodeInfo(const std::string &nodeId, UbseNodeInfo info)
{
    UbseResult ret = UBSE_OK;
    rwMutex.lock();
    if (nodeInfos.find(nodeId) == nodeInfos.end()) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " first add, update node info, current nodeId=" << GetCurrentNodeId();
        info.clusterState = UbseNodeClusterState::UBSE_NODE_INIT;
        nodeInfos[nodeId] = info;
        // 使用numaInfos更新拓扑数据中的本端信息
        UbseSocketIdChange(nodeId);
        rwMutex.unlock();
        if (nodeId == currentNodeId) {
            ExecLocalStateHandler(info, localNotifyHandlers);
        }
        ret = ExecClusterStateHandler(info, clusterNotifyHandlers);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "nodeId=" << nodeId << " first add notify cluster state failed, " << FormatRetCode(ret);
        } else {
            UBSE_LOG_INFO << "nodeId=" << nodeId << " first add notify cluster state success";
        }
        return ret;
    }
    auto &existing = nodeInfos[nodeId];
    info.clusterState = existing.clusterState;
    info.localState = existing.localState;
    // 保持原有的GUID（如果新info没有GUID，使用原有的）
    if (info.guid.empty() && !existing.guid.empty()) {
        info.guid = existing.guid;
    }
    existing = std::move(info);
    // 使用numaInfos更新拓扑数据中的本端信息
    UbseSocketIdChange(nodeId);
    rwMutex.unlock();
    return UBSE_OK;
}

void LogOnSocketIdMismatch(const std::set<uint32_t> &lcneChipIdSet, const std::set<uint32_t> &osSocketIdSet)
{
    if (lcneChipIdSet.size() != osSocketIdSet.size()) {
        UBSE_LOG_WARN << "Mismatch in sockets. lcne reports " << lcneChipIdSet.size()
                      << " CPUs, while OS reports " << osSocketIdSet.size() << " sockets.";
        std::ostringstream oss;
        oss << "os_socket_ids include";
        std::for_each(osSocketIdSet.begin(), osSocketIdSet.end(),
                      [&oss](const uint32_t &osSocketId) { oss << " " << osSocketId; });
        oss << ". lcne_chip_ids include";
        std::for_each(lcneChipIdSet.begin(), lcneChipIdSet.end(),
                      [&oss](const uint32_t &lcneChipId) { oss << " " << lcneChipId; });
        UBSE_LOG_WARN << oss.str();
    }
}

void UbseNodeController::UbseSocketIdChange(const std::string &nodeId)
{
    std::unordered_map<uint32_t, uint32_t> socketIdMap;
    std::set<uint32_t> osSocketIdSet; // 本节点OS-socketId
    std::set<uint32_t> lcneChipIdSet; // 本节点chipId
    // 排序全量SocketID
    for (auto &numa : nodeInfos[nodeId].numaInfos) {
        osSocketIdSet.insert(numa.second.socketId);
    }
    // 排序全量chipID
    for (auto &cpu : nodeInfos[nodeId].cpuInfos) {
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
    for (auto &cpu : nodeInfos[nodeId].cpuInfos) {
        uint32_t chipId;
        ubse::utils::ConvertStrToUint32(cpu.second.chipId, chipId);
        auto it = socketIdMap.find(chipId);
        if (it != socketIdMap.end()) {
            cpu.second.socketId = it->second;
        }
    }
}

std::string CreateLinkIdAndPhysicalLink(const LinkInfo &linkInfo, PhysicalLink &physicalLink)
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
            physicalLink = {slotId, socketId, portId, linkInfo.interfaceName,
                            peerSlotId, peerSocketId, peerPortId, linkInfo.peerInterfaceName};
            return linkInfo.slotId + "/" + linkInfo.socketId + "/" + linkInfo.portId + "-"
                   + linkInfo.peerSlotId + "/" + linkInfo.peerSocketId + "/" + linkInfo.peerPortId;
        } else {
            physicalLink = {peerSlotId, peerSocketId, peerPortId,
                            linkInfo.peerInterfaceName,
                            slotId, socketId, portId,
                            linkInfo.interfaceName};
            return linkInfo.peerSlotId + "/" + linkInfo.peerSocketId + "/" + linkInfo.peerPortId + "-"
                   + linkInfo.slotId + "/" + linkInfo.socketId + "/" + linkInfo.portId;
        }
    } catch (const std::exception &e) {
        UBSE_LOG_WARN << "LCNE provides data that cannot be converted to uint32, with the specific data being: "
                       << "slotId=" << linkInfo.slotId << ", socketId=" << linkInfo.socketId << ", portId="
                       << linkInfo.portId << ", peerSlotId=" << linkInfo.peerSlotId << ", peerSocketId="
                       << linkInfo.peerSocketId << ", peerPortId=" << linkInfo.peerPortId;
    }
    return "ERROR-LINK";
}

void UbseNodeController::UpdateConnect(PhysicalLink &physicalLink, std::string &linkId)
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
    for (auto &connect : devDirConnectInfo) {
        oss << "LinkId=" << connect.first << ", slotId=" << connect.second.slotId << ", chipId="
            << connect.second.chipId << ", portId=" << connect.second.portId << ", peerSlotId="
            << connect.second.peerSlotId << ", peerChipId=" << connect.second.peerChipId << ", peerPortId="
            << connect.second.peerPortId;
        if (connect.second.linkStatus == LinkStatus::conflict) {
            oss << ", status conflict";
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
    for (auto &nodeInfo : localNodeInfos) {
        for (auto &topoInfo : nodeInfo.second.cpuInfos) {
            CreateAndUpdateInfo(topoInfo);
        }
    }
}

void UbseNodeController::CreateAndUpdateInfo(std::pair<const UbseCpuLocation, UbseCpuInfo> topoInfo)
{
    std::string slotId = topoInfo.first.nodeId;
    std::string chipId = topoInfo.second.chipId;

    for (auto &portInfo : topoInfo.second.portInfos) {
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
        if (remoteSlotId == "-" || remoteChipId == "-" || remotePortId == "-") {
            continue;
        }
        // 生成linkid 和 要填入的数据
        LinkInfo linkInfo{slotId, chipId, portId, interfaceName, remoteSlotId,
                          remoteChipId, remotePortId, peerInterfaceName};
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
    rwMutex.unlock();
    // local 状态的变更，restore需要重试直到平滑成功;
    ExecLocalStateHandler(nodeInfos[currentNodeId], localNotifyHandlers);
    UBSE_LOG_INFO << "local node update local state to " << static_cast<uint32_t>(state);
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

uint32_t GenerateFaultUbseNode(const std::string &nodeId, UbseNodeInfo &faultNodeInfo)
{
    faultNodeInfo.nodeId = nodeId;
    auto ret = ConvertStrToUint32(faultNodeInfo.nodeId, faultNodeInfo.slotId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "generate fault nodeId=" << nodeId << ", convert slot id failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "generate fault nodeId="<<nodeId<<", slotId="<<faultNodeInfo.slotId;
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
    for (auto &node : ubseNodeInfos) {
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

uint32_t UbseNodeController::UpdateNodeInfoClusterState(const std::string &nodeId, UbseNodeClusterState state)
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
        rwMutex.unlock();
        UBSE_LOG_WARN << "nodeId=" << nodeId << " cluster node info not collect, set default item.";
        return UBSE_OK;
    }
    if (!CanUpdateNodeClusterState(nodeInfos[nodeId].clusterState, state)) {
        rwMutex.unlock();
        UBSE_LOG_ERROR << "nodeId=" << nodeId << " can not cluster local state, current state="
                       << static_cast<uint32_t>(nodeInfos[nodeId].clusterState)
                       << ", update state=" << static_cast<uint32_t>(state);
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "nodeId=" << nodeId
                  << " update cluster state, current state=" << static_cast<uint32_t>(nodeInfos[nodeId].clusterState)
                  << ", update state=" << static_cast<uint32_t>(state);
    nodeInfos[nodeId].clusterState = state;
    rwMutex.unlock();
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

void UbseNodeController::SetCurrentNodeId(const std::string &nodeId)
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
    for (auto &connect : connectInfo) {
        deployedNode.insert(connect.second.slotId);
        deployedNode.insert(connect.second.peerSlotId);
    }
    std::ostringstream oss;
    std::for_each(deployedNode.begin(), deployedNode.end(),
                  [&oss](const uint32_t &slotId) { oss << std::to_string(slotId) << ","; });
    UBSE_LOG_INFO << "[node_controller] UbseGetAllDeployedNode slotId are:" << oss.str();
    return deployedNode;
}

UbseResult GetUbseIpAddrVecOffset(const std::vector<UbseIpAddr> &ipList, UbseSerialization &outStream)
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
                                                             UbseNumaLocation::Equal> &numaInfos,
                                    UbseSerialization &outStream)
{
    outStream << (right_v<size_t>(numaInfos.size()));
    for (auto numa : numaInfos) {
        UbseSerialization item;
        item << numa.first.nodeId << numa.first.numaId << numa.second.socketId << numa.second.bindCore
             << numa.second.size << numa.second.freeSize << numa.second.nr_hugepages_2M << numa.second.free_hugepages_2M
             << numa.second.timestamp << numa.second.nr_hugepages_1G << numa.second.free_hugepages_1G
             << numa.second.mempool_total << numa.second.mempool_used << numa.second.mempool_available_cleared
             << numa.second.mempool_available_uncleared << numa.second.nr_hugepages_512M << numa.second.free_hugepages_512M;
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
    UbseSerialization &outStream)
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

UbseResult GetUbseNodeInfoOffset(UbseNodeInfo info, UbseSerialization &outStream)
{
    outStream << info.nodeId << info.slotId << info.bondingEid << info.hostName << info.comIp << info.guid
              << enum_v(info.allocator) << info.pmdMapping << info.blockSize;
    UbseSerialization ipAddrOffset;
    auto ret = GetUbseIpAddrVecOffset(info.ipList, ipAddrOffset);
    UbseSerialization numaInfoOffset;
    ret |= GetUbseNumaInfoVecOffset(info.numaInfos, numaInfoOffset);
    UbseSerialization cpuInfoOffset;
    ret |= GetUbseCpuInfoOffset(info.cpuInfos, cpuInfoOffset);
    outStream << ipAddrOffset << numaInfoOffset << cpuInfoOffset << enum_v(info.localState) << enum_v(info.clusterState)
              << info.eventMessage << info.isLender << enum_v(info.sysSentryState) << enum_v(info.obmmState);

    if (ret != UBSE_OK || !outStream.Check()) {
        UBSE_LOG_ERROR << "Ubse serialize node info data failed, " << FormatRetCode(ret);
    }
    return ret;
}

uint32_t SerializeUbseNode(UbseNodeInfo info, uint8_t *&buffer, size_t &size)
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

uint32_t SerializeUbseNodeList(std::vector<UbseNodeInfo> infos, uint8_t *&buffer, size_t &size)
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

uint32_t SerializeDevDirConnectInfo(std::map<std::string, PhysicalLink> &devDirConnectInfo, uint8_t *&buffer,
                                    size_t &size)
{
    UbseResult ret = UBSE_OK;
    UbseSerialization outStream;
    outStream << (right_v<size_t>(devDirConnectInfo.size()));
    for (auto &physicalLinkInfo : devDirConnectInfo) {
        std::string linkId = physicalLinkInfo.first;
        PhysicalLink physicalLink = physicalLinkInfo.second;
        outStream << linkId << physicalLink.slotId << physicalLink.chipId << physicalLink.portId
                  << physicalLink.interfaceName
                  << physicalLink.peerSlotId << physicalLink.peerChipId << physicalLink.peerPortId
                  << physicalLink.peerInterfaceName
                  << enum_v(physicalLink.linkStatus);
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
template<typename IpAddrType>
UbseResult ValidateAndCopyIpData(const std::vector<uint8_t>& srcData,
                                 IpAddrType& destAddr,
                                 size_t expectedSize,
                                 const char* ipTypeName)
{
    if (srcData.size() != expectedSize) {
        UBSE_LOG_ERROR << ipTypeName << " address size mismatch. Expected: "
                       << expectedSize << ", Got: " << srcData.size();
        return UBSE_ERROR;
    }

    std::copy(srcData.begin(), srcData.end(), destAddr.addr);
    return UBSE_OK;
}

UbseResult SetUbseIpAddr(UbseDeSerialization &inStream, UbseIpAddr &addr)
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

UbseResult ParseIpList(UbseDeSerialization &inStream, UbseNodeInfo &info)
{
    // 数组类型simpo先获取元素个数
    size_t itemNum = 0;
    inStream >> itemNum;
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "Ubse deserialize itemNum failed";
        return UBSE_ERROR;
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

uint32_t ParseNumaInfo(UbseDeSerialization &inStream, UbseNodeInfo &nodeInfo)
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

uint32_t ParseCpuInfo(UbseDeSerialization &inStream, UbseNodeInfo &nodeInfo)
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

uint32_t ParseNodeInfo(UbseNodeInfo &info, UbseDeSerialization &inStream)
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
    UbseDeSerialization ipAddrOffset;
    UbseDeSerialization numaInfoOffset;
    UbseDeSerialization cpuInfoOffset;
    inStream >> ipAddrOffset >> numaInfoOffset >> cpuInfoOffset;
    inStream >> enum_v(info.localState) >> enum_v(info.clusterState);
    inStream >> info.eventMessage >> info.isLender >> enum_v(info.sysSentryState) >> enum_v(info.obmmState);
    auto ret = ParseIpList(ipAddrOffset, info);
    ret |= ParseNumaInfo(numaInfoOffset, info);
    ret |= ParseCpuInfo(cpuInfoOffset, info);
    return ret;
}

uint32_t DeSerializeUbseNode(UbseNodeInfo &info, uint8_t *buffer, size_t size)
{
    UbseDeSerialization inStream(buffer, size);
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "Deserialize mem node info failed";
        return UBSE_ERROR;
    }
    return ParseNodeInfo(info, inStream);
}

uint32_t DeSerializeUbseNodeList(std::vector<UbseNodeInfo> &infos, uint8_t *buffer, size_t size)
{
    UbseResult ret = UBSE_OK;
    UbseDeSerialization inStream(buffer, size);
    size_t num = 0;
    inStream >> num;
    infos.reserve(num);
    if (!inStream.Check()) {return UBSE_ERROR;}
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

uint32_t DeSerializeDevDirConnectInfo(std::map<std::string, PhysicalLink> &devDirConnectInfo, uint8_t *buffer,
                                      size_t size)
{
    UbseResult ret = UBSE_OK;
    UbseDeSerialization inStream(buffer, size);
    // map大小
    size_t num = 0;
    inStream >> num;
    if (!inStream.Check()) {return UBSE_ERROR;}
    for (size_t i = 0; i < num; i++) {
        std::string linkId;
        inStream >> linkId;
        if (!inStream.Check()) {return UBSE_ERROR;}
        auto &physicalLink = devDirConnectInfo[linkId];
        inStream >> physicalLink.slotId >> physicalLink.chipId >> physicalLink.portId >> physicalLink.interfaceName
            >> physicalLink.peerSlotId >> physicalLink.peerChipId >> physicalLink.peerPortId
            >> physicalLink.peerInterfaceName;
        inStream >> enum_v(physicalLink.linkStatus);
        if (!inStream.Check()) {
            UBSE_LOG_ERROR << "Ubse deserialize node info list[" << i << "]  failed, " << FormatRetCode(ret);
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

uint32_t UbseGetNodeInfos(std::vector<NodeInfo> &nodeInfos)
{
    auto nodeInfoMap = UbseNodeController::GetInstance().GetAllNodes();
    if (nodeInfoMap.empty()) {
        return UBSE_ERROR;
    }
    nodeInfos.reserve(nodeInfoMap.size());
    for (const auto &[_, ubseNodeInfo] : nodeInfoMap) {
        NodeInfo nodeInfo{};
        nodeInfo.nodeId = ubseNodeInfo.nodeId;
        nodeInfo.hostName = ubseNodeInfo.hostName;
        for (const auto &ip : ubseNodeInfo.ipList) {
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

uint32_t UbseNodeGetNodeIdByHostname(const std::string &hostname, std::string &nodeId)
{
    auto nodeInfoMap = UbseNodeController::GetInstance().GetAllNodes();
    for (const auto &[_, nodeInfo] : nodeInfoMap) {
        if (nodeInfo.hostName == hostname) {
            nodeId = nodeInfo.nodeId;
            return UBSE_OK;
        }
    }
    return UBSE_ERROR;
}

// 辅助函数：解析字符串IP为UbseIpAddr结构
bool parseIpString(const std::string &ipStr, UbseIpAddr &out)
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

bool isIpInList(const std::string &ipStr, const std::vector<UbseIpAddr> &ipList)
{
    UbseIpAddr target{};
    if (!parseIpString(ipStr, target)) {
        return false; // 无效IP字符串，直接返回false
    }
    for (const auto &ip : ipList) {
        // 类型不匹配则跳过
        if (ip.type != target.type) {continue;}

        // 根据类型比较二进制数据
        if (ip.type == UbseIpType::UBSE_IP_V4) {
            if (std::memcmp(ip.ipv4.addr, target.ipv4.addr, NO_4) == 0) {return true;}
        } else { // IPv6
            if (std::memcmp(ip.ipv6.addr, target.ipv6.addr, NO_16) == 0) {return true;}
        }
    }
    return false;
}

bool CheckGuid(const std::string &value, const std::pair<const std::string, UbseNodeInfo> &nodeInfo)
{
    for (const auto &nodeData : nodeInfo.second.cpuInfos) {
        if (value == nodeData.second.guid) {
            return true;
        }
    }
    return false;
}

uint32_t UbseGetNodeIdByAttrValue(const NodeAttr &attr, const std::string &value, uint32_t &nodeId)
{
    std::unordered_map<std::string, UbseNodeInfo> nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    for (const auto &nodeInfo : nodeInfos) {
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

uint32_t UbseNodeGetLinkUpNodes(std::vector<UbseRoleInfo> &roleInfos)
{
    return UbseGetAllNodeInfos(roleInfos);
}
} // namespace ubse::nodeController