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
#include <fstream>
#include <iostream>
#include "ubse_common_def.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_node_controller.h"
#include "ubse_topology_interface.h"

namespace ubse::election {
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::nodeController;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_ELECTION_MID)

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
    bool ubEnable;
    GetUBEnable(ubEnable);
    currentNode_.port = ubEnable ? URMA_LISTEN_JETTY : TCP_LISTEN_PORT;
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
        UBSE_LOG_WARN << "heartBeatTime should be between 1000 and 60000, use default value: 5000";
    } else {
        heartBeatTime_ = heartBeatTime;
    }

    ret = LoadConfig();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] UbseElectionNodeMgr: LoadConfig failed.";
    }
}

void UbseElectionNodeMgr::ParseAllNodesVector(const std::vector<UbseNodeInfo> &allNodesVec)
{
    bool ubEnable;
    GetUBEnable(ubEnable);
    for (const auto &node : allNodesVec) {
        uint16_t port = ubEnable ? URMA_LISTEN_JETTY : TCP_LISTEN_PORT;
        Node tempNode;
        tempNode.id = node.nodeId;
        tempNode.ip = ubEnable ? std::string(node.bondingEid) : node.comIp;
        tempNode.port = port;
        currentAllNodes_.push_back(tempNode);
    }
}

UbseResult UbseElectionNodeMgr::LoadConfig()
{
    UbseNodeInfo ubseNodeInfo = UbseNodeController::GetInstance().GetCurNode();
    if (ubseNodeInfo.nodeId.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] LoadConfig get nodeId failed.";
        return UBSE_ERROR;
    }
    bool ubEnable;
    GetUBEnable(ubEnable);
    currentNode_.id = ubseNodeInfo.nodeId;
    if (ubEnable) {
        currentNode_.ip = std::string(ubseNodeInfo.bondingEid);
    } else {
        currentNode_.ip = ubseNodeInfo.comIp;
        currentNode_.port = TCP_LISTEN_PORT;
    }

    lastAllNodes_.clear();
    lastAllNodes_ = std::move(currentAllNodes_);
    currentAllNodes_.clear();

    std::vector<UbseNodeInfo> ubseNodeInfos = UbseNodeController::GetInstance().GetStaticNodeInfo();
    if (ubseNodeInfos.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] LoadConfig get allNodes failed.";
        return UBSE_ERROR;
    }
    ParseAllNodesVector(ubseNodeInfos);

    // 校验
    if (currentNode_.id == "" || currentNode_.ip == "" || currentAllNodes_.empty()) {
        UBSE_LOG_WARN << "[ELECTION] LoadConfig: invalid local config, please check.";
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

UbseResult UbseElectionNodeMgr::GetMyselfNode(Node &myself)
{
    if (currentNode_.id == "" || currentNode_.ip == "" || currentNode_.port == 0) {
        // 节点无效返回 error
        UBSE_LOG_WARN << "[ELECTION] GetMyselfNode: invalid local node.";
        return UBSE_ERROR;
    }
    myself = currentNode_;
    return UBSE_OK;
}

UbseResult UbseElectionNodeMgr::GetAllNode(std::vector<Node> &allNodes)
{
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