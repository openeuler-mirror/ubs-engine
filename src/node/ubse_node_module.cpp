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

#include "ubse_node_module.h"

#include <referable/ubse_ref.h> // for Ref
#include <securec.h>            // for memcpy_s, EOK, errno_t
#include <unistd.h>             // for sleep
#include <algorithm>            // for max
#include <fstream>
#include <memory> // for operator==, shared_ptr
#include <random>
#include <regex> // for regex_match, regex

#include "src/controllers/node/ubse_node_api.h"
#include "ubse_com_base.h"   // for UbseLinkInfo, UbseModuleCode
#include "ubse_com_def.h"    // for UbseLinkState
#include "ubse_com_module.h" // for UbseComModule
#include "ubse_election.h"
#include "ubse_error.h"        // for UBSE_OK, UBSE_ERROR_NULLPTR
#include "ubse_event_module.h" // for UbseEventModule
#include "ubse_file_util.h"    // for UbseFileUtil
#include "ubse_logger_inner.h" // for RM_LOG_ERROR, RM_LOG_INFO
#include "ubse_topology_interface.h"

namespace ubse::node {
using namespace ubse::com;
using namespace ubse::election;
using namespace ubse::event;
using namespace ubse::node::api;

DYNAMIC_CREATE(UbseNodeModule, UbseElectionModule);
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_NODE_MID)
constexpr uint8_t NODE_UP_STATE = 1;

std::shared_mutex UbseNodeModule::nodeMutex;
std::vector<UbseRoleInfo> UbseNodeModule::linkUpNodes{};
std::unordered_set<std::string> taskSet{};
std::shared_mutex taskMutex{};

bool IsSpecialIP(const std::string &ip)
{
    // 排除特殊 IP 地址：0.0.0.0, 127.x.x.x, 169.254.x.x
    return std::regex_match(ip, std::regex("^(0\\.0\\.0\\.0|127\\..*|169\\.254\\..*)$"));
}

uint32_t UbseNodeTelemetryGetIpInfo(std::vector<std::string> &ipInfos)
{
    std::ifstream file("/proc/net/fib_trie");
    if (!file.is_open()) {
        UBSE_LOG_ERROR << "Error opening file /proc/net/fib_trie";
        return UBSE_ERROR;
    }

    std::string line;
    std::unordered_set<std::string> uniqueIPs;
    std::regex ipRegex(R"(\b(\d+\.\d+\.\d+\.\d+)\b)"); // 匹配 IP 地址的正则表达式

    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, ipRegex)) {
            std::string ip = match.str(1); // 正则匹配第2项
            if (!IsSpecialIP(ip)) {        // 排除特殊 IP 地址
                uniqueIPs.insert(ip);
            }
        }
    }
    file.close();
    ipInfos.insert(ipInfos.end(), uniqueIPs.begin(), uniqueIPs.end());
    return UBSE_OK;
}

UbseResult UbseNodeModule::Initialize()
{
    UbseContext &ubseContext = UbseContext::GetInstance();
    if (ubseContext.GetProcessMode() == ProcessMode::CLI) {
        UBSE_LOG_INFO << "CLI mode; Skip initializing node module";
        return UBSE_OK;
    }
    ubse::mti::MtiNodeInfo ubseNodeInfo;
    auto ret = UbseGetLocalNodeInfo(ubseNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get nodeId failed," << FormatRetCode(ret);
        return ret;
    }
    nodeId_ = ubseNodeInfo.nodeId;

    UbseElectionHandlerBuilder Builder;
    Builder.SetHandler([this](UbseElectionEventType &type, UBSE_ID_TYPE) { return EstablishDataChannel(type); });
    Builder.SetPriority(UbseElectionHandlerPriority::HIGH);
    Builder.SetType(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION);
    Builder.SetName("EstablishChannel");
    UbseElectionChangeAttachHandler(Builder.Build());

    UbseElectionHandlerBuilder NodeUpBuilder;
    NodeUpBuilder.SetHandler([this](UbseElectionEventType &type, UBSE_ID_TYPE) { return EstablishDataChannel(type); });
    NodeUpBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeUpBuilder.SetType(UbseElectionEventType::NODE_UP);
    NodeUpBuilder.SetName("EstablishChannelNodeUp");
    UbseElectionChangeAttachHandler(NodeUpBuilder.Build());

    UbseElectionHandlerBuilder NodeDownBuilder;
    NodeDownBuilder.SetHandler(
        [](UbseElectionEventType, UBSE_ID_TYPE nodeId) { return UbseNodeModule::NodeDownHandler(nodeId); });
    NodeDownBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeDownBuilder.SetType(UbseElectionEventType::NODE_DOWN);
    NodeDownBuilder.SetName("UpdateNodeDown");
    UbseElectionChangeAttachHandler(NodeDownBuilder.Build());

    std::string UBSE_EVENT_XALARM_PANIC = "ALARM_PANIC_EVENT";
    UbseSubEvent(UBSE_EVENT_XALARM_PANIC, UbseNodeModule::NodePanicHandler, UbseEventPriority::HIGH);
    std::string UBSE_BROKEN_NOTIFY = "ubse.node.reconnect";
    UbseSubEvent(UBSE_BROKEN_NOTIFY, UbseNodeModule::BrokenHandler, UbseEventPriority::HIGH);
    return CheckNodeId();
}

UbseResult UbseNodeModule::Start()
{
    return UBSE_OK;
}

void UbseNodeModule::UnInitialize()
{
    std::string UBSE_EVENT_XALARM_PANIC = "ALARM_PANIC_EVENT";
    UbseUnSubEvent(UBSE_EVENT_XALARM_PANIC, UbseNodeModule::NodePanicHandler);
    std::string UBSE_BROKEN_NOTIFY = "ubse.node.reconnect";
    UbseUnSubEvent(UBSE_BROKEN_NOTIFY, UbseNodeModule::BrokenHandler);
}

void UbseNodeModule::Stop()
{
    UbseElectionHandlerBuilder Builder;
    Builder.SetHandler([this](UbseElectionEventType &type, UBSE_ID_TYPE) { return EstablishDataChannel(type); });
    Builder.SetPriority(UbseElectionHandlerPriority::HIGH);
    Builder.SetType(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION);
    Builder.SetName("EstablishChannel");
    UbseElectionChangeDeAttachHandler(Builder.Build());

    UbseElectionHandlerBuilder NodeUpBuilder;
    NodeUpBuilder.SetHandler([this](UbseElectionEventType &type, UBSE_ID_TYPE) { return EstablishDataChannel(type); });
    NodeUpBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeUpBuilder.SetType(UbseElectionEventType::NODE_UP);
    NodeUpBuilder.SetName("EstablishChannelNodeUp");
    UbseElectionChangeDeAttachHandler(Builder.Build());

    UbseElectionHandlerBuilder NodeDownBuilder;
    NodeDownBuilder.SetHandler(
        [](UbseElectionEventType, UBSE_ID_TYPE nodeId) { return UbseNodeModule::NodeDownHandler(nodeId); });
    NodeDownBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    NodeDownBuilder.SetType(UbseElectionEventType::NODE_DOWN);
    NodeDownBuilder.SetName("UpdateNodeDown");
    UbseElectionChangeDeAttachHandler(NodeDownBuilder.Build());

    for (const auto &name : taskSet) {
        UBSE_LOG_INFO << "wait exit task=" << name;
        auto taskExecutor = UbseContext::GetInstance().GetModule();
        if (taskExecutor == nullptr) {
            UBSE_LOG_WARN << "task module already exit.";
            break;
        }
        taskExecutor->Remove(name);
    }
    UBSE_LOG_INFO << "all task stop.";
}

void ParseIp(const std::string &msg, std::string &nodeId, std::string &nodeIp, uint16_t &port, std::string &role)
{
    std::stringstream ss(msg);
    std::getline(ss, nodeId, '-'); // 获取nodeId
    std::getline(ss, nodeIp, '-'); // 获取nodeIp
    std::string portStr;
    std::getline(ss, portStr, '-'); // 获取port

    port = static_cast<uint16_t>(std::stoi(portStr));
    std::getline(ss, role, '-'); // 获取角色
}

UbseResult UbseNodeModule::BrokenHandler(std::string &, std::string &eventMessage)
{
    if (g_globalStop.load()) {
        UBSE_LOG_WARN << "process already stop when handle broken channel.";
        return UBSE_OK;
    }
    std::string executorName = GenerateTaskName();
    auto taskExecutor = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    UbseResult ret = taskExecutor->Create(executorName, NO_1, NO_2);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "create node executor failed";
        return ret;
    }
    auto ptr = taskExecutor->Get(executorName);
    if (ptr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    taskMutex.lock();
    taskSet.insert(executorName);
    taskMutex.unlock();
    ptr->Execute([eventMessage]() -> void {
        std::string nodeId;
        std::string nodeIp;
        uint16_t port;
        std::string role;
        ParseIp(eventMessage, nodeId, nodeIp, port, role);
        auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
        if (comModule == nullptr) {
            UBSE_LOG_ERROR << "com module not load when broken handler.";
            return;
        }
        UBSE_LOG_INFO << "broken channel notify,start to connect to nodeId=" << nodeId << ", nodeIp=" << nodeIp
                      << ", port=" << port;
        UbseResult connectRet =
            comModule->ConnectWithOption(ConnectOption{nodeId, nodeIp, port, UbseChannelType::NORMAL});
        if (connectRet != UBSE_OK) {
            UBSE_LOG_ERROR << "connect to nodeId=" << nodeId << " failed, " << FormatRetCode(connectRet);
            return;
        }
        PushLinkUpNode(nodeId, role);
    });
    ptr->Wait();
    taskExecutor->Remove(executorName);
    taskMutex.lock();
    taskSet.erase(executorName);
    taskMutex.unlock();
    return UBSE_OK;
}

uint32_t UbseGetNodeHostName(std::vector<std::string> &hostName)
{
    auto lineInfo = std::vector<std::string>();
    auto ret = ubse::utils::UbseFileUtil::GetFileInfo("/proc/sys/kernel/hostname", lineInfo);
    if (UBSE_UNLIKELY(ret != UBSE_OK) || lineInfo.size() != 1 || lineInfo[0].empty()) {
        UBSE_LOG_WARN << "get hostname failed, hostname size is " << lineInfo.size();
        hostName.emplace_back("");
    } else {
        hostName.emplace_back(lineInfo[0]);
    }
    return UBSE_OK;
}

UbseResult UbseNodeModule::CheckNodeId()
{
    // 校验长度在1到64之间
    if (nodeId_.length() < NODE_NAME_MIN_LENGTH || nodeId_.length() > NODE_NAME_MAX_LENGTH) {
        UBSE_LOG_ERROR << "nodeId:" << nodeId_ << " length is invalid, should be between 1 and 64";
        return UBSE_ERROR_INVAL;
    }
    // 校验是否符合合法字符的正则表达式
    std::regex legalChars(R"(^[a-zA-Z0-9\_\-]+$)");
    if (!std::regex_match(nodeId_, legalChars)) {
        UBSE_LOG_ERROR << "nodeId:" << nodeId_ << " is invalid";
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

UbseResult UbseNodeModule::EstablishDataChannel(UbseElectionEventType &type)
{
    UBSE_LOG_INFO << "Leader changed, start to establish data channel, current type: " << static_cast<uint32_t>(type);
    auto elcModule = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (elcModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    Node masterNode{};
    Node standbyNode{};
    std::vector<Node> agentNode{};
    UbseResult ret = elcModule->UbseGetAllNodes(masterNode, standbyNode, agentNode);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get all node failed, ret: " << ret;
        return ret;
    }
    agentNode.push_back(standbyNode);
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    if (elcModule->IsLeader()) {
        if (type == UbseElectionEventType::MASTER_ONLINE_NOTIFICATION) {
            ResetLinkUpNode(masterNode.id);
        }
        ret = Connect(agentNode, standbyNode.id);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "leader's establishment of data channel has some error, ret: " << ret;
        } else {
            UBSE_LOG_INFO << "Master connection successful, start to initialize PSK on master.";
        }
        return ret;
    } else {
        for (auto &node : agentNode) {
            if (node.id == nodeId_) {
                continue;
            }
            (void)comModule->RemoveChannel(node.id, UbseChannelType::NORMAL);
            (void)comModule->RemoveChannel(node.id, UbseChannelType::EMERGENCY);
        }
        return UBSE_OK;
    }
}

// 主节点上线，刷新link node节点信息，同时将主节点写入
void UbseNodeModule::ResetLinkUpNode(std::string masterId)
{
    std::unique_lock<std::shared_mutex> lock(nodeMutex);
    linkUpNodes.clear();
    linkUpNodes.push_back({masterId, ELECTION_ROLE_MASTER, 1});
}

void UbseNodeModule::PushLinkUpNode(std::string nodeId, std::string role)
{
    std::unique_lock<std::shared_mutex> lock(nodeMutex);
    UBSE_LOG_INFO << "established node: " << nodeId;

    for (auto node : linkUpNodes) {
        if (node.nodeId == nodeId) {
            return;
        }
    }
    linkUpNodes.push_back({nodeId, role, 1});
}

UbseResult UbseNodeModule::NodePanicHandler(std::string &, std::string &eventMessage)
{
    UBSE_LOG_INFO << "Node: " << eventMessage << " panic, update link node";
    std::unique_lock<std::shared_mutex> lock(nodeMutex);
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    for (auto it = linkUpNodes.begin(); it != linkUpNodes.end();) {
        if (it->nodeId == eventMessage) {
            it = linkUpNodes.erase(it); // 删除元素并更新迭代器
        } else {
            ++it; // 继续下一个元素
        }
    }
    (void)comModule->RemoveChannel(eventMessage, UbseChannelType::NORMAL);
    (void)comModule->RemoveChannel(eventMessage, UbseChannelType::EMERGENCY);

    return UBSE_OK;
}

/**
 * @brief 监听节点下线事件；节点模块不做断连操作，通信模块会默认重连 & 节点重新上线后
 * 节点模块再次建链，由通信模块确保通道的唯一性
 * @return
 */
UbseResult UbseNodeModule::NodeDownHandler(std::string nodeId)
{
    std::unique_lock<std::shared_mutex> lock(nodeMutex);
    UBSE_LOG_INFO << "Node: " << nodeId << " down, update link node";
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    for (auto it = linkUpNodes.begin(); it != linkUpNodes.end();) {
        if (it->nodeId == nodeId) {
            it = linkUpNodes.erase(it); // 删除元素并更新迭代器
        } else {
            ++it; // 继续下一个元素
        }
    }
    (void)comModule->RemoveChannel(nodeId, UbseChannelType::NORMAL);
    (void)comModule->RemoveChannel(nodeId, UbseChannelType::EMERGENCY);

    return UBSE_OK;
}

std::string GenerateTaskName()
{
    std::random_device rd;                      // 获取随机数种子
    std::mt19937 gen(rd());                     // 使用梅森旋转算法生成随机数
    std::uniform_int_distribution<> dist(0, 9); // 生成 0 到 9 的随机数字
    std::string randomID;

    for (size_t i = 0; i < 10; ++i) {          // 生成的长度为10的随机字符串
        randomID += std::to_string(dist(gen)); // 随机选择数字并转换为字符串
    }
    return "NodeExecutor" + randomID;
}

UbseResult UbseNodeModule::Connect(std::vector<ubse::election::Node> agentNodes, std::string standbyId)
{
    if (agentNodes.empty()) {
        UBSE_LOG_INFO << "no agent node, will skip establish channel";
        return UBSE_OK;
    }
    std::string executorName = GenerateTaskName();
    auto taskExecutor = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    // 节点回调可能是并发的，每次连接需要获取新的任务实例，确保不同的回调之间互不影响
    // 若后续有更多节点需要继续增加队列容量，公式： 容量>=单次极限最多建链节点数*2。
    UbseResult ret = taskExecutor->Create(executorName, NO_10, 20); // 16P环境基线场景需要跟7个节点，建立14条链路。
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "create node executor failed";
        return ret;
    }
    auto ptr = taskExecutor->Get(executorName);
    if (ptr == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    taskMutex.lock();
    taskSet.insert(executorName);
    taskMutex.unlock();
    ret = ConnectChannel(ptr, agentNodes, executorName, standbyId);
    taskExecutor->Remove(executorName);
    taskMutex.lock();
    taskSet.erase(executorName);
    taskMutex.unlock();
    UBSE_LOG_INFO << "task: " << executorName << " executor end.";
    return ret;
}

UbseResult UbseNodeModule::ConnectChannel(UbseTaskExecutorPtr ptr, const std::vector<Node> &agentNodes,
                                          const std::string executorName, const std::string &standbyId)
{
    UbseResult ret = UBSE_OK;
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        return UBSE_ERROR_CONF_INVALID;
    }
    for (const auto &node : agentNodes) {
        if (node.ip.empty() || node.id.empty()) {
            continue;
        }
        UBSE_LOG_INFO << "start connect to node: " << node.id << " task:" << executorName;
        ptr->Execute([&ret, node, comModule, executorName, standbyId]() -> void {
            UBSE_LOG_INFO << "start connecting to node: " << node.id
                          << ", normal channel in thread , task:" << executorName;
            UbseResult connectRet =
                comModule->ConnectWithOption(ConnectOption{node.id, node.ip, node.port, UbseChannelType::NORMAL});
            if (connectRet != UBSE_OK) {
                ret = connectRet;
                UBSE_LOG_ERROR << "connect to node: " << node.id << " node failed, ret: " << FormatRetCode(connectRet)
                               << ", normal channel, task: " << executorName;
                return;
            }
            UBSE_LOG_INFO << "connect to node: " << node.id << " node success normal channel, task: " << executorName;
            PushLinkUpNode(node.id, (node.id == standbyId) ? ELECTION_ROLE_STANDBY : ELECTION_ROLE_AGENT);
        });
    }
    ptr->Wait();
    return ret;
}

std::vector<UbseRoleInfo> UbseNodeModule::GetLinkUpNodes()
{
    std::shared_lock<std::shared_mutex> lock(nodeMutex);
    return linkUpNodes;
}

} // namespace ubse::node