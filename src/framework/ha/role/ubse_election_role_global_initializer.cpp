/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_election_role_global_initializer.h"
#include "ubse_election_role_mgr.h"
#include "ubse_timer.h"

namespace ubse::election {
const std::string UBSE_ELECTION_GLOBAL_INIT_PROCTIMER = "UbseGlobalInitProcTimer";
const std::string UBSE_ELECTION_INIT_DISCOVERY_TIMER = "UbseInitDiscoveryTimer";
const std::string UBSE_ELECTION_GLOBAL_INIT_COM = "UbseGlobalInitComTimer";
const std::string UBSE_ELECTION_INIT_PD_QUERY = "UbseInitPdQueryTimer";
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::timer;
using namespace ubse::context;
using namespace ubse::nodeController;
GlobalInitializer::GlobalInitializer() : lastTimeMs_(0)
{
    auto ret = GetBootTime(startTimeMs_);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    lastTimeMs_ = startTimeMs_;
    Node myself;
    UbseElectionNodeMgr::GetInstance().GetMyselfNode(myself);
    myselfID_ = myself.id;
    ret = UbseElectionNodeMgr::GetInstance().GetGroupId(groupId_);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] GetGroupId fail";
        return;
    }
    RegisterTimers();
    UBSE_LOG_INFO << "[ELECTION] Global Initializer: " << myselfID_ << ".";
}
GlobalInitializer::~GlobalInitializer()
{
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_INIT_PROCTIMER);
    UbseTimerHandlerUnregister(UBSE_ELECTION_INIT_DISCOVERY_TIMER);
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_INIT_COM);
    UbseTimerHandlerUnregister(UBSE_ELECTION_INIT_PD_QUERY);
}

void GlobalInitializer::RegisterTimers()
{
    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_INIT_PROCTIMER,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            RoleMgr::GetInstance().GlobalProcTimer();
            return UBSE_OK;
        }, UBSE_GLOBAL_PROC_INTERVAL);
    UbseTimerHandlerRegister(
        UBSE_ELECTION_INIT_DISCOVERY_TIMER,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
            if (globalRole != nullptr) { RoleMgr::GetInstance().ConnectInterManagingGroup(); }
            return UBSE_OK;
        }, UBSE_GLOBAL_DISCOVERY_INTERVAL);
    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_INIT_COM,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            UBSE_ID_TYPE groupId;
            auto ret = UbseElectionNodeMgr::GetInstance().GetGroupId(groupId);
            if (ret != UBSE_OK) { return UBSE_ERROR; }
            auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
            if (globalRole != nullptr && globalRole->GetGlobalRoleType() != GlobalRoleType::GLOBAL_AGENT) {
                ConnectManagingMasters();
            }
            return UBSE_OK;
        }, UBSE_GLOBAL_COM_INTERVAL);
    UbseTimerHandlerRegister(
        UBSE_ELECTION_INIT_PD_QUERY,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
            if (globalRole != nullptr) { RoleMgr::GetInstance().QueryManagingMaster(); }
            return UBSE_OK;
        }, UBSE_GLOBAL_QUERY_LOCAL_MASTER_INTERVAL);
}

void GlobalInitializer::CheckAndSwitchMaster(const Node &myself, const std::vector<Node> &masterIds, RoleContext ctx)
{
    if ((lastTimeMs_ - startTimeMs_) <= GetHeartTimeInterval() * NO_2) {
        if (IsSmallestNode(myself, masterIds)) {
            // 阶段1内 最小ID的发起选组，没有拒绝就宣布为主
            if (SendGlobalElectionPkt(myselfID_) == ELECTION_PKT_RESULT_ACCEPT) {
                UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch global master - 1";
                RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_MASTER, ctx);
            }
        }
    } else if ((lastTimeMs_ - startTimeMs_) > GetHeartTimeInterval() * NO_2 &&
               (lastTimeMs_ - startTimeMs_) <= GetHeartTimeInterval() * NO_4) {
        if (IsSecondSmallestNode(myself, masterIds)) {
            // 阶段2内 最小ID的发起选组，没有拒绝就宣布为主
            if (SendGlobalElectionPkt(myselfID_) == ELECTION_PKT_RESULT_ACCEPT) {
                UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch global master - 2";
                RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_MASTER, ctx);
            }
        }
    } else {
        // 和接收互斥
        if (SendGlobalElectionPkt(myselfID_) == ELECTION_PKT_RESULT_ACCEPT) {
            UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch global master - 3";
            RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_MASTER, ctx);
        }
    }
}

void GlobalInitializer::ProcRoleSwitch(const std::vector<Node> &masterIds)
{
    RoleContext ctx;
    ctx.masterId = myselfID_;
    ctx.turnId = 0;
    auto ret = GetBootTime(lastTimeMs_);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    if (GetElectionCandidate() && GetElectionWait()) {
        CheckAndSwitchMaster({myselfID_}, masterIds, ctx);
    } else if (GetElectionCandidate() && !GetElectionWait()) {
        if (SendGlobalElectionPkt(myselfID_) == ELECTION_PKT_RESULT_ACCEPT) {
            UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch master - 4";
            RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_MASTER, ctx);
        }
    }
}

void GlobalInitializer::ProcTimer()
{
    // 挂载组不参与全局选主
    if (!RoleMgr::GetInstance().IsManagingGroup(groupId_)) {
        UBSE_LOG_INFO << "[ELECTION] The group do not participate in global election. The groupId is " << groupId_;
        return;
    }

    std::vector<UBSE_ID_TYPE> pdMasterIds = RoleMgr::GetInstance().GetManagingGroupMasterIds();
    if (pdMasterIds.empty()) {
        UBSE_LOG_DEBUG << "[ELECTION] ManagingGroup MasterIds is empty.";
        return;
    }

    // pd num >= 5 尝试建链一轮 再进行全局选主
    if (!hasConnMasterNodesOnce_) {
        ConnectManagingMasters();
        hasConnMasterNodesOnce_ = true;
    }

    UbseNodeLocalState localState = UbseElectionNodeMgr::GetInstance().GetLocalNodeState();
    if (localState == UbseNodeLocalState::UBSE_NODE_READY) {
        std::vector<Node> masterIds{};
        for (const auto& item : pdMasterIds) {
            masterIds.push_back({item});
            UBSE_LOG_INFO << "[ELECTION] query Managing Group masterId = " << item;
        }
        masterIds.push_back({myselfID_});
        if (!isStartTimeSet_) {
            auto ret = GetBootTime(startTimeMs_);
            if (ret != UBSE_OK) {
                UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
            }
            isStartTimeSet_ = true;
        }
        ProcRoleSwitch(masterIds);
    }
}

uint32_t GlobalInitializer::RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    UbseNodeLocalState localState = UbseElectionNodeMgr::GetInstance().GetLocalNodeState();
    if (localState == UbseNodeLocalState::UBSE_NODE_RESTORE) {
        if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_SELECT) {
            reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
        } else if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_HEART) {
            reply.replyResult = ELECTION_PKT_TYPE_REJECT;
        }
        reply.replyId = myselfID_;
    } else if (localState == UbseNodeLocalState::UBSE_NODE_READY) {
        if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_SELECT && GetElectionCandidate()) {
            if (myselfID_ < rcvPkt.masterId) {
                reply.replyResult = ELECTION_PKT_TYPE_REJECT;
            } else {
                reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
            }
            reply.replyId = myselfID_;
        } else if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_SELECT && !GetElectionCandidate()) {
            reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
            reply.replyId = myselfID_;
        } else if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_HEART) {
            globalTurnId_ = rcvPkt.turnId;
            RoleContext ctx;

            ctx.masterId = rcvPkt.masterId;
            ctx.standbyId = rcvPkt.standbyId;
            ctx.turnId = globalTurnId_;

            if (rcvPkt.standbyId == myselfID_) {
                RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_STANDBY, ctx);
            } else {
                RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_AGENT, ctx);
            }
            reply.replyId = myselfID_;
            reply.replyResult = 0;
        } else {
            UBSE_LOG_WARN << "[ELECTION] Global Initializer rcvPkt.type: " << rcvPkt.type << ".";
        }
    }
    return UBSE_OK;
}

UBSE_ID_TYPE GlobalInitializer::GetMasterNode()
{
    return INVALID_NODE_ID;
}

UBSE_ID_TYPE GlobalInitializer::GetStandbyNode()
{
    return INVALID_NODE_ID;
}

std::vector<UBSE_ID_TYPE> GlobalInitializer::GetAgentNodes()
{
    return {};
}

uint8_t GlobalInitializer::GetMasterStatus()
{
    return masterStatus_;
}

uint8_t GlobalInitializer::GetStandbyStatus()
{
    return standbyStatus_;
}
}
