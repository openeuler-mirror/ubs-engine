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

#include "ubse_election_role_mgr.h"
#include <memory>
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election_group_info_simpo.h"
#include "ubse_election_pkt_simpo.h"
#include "ubse_election_reply_pkt_simpo.h"
#include "ubse_election_role_global_agent.h"
#include "ubse_election_role_global_initializer.h"
#include "ubse_election_role_global_master.h"
#include "ubse_election_role_global_standby.h"
#include "ubse_net_util.h"

namespace ubse::election {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::utils;
using namespace ubse::election::message;
std::shared_ptr<RoleMgr> RoleMgr::instance_ = nullptr;

std::shared_ptr<ElectionRole> RoleMgr::GetRole()
{
    return currentRole_;
}

std::shared_ptr<ElectionRole> RoleMgr::GetGlobalRole()
{
    return globalCurrentRole_;
}

void RoleMgr::ProcTimer()
{
    std::lock_guard<std::mutex> lock(mutex_);
    GetRole()->ProcTimer();
}

void RoleMgr::GlobalProcTimer()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto globalRole = GetGlobalRole()) {
        globalRole->ProcTimer();
    }
}

uint32_t RoleMgr::RecvPkt(UBSE_ID_TYPE srcID, const ElectionPkt &rcvPkt, ElectionReplyPkt &reply)
{
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t ret;
    if (rcvPkt.type == ELECTION_PKT_TYPE_SELECT || rcvPkt.type == ELECTION_PKT_TYPE_HEART) {
        ret = GetRole()->RecvPkt(srcID, rcvPkt, reply);
    } else if (GetGlobalRole() != nullptr &&
        (rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_SELECT || rcvPkt.type == ELECTION_PKT_TYPE_GLOBAL_HEART)) {
        ret = GetGlobalRole()->RecvPkt(srcID, rcvPkt, reply);
    } else {
        UBSE_LOG_ERROR << "[ELECTION] Received unknown layer.";
        ret = UBSE_ERROR;
    }
    return ret;
}

void RoleMgr::RecvInterGroupInfo(const InterGroupInfo &rcvInfo, InterGroupInfo &replyInfo)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (rcvInfo.type == ELECTION_GROUP_INFO_TYPE_QUERY_LOCAL_MASTER) {
        GetRole()->RecvInterGroupInfo(rcvInfo, replyInfo);
    } else if (rcvInfo.type == ELECTION_GROUP_INFO_TYPE_GLOBAL_CASCADE_REPORT) {
        GetGlobalRole()->RecvInterGroupInfo(rcvInfo, replyInfo);
    }
}

void RoleMgr::SwitchRole(RoleType roleType, RoleContext &ctx)
{
    if (currentRole_) {
        currentRole_->CleanupRoutes();
    }
    RoleType role;
    bool flag = false;
    switch (roleType) {
        case RoleType::MASTER:
            role = RoleMgr::GetInstance().GetRole()->GetRoleType();
            if (role != RoleType::STANDBY) {
                flag = true;
            }
            currentRole_ = SafeMakeShared<Master>(ctx);
            if (!currentRole_) {
                UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared currentRole failed.";
                return;
            }
            if (flag) {
                RoleChangeNotifyAsync(UbseElectionEventType::CHANGE_TO_MASTER, ctx.masterId);
            }
            RoleChangeNotifyAsync(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION, ctx.masterId);
            if (UbseElectionNodeMgr::GetInstance().IsHierarchicalElection()) {
                globalCurrentRole_ = SafeMakeShared<GlobalInitializer>();
                if (!globalCurrentRole_) {
                    UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared globalCurrentRole failed.";
                    return;
                }
            }
            UBSE_LOG_INFO << "[ELECTION] SwitchRole Group Master, node_id = " << ctx.masterId << ".";
            break;
        case RoleType::STANDBY:
            currentRole_ = SafeMakeShared<Standby>(ctx);
            if (!currentRole_) {
                UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared currentRole failed.";
                return;
            }
            globalCurrentRole_ = nullptr;
            RoleChangeNotifyAsync(UbseElectionEventType::CHANGE_TO_STANDBY, ctx.standbyId);
            UBSE_LOG_INFO << "[ELECTION] SwitchRole Group Standby: " << ctx.standbyId << ".";
            break;
        case RoleType::AGENT:
            currentRole_ = SafeMakeShared<Agent>(ctx);
            if (!currentRole_) {
                UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared currentRole failed.";
                return;
            }
            globalCurrentRole_ = nullptr;
            RoleChangeNotifyAsync(UbseElectionEventType::CHANGE_TO_AGENT, ctx.standbyId);
            UBSE_LOG_INFO << "[ELECTION] SwitchRole Group Agent.";
            RoleChangeNotifyAsync(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION, ctx.masterId);
            UBSE_LOG_INFO << "[ELECTION] The Group Master is online: " << ctx.masterId << ", turnId is: " << ctx.turnId;
            break;
        case RoleType::INITIALIZER:
            currentRole_ = SafeMakeShared<Initializer>();
            if (!currentRole_) {
                UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared currentRole failed.";
                return;
            }
            globalCurrentRole_ = nullptr;
            UBSE_LOG_INFO << "[ELECTION] SwitchRole Group Initializer.";
            break;
    }
}

void RoleMgr::SwitchGlobalRole(GlobalRoleType globalRoleType, RoleContext &ctx)
{
    if (globalCurrentRole_) {
        globalCurrentRole_->CleanupRoutes();
    }
    switch (globalRoleType) {
        case GlobalRoleType::GLOBAL_MASTER:
            globalCurrentRole_ = SafeMakeShared<GlobalMaster>(ctx);
            if (!globalCurrentRole_) {
                UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared globalCurrentRole failed.";
                return;
            }
            RoleChangeNotifyAsync(UbseElectionEventType::GLOBAL_MASTER_ONLINE_NOTIFICATION, ctx.masterId);
            UBSE_LOG_INFO << "[ELECTION] SwitchRole Global Master, node_id = " << ctx.masterId << ".";
            break;
        case GlobalRoleType::GLOBAL_STANDBY:
            globalCurrentRole_ = SafeMakeShared<GlobalStandby>(ctx);
            if (!globalCurrentRole_) {
                UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared globalCurrentRole failed.";
                return;
            }
            UBSE_LOG_INFO << "[ELECTION] SwitchRole Global Standby: " << ctx.standbyId << ".";
            break;
        case GlobalRoleType::GLOBAL_AGENT:
            globalCurrentRole_ = SafeMakeShared<GlobalAgent>(ctx);
            if (!globalCurrentRole_) {
                UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared globalCurrentRole failed.";
                return;
            }
            UBSE_LOG_INFO << "[ELECTION] SwitchRole Global Agent.";
            RoleChangeNotifyAsync(UbseElectionEventType::GLOBAL_MASTER_ONLINE_NOTIFICATION, ctx.masterId);
            UBSE_LOG_INFO << "[ELECTION] The Global Master is online: " << ctx.masterId
            << ", global turnId is: " << ctx.turnId;
            break;
        case GlobalRoleType::GLOBAL_INITIALIZER:
            globalCurrentRole_ = SafeMakeShared<GlobalInitializer>();
            if (!globalCurrentRole_) {
                UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared globalCurrentRole failed.";
                return;
            }
            UBSE_LOG_INFO << "[ELECTION] SwitchRole Global Initializer.";
            break;
        default:
            break;
    }
}

uint32_t RoleMgr::RoleChangeAttach(UbseElectionEventType type, UbseElectionHandler handler)
{
    std::unique_lock<std::recursive_mutex> uniqueLock(mProcessorLock_);
    // 名称查重
    auto &handlers = handlers_[type];
    if (std::any_of(handlers.begin(), handlers.end(), [&](const auto &h) { return h->name == handler.name; })) {
        UBSE_LOG_ERROR << "[ELECTION] RoleChangeAttach handler name dup - " << handler.name;
        return UbseElectionDupNameError;
    }

    // 创建共享对象
    HandlerPtr handler_ptr = SafeMakeShared<UbseElectionHandler>(handler);
    if (!handler_ptr) {
        UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared currentRole failed.";
        return UbseElectionHandlerError;
    }
    active_handlers_.push_back(handler_ptr);

    auto safe_handler_ptr =
        SafeMakeShared<SafeHandler>(handler_ptr, handler.name, handler.priority, handler.sequenceId);
    if (!safe_handler_ptr) {
        UBSE_LOG_ERROR << "[ELECTION] SafeMakeShared failed.";
        return UbseElectionHandlerError;
    }
    handlers.emplace_back(safe_handler_ptr);

    // 排序
    std::sort(handlers.begin(), handlers.end(),
              [](const std::shared_ptr<SafeHandler> &a, const std::shared_ptr<SafeHandler> &b) { return *a < *b; });
    return UbseElectionOk;
}

uint32_t RoleMgr::RoleChangeDeAttach(UbseElectionEventType type, UbseElectionHandler handler)
{
    std::unique_lock<std::recursive_mutex> uniqueLock(mProcessorLock_);
    auto it = handlers_.find(type);
    if (it == handlers_.end()) {
        return UbseElectionTypeError;
    }
    auto &handlers = it->second;
    auto orig_size = handlers.size();
    std::string name = handler.name;
    auto it_safe = std::find_if(handlers.begin(), handlers.end(),
                                [&name](const std::shared_ptr<SafeHandler> &ptr) { return ptr->name == name; });
    std::shared_ptr<SafeHandler> saftHandler;
    if (it_safe != handlers.end()) {
        saftHandler = *it_safe;
    } else {
        return UbseElectionHandlerError;
    }
    {
        std::unique_lock<std::mutex> lock(saftHandler->mutex);
        active_handlers_.erase(std::remove_if(active_handlers_.begin(), active_handlers_.end(),
                                              [&](const HandlerPtr &ptr) { return ptr->name == handler.name; }),
                               active_handlers_.end());
        handlers.erase(
            std::remove_if(handlers.begin(), handlers.end(), [&](const auto &h) { return h->name == handler.name; }),
            handlers.end());
    }
    return handlers.size() != orig_size ? UbseElectionOk : UbseElectionHandlerError;
}

UbseResult HandleRoleChangeNotifyError(uint32_t ret, UbseElectionEventType &type, const std::string &handlerName)
{
    if (ret == UbseElectionError) {
        UBSE_LOG_WARN << "[ELECTION] RoleChangeNotify handler error - " << handlerName;
        if (type == UbseElectionEventType::CHANGE_TO_SWITCHOVER) {
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

uint32_t RoleMgr::RoleChangeNotify(UbseElectionEventType type, UBSE_ID_TYPE newId)
{
    std::vector<std::shared_ptr<SafeHandler>> current_handlers;
    {
        std::unique_lock<std::recursive_mutex> uniqueLock(mProcessorLock_);
        auto it = handlers_.find(type);
        if (it != handlers_.end()) {
            current_handlers = it->second;
        }
    }
    uint32_t ret = UbseElectionOk;
    for (const auto &safe_h : current_handlers) {
        UBSE_LOG_INFO << "[ELECTION] RoleChangeNotify handler - " << safe_h->name << ", type=" << int(type);
        std::unique_lock<std::mutex> lock(safe_h->mutex);
        if (auto h = safe_h->weak_handler.lock()) {
            UBSE_ID_TYPE tmpId = newId;
            ret = h->handler(type, tmpId);
            if (HandleRoleChangeNotifyError(ret, type, safe_h->name) == UBSE_ERROR) {
                break;
            }
        }
    }
    if (ret == UbseElectionOk) {
        UbseContext::GetInstance().SetWorkReadiness(IS_READY);
    }
    UBSE_LOG_INFO << "[ELECTION] WorkReadiness is " << UbseContext::GetInstance().GetWorkReadiness()
                << ",ElectionEventType is " << int(type) << ".";
    return ret;
}

void RoleMgr::RoleChangeNotifyAsync(UbseElectionEventType type, UBSE_ID_TYPE newId)
{
    // 异步执行 RoleChangeNotify
    try {
        std::thread([this, type, newId]() { RoleChangeNotify(type, newId); }).detach();
    } catch (const std::system_error&) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to create thread.";
    }
    return;
}

std::string RoleMgr::GetIpById(const UBSE_ID_TYPE &id)
{
    auto node = nodeMgr::GetUbseNodeById(id);
    if (node.addr.empty()) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get ip by id, id = " << id;
        return "";
    }
    UBSE_LOG_INFO << "[ELECTION] GetIp by id, ip = " << node.addr << ", id = " << id;
    return node.addr;
}

static std::string NextNodeInGroup(const std::string &nodeStr)
{
    auto nodeInfo = nodeMgr::GetUbseNodeById(nodeStr);
    auto groupNodes = nodeMgr::GetNodesByGroupId(nodeInfo.groupId);
    if (groupNodes.empty()) {
        UBSE_LOG_WARN << "[ELECTION] Group " << nodeInfo.groupId << " has no nodes";
        return nodeStr;
    }
    size_t currentIndex = 0;
    bool found = false;
    for (size_t i = 0; i < groupNodes.size(); ++i) {
        if (groupNodes[i].nodeId == nodeStr) {
            currentIndex = i;
            found = true;
            break;
        }
    }
    if (!found) {
        UBSE_LOG_ERROR << "[ELECTION] Node " << nodeStr << " not found in its group";
        return nodeStr;
    }
    size_t nextIndex = (currentIndex + 1) % groupNodes.size();
    return groupNodes[nextIndex].nodeId;
}

void RoleMgr::ConnectInterManagingGroup()
{
    auto globalRoleType = RoleMgr::GetInstance().GetGlobalRole()->GetGlobalRoleType();
    if (globalRoleType == GlobalRoleType::GLOBAL_AGENT) {
        return;
    }
    std::unordered_map<UBSE_ID_TYPE, UBSE_ID_TYPE> targets;
    {
        std::lock_guard<std::mutex> lock(discoveryMtx_);
        targets = discoveryTargetByGroup;
    }
    for (const auto &kv : targets) {
        const std::string &groupId = kv.first;
        const std::string &targetNodeId = kv.second;

        std::string ip = GetIpById(targetNodeId);
        UBSE_LOG_INFO << "[ELECTION] Discovery Connections for group " << groupId
        << "node=" << targetNodeId << " ip=" << ip;
        auto ret = RoleMgr::GetInstance().GetCommMgr()->ConnectForGroupMaster(targetNodeId, ip);
        if (ret == UBSE_OK) {
            std::lock_guard<std::mutex> lock(discoveryMtx_);
            connFailedCntByGroup[groupId] = 0;
            continue;
        }
        std::lock_guard<std::mutex> lock(discoveryMtx_);
        connFailedCntByGroup[groupId] ++;
        UBSE_LOG_WARN << "[ELECTION] Connect failed：group=" << groupId
        << " node=" << targetNodeId << " failCnt=" << connFailedCntByGroup[groupId];
        if (connFailedCntByGroup[groupId] >= NO_3) {
            std::string newNode = NextNodeInGroup(targetNodeId);
            connFailedCntByGroup[groupId] = 0;
            discoveryTargetByGroup[groupId] = newNode;
            std::string ip2 = GetIpById(newNode);
            UBSE_LOG_INFO << "[ELECTION] 3 consecutive connection failures, switching target: group="
            << groupId << ", from=" << targetNodeId << ", to=" << newNode << ", ip=" << ip2;
        }
    }
}

void RoleMgr::QueryManagingMaster()
{
    InterGroupInfo pkt{};
    pkt.type = ELECTION_GROUP_INFO_TYPE_QUERY_LOCAL_MASTER;
    std::unordered_map<UBSE_ID_TYPE, UBSE_ID_TYPE> discoveryNodes =
        RoleMgr::GetInstance().GetCommMgr()->GetInterManagementGroupLinkMap();
    for (const auto &item : discoveryNodes) {
        auto sendRet = SendQueryInformation(item.second, pkt);
        if (sendRet != UBSE_OK) {
            UBSE_LOG_INFO << "[ELECTION] SendQueryInformation failed." << item.second;
        }
    }
}
static void UpdateGroupStateAndRedirect(const InterGroupInfo& reply, std::mutex& queryMtx,
                                        std::unordered_map<UBSE_ID_TYPE, GroupSummaryInfo>& groupStates)
{
    // 检查回复者是否为真正的Master
    bool needUpdateTarget = false;
    UBSE_ID_TYPE realMasterId;
    if (!reply.nodeId.empty() && !reply.groupMasterId.empty() && reply.nodeId != reply.groupMasterId) {
        needUpdateTarget = true;
        realMasterId = reply.groupMasterId;
    }

    // 更新本地缓存（加锁保护）
    {
        std::lock_guard<std::mutex> lock(queryMtx);
        if (!reply.groupId.empty() && !reply.groupMasterId.empty()) {
            groupStates[reply.groupId] = {reply.groupId, reply.groupMasterId, reply.groupStandbyId};
        }
    }

    // 如果回复者不是真正的Master，重定向到真正的Master
    if (needUpdateTarget && !reply.groupId.empty()) {
        RoleMgr::GetInstance().UpdateDiscoveryTarget(reply.groupId, realMasterId);
    }
}

void AsyncDealQueryReply(void* ctx, void* recv, uint32_t len, int32_t result)
{
    auto* context = static_cast<CallbackQueryCtx*>(ctx);
    if (context == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Received null context in callback";
        return;
    }
    const auto& nodeId = context->destId;
    auto& groupStates = *context->groupStates;
    auto& queryMtx = *context->queryMtx;
    if (result != UBSE_OK) {
        UBSE_LOG_INFO << "[ELECTION] Failed to query information.";
        return;
    }
    UbseBaseMessagePtr respMsg = new (std::nothrow) message::UbseElectionGroupInfoSimpo();
    if (respMsg == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to allocate response message.";
        return;
    }
    auto ret = respMsg->SetInputRawData(static_cast<uint8_t*>(recv), len);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] SendQueryInformation failed." << FormatRetCode(ret);
        return;
    }
    ret = respMsg->Deserialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to deserialize response message." << FormatRetCode(ret);
        return;
    }
    auto* replyMsg = dynamic_cast<UbseElectionGroupInfoSimpo*>(respMsg.Get());
    if (!replyMsg) {
        UBSE_LOG_ERROR << "[ELECTION] cast to UbseElectionGroupInfoSimpo failed";
        return;
    }
    InterGroupInfo reply = replyMsg->GetInterGroupInfo();
    if (reply.type == ELECTION_PKT_REPLY_GLOBAL_STOP) {
        UBSE_LOG_DEBUG << "[ELECTION] node = " << nodeId <<"stopped.";
        return;
    }
    UpdateGroupStateAndRedirect(reply, queryMtx, groupStates);
    SafeDelete(context);
}

UbseResult RoleMgr::SendQueryInformation(UBSE_ID_TYPE destId, const InterGroupInfo &pkt)
{
    UbseContext &ubseContext = UbseContext::GetInstance();
    auto rackComModule = ubseContext.GetModule<UbseComModule>();
    if (rackComModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] get rackComModule failed";
        return UBSE_ERROR;
    }
    InterGroupInfo electionPkt{ pkt };
    UbseBaseMessagePtr electionSimpoPtr = new (std::nothrow) UbseElectionGroupInfoSimpo(electionPkt);
    if (electionSimpoPtr == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Newing RackElectionPktSimpo failed.";
        return UBSE_ERROR;
    }
    ubse::com::SendParam sendParam(destId, static_cast<uint16_t>(UbseModuleCode::ELECTION),
                                   static_cast<uint16_t>(UbseElectionOpCode::ELECTION_INTER_GROUP_INFO),
                                   UbseChannelType::NORMAL);
    auto context = new (std::nothrow) CallbackQueryCtx;
    if (context == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] New context failed.";
        return UBSE_ERROR_NULLPTR;
    }
    std::unique_lock<std::mutex> lock(queryMutex_);
    context->groupStates = &groupStates;
    context->destId = destId;
    context->queryMtx = &queryMutex_;
    ubse::com::UbseComCallback callback;
    callback.cb = AsyncDealQueryReply;
    callback.cbCtx = reinterpret_cast<void *>(context);
    lock.unlock();
    auto retCode = rackComModule->RpcAsyncSend(sendParam, electionSimpoPtr, callback);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] RpcSend dispatch failed : " << destId;
        SafeDelete(context);
        return retCode;
    }
    return UBSE_OK;
}

std::vector<UBSE_ID_TYPE> RoleMgr::GetManagingGroupMasterIds()
{
    std::vector<UBSE_ID_TYPE> pdGroupMasterIds{};
    std::unique_lock<std::mutex> lock(queryMutex_);
    for (const auto &kv : groupStates) {
        pdGroupMasterIds.push_back(kv.second.groupMasterId);
    }
    return pdGroupMasterIds;
}

UbseResult RoleMgr::GetGroupState(const UBSE_ID_TYPE &groupId, GroupSummaryInfo &state)
{
    std::lock_guard<std::mutex> lock(queryMutex_);
    auto it = groupStates.find(groupId);
    if (it == groupStates.end()) {
        UBSE_LOG_WARN << "[ELECTION] Group " << groupId << " not found in groupStates";
        return UBSE_ERROR;
    }
    state = it->second;
    return UBSE_OK;
}

std::unordered_map<UBSE_ID_TYPE, GroupSummaryInfo> RoleMgr::GetAllGroupStates()
{
    std::lock_guard<std::mutex> lock(queryMutex_);
    return groupStates;
}

void RoleMgr::UpdateDiscoveryTarget(const UBSE_ID_TYPE &groupId, const UBSE_ID_TYPE &targetNodeId)
{
    if (groupId.empty() || targetNodeId.empty()) {
        UBSE_LOG_WARN << "[ELECTION] UpdateDiscoveryTarget invalid: groupId=" << groupId
        << ", targetNodeId=" << targetNodeId;
        return;
    }

    std::lock_guard<std::mutex> lock(discoveryMtx_);
    discoveryTargetByGroup[groupId] = targetNodeId;
    connFailedCntByGroup[groupId] = 0;
    UBSE_LOG_INFO << "[ELECTION] Update discovered connection target: groupId=" << groupId
    << ", targetNodeId=" << targetNodeId;
}

void RoleMgr::InitManagingGroupCount()
{
    auto nodesMap = nodeMgr::GetAllNodesStoredByGroup();
    uint16_t totalGroupCount = nodesMap.size();
    // 校验总组数必须为偶数
    if (totalGroupCount % NO_2 != 0) {
        UBSE_LOG_ERROR << "[ELECTION] Invalid totalGroupCount=" << totalGroupCount
                       << ", expected even number. Please check cluster configuration.";
        managingGroupCount_ = 0;  // 无效值
        return;
    }

    managingGroupCount_ = totalGroupCount / NO_2;
    if (managingGroupCount_ == 0) {
        managingGroupCount_ = 1;
    }
    UBSE_LOG_INFO << "[ELECTION] InitManagingGroupCount: managingGroupCount_=" << managingGroupCount_
                  << ", totalGroupCount=" << totalGroupCount;
}

bool RoleMgr::IsManagingGroup(const UBSE_ID_TYPE &groupId)
{
    if (managingGroupCount_ == 0) {
        UBSE_LOG_ERROR << "[ELECTION] managingGroupCount_ not initialized";
        return false;
    }

    uint32_t groupIdInt = static_cast<uint32_t>(std::stoi(groupId));
    return groupIdInt <= managingGroupCount_;
}

std::unordered_map<std::string, std::string> RoleMgr::ComputeDiscoveryTargets(const std::string &myNodeId,
    const std::unordered_map<uint16_t, std::vector<nodeMgr::UbseNodeStaticInfo>> &nodeMap)
{
    std::unordered_map<std::string, std::string> result;
    std::vector<uint16_t> sortedGroupIds;
    for (const auto &kv : nodeMap) {
        sortedGroupIds.push_back(kv.first);
    }
    std::sort(sortedGroupIds.begin(), sortedGroupIds.end());
    auto nodeInfo = nodeMgr::GetUbseNodeById(myNodeId);
    uint16_t myGroupId = nodeInfo.groupId;
    if (myGroupId == 0 || sortedGroupIds.empty()) {
        return result;
    }
    auto getFirstNodeId = [&nodeMap](uint16_t groupId) -> std::string {
        auto it = nodeMap.find(groupId);
        if (it == nodeMap.end() || it->second.empty()) { return ""; }
        std::string firstId = it->second[0].nodeId;
        uint16_t firstVal = static_cast<uint16_t>(std::stoi(firstId));
        for (const auto &node : it->second) {
            uint16_t nodeVal = static_cast<uint16_t>(std::stoi(node.nodeId));
            if (nodeVal < firstVal) {
                firstId = node.nodeId;
                firstVal = nodeVal;
            }
        }
        return firstId;
    };
    if (IsManagingGroup(std::to_string(myGroupId))) {
        for (size_t i = 0; i < static_cast<size_t>(managingGroupCount_); ++i) {
            uint16_t pdGroupId = sortedGroupIds[i];
            std::string firstNodeId = getFirstNodeId(pdGroupId);
            if (!firstNodeId.empty()) {
                result[std::to_string(pdGroupId)] = firstNodeId;
            }
        }
    } else {
        size_t mountIndex = static_cast<size_t>(myGroupId - managingGroupCount_ - 1);
        size_t pdIndex = mountIndex % static_cast<size_t>(managingGroupCount_);
        uint16_t targetPdGroupId = sortedGroupIds[pdIndex];
        std::string firstNodeId = getFirstNodeId(targetPdGroupId);
        if (!firstNodeId.empty()) {
            result[std::to_string(targetPdGroupId)] = firstNodeId;
        }
    }
    return result;
}
} // namespace ubse::election