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
#include <future>
#include <shared_mutex>
#include "role/ubse_election_role_mgr.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election_node_mgr.h"
#include "ubse_election_pkt_simpo.h"
#include "ubse_election_reply_pkt_simpo.h"
#include "ubse_election_utils.h"
#include "ubse_event_module.h"
namespace ubse::election {
using namespace ubse::context;
using namespace ubse::com;
using namespace ubse::election::message;
using namespace ubse::log;
using namespace ubse::event;
using namespace ubse::election::utils;

UBSE_DEFINE_THIS_MODULE("ubse");

uint32_t UbseElectionCommMgr::Connect(const UBSE_ID_TYPE &dstIp)
{
    {
        std::shared_lock<std::shared_mutex> lock(mtx_);
        std::string dstIdTemp;
        for (auto &connectedNodeId : connectSuccessNodes_) {
            if (UbseElectionNodeMgr::GetInstance().GetNodeIdByIp(dstIp, dstIdTemp) == UBSE_OK &&
                connectedNodeId == dstIdTemp) {
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
    option.nodeId = dstIp;
    option.ip = dstIp;
    option.channelType = UbseChannelType::NORMAL;
    auto ret = UbseElectionNodeMgr::GetInstance().GetPortByIp(option.ip, option.port);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to fetch node information id : " << option.nodeId;
        return UBSE_ERROR;
    }
    std::string remoteId;
    auto retCode = ubseComModule->ConnectWithOption(option, remoteId);
    if (retCode != UBSE_OK) {
        UBSE_LOG_WARN << "[ELECTION] Connect failed: " << option.ip;
    } else {
        {
            std::unique_lock<std::shared_mutex> writeLock(mtx_);
            UBSE_LOG_INFO << "[ELECTION] Connect successfully, nodeId = " << remoteId;
            if (UbseElectionNodeMgr::GetInstance().UpdateNodeIdWithConnect(dstIp, remoteId) == UBSE_OK) {
                connectSuccessNodes_.emplace_back(remoteId);
            }
        }
    }
    return UBSE_OK;
}

uint32_t UbseElectionCommMgr::DisConnect(const UBSE_ID_TYPE &dstId)
{
    {
        std::shared_lock<std::shared_mutex> lock(mtx_);
        if (std::find(connectSuccessNodes_.begin(), connectSuccessNodes_.end(), dstId) == connectSuccessNodes_.end()) {
            UBSE_LOG_DEBUG << "[ELECTION] The node is disconnected , node_id = " << dstId;
            return UBSE_OK;
        }
    }

    UbseContext &ctx = UbseContext::GetInstance();
    auto ubseComModule = ctx.GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] get UbseComModule failed";
        return UBSE_ERROR;
    }
    auto retCode = ubseComModule->RemoveChannel(dstId, UbseChannelType::NORMAL);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] RemoveChannel failed";
    } else {
        {
            std::unique_lock<std::shared_mutex> writeLock(mtx_);
            UBSE_LOG_INFO << "[ELECTION] Remove successfully, nodeId = " << dstId;
            connectSuccessNodes_.erase(std::remove(connectSuccessNodes_.begin(), connectSuccessNodes_.end(), dstId),
                                       connectSuccessNodes_.end());
        }
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
                                   static_cast<uint16_t>(UbseElectionOpCode::ELECTION_PKT), UbseChannelType::NORMAL);
    UbseBaseMessagePtr electionReplySimpoPtr = new (std::nothrow) UbseElectionReplyPktSimpo(reply);
    if (electionReplySimpoPtr == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Newing UbseElectionReplyPktSimpo failed.";
        return UBSE_ERROR_NULLPTR;
    }
    auto future = std::async(std::launch::async, [ubseComModule, sendParam, electionSimpoPtr, electionReplySimpoPtr]() {
                      return ubseComModule->RpcSend(sendParam, electionSimpoPtr, electionReplySimpoPtr);
                  }).share();
    auto status = future.wait_for(std::chrono::seconds(UbseElectionNodeMgr::GetInstance().GetHeartBeatTime()));
    if (status == std::future_status::ready) {
        auto ret = future.get();
        // 将响应对象从原始指针转换为UbseResourceRpcResponseSimpo类型
        auto response = UbseBaseMessage::DeConvert<UbseElectionReplyPktSimpo>(electionReplySimpoPtr);
        // 获取响应结果
        reply = response->GetElectionReplyPkt();
        return ret;
        // 处理 ret
    } else {
        UBSE_LOG_ERROR << "[ELECTION] RpcSend dispatch timeout when send pkt to " << destID;
        return UBSE_ERROR;
    }
}

std::vector<UBSE_ID_TYPE> UbseElectionCommMgr::GetConnectedNodes() const
{
    std::shared_lock<std::shared_mutex> lock(mtx_);
    return connectSuccessNodes_;
}

void UbseElectionCommMgr::ElectionNodeDownNotify(const std::string &nodeId)
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
        RoleMgr::GetInstance().GetRole()->SetNodeDownStatus(nodeId);
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
            if (it.ubseLinkState == 1 && it.changeChType == "Normal") {
                ElectionNodeDownNotify(it.nodeId);
                UBSE_LOG_INFO << "[ELECTION] Event disconnect to node id is: " << it.nodeId
                              << ", eventId is: " << eventId;
                connectSuccessNodes_.erase(
                    std::remove(connectSuccessNodes_.begin(), connectSuccessNodes_.end(), it.nodeId),
                    connectSuccessNodes_.end());
            }
        }
    }
    return UBSE_OK;
}

UbseResult UbseElectionCommMgr::ElectionFaultHandler(std::string &eventId, std::string &eventMessage)
{
    std::string faultNodeId;
    std::string faultType;
    if (!parseFaultEventMsg(eventMessage, faultNodeId, faultType)) {
        UBSE_LOG_ERROR << "[ELECTION] parse FaultEventMsg failed.";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "[ELECTION] Fault Event disconnect to nodeId=" << faultNodeId << ", faultType=" << faultType
                  << ", eventId is: " << eventId;
    if (faultType == "1007") {
        if (DisConnect(faultNodeId) != UBSE_OK) {
            UBSE_LOG_ERROR << "[ELECTION] DisConnect failed, nodeId=" << faultNodeId;
            return UBSE_ERROR;
        }
        ElectionNodeDownNotify(faultNodeId);
    }
    return UBSE_OK;
}

UbseResult UbseElectionCommMgr::ElectionTopoChangeHandler(std::string &eventId, std::string &eventMessage)
{
    UBSE_LOG_INFO << "[ELECTION] start to exec TopoChangeHandler, eventId = " << eventId;
    UbseElectionNodeMgr::GetInstance().ParseAllNodesVector();
    return UBSE_OK;
}

UbseResult UbseElectionCommMgr::NewChannelCB(const std::string &remoteIp, const std::string &remoteNodeId)
{
    // remoteInfo为对端IP
    UBSE_LOG_DEBUG << "[ELECTION] start to NCCB.";
    std::unordered_map<std::string, UBSE_ID_TYPE> nodeIpMap;
    if (UbseElectionNodeMgr::GetInstance().GetNodeIpMap(nodeIpMap) != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Get nodeIpMap failed.";
        return UBSE_ERROR;
    }
    if (nodeIpMap.find(remoteIp) != nodeIpMap.end()) {
        UBSE_LOG_DEBUG << "[ELECTION] Updating for ip = " << remoteIp << " to id = " << remoteNodeId;
        UbseElectionNodeMgr::GetInstance().UpdateNodeIdWithConnect(remoteIp, remoteNodeId);
        return UBSE_OK;
    }
    UBSE_LOG_DEBUG << "[ELECTION] NCCB Ip Not Found.";
    return UBSE_ERROR;
}

UbseResult UbseElectionCommMgr::ElectionSubEvent()
{
    auto eventModule = UbseContext::GetInstance().GetModule<UbseEventModule>();
    if (eventModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to get UbseEventModule";
        return UBSE_ERROR;
    }
    auto ret = eventModule->UbseSubEvent(
        UBSE_EVENT_NODE_STATE,
        [this](std::string &eventId, std::string &eventMessage) -> u_int32_t {
            return ElectionResponseHandler(eventId, eventMessage);
        },
        HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to SubEvent" << UBSE_EVENT_NODE_STATE << "," << FormatRetCode(ret);
        return ret;
    }
    std::string panicAndRebootFaultEventId = "UbsePanicAndRebootFaultEvent";
    ret = eventModule->UbseSubEvent(
        panicAndRebootFaultEventId,
        [this](std::string &eventId, std::string &eventMessage) -> u_int32_t {
            return ElectionFaultHandler(eventId, eventMessage);
        },
        HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to SubEvent" << panicAndRebootFaultEventId << ", " << FormatRetCode(ret);
        return ret;
    }
    ret = eventModule->UbseSubEvent(
        UBSE_EVENT_NODE_TOPO_LINK_CHANGE,
        [this](std::string &eventId, std::string &eventMessage) -> u_int32_t {
            return ElectionTopoChangeHandler(eventId, eventMessage);
        },
        HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Failed to SubEvent" << UBSE_EVENT_NODE_TOPO_LINK_CHANGE << ", "
                       << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseElectionCommMgr::Start()
{
    if (ElectionSubEvent() != UBSE_OK) {
        return UBSE_ERROR;
    }
    Node currentNode;
    if (UbseElectionNodeMgr::GetInstance().GetMyselfNode(currentNode) != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] Get myself node failed";
        return UBSE_ERROR;
    }
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto retStartComService = ubseComModule->StartComService(
        currentNode.id, currentNode.ip,
        [this](const std::string &remoteIp, const std::string &remoteNodeId) {
            return NewChannelCB(remoteIp, remoteNodeId);
        },
        nullptr);
    if (retStartComService != UBSE_OK) {
        UBSE_LOG_ERROR << "[ELECTION] StartComService failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::election
