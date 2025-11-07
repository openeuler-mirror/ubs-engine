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
#include <chrono>
#include <thread>
#include "ubse_common_def.h"
#include "config.h"
#include "role/ubse_election_role_mgr.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election_pkt_handler.h"
#include "ubse_node_controller_module.h"

namespace ubse::election {
using namespace ubse::context;
using namespace ubse::com;
using namespace ubse::config;
using namespace ubse::nodeController;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_ELECTION_MID);
DYNAMIC_CREATE(UbseElectionModule, UbseNodeControllerModule);

UbseResult UbseElectionModule::Initialize()
{
    return UBSE_OK;
}

void UbseElectionModule::TimerTaskElection()
{
    while (!g_globalStop.load()) {
        std::unique_lock<std::mutex> lock(timerTaskElectionMtx_, std::defer_lock);
        if (lock.try_lock()) {
            RoleMgr::GetInstance().ProcTimer();
        } else {
            break;
        }
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
        std::unique_lock<std::mutex> lock(timerTaskComMtx_, std::defer_lock);
        if (lock.try_lock()) {
            ConnectAllNodes();
        } else {
            break;
        }
        if (g_globalCv.wait_for(lock, std::chrono::seconds(NO_1), [] { return g_globalStop.load(); })) {
            std::cout << "[ELECTION] Stopped the interval while waiting to establish a heartbeat." << std::endl;
            break;
        }
    }
    UBSE_LOG_INFO << "[ELECTION] TimerTaskCom over";
}

UbseResult UbseElectionModule::Start()
{
    UBSE_LOG_INFO << "[ELECTION] UbseElectionModule::Start().";

    // 注册接受处理函数
    auto ret = UbseElectionPktHandler::RegElectionPktHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] UbseElectionModule handler error";
    }

    // 初始化加载 LCNE 配置
    UbseElectionNodeMgr::GetInstance().LoadConfig();

    // 加载单例，并判断是否加载成功
    auto commMgr = RoleMgr::GetInstance().GetCommMgr();
    if (!commMgr) {
        UBSE_LOG_ERROR << "[ELECTION] CommMgr is null";
        return UBSE_ERROR;
    }

    auto currentRole = RoleMgr::GetInstance().GetRole();
    if (!currentRole) {
        UBSE_LOG_ERROR << "[ELECTION] currentRole is null";
        return UBSE_ERROR;
    }
    // 开启事件订阅通知
    UbseResult result = commMgr->Start();
    if (result != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] election CommMgr Start failed";
        return UBSE_ERROR;
    }

    auto taskExecutorModule = UbseContext::GetInstance().GetModule<ubse::task_executor::UbseTaskExecutorModule>();
    if (taskExecutorModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    ret = taskExecutorModule->Create(ELECTION_TASK_EXECUTOR_NAME, 7, 7); // 创建线程数为7的线程池
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Create election task executor failed";
        return ret;
    }

    // UT 场景下，启用下面两个线程，会导致 UT 程序无法正常退出
#if !defined(ENABLE_UBSE_TESTING) || ENABLE_UBSE_TESTING != 1
    ConnectAllNodes();
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
    }
    UBSE_LOG_DEBUG << "[ELECTION] UbseElectionModule::Stop - end.";
}

void UbseElectionModule::UnInitialize() {}

UbseResult UbseElectionModule::UbseGetMasterNode(Node &masterNode)
{
    UbseElectionNodeMgr &ubseElectionNodeMgr = UbseElectionNodeMgr::GetInstance();
    auto role = RoleMgr::GetInstance().GetRole();
    if (!role) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get RoleMgrInstance";
        return UBSE_ERROR;
    }
    masterNode.id = role->GetMasterNode();
    auto ret = ubseElectionNodeMgr.GetNodeInfoByID(masterNode.id, masterNode.ip, masterNode.port);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get nodeInfo by master nodeID: " << masterNode.id;
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseElectionModule::UbseGetStandbyNode(Node &standbyNode)
{
    UbseElectionNodeMgr &ubseElectionNodeMgr = UbseElectionNodeMgr::GetInstance();
    auto role = RoleMgr::GetInstance().GetRole();
    if (!role) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get RoleMgrInstance";
        return UBSE_ERROR;
    }
    standbyNode.id = role->GetStandbyNode();
    auto ret = ubseElectionNodeMgr.GetNodeInfoByID(standbyNode.id, standbyNode.ip, standbyNode.port);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get nodeInfo by standby nodeID: " << standbyNode.id;
        return ret;
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
    // 没有备是正常的，不判断结果
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
    RoleContext ctx;
    ctx.masterId = role->GetStandbyNode();
    ctx.standbyId = INVALID_NODE_ID;
    ctx.turnId = role->GetTurnId() + 1;
    ubse::context::UbseContext::GetInstance().SetWorkReadiness(NOT_READY);
    RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
    RoleMgr::GetInstance().RoleChangeNotifyAsync(UbseElectionEventType::STANDBY_CHANGE_TO_MASTER, ctx.masterId);
}
} // namespace ubse::election
