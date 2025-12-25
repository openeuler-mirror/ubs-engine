/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_node_controller.h"
#includ  <netinet/in.h>
#include <queue>
#include <regex>
#include <set>
#include "securec.h"
#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_lcne_module.h"
#include "ubse_serial_util.h"
#include "ubse_str_util.h"
#include "ubse_topology_interface.h"
#include "ubse_conf_module.h"
#include "ubse_node_controller_collector.h"
#include "ubse_net_util.h"
#include "ubse_node_controller_util.h"
#include "ubse_node_topology.h"
#include "ubse_node_controller_agent.h"

namespace ubse::nodeController {
using namespace ubse::context;
using namespace ubse::election;
using namespace ubse::serial;
using namespace ubse::mti;
using namespace ubse::config;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_NODE_CONTROLLER_MID)

const uint32_t LOCAL_HANDLER_RETRY_DURATION = 2;
const uint32_t IPV4_LENGTH = 4;
const uint32_t IPV6_LENGTH = 16;

/**
 * 从 LCNE 模块获取全量静态节点列表，用于选主模块查询全量节点列表，做选主操作
 * @return
 */
std::vector<UbseNodeInfo> UbseNodeController::GetStaticNodeInfo()
{
    std::vector<UbseNodeInfo> nodeInfos{};
    if (!IsUBEnable()) {
        return GetStaticNodeInfoFromConf();
    }
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "lcne module not load";
        return {};
    }
    std::vector<MtiNodeInfo> ubseNodeInfos{};
    auto ret = module->UbseGetAllNodeInfos(ubseNodeInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get all node infos from lcne failed, " << FormatRetCode(ret);
        return {};
    }
    for (auto node : ubseNodeInfos) {
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
    std::shared_lock<std::shared_mutex> lock(rwMutex);
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "election module not load";
        return {};
    }
    if (module->IsLeader()) {
        return nodeInfos;
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
        UBSE_LOG_ERROR << "get all node form master=" << masterNode.id << " failed, " << FormatRetCode(ret);
        return {};
    }
    std::unordered_map<std::string, UbseNodeInfo> maps{};
    for (auto info : infos) {
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
    for (auto iter : nodeInfos) {
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
            UBSE_LOG_DEBUG << "nodeId=" << currentNodeId << ", socketId=" << socketId << " , eid=" << cpuInfo.eid;
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
            UBSE_LOG_DEBUG << "nodeId=" << nodeId << ", socketId=" << socketId << " , eid=" << cpuInfo.eid;
            return ConvertStrToUint32(cpuInfo.eid, eid, NO_16);
        }
    }
    UBSE_LOG_ERROR << "nodeId=" << nodeId << ", socketId=" << socketId << " not found";
    return UBSE_ERROR;
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
    for (auto handler : handlers) {
        if (handler == nullptr) {
            continue;
        }
        ret |= handler(nodeInfo);
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << nodeInfo.nodeId << " update state="
                    << static_cast<uint32_t>(nodeInfo.clusterState)<< " exec handler failed, " << FormatRetCode(ret);
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
    existing = std::move(info);
    // 使用numaInfos更新拓扑数据中的本端信息
    UbseSocketIdChange(nodeId);
    rwMutex.unlock();
    return UBSE_OK;
}

void UbseNodeController::UbseSocketIdChange(const std::string &nodeId)
{
    std::unordered_map<uint32_t, uint32_t> socketIdMap;
    std::set<uint32_t> osSocketIdSet; // 本节点OS-socketId
    std::set<uint32_t> lcneChipIdSet; // 本节点chipId
    // 1. 排序全量SocketID
    for (auto &numa : nodeInfos[nodeId].numaInfos) {
        osSocketIdSet.insert(numa.second.socketId);
    }
    // 2. 排序全量chipID
    for (auto &cpu : nodeInfos[nodeId].cpuInfos) {
        uint32_t chipId{0};
        ubse::utils::ConvertStrToUint32(cpu.second.chipId, chipId);
        lcneChipIdSet.insert(chipId);
    }
    // 3. 构建映射关系
    if (lcneChipIdSet.size() != osSocketIdSet.size()) {
        UBSE_LOG_ERROR << "The number of CPUs provided by lcne for the current node does not match the number of CPUs "
                          "seen by the OS for the current node.";
    }
    auto it1 = lcneChipIdSet.begin();
    auto it2 = osSocketIdSet.begin();
    for (size_t i = 0; i < lcneChipIdSet.size() && i < osSocketIdSet.size(); i++) {
        socketIdMap[*it1] = *it2;
        ++it1;
        ++it2;
    }
    std::ostringstream oss;
    oss << "[node_controller] UbseSocketIdChange nodeId is: " << nodeId << "\n";
    std::for_each(osSocketIdSet.begin(), osSocketIdSet.end(),
                  [&oss](const uint32_t &osSocketId) { oss << "osSocketId is: " << osSocketId << "\n"; });
    std::for_each(lcneChipIdSet.begin(), lcneChipIdSet.end(),
                  [&oss](const uint32_t &lcneChipId) { oss << "lcneChipId is: " << lcneChipId << "\n"; });
    std::for_each(socketIdMap.begin(), socketIdMap.end(), [&oss](const std::pair<uint32_t, uint32_t> &socketPair) {
        oss << "lcneChipId is: " << socketPair.first << ","
            << " osSocketId is: " << socketPair.second << "\n";
    });
    oss << "\n";
    // 4. 进行更新,暂不改动key,若两边数据的cpu数量不符，此处只能保证数组不越界
    for (auto &cpu : nodeInfos[nodeId].cpuInfos) {
        uint32_t chipId{0};
        ubse::utils::ConvertStrToUint32(cpu.second.chipId, chipId);
        auto it = socketIdMap.find(chipId);
        if (it != socketIdMap.end()) {
            cpu.second.socketId = it->second;
        }
    }
    std::for_each(nodeInfos[nodeId].cpuInfos.begin(), nodeInfos[nodeId].cpuInfos.end(),
                  [&oss](const std::pair<UbseCpuLocation, UbseCpuInfo> &cpuInfo) {
                      oss << "After Mapping ,the socketId is: " << cpuInfo.second.socketId << "\n";
                  });
    UBSE_LOG_DEBUG << oss.str();
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
            physicalLink = {slotId, socketId, portId, peerSlotId, peerSocketId, peerPortId};
            return linkInfo.slotId + "/" + linkInfo.socketId + "/" + linkInfo.portId + "-" + linkInfo.peerSlotId + "/" +
                   linkInfo.peerSocketId + "/" + linkInfo.peerPortId;
        } else {
            physicalLink = {peerSlotId, peerSocketId, peerPortId, slotId, socketId, portId};
            return linkInfo.peerSlotId + "/" + linkInfo.peerSocketId + "/" + linkInfo.peerPortId + "-" +
                   linkInfo.slotId + "/" + linkInfo.socketId + "/" + linkInfo.portId;
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "LCNE provides data that cannot be converted to uint32, with the specific data being: "
                       << "slotId is " << linkInfo.slotId << "socketId is " << linkInfo.socketId << "portId is "
                       << linkInfo.portId << "peerSlotId is " << linkInfo.peerSlotId << "peerSocketId is "
                       << linkInfo.peerSocketId << "peerPortId is " << linkInfo.peerPortId;
    }
    return "ERROR-LINK";
}

void UbseNodeController::UpdateConnect(PhysicalLink &physicalLink, std::string &linkId)
{
    // 1.没有，插入数据，状态置为冲突
    auto it = devDirConnectInfo.find(linkId);
    if (it == devDirConnectInfo.end()) {
        physicalLink.linkStatus = LinkStatus::conflict;
        devDirConnectInfo[linkId] = physicalLink;
    } else { // 2.有，状态置为可用
        physicalLink.linkStatus = LinkStatus::available;
    }
}

void UbseNodeController::PrintDevDirConnectInfo()
{
    std::stringstream oss;
    oss << "------ DevDirConnectInfo INFO ------\n";
    for (auto &connect : devDirConnectInfo) {
        oss << "LinkId is " << connect.first << " slotId is " << connect.second.slotId << " chipId is "
            << connect.second.chipId << " portId is " << connect.second.portId << " peerSlotId is "
            << connect.second.peerSlotId << " peerChipId is " << connect.second.peerChipId << " peerPortId is "
            << connect.second.peerPortId << "\n";
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
        // 对端
        std::string remoteSlotId = portInfo.second.remoteSlotId;
        std::string remoteChipId = portInfo.second.remoteChipId;
        std::string remotePortId = portInfo.second.remotePortId;
        // 生成linkid 和 要填入的数据
        LinkInfo linkInfo{slotId, chipId, portId, remoteSlotId, remoteChipId, remotePortId};
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
                   updateState == UbseNodeClusterState::UBSE_NODE_PRE_BMC;
        case UbseNodeClusterState::UBSE_NODE_WORKING:
            return updateState == UbseNodeClusterState::UBSE_NODE_SMOOTHING ||
                   updateState == UbseNodeClusterState::UBSE_NODE_UNKNOWN ||
                   updateState == UbseNodeClusterState::UBSE_NODE_FAULT ||
                   updateState == UbseNodeClusterState::UBSE_NODE_PRE_BMC;
        case UbseNodeClusterState::UBSE_NODE_UNKNOWN:
            return updateState == UbseNodeClusterState::UBSE_NODE_SMOOTHING ||
                   updateState == UbseNodeClusterState::UBSE_NODE_FAULT;
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
    auto module = UbseContext::GetInstance().GetModule<UbseLcneModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "generate fault nodeId=" << nodeId << ", lcne module null.";
        return {};
    }
    std::vector<MtiNodeInfo> ubseNodeInfos{};
    ret = module->UbseGetAllNodeInfos(ubseNodeInfos);
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
             << numa.second.timestamp;

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
    outStream << info.nodeId << info.slotId << info.bondingEid << info.hostName << info.comIp;
    UbseSerialization ipAddrOffset;
    auto ret = GetUbseIpAddrVecOffset(info.ipList, ipAddrOffset);
    UbseSerialization numaInfoOffset;
    ret |= GetUbseNumaInfoVecOffset(info.numaInfos, numaInfoOffset);
    UbseSerialization cpuInfoOffset;
    ret |= GetUbseCpuInfoOffset(info.cpuInfos, cpuInfoOffset);
    outStream << ipAddrOffset << numaInfoOffset << cpuInfoOffset << enum_v(info.localState)
              << enum_v(info.clusterState);

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
                  << physicalLink.peerSlotId << physicalLink.peerChipId << physicalLink.peerPortId
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

UbseResult SetUbseIpAddr()
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
    for (size_t i = 0; i < itemNum; i++) {
        UbseDeSerialization item;
        inStream >> item;
        UbseNumaLocation location{};
        item >> location.nodeId >> location.numaId;
        UbseNumaInfo info{};
        info.location = location;
        item >> info.socketId >> info.bindCore >> info.size >> info.freeSize >> info.nr_hugepages_2M >>
            info.free_hugepages_2M >> info.timestamp;
        nodeInfo.numaInfos[location] = info;
        if (!inStream.Check()) {
            UBSE_LOG_ERROR << "Ubse deserialize numa info vec failed";
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

uint32_t ParseCpuInfo(UbseDeSerialization &inStream, UbseNodeInfo &nodeInfo)
{
    errno_t cpyRet = 0;
    size_t itemNum = 0;
    inStream >> itemNum;
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
        for (size_t j = 0; j < portNum; j++) {
            UbseDeSerialization port;
            item >> port;
            UbsePortInfo portInfo{};
            port >> portInfo.portId >> portInfo.ifName >> portInfo.portRole >> enum_v(portInfo.portStatus) >>
                portInfo.portCna >> portInfo.urmaEid >> portInfo.remoteSlotId >> portInfo.remoteChipId >>
                portInfo.remoteCardId >> portInfo.remoteIfName >> portInfo.remotePortId;
            info.portInfos[portInfo.portId] = portInfo;
        }
        nodeInfo.cpuInfos[location] = info;

        if (!inStream.Check()) {
            UBSE_LOG_ERROR << "Ubse deserialize cpu info vec failed";
            return UBSE_ERROR;
        }
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
    UbseDeSerialization ipAddrOffset;
    UbseDeSerialization numaInfoOffset;
    UbseDeSerialization cpuInfoOffset;
    inStream >> ipAddrOffset >> numaInfoOffset >> cpuInfoOffset;
    inStream >> enum_v(info.localState) >> enum_v(info.clusterState);
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
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "Deserialize mem node list failed";
        return UBSE_ERROR;
    }
    size_t num = 0;
    inStream >> num;
    infos.reserve(num);
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
    UbseDeSerialization inStream(buffer, size);
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "Deserialize devDirConnectInfo failed";
        return UBSE_ERROR;
    }
    // map大小
    size_t num = 0;
    inStream >> num;

    for (size_t i = 0; i < num; i++) {
        std::string linkId;
        inStream >> linkId;
        auto &physicalLink = devDirConnectInfo[linkId];
        inStream >> physicalLink.slotId >> physicalLink.chipId >> physicalLink.portId >> physicalLink.peerSlotId >>
            physicalLink.peerChipId >> physicalLink.peerPortId;
        inStream >> enum_v(physicalLink.linkStatus);
        if (!inStream.Check()) {
            UBSE_LOG_ERROR << "Ubse deserialize node info list[" << i << "]  failed, " << FormatRetCode(UBSE_ERROR);
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

// 拓扑图相关功能
UbseResult UbseNodeGetBorrowNodeCna(UbseNodeMemCnaInfoOutput &ubseNodeMemCnaInfoOutput,
                                    const ubse::mti::DevTopology &devTopologyInfo, const DevName &exportDevName,
                                    const DevName &borrowDevName)
{
    auto itBorrow = devTopologyInfo.find(borrowDevName);
    if (itBorrow == devTopologyInfo.end()) {
        UBSE_LOG_ERROR << "borrowDevName " << borrowDevName.devName << "Does not exist in devTopologyInfo";
        return UBSE_ERROR;
    }
    for (auto &borrowDirectConnect : itBorrow->second.second) {
        // 从借入访问的借出的设备名称
        UbseDevName borrowToExportDevName(borrowDirectConnect.second.remoteSlotId + "-" +
                                          borrowDirectConnect.second.remoteChipId);
        if (borrowToExportDevName.devName != exportDevName.devName) {
            UBSE_LOG_DEBUG << "The borrowToExportDevName " << borrowToExportDevName.devName << " is not direct dev";
            continue;
        }
        UBSE_LOG_DEBUG << "The borrowToExportDevName " << borrowToExportDevName.devName << " is direct dev";
        if (borrowDirectConnect.second.portStatus == ubse::mti::PortStatus::DOWN) {
            UBSE_LOG_DEBUG << "borrowDev portStatus to export is DOWN, borrowDevName is " << borrowDevName.devName;
            continue;
        }
        ubseNodeMemCnaInfoOutput.portGroupId = borrowDirectConnect.second.portId;
        ubseNodeMemCnaInfoOutput.borrowNodeCna = itBorrow->second.first.busNodeCna;
        return UBSE_OK;
    }
    UBSE_LOG_ERROR << "borrowDevName " << borrowDevName.devName
                 << "is not directly linked to exportDevName: " << exportDevName.devName;
    return UBSE_ERROR;
}

void ChangeEdgeInfo(
    std::unordered_map<ubse::mti::DevName, ubse::mti::DevName, ubse::mti::DevNameHash> &socketIdMap,
    std::unordered_map<ubse::mti::UbseDevPortName, ubse::mti::UbsePortInfo, ubse::mti::DevPortNameHash> &portInfos)
{
    for (auto &port : portInfos) {
        auto &portInfo = port.second;
        // 对端设备信息
        auto itRemote = socketIdMap.find(portInfo.remoteDevName);
        // Dbmap中没有对端，则不改动对端设备信息
        if (itRemote == socketIdMap.end()) {
            continue;
        }
        auto &remoteOsDevName = itRemote->second;
        std::string remoteNewNodeId;
        std::string remoteNewSocketId;
        remoteOsDevName.SplitDevName(remoteNewNodeId, remoteNewSocketId);

        portInfo.remoteDevName = remoteOsDevName;
        portInfo.remoteChipId = remoteNewSocketId;
    }
}

std::string GetSocketId(const std::string &devName)
{
    size_t pos = devName.find('-');
    if (pos == std::string::npos) {
        return ""; // 内部devName其实必然为标准的设备名
    } else {
        return devName.substr(pos + 1);
    }
}

void AccessMapChangeFunc(std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                         std::unordered_map<std::string, std::unordered_set<std::string>> &nodeIdToDevNameMap,
                         std::unordered_map<DevName, DevName, DevNameHash> &socketIdMap)
{
    std::unordered_map<std::string, std::string> newDevNameToNodeIdMap;
    for (auto &devNameToNodeId : devNameToNodeIdMap) {
        DevName lcneDevName(devNameToNodeId.first);
        auto it = socketIdMap.find(lcneDevName);
        if (it == socketIdMap.end()) {
            newDevNameToNodeIdMap.insert(devNameToNodeId);
            continue;
        }
        // 如有映射，则进行改动(改动原值)
        auto &osDevName = it->second;
        std::string newSocketId;
        std::string newNodeId;
        osDevName.SplitDevName(newNodeId, newSocketId);
        newDevNameToNodeIdMap[osDevName.devName] = newNodeId;
    }
    devNameToNodeIdMap = newDevNameToNodeIdMap;

    // 7.nodeid不会变化，因此不存在改动外层key的情况
    for (auto &nodeIdToDevName : nodeIdToDevNameMap) {
        auto &nodeId = nodeIdToDevName.first;
        std::unordered_set<std::string> newDevNameSet;
        for (auto &localDevNameStr : nodeIdToDevName.second) {
            DevName lcneDevName(localDevNameStr);
            auto it = socketIdMap.find(lcneDevName);
            if (it == socketIdMap.end()) {
                newDevNameSet.insert(localDevNameStr);
                continue;
            }
            auto &osDevName = it->second;
            newDevNameSet.insert(osDevName.devName);
        }
        nodeIdToDevName.second = newDevNameSet;
    }
}

void DevTopoChangeFunc(DevTopology &devTopologyInfo, std::unordered_map<DevName, DevName, DevNameHash> &socketIdMap)
{
    DevTopology newDevTopologyInfo;
    for (auto &devTopo : devTopologyInfo) {
        auto &lcneDevName = devTopo.first;
        // DBmap中没有映射，则不改动该key，只改动其边
        auto it = socketIdMap.find(lcneDevName);
        if (it == socketIdMap.end()) {
            UBSE_LOG_DEBUG << "LCNE Unmodified topology key is " << lcneDevName.devName;
            // b.边信息
            std::unordered_map<ubse::mti::UbseDevPortName, ubse::mti::UbsePortInfo, DevPortNameHash> &portInfos =
                devTopo.second.second;
            ChangeEdgeInfo(socketIdMap, portInfos);
            // 插入修改后结果
            newDevTopologyInfo.insert(devTopo);
            continue;
        }
        // 如有映射，则进行改动，改动其key和值
        auto &osDevName = it->second;
        std::string newSocketId;
        std::string newNodeId;
        osDevName.SplitDevName(newNodeId, newSocketId);
        // a.设备信息
        auto &devInfo = devTopo.second.first;
        devInfo.devName = osDevName;
        devInfo.chipId = newSocketId;
        // b.边信息
        std::unordered_map<ubse::mti::UbseDevPortName, ubse::mti::UbsePortInfo, ubse::mti::DevPortNameHash> &portInfos =
            devTopo.second.second;
        ChangeEdgeInfo(socketIdMap, portInfos);
        // 拷贝修改后结果
        UBSE_LOG_DEBUG << "LCNE The modified topology key was originally " << lcneDevName.devName
                     << " and after modification, it is " << osDevName.devName;
        newDevTopologyInfo[osDevName] = devTopo.second;
    }
    devTopologyInfo = newDevTopologyInfo;
}

void UbseSocketIdChange(std::unordered_map<std::string, TelemetryNodeData> &nodeDbMap, DevTopology &devTopologyInfo,
                        std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                        std::unordered_map<std::string, std::unordered_set<std::string>> &nodeIdToDevNameMap)
{
    // nodeDbMap(OS socketId) devTopologyInfo(LCNE socketId)
    std::unordered_map<DevName, DevName, DevNameHash> socketIdMap;
    // 1.遍历LCNE数据，当nodeDbMap存在该nodeid的情况，才做修改
    for (auto &nodeInfo : nodeIdToDevNameMap) {
        auto &nodeId = nodeInfo.first;
        auto it = nodeDbMap.find(nodeId);
        if (it == nodeDbMap.end()) {
            continue;
        }
        // 2.该节点 LCNE socketId数据排序
        std::vector<std::string> lcneSocketIdVec;
        for (auto &devName : nodeInfo.second) {
            auto socketName = GetSocketId(devName);
            lcneSocketIdVec.emplace_back(socketName);
        }
        // socketid保证为uint32的string
        std::sort(lcneSocketIdVec.begin(), lcneSocketIdVec.end(), [](const std::string &a, const std::string &b) {
            try {
                return std::stoul(a) < std::stoul(b);
            } catch (...) {
                return std::tuple(a.length(), a) < std::tuple(b.length(), b);
            }
        });
        // 3.该节点OS socketId数据排序
        auto &osSocketIdVec = it->second.sockets;
        for (auto &os : osSocketIdVec) {
            UBSE_LOG_DEBUG << "Begin osSocketIdVec socketId is " << os.socketId;
        }
        std::sort(osSocketIdVec.begin(), osSocketIdVec.end(), [](const SocketData &a, const SocketData &b) -> bool {
            try {
                return std::stoul(a.socketId) < std::stoul(b.socketId);
            } catch (...) {
                return std::tuple(a.socketId.length(), a.socketId) < std::tuple(b.socketId.length(), b.socketId);
            }
        });
        for (auto &os : osSocketIdVec) {
            UBSE_LOG_DEBUG << "End osSocketIdVec socketId is " << os.socketId;
        }
        // 4.提供该节点的socketId映射 LCNE socketId -> OS socketId
        for (size_t i = 0; i < lcneSocketIdVec.size() && i < osSocketIdVec.size(); i++) {
            DevName lcneDevName(nodeId, lcneSocketIdVec[i]);
            DevName osDevName(nodeId, osSocketIdVec[i].socketId);
            socketIdMap[lcneDevName] = osDevName;
            UBSE_LOG_DEBUG << "LCNE devName is " << lcneDevName.devName << ", and corresponding Os devName is "
                         << osDevName.devName;
        }
    }
    // 5.遍历每个lcne相关数据，进行修改,全局拓扑
    DevTopoChangeFunc(devTopologyInfo, socketIdMap);
    // 6.辅助map
    AccessMapChangeFunc(devNameToNodeIdMap, nodeIdToDevNameMap, socketIdMap);
}

UbseResult UbseGetElectionMap(std::unordered_map<std::string, ElectionNodeInfo> &nodeRoleMap)
{
    auto electionModule = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "electionModule not init";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    Node master{};
    Node standby{};
    std::vector<Node> agent{};
    UbseResult ret = electionModule->UbseGetAllNodes(master, standby, agent);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseGetAllNodes failed. " << FormatRetCode(ret);
        return ret;
    }
    if (!master.id.empty()) {
        nodeRoleMap[master.id] = {true, ELECTION_ROLE_MASTER};
        UBSE_LOG_DEBUG << "UbseGetElectionMap master.id is " << master.id;
    }
    if (!standby.id.empty()) {
        nodeRoleMap[standby.id] = {true, ELECTION_ROLE_STANDBY};
        UBSE_LOG_DEBUG << "UbseGetElectionMap standby.id is " << standby.id;
    }
    for (auto &ag : agent) {
        if (!ag.id.empty()) {
            nodeRoleMap[ag.id] = {true, ELECTION_ROLE_AGENT};
            UBSE_LOG_DEBUG << "UbseGetElectionMap ag.id is " << ag.id;
        }
    }
    return UBSE_OK;
}
void FillTelemetryNodeData(const std::string &nodeId, const UbseNodeInfo &nodeInfo,
                           std::unordered_map<std::string, TelemetryNodeData> &nodeDbMap)
{
    TelemetryNodeData &telData = nodeDbMap[nodeId];
    telData.nodeId = nodeId;
    telData.hostname = nodeInfo.hostName;

    std::map<uint32_t, SocketData> socketMap;

    // 处理 NUMA 信息
    for (const auto &[loc, numaInfo] : nodeInfo.numaInfos) {
        uint32_t sockId = numaInfo.socketId;
        SocketData &sock = socketMap[sockId];
        sock.socketId = std::to_string(sockId); // 确保 socketId 被设置

        NumaData numaData;
        numaData.numaId = std::to_string(numaInfo.location.numaId);

        if (std::find(sock.numas.begin(), sock.numas.end(), numaData) == sock.numas.end()) {
            sock.numas.push_back(numaData);
        }

        for (auto cpuId : numaInfo.bindCore) {
            CpuData cpuData;
            cpuData.CpuId = std::to_string(cpuId);
            if (std::find(sock.cpus.begin(), sock.cpus.end(), cpuData) == sock.cpus.end()) {
                sock.cpus.push_back(cpuData);
            }
        }
    }

    // 写回
    for (auto &[_, sock] : socketMap) {
        telData.sockets.push_back(std::move(sock));
    }
}
void BuildDevTopologyAndMappings(const std::string &nodeId, const UbseNodeInfo &nodeInfo,
                                 ubse::mti::DevTopology &devTopologyInfo,
                                 std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                                 std::unordered_map<std::string, std::unordered_set<std::string>> &nodeIdToDevNameMap)
{
    for (const auto &[loc, cpuInfo] : nodeInfo.cpuInfos) {
        // 构造 devName = nodeId + "-" + chipId
        std::string devNameStr = nodeId + "-" + cpuInfo.chipId;
        ubse::mti::DevName devName;
        devName.devName = devNameStr;

        // 构建 UbseDeviceInfo
        ubse::mti::UbseDeviceInfo devInfo;
        devInfo.devName = devName;
        devInfo.slotId = std::to_string(nodeInfo.slotId);
        devInfo.chipId = cpuInfo.chipId;
        devInfo.cardId = cpuInfo.cardId;
        devInfo.type = ubse::mti::DevType::CPU;
        devInfo.eid = cpuInfo.eid;
        devInfo.guid = cpuInfo.guid;
        devInfo.busNodeCna = cpuInfo.busNodeCna;

        // 构建 PortMap
        std::unordered_map<ubse::mti::UbseDevPortName, ubse::mti::UbsePortInfo, ubse::mti::DevPortNameHash> portMap;

        for (const auto &[portLoc, portInfo] : cpuInfo.portInfos) {
            ubse::mti::UbsePortInfo port;
            port.portId = portInfo.portId;
            port.ifName = portInfo.ifName;
            port.portRole = portInfo.portRole;
            port.portStatus = static_cast<ubse::mti::PortStatus>(portInfo.portStatus);

            // 手动构造 devPortName
            ubse::mti::UbseDevPortName devPortName(std::to_string(nodeInfo.slotId), cpuInfo.chipId, cpuInfo.cardId,
                                                   portInfo.portId);

            // 对端信息
            port.remoteSlotId = portInfo.remoteSlotId;
            port.remoteChipId = portInfo.remoteChipId;
            port.remoteCardId = portInfo.remoteCardId;
            port.remoteIfName = portInfo.remoteIfName;
            port.remoteDevName = {portInfo.remoteSlotId, portInfo.remoteChipId};
            port.remotePortId = portInfo.remotePortId;

            portMap[devPortName] = port;
        }

        // 插入拓扑
        devTopologyInfo[devName] = std::make_pair(devInfo, portMap);

        // 填充映射表
        devNameToNodeIdMap[devName.devName] = nodeId;
        nodeIdToDevNameMap[nodeId].insert(devName.devName);

        // 处理对端映射（模仿旧逻辑）
        for (const auto &[portName, portInfo] : cpuInfo.portInfos) {
            DevName peerDevName = {portInfo.remoteSlotId, portInfo.remoteChipId};
            if (!peerDevName.devName.empty()) {
                const std::string &peerDevNameStr = peerDevName.devName;
                const std::string &peerNodeId = portInfo.remoteSlotId; // 模仿旧代码

                devNameToNodeIdMap[peerDevNameStr] = peerNodeId;
                nodeIdToDevNameMap[peerNodeId].insert(peerDevNameStr);
            }
        }
    }
}

uint32_t UbseNodeTopoGetBasicData(std::unordered_map<std::string, TelemetryNodeData> &nodeDbMap,
                                  std::unordered_map<std::string, ElectionNodeInfo> &nodeRoleMap,
                                  DevTopology &devTopologyInfo,
                                  std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                                  std::unordered_map<std::string, std::unordered_set<std::string>> &nodeIdToDevNameMap)
{
    const auto &allNodeInfo = UbseNodeController::GetInstance().GetAllNodes();

    // 清空输出
    nodeDbMap.clear();
    nodeRoleMap.clear();
    devTopologyInfo.clear();
    devNameToNodeIdMap.clear();
    nodeIdToDevNameMap.clear();

    // 遍历所有节点
    for (const auto &[nodeId, nodeInfo] : allNodeInfo) {
        // 1. 填充遥测数据
        FillTelemetryNodeData(nodeId, nodeInfo, nodeDbMap);

        // 2. 构建设备拓扑和映射
        BuildDevTopologyAndMappings(nodeId, nodeInfo, devTopologyInfo, devNameToNodeIdMap, nodeIdToDevNameMap);
    }

    // 3. 填充选举角色
    auto ret = UbseGetElectionMap(nodeRoleMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseGetElectionMap Failed. " << FormatRetCode(ret);
        return ret;
    }

    // 4. 硬件拓扑改造：socketId 映射调整
    UbseSocketIdChange(nodeDbMap, devTopologyInfo, devNameToNodeIdMap, nodeIdToDevNameMap);

    return UBSE_OK;
}

uint32_t UbseNodeMemGetTopologyCnaInfo(const UbseNodeMemCnaInfoInput &nodeMemCnaInfoInput,
                                       UbseNodeMemCnaInfoOutput &nodeMemCnaInfoOutput)
{
    // 获取硬件拓扑，基于socket链接
    std::unordered_map<std::string, TelemetryNodeData> nodeDbMap{};  // K:nodeId V：遥测数据
    std::unordered_map<std::string, ElectionNodeInfo> nodeRoleMap{}; // K:nodeId V：角色数据
    DevTopology devTopologyInfo;
    std::unordered_map<std::string, std::string> devNameToNodeIdMap;
    std::unordered_map<std::string, std::unordered_set<std::string>> nodeIdToDevNameMap;
    auto ret =
        UbseNodeTopoGetBasicData(nodeDbMap, nodeRoleMap, devTopologyInfo, devNameToNodeIdMap, nodeIdToDevNameMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseNodeTopologyMgrGetDevInfo Failed. " << FormatRetCode(ret);
        return ret;
    }
    // 找到借出设备
    DevName exportDevName(nodeMemCnaInfoInput.exportNodeId + "-" + nodeMemCnaInfoInput.exportSocketId);
    auto it = devTopologyInfo.find(exportDevName);
    if (it == devTopologyInfo.end()) {
        UBSE_LOG_ERROR << "exportDevName " << exportDevName.devName << "Does not exist in devTopologyInfo";
        return UBSE_ERROR;
    } else if (it->second.first.type != ubse::mti::DevType::CPU) {
        UBSE_LOG_ERROR << "exportDevName " << exportDevName.devName << "is not CPU";
        return UBSE_ERROR;
    }
    UBSE_LOG_DEBUG << "exportDevName is " << exportDevName.devName;
    // 2D full mesh连接下，node之间的socket只会一一对应连接，不会全连接
    for (auto &directConnect : it->second.second) {
        std::string directNodeId = directConnect.second.remoteSlotId;
        if (directConnect.second.remoteSlotId != nodeMemCnaInfoInput.borrowNodeId) {
            UBSE_LOG_DEBUG << "The directNodeId " << directNodeId << " is not direct node";
            continue;
        }
        UBSE_LOG_DEBUG << "The directNodeId " << directNodeId << " is direct node";
        // 找到匹配的直连节点
        // 获取借入端的数据，需要在借入端节点查找
        DevName borrowDevName(directConnect.second.remoteSlotId + "-" + directConnect.second.remoteChipId);
        if (directConnect.second.portStatus == ubse::mti::PortStatus::DOWN) {
            UBSE_LOG_DEBUG << "exportDev portStatus to borrow is DOWN, borrowDevName is " << borrowDevName.devName;
            continue;
        }
        ret = UbseNodeGetBorrowNodeCna(nodeMemCnaInfoOutput, devTopologyInfo, exportDevName, borrowDevName);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "UbseNodeGetBorrowNodeCna failed. " << FormatRetCode(ret);
            return UBSE_ERROR;
        }
        // 借出端可查数据
        nodeMemCnaInfoOutput.exportSocketId = nodeMemCnaInfoInput.exportSocketId; // 借出设备的socket_id
        nodeMemCnaInfoOutput.exportNodeCna = it->second.first.busNodeCna;         // 借出端设备cna标识
        nodeMemCnaInfoOutput.borrowSocketId = directConnect.second.remoteChipId;  // 借入设备的socket_id
        return UBSE_OK;
    }
    UBSE_LOG_ERROR << "exportDevName " << exportDevName.devName
                   << "is not directly linked to nodeId: " << nodeMemCnaInfoInput.borrowNodeId;
    return UBSE_ERROR;
}
void TopoBfsPerLayerPerEdge(
    std::vector<std::pair<TopologyEdgeInfo, int>> &edgeData, std::queue<std::string> &que, int jumpCount,
    std::unordered_set<std::string> &traversedDevNameSet,
    std::unordered_map<ubse::mti::UbseDevPortName, ubse::mti::UbsePortInfo, ubse::mti::DevPortNameHash> &edgeMap)
{
    for (auto &edge : edgeMap) {
        ubse::mti::UbsePortInfo ubsePortInfo = edge.second;
        if (ubsePortInfo.portStatus == ubse::mti::PortStatus::DOWN) {
            continue;
        }
        auto remoteDevName = ubsePortInfo.remoteDevName.devName;
        if (traversedDevNameSet.find(remoteDevName) == traversedDevNameSet.end()) {
            traversedDevNameSet.insert(remoteDevName);
            que.push(remoteDevName);
            TopologyEdgeInfo topologyEdgeInfo{remoteDevName, ubsePortInfo.ifName};
            edgeData.emplace_back(topologyEdgeInfo, jumpCount);
        }
    }
}
UbseResult TopoBfsPerLayer(const DevTopology &devTopologyInfo, std::vector<std::pair<TopologyEdgeInfo, int>> &edgeData,
                           std::queue<std::string> &que, int jumpCount,
                           std::unordered_set<std::string> &traversedDevNameSet)
{
    size_t levelSize = que.size();
    for (size_t queCount = 0; queCount < levelSize; queCount++) {
        DevName devName;
        devName.devName = que.front();
        que.pop();
        auto it = devTopologyInfo.find(devName);
        if (it == devTopologyInfo.end()) {
            UBSE_LOG_INFO << "The DevName " << devName.devName << " doesn't exist.";
            continue;
        }
        std::unordered_map<ubse::mti::UbseDevPortName, ubse::mti::UbsePortInfo, ubse::mti::DevPortNameHash> edgeMap =
            it->second.second;
        TopoBfsPerLayerPerEdge(edgeData, que, jumpCount, traversedDevNameSet, edgeMap);
    }
    return UBSE_OK;
}
// 根据跳数bfs
UbseResult UbseTopologyBfs(int jump, DevTopology &devTopologyInfo,
                           std::vector<std::pair<TopologyEdgeInfo, int>> &edgeData, const std::string &localDevName)
{
    std::queue<std::string> que;
    std::unordered_set<std::string> traversedDevNameSet;
    que.push(localDevName);
    traversedDevNameSet.insert(localDevName);
    for (int jumpCount = 1; jumpCount <= jump; jumpCount++) {
        if (que.empty()) {
            break;
        }
        auto ret = TopoBfsPerLayer(devTopologyInfo, edgeData, que, jumpCount, traversedDevNameSet);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "TopoBfsPerLayer failed. " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}
UbseResult UbseGetTopologyInfoByJump(const JumpCount &jump, DevTopology &devTopologyInfo,
                                     std::vector<std::pair<TopologyEdgeInfo, int>> &edgeData,
                                     const std::string &localDevName)
{
    if (jump == JumpCount::One) {
        return UbseTopologyBfs(1, devTopologyInfo, edgeData, localDevName); // 1表示1跳
    } else if (jump == JumpCount::Two) {
        return UbseTopologyBfs(2, devTopologyInfo, edgeData, localDevName); // 2表示2跳
    } else if (jump == JumpCount::All) {
        return UbseTopologyBfs(devTopologyInfo.size(), devTopologyInfo, edgeData,
                               localDevName); // 所有的跳数，即小于总节点数
    }
    return UBSE_ERROR;
}
UbseResult DevNameRemoveNodeName(const std::string &remoteDevNameStr, std::string &remoteDevSocketNameStr)
{
    std::regex pattern(R"((\w+)-(\w+))");

    std::smatch matches; // 用于保存匹配结果

    if (std::regex_match(remoteDevNameStr, matches, pattern)) {
        remoteDevSocketNameStr = matches[2].str(); // 2用于获取节点内设备名称
    } else {
        UBSE_LOG_ERROR << "The remoteDevNameStr " << remoteDevNameStr << " is not in the correct format.";
        return UBSE_ERROR;
    }

    return UBSE_OK;
}
UbseResult UbseNodeExtractDevNameInfo(std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                                      std::string &remoteNodeName, std::string &remoteDevSocketNameStr,
                                      const std::string &remoteDevNameStr)
{
    auto it = devNameToNodeIdMap.find(remoteDevNameStr);
    if (it == devNameToNodeIdMap.end()) {
        UBSE_LOG_ERROR << "The remoteDevName " << remoteDevNameStr << " cannot match any str in devNameToNodeIdMap.";
        return UBSE_ERROR;
    }
    remoteNodeName = it->second;
    // 设备名-> socket名
    auto ret = DevNameRemoveNodeName(remoteDevNameStr, remoteDevSocketNameStr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to parse the regular expression." << remoteDevNameStr << "Failed. "
                       << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}
TelemetrySocketData &UbseNodePadSocketData(const std::string &remoteNodeName, const std::string &remoteDevSocketNameStr,
                                           const std::string &remoteDevNameStr,
                                           TelemetrySocketData &telemetrySocketData,
                                           std::unordered_map<std::string, TelemetryNodeData> &nodeDbMap)
{
    telemetrySocketData.nodeId = remoteNodeName;                  // LCNE可提供
    telemetrySocketData.socket.socketId = remoteDevSocketNameStr; // LCNE可提供
    auto itLdc = nodeDbMap.find(remoteNodeName);
    if (itLdc == nodeDbMap.end()) {
        UBSE_LOG_WARN << "The remoteNodeName " << remoteNodeName << " cannot match any str in nodeDbMap.";
    } else {
        TelemetryNodeData nodeData = itLdc->second;
        telemetrySocketData.hostname = nodeData.hostname;
        // 查找设备
        auto itSocket = find_if(nodeData.sockets.begin(), nodeData.sockets.end(),
                                [&remoteDevSocketNameStr](const SocketData &socketData) {
                                    return socketData.socketId == remoteDevSocketNameStr;
                                });
        if (itSocket != nodeData.sockets.end()) {
            telemetrySocketData.socket = *itSocket;
        } else {
            UBSE_LOG_WARN << "The data of the device named " << remoteDevNameStr << " does not exist in the database.";
        }
    }
    return telemetrySocketData;
}
UbseResult MemFillPerEdgeData(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology,
                              std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                              const std::string &localDevName, const std::pair<TopologyEdgeInfo, int> &edge,
                              UbseNodeData &ubseNodeData)
{
    std::string remoteDevNodeName;
    std::string remoteDevSocketNameStr;
    std::string remoteDevNameStr = edge.first.remoteDevName; // 对端DevName
    // 提取对端设备信息
    auto ret =
        UbseNodeExtractDevNameInfo(devNameToNodeIdMap, remoteDevNodeName, remoteDevSocketNameStr, remoteDevNameStr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Extract DevName: " << remoteDevNameStr << " Failed. " << FormatRetCode(ret);
        return ret;
    }
    // 遥测数据，数据库里存在遥测数据就填充，不存在就不填充。
    TelemetrySocketData telemetrySocketData{};
    telemetrySocketData = UbseNodePadSocketData(remoteDevNodeName, remoteDevSocketNameStr, remoteDevNameStr,
                                                telemetrySocketData, ubseNodeData.nodeDbMap);
    MemNodeData memNodeData(std::move(telemetrySocketData));

    // 选举数据
    auto itRole1 = ubseNodeData.nodeRoleMap.find(remoteDevNodeName);
    if (itRole1 != ubseNodeData.nodeRoleMap.end()) {
        memNodeData.isRegisterRm = true;
    } else {
        memNodeData.isRegisterRm = false;
    }

    nodeTopology[localDevName].emplace_back(memNodeData);

    return UBSE_OK;
}
UbseResult MemFillAllEdgeData(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology,
                              std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                              const std::string &localDevName, std::vector<std::pair<TopologyEdgeInfo, int>> &edgeData,
                              UbseNodeData &ubseNodeData)
{
    for (auto &edge : edgeData) {
        if (edge.second != 1) { // 只要1跳
            continue;
        }
        auto ret = MemFillPerEdgeData(nodeTopology, devNameToNodeIdMap, localDevName, edge, ubseNodeData);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "MemFillPerEdgeData Failed. " << FormatRetCode(ret);
            return ret;
        }
    }
    return UBSE_OK;
}
UbseResult MemTopoGetResult(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology,
                            DevTopology &devTopologyInfo,
                            std::unordered_map<std::string, std::string> &devNameToNodeIdMap,
                            std::unordered_map<std::string, std::vector<std::pair<TopologyEdgeInfo, int>>> &edgeDataMap,
                            UbseNodeData &ubseNodeData)
{
    for (auto &edgeInfo : edgeDataMap) {
        std::string localDevNameStr = edgeInfo.first;
        std::vector<std::pair<TopologyEdgeInfo, int>> edgeData = edgeInfo.second;
        if (edgeData.empty()) {
            nodeTopology[localDevNameStr] = {};
            continue;
        }
        DevName localDevName(localDevNameStr);
        auto it = devTopologyInfo.find(localDevName);
        if (it != devTopologyInfo.end() && it->second.first.type == ubse::mti::DevType::CPU) {
            auto ret = MemFillAllEdgeData(nodeTopology, devNameToNodeIdMap, localDevNameStr, edgeData, ubseNodeData);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "MemFillAllEdgeData Failed. " << FormatRetCode(ret);
                return ret;
            }
        }
    }
    return UBSE_OK;
}
UbseResult MemGetTopologyInfo(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology)
{
    UBSE_LOG_DEBUG << "UbseMemGetTopologyInfo init";
    std::unordered_map<std::string, TelemetryNodeData> nodeDbMap{};  // K:nodeId V：遥测数据
    std::unordered_map<std::string, ElectionNodeInfo> nodeRoleMap{}; // K:nodeId V：角色数据
    DevTopology devTopologyInfo;
    std::unordered_map<std::string, std::string> devNameToNodeIdMap;
    std::unordered_map<std::string, std::unordered_set<std::string>> nodeIdToDevNameMap;
    auto ret =
        UbseNodeTopoGetBasicData(nodeDbMap, nodeRoleMap, devTopologyInfo, devNameToNodeIdMap, nodeIdToDevNameMap);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseNodeTopoGetBasicData Failed. " << FormatRetCode(ret);
        return ret;
    }
    // bfs
    std::unordered_map<std::string, std::vector<std::pair<TopologyEdgeInfo, int>>>
        edgeDataMap; // k:devName v:对端dev信息，跳数
    for (auto &devInfo : devTopologyInfo) {
        std::string localSocketId = devInfo.first.devName;
        std::vector<std::pair<TopologyEdgeInfo, int>> edgeData;
        UbseGetTopologyInfoByJump(JumpCount::One, devTopologyInfo, edgeData, localSocketId);
        edgeDataMap[localSocketId] = edgeData;
    }
    // 提供包含节点信息的结果
    UbseNodeData ubseNodeData{nodeDbMap, nodeRoleMap};
    return MemTopoGetResult(nodeTopology, devTopologyInfo, devNameToNodeIdMap, edgeDataMap, ubseNodeData);
}
std::unordered_map<std::string, std::vector<MemNodeData>> ConvertToOldTopology(
    const std::unordered_map<std::string, UbseNodeInfo> &newNodeInfoMap)
{
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;

    for (const auto &[nodeId, nodeInfo] : newNodeInfoMap) {
        // 用于按 socketId 分组数据
        std::unordered_map<uint32_t, SocketData> socketDataMap;

        // Step 1: 遍历 cpuInfos，填充 cpus 和 socketId
        for (const auto &[cpuLoc, cpuInfo] : nodeInfo.cpuInfos) {
            uint32_t socketId = cpuInfo.socketId;

            SocketData &sockData = socketDataMap[socketId];
            sockData.socketId = std::to_string(socketId);

            // 添加 CPU
            CpuData cpuData;
            cpuData.CpuId = cpuInfo.chipId; // 或 slotId? 根据实际语义调整
            // 避免重复添加
            if (std::find(sockData.cpus.begin(), sockData.cpus.end(), cpuData) == sockData.cpus.end()) {
                sockData.cpus.push_back(std::move(cpuData));
            }
        }

        // Step 2: 遍历 numaInfos，填充 numas
        for (const auto &[numaLoc, numaInfo] : nodeInfo.numaInfos) {
            uint32_t socketId = numaInfo.socketId;
            SocketData &sockData = socketDataMap[socketId];
            sockData.socketId = std::to_string(socketId); // 确保初始化

            NumaData numaData;
            numaData.numaId = std::to_string(numaInfo.location.numaId);
            if (std::find(sockData.numas.begin(), sockData.numas.end(), numaData) == sockData.numas.end()) {
                sockData.numas.push_back(std::move(numaData));
            }
        }

        // Step 3: 为每个 socket 构造 MemNodeData
        std::vector<MemNodeData> memNodeDataVector;
        for (auto &[socketId, socketData] : socketDataMap) {
            TelemetrySocketData telemetryData;
            telemetryData.nodeId = nodeInfo.nodeId;
            telemetryData.socket = socketData;
            telemetryData.hostname = nodeInfo.hostName;

            MemNodeData memNodeData;
            // 使用移动构造
            static_cast<TelemetrySocketData &>(memNodeData) = std::move(telemetryData);
            memNodeData.isRegisterRm = true; // 根据实际逻辑判断是否可注册 UBSE

            memNodeDataVector.push_back(std::move(memNodeData));
        }

        // Step 4: 插入到 nodeTopology，key 是 devNameStr（这里假设 nodeId 就是 devNameStr）
        std::string devNameStr = nodeInfo.nodeId; // 或其他映射逻辑
        nodeTopology[devNameStr] = std::move(memNodeDataVector);
    }

    return nodeTopology;
}

uint32_t UbseMemGetTopologyInfo(std::unordered_map<std::string, std::vector<MemNodeData>> &nodeTopology)
{
    auto ret = MemGetTopologyInfo(nodeTopology);
    // mem不需要0跳数设备（非CPU拓扑0跳数）
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemGetTopologyInfo Failed. " << FormatRetCode(ret);
        return ret;
    }
    for (auto it = nodeTopology.begin(); it != nodeTopology.end();) {
        if (it->second.empty()) {
            it = nodeTopology.erase(it);
        } else {
            ++it;
        }
    }
    return ret;
}

uint32_t UbseNodeGetLinkUpNodes(std::vector<UbseRoleInfo> &roleInfos)
{
    return UbseGetAllNodeInfos
}
} // namespace ubse::nodeController