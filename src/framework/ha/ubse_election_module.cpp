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

#include "ubse_election_module.h"
#include "ubse_module.h"

#include <chrono>
#include "config.h"
#include "role/ubse_election_role_mgr.h"
#include "ubse_common_def.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election_pkt_handler.h"
#include "ubse_lcne_module.h"
#include "ubse_net_util.h"
#include "ubse_node_mgr.h"
#include "ubse_node_mgr_module.h"

namespace ubse::election {
using namespace ubse::context;
using namespace ubse::com;
using namespace ubse::config;
using namespace ubse::nodeController;
using namespace ubse::nodeMgr;

UBSE_DEFINE_THIS_MODULE("ubse");
OPTIONAL_MODULE_IMPL(UbseElectionModule, UbseComModule, mti::UbseLcneModule, UbseNodeMgrModule);

UbseResult UbseElectionModule::Initialize()
{
    if (IsUrma()) {
        ApplyUrmaDev();
    }
    return UBSE_OK;
}

UbseResult UbseElectionModule::GetNodeIpInfoById(const std::string &id, std::string &ip)
{
    return UbseElectionNodeMgr::GetInstance().GetNodeIpById(id, ip);
}

void UbseElectionModule::TimerTaskElection()
{
    while (!g_globalStop.load()) {
        std::unique_lock<std::mutex> lock(timerTaskElectionMtx_);
        RoleMgr::GetInstance().ProcTimer();
        if (g_globalCv.wait_for(lock, std::chrono::milliseconds(NO_500), [] { return g_globalStop.load(); })) {
            std::cout << "[ELECTION] Stopped during the waiting period for ProcTimer." << std::endl;
            break;
        }
    }
    UBSE_LOG_INFO << "[ELECTION] TimerTaskElection over";
}
void UbseElectionModule::TimerTaskCom()
{
    while (!g_globalStop.load()) {
        std::unique_lock<std::mutex> lock(timerTaskComMtx_);
        auto role = RoleMgr::GetInstance().GetRole()->GetRoleType();
        if (role != RoleType::AGENT) {
            ConnectAllNodes();
        }
        if (g_globalCv.wait_for(lock, std::chrono::seconds(NO_1), [] { return g_globalStop.load(); })) {
            std::cout << "[ELECTION] Stopped the interval while waiting to establish a heartbeat." << std::endl;
            break;
        }
    }
}

UbseResult UbseElectionModule::Start()
{
    UBSE_LOG_INFO << "[ELECTION] UbseElectionModule::Start().";
    // 加载单例，并判断是否加载成功
    auto commMgr = RoleMgr::GetInstance().GetCommMgr();
    if (!commMgr) {
        UBSE_LOG_ERROR << "[ELECTION] CommMgr is null";
        return UBSE_ERROR;
    }
    // 开启事件订阅通知
    UbseResult result = commMgr->Start();
    if (result != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] election CommMgr Start failed";
        return UBSE_ERROR;
    }
    // 注册接受处理函数
    auto ret = UbseElectionPktHandler::RegElectionPktHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] UbseElectionModule handler error";
        return UBSE_ERROR;
    }
    auto currentRole = RoleMgr::GetInstance().GetRole();
    if (!currentRole) {
        UBSE_LOG_ERROR << "[ELECTION] currentRole is null";
        return UBSE_ERROR;
    }
    auto taskExecutorModule = UbseContext::GetInstance().GetModule<ubse::task_executor::UbseTaskExecutorModule>();
    if (taskExecutorModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    ret = taskExecutorModule->Create(ELECTION_TASK_EXECUTOR_NAME, NO_4, NO_7);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Create election task executor failed";
        return ret;
    }
    ret = taskExecutorModule->Create(ELECTION_GLOBAL_TASK_EXECUTOR_NAME, NO_4, NO_7);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Create election global task executor failed";
        return ret;
    }
    // UT 场景下，启用下面两个线程，会导致 UT 程序无法正常退出
#if !defined(ENABLE_UBSE_TESTING) || ENABLE_UBSE_TESTING != 1
    threads_.emplace_back(&UbseElectionModule::TimerTaskElection, this);
    threads_.emplace_back(&UbseElectionModule::TimerTaskCom, this);
#endif
    return UBSE_OK;
}

void UbseElectionModule::Stop()
{
    UBSE_LOG_DEBUG << "[ELECTION] UbseElectionModule::Stop - start.";
    for (auto &th : threads_) {
        if (th.joinable()) {
            th.join();
        }
    }
    auto taskExecutorModule = UbseContext::GetInstance().GetModule<ubse::task_executor::UbseTaskExecutorModule>();
    if (taskExecutorModule != nullptr) {
        taskExecutorModule->Remove(ELECTION_TASK_EXECUTOR_NAME);
        taskExecutorModule->Remove(ELECTION_GLOBAL_TASK_EXECUTOR_NAME);
    }
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule != nullptr) {
        ubseComModule->StopComService();
    }
    UBSE_LOG_DEBUG << "[ELECTION] UbseElectionModule::Stop - end.";
}

void UbseElectionModule::UnInitialize() {}

UbseResult UbseElectionModule::UbseGetMasterNode(Node &masterNode)
{
    auto& roleMgr = RoleMgr::GetInstance();
    auto& electionNodeMgr = UbseElectionNodeMgr::GetInstance();

    auto role = roleMgr.GetRole();
    if (!role) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get RoleMgrInstance";
        return UBSE_ERROR;
    }

    // 根据选举模式获取 Master ID
    UBSE_ID_TYPE masterId;
    if (electionNodeMgr.IsHierarchicalElection()) {
        masterId = role->GetGlobalMasterNode();
    } else {
        masterId = role->GetMasterNode();
    }

    if (masterId.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] Master node ID is empty";
        return UBSE_ERROR;
    }

    // 填充 masterNode 信息
    masterNode.id = masterId;
    auto nodeInfo = ubse::nodeMgr::GetUbseNodeById(masterId);

    bool ubEnable = true;
    electionNodeMgr.GetUBEnable(ubEnable);

    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_WARN << "[ELECTION] Master nodeId=" << masterId << " not found in currentAllNodes";
        masterNode.ip = NODE_IP_NULL;
        masterNode.port = NODE_PORT_NULL;
    } else {
        masterNode.ip = ubEnable ? nodeInfo.bonding0Eid : nodeInfo.addr;
        masterNode.port = TCP_LISTEN_PORT;
    }

    return UBSE_OK;
}

UbseResult UbseElectionModule::UbseGetStandbyNode(Node &standbyNode)
{
    auto& roleMgr = RoleMgr::GetInstance();
    auto& electionNodeMgr = UbseElectionNodeMgr::GetInstance();

    auto role = roleMgr.GetRole();
    if (!role) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get RoleMgrInstance";
        return UBSE_ERROR;
    }

    // 根据选举模式获取 Standby ID
    UBSE_ID_TYPE standbyId;
    if (electionNodeMgr.IsHierarchicalElection()) {
        standbyId = role->GetGlobalStandbyNode();
    } else {
        standbyId = role->GetStandbyNode();
    }

    if (standbyId.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] Standby node ID is empty";
        return UBSE_ERROR;
    }

    // 填充 standbyNode 信息
    standbyNode.id = standbyId;
    auto nodeInfo = ubse::nodeMgr::GetUbseNodeById(standbyId);

    bool ubEnable = true;
    electionNodeMgr.GetUBEnable(ubEnable);

    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_WARN << "[ELECTION] Standby nodeId=" << standbyId << " not found in currentAllNodes";
        standbyNode.ip = NODE_IP_NULL;
        standbyNode.port = NODE_PORT_NULL;
    } else {
        standbyNode.ip = ubEnable ? nodeInfo.bonding0Eid : nodeInfo.addr;
        standbyNode.port = TCP_LISTEN_PORT;
    }

    return UBSE_OK;
}

UbseResult UbseElectionModule::GetLocalMasterNode(Node &localMasterNode)
{
    UbseElectionNodeMgr &ubseElectionNodeMgr = UbseElectionNodeMgr::GetInstance();
    auto role = RoleMgr::GetInstance().GetRole();
    if (!role) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get RoleMgrInstance";
        return UBSE_ERROR;
    }
    localMasterNode.id = role->GetMasterNode();
    if (localMasterNode.id.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] masterNode Id is Empty";
        return UBSE_ERROR;
    }
    auto ret = ubseElectionNodeMgr.GetNodeInfoByID(localMasterNode.id, localMasterNode.ip, localMasterNode.port);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] master nodeId =" << localMasterNode.id << " not in currentAllNodes.";
        localMasterNode.ip = NODE_IP_NULL;
        localMasterNode.port = NODE_PORT_NULL;
    }
    return UBSE_OK;
}

UbseResult UbseElectionModule::GetLocalStandbyNode(Node &localStandbyNode)
{
    UbseElectionNodeMgr &ubseElectionNodeMgr = UbseElectionNodeMgr::GetInstance();
    auto role = RoleMgr::GetInstance().GetRole();
    if (!role) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get RoleMgrInstance";
        return UBSE_ERROR;
    }
    localStandbyNode.id = role->GetStandbyNode();
    if (localStandbyNode.id.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] standbyNode Id is Empty";
        return UBSE_ERROR;
    }
    auto ret = ubseElectionNodeMgr.GetNodeInfoByID(localStandbyNode.id, localStandbyNode.ip, localStandbyNode.port);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] standby nodeId =" << localStandbyNode.id << " not in currentAllNodes.";
        localStandbyNode.ip = NODE_IP_NULL;
        localStandbyNode.port = NODE_PORT_NULL;
    }
    return UBSE_OK;
}

UbseResult UbseElectionModule::UbseGetAllNodes(Node &master, Node &standby, std::vector<Node> &agent)
{
    UbseElectionNodeMgr &ubseElectionNodeMgr = UbseElectionNodeMgr::GetInstance();
    std::vector<Node> allNodes;
    ubseElectionNodeMgr.GetAllNode(allNodes);
    auto role = RoleMgr::GetInstance().GetRole();
    if (!role) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get RoleMgrInstance";
        return UBSE_ERROR;
    }
    master.id = role->GetMasterNode();
    standby.id = role->GetStandbyNode();
    std::vector<UBSE_ID_TYPE> agentNodes = role->GetAgentNodes();
    auto ret = ubseElectionNodeMgr.GetNodeInfoByID(master.id, master.ip, master.port);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get nodeInfo by master nodeID: " << master.id;
        return ret;
    }
    ubseElectionNodeMgr.GetNodeInfoByID(standby.id, standby.ip, standby.port);

    for (auto &node : allNodes) {
        if (std::find(agentNodes.begin(), agentNodes.end(), node.id) != agentNodes.end()) {
            if (node.id != master.id && node.id != standby.id) {
                agent.push_back(node);
            }
        }
    }
    return UBSE_OK;
}

bool UbseElectionModule::IsLeader()
{
    Node currentNode;
    UBSE_ID_TYPE masterId;
    UbseElectionNodeMgr &ubseElectionNodeMgr = UbseElectionNodeMgr::GetInstance();
    UbseResult ret = ubseElectionNodeMgr.GetMyselfNode(currentNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Get myself nodeId failed";
        return false;
    }
    masterId = RoleMgr::GetInstance().GetRole()->GetMasterNode();
    if (masterId == currentNode.id) {
        return true;
    }

    return false;
}

UbseResult UbseElectionModule::GetCurrentNode(Node &currentNode)
{
    UbseResult ret = UbseElectionNodeMgr::GetInstance().GetMyselfNode(currentNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Get currentNode Failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseElectionModule::GetMasterStatus(uint8_t &status)
{
    status = RoleMgr::GetInstance().GetRole()->GetMasterStatus();
    return UBSE_OK;
}

UbseResult UbseElectionModule::GetStandbyStatus(uint8_t &status)
{
    status = RoleMgr::GetInstance().GetRole()->GetStandbyStatus();
    return UBSE_OK;
}

UbseResult UbseElectionModule::GetAllNodes(std::vector<Node> &allNodes)
{
    UbseResult result = UbseElectionNodeMgr::GetInstance().GetAllNode(allNodes);
    if (result == UBSE_ERROR) {
        UBSE_LOG_ERROR << "[ELECTION] Get allNodes failed.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseElectionModule::SwitchMasterFromStandby()
{
    auto role = RoleMgr::GetInstance().GetRole();
    if (!role) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get RoleMgrInstance";
        return;
    }
    if (role->GetRoleType() != RoleType::STANDBY) {
        UBSE_LOG_INFO << "[ELECTION] Current node role is not standby.";
        return;
    }
    if (!GetElectionCandidate()) {
        UBSE_LOG_INFO << "[ELECTION] Current node does not participate in the master election.";
        return;
    }
    RoleContext ctx;
    ctx.masterId = role->GetStandbyNode();
    ctx.standbyId = INVALID_NODE_ID;
    ctx.turnId = role->GetTurnId() + 1;
    ubse::context::UbseContext::GetInstance().SetWorkReadiness(NOT_READY);
    RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
    RoleMgr::GetInstance().RoleChangeNotifyAsync(UbseElectionEventType::STANDBY_CHANGE_TO_MASTER, ctx.masterId);
}

void UbseElectionModule::SwitchAgentFromMaster()
{
    auto role = RoleMgr::GetInstance().GetRole();
    if (!role) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get RoleMgrInstance";
        return;
    }
    if (role->GetRoleType() != RoleType::MASTER) {
        UBSE_LOG_INFO << "[ELECTION] Current node role is not master.";
        return;
    }
    RoleContext ctx;
    ctx.masterId = role->GetStandbyNode();
    ctx.standbyId = INVALID_NODE_ID;
    ctx.turnId = role->GetTurnId();
    RoleMgr::GetInstance().SwitchRole(RoleType::AGENT, ctx);
}

UbseResult UbseElectionModule::GetHaTopologyInfo(HaTopologyInfo &haTopology)
{
    return UBSE_OK;
}
} // namespace ubse::election
