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

#include "ubse_election_group_info_simpo.h"
#include "ubse_election_pkt_simpo.h"
#include "ubse_election_reply_pkt_simpo.h"
#include "ubse_election_role_mgr.h"
#include "ubse_node_mgr.h"
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
using namespace ubse::com;

void GlobalMaster::InitNodesStatus()
{
    std::vector<UBSE_ID_TYPE> allNeighbourNodes = RoleMgr::GetInstance().GetManagingGroupMasterIds();
    for (const auto &nodeId : allNeighbourNodes) {
        globalStandbyAgentBroadcast_[nodeId] = BroadcastStatus::Default();
    }
}

void GlobalMaster::SyncBroadcastMap()
{
    std::vector<UBSE_ID_TYPE> latestMasterIds = RoleMgr::GetInstance().GetManagingGroupMasterIds();
    std::set<UBSE_ID_TYPE> latestSet(latestMasterIds.begin(), latestMasterIds.end());
    for (auto it = globalStandbyAgentBroadcast_.begin(); it != globalStandbyAgentBroadcast_.end();) {
        if (latestSet.find(it->first) == latestSet.end()) {
            UBSE_LOG_INFO << "[ELECTION] SyncBroadcastMap remove stale master: " << it->first;
            if (it->first == globalStandbyId_) {
                globalStandbyId_ = INVALID_NODE_ID;
            }
            it = globalStandbyAgentBroadcast_.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto &nodeId : latestMasterIds) {
        if (globalStandbyAgentBroadcast_.find(nodeId) == globalStandbyAgentBroadcast_.end()) {
            globalStandbyAgentBroadcast_[nodeId] = BroadcastStatus::Default();
            UBSE_LOG_INFO << "[ELECTION] SyncBroadcastMap add new master: " << nodeId;
        }
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
    auto ret = UbseElectionNodeMgr::GetInstance().GetGroupId(groupId_);
    if (ret != UBSE_OK || groupId_.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] GetGroupId fail";
    }
    globalStandbyId_ = ctx.standbyId;
    globalTurnId_ = ctx.turnId + 1;
    InitNodesStatus();
    UBSE_LOG_INFO << "[ELECTION] Master start ProcTimer: " << nodeId_ << ".";
    stopping_ = false;
    activeCount_ = 0;
    InitManagingToCascadeNodeIds();
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
    if (globalStandbyId_ != INVALID_NODE_ID) {
        auto it = globalStandbyAgentBroadcast_.find(globalStandbyId_);
        if (it != globalStandbyAgentBroadcast_.end() && it->second.heartBeatLossCnt >= GetHbLostTimes()) {
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
        SyncBroadcastMap();
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
    DetectCascadeGroupTimeout();
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
        auto nodeInfo = nodeMgr::GetUbseNodeById(nodeId);
        auto it = managingToCascadeNodeId_.find(std::to_string(nodeInfo.groupId));
        if (it == managingToCascadeNodeId_.end()) { continue; }
        AddDownstreamGroupRoute(std::to_string(nodeInfo.groupId), nodeId, nodeId); // 管理组分组路由
        uint16_t managingGroupCount = RoleMgr::GetInstance().GetManagingGroupCount();
        uint16_t cascadeGroupId = nodeInfo.groupId + managingGroupCount;
        AddDownstreamGroupRoute(std::to_string(cascadeGroupId), it->second, nodeId); // 级联组分组路由
    }

    for (const auto &nodeId : removeNodes) {
        UBSE_LOG_INFO << "[ELECTION] Global Master NodeRemoved: " << nodeId;
        RoleMgr::GetInstance().RoleChangeNotifyAsync(UbseElectionEventType::GLOBAL_NODE_DOWN, nodeId);
        if (nodeId == globalStandbyId_) {
            globalStandbyId_ = INVALID_NODE_ID;
        }
        std::vector<std::pair<UBSE_ID_TYPE, UBSE_ID_TYPE>> routesToDelete;                    // ← 先收集
        for (const auto &entry : downstreamRouteEntries_) {                                   // ← 直接遍历路由表
            if (entry.second.nextHopNodeId == nodeId) {
                routesToDelete.emplace_back(entry.first, entry.second.dstNodeId);
            }
        }
        for (const auto &r : routesToDelete) {                                                // ← 后删除
            UBSE_LOG_INFO << "[ELECTION] DealNodeUpdate delete downstream route, groupId=" << r.first
                          << ", nextHopNodeId=" << nodeId << ", dstNodeId=" << r.second;
            downstreamRouteEntries_.erase(r.first);
            auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
            if (comModule != nullptr) {
                comModule->DelRoute(r.second);
            }
        }
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
    auto& globalCascadeGroupTopologies = *context->globalCascadeGroupTopologies;
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
    if (!reply.cascadeGroupId.empty() && !reply.cascadeGroupNodeIds.empty()) {
        globalCascadeGroupTopologies[reply.cascadeGroupId] = GroupTopology{
            reply.cascadeGroupId,
            false,
            reply.cascadeMasterId,
            reply.cascadeStandbyId,
            reply.cascadeGroupNodeIds};
    } else if (!reply.groupId.empty()) {
        uint16_t managingGroupCount = RoleMgr::GetInstance().GetManagingGroupCount();
        if (managingGroupCount > 0) {
            uint16_t cascadeGroupId = 0;
            try {
                cascadeGroupId = static_cast<uint16_t>(std::stoi(reply.groupId)) + managingGroupCount;
            } catch (const std::exception& e) {
                UBSE_LOG_WARN << "[ELECTION] invalid groupId from reply, groupId=" << reply.groupId
                              << ", error=" << e.what();
                return;
            }
            auto it = globalCascadeGroupTopologies.find(std::to_string(cascadeGroupId));
            if (it != globalCascadeGroupTopologies.end()) {
                UBSE_LOG_INFO << "[ELECTION] cascade group removed by managing group report, groupId=" << reply.groupId
                              << ", cascadeGroupId=" << cascadeGroupId;
                globalCascadeGroupTopologies.erase(it);
            }
        }
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
    context->globalCascadeGroupTopologies = &globalCascadeGroupTopologies_;
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

void AcceptNewGlobalMaster(const ElectionPkt rcvPkt, ElectionReplyPkt &reply, const UBSE_ID_TYPE masterId)
{
    if (rcvPkt.standbyId == masterId) {
        RoleContext ctx;
        ctx.masterId = rcvPkt.masterId;
        ctx.turnId = rcvPkt.turnId;
        ctx.standbyId = masterId;
        RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_STANDBY, ctx);
    } else {
        RoleContext ctx;
        ctx.masterId = rcvPkt.masterId;
        ctx.turnId = rcvPkt.turnId;
        RoleMgr::GetInstance().SwitchGlobalRole(GlobalRoleType::GLOBAL_AGENT, ctx);
    }
    reply.replyResult = ELECTION_PKT_RESULT_ACCEPT;
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

    // 当前集群数量大于一半管理柜的数量，则拒绝其他所有心跳
    uint16_t managingGroupCount = RoleMgr::GetInstance().GetManagingGroupCount();
    // 当前集群数量大于一半管理柜的数量，则拒绝其他所有心跳
    if (agentIds.size() + 1 > managingGroupCount) {
        reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
    } else {
        if (agentIds.size() > partitionAgentIDs.size()) {
            reply.replyResult = ELECTION_PKT_TYPE_REJECT_HAS_MASTER;
        } else if (agentIds.size() < partitionAgentIDs.size()) {
            AcceptNewGlobalMaster(rcvPkt, reply, nodeId_);
        } else {
            if (rcvPkt.turnId > globalTurnId_) {
                AcceptNewGlobalMaster(rcvPkt, reply, nodeId_);
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
    std::vector<UBSE_ID_TYPE> routesToDelete;
    {
        std::lock_guard<std::mutex> lock(mtx_);  // 加锁
        auto bcIt = globalStandbyAgentBroadcast_.find(nodeId);
        if (bcIt != globalStandbyAgentBroadcast_.end() &&
            bcIt->second.activeStatus == HeartBeatState::ACTIVE) {
            UBSE_LOG_INFO << "[ELECTION] Global Master NodeRemoved: " << nodeId;
            RoleMgr::GetInstance().RoleChangeNotifyAsync(UbseElectionEventType::GLOBAL_NODE_DOWN, nodeId);
            bcIt->second.activeStatus = HeartBeatState::LOST;
            bcIt->second.masterOnlineBcStatus = NotifyStatus::NOT_BROADCAST;
            bcIt->second.masterOnlineBcTimes = 0;
            auto it = std::find(globalStandbyAgentNodes_.begin(), globalStandbyAgentNodes_.end(), nodeId);
            if (it != globalStandbyAgentNodes_.end()) {
                globalStandbyAgentNodes_.erase(it);
            }
            std::vector<UBSE_ID_TYPE> groupIdsToDelete;
            for (const auto& entry : downstreamRouteEntries_) {
                if (entry.second.nextHopNodeId == nodeId) {
                    routesToDelete.push_back(entry.second.dstNodeId);
                    groupIdsToDelete.push_back(entry.first);
                }
            }
            for (const auto& groupId : groupIdsToDelete) {
                downstreamRouteEntries_.erase(groupId);
            }
        }
        if (nodeId == globalStandbyId_) {
            globalStandbyId_ = INVALID_NODE_ID;
        }
    }
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule != nullptr) {
        for (const auto &dstNodeId : routesToDelete) {
            UBSE_LOG_INFO << "[ELECTION] SetNodeDownStatus delete downstream route, nextHopNodeId="
                          << nodeId << ", dstNodeId=" << dstNodeId;
            comModule->DelRoute(dstNodeId);
        }
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
        GetBootTime(lastCascadeReportTime_);
        UBSE_ID_TYPE previousCascadeMasterId = cascadeGroupReport_.groupMasterId;
        cascadeGroupReport_ = rcvInfo;
        UBSE_ID_TYPE currentCascadeMasterId = cascadeGroupReport_.groupMasterId;

        if (previousCascadeMasterId != currentCascadeMasterId) {
            if (!previousCascadeMasterId.empty()) {
                UBSE_LOG_INFO << "[ELECTION] Cascade group master offline: previousCascadeMasterId="
                              << previousCascadeMasterId
                              << ", currentCascadeMasterId=" << currentCascadeMasterId;
                if (!g_globalStop.load()) {
                    RoleMgr::GetInstance().RoleChangeNotifyAsync(
                        UbseElectionEventType::GLOBAL_CASCADE_NODE_DOWN, previousCascadeMasterId);
                }
            }
            std::unique_lock<std::mutex> lock(mtx_);
            DeleteDownstreamGroupRoute(rcvInfo.groupId);
            AddDownstreamGroupRoute(rcvInfo.groupId, currentCascadeMasterId, currentCascadeMasterId);
            lock.unlock();
        }
        // 回复全局主备
        replyInfo.nodeId = nodeId_;
        replyInfo.globalMasterId = nodeId_;
        replyInfo.globalStandbyId = globalStandbyId_;
        replyInfo.groupId = groupId_;
        auto groupRole = RoleMgr::GetInstance().GetRole();
        if (groupRole != nullptr) {
            std::vector<UBSE_ID_TYPE> agentNodes = groupRole->GetAgentNodes();
            replyInfo.groupStandbyId = groupRole->GetStandbyNode();
            replyInfo.groupMasterId = groupRole->GetMasterNode();
            if (replyInfo.groupMasterId != INVALID_NODE_ID) {
                replyInfo.groupNodeIds.push_back(replyInfo.groupMasterId);
            }
            if (replyInfo.groupStandbyId != INVALID_NODE_ID) {
                replyInfo.groupNodeIds.push_back(replyInfo.groupStandbyId);
            }
            for (auto &node : agentNodes) {
                if (node != replyInfo.groupMasterId && node != replyInfo.groupStandbyId) {
                    replyInfo.groupNodeIds.push_back(node);
                }
            }
        }
    }
}

InterGroupInfo GlobalMaster::GetCascadeGroupReport()
{
    return cascadeGroupReport_;
}

void GlobalMaster::CleanupRoutes()
{
    DeleteAllDownstreamGroupRoutes();
}

void GlobalMaster::InitManagingToCascadeNodeIds()
{
    auto groupMap = nodeMgr::GetAllNodesStoredByGroup();
    if (groupMap.empty()) { return; }
    std::vector<uint16_t> sortedGroupIds;
    for (const auto &kv : groupMap) { sortedGroupIds.push_back(kv.first); }
    std::sort(sortedGroupIds.begin(), sortedGroupIds.end());
    uint16_t totalGroupCount = static_cast<uint16_t>(sortedGroupIds.size());
    uint16_t managingGroupCount = totalGroupCount / 2;
    if (totalGroupCount % 2 != 0) {
        managingGroupCount += 1;
    }
    for (size_t i = 0; i < managingGroupCount; ++i) {
        uint16_t managingGroupId = sortedGroupIds[i];
        if (i + managingGroupCount >= sortedGroupIds.size()) {
            UBSE_LOG_INFO << "[ELECTION] managing group " << managingGroupId
                          << " has no paired cascade group (odd total count).";
            continue;
        }
        uint16_t cascadeGroupId = sortedGroupIds[i + managingGroupCount];
        const auto &cascadeNodes = groupMap[cascadeGroupId];
        if (cascadeNodes.empty()) { continue; }
        managingToCascadeNodeId_[std::to_string(managingGroupId)] = cascadeNodes[NO_0].nodeId;
    }
}

void GlobalMaster::AddDownstreamGroupRoute(const UBSE_ID_TYPE &groupId, const UBSE_ID_TYPE &dstNodeId,
    const UBSE_ID_TYPE &nextHopNodeId)
{
    if (dstNodeId.empty() || nextHopNodeId.empty()) { return; }
    uint32_t capability = UbseElectionNodeMgr::GetInstance().GetCapability();
    RouteEntry entry;
    entry.dstNodeId = dstNodeId;
    entry.capacity = capability;
    entry.priority = 64;
    entry.nextHopNodeId = nextHopNodeId;
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_WARN << "[ELECTION] AddDownstreamGroupRoute: Getting ComModule failed.";
        return;
    }
    if (comModule->AddRoute(entry) == UBSE_OK) {
        downstreamRouteEntries_[groupId] = entry;
        UBSE_LOG_INFO << "[ELECTION] AddDownstreamGroupRoute: groupId=" << groupId
                      << ", dstNodeId=" << entry.dstNodeId
                      << ", capacity=" << entry.capacity
                      << ", nextHopNodeId=" << entry.nextHopNodeId;
    } else {
        UBSE_LOG_WARN << "[ELECTION] AddDownstreamGroupRoute: AddRoute fail, groupId=" << groupId;
    }
}

void GlobalMaster::DeleteDownstreamGroupRoute(const UBSE_ID_TYPE &groupId)
{
    auto it = downstreamRouteEntries_.find(groupId);
    if (it == downstreamRouteEntries_.end()) { return; }
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) { return; }
    UBSE_LOG_INFO << "[ELECTION] DeleteDownstreamGroupRoute: groupId=" << groupId
                  << ", dstNodeId=" << it->second.dstNodeId;
    comModule->DelRoute(it->second.dstNodeId);
    downstreamRouteEntries_.erase(it);
}

void GlobalMaster::DeleteAllDownstreamGroupRoutes()
{
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) { return; }
    std::vector<UBSE_ID_TYPE> dstNodeIds;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        for (const auto &pair : downstreamRouteEntries_) {
            dstNodeIds.push_back(pair.second.dstNodeId);
        }
        downstreamRouteEntries_.clear();
    }
    for (const auto &dstNodeId : dstNodeIds) {
        comModule->DelRoute(dstNodeId);
    }
}

std::vector<GroupTopology> GlobalMaster::GetManagingGroupNodeIds()
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto activeNodes = GetActiveNodes();
    std::unordered_set<UBSE_ID_TYPE> activeSet(activeNodes.begin(), activeNodes.end());
    std::vector<GroupTopology> managingGroupTopologies{};
    for (auto it = globalStandbyAgentGroupTopologies_.begin(); it != globalStandbyAgentGroupTopologies_.end();) {
        const auto &groupMasterId = it->second.groupMasterId;
        if (activeSet.find(groupMasterId) == activeSet.end()) {
            UBSE_LOG_INFO << "[ELECTION] skip non-active managing group, groupId=" << it->first
                          << ", groupMasterId=" << groupMasterId;
            uint16_t managingGroupCount = RoleMgr::GetInstance().GetManagingGroupCount();
            uint16_t cascadeGroupId = std::stoi(it->first) + managingGroupCount;
            globalCascadeGroupTopologies_.erase(std::to_string(cascadeGroupId));
            it = globalStandbyAgentGroupTopologies_.erase(it);
            continue;
        }
        managingGroupTopologies.push_back(it->second);
        ++it;
    }
    return managingGroupTopologies;
}

std::vector<GroupTopology> GlobalMaster::GetCascadeGroupNodeIds()
{
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<GroupTopology> cascadeGroupTopologies{};
    for (const auto &node : globalCascadeGroupTopologies_) {
        cascadeGroupTopologies.push_back(node.second);
    }
    return cascadeGroupTopologies;
}

void GlobalMaster::DetectCascadeGroupTimeout()
{
    if (cascadeGroupReport_.groupMasterId.empty()) {
        return;
    }
    uint64_t bootTime;
    if (GetBootTime(bootTime) != UBSE_OK) {
        return;
    }
    uint32_t timeoutThreshold = UBSE_GLOBAL_QUERY_LOCAL_MASTER_INTERVAL * NO_10 * NO_1000; // 10s
    if (bootTime - lastCascadeReportTime_ > timeoutThreshold) {
        UBSE_LOG_INFO << "[ELECTION] Cascade group report timeout, masterId="
                       << cascadeGroupReport_.groupMasterId;
        if (!g_globalStop.load()) {
            RoleMgr::GetInstance().RoleChangeNotifyAsync(
                UbseElectionEventType::GLOBAL_CASCADE_NODE_DOWN, cascadeGroupReport_.groupMasterId);
        }
        std::unique_lock<std::mutex> lock(mtx_);
        DeleteDownstreamGroupRoute(cascadeGroupReport_.groupId);
        lock.unlock();
        cascadeGroupReport_ = {};
    }
}

}