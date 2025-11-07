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

#include "ubse_election_comm_mgr.h"
#include <algorithm>
#include <shared_mutex>
#include "ubse_event_module.h"
#include "role/ubse_election_role_mgr.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election_node_mgr.h"
#include "ubse_election_pkt_simpo.h"
#include "ubse_election_reply_pkt_simpo.h"
#include "ubse_election_utils.h"
namespace ubse::election {
using namespace ubse::context;
using namespace ubse::com;
using namespace ubse::election::message;
using namespace ubse::log;
using namespace ubse::event;
using namespace ubse::election::utils;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_ELECTION_MID)

uint32_t UbseElectionCommMgr::Connect(const UBSE_ID_TYPE &dstId)
{
    {
        std::shared_lock<std::shared_mutex> lock(mtx);
        for (auto &connectSuccess : connectSuccessNodes_) {
            if (connectSuccess == dstId) {
                return UBSE_OK;
            }
        }
    }

    UbseContext &ctx = UbseContext::GetInstance();
    auto ubseComModule = ctx.GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] get UbseComModule failed";
        return UBSE_ERROR;
    }
    ConnectOption option;
    option.nodeId = dstId;
    auto ret = UbseElectionNodeMgr::GetInstance().GetNodeInfoByID(option.nodeId, option.ip, option.port);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to fetch node information id : " << option.nodeId;
        return UBSE_ERROR;
    }
    option.channelType = UbseChannelType::HEARTBEAT;
    auto retCode = ubseComModule->ConnectWithOption(option);
    if (retCode != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] Connect failed: " << option.nodeId;
    } else {
        std::unique_lock<std::shared_mutex> lock(mtx);
        connectSuccessNodes_.emplace_back(dstId);
    }
    return UBSE_OK;
}
uint32_t UbseElectionCommMgr::DisConnect(const UBSE_ID_TYPE &dstId)
{
    {
        std::shared_lock<std::shared_mutex> lock(mtx);
        if (std::find(connectSuccessNodes_.begin(), connectSuccessNodes_.end(), dstId) == connectSuccessNodes_.end()) {
            UBSE_LOG_DEBUG << "[ELECTION] The node is disconnected :" << dstId;
            return UBSE_OK;
        }
    }

    UbseContext &ctx = UbseContext::GetInstance();
    auto ubseComModule = ctx.GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] get UbseComModule failed";
        return UBSE_ERROR;
    }
    auto retCode = ubseComModule->RemoveChannel(dstId, UbseChannelType::HEARTBEAT);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] RemoveChannel failed";
    } else {
        std::unique_lock<std::shared_mutex> lock(mtx);
        connectSuccessNodes_.erase(std::remove(connectSuccessNodes_.begin(), connectSuccessNodes_.end(), dstId),
                                   connectSuccessNodes_.end());
    }
    return UBSE_OK;
}

uint32_t UbseElectionCommMgr::SendElectionPkt(UBSE_ID_TYPE destID, const ElectionPkt &pkt, ElectionReplyPkt &reply)
{
    UbseContext &ubseContext = UbseContext::GetInstance();
    auto ubseComModule = ubseContext.GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] get ubseComModule failed";
        return UBSE_ERROR;
    }
    ElectionPkt electionPkt{pkt};
    UbseBaseMessagePtr electionSimpoPtr = new (std::nothrow) UbseElectionPktSimpo(electionPkt);
    if (electionSimpoPtr == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Newing UbseElectionPktSimpo failed.";
        return UBSE_ERROR_NULLPTR;
    }
    // 需要定义一下ModuleCode和OpCode
    ubse::com::SendParam sendParam(destID, static_cast<uint16_t>(UbseModuleCode::ELECTION),
                                   static_cast<uint16_t>(UbseOpCode::ELECTION_PKT), UbseChannelType::HEARTBEAT);
    UbseBaseMessagePtr electionReplySimpoPtr = new (std::nothrow) UbseElectionReplyPktSimpo(reply);
    if (electionReplySimpoPtr == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Newing UbseElectionReplyPktSimpo failed.";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = ubseComModule->RpcSend(sendParam, electionSimpoPtr, electionReplySimpoPtr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] RpcSend dispatch failed : " << destID;
    }
    // 将响应对象从原始指针转换为UbseResourceRpcResponseSimpo类型
    auto response = UbseBaseMessage::DeConvert<UbseElectionReplyPktSimpo>(electionReplySimpoPtr);
    // 获取响应结果
    reply = response->GetElectionReplyPkt();
    return ret;
}
std::vector<UBSE_ID_TYPE> UbseElectionCommMgr::GetConnectedNodes() const
{
    std::shared_lock<std::shared_mutex> lock(mtx);
    return connectSuccessNodes_;
}

void UbseElectionCommMgr::ElectionNodeDownNotify(const NodeLinkInfo &info)
{
    Node currentNode;
    UBSE_ID_TYPE masterId;
    UbseElectionNodeMgr &ubseElectionNodeMgr = UbseElectionNodeMgr::GetInstance();
    UbseResult ret = ubseElectionNodeMgr.GetMyselfNode(currentNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Get myself nodeId failed";
        return;
    }
    masterId = RoleMgr::GetInstance().GetRole()->GetMasterNode();
    if (masterId == currentNode.id) {
        RoleMgr::GetInstance().GetRole()->SetNodeDownStatus(info.nodeId);
    }
}

void UbseElectionCommMgr::NodeShouldReconnect(const NodeLinkInfo &info)
{
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] election module not load";
        return;
    }
    auto eventModule = context::UbseContext::GetInstance().GetModule<event::UbseEventModule>();
    if (eventModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Get eventModule failed";
        return;
    }
    if (module->IsLeader() && std::find(connectSuccessNodes_.begin(), connectSuccessNodes_.end(), info.nodeId) !=
                                  connectSuccessNodes_.end()) {
        UBSE_LOG_INFO << "[ELECTION] Reconnect to node, id = " << info.nodeId;
        Node node{info.nodeId, "", NO_0};
        auto ret = UbseElectionNodeMgr::GetInstance().GetNodeInfoByID(node.id, node.ip, node.port);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[ELECTION] Failed to fetch node information id : " << node.id;
            return;
        }
        UbseRoleInfo roleInfo{};
        ret = UbseGetRoleInfo(roleInfo, node.id);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[ELECTION] Failed to fetch node role, id : " << node.id;
            return;
        }
        std::string eventMessage = node.id + "-" + node.ip + "-" + std::to_string(node.port) + "-" + roleInfo.nodeRole;
        ret = eventModule->UbsePubEvent(UBSE_EVENT_NODE_RECONNECT, eventMessage);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "[ELECTION] Failed to PubEvent" << UBSE_EVENT_NODE_RECONNECT
                           << ", nodeId = " << info.nodeId << "," << FormatRetCode(ret);
            return;
        }
    }
}

UbseResult UbseElectionCommMgr::ElectionResponseHandler(std::string &eventId, std::string &eventMessage)
{
    std::vector<Node> lcneNodes;
    auto ret = UbseElectionNodeMgr::GetInstance().GetAllNode(lcneNodes);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] GetAllNode failed.";
        return UBSE_ERROR;
    }
    std::vector<NodeLinkInfo> nodeLinkList;
    nodeLinkList = ParseEventMessage(eventMessage);
    for (auto it : nodeLinkList) {
        // 判断 it.nodeId 是否存在于 lcneNodes 中
        auto itFound =
            std::find_if(lcneNodes.begin(), lcneNodes.end(), [&it](const Node &node) { return node.id == it.nodeId; });
        if (itFound != lcneNodes.end()) {
            if (it.ubseLinkState == 1 && it.changeChType == "Heartbeat") {
                ElectionNodeDownNotify(it);
                UBSE_LOG_INFO << "[ELECTION] Event disconnect to node id is: " << it.nodeId
                            << ", eventId is: " << eventId;
                connectSuccessNodes_.erase(
                    std::remove(connectSuccessNodes_.begin(), connectSuccessNodes_.end(), it.nodeId),
                    connectSuccessNodes_.end());
            } else if (it.ubseLinkState == 1 && it.changeChType == "Normal") {
                UBSE_LOG_INFO << "[ELECTION] Received the node link down event, event = "
                            << eventId << ", id = " << it.nodeId;
                NodeShouldReconnect(it);
            }
        }
    }
    return UBSE_OK;
}

UbseResult UbseElectionCommMgr::Start()
{
    auto eventModule = UbseContext::GetInstance().GetModule<UbseEventModule>();
    if (eventModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get UbseEventModule";
        return UBSE_ERROR;
    }
    std::string subId = "ubse.node.state";
    auto ret = eventModule->UbseSubEvent(
        subId,
        [this](std::string &eventId, std::string &eventMessage) -> u_int32_t {
            return ElectionResponseHandler(eventId, eventMessage);
        },
        HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to SubEvent" << subId << "," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::election
