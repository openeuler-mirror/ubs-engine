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

#include <trace_context.h>

#include <algorithm>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "ubse_context.h"
#include "ubse_election_node_mgr.h"
#include "ubse_election_role_mgr.h"
#include "ubse_election_pkt_simpo.h"
#include "ubse_election_reply_pkt_simpo.h"
#include "ubse_election_def.h"

namespace ubse::election {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::context;
using namespace ubse::election::message;
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

Master::Master(RoleContext &ctx) : turnId_(0), sequenceId_(), workStatus_(IS_READY)
{
    Node myself;
    if (UBSE_ERROR == UbseElectionNodeMgr::GetInstance().GetMyselfNode(myself)) {
        UBSE_LOG_ERROR << "[ELECTION] Master GetMyselfNode: no node found.";
        masterId_ = INVALID_NODE_ID;
    } else {
        masterId_ = myself.id;
    }
    standbyId_ = ctx.standbyId;
    std::vector<UBSE_ID_TYPE> allNodes = RoleMgr::GetInstance().GetCommMgr()->GetConnectedNodes();
    turnId_ = ctx.turnId + 1;
    sequenceId_ = 0;
    InitNodesStatus(allNodes);
    auto ret = UbseElectionNodeMgr::GetInstance().GetGroupId(groupId_);
    if (ret != UBSE_OK || groupId_.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] GetGroupId fail";
    }
    UBSE_LOG_INFO << "[ELECTION] Master start ProcTimer: " << masterId_ << ".";
    stopping_ = false;
    activeCount_ = 0;
}

std::vector<UBSE_ID_TYPE> Master::GetAllAgentIDs()
{
    std::vector<UBSE_ID_TYPE> result;

    for (const auto &node : broadcast_) {
        if (node.second.activeStatus == HeartBeatState::ACTIVE && node.first != masterId_ && node.first != standbyId_) {
            result.push_back(node.first);
        }
    }

    return result;
}

void Master::DealHbCnt(const UBSE_ID_TYPE &id)
{
    broadcast_[id].heartBeatLossCnt++;
    if (broadcast_[id].heartBeatLossCnt > GetHbLostTimes()) {
        broadcast_[id].activeStatus = HeartBeatState::LOST;
        broadcast_[id].masterOnlineBcStatus = NotifyStatus::NOT_BROADCAST;
        broadcast_[id].masterOnlineBcTimes = 0;
        if (broadcast_[id].heartBeatLossCnt <= GetHbLostTimes()*NO_10 || broadcast_[id].heartBeatLossCnt % NO_15 ==0) {
            UBSE_LOG_WARN << "[ELECTION] nodeId=" << id << ", nodeStatus=" << int(broadcast_[id].activeStatus)
                          << ", heartBeatLossCnt=" << broadcast_[id].heartBeatLossCnt;
        }
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
    pkt.masterId = masterId_;
    pkt.standbyId = standbyId_;
    pkt.turnId = turnId_;
    pkt.sequenceId = sequenceId_;
    pkt.agentIds = GetAllAgentIDs();
    pkt.agentCount = pkt.agentIds.size();
    auto currentStatus = UbseContext::GetInstance().GetWorkReadiness();
    pkt.masterStatus = currentStatus;
    pkt.standbyStatus = standbyStatus_;
    pkt.globalMasterId = GetGlobalMasterNode();
    pkt.globalStandbyId = GetGlobalStandbyNode();
}

void Master::ReplaceStandbyNode(ElectionPkt &pkt)
{
    if (standbyId_ != INVALID_NODE_ID && broadcast_[standbyId_].heartBeatLossCnt >= GetHbLostTimes()) {
        // 更换备节点
        UBSE_ID_TYPE smallestId = FindSmallestIdExcludingMasterAndAgent(GetActiveNodes(), masterId_, standbyId_);
        standbyId_ = smallestId;
        pkt.standbyId = standbyId_;
        UBSE_LOG_INFO << "[ELECTION] Master Appoint the new standby nodeId=" << standbyId_;
        if (standbyId_ != INVALID_NODE_ID) {
            SwitchStandbyNode(standbyId_);
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
    if ((current - lastTimeMs_) > GetHeartTimeInterval()) {
        std::unique_lock<std::mutex> lock(mtx_);
        ElectionPkt pkt;
        ElectionReplyPkt reply;
        if (standbyId_ == INVALID_NODE_ID) {
            standbyId_ = FindSmallestIdExcludingMaster(masterId_, GetActiveNodes());
            if (standbyId_ != INVALID_NODE_ID) {
                UBSE_LOG_INFO << "[ELECTION] Master Appoint the standby node id=" << standbyId_;
                SwitchStandbyNode(standbyId_);
            }
        }
        PrepareElectionPkt(pkt);
        ReplaceStandbyNode(pkt);
        std::vector<UBSE_ID_TYPE> allNodes = RoleMgr::GetInstance().GetCommMgr()->GetConnectedNodes();
        for (const auto &id : allNodes) {
            UBSE_LOG_DEBUG << "[ELECTION] ProcTimer MASTER send pkt id=" << id;
            pkt.broadcast = static_cast<uint8_t>(broadcast_[id].masterOnlineBcStatus);
            {
                UnlockGuard unlockGuard(lock);
                auto ret = SendHeartBeat(id, pkt);
                if (ret != UBSE_OK) {
                    UBSE_LOG_ERROR << "[ELECTION] send heart to nodeId=" << id << " failed";
                }
            }
            DealHbCnt(id);
            TraceContext::Clear();
        }
        UBSE_LOG_DEBUG << "[ELECTION] ProcTimer MASTER send pkt finished ";
        DealNodeUpdate();
        preNodes_ = GetActiveNodes();
        sequenceId_++;
        lastTimeMs_ = current;
    }
}

void UpdateBroadcastStatus(const std::string &nodeId, const ElectionReplyPkt &reply,
                           std::map<UBSE_ID_TYPE, BroadcastStatus> &broad, uint8_t &status, std::mutex &mtx)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (reply.replyResult == ELECTION_PKT_RESULT_ACCEPT) {
        broad[nodeId].heartBeatLossCnt = 0;
        broad[nodeId].activeStatus = HeartBeatState::ACTIVE;
        if (reply.broadcast == 1) {
            broad[nodeId].masterOnlineBcStatus = NotifyStatus::BROADCAST;
        } else {
            broad[nodeId].masterOnlineBcTimes += 1;
        }
    }
    UBSE_LOG_DEBUG << "[ELECTION] nodeId=" << nodeId
                   << ", nodeStatus="<< int(broad[nodeId].activeStatus)
                   << ", heartBeatLossCnt=" << broad[nodeId].heartBeatLossCnt
                   << ", reply=" << reply.replyResult;
    status = reply.standbyStatus;
}

// 处理选举回复消息
void ProcessReply(CallbackCtx* context, int32_t result, void* recv, uint32_t len)
{
    // 从上下文提取所需参数
    const auto& nodeId = context->destId;
    auto& mtx = *context->mtx;
    auto& broad = *context->broadcast;
    auto& status = *context->standbyStatus;
    if (result != 0) {
        UBSE_LOG_ERROR << "[ELECTION] RpcSend dispatch failed : " << nodeId
                       << ", ErrorCode=" << result;
        return;
    }
    UbseBaseMessagePtr respMsg = new (std::nothrow) UbseElectionReplyPktSimpo();
    if (respMsg == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] new RackElectionReplyPktSimpo failed";
        return;
    }
    auto ret = respMsg->SetInputRawData(static_cast<uint8_t*>(recv), len);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] SetInputRawData failed, " << FormatRetCode(ret);
        return;
    }
    ret = respMsg->Deserialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] deserialize failed, " << FormatRetCode(ret);
        return;
    }
    auto* replyMsg = dynamic_cast<UbseElectionReplyPktSimpo*>(respMsg.Get());
    if (!replyMsg) {
        UBSE_LOG_ERROR << "[ELECTION] cast to RackElectionReplyPktSimpo failed";
        return;
    }
    ElectionReplyPkt reply = replyMsg->GetElectionReplyPkt();
    if (reply.replyResult == ELECTION_PKT_REPLY_GLOBAL_STOP) {
        UBSE_LOG_DEBUG << "[ELECTION] node = " << nodeId << " stopped";
        return;
    }
    UpdateBroadcastStatus(nodeId, reply, broad, status, mtx);
}

void AsyncDealReply(void* ctx, void* recv, uint32_t len, int32_t result)
{
    auto* context = static_cast<CallbackCtx*>(ctx);
    if (context == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Received null context in callback";
        return;
    }
    auto& stopping = *context->stopping;
    auto& activeCount = *context->activeCount;
    const auto& nodeId = context->destId;
    activeCount.fetch_add(1);
    if (stopping.load()) {
        UBSE_LOG_INFO << "[ELECTION] Master has stopped, skipping callback; nodeId=" << nodeId;
        activeCount.fetch_sub(1);
        SafeDelete(context);
        return;
    }
    UBSE_LOG_DEBUG << "[ELECTION] Asynchronous reply, nodeId=" << nodeId;
    ProcessReply(context, result, recv, len);
    activeCount.fetch_sub(1);
    SafeDelete(context);
}

uint32_t Master::SendHeartBeat(UBSE_ID_TYPE destID, const ElectionPkt &pkt)
{
    UbseContext &ubseContext = UbseContext::GetInstance();
    auto rackComModule = ubseContext.GetModule<UbseComModule>();
    if (rackComModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] get rackComModule failed";
        return UBSE_ERROR;
    }
    ElectionPkt electionPkt{ pkt };
    UbseBaseMessagePtr electionSimpoPtr = new (std::nothrow) UbseElectionPktSimpo(electionPkt);
    if (electionSimpoPtr == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Newing RackElectionPktSimpo failed.";
        return UBSE_ERROR;
    }
    ubse::com::SendParam sendParam(destID, static_cast<uint16_t>(UbseModuleCode::ELECTION),
                                   static_cast<uint16_t>(UbseElectionOpCode::ELECTION_PKT), UbseChannelType::NORMAL);
    auto context = new (std::nothrow) CallbackCtx;
    if (context == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] New context failed.";
        return UBSE_ERROR_NULLPTR;
    }
    std::unique_lock<std::mutex> lock(mtx_);
    context->broadcast = &broadcast_;
    context->destId = destID;
    context->standbyStatus = &standbyStatus_;
    context->mtx = &mtx_;
    context->stopping = &stopping_;
    context->activeCount = &activeCount_;
    ubse::com::UbseComCallback callback;
    callback.cb = AsyncDealReply;
    callback.cbCtx = reinterpret_cast<void *>(context);
    lock.unlock();
    auto retCode = rackComModule->RpcAsyncSend(sendParam, electionSimpoPtr, callback);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] RpcSend dispatch failed : " << destID;
        SafeDelete(context);
        return retCode;
    }
    return UBSE_OK;
}

void Master::HandleSplitBrainMerge(const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    // turnId相同，比较masterId大小
    UBSE_ID_TYPE newMasterId = masterId_ < rcvPkt.masterId ? masterId_ : rcvPkt.masterId;
    if (newMasterId == masterId_) {
        // 收编
        reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
        reply.masterId = masterId_;
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
    std::vector<UBSE_ID_TYPE> agentIds = GetAllAgentIDs();
    if (standbyId_ != INVALID_NODE_ID) {
        agentIds.push_back(standbyId_);
    }

    std::vector<UBSE_ID_TYPE> partitionAgentIDs = rcvPkt.agentIds;
    if (rcvPkt.standbyId != INVALID_NODE_ID) {
        partitionAgentIDs.push_back(rcvPkt.standbyId);
    }

    std::vector<Node> allNodes{};
    UbseResult result = UbseElectionNodeMgr::GetInstance().GetAllNode(allNodes);
    if (result == UBSE_ERROR) {
        UBSE_LOG_ERROR << "[ELECTION] GetAllNode: no node found.";
        return UBSE_ERROR;
    }

    if (agentIds.size() + 1 > allNodes.size() / 2) {
        reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
    } else {
        if (agentIds.size() > partitionAgentIDs.size()) {
            reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
        } else if (agentIds.size() < partitionAgentIDs.size()) {
            AcceptNewMaster(rcvPkt, reply, masterId_);
        } else {
            if (rcvPkt.turnId > turnId_) {
                AcceptNewMaster(rcvPkt, reply, masterId_);
            } else if (rcvPkt.turnId == turnId_) {
                HandleSplitBrainMerge(rcvPkt, reply);
            } else {
                reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
            }
        }
    }
    return 0;
}

uint32_t Master::RecvPktElection(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
    reply.replyId = masterId_;
    reply.masterId = masterId_;
    reply.turnId = turnId_;
    return UBSE_OK;
}

uint32_t Master::RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    if (g_globalStop.load()) {
        UBSE_LOG_DEBUG << "[ELECTION] master node is stopping when recv pkt from nodeId=" << srcID;
        return 0;
    }
    // 主收到心跳 两种场景，1：主假死后恢复 2：脑裂合并
    if (rcvPkt.type == ELECTION_PKT_TYPE_HEART) {
        RecvPktHeart(srcID, rcvPkt, reply);
    } else if (rcvPkt.type == ELECTION_PKT_TYPE_SELECT) {
        RecvPktElection(srcID, rcvPkt, reply);
    }
    return 0;
}

void Master::RecvInterGroupInfo(const InterGroupInfo &rcvInfo, InterGroupInfo &replyInfo)
{
    if (rcvInfo.type == ELECTION_GROUP_INFO_TYPE_QUERY_LOCAL_MASTER) {
        replyInfo.groupId = groupId_;
        replyInfo.nodeId = masterId_;
        replyInfo.groupMasterId = masterId_;
        replyInfo.groupStandbyId = standbyId_;
    } else {
        UBSE_LOG_WARN << "[ELECTION] Master rcvInterGroupInfo.type: " << rcvInfo.type << ".";
    }
}

UBSE_ID_TYPE Master::GetMasterNode()
{
    return masterId_;
}

UBSE_ID_TYPE Master::GetStandbyNode()
{
    return standbyId_;
}

UBSE_ID_TYPE Master::GetGlobalMasterNode()
{
    UBSE_ID_TYPE result = INVALID_NODE_ID;
    auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        result = globalRole->GetGlobalMasterNode();
    }

    return result;
}

UBSE_ID_TYPE Master::GetGlobalStandbyNode()
{
    UBSE_ID_TYPE result = INVALID_NODE_ID;
    auto globalRole =RoleMgr::GetInstance().GetGlobalRole();
    if (globalRole != nullptr) {
        result = globalRole->GetGlobalStandbyNode();
    }
    return result;
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
    return standbyStatus_;
}

std::vector<UBSE_ID_TYPE> Master::GetActiveNodes()
{
    std::vector<UBSE_ID_TYPE> activeNodes{};
    std::lock_guard<std::mutex> lock(mtx_);  // 加锁
    for (const auto &node : broadcast_) {
        if (node.second.activeStatus == HeartBeatState::ACTIVE) {
            activeNodes.push_back(node.first);
        }
    }
    return activeNodes;
}

void Master::SetNodeDownStatus(UBSE_ID_TYPE nodeId)
{
    std::lock_guard<std::mutex> lock(mtx_);  // 加锁
    if (broadcast_[nodeId].activeStatus == HeartBeatState::ACTIVE) {
        UBSE_LOG_INFO << "[ELECTION] Master NodeRemoved: " << nodeId;
        RoleMgr::GetInstance().RoleChangeNotifyAsync(UbseElectionEventType::NODE_DOWN, nodeId);
        broadcast_[nodeId].activeStatus = HeartBeatState::LOST;
        broadcast_[nodeId].masterOnlineBcStatus = NotifyStatus::NOT_BROADCAST;
        broadcast_[nodeId].masterOnlineBcTimes = 0;
        auto it = std::find(preNodes_.begin(), preNodes_.end(), nodeId);
        if (it != preNodes_.end()) {
            preNodes_.erase(it);
        }
    }
    if (nodeId == standbyId_) {
        standbyId_ = INVALID_NODE_ID;
    }
}
}