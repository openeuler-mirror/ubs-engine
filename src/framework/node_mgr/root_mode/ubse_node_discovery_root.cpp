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

#include "ubse_node_discovery_root.h"

#include "message/ubse_node_discovery_list_serial.h"
#include "message/ubse_node_discovery_serial.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_event_module.h"
#include "ubse_logger_module.h"
#include "ubse_node_static_info_mgr.h"
#include "ubse_timer.h"
#include "ubse_node_discovery_common.h"
#include "ubse_node_mgr_root_mode_utils.h"
#include "ubse_node_mgr.h"

UBSE_DEFINE_THIS_MODULE("ubse");
namespace ubse::nodeMgr {
using namespace ubse::context;
using namespace ubse::com;
using namespace ubse::timer;
using namespace ubse::event;

constexpr uint32_t BROADCAST_DURATION = 1;

UbseNodeDiscoveryRoot *UbseNodeDiscoveryRoot::instance_ = nullptr;
std::mutex UbseNodeDiscoveryRoot::instanceMutex_{};

UbseResult UbseNodeDiscoveryRoot::Init()
{
    auto rootIps = UbseNodeStaticInfoMgr::GetInstance().GetRootIpList();
    const auto &currentAddr = UbseNodeStaticInfoMgr::GetInstance().GetCurrentNode().addr;
    if (std::find(rootIps.begin(), rootIps.end(), currentAddr) == rootIps.end()) {
        return UBSE_OK;
    }

    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "get com module failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    UbseComBaseMessageHandlerPtr nodeDiscoveryReportHandler = new (std::nothrow) UbseNodeDiscoveryReportHandler();
    if (nodeDiscoveryReportHandler == nullptr) {
        UBSE_LOG_ERROR << "get node discovery report handler failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RegRpcService<UbseNodeDiscoverySerial,
        UbseNodeDiscoveryListSerial>(nodeDiscoveryReportHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "register report handler failed, " << FormatRetCode(ret);
        return ret;
    }

    taskExecutor_ = UbseTaskExecutor::Create("UbseNodeDiscoveryRoot", NO_2, NO_2048);
    if (taskExecutor_ == nullptr || !taskExecutor_->Start()) {
        UBSE_LOG_ERROR << "start root discovery thread failed";
        return UBSE_ERROR;
    }

    ret = SubscribeNodeStateEvent();
    if (ret != UBSE_OK) {
        return ret;
    }

    for (const auto &rootIp : rootIps) {
        if (rootIp == currentAddr) {
            continue;
        }
        ExecBroadcastToRoot(rootIp);
    }
    return UBSE_OK;
}

void UbseNodeDiscoveryRoot::UnInit()
{
    if (taskExecutor_ != nullptr) {
        taskExecutor_->Stop();
    }
    std::vector<std::string> retryTasks{};
    {
        std::lock_guard lock(retryMutex_);
        retryTasks.assign(retryBroadcastTasks_.begin(), retryBroadcastTasks_.end());
        retryBroadcastTasks_.clear();
    }
    for (const auto &taskName : retryTasks) {
        UbseTimerHandlerUnregister(taskName);
    }
}

void UbseNodeDiscoveryRoot::BroadcastClusterNodeInfo()
{
    std::unordered_set<std::string> nodes{};
    {
        std::lock_guard lock(connectedNodesMutex_);
        nodes = connectedNodes_;
    }
    for (const auto &remoteId : nodes) {
        ExecBroadcastToConnectedNode(remoteId);
    }
}

void UbseNodeDiscoveryRoot::AddConnectedNode(const std::string &nodeId)
{
    std::lock_guard lock(connectedNodesMutex_);
    connectedNodes_.emplace(nodeId);
}

void UbseNodeDiscoveryRoot::RemoveConnectedNode(const std::string &nodeId)
{
    std::lock_guard lock(connectedNodesMutex_);
    connectedNodes_.erase(nodeId);
}

UbseResult UbseNodeDiscoveryRoot::SubscribeNodeStateEvent()
{
    auto eventModule = UbseContext::GetInstance().GetModule<UbseEventModule>();
    if (eventModule == nullptr) {
        UBSE_LOG_ERROR << "get event module failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    auto ret = eventModule->UbseSubEvent(
        UBSE_EVENT_NODE_STATE,
        [this](std::string &eventId, std::string &eventMessage) -> uint32_t {
            return HandleNodeStateEvent(eventId, eventMessage);
        },
        MEDIUM);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "subscribe node state event failed, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseNodeDiscoveryRoot::HandleNodeStateEvent(const std::string &eventId, const std::string &eventMessage)
{
    auto linkList = utils::UbseNodeMgrRootModeUtils::ParseNodeStateEventMessage(eventMessage);
    if (linkList.empty()) {
        UBSE_LOG_ERROR << "parse node state event message failed, eventMessage=" << eventMessage;
        return UBSE_ERROR_INVAL;
    }
    uint32_t linkStateDown = 1;
    for (const auto &linkInfo : linkList) {
        if (linkInfo.ubseLinkState != linkStateDown || linkInfo.channelType != "Normal") {
            continue;
        }
        RemoveConnectedNode(linkInfo.nodeId);
        std::string nodeIp{};
        auto ret = UbseNodeDiscoveryCommon::GetInstance().GetClusterNodeIpById(linkInfo.nodeId, nodeIp);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "get node ip by id failed, nodeId=" << linkInfo.nodeId << ", " << FormatRetCode(ret);
            continue;
        }
        auto rootIps = UbseNodeStaticInfoMgr::GetInstance().GetRootIpList();
        if (std::find(rootIps.begin(), rootIps.end(), nodeIp) == rootIps.end()) {
            continue;
        }
        ExecBroadcastToRoot(nodeIp);
    }
    return UBSE_OK;
}

void UbseNodeDiscoveryRoot::CleanupRetryTask(const std::string &taskName)
{
    UbseTimerHandlerUnregister(taskName);
    std::lock_guard lock(retryMutex_);
    retryBroadcastTasks_.erase(taskName);
}

void UbseNodeDiscoveryRoot::ScheduleRetry(const std::string &taskName, std::function<UbseResult()> retryFunc)
{
    if (g_globalStop.load()) {
        return;
    }
    UbseTimerHandlerRegister(taskName, std::move(retryFunc), BROADCAST_DURATION);
    std::lock_guard lock(retryMutex_);
    retryBroadcastTasks_.emplace(taskName);
}

void UbseNodeDiscoveryRoot::ExecBroadcastToRoot(const std::string &rootIp)
{
    const auto taskName = "UbseBroadcastToRoot-" + rootIp;
    if (g_globalStop.load() || taskExecutor_ == nullptr) {
        CleanupRetryTask(taskName);
        return;
    }
    taskExecutor_->Execute([this, rootIp, taskName]() -> void {
        utils::NodeDiscoveryLockGuard lockGuard(taskName);
        UBSE_LOG_INFO << "start to broadcast to root=" << rootIp;
        auto ret = ConnectAndBroadcastToRoot(rootIp);
        if (ret == UBSE_OK) {
            CleanupRetryTask(taskName);
            return;
        }
        UBSE_LOG_ERROR << "broadcast to root=" << rootIp << " failed, " << FormatRetCode(ret);
        ScheduleRetry(taskName, [this, rootIp]() -> UbseResult { ExecBroadcastToRoot(rootIp); return UBSE_OK; });
    });
}

UbseResult UbseNodeDiscoveryRoot::ConnectAndBroadcastToRoot(const std::string &rootIp)
{
    auto ret = UbseNodeDiscoveryCommon::GetInstance().ConnectRootNode(rootIp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "connect to root=" << rootIp << " failed," << FormatRetCode(ret);
        return ret;
    }
    std::string remoteId;
    ret = UbseNodeDiscoveryCommon::GetInstance().GetClusterNodeIdByIp(rootIp, remoteId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get node id by ip failed, ip=" << rootIp << ", " << FormatRetCode(ret);
        return ret;
    }
    AddConnectedNode(remoteId);
    ret = BroadcastToNode(remoteId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "broadcast cluster node info to root=" << remoteId << " failed, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

void UbseNodeDiscoveryRoot::ExecBroadcastToConnectedNode(const std::string &remoteId)
{
    const auto taskName = "UbseBroadcastToConnectedNode-" + remoteId;
    if (g_globalStop.load() || taskExecutor_ == nullptr) {
        CleanupRetryTask(taskName);
        return;
    }
    taskExecutor_->Execute([this, remoteId, taskName]() -> void {
        utils::NodeDiscoveryLockGuard lockGuard(taskName);
        UBSE_LOG_INFO << "start to broadcast to node=" << remoteId;
        auto ret = BroadcastToNode(remoteId);
        if (ret == UBSE_OK) {
            CleanupRetryTask(taskName);
            return;
        }
        UBSE_LOG_ERROR << "broadcast to node=" << remoteId << " failed, " << FormatRetCode(ret);
        ScheduleRetry(taskName,
            [this, remoteId]() -> UbseResult { ExecBroadcastToConnectedNode(remoteId); return UBSE_OK; });
    });
}

UbseResult UbseNodeDiscoveryRoot::BroadcastToNode(const std::string &remoteId)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "get com module failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    SendParam sendParam(remoteId, static_cast<uint16_t>(UbseModuleCode::NODE_MGR),
                        static_cast<uint16_t>(UbseNodeMgrOpCode::NODE_INFO_BROADCAST),
                        UbseChannelType::NORMAL);
    std::vector<UbseNodeStaticInfo> nodes = GetAllNodes();
    UbseBaseMessagePtr ubseBaseMessagePtr = new (std::nothrow) UbseNodeDiscoveryListSerial(nodes);
    if (ubseBaseMessagePtr == nullptr) {
        UBSE_LOG_ERROR << "new ubseNodeDiscoveryListSerial req failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseBaseMessagePtr ubseBaseMessageReplyPtr = new (std::nothrow) UbseNodeDiscoveryListSerial();
    if (ubseBaseMessageReplyPtr == nullptr) {
        UBSE_LOG_ERROR << "new ubseNodeDiscoveryListSerial resp failed";
        return UBSE_ERROR_NULLPTR;
    }
    return comModule->RpcSend(sendParam, ubseBaseMessagePtr, ubseBaseMessageReplyPtr);
}

UbseResult UbseNodeDiscoveryReportHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                  UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseNodeDiscoverySerial>(req);
    auto response = UbseBaseMessage::DeConvert<UbseNodeDiscoveryListSerial>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = request->Deserialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "cluster node info deserialize failed, " << FormatRetCode(ret);
        return ret;
    }
    auto normalNode = request->GetUbseSuperNode();
    UbseNodeStaticInfoMgr::GetInstance().SetNodes({normalNode});
    UbseNodeDiscoveryRoot::GetInstance().BroadcastClusterNodeInfo();
    UbseNodeDiscoveryRoot::GetInstance().AddConnectedNode(normalNode.nodeId);
    auto clusterNodes = GetAllNodes();
    response->SetUbseNodeList(clusterNodes);
    auto eventModule = UbseContext::GetInstance().GetModule<UbseEventModule>();
    if (eventModule == nullptr) {
        UBSE_LOG_ERROR << "get event module failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    std::string eventMessage = "Receive Node Discovery Report, nodeId=" + normalNode.nodeId;
    return eventModule->UbsePubEvent(UBSE_EVENT_NODE_DISCOVERY, eventMessage);
}

uint16_t UbseNodeDiscoveryReportHandler::GetOpCode()
{
    return static_cast<uint16_t>(ubse::com::UbseNodeMgrOpCode::NODE_INFO_REPORT);
}

uint16_t UbseNodeDiscoveryReportHandler::GetModuleCode()
{
    return static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_MGR);
}
} // namespace ubse::nodeMgr