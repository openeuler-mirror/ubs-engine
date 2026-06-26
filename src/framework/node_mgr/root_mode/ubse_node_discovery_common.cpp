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

#include "ubse_node_discovery_common.h"

#include "adapter_plugins/mti/ubse_smbios.h"
#include "message/ubse_node_discovery_list_serial.h"
#include "message/ubse_node_discovery_serial.h"
#include "ubse_com_module.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_event_module.h"
#include "ubse_logger.h"
#include "ubse_net_util.h"
#include "ubse_node_static_info_mgr.h"
#include "ubse_str_util.h"
#include "ubse_timer.h"

UBSE_DEFINE_THIS_MODULE("ubse");
namespace ubse::nodeMgr {
using namespace ubse::com;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::log;
using namespace ubse::timer;
using namespace ubse::adapter_plugins::smbios;
using namespace ubse::utils;
using namespace ubse::election;
using namespace ubse::event;

constexpr uint32_t REPORT_RETRY_DURATION = 1;
constexpr uint32_t MAX_RETRY_COUNT = 3;

UbseNodeDiscoveryCommon *UbseNodeDiscoveryCommon::instance_ = nullptr;

UbseResult UbseNodeDiscoveryCommon::Init()
{
    std::vector<std::string> rootIpList = UbseNodeStaticInfoMgr::GetInstance().GetRootIpList();
    if (rootIpList.empty()) {
        UBSE_LOG_ERROR << "root ip list is empty";
        return UBSE_ERROR_INVAL;
    }
    UbseNodeStaticInfo node{};
    auto ret = UbseNodeStaticInfoMgr::GetInstance().InitCurNodeInfo(node);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "init current node failed, " << FormatRetCode(ret);
        return ret;
    }
    bool isRoot = false;
    ret = GetLocalAddr(node, rootIpList, isRoot);
    if (ret != UBSE_OK) {
        return ret;
    }
    UbseNodeStaticInfoMgr::GetInstance().SetCurrentNode(node);

    ret = StartRpcServer();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "start rpc server failed, " << FormatRetCode(ret);
        return ret;
    }

    ret = RegisterComHandler();
    if (ret != UBSE_OK) {
        return ret;
    }

    if (isRoot) {
        UBSE_LOG_INFO << "current node is root, skip report";
        return UBSE_OK;
    }

    taskExecutor_ = UbseTaskExecutor::Create("UbseNodeDiscoveryNormal", NO_1, NO_1024);
    if (taskExecutor_ == nullptr || !taskExecutor_->Start()) {
        UBSE_LOG_ERROR << "start root discovery thread failed";
        return UBSE_ERROR_NULLPTR;
    }
    ret = SubscribeNodeStateEvent();
    if (ret != UBSE_OK) {
        return ret;
    }
    ExecNodeDiscoveryTask(defaultRoot_);
    return UBSE_OK;
}

std::string GenerateDiscoveryTaskName(const std::string &ip)
{
    return "UbseNodeDiscoveryReport-" + ip;
}

void UbseNodeDiscoveryCommon::UnInit()
{
    for (const auto &ip : UbseNodeStaticInfoMgr::GetInstance().GetRootIpList()) {
        UbseTimerHandlerUnregister(GenerateDiscoveryTaskName(ip));
    }
    if (taskExecutor_ != nullptr) {
        taskExecutor_->Stop();
    }
    std::unique_lock lock(reportRetryRecordsMutex_);
    reportRetryRecords_.clear();
}

UbseResult UbseNodeDiscoveryCommon::ConnectRootNode(const std::string &ip)
{
    UbseContext &ctx = UbseContext::GetInstance();
    auto ubseComModule = ctx.GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "get UbseComModule failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    ConnectOption option;
    option.nodeId = ip;
    option.ip = ip;
    option.channelType = UbseChannelType::NORMAL;
    option.port = TCP_LISTEN_PORT;
    std::string remoteId;
    auto retCode = ubseComModule->ConnectWithOption(option, remoteId);
    if (retCode != UBSE_OK) {
        UBSE_LOG_WARN << "connect failed, ip=" << option.ip << ", " << FormatRetCode(retCode);
    } else {
        std::unique_lock lock(mutex_);
        clusterNodeIdToIpMap_[remoteId] = ip;
        UBSE_LOG_INFO << "connect successfully, nodeId=" << remoteId;
    }
    return retCode;
}

UbseResult UbseNodeDiscoveryCommon::DisconnectRootNode(const std::string &ip)
{
    std::string nodeId{};
    auto retCode = GetClusterNodeIdByIp(ip, nodeId);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "get cluster node id by ip failed, ip=" << ip << ", " << FormatRetCode(retCode);
        return retCode;
    }
    UbseContext &ctx = UbseContext::GetInstance();
    auto ubseComModule = ctx.GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "get com module failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    retCode = ubseComModule->RemoveChannel(nodeId, UbseChannelType::NORMAL);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "remove channel failed, nodeId=" << nodeId << ", " << FormatRetCode(retCode);
    } else {
        std::unique_lock lock(mutex_);
        clusterNodeIdToIpMap_.erase(nodeId);
        UBSE_LOG_INFO << "remove successfully, nodeId=" << nodeId;
    }
    return retCode;
}

UbseResult UbseNodeDiscoveryCommon::GetClusterNodeIdByIp(const std::string &ip, std::string &nodeId)
{
    std::shared_lock lock(mutex_);
    for (const auto &pair : clusterNodeIdToIpMap_) {
        if (pair.second == ip) {
            nodeId = pair.first;
            return UBSE_OK;
        }
    }
    return UBSE_ERROR;
}

UbseResult UbseNodeDiscoveryCommon::GetClusterNodeIpById(const std::string &nodeId, std::string &ip)
{
    std::shared_lock lock(mutex_);
    auto it = clusterNodeIdToIpMap_.find(nodeId);
    if (it == clusterNodeIdToIpMap_.end()) {
        UBSE_LOG_ERROR << "nodeId=" << nodeId << " not found";
        return UBSE_ERROR;
    }
    ip = it->second;
    return UBSE_OK;
}

UbseResult UbseNodeDiscoveryCommon::GetLocalAddr(UbseNodeStaticInfo &node,
    const std::vector<std::string> &rootIpList, bool &isRoot)
{
    auto ret = UbseNetUtil::FindLocalIpInIpList(rootIpList, node.addr);
    if (ret == UBSE_ERROR_EMPTY) {
        uint32_t index = node.groupId % rootIpList.size();
        defaultRoot_ = rootIpList[index];
        UBSE_LOG_INFO << "node=" << node.nodeId << " select default root=" << defaultRoot_;
        ret = UbseNetUtil::FindLocalIpByRemote(defaultRoot_, node.addr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "init current node ip failed, " << FormatRetCode(ret);
            return ret;
        }
        isRoot = false;
        return UBSE_OK;
    }
    if (ret != UBSE_OK) {
        return ret;
    }
    isRoot = true;
    return UBSE_OK;
}

UbseResult UbseNodeDiscoveryCommon::StartRpcServer()
{
    auto node = UbseNodeStaticInfoMgr::GetInstance().GetCurrentNode();
    auto ubseComModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    return ubseComModule->StartComService(
        node.nodeId, node.addr,
        [this](const std::string &remoteIp, const std::string &remoteNodeId) {
            return NewChannelCallback(remoteIp, remoteNodeId);
        },
        nullptr);
}

UbseResult UbseNodeDiscoveryCommon::NewChannelCallback(const std::string &remoteIp, const std::string &remoteNodeId)
{
    UBSE_LOG_INFO << "new channel callback, remoteIp=" << remoteIp << ", remoteNodeId=" << remoteNodeId;
    std::unique_lock lock(mutex_);
    clusterNodeIdToIpMap_[remoteNodeId] = remoteIp;
    return UBSE_OK;
}

UbseResult UbseNodeDiscoveryCommon::RegisterComHandler()
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "get com module failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    UbseComBaseMessageHandlerPtr nodeDiscoveryDistributeHandler = new (std::nothrow)
        UbseNodeDiscoveryDistributeHandler();
    if (nodeDiscoveryDistributeHandler == nullptr) {
        UBSE_LOG_ERROR << "get node discovery distribute handler failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RegRpcService<UbseNodeDiscoveryListSerial, UbseNodeDiscoveryListSerial>(
        nodeDiscoveryDistributeHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "register distribute handler failed, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseNodeDiscoveryCommon::SubscribeNodeStateEvent()
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
    UBSE_LOG_INFO << "subscribe node state event success";
    return UBSE_OK;
}

UbseResult UbseNodeDiscoveryCommon::HandleNodeStateEvent(const std::string &eventId, const std::string &eventMessage)
{
    auto linkList = NodeDiscoveryParseEvent::ParseNodeStateEventMessage(eventMessage);
    if (linkList.empty()) {
        UBSE_LOG_ERROR << "parse node state event message failed, eventMessage=" << eventMessage;
        return UBSE_ERROR_INVAL;
    }
    uint32_t linkStateDown = 1;
    for (const auto &linkInfo : linkList) {
        if (linkInfo.ubseLinkState != linkStateDown || linkInfo.channelType != "Normal") {
            continue;
        }
        std::string nodeIp{};
        auto ret = GetClusterNodeIpById(linkInfo.nodeId, nodeIp);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "get node ip by id failed, nodeId=" << linkInfo.nodeId << ", " << FormatRetCode(ret);
            continue;
        }
        std::string nonDefaultRoot = GetConnectedNonDefaultRoot();
        if (nodeIp == defaultRoot_ || nodeIp == nonDefaultRoot) {
            ExecNodeDiscoveryTask(nodeIp);
            return UBSE_OK;
        }
    }
    UBSE_LOG_INFO << "node state event handled, no need to reconnect to root";
    return UBSE_OK;
}

void UbseNodeDiscoveryCommon::ExecNodeDiscoveryTask(const std::string &rootIp)
{
    const auto taskName = GenerateDiscoveryTaskName(rootIp);
    if (g_globalStop.load() || taskExecutor_ == nullptr) {
        UbseTimerHandlerUnregister(taskName);
        std::unique_lock lock(reportRetryRecordsMutex_);
        reportRetryRecords_.erase(rootIp);
        return;
    }
    taskExecutor_->Execute([this, rootIp, taskName]() -> void {
        NodeDiscoveryLockGuard lockGuard(rootIp);
        auto ret = ReportAndQuerySuperClusterTopo(rootIp);
        if (ret == UBSE_OK) {
            OnReportSuccess(rootIp, taskName);
            return;
        }
        UBSE_LOG_ERROR << "connect and report to root=" << rootIp << " failed, " << FormatRetCode(ret);
        OnReportFailure(rootIp, taskName);
    });
}

UbseResult UbseNodeDiscoveryCommon::ReportAndQuerySuperClusterTopo(const std::string &rootIp)
{
    auto ret = ConnectRootNode(rootIp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "connect to root=" << rootIp << " failed, " << FormatRetCode(ret);
        return ret;
    }
    if (rootIp != defaultRoot_) {
        SetConnectedNonDefaultRoot(rootIp);
    }
    ret = SendMsgToRoot(rootIp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send msg to root=" << rootIp << " failed, " << FormatRetCode(ret);
        return ret;
    }
    auto eventModule = UbseContext::GetInstance().GetModule<UbseEventModule>();
    if (eventModule == nullptr) {
        UBSE_LOG_ERROR << "get event module failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    std::string eventMessage = "Get super cluster topo";
    return eventModule->UbsePubEvent(UBSE_EVENT_NODE_DISCOVERY, eventMessage);
}

UbseResult UbseNodeDiscoveryCommon::SendMsgToRoot(const std::string &rootIp)
{
    std::string remoteId{};
    auto ret = GetClusterNodeIdByIp(rootIp, remoteId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get root=" << rootIp << " id failed, " << FormatRetCode(ret);
        return ret;
    }
    SendParam sendParam(remoteId, static_cast<uint16_t>(UbseModuleCode::NODE_MGR),
                        static_cast<uint16_t>(UbseNodeMgrOpCode::NODE_INFO_REPORT),
                        UbseChannelType::NORMAL);
    auto node = UbseNodeStaticInfoMgr::GetInstance().GetCurrentNode();
    UbseBaseMessagePtr ubseNodePtr = new (std::nothrow) UbseNodeDiscoverySerial(node);
    if (ubseNodePtr == nullptr) {
        UBSE_LOG_ERROR << "new ubseNode ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseBaseMessagePtr ubseNodeReplyPtr = new (std::nothrow) UbseNodeDiscoveryListSerial();
    if (ubseNodeReplyPtr == nullptr) {
        UBSE_LOG_ERROR << "new ubseNode reply ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "get com module failed.";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    ret = comModule->RpcSend(sendParam, ubseNodePtr, ubseNodeReplyPtr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "report node to root=" << remoteId << " failed, " << FormatRetCode(ret);
        return ret;
    }
    auto nodeReply = UbseBaseMessage::DeConvert<UbseNodeDiscoveryListSerial>(ubseNodeReplyPtr);
    if (nodeReply == nullptr) {
        UBSE_LOG_ERROR << "failed to convert report response";
        return UBSE_ERROR_NULLPTR;
    }
    auto nodes = nodeReply->GetUbseNodeList();
    UbseNodeStaticInfoMgr::GetInstance().SetNodes(nodes);
    UBSE_LOG_INFO << "success to report node=" << ubseNodePtr->ToString() << " to root=" << remoteId;
    return UBSE_OK;
}

void UbseNodeDiscoveryCommon::OnReportSuccess(const std::string &rootIp, const std::string &taskName)
{
    UbseTimerHandlerUnregister(taskName);
    if (rootIp == defaultRoot_) {
        DisconnectNonDefaultRoot();
    }
    std::unique_lock lock(reportRetryRecordsMutex_);
    reportRetryRecords_.erase(rootIp);
}

// 寻找给定root ip的下一位根节点ip; 当前给定root上报3次失败后,寻找下一位root上报
std::string FindNextRootIp(const std::string &ip)
{
    auto rootIps = UbseNodeStaticInfoMgr::GetInstance().GetRootIpList();
    if (rootIps.empty()) {
        UBSE_LOG_ERROR << "root ip list is empty";
        return "";
    }
    auto it = std::find(rootIps.begin(), rootIps.end(), ip);
    if (it == rootIps.end()) {
        UBSE_LOG_WARN << "ip=" << ip << " not found in root list";
        return rootIps[0];
    }
    size_t currentIndex = std::distance(rootIps.begin(), it);
    size_t nextIndex = (currentIndex + 1) % rootIps.size();
    return rootIps[nextIndex];
}

void UbseNodeDiscoveryCommon::OnReportFailure(const std::string &rootIp, const std::string &taskName)
{
    std::unique_lock lock(reportRetryRecordsMutex_);
    auto &record = reportRetryRecords_[rootIp];
    record.retryCount++;
    if (record.retryCount < MAX_RETRY_COUNT) {
        if (!record.timerRegistered) {
            UBSE_LOG_INFO << "create retry task connect and report to root=" << rootIp;
            UbseTimerHandlerRegister(
                taskName,
                [this, rootIp]() -> UbseResult {
                    ExecNodeDiscoveryTask(rootIp);
                    return UBSE_OK;
                },
                REPORT_RETRY_DURATION);
            record.timerRegistered = true;
        }
        return;
    }
    if (rootIp != defaultRoot_) {
        UbseTimerHandlerUnregister(taskName);
        DisconnectNonDefaultRoot();
        reportRetryRecords_.erase(rootIp);
    }
    lock.unlock();
    if (!GetConnectedNonDefaultRoot().empty()) {
        return;
    }
    auto nextRootIp = FindNextRootIp(rootIp);
    UBSE_LOG_INFO << "connect and report to root" << rootIp << " failed, try next ip=" << nextRootIp;
    ExecNodeDiscoveryTask(nextRootIp);
}

void UbseNodeDiscoveryCommon::DisconnectNonDefaultRoot()
{
    std::string nonDefaultRoot = GetConnectedNonDefaultRoot();
    if (nonDefaultRoot.empty()) {
        UBSE_LOG_INFO << "no non-default root connected, skip disconnect";
        return;
    }

    UbseContext &ctx = UbseContext::GetInstance();
    auto module = ctx.GetModule<UbseElectionModule>();
    if (module != nullptr) {
        Node master{};
        auto retCode = module->UbseGetMasterNode(master);
        if (retCode == UBSE_OK && master.ip == nonDefaultRoot) {
            UBSE_LOG_INFO << "connect non-default root=" << nonDefaultRoot << " is master, skip disconnect";
            return;
        }
    }
    auto ret = DisconnectRootNode(nonDefaultRoot);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "disconnect root" << nonDefaultRoot << " failed," << FormatRetCode(ret);
        return;
    }
    std::unique_lock lock(mutex_);
    connectedNonDefaultRoot_.clear();
}

void UbseNodeDiscoveryCommon::SetConnectedNonDefaultRoot(const std::string &ip)
{
    std::unique_lock lock(mutex_);
    connectedNonDefaultRoot_ = ip;
}

std::string UbseNodeDiscoveryCommon::GetConnectedNonDefaultRoot()
{
    std::shared_lock lock(mutex_);
    return connectedNonDefaultRoot_;
}

UbseResult UbseNodeDiscoveryDistributeHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                      UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseNodeDiscoveryListSerial>(req);
    auto response = UbseBaseMessage::DeConvert<UbseNodeDiscoveryListSerial>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = request->Deserialize();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "deserialize cluster topo failed, " << FormatRetCode(ret);
        return ret;
    }
    UbseNodeStaticInfoMgr::GetInstance().SetNodes(request->GetUbseNodeList());
    auto eventModule = UbseContext::GetInstance().GetModule<UbseEventModule>();
    if (eventModule == nullptr) {
        UBSE_LOG_ERROR << "Get eventModule failed";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    std::string eventMessage = "Receive Node Discovery Distribute";
    return eventModule->UbsePubEvent(UBSE_EVENT_NODE_DISCOVERY, eventMessage);
}

uint16_t UbseNodeDiscoveryDistributeHandler::GetModuleCode()
{
    return static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_MGR);
}

uint16_t UbseNodeDiscoveryDistributeHandler::GetOpCode()
{
    return static_cast<uint16_t>(ubse::com::UbseNodeMgrOpCode::NODE_INFO_BROADCAST);
}

std::mutex NodeDiscoveryLockGuard::mutex_;
std::unordered_map<std::string, std::shared_ptr<std::shared_mutex>> NodeDiscoveryLockGuard::locks_{};

std::shared_ptr<std::shared_mutex> NodeDiscoveryLockGuard::GetLock(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = locks_.find(key);
    if (it != locks_.end()) {
        return it->second;
    }
    auto lockPtr = std::make_shared<std::shared_mutex>();
    locks_[key] = lockPtr;
    return lockPtr;
}

NodeDiscoveryLockGuard::NodeDiscoveryLockGuard(const std::string &key) : lock_(GetLock(key))
{
    lock_->lock();
}

NodeDiscoveryLockGuard::~NodeDiscoveryLockGuard()
{
    lock_->unlock();
}

constexpr size_t FIELDS_COUNT = 4;
constexpr size_t PAIR_SIZE = 2;
std::vector<NodeState> NodeDiscoveryParseEvent::ParseNodeStateEventMessage(const std::string &eventMessage)
{
    std::vector<NodeState> linkList;
    std::vector<std::string> linkBlocks;
    Split(eventMessage, ";", linkBlocks);
    for (const auto &block : linkBlocks) {
        if (block.empty()) {
            continue;
        }
        std::vector<std::string> fields;
        Split(block, ",", fields);
        if (fields.size() != FIELDS_COUNT) {
            continue;
        }
        std::vector<std::string> nodeId;
        Split(fields[0], ":", nodeId);
        std::vector<std::string> state;
        Split(fields[1], ":", state);
        std::vector<std::string> timeStamp;
        Split(fields[2], ":", timeStamp);
        std::vector<std::string> channelType;
        Split(fields[3], ":", channelType);
        if (nodeId.size() != PAIR_SIZE || state.size() != PAIR_SIZE || timeStamp.size() != PAIR_SIZE ||
            channelType.size() != PAIR_SIZE) {
            continue;
            }
        uint32_t ubseLinkState = 0;
        if (ConvertStrToUint32(state[1], ubseLinkState) != UBSE_OK) {
            continue;
        }
        linkList.emplace_back(NodeState{nodeId[1], ubseLinkState, channelType[1]});
    }
    return linkList;
}
} // namespace ubse::nodeMgr