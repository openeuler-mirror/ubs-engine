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

#include "ubse_election_node_mgr.h"
#include "ubse_common_def.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_node_controller.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"

namespace ubse::election {
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::nodeController;
UBSE_DEFINE_THIS_MODULE("ubse");

static const std::string ELECTION_ROLE_INIT = "init";

UbseElectionNodeMgr &UbseElectionNodeMgr::GetInstance()
{
    static UbseElectionNodeMgr instance;
    return instance;
}

UbseResult GetUBEnable(bool &ubEnable)
{
    auto ubseConfModule = ubse::context::UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (ubseConfModule == nullptr) {
        UBSE_LOG_ERROR << "Get config info failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    std::string ipList;
    auto ret = ubseConfModule->GetConf<std::string>("ubse.rpc", "cluster.ipList", ipList);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "Unable to get ub config, use default urma, " << FormatRetCode(ret);
        ubEnable = true;
        return UBSE_OK;
    }
    ubEnable = false;
    return UBSE_OK;
}

UbseElectionNodeMgr::UbseElectionNodeMgr()
    : heartBeatTime_(DEFAULT_HEART_BEAT_TIME),
      heartBeatLost_(DEFAULT_HEART_BEAT_LOST),
      master_(ELECTION_ROLE_INIT),
      standby_(ELECTION_ROLE_INIT),
      currentNode_()
{
    currentNode_.id = "";
    currentNode_.ip = "";
    currentNode_.port = TCP_LISTEN_PORT;
    currentNode_.state = UbseNodeChangeState::INIT;

    uint32_t heartBeatTime = DEFAULT_HEART_BEAT_TIME;
    auto module = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] GetHeartBeatTime GetModule failed.";
        heartBeatTime_ = heartBeatTime;
        heartBeatLost_ = DEFAULT_HEART_BEAT_LOST;
        return;
    }

    UbseResult ret =
        module->GetConf<uint32_t>(UBSE_ELECTION_SECTION, UBSE_ELECTION_HEARTBEAT_LOST_THRESHOLD, heartBeatLost_);
    if (ret != UBSE_OK || heartBeatLost_ < DEFAULT_HEART_BEAT_LOST || heartBeatLost_ > MAX_HEART_BEAT_LOST) {
        UBSE_LOG_WARN << "[ELECTION] Heartbeat loss threshold is invalid. Resetting to default: "
                      << DEFAULT_HEART_BEAT_LOST << " .";
        heartBeatLost_ = DEFAULT_HEART_BEAT_LOST;
    }

    ret = module->GetConf<uint32_t>(UBSE_ELECTION_SECTION, UBSE_ELECTION_HEARTBEAT_TIME_INTERVAL, heartBeatTime);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetHeartBeatTime get heartbeat.timeInterval failed, " << FormatRetCode(ret)
                      << ", will use default value : " << DEFAULT_HEART_BEAT_TIME;
        heartBeatTime_ = heartBeatTime;
    }
    if (heartBeatTime < 1000 || heartBeatTime > 60000) { // 心跳时间范围在1000到60000之间
        heartBeatTime_ = DEFAULT_HEART_BEAT_TIME;
        UBSE_LOG_WARN << "heartBeatTime should be between 1000 and 60000, use default value: "
                      << DEFAULT_HEART_BEAT_TIME;
    } else {
        heartBeatTime_ = heartBeatTime;
    }

    ret = LoadConfig();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] UbseElectionNodeMgr: LoadConfig failed.";
    }
}

void BuildEdgeInfo(
    std::pair<const adapter_plugins::mti::UbseDevPortName, adapter_plugins::mti::UbseMtiCpuTopoPortInfo>& port,
    UbsePortInfo& portInfo)
{
    portInfo.portId = port.second.portId;
    portInfo.ifName = port.second.ifName;
    portInfo.portRole = port.second.portRole;
    portInfo.portStatus = static_cast<PortStatus>(port.second.portStatus);
    portInfo.portCna = port.second.portCna;
    portInfo.urmaEid = port.second.urmaEid;
    portInfo.remoteSlotId = port.second.remoteSlotId;
    portInfo.remoteChipId = port.second.remoteChipId;
    portInfo.remoteCardId = port.second.remoteCardId;
    portInfo.remoteIfName = port.second.remoteIfName;
    portInfo.remotePortId = port.second.remotePortId;
}

UbseResult CollectCpuInfo(UbseNodeInfo &ubseNodeInfo, const std::string &nodeId)
{
    adapter_plugins::mti::UbseDevTopology devTopology{};
    adapter_plugins::mti::UbseMtiCpuTopoInfoMap cpuTopoInfosGroupByDevName{};
    auto ret = adapter_plugins::mti::UbseMtiInterface::GetInstance().GetClusterCpuTopo(cpuTopoInfosGroupByDevName);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[MTI] get cpuTopoInfo not successful, " << FormatRetCode(ret);
        return ret;
    }
    for (auto& [devName, cpuTopoInfo] : cpuTopoInfosGroupByDevName) {
        std::string devNodeId, socketId;
        devName.SplitDevName(devNodeId, socketId);
        if (devNodeId != nodeId) {
            continue;
        }
        UbseCpuInfo info{};
        info.slotId = cpuTopoInfo.slotId;
        info.socketId = cpuTopoInfo.socketId;
        UbseCpuLocation location{nodeId, info.socketId};
        auto cpyRet = strcpy_s(info.primaryEid, sizeof(info.primaryEid), cpuTopoInfo.primaryEid.c_str());
        if (cpyRet != EOK) {
            UBSE_LOG_ERROR << "copy primaryEid failed, ErrorCode=" << cpyRet;
            return cpyRet;
        }
        info.chipId = cpuTopoInfo.chipId;
        info.cardId = cpuTopoInfo.cardId;
        info.busNodeCna = cpuTopoInfo.busNodeCna;
        info.eid = cpuTopoInfo.eid;  // LCNE获取时能保证key存在
        info.guid = cpuTopoInfo.guid;  // LCNE获取时能保证key存在
        for (auto& port : cpuTopoInfo.portInfos) {
            nodeController::UbsePortInfo portInfo{};
            BuildEdgeInfo(port, portInfo);
            info.portInfos[portInfo.portId] = portInfo;
        }
        ubseNodeInfo.cpuInfos[location] = info;
    }
    return UBSE_OK;
}

std::unordered_set<UBSE_ID_TYPE> UbseElectionNodeMgr::GetTopoLinkedNodes() const
{
    UbseNodeInfo curNodeInfo{};
    std::unordered_set<UBSE_ID_TYPE> topoLinkedNodes{};
    CollectCpuInfo(curNodeInfo, currentNode_.id);
    for (const auto &[cpuLocation, cpuInfo] : curNodeInfo.cpuInfos) {
        for (const auto &[portId, portInfo] : cpuInfo.portInfos) {
            if (portInfo.remoteSlotId != "-") {
                topoLinkedNodes.insert(portInfo.remoteSlotId);
            }
        }
    }
    return topoLinkedNodes;
}

void UbseElectionNodeMgr::ParseAllNodesVector()
{
    bool ubEnable = true;
    GetUBEnable(ubEnable);
    const uint16_t port = TCP_LISTEN_PORT;
    std::unique_lock<std::shared_mutex> lock(mtx_);
    if (ubEnable) {
        currentAllNodes_.clear();
        nodeIpMap_.clear();
        std::vector<UbseNodeInfo> ubseNodeInfos = UbseNodeController::GetInstance().GetStaticNodeInfo();
        if (ubseNodeInfos.empty()) {
            UBSE_LOG_ERROR << "[ELECTION] LoadConfig get allNodes failed.";
            return;
        }
        auto topoLinkedNodes = GetTopoLinkedNodes();
        for (const auto &node : ubseNodeInfos) {
            Node tempNode;
            tempNode.id = node.nodeId;
            tempNode.ip = std::string(node.bondingEid);
            tempNode.port = port;
            if (topoLinkedNodes.find(node.nodeId) != topoLinkedNodes.end() || currentNode_.id == node.nodeId) {
                currentAllNodes_.push_back(tempNode);
                nodeIpMap_.emplace(tempNode.ip, node.nodeId);
            }
        }
    } else if (!ubEnable && currentAllNodes_.empty()) {
        std::vector<std::string> ipList{};
        adapter_plugins::mti::UbseMtiInterface::GetInstance().GetClusterIpList(ipList);
        for (const auto &ip : ipList) {
            Node tempNode;
            tempNode.ip = ip;
            tempNode.port = port;
            if (currentNode_.ip == ip) {
                tempNode.id = currentNode_.id;
                UBSE_LOG_INFO << "[ELECTION] current id =" << currentNode_.id << " current ip =" << currentNode_.ip;
            }
            currentAllNodes_.push_back(tempNode);
            nodeIpMap_.emplace(tempNode.ip, tempNode.id);
        }
    }
}

UbseResult UbseElectionNodeMgr::LoadConfig()
{
    UbseNodeInfo ubseNodeInfo = UbseNodeController::GetInstance().GetCurNode();
    if (ubseNodeInfo.nodeId.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] LoadConfig get nodeId failed.";
        return UBSE_ERROR;
    }
    bool ubEnable = true;
    GetUBEnable(ubEnable);
    if (ubEnable) {
        currentNode_.ip = std::string(ubseNodeInfo.bondingEid);
    } else {
        adapter_plugins::mti::UbseMtiInterface::GetInstance().GetLocalIp(currentNode_.ip);
        currentNode_.port = TCP_LISTEN_PORT;
    }
    currentNode_.id = ubseNodeInfo.nodeId;
    if (currentNode_.id.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] LoadConfig nodeId empty.";
        return UBSE_ERROR;
    }
    ParseAllNodesVector();
    // 校验
    if (currentNode_.id.empty() || currentNode_.ip.empty() || currentAllNodes_.empty()) {
        UBSE_LOG_WARN << "[ELECTION] LoadConfig: invalid local config, please check.";
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

UbseResult UbseElectionNodeMgr::GetMyselfNode(Node &myself)
{
    std::shared_lock<std::shared_mutex> lock(mtx_);
    if (currentNode_.id.empty() || currentNode_.ip.empty() || currentNode_.port == 0) {
        // 节点无效返回 error
        UBSE_LOG_WARN << "[ELECTION] GetMyselfNode: invalid local node.";
        return UBSE_ERROR;
    }
    myself = currentNode_;
    return UBSE_OK;
}

UbseResult UbseElectionNodeMgr::GetAllNode(std::vector<Node> &allNodes)
{
    std::shared_lock<std::shared_mutex> lock(mtx_);
    // 判断当前所有节点是否为空
    if (currentAllNodes_.empty()) {
        UBSE_LOG_WARN << "[ELECTION] GetAllNode: no node found.";
        return UBSE_ERROR;
    }
    allNodes = currentAllNodes_;
    return UBSE_OK;
}

UbseResult UbseElectionNodeMgr::GetAllNeighbourNode(std::vector<Node> &neighbourNodes)
{
    auto ret = GetAllNode(neighbourNodes);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetAllNode failed.";
        return UBSE_ERROR;
    }
    Node myself{};
    ret = GetMyselfNode(myself);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetMyselfNode failed.";
        return UBSE_ERROR;
    }
    // 如果myself不在neighbourNodes里
    if (std::find(neighbourNodes.begin(), neighbourNodes.end(), myself) == neighbourNodes.end()) {
        UBSE_LOG_WARN << "[ELECTION] MyselfNode not in AllNodes : " << myself.id;
        return UBSE_ERROR;
    }
    neighbourNodes.erase(std::remove(neighbourNodes.begin(), neighbourNodes.end(), myself), neighbourNodes.end());
    return UBSE_OK;
}

UbseResult UbseElectionNodeMgr::GetNodeInfoByID(const UBSE_ID_TYPE &id, std::string &ip, uint16_t &port)
{
    if (id.empty()) {
        UBSE_LOG_DEBUG << "[ELECTION] GetNodeInfoByID: id is empty.";
        return UBSE_ERROR;
    }
    std::shared_lock<std::shared_mutex> lock(mtx_);
    if (currentAllNodes_.empty()) {
        UBSE_LOG_WARN << "[ELECTION] GetNodeInfoByID: currentAllNodes_ is empty.";
        return UBSE_ERROR;
    }
    for (const auto &it : currentAllNodes_) {
        if (it.id == id) {
            ip = it.ip;
            port = it.port;
            return UBSE_OK;
        }
    }
    UBSE_LOG_DEBUG << "[ELECTION] GetNodeInfoByID: id:" << id << "not found.";
    return UBSE_ERROR;
}

UbseResult UbseElectionNodeMgr::GetPortByIp(const std::string &ip, uint16_t &port)
{
    if (ip.empty()) {
        UBSE_LOG_DEBUG << "[ELECTION] GetPortByIp: id is empty.";
        return UBSE_ERROR;
    }
    std::shared_lock<std::shared_mutex> lock(mtx_);
    if (currentAllNodes_.empty()) {
        UBSE_LOG_WARN << "[ELECTION] GetPortByIp: currentAllNodes_ is empty.";
        return UBSE_ERROR;
    }
    for (const auto &it : currentAllNodes_) {
        if (it.ip == ip) {
            port = it.port;
            return UBSE_OK;
        }
    }
    UBSE_LOG_DEBUG << "[ELECTION] GetPortByIp: ip=" << ip << "not found.";
    return UBSE_ERROR;
}

UbseResult UbseElectionNodeMgr::UpdateNodeIdWithConnect(const std::string &ip, const std::string &id)
{
    if (ip.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] ip is empty.";
        return UBSE_ERROR;
    }
    if (id.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] id is empty.";
        return UBSE_ERROR;
    }
    std::unique_lock<std::shared_mutex> lock(mtx_);
    if (currentAllNodes_.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] currentAllNodes_ is empty.";
        return UBSE_ERROR;
    }

    for (auto &node : currentAllNodes_) {
        if (node.ip == ip) {
            node.id = id;
            nodeIpMap_[ip] = id;
            UBSE_LOG_DEBUG << "[ELECTION] Updated for ip = " << ip << " to id = " << id;
            return UBSE_OK;
        }
    }
    // 如果没有找到匹配的节点
    UBSE_LOG_ERROR << "[ELECTION] ip=" << ip << " not found in currentAllNodes_.";
    return UBSE_ERROR;
}

UbseResult UbseElectionNodeMgr::GetNodeIdByIp(const std::string &ip, std::string &id)
{
    if (ip.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] ip is empty.";
        return UBSE_ERROR;
    }
    std::shared_lock<std::shared_mutex> lock(mtx_);
    if (nodeIpMap_.find(ip) != nodeIpMap_.end()) {
        id = nodeIpMap_[ip];
        return UBSE_OK;
    }
    UBSE_LOG_ERROR << "[ELECTION] ip not found in nodeIpMap_.";
    return UBSE_ERROR;
}

UbseResult UbseElectionNodeMgr::GetNodeIpById(const std::string &id, std::string &ip)
{
    if (id.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] ip is empty.";
        return UBSE_ERROR;
    }
    std::shared_lock<std::shared_mutex> lock(mtx_);
    for (auto info : nodeIpMap_) {
        if (std::get<1>(info) == id) {
            ip = std::get<0>(info);
            return UBSE_OK;
        }
    }
    UBSE_LOG_ERROR << "[ELECTION] id not found in nodeIpMap_.";
    return UBSE_ERROR;
}

UbseResult UbseElectionNodeMgr::GetNodeIpMap(std::unordered_map<std::string, UBSE_ID_TYPE> &nodeIpMap)
{
    std::shared_lock<std::shared_mutex> lock(mtx_);
    // 判断当前所有节点是否为空
    if (nodeIpMap_.empty()) {
        UBSE_LOG_WARN << "[ELECTION] nodeIpMap_ is empty.";
        return UBSE_ERROR;
    }
    nodeIpMap = nodeIpMap_;
    return UBSE_OK;
}

uint32_t UbseElectionNodeMgr::GetHeartBeatTime() const
{
    return heartBeatTime_;
}

uint32_t UbseElectionNodeMgr::GetHeartBeatLost() const
{
    return heartBeatLost_;
}

UbseNodeLocalState UbseElectionNodeMgr::GetLocalNodeState()
{
    UbseNodeInfo ubseNodeInfo = UbseNodeController::GetInstance().GetCurNode();
    if (ubseNodeInfo.nodeId.empty()) {
        UBSE_LOG_WARN << "[ELECTION] Current node id is empty.";
        ubseNodeInfo.localState = UbseNodeLocalState::UBSE_NODE_RESTORE;
    }
    return ubseNodeInfo.localState;
}
} // namespace ubse::election