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

#include "ubse_election_role_master.h"

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <csignal>
#include <thread>

#include "ubse_context.h"
#include "ubse_election_node_mgr.h"
#include "ubse_election_role_mgr.h"

namespace ubse::election {
using namespace ubse::context;
void SwitchStandbyNode(UBSE_ID_TYPE standbyId)
{
    RoleMgr::GetInstance().RoleChangeNotifyAsync(UbseElectionEventType::CHANGE_SWITCH_TO_STANDBY, standbyId);
}

void Master::InitNodesStatus(const std::vector<UBSE_ID_TYPE> &allNodes)
{
    std::vector<UBSE_ID_TYPE> allNeighbourNodes{};
    GetAllNeighbourNode(allNeighbourNodes);
    std::map<UBSE_ID_TYPE, BroadcastStatus> result;
    for (const auto &nodeId : allNeighbourNodes) {
        broadcast_[nodeId] = BroadcastStatus::Default();
    }
}

Master::Master(RoleContext &ctx) : turnId(0), sequenceId(), workStatus(IS_READY)
{
    Node myself;
    if (UBSE_ERROR == UbseElectionNodeMgr::GetInstance().GetMyselfNode(myself)) {
        UBSE_LOG_ERROR << "[ELECTION] Master GetMyselfNode: no node found.";
        return;
    }
    masterId = myself.id;
    standbyId = ctx.standbyId;
    std::vector<UBSE_ID_TYPE> allNodes = RoleMgr::GetInstance().GetCommMgr()->GetConnectedNodes();
    turnId = ctx.turnId + 1;
    sequenceId = 0;
    InitNodesStatus(allNodes);
    UBSE_LOG_INFO << "[ELECTION] Master start ProcTimer: " << masterId << ".";
}

std::vector<UBSE_ID_TYPE> Master::GetAllAgentIDs()
{
    std::vector<UBSE_ID_TYPE> result;

    for (const auto &node : broadcast_) {
        if (node.second.activeStatus == HeartBeatState::ACTIVE && node.first != masterId && node.first != standbyId) {
            result.push_back(node.first);
        }
    }

    return result;
}

void Master::DealBroadcast(ElectionReplyPkt &reply, const UBSE_ID_TYPE &id)
{
    if (reply.replyResult == ELECTION_PKT_RESULT_ACCEPT) {
        broadcast_[id].heartBeatLossCnt = 0;
        broadcast_[id].activeStatus = HeartBeatState::ACTIVE;
        if (reply.broadcast == 1) {
            broadcast_[reply.replyId].masterOnlineBcStatus = NotifyStatus::BROADCAST;
        } else {
            broadcast_[reply.replyId].masterOnlineBcTimes += 1;
        }
    } else {
        DealHbCnt(id);
    }
}

void Master::DealHbCnt(const UBSE_ID_TYPE &id)
{
    broadcast_[id].heartBeatLossCnt++;
    if (broadcast_[id].heartBeatLossCnt >= GetHbLostTimes()) {
        broadcast_[id].activeStatus = HeartBeatState::LOST;
        broadcast_[id].masterOnlineBcStatus = NotifyStatus::NOT_BROADCAST;
        broadcast_[id].masterOnlineBcTimes = 0;
    }
}

void Master::DealNodeUpdate()
{
    std::vector<UBSE_ID_TYPE> addNodes;
    std::vector<UBSE_ID_TYPE> removeNodes;
    std::vector<UBSE_ID_TYPE> currentNodes = GetActiveNodes();
    std::sort(currentNodes.begin(), currentNodes.end());
    std::sort(preNodes_.begin(), preNodes_.end());
    // 若没有节点变动，直接返回
    if (currentNodes == preNodes_) {
        return;
    }

    // 找到增加的元素
    for (const auto &nodeId : currentNodes) {
        if (std::find(preNodes_.begin(), preNodes_.end(), nodeId) == preNodes_.end()) {
            addNodes.push_back(nodeId);
        }
    }

    // 找到删除的元素
    for (const auto &nodeId : preNodes_) {
        if (std::find(currentNodes.begin(), currentNodes.end(), nodeId) == currentNodes.end()) {
            removeNodes.push_back(nodeId);
        }
    }

    for (const auto &nodeId : addNodes) {
        UBSE_LOG_INFO << "[ELECTION] Master NodeAdded: " << nodeId;
        RoleMgr::GetInstance().RoleChangeNotifyAsync(UbseElectionEventType::NODE_UP, nodeId);
    }

    for (const auto &nodeId : removeNodes) {
        UBSE_LOG_INFO << "[ELECTION] Master NodeRemoved: " << nodeId;
        RoleMgr::GetInstance().RoleChangeNotifyAsync(UbseElectionEventType::NODE_DOWN, nodeId);
    }
}

void Master::PrepareElectionPkt(ElectionPkt &pkt)
{
    pkt.type = ELECTION_PKT_TYPE_HEART;
    pkt.masterId = masterId;
    pkt.standbyId = standbyId;
    pkt.turnId = turnId;
    pkt.sequenceId = sequenceId;
    pkt.agentIds = GetAllAgentIDs();
    pkt.agentCount = pkt.agentIds.size();
    auto currentStatus = UbseContext::GetInstance().GetWorkReadiness();
    pkt.masterStatus = currentStatus;
    pkt.standbyStatus = standbyStatus;
}

void Master::UpdateStandbyNode(ElectionPkt &pkt, ElectionReplyPkt &reply)
{
    if (standbyId != INVALID_NODE_ID) {
        UBSE_LOG_DEBUG << "[ELECTION] ProcTimer MASTER send pkt standby id is: " << standbyId;
        pkt.broadcast = 1; // 第一次给备发心跳不触发master上线事件通知
        auto ret = RoleMgr::GetInstance().GetCommMgr()->SendElectionPkt(standbyId, pkt, reply);
        if (ret != UBSE_OK || reply.replyResult != ELECTION_PKT_RESULT_ACCEPT) {
            broadcast_[standbyId].heartBeatLossCnt++;
        } else {
            broadcast_[standbyId].heartBeatLossCnt = 0;
            standbyStatus = reply.standbyStatus;
        }
    }
}

void Master::ReplaceStandbyNode(ElectionPkt &pkt)
{
    if (standbyId != INVALID_NODE_ID && broadcast_[standbyId].heartBeatLossCnt >= GetHbLostTimes()) {
        // 更换备节点
        UBSE_ID_TYPE smallestId = FindSmallestIdExcludingMasterAndAgent(GetActiveNodes(), masterId, standbyId);
        standbyId = smallestId;
        pkt.standbyId = standbyId;
        if (standbyId != INVALID_NODE_ID) {
            SwitchStandbyNode(standbyId);
        }
    }
}

void Master::GetAllNeighbourNode(std::vector<UBSE_ID_TYPE> &allNodes)
{
    std::vector<Node> neighbourNodes{};
    auto result = UbseElectionNodeMgr::GetInstance().GetAllNeighbourNode(neighbourNodes);
    if (result != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] GetAllNeighbourNodes failed.";
        return;
    }

    for (const auto &node :neighbourNodes) {
        allNodes.push_back(node.id);
    }
}

void Master::ProcTimer()
{
    uint64_t current;
    auto result = GetBootTime(current);
    if (result != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] GetBootTime fail";
    }
    if ((current - lastTimeMs) > GetHeartTimeInterval() && IsHeartBeatEnabled(heartBeatStatus)) {
        std::lock_guard<std::mutex> lock(mtx);  // 加锁
        ElectionPkt pkt;
        ElectionReplyPkt reply;

        if (standbyId == INVALID_NODE_ID) {
            standbyId = FindSmallestIdExcludingMaster(masterId, GetActiveNodes());
            if (standbyId != INVALID_NODE_ID) {
                UBSE_LOG_INFO << "[ELECTION] Master Appoint the standby node id: " << standbyId;
                SwitchStandbyNode(standbyId);
            }
        }
        PrepareElectionPkt(pkt);
        UpdateStandbyNode(pkt, reply);
        ReplaceStandbyNode(pkt);

        std::vector<UBSE_ID_TYPE> allNodes = RoleMgr::GetInstance().GetCommMgr()->GetConnectedNodes();
        for (const auto &id : allNodes) {
            UBSE_LOG_DEBUG << "[ELECTION] ProcTimer MASTER send pkt id is: " << id;
            pkt.broadcast = static_cast<uint8_t>(broadcast_[id].masterOnlineBcStatus);
            if (broadcast_[id].masterOnlineBcTimes >= 3) { // 3, 主上线事件最多通知三次
                pkt.broadcast = 1; // 1, 标志位设置为1, 已通知
            }
            auto ret = RoleMgr::GetInstance().GetCommMgr()->SendElectionPkt(id, pkt, reply);
            if (ret != UBSE_OK) {
                DealHbCnt(id);
                continue;
            }
            DealBroadcast(reply, id);
            standbyStatus = reply.standbyStatus;
        }
        DealNodeUpdate();
        preNodes_ = GetActiveNodes();
        sequenceId++;
        lastTimeMs = current;
    }
}

UBSE_ID_TYPE DeterminePartitionMaster(const UBSE_ID_TYPE &masterId, const std::vector<UBSE_ID_TYPE> &agentIds,
    const UBSE_ID_TYPE &partitionMasterId, const std::vector<UBSE_ID_TYPE> &partitionAgentIDs)
{
    // 比较成员数量
    if (agentIds.size() > partitionAgentIDs.size()) {
        return masterId;
    } else if (agentIds.size() < partitionAgentIDs.size()) {
        return partitionMasterId;
    } else {
        // 成员数量相同，比较 Master 的 ID
        return masterId < partitionMasterId ? masterId : partitionMasterId;
    }
}

void Master::HandleSplitBrainMerge(const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    // 脑裂合并
    std::vector<UBSE_ID_TYPE> agentIds = GetAllAgentIDs();
    if (standbyId != INVALID_NODE_ID) {
        agentIds.push_back(standbyId);
    }
    UBSE_ID_TYPE partitionMasterId = rcvPkt.masterId;
    std::vector<UBSE_ID_TYPE> partitionAgentIDs = rcvPkt.agentIds;
    if (rcvPkt.standbyId != INVALID_NODE_ID) {
        partitionAgentIDs.push_back(rcvPkt.standbyId);
    }
    UBSE_ID_TYPE newMasterId = DeterminePartitionMaster(masterId, agentIds, partitionMasterId, partitionAgentIDs);
    if (newMasterId == masterId) {
        // 收编
        reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
        reply.masterId = masterId;
    } else {
        // 被收编
        RoleContext ctx;
        ctx.masterId = rcvPkt.masterId;
        ctx.standbyId = rcvPkt.standbyId;
        ctx.turnId = rcvPkt.turnId;
        RoleMgr::GetInstance().SwitchRole(RoleType::AGENT, ctx);
        reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
    }
}

uint32_t Master::RecvPktHeart(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    if (rcvPkt.turnId > turnId) {
        if (rcvPkt.standbyId == masterId) {
            RoleContext ctx;
            ctx.masterId = rcvPkt.masterId;
            ctx.turnId = rcvPkt.turnId;
            ctx.standbyId = masterId;
            RoleMgr::GetInstance().SwitchRole(RoleType::STANDBY, ctx);
        } else {
            RoleContext ctx;
            ctx.masterId = rcvPkt.masterId;
            ctx.turnId = rcvPkt.turnId;
            RoleMgr::GetInstance().SwitchRole(RoleType::AGENT, ctx);
        }
        reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
    } else if (rcvPkt.turnId == turnId) {
        HandleSplitBrainMerge(rcvPkt, reply);
    } else {
        reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
    }

    return 0;
}

uint32_t Master::RecvPktElection(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
    reply.replyId = masterId;
    reply.masterId = masterId;
    reply.turnId = turnId;
    return UBSE_OK;
}

uint32_t Master::RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    // 主收到心跳 两种场景，1：主假死后恢复 2：脑裂合并
    if (rcvPkt.type == ELECTION_PKT_TYPE_HEART && IsHeartBeatEnabled(heartBeatStatus)) {
        RecvPktHeart(srcID, rcvPkt, reply);
    } else if (rcvPkt.type == ELECTION_PKT_TYPE_SELECT) {
        RecvPktElection(srcID, rcvPkt, reply);
    }
    return 0;
}

UBSE_ID_TYPE Master::GetMasterNode()
{
    Node myself;
    UbseResult result = UbseElectionNodeMgr::GetInstance().GetMyselfNode(myself);
    if (result != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] Invalid local master node.";
        return INVALID_NODE_ID;
    }
    return myself.id;
}

UBSE_ID_TYPE Master::GetStandbyNode()
{
    return standbyId;
}

std::vector<UBSE_ID_TYPE> Master::GetAgentNodes()
{
    return GetActiveNodes();
}

uint8_t Master::GetMasterStatus()
{
    auto currentStatus = UbseContext::GetInstance().GetWorkReadiness();
    return currentStatus;
}

uint8_t Master::GetStandbyStatus()
{
    return standbyStatus;
}

std::vector<UBSE_ID_TYPE> Master::GetActiveNodes()
{
    std::vector<UBSE_ID_TYPE> activeNodes{};
    for (const auto &node : broadcast_) {
        if (node.second.activeStatus == HeartBeatState::ACTIVE) {
            activeNodes.push_back(node.first);
        }
    }
    return activeNodes;
}

void Master::SetNodeDownStatus(UBSE_ID_TYPE nodeId)
{
    std::lock_guard<std::mutex> lock(mtx);  // 加锁
    if (broadcast_[nodeId].activeStatus == HeartBeatState::ACTIVE) {
        UBSE_LOG_INFO << "[ELECTION] Master NodeRemoved: " << nodeId;
        RoleMgr::GetInstance().RoleChangeNotifyAsync(UbseElectionEventType::NODE_DOWN, nodeId);
        broadcast_[nodeId].activeStatus = HeartBeatState::LOST;
        broadcast_[nodeId].masterOnlineBcStatus = NotifyStatus::NOT_BROADCAST;
        broadcast_[nodeId].masterOnlineBcTimes = 0;
        preNodes_ = GetActiveNodes();
    }
    if (nodeId == standbyId) {
        standbyId = INVALID_NODE_ID;
    }
}
}