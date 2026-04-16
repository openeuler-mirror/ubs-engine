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

#include "ubse_election.h"

#include "role/ubse_election_role_mgr.h"
#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_error.h"
#include "ubse_logger.h"

namespace ubse::election {
using namespace ubse::log;

UBSE_DEFINE_THIS_MODULE("ubse");

uint32_t UbseGetNodeCount(uint32_t &count)
{
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Getting the election module failed.";
        return UBSE_ERROR;
    }
    Node master{};
    Node standby{};
    std::vector<Node> agents{};
    auto ret = electionModule->UbseGetAllNodes(master, standby, agents);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] Get all nodes roleInfos failed";
        return ret;
    }
    if (master.id != INVALID_NODE_ID && standby.id != INVALID_NODE_ID) {
        count = agents.size() + 2; // 2，主节点和备节点
    } else if (master.id != INVALID_NODE_ID || standby.id != INVALID_NODE_ID) {
        count = agents.size() + 1; // 1,主节点或者备节点
    } else {
        count = agents.size();
    }
    return UBSE_OK;
}

uint32_t UbseGetRoleInfos(std::vector<UbseRoleInfo> &roleInfos, uint32_t &count)
{
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Getting the election module failed.";
        return UBSE_ERROR;
    }
    Node master{};
    Node standby{};
    std::vector<Node> agents{};
    auto ret = electionModule->UbseGetAllNodes(master, standby, agents);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] Get all nodes roleInfos failed";
        return ret;
    }
    std::vector<UbseRoleInfo> allRoleInfos;
    if (!master.id.empty()) {
        allRoleInfos.emplace_back(master.id, ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    }
    if (!standby.id.empty()) {
        allRoleInfos.emplace_back(standby.id, ELECTION_ROLE_STANDBY, ELECTION_NODE_ONLINE);
    }
    for (const auto &agent : agents) {
        allRoleInfos.emplace_back(agent.id, ELECTION_ROLE_AGENT, ELECTION_NODE_ONLINE);
    }
    roleInfos = allRoleInfos;

    count = roleInfos.size();
    return UBSE_OK;
}

uint32_t UbseGetRoleInfo(UbseRoleInfo &roleInfo, const std::string &nodeId)
{
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Getting the election module failed.";
        return UBSE_ERROR;
    }
    Node master{};
    Node standby{};
    std::vector<Node> agents{};
    auto ret = electionModule->UbseGetAllNodes(master, standby, agents);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] Get all nodes roleInfos failed";
        return ret;
    }
    std::vector<UbseRoleInfo> allRoleInfos;
    allRoleInfos.emplace_back(master.id, ELECTION_ROLE_MASTER);
    allRoleInfos.emplace_back(standby.id, ELECTION_ROLE_STANDBY);
    for (const auto &agent : agents) {
        allRoleInfos.emplace_back(agent.id, ELECTION_ROLE_AGENT);
    }

    for (const auto &item : allRoleInfos) {
        if (item.nodeId == nodeId) {
            roleInfo.nodeId = item.nodeId;
            roleInfo.nodeRole = item.nodeRole;
            roleInfo.status = ELECTION_NODE_ONLINE;
            return UBSE_OK;
        }
    }
    UBSE_LOG_WARN << "[ELECTION] Get the nodeId failed: " << nodeId;
    return UBSE_ERROR;
}

bool IsValidElectionEventType(UbseElectionEventType type)
{
    const auto &validTypes = GetEleEventTypes();
    return std::find(validTypes.begin(), validTypes.end(), type) != validTypes.end();
}

uint32_t UbseElectionChangeAttachHandler(const UbseElectionHandler &handler)
{
    if (!IsValidElectionEventType(handler.type)) {
        UBSE_LOG_ERROR << "[ELECTION] Invalid election event type";
        return UbseElectionTypeError;
    }
    if (handler.name.size() > ELECTION_HANDLER_NAME_MAX_SIZE || handler.name.size() <= 0) {
        UBSE_LOG_WARN << "[ELECTION] Invalid handler name size";
        return UbseElectionNameError;
    }
    return RoleMgr::GetInstance().RoleChangeAttach(handler.type, handler);
}

uint32_t UbseElectionChangeDeAttachHandler(const UbseElectionHandler &handler)
{
    if (!IsValidElectionEventType(handler.type)) {
        UBSE_LOG_WARN << "[ELECTION] Invalid election event type";
        return UbseElectionTypeError;
    }
    return RoleMgr::GetInstance().RoleChangeDeAttach(handler.type, handler);
}

uint32_t UbseGetMasterInfo(UbseRoleInfo &roleInfo)
{
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Getting the election module failed.";
        return UBSE_ERROR_NULLPTR;
    }
    Node masterNode{};
    auto ret = electionModule->UbseGetMasterNode(masterNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] UbseGetMasterNodeUbseGetMasterNode: Get master node roleInfo failed";
        return ret;
    }
    roleInfo.nodeId = masterNode.id;
    roleInfo.nodeRole = ELECTION_ROLE_MASTER;
    roleInfo.status = ELECTION_NODE_ONLINE;
    if (roleInfo.nodeId == "") {
        UBSE_LOG_WARN << "[ELECTION] Get master node roleInfo failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseGetStandbyInfo(UbseRoleInfo &roleInfo)
{
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Getting the election module failed.";
        return UBSE_ERROR_NULLPTR;
    }
    Node standbyNode;
    auto ret = electionModule->UbseGetStandbyNode(standbyNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get standby node role information.";
        return ret;
    }
    roleInfo.nodeId = standbyNode.id;
    roleInfo.nodeRole = ELECTION_ROLE_STANDBY;
    roleInfo.status = ELECTION_NODE_ONLINE;
    if (roleInfo.nodeId.empty()) {
        UBSE_LOG_WARN << "[ELECTION] Standby node has not been elected by the master node yet.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseGetAllNodeInfos(std::vector<UbseRoleInfo> &roleInfos)
{
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Getting the election module failed.";
        return UBSE_ERROR;
    }
    Node masterNode{};
    Node standbyNode{};
    std::vector<Node> agentNodes{};
    auto ret = electionModule->UbseGetAllNodes(masterNode, standbyNode, agentNodes);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] Get all nodes roleInfos failed";
        return ret;
    }
    if (!masterNode.id.empty()) {
        UbseRoleInfo masterNodeInfo = { masterNode.id, ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE };
        roleInfos.push_back(masterNodeInfo);
    }
    if (!standbyNode.id.empty()) {
        UbseRoleInfo standbyNodeInfo = { standbyNode.id, ELECTION_ROLE_STANDBY, ELECTION_NODE_ONLINE };
        roleInfos.push_back(standbyNodeInfo);
    }
    for (const auto &node : agentNodes) {
        roleInfos.emplace_back(node.id, ELECTION_ROLE_AGENT, ELECTION_NODE_ONLINE);
    }
    return UBSE_OK;
}

uint32_t UbseGetNodeStatus(const std::string &role, uint8_t &status)
{
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Getting the election module failed.";
        return UBSE_ERROR;
    }
    uint8_t currentStatus = 0;
    if (role == ELECTION_ROLE_MASTER) {
        auto ret = electionModule->GetMasterStatus(currentStatus);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "[ELECTION] Get master status failed";
            return ret;
        }
    } else if (role == ELECTION_ROLE_STANDBY) {
        auto ret = electionModule->GetStandbyStatus(currentStatus);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "[ELECTION] Get standby status failed";
            return ret;
        }
    } else if (role == ELECTION_ROLE_AGENT) {
        auto &ubseContext = ubse::context::UbseContext::GetInstance();
        currentStatus = ubseContext.GetWorkReadiness();
    } else {
        UBSE_LOG_WARN << "[ELECTION] Invalid role";
        return UBSE_ERROR;
    }
    status = currentStatus;
    return UBSE_OK;
}

uint32_t UbseGetCurrentNodeInfo(UbseRoleInfo &currentNode)
{
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Getting the election module failed.";
        return UBSE_ERROR;
    }
    Node currentNodeInfo{};
    auto ret = electionModule->GetCurrentNode(currentNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] Get current node Id failed";
        return ret;
    }

    currentNode.nodeId = currentNodeInfo.id;
    auto role = RoleMgr::GetInstance().GetRole()->GetRoleType();
    if (role == RoleType::STANDBY) {
        currentNode.nodeRole = ELECTION_ROLE_STANDBY;
    } else if (role == RoleType::AGENT) {
        currentNode.nodeRole = ELECTION_ROLE_AGENT;
    } else if (role == RoleType::MASTER) {
        currentNode.nodeRole = ELECTION_ROLE_MASTER;
    } else {
        UBSE_LOG_WARN << "[ELECTION] Current node role is initiator.";
        currentNode.nodeRole = ELECTION_ROLE_INITIALIZE;
    }
    return UBSE_OK;
}

uint32_t UbseGetNodeIds(std::vector<UBSE_ID_TYPE> &nodeIds)
{
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Getting the election module failed.";
        return UBSE_ERROR;
    }
    std::vector<Node> allNodes{};
    auto ret = electionModule->GetAllNodes(allNodes);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] Get all nodeIds failed";
        return ret;
    }
    for (const auto &node : allNodes) {
        nodeIds.push_back(node.id);
    }
    return UBSE_OK;
}

uint32_t UbseGetAllNodeStatusInfo(std::vector<UbseRoleInfo> &roleInfos)
{
    auto &ubseContext = ubse::context::UbseContext::GetInstance();
    auto electionModule = ubseContext.GetModule<ubse::election::UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Getting the election module failed.";
        return UBSE_ERROR;
    }
    std::vector<Node> allNodes{};
    std::vector<Node> allNodesCluster{};
    Node masterNode{};
    Node standbyNode{};

    // 获取所有物理节点信息
    uint32_t ret = electionModule->GetAllNodes(allNodes);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] Get All Nodes Failed";
        return ret;
    }

    // 获取所有集群节点信息
    ret = electionModule->UbseGetAllNodes(masterNode, standbyNode, allNodesCluster);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] Get All Nodes RoleInfos Failed";
        return ret;
    }
    allNodesCluster.push_back(masterNode);
    allNodesCluster.push_back(standbyNode);

    // 遍历所有物理节点，如果在集群节点中，则设置为在线，否则设置为离线
    for (const auto &node : allNodes) {
        auto it = std::find_if(allNodesCluster.begin(), allNodesCluster.end(),
            [&node](const Node &n) { return n.id == node.id; });
        if (it != allNodesCluster.end()) {
            if (it->id == masterNode.id) {
                roleInfos.emplace_back(node.id, ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
            } else if (it->id == standbyNode.id) {
                roleInfos.emplace_back(node.id, ELECTION_ROLE_STANDBY, ELECTION_NODE_ONLINE);
            } else {
                roleInfos.emplace_back(node.id, ELECTION_ROLE_AGENT, ELECTION_NODE_ONLINE);
            }
        } else {
            roleInfos.emplace_back(node.id, ELECTION_ROLE_INITIALIZE, ELECTION_NODE_OFFLINE);
        }
    }

    return ret;
}

uint32_t UbseGetRole(std::string &role)
{
    UbseRoleInfo currentNode;
    auto ret = UbseGetCurrentNodeInfo(currentNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetRole failed, " << FormatRetCode(ret);
        return ret;
    }
    role = currentNode.nodeRole;
    return ret;
}

uint32_t UbseGetMasterNodeId(std::string &masterNodeId)
{
    UbseRoleInfo roleInfo;
    auto ret = UbseGetMasterInfo(roleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetMasterNodeId failed, " << FormatRetCode(ret);
        return ret;
    }
    masterNodeId = roleInfo.nodeId;
    return ret;
}

uint32_t UbseGetCurrentNodeId(std::string &currentNodeId)
{
    UbseRoleInfo currentNode;
    auto ret = UbseGetCurrentNodeInfo(currentNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "GetMasterNodeId failed, " << FormatRetCode(ret);
        return ret;
    }
    currentNodeId = currentNode.nodeId;
    return ret;
}
}