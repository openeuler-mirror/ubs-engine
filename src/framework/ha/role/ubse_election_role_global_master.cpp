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

#include "ubse_election_role_global_master.h"
#include "ubse_election_pkt_simpo.h"
#include "ubse_election_reply_pkt_simpo.h"
#include "ubse_election_role_mgr.h"
#include "ubse_timer.h"

namespace ubse::election {
const std::string UBSE_ELECTION_GLOBAL_MASTER_TIMER = "UbseGlobalMasterTimer";
const std::string UBSE_ELECTION_GLOBAL_MASTER_QUERY_TIMER = "UbseGlobalMasterQueryTimer";
const std::string UBSE_ELECTION_GLOBAL_MASTER_COM = "UbseGlobalMasterComTimer";
const std::string UBSE_ELECTION_GLOBAL_MASTER_QUERY_COM = "UbseGlobalMasterQueryComTimer";
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::timer;
using namespace ubse::context;
using namespace ubse::election::message;
void GlobalMaster::InitNodesStatus()
{
    std::vector<UBSE_ID_TYPE> allNeighbourNodes = RoleMgr::GetInstance().GetManagingGroupMasterIds();
    for (const auto &nodeId : allNeighbourNodes) {
        globalStandbyAgentBroadcast_[nodeId] = BroadcastStatus::Default();
    }
}

GlobalMaster::GlobalMaster(RoleContext &ctx) : globalTurnId_(0)
{
    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_MASTER_TIMER,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            RoleMgr::GetInstance().GlobalProcTimer();
            return UBSE_OK;
        }, UBSE_GLOBAL_PROC_INTERVAL);
    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_MASTER_QUERY_TIMER,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
            if (globalRole != nullptr) {
                RoleMgr::GetInstance().ConnectInterManagingGroup();
            }
            return UBSE_OK;
        }, UBSE_GLOBAL_QUERY_COM_INTERVAL);
    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_MASTER_COM,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            UBSE_ID_TYPE groupId;
            auto ret = UbseElectionNodeMgr::GetInstance().GetGroupId(groupId);
            if (ret != UBSE_OK) {return UBSE_ERROR;}
            auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
            if (globalRole != nullptr && globalRole->GetGlobalRoleType() != GlobalRoleType::GLOBAL_AGENT
                && RoleMgr::GetInstance().IsManagingGroup(groupId)) {
                ConnectManagingMasters();
            }
            return UBSE_OK;
        }, UBSE_GLOBAL_COM_INTERVAL);
    UbseTimerHandlerRegister(
        UBSE_ELECTION_GLOBAL_MASTER_QUERY_COM,
        []() -> UbseResult {
            if (g_globalStop.load()) { return UBSE_OK; }
            auto globalRole = RoleMgr::GetInstance().GetGlobalRole();
            if (globalRole != nullptr) {
                RoleMgr::GetInstance().QueryManagingMaster();
            }
            return UBSE_OK;
        }, UBSE_GLOBAL_QUERY_LOCAL_MASTER_INTERVAL);
    nodeId_ = ctx.masterId;
    globalStandbyId_ = ctx.standbyId;
    globalTurnId_ = ctx.turnId + 1;
    InitNodesStatus();
    UBSE_LOG_INFO << "[ELECTION] Master start ProcTimer: " << nodeId_ << ".";
    stopping_ = false;
    activeCount_ = 0;
}

GlobalMaster::~GlobalMaster()
{
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_MASTER_TIMER);
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_MASTER_QUERY_TIMER);
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_MASTER_COM);
    UbseTimerHandlerUnregister(UBSE_ELECTION_GLOBAL_MASTER_QUERY_COM);
    stopping_ = true;
    // 等待所有回调结束
    while (activeCount_.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    UBSE_LOG_INFO <<"[ELECTION] Global Master destruction completed";
}

std::vector<UBSE_ID_TYPE> GlobalMaster::GetAllGlobalAgentIds() const
{
    std::vector<UBSE_ID_TYPE> result;

    for (const auto &node : globalStandbyAgentBroadcast_) {
        if (node.second.activeStatus == HeartBeatState::ACTIVE
            && node.first != nodeId_ && node.first != globalStandbyId_) {
            result.push_back(node.first);
            }
    }

    return result;
}

void GlobalMaster::DealHbCnt(const UBSE_ID_TYPE &id)
{
    globalStandbyAgentBroadcast_[id].heartBeatLossCnt++;
    if (globalStandbyAgentBroadcast_[id].heartBeatLossCnt > GetHbLostTimes()) {
        globalStandbyAgentBroadcast_[id].activeStatus = HeartBeatState::LOST;
        globalStandbyAgentBroadcast_[id].masterOnlineBcStatus = NotifyStatus::NOT_BROADCAST;
        globalStandbyAgentBroadcast_[id].masterOnlineBcTimes = 0;
        if (globalStandbyAgentBroadcast_[id].heartBeatLossCnt <= GetHbLostTimes()*NO_10
            || globalStandbyAgentBroadcast_[id].heartBeatLossCnt % NO_15 ==0) {
            UBSE_LOG_WARN << "[ELECTION] nodeId=" << id << ", nodeStatus=" << int(globalStandbyAgentBroadcast_[id].activeStatus)
                          << ", heartBeatLossCnt=" << globalStandbyAgentBroadcast_[id].heartBeatLossCnt;
            }
    }
}

void GlobalMaster::PrepareHeartBeatPkt(ElectionPkt &pkt)
{
    pkt.type = ELECTION_PKT_TYPE_GLOBAL_HEART;
    pkt.masterId = nodeId_;
    pkt.standbyId = globalStandbyId_;
    pkt.turnId = globalTurnId_;
    pkt.agentIds = GetAllGlobalAgentIds();
    pkt.agentCount = pkt.agentIds.size();
    auto currentStatus = UbseContext::GetInstance().GetWorkReadiness();
    pkt.masterStatus = currentStatus;
    pkt.standbyStatus = globalStandbyStatus_;
}

void GlobalMaster::ReplaceStandbyNode(ElectionPkt &pkt)
{
    if (globalStandbyId_ != INVALID_NODE_ID
        && globalStandbyAgentBroadcast_[globalStandbyId_].heartBeatLossCnt >= GetHbLostTimes()) {
        // 更换备节点
        UBSE_ID_TYPE smallestId = FindSmallestIdExcludingMasterAndAgent(GetActiveNodes(),
            nodeId_, globalStandbyId_);
        if (smallestId != INVALID_NODE_ID) {
            globalStandbyId_ = smallestId;
            pkt.standbyId = globalStandbyId_;
            UBSE_LOG_INFO << "[ELECTION] Master Appoint the new standby nodeId = " << globalStandbyId_;
        }
        }
}

void GlobalMaster::ProcTimer()
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
        if (globalStandbyId_ == INVALID_NODE_ID) {
            globalStandbyId_ = FindSmallestIdExcludingMaster(nodeId_, GetActiveNodes());
            if (globalStandbyId_ != INVALID_NODE_ID) {
                UBSE_LOG_INFO << "[ELECTION] Global Master Appoint the global standby node id: " << globalStandbyId_;
            }
        }
        PrepareHeartBeatPkt(pkt);
        ReplaceStandbyNode(pkt);
        std::vector<UBSE_ID_TYPE> allNodes = RoleMgr::GetInstance().GetCommMgr()->GetConnectedMasterNodes();
        for (const auto &id : allNodes) {
            UBSE_LOG_DEBUG << "[ELECTION] ProcTimer MASTER send pkt id is: " << id;
            pkt.broadcast = static_cast<uint8_t>(globalStandbyAgentBroadcast_[id].masterOnlineBcStatus);
            lock.unlock();
            auto ret = SendGlobalHeartBeat(id, pkt);
            if (ret !=UBSE_OK) {
                UBSE_LOG_ERROR << "[ELECTION] send heart to nodeId= "<< id << " failed";
            }
            lock.lock();
            DealHbCnt(id);
            TraceContext::Clear();
        }
        UBSE_LOG_DEBUG << "[ELECTION] ProcTimer MASTER send pkt finished ";
        DealNodeUpdate();
        globalStandbyAgentNodes_ = GetActiveNodes();
        lastTimeMs_ = current;
    }
}

void GlobalMaster::DealNodeUpdate()
{
    std::vector<UBSE_ID_TYPE> addNodes;
    std::vector<UBSE_ID_TYPE> removeNodes;
    std::vector<UBSE_ID_TYPE> currentNodes = GetActiveNodes();
    std::sort(currentNodes.begin(), currentNodes.end());
    std::sort(globalStandbyAgentNodes_.begin(), globalStandbyAgentNodes_.end());
    // 若没有节点变动，直接返回
    if (currentNodes == globalStandbyAgentNodes_) {
        return;
    }

    // 找到增加的元素
    for (const auto &nodeId : currentNodes) {
        if (std::find(globalStandbyAgentNodes_.begin(), globalStandbyAgentNodes_.end(), nodeId) == globalStandbyAgentNodes_.end()) {
            addNodes.push_back(nodeId);
        }
    }

    // 找到删除的元素
    for (const auto &nodeId : globalStandbyAgentNodes_) {
        if (std::find(currentNodes.begin(), currentNodes.end(), nodeId) == currentNodes.end()) {
            removeNodes.push_back(nodeId);
        }
    }

    for (const auto &nodeId : addNodes) {
        UBSE_LOG_INFO << "[ELECTION] Global Master NodeAdded: " << nodeId;
        RoleMgr::GetInstance().RoleChangeNotifyAsync(UbseElectionEventType::GLOBAL_NODE_UP, nodeId);
    }

    for (const auto &nodeId : removeNodes) {
        UBSE_LOG_INFO << "[ELECTION] Global Master NodeRemoved: " << nodeId;
        RoleMgr::GetInstance().RoleChangeNotifyAsync(UbseElectionEventType::GLOBAL_NODE_DOWN, nodeId);
    }
}

void UpdateGlobalBroadcastStatus(const std::string &nodeId, const ElectionReplyPkt &reply,
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
void ProcessGlobalReply(GlobalCallbackCtx* context, int32_t result, void* recv, uint32_t len)
{
    const auto& nodeId = context->destId;
    auto& mtx = *context->mtx;
    auto& broad = *context->broadcast;
    auto& globalStandbyAgentGroupTopologies = *context->globalStandbyAgentGroupTopologies;
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
    UpdateGlobalBroadcastStatus(nodeId, reply, broad, status, mtx);
    std::lock_guard<std::mutex> lock(mtx);
    if (!reply.groupId.empty() && !reply.managingGroupNodeIds.empty()) {
        globalStandbyAgentGroupTopologies[reply.groupId] = GroupTopology{
            reply.groupId,
            true,
            reply.masterId,
            reply.standbyId,
            reply.managingGroupNodeIds};
    }
}

void AsyncDealGlobalReply(void* ctx, void* recv, uint32_t len, int32_t result)
{
    auto* context = static_cast<GlobalCallbackCtx*>(ctx);
    if (context == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Received null context in callback";
        return;
    }
    auto& stopping = *context->stopping;
    const auto& nodeId = context->destId;
    if (stopping.load()) {
        UBSE_LOG_INFO << "[ELECTION] Master has stopped, skipping callback; nodeId=" << nodeId;
        SafeDelete(context);
        return;
    }
    auto& activeCount = *context->activeCount;
    activeCount.fetch_add(1);
    ProcessGlobalReply(context, result, recv, len);
    activeCount.fetch_sub(1);
    SafeDelete(context);
}

uint32_t GlobalMaster::SendGlobalHeartBeat(UBSE_ID_TYPE destID, const ElectionPkt &pkt)
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
    auto context = new (std::nothrow) GlobalCallbackCtx;
    if (context == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] New GlobalCallbackCtx failed.";
        return UBSE_ERROR_NULLPTR;
    }
    std::unique_lock<std::mutex> lock(mtx_);
    context->broadcast = &globalStandbyAgentBroadcast_;
    context->globalStandbyAgentGroupTopologies = &globalStandbyAgentGroupTopologies_;
    context->destId = destID;
    context->standbyStatus = &globalStandbyStatus_;
    context->mtx = &mtx_;
    context->stopping = &stopping_;
    context->activeCount = &activeCount_;
    ubse::com::UbseComCallback callback;
    callback.cb = AsyncDealGlobalReply;
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

void GlobalMaster::HandleSplitBrainMerge(const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    // turnId相同，比较masterId大小
    UBSE_ID_TYPE newMasterId = nodeId_ < rcvPkt.masterId ? nodeId_ : rcvPkt.masterId;
    if (newMasterId == nodeId_) {
        // 收编
        reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
        reply.masterId = nodeId_;
    } else {
        // 被收编
        RoleContext ctx;
        ctx.masterId = rcvPkt.masterId;
        ctx.standbyId = rcvPkt.standbyId;
        ctx.turnId = rcvPkt.turnId;
        RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_AGENT, ctx);
        reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
    }
}

uint32_t GlobalMaster::RecvPktHeart(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    std::vector<UBSE_ID_TYPE> agentIds = GetAllGlobalAgentIds();
    if (globalStandbyId_ != INVALID_NODE_ID) {
        agentIds.push_back(globalStandbyId_);
    }

    std::vector<UBSE_ID_TYPE> partitionAgentIDs = rcvPkt.agentIds;
    if (rcvPkt.standbyId != INVALID_NODE_ID) {
        partitionAgentIDs.push_back(rcvPkt.standbyId);
    }

    auto groupMap = nodeMgr::GetAllNodesStoredByGroup();
    auto managingGroupCount = groupMap.size() / 2;
    // 当前集群数量大于一半管理柜的数量，则拒绝其他所有心跳
    if (agentIds.size() + 1 > managingGroupCount) {
        reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
    } else {
        if (agentIds.size() > partitionAgentIDs.size()) {
            reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
        } else if (agentIds.size() < partitionAgentIDs.size()) {
            AcceptNewMaster(rcvPkt, reply, nodeId_);
        } else {
            if (rcvPkt.turnId > globalTurnId_) {
                AcceptNewMaster(rcvPkt, reply, nodeId_);
            } else if (rcvPkt.turnId == globalTurnId_) {
                HandleSplitBrainMerge(rcvPkt, reply);
            } else {
                reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
            }
        }
    }
    return 0;
}

uint32_t GlobalMaster::RecvPktElection(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
    reply.replyId = nodeId_;
    reply.masterId = nodeId_;
    reply.turnId = globalTurnId_;
    return UBSE_OK;
}

uint32_t GlobalMaster::RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt rcvPkt, ElectionReplyPkt &reply)
{
    if (g_globalStop.load()) {
        UBSE_LOG_DEBUG << "[ELECTION] master node is stopping when recv pkt from nodeId =" << srcID;
        return 0;
    }
    // 主收到心跳 两种场景，1：主假死后恢复 2：脑裂合并
    if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_HEART) {
        RecvPktHeart(srcID, rcvPkt, reply);
    } else if (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_SELECT) {
        RecvPktElection(srcID, rcvPkt, reply);
    }
    return 0;
}

UBSE_ID_TYPE GlobalMaster::GetGlobalMasterNode()
{
    return nodeId_;
}

UBSE_ID_TYPE GlobalMaster::GetGlobalStandbyNode()
{
    return globalStandbyId_;
}

std::vector<UBSE_ID_TYPE> GlobalMaster::GetAgentNodes()
{
    return GetActiveNodes();
}

uint8_t GlobalMaster::GetMasterStatus()
{
    auto currentStatus = UbseContext::GetInstance().GetWorkReadiness();
    return currentStatus;
}

uint8_t GlobalMaster::GetStandbyStatus()
{
    return globalStandbyStatus_;
}

std::vector<UBSE_ID_TYPE> GlobalMaster::GetActiveNodes()
{
    std::vector<UBSE_ID_TYPE> activeNodes{};
    for (const auto &node : globalStandbyAgentBroadcast_) {
        if (node.second.activeStatus == HeartBeatState::ACTIVE) {
            activeNodes.push_back(node.first);
        }
    }
    return activeNodes;
}

void GlobalMaster::SetNodeDownStatus(UBSE_ID_TYPE nodeId)
{
    std::lock_guard<std::mutex> lock(mtx_);  // 加锁
    if (globalStandbyAgentBroadcast_[nodeId].activeStatus == HeartBeatState::ACTIVE) {
        UBSE_LOG_INFO << "[ELECTION] Global Master NodeRemoved: " << nodeId;
        RoleMgr::GetInstance().RoleChangeNotifyAsync(UbseElectionEventType::GLOBAL_NODE_DOWN, nodeId);
        globalStandbyAgentBroadcast_[nodeId].activeStatus = HeartBeatState::LOST;
        globalStandbyAgentBroadcast_[nodeId].masterOnlineBcStatus = NotifyStatus::NOT_BROADCAST;
        globalStandbyAgentBroadcast_[nodeId].masterOnlineBcTimes = 0;
        auto it = std::find(globalStandbyAgentNodes_.begin(), globalStandbyAgentNodes_.end(), nodeId);
        if (it != globalStandbyAgentNodes_.end()) {
            globalStandbyAgentNodes_.erase(it);
        }
    }
    if (nodeId == globalStandbyId_) {
        globalStandbyId_ = INVALID_NODE_ID;
    }
}

uint64_t GlobalMaster::GetTurnId()
{
    return globalTurnId_;
}

RoleType GlobalMaster::GetRoleType()
{
    return RoleType::MASTER;
}

GlobalRoleType GlobalMaster::GetGlobalRoleType()
{
    return GlobalRoleType::GLOBAL_MASTER;
}

void GlobalMaster::RecvInterGroupInfo(const InterGroupInfo &rcvInfo, InterGroupInfo &replyInfo)
{
    if (rcvInfo.type == ELECTION_GROUP_INFO_TYPE_GLOBAL_CASCADE_REPORT) {
        cascadeGroupReport_ = rcvInfo;
        // 回复全局主备
        replyInfo.nodeId = nodeId_;
        replyInfo.groupMasterId = nodeId_;
        replyInfo.groupStandbyId = globalStandbyId_;
    }
}

InterGroupInfo GlobalMaster::GetCascadeGroupReport()
{
    return cascadeGroupReport_;
}

std::vector<GroupTopology> GlobalMaster::GetManagingGroupNodeIds()
{
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<GroupTopology> managingGroupTopologies{};
    for (const auto &node : globalStandbyAgentGroupTopologies_) {
        managingGroupTopologies.push_back(node.second);
    }
    return managingGroupTopologies;
}

}