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

#include "ubse_election_role_initializer.h"
#include <chrono>
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election_node_mgr.h"
#include "ubse_election_role_mgr.h"
#include "ubse_node_controller.h"

namespace ubse::election {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::config;
using namespace ubse::nodeController;
Initializer::Initializer() : lastTimeMs_(0)
{
    auto ret = GetBootTime(startTimeMs_);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    lastTimeMs_ = startTimeMs_;
    auto currentNode = nodeMgr::GetCurrentNode();
    myselfID_ = currentNode.nodeId;
    UBSE_LOG_INFO << "[ELECTION] Initializer: " << myselfID_ << ".";
}
Initializer::~Initializer() {}

void Initializer::CheckAndSwitchMaster(const Node &myself, const std::vector<Node> &allNodes, RoleContext ctx)
{
    if ((lastTimeMs_ - startTimeMs_) <= GetHeartTimeInterval() * NO_2) {
        if (IsSmallestNode(myself, allNodes)) {
            // 阶段1内 最小ID的发起选组，没有拒绝就宣布为主
            if (SendElectionPkt(myselfID_, myselfIp_) == ELECTION_PKT_RESULT_ACCEPT) {
                UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch master - 1";
                RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
            }
        }
    } else if ((lastTimeMs_ - startTimeMs_) > GetHeartTimeInterval() * NO_2 &&
               (lastTimeMs_ - startTimeMs_) <= GetHeartTimeInterval() * NO_4) {
        if (IsSecondSmallestNode(myself, allNodes)) {
            // 阶段2内 最小ID的发起选组，没有拒绝就宣布为主
            if (SendElectionPkt(myselfID_, myselfIp_) == ELECTION_PKT_RESULT_ACCEPT) {
                UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch master - 2";
                RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
            }
        }
    } else {
        // 和接收互斥
        if (SendElectionPkt(myselfID_, myselfIp_) == ELECTION_PKT_RESULT_ACCEPT) {
            UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch master - 3";
            RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
        }
    }
}

void Initializer::ProcRoleSwitch(const std::vector<Node> &allNodes)
{
    RoleContext ctx;
    Node myself;
    ctx.masterId = myselfID_;
    ctx.turnId = 0;
    UbseElectionNodeMgr::GetInstance().GetMyselfNode(myself);
    auto ret = GetBootTime(lastTimeMs_);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    if (GetElectionCandidate() && GetElectionWait()) {
        CheckAndSwitchMaster(myself, allNodes, ctx);
    } else if (GetElectionCandidate() && !GetElectionWait()) {
        if (SendElectionPkt(myselfID_, myselfIp_) == ELECTION_PKT_RESULT_ACCEPT) {
            UBSE_LOG_INFO << "[ELECTION] Initializer: ProcTimer switch master - 4";
            RoleMgr::GetInstance().SwitchRole(RoleType::MASTER, ctx);
        }
    }
}

void Initializer::ProcTimer()
{
    std::vector<Node> electNodes;
    if (UbseElectionNodeMgr::GetInstance().GetLocalNodeState() != UbseNodeLocalState::UBSE_NODE_READY) {
        return;
    }
    if (UbseElectionNodeMgr::GetInstance().IsRootEnable()) {
        std::vector<std::string> rootList = nodeMgr::GetRootIpList();
        auto currentNode = nodeMgr::GetCurrentNode();
        myselfIp_ = currentNode.addr;
        if (std::find(rootList.begin(), rootList.end(), myselfIp_) == rootList.end()) {
            return;
        }
        // 指定根节点选主
        for (const auto& ip : rootList) {
            UBSE_ID_TYPE id;
            auto ret = UbseElectionNodeMgr::GetInstance().GetNodeIdByIp(ip, id);
            if (ret != UBSE_OK) {
                UBSE_LOG_WARN << "[ELECTION] GetNodeIdByIp failed, " << "ip=" << ip;
                continue;
            }
            electNodes.emplace_back(Node{id});
        }
    } else {
        UbseElectionNodeMgr::GetInstance().GetAllNode(electNodes);
    }
    UBSE_LOG_INFO << "[ELECTION] local node state is ready, start to elect.";
    if (!isStartTimeSet_) {
        auto ret = GetBootTime(startTimeMs_);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
        }
        isStartTimeSet_ = true;
    }
    ProcRoleSwitch(electNodes);
}

uint32_t Initializer::RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    if (UbseElectionNodeMgr::GetInstance().IsRootEnable()) {
        std::vector<std::string> rootList = nodeMgr::GetRootIpList();
        bool inRootList = std::find(rootList.begin(), rootList.end(), rcvPkt.masterIp) != rootList.end();
        if (!inRootList) {
            reply.replyId = myselfID_;
            reply.replyResult = ELECTION_PKT_TYPE_REJECT;
            return UBSE_OK;
        }
    }
    UbseNodeLocalState localState = UbseElectionNodeMgr::GetInstance().GetLocalNodeState();
    // 1. 节点处于恢复状态：仅处理 SELECT 和 HEART 的基本回应
    if (localState == UbseNodeLocalState::UBSE_NODE_RESTORE) {
        if (rcvPkt.type == ELECTION_PKT_TYPE_SELECT) {
            reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
        } else if (rcvPkt.type == ELECTION_PKT_TYPE_HEART) {
            reply.replyResult = ELECTION_PKT_TYPE_REJECT;
        }
        reply.replyId = myselfID_;
        return UBSE_OK;
    }

    if (localState != UbseNodeLocalState::UBSE_NODE_READY) {
        return UBSE_OK;
    }

    auto handleSelect = [&](bool canParticipate) {
        if (canParticipate) {
            // 可参与竞选：比较 masterId 决定接受或拒绝
            reply.replyResult = (myselfID_ < rcvPkt.masterId) ? ELECTION_PKT_TYPE_REJECT : ELECTION_PKT_RESULT_ACCEPT;
        } else {
            // 不可参与竞选：直接接受
            reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
        }
        reply.replyId = myselfID_;
    };

    auto handleHeart = [&]() {
        masterId_ = rcvPkt.masterId;
        turnId_ = rcvPkt.turnId;
        RoleContext ctx;
        ctx.masterId = masterId_;
        ctx.standbyId = rcvPkt.standbyId;
        ctx.turnId = turnId_;
        RoleType targetRole = (rcvPkt.standbyId == myselfID_) ? RoleType::STANDBY : RoleType::AGENT;
        RoleMgr::GetInstance().SwitchRole(targetRole, ctx);
        reply.replyId = myselfID_;
        reply.replyResult = 0;
    };

    bool electionCandidate = GetElectionCandidate();

    if (rcvPkt.type == ELECTION_PKT_TYPE_SELECT) {
        bool canParticipate = false;
        if (UbseElectionNodeMgr::GetInstance().IsRootEnable()) {
            std::vector<std::string> rootList = nodeMgr::GetRootIpList();
            auto currentNode = nodeMgr::GetCurrentNode();
            myselfIp_ = currentNode.addr;
            bool inRootList = std::find(rootList.begin(), rootList.end(), myselfIp_) != rootList.end();
            canParticipate = electionCandidate && inRootList;
        } else {
            canParticipate = electionCandidate;
        }
        handleSelect(canParticipate);
    } else if (rcvPkt.type == ELECTION_PKT_TYPE_HEART) {
        handleHeart();
    } else {
        UBSE_LOG_WARN << "[ELECTION] Initializer rcvPkt.type: " << rcvPkt.type << ".";
    }

    return UBSE_OK;
}

std::vector<UBSE_ID_TYPE> Initializer::GetAgentNodes()
{
    return {};
}

uint8_t Initializer::GetMasterStatus()
{
    return masterStatus_;
}

uint8_t Initializer::GetStandbyStatus()
{
    return standbyStatus_;
}
} // namespace ubse::election