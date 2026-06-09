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
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "role/ubse_election_role.h"
#include "ubse_common_def.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_node_controller.h"
#include "ubse_node_mgr.h"

namespace ubse::election {
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::nodeController;
using namespace ubse::nodeMgr;

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
    ubEnable_ = IsUrma();
    if (GetRootIpList().empty()) { rootEnable_ = false; }
    if (GetAllNodesStoredByGroup().size() != NO_1) { isHierarchicalElection_ = true; }
    ret = LoadConfig();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] UbseElectionNodeMgr: LoadConfig failed.";
    }
}

std::unordered_set<UBSE_ID_TYPE> UbseElectionNodeMgr::GetTopoLinkedNodes() const
{
    std::unordered_set<UBSE_ID_TYPE> topoLinkedNodes{};
    adapter_plugins::mti::UbseDevTopology devTopology{};
    auto ret = adapter_plugins::mti::UbseMtiInterface::GetInstance().GetCurNodeTopo(devTopology);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[MTI] get devTopology not successful, " << FormatRetCode(ret);
        return topoLinkedNodes;
    }
    for (auto& [devName, devTopo] : devTopology) {
        std::string devNodeId, chipId;
        devName.GetNodeIdAndChipId(devNodeId, chipId);
        for (auto& port : devTopo.second) {
            if (port.second.remoteSlotId != "-") {
                topoLinkedNodes.insert(port.second.remoteSlotId);
            }
        }
    }
    return topoLinkedNodes;
}

void UbseElectionNodeMgr::ParseAllNodesVector()
{
    std::unique_lock<std::shared_mutex> lock(mtx_);
    if (!isHierarchicalElection_) {
        if (ubEnable_) {
            currentAllNodes_.clear();
            nodeIpMap_.clear();
            auto ubseNodeInfos = GetAllNodes();
            if (ubseNodeInfos.empty()) {
                UBSE_LOG_ERROR << "[ELECTION] LoadConfig get allNodes failed.";
                return;
            }
            auto topoLinkedNodes = GetTopoLinkedNodes();
            for (const auto &node : ubseNodeInfos) {
                Node tempNode;
                tempNode.id = node.nodeId;
                tempNode.ip = node.bonding0Eid;
                tempNode.port = TCP_LISTEN_PORT;
                if (topoLinkedNodes.find(node.nodeId) != topoLinkedNodes.end() || currentNode_.id == node.nodeId) {
                    currentAllNodes_.push_back(tempNode);
                    nodeIpMap_.emplace(tempNode.ip, node.nodeId);
                }
            }
        } else if (!ubEnable_ && currentAllNodes_.empty()) {
            std::vector<std::string> ipList{};
            auto ubseNodeInfos = GetAllNodes();
            if (ubseNodeInfos.empty()) {
                UBSE_LOG_ERROR << "[ELECTION] LoadConfig get allNodes failed.";
                return;
            }
            for (const auto &nodeInfo : ubseNodeInfos) {
                Node tempNode;
                tempNode.ip = nodeInfo.addr;
                tempNode.port = TCP_LISTEN_PORT;
                if (currentNode_.ip == nodeInfo.addr) {
                    tempNode.id = currentNode_.id;
                    UBSE_LOG_INFO << "[ELECTION] current id =" << currentNode_.id << " current ip =" << currentNode_.ip;
                }
                currentAllNodes_.push_back(tempNode);
                nodeIpMap_.emplace(tempNode.ip, tempNode.id);
            }
        }
    } else {
        if (!rootEnable_) {
            GetGroupNodes(currentAllNodes_);
        }
    }
}

UbseResult UbseElectionNodeMgr::LoadConfig()
{
    UbseNodeStaticInfo nodeStaticInfo = GetCurrentNode();
    if (nodeStaticInfo.nodeId.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] GetCurrentNode failed.";
        return UBSE_ERROR;
    }

    currentNode_.id = nodeStaticInfo.nodeId;
    currentNode_.port = TCP_LISTEN_PORT;
    if (isHierarchicalElection_ && rootEnable_) {
        currentNode_.ip = nodeStaticInfo.addr;
    } else {
        currentNode_.ip = ubEnable_ ? nodeStaticInfo.bonding0Eid : nodeStaticInfo.addr;
    }

    ParseAllNodesVector();

    if (currentNode_.id.empty() || currentNode_.ip.empty() || currentAllNodes_.empty()) {
        UBSE_LOG_WARN << "[ELECTION] LoadConfig: invalid local config, please check.";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "[ELECTION] LoadConfig success: nodeId=" << currentNode_.id
                  << ", ip=" << currentNode_.ip << ", port=" << currentNode_.port;
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

UbseResult UbseElectionNodeMgr::GetGroupNodes(std::vector<Node> &groupNodes)
{
    UbseNodeStaticInfo nodeInfo = GetCurrentNode();
    std::vector<UbseNodeStaticInfo> nodeStaticInfos = GetNodesByGroupId(nodeInfo.groupId);
    bool ubEnable = true;
    GetUBEnable(ubEnable);
    groupNodes.clear();
    for (const auto &nodeStaticInfo : nodeStaticInfos) {
        Node node;
        node.id = nodeStaticInfo.nodeId;
        node.ip = ubEnable? nodeStaticInfo.bonding0Eid : nodeStaticInfo.addr;
        node.port = TCP_LISTEN_PORT;
        groupNodes.push_back(node);
    }
    return UBSE_OK;
}

UbseResult UbseElectionNodeMgr::GetGroupId(std::string &groupId)
{
    UbseNodeStaticInfo nodeInfo = GetCurrentNode();
    UBSE_LOG_INFO <<"[ELECTION] Current node group id is " << nodeInfo.groupId;
    groupId = std::to_string(nodeInfo.groupId);
    return UBSE_OK;
}

UbseResult UbseElectionNodeMgr::GetGroupIdByNodeId(const std::string &nodeId, std::string &groupId)
{
    UbseNodeStaticInfo nodeInfo = GetUbseNodeById(nodeId);
    UBSE_LOG_INFO <<"[ELECTION] node id is "<< nodeId <<", group id is " << nodeInfo.groupId;
    groupId = std::to_string(nodeInfo.groupId);
    return UBSE_OK;
}
} // namespace ubse::election