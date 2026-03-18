/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_mem_controller_pre_online.h"

#include "src/controllers/mem/mem_decoder_utils/ubse_mem_decoder_utils.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_mem_controller_msg.h"
#include "ubse_mmi_interface.h"
#include "ubse_mem_util.h"
#include "ubse_mmi_module.h"
#include "ubse_node.h"
#include "ubse_node_controller.h"
#include "ubse_serial_util.h"
#include "ubse_str_util.h"
#include "ubse_thread_pool_module.h"
#include "ubse_mmi_interface.h"
#include "ubse_timer.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::context;
using namespace ubse::election;
using namespace ubse::nodeController;
using namespace ubse::nodeController;
using namespace ubse::task_executor;
using namespace ubse::utils;
using namespace ubse::mmi;
using namespace ubse::config;
using namespace ubse::mem::util;
using namespace ubse::serial;
using namespace ubse::adapter_plugins::mmi;

const uint64_t MIN_PRE_ONLINE_SIZE = 128;         // 最小预上线内存，单位MB
const uint64_t DEFAULT_PRE_ONLINE_SIZE = 4096;    // 默认预上线内存，单位MB
const uint64_t MAX_PRE_ONLINE_SIZE = 262144;      // 最大预上线内存，单位MB，对应256G
const uint32_t PRE_ONLINE_MSG_RETRY_DURATION = 2; // 发送 & 回复请求失败重试间隔，重试间隔2s
const uint32_t TIMEOUT_CHECK_DURATION = 60000;    // 预上线超时检查器，检查间隔1分钟
// 主节点等待节点预上线超时时间20分钟，当前每G最大上线耗时3s， 最大预上线内存 预计需要12.8分钟.
// 若主节点等到20分钟还未执行完毕，将对应节点状态置为 offline.
const uint64_t PRE_ONLINE_TIMEOUT = 12000;
const uint32_t SEND_PRE_ONLINE_MSG_TIMES = 5; // master发送消息失败后重试次数
const uint32_t MB_SIZE = 1024 * 1024;
constexpr int MAX_CNA_LIST_SIZE = 256;

const std::string PRE_ONLINE_TASK_NAME = "pre_online_task";

// 节点预上线状态map-中心侧
static std::map<std::string, PreOnLineState> nodePreOnLine{};
// 操作节点预上线状态map-中心侧
static std::shared_mutex preOnLineMutexMap{};
// 执行预上线操作-节点侧
static std::shared_mutex preOnLineOpreateMutex{};

// 节点预上线任务map-中心侧；
static std::map<std::string, PreOnLineTask> preOnLineTask{};
static std::shared_mutex preOnLineTaskMutexMap{};

bool IsClusterPreOnLineReady()
{
    preOnLineMutexMap.lock_shared();
    int onlineCount = 0;
    for (const auto &pair : nodePreOnLine) {
        if (pair.second == PreOnLineState::ONLINE &&
            ++onlineCount >= 2) { // 当集群>=2节点处于Online状态，即可进行numa借用；但不保证一定能成功。
            preOnLineMutexMap.unlock_shared();
            return true;
        }
    }
    preOnLineMutexMap.unlock_shared();
    return false;
}

void SetNodePreOnLine(const std::string &nodeId, PreOnLineState state)
{
    preOnLineMutexMap.lock();
    nodePreOnLine[nodeId] = state;
    preOnLineMutexMap.unlock();
}

bool IsNodeOnLine(const std::string &nodeId)
{
    preOnLineMutexMap.lock_shared();
    if (nodePreOnLine.find(nodeId) != nodePreOnLine.end() && nodePreOnLine[nodeId] == PreOnLineState::ONLINE) {
        UBSE_LOG_INFO << "nodeId=" << nodeId << " already onLine";
        preOnLineMutexMap.unlock_shared();
        return true;
    }
    preOnLineMutexMap.unlock_shared();
    return false;
}

void PreOnlineThread(const ubse::nodeController::UbseNodeInfo &ubseNode, bool isInitCluster)
{
    try {
        std::thread([ubseNode, isInitCluster]() -> void {
            auto ret = PreOnLineExec(ubseNode, isInitCluster);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "node=" << ubseNode.nodeId << " send msg failed, set offline";
                SetNodePreOnLine(ubseNode.nodeId, PreOnLineState::OFFLINE);
            }
        }).detach();
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "nodeId=" << ubseNode.nodeId << " create thread failed, " << e.what();
    }
}

UbseResult PreOnlineHandler(const ubse::nodeController::UbseNodeInfo &ubseNode)
{
    UBSE_LOG_INFO << "nodeId=" << ubseNode.nodeId << " online, state=" << static_cast<uint32_t>(ubseNode.clusterState);
    if (ubseNode.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT ||
        ubseNode.clusterState == UbseNodeClusterState::UBSE_NODE_UNKNOWN) {
        UBSE_LOG_WARN << "nodeId=" << ubseNode.nodeId << "not working, set to offline";
        SetNodePreOnLine(ubseNode.nodeId, PreOnLineState::OFFLINE);
        return UBSE_OK;
    }
    if (ubseNode.clusterState != UbseNodeClusterState::UBSE_NODE_SMOOTHING) {
        UBSE_LOG_WARN << "nodeId=" << ubseNode.nodeId << "not smoothing";
        return UBSE_OK;
    }
    if (!ValidPreOnLine(ubseNode.nodeId)) {
        return UBSE_OK;
    }
    auto nodes = UbseNodeController::GetInstance().GetAllNodes();
    if (nodes.size() <= 1) { // 集群节点<=1，不做预上线
        UBSE_LOG_INFO << "current cluster only has one node=" << ubseNode.nodeId << " when smoothing, skip pre online.";
    } else if (nodes.size() == 2) { // 集群节点=2，将2个节点进行预上线，若没有直连关系，也置为ONLINE
        UBSE_LOG_INFO << "current cluster only has two node when smoothing, pre online.";
        for (auto clusterNode : nodes) {
            if (IsNodeOnLine(clusterNode.second.nodeId)) {
                UBSE_LOG_INFO << "node=" << clusterNode.second.nodeId
                              << " already online when smooth, skip pre online.";
                continue;
            }
            PreOnlineThread(clusterNode.second, true);
        }
    } else { // 集群>2，将加入集群的节点进行上线。
        UBSE_LOG_INFO << "current cluster has more than two node, pre online.";
        PreOnlineThread(ubseNode, false);
    }
    return UBSE_OK;
}

bool ValidPreOnLine(const std::string &nodeId)
{
    if (g_globalStop.load()) {
        UBSE_LOG_WARN << "process already stop.";
        return false;
    }
    if (!IsPreOnLineEnable()) {
        UBSE_LOG_WARN << "pre online disable.";
        return false;
    }
    if (IsNodeOnLine(nodeId)) {
        return false;
    }
    return true;
}

/**
* 解析LCNE链路变更的msg，格式：DOWN;1-1:2-1|UP;1-1:3-1|DOWN;2-1:4-1|UP;3-1:5-1|
* @param msg
* @return
*/
std::vector<std::string> ParseLcneTopologyNotifyMsg(const std::string &msg)
{
    std::istringstream stream(msg);
    std::string segment;
    std::vector<std::string> nodeList{};
    while (std::getline(stream, segment, '|')) {
        if (segment.find("DOWN") != std::string::npos) {
            size_t pos = segment.find(';');
            if (pos == std::string::npos) {
                continue;
            }
            std::string downPart = segment.substr(pos + 1);
            size_t colonPos = downPart.find(':');
            if (colonPos != std::string::npos) {
                std::string number = downPart.substr(0, colonPos);
                nodeList.push_back(number.substr(0, number.find('-')));
            }
        } else if (segment.find("UP") != std::string::npos) {
            size_t pos = segment.find(';');
            if (pos == std::string::npos) {
                continue;
            }
            std::string upPart = segment.substr(pos + 1);
            size_t colonPos = upPart.find(':');
            if (colonPos != std::string::npos) {
                std::string number = upPart.substr(0, colonPos);
                nodeList.push_back(number.substr(0, number.find('-')));
            }
        }
    }
    return nodeList;
}

bool ValidWhenLcneTopologyChange(const std::string &eventMessage)
{
    if (g_globalStop.load()) {
        UBSE_LOG_WARN << "process already stop.";
        return false;
    }
    if (eventMessage.empty()) {
        UBSE_LOG_INFO << "lcne, msg empty skip pre online.";
        return false;
    }
    if (!IsPreOnLineEnable()) {
        UBSE_LOG_WARN << "pre online disable.";
        return false;
    }
    return true;
}

UbseResult LcneTopologyChangeHandler(std::string &, std::string &eventMessage)
{
    UBSE_LOG_INFO << "lcne, msg=" << eventMessage;
    if (!ValidWhenLcneTopologyChange(eventMessage)) {
        return UBSE_OK;
    }
    std::vector<std::string> nodeIds = ParseLcneTopologyNotifyMsg(eventMessage);
    auto nodes = UbseNodeController::GetInstance().GetAllNodes();
    if (nodes.empty()) {
        UBSE_LOG_INFO << "lcne, current cluster is empty, skip pre online.";
    } else if (nodes.size() == 1) { // 集群节点<=1，不做预上线
        UBSE_LOG_INFO << "lcne, current cluster only has one node, skip pre online.";
    } else if (nodes.size() == 2) { // 集群节点=2，将2个节点进行预上线，若没有直连关系，也置为ONLINE
        UBSE_LOG_INFO << "lcne, current cluster only has two node, pre online.";
        for (const auto& clusterNode : nodes) {
            if (IsNodeOnLine(clusterNode.second.nodeId)) {
                continue;
            }
            PreOnlineThread(clusterNode.second, true);
        }
    } else { // 集群>2，将加入集群的节点进行上线。
        for (const auto& id : nodeIds) {
            UBSE_LOG_INFO << "lcne, current cluster only has more than three node, pre online.";
            auto iter = nodes.find(id);
            if (iter == nodes.end()) {
                UBSE_LOG_WARN << "lcne, nodeId=" << id << " not collect.";
                continue;
            }
            UBSE_LOG_INFO << "lcne, node=" << id << " state="
                          << static_cast<uint32_t>(iter->second.clusterState);
            if (iter->second.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT ||
                iter->second.clusterState == UbseNodeClusterState::UBSE_NODE_UNKNOWN) {
                continue;
            }
            PreOnlineThread(iter->second, false);
        }
    }
    return UBSE_OK;
}

uint16_t GetMarId(const std::string &portId, uint16_t &marId)
{
    if (portId.empty()) {
        UBSE_LOG_WARN << "The portId=" << portId;
        return UBSE_ERROR;
    }
    uint64_t value{};
    const bool ret = ubse::utils::StrToULong(portId, value);
    if (!ret) {
        UBSE_LOG_ERROR << "portGroupIdStr(" << portId << ") is invalid.";
        return UBSE_ERROR;
    }
    if (value > UINT16_MAX) {
        UBSE_LOG_ERROR << "portGroupIdStr(" << portId << ") is invalid.";
        return UBSE_ERROR;
    }
    marId = decoder::utils::MemDecoderUtils::portToPortSet[value];
    return UBSE_OK;
}

UbseResult GenerateSocketCna(const std::string& nodeId, UbseCpuInfo cpu, ubse::nodeController::UbseNodeInfo remoteNode,
                             ubse::nodeController::UbsePortInfo port, SocketCnaInfo& cnaInfo)
{
    uint32_t remoteSocketId = 0;
    if (!ubse::utils::StrToUint(port.remoteChipId, remoteSocketId)) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << ", remote lcne nodeId=" << remoteNode.nodeId
                      << " chipId invalid, chipId=" << port.remoteChipId;
        return UBSE_ERROR;
    }
    UbseCpuLocation location{remoteNode.nodeId, remoteSocketId};
    cnaInfo.importNodeId = nodeId;
    cnaInfo.importSocketId = cpu.socketId;
    cnaInfo.exportNodeId = remoteNode.nodeId;
    cnaInfo.exportSocketId = remoteSocketId;
    cnaInfo.scna = cpu.busNodeCna;
    cnaInfo.dcna = remoteNode.cpuInfos[location].busNodeCna;
    auto ret = ConvertStrToUint32(cpu.eid, cnaInfo.seid, NO_16);
    ret |= ConvertStrToUint32(remoteNode.cpuInfos[location].eid, cnaInfo.deid, NO_16);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << ", eid=" << cpu.eid << ", remote nodeId=" << remoteNode.nodeId
                      << ", chipId=" << remoteSocketId << ", eid=" << remoteNode.cpuInfos[location].eid;
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "nodeId=" << nodeId << ", eid=" << cpu.eid << ", remote nodeId=" << remoteNode.nodeId
                  << ", chipId=" << remoteSocketId << ", eid=" << remoteNode.cpuInfos[location].eid;
    uint16_t marId = 0;
    auto marRet = GetMarId(port.portId, marId);
    if (marRet != UBSE_OK) {
        UBSE_LOG_WARN << "nodeId=" << nodeId << " marId not valid, portId=" << port.portId;
        return UBSE_ERROR;
    }
    cnaInfo.marId = marId;
    return UBSE_OK;
}

std::vector<SocketCnaInfo> GeneratePreOnLineCna(const std::string &nodeId)
{
    UBSE_LOG_INFO << "nodeId=" << nodeId << " start to generate pre online cna";
    ubse::nodeController::UbseNodeInfo info = UbseNodeController::GetInstance().GetNodeById(nodeId);
    auto allNodes = UbseNodeController::GetInstance().GetAllNodes();
    std::vector<SocketCnaInfo> cnas{};
    for (auto cpu : info.cpuInfos) {
        for (auto port : cpu.second.portInfos) {
            auto remoteNode = UbseNodeController::GetInstance().GetNodeById(port.second.remoteSlotId);
            if (remoteNode.nodeId.empty()) {
                UBSE_LOG_WARN << "nodeId=" << nodeId << ", remote lcne nodeId=" << port.second.remoteSlotId
                              << " not collect.";
                continue;
            }
            UBSE_LOG_INFO << "nodeId=" << nodeId << ", remote lcne nodeId=" << remoteNode.nodeId
                          << ", state=" << static_cast<uint32_t>(remoteNode.clusterState);
            if (remoteNode.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT ||
                remoteNode.clusterState == UbseNodeClusterState::UBSE_NODE_UNKNOWN) {
                UBSE_LOG_WARN << "nodeId=" << nodeId << ", remote lcne nodeId=" << remoteNode.nodeId << " not working.";
                continue;
            }
            SocketCnaInfo cnaInfo{};
            auto ret = GenerateSocketCna(nodeId, cpu.second, remoteNode, port.second, cnaInfo);
            if (ret != UBSE_OK) {
                UBSE_LOG_WARN << "nodeId=" << nodeId << ", cpu=" << cpu.second.socketId
                              << ", remoteNodeId=" << remoteNode.nodeId << ", remote cpu=" << port.second.remoteChipId
                              << ", generate cna info failed";
                continue;
            }
            cnas.push_back(cnaInfo);
        }
    }
    return cnas;
}

std::unordered_set<std::string> FilterLcneRemote(ubse::nodeController::UbseNodeInfo currentNode)
{
    std::unordered_set<std::string> remoteNodeIdSet{};
    UBSE_LOG_INFO << "print nodeId=" << currentNode.nodeId << " lcne remote nodeId";
    for (auto cpu : currentNode.cpuInfos) {
        for (auto port : cpu.second.portInfos) {
            if (port.second.portStatus == PortStatus::DOWN) {
                UBSE_LOG_WARN << "nodeId=" << currentNode.nodeId << " portId=" << port.first << " Down";
                continue;
            }
            UBSE_LOG_INFO << "nodeId=" << currentNode.nodeId << ", portId=" << port.first
                          << " lcne remote nodeId=" << port.second.remoteSlotId;
            auto node = UbseNodeController::GetInstance().GetNodeById(port.second.remoteSlotId);
            if (node.nodeId.empty()) {
                UBSE_LOG_WARN << "nodeId=" << currentNode.nodeId << ", portId=" << port.first
                              << " lcne remote nodeId=" << port.second.remoteSlotId << " not collected";
                continue;
            }
            if (node.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT ||
                node.clusterState == UbseNodeClusterState::UBSE_NODE_UNKNOWN) {
                UBSE_LOG_WARN << "nodeId=" << currentNode.nodeId << ", portId=" << port.first
                              << " lcne remote nodeId=" << port.second.remoteSlotId
                              << " not working, state=" << static_cast<uint32_t>(node.clusterState);
                continue;
            }
            UBSE_LOG_INFO << "nodeId=" << currentNode.nodeId << ", portId=" << port.first
                          << " add lcne remote nodeId=" << port.second.remoteSlotId;
            remoteNodeIdSet.insert(port.second.remoteSlotId);
        }
    }
    return remoteNodeIdSet;
}

UbseTaskExecutorPtr GenerateTask(const std::string &taskName)
{
    auto taskExecutor = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        UBSE_LOG_ERROR << "task executor not load";
        return nullptr;
    }
    return taskExecutor->Get("ubseMemController");
}

void RemoveTask(const std::string &taskName)
{
    auto taskExecutor = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        UBSE_LOG_WARN << "task executor not load";
        return;
    }
    taskExecutor->Remove(taskName);
}

UbseResult ReqPreOnLine(const std::string &nodeId, const std::string &taskName, UbseTaskExecutorPtr ptr,
                        std::unordered_set<std::string> remoteNodeIdSet)
{
    UbseResult ret = UBSE_OK;
    for (auto preOnLineNode : remoteNodeIdSet) {
        std::vector<SocketCnaInfo> cnas = GeneratePreOnLineCna(preOnLineNode);
        UBSE_LOG_INFO << "print nodeId=" << nodeId << " remote nodeId" << preOnLineNode
                      << " filter pre online cpu size=" << cnas.size() << ", task name=" << taskName;
        for (auto obj : cnas) {
            UBSE_LOG_INFO << "importNodeId=" << obj.importNodeId << ", importSocketId=" << obj.importSocketId
                          << ", exportNodeId=" << obj.exportNodeId << ", exportSocketId=" << obj.exportSocketId
                          << ", scna=" << obj.scna << ", dcna=" << obj.dcna << ", marId=" << obj.marId;
        }
        ptr->Execute([&ret, cnas, nodeId, taskName, preOnLineNode]() -> void {
            UBSE_LOG_INFO << "task=" << taskName << " nodeId=" << nodeId << ", pre online nodeId=" << preOnLineNode
                          << " start to req";
            PreOnLineReq req{};
            req.cnas = cnas;
            req.taskName = taskName;
            auto sendRet = UBSE_OK;
            for (size_t i = 0; i < SEND_PRE_ONLINE_MSG_TIMES; i++) {
                sendRet = PreOnLineRequest(preOnLineNode, req);
                if (sendRet == UBSE_OK) {
                    break;
                }
                UBSE_LOG_ERROR << "[pre_online] task=" << taskName << " nodeId=" << nodeId
                               << ", pre online nodeId=" << preOnLineNode << " send pre online msg failed, "
                               << FormatRetCode(ret) << ", will retry.";
                sleep(PRE_ONLINE_MSG_RETRY_DURATION);
            }
            if (sendRet != UBSE_OK) {
                UBSE_LOG_ERROR << "[pre_online] task=" << taskName << " nodeId=" << nodeId
                               << ", pre online nodeId=" << preOnLineNode << " send pre online msg failed, "
                               << FormatRetCode(ret);
            }
            ret |= sendRet;
        });
    }
    ptr->Wait();
    return ret;
}

UbseResult PreOnLineExec(ubse::nodeController::UbseNodeInfo currentNode, bool isInitCluster)
{
    UBSE_LOG_INFO << "nodeId=" << currentNode.nodeId << " exec pre online, isInit=" << isInitCluster;
    UbseResult ret = UBSE_OK;
    std::string nodeId = currentNode.nodeId;
    std::string taskName = nodeId + "_" + GenerateRandomStr(10); // 生成长度为10的随机字符串
    std::unordered_set<std::string> remoteNodeIdSet = FilterLcneRemote(currentNode);
    if (remoteNodeIdSet.empty()) {
        UBSE_LOG_INFO << "nodeId=" << nodeId
                      << " has no working lcne remote nodeId, skip pre online, isInit=" << isInitCluster;
        // 当集群只有2个节点场景为初始场景，在集群内没有直连节点的情况下也需要将节点状态置为预上线。
        if (isInitCluster) {
            preOnLineMutexMap.lock();
            nodePreOnLine[currentNode.nodeId] = PreOnLineState::ONLINE;
            preOnLineMutexMap.unlock();
            return UBSE_OK;
        }
        // 当集群存在大于2个节点时，在集群内没有直连节点的情况下，不进行预上线。
        return UBSE_ERROR;
    }
    remoteNodeIdSet.insert(nodeId);
    auto ptr = GenerateTask(taskName);
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "get task=" << taskName << " failed.";
        return UBSE_ERROR_NULLPTR;
    }
    preOnLineTaskMutexMap.lock();
    PreOnLineTask task{};
    task.taskName = taskName;
    task.nodeId = nodeId;
    std::vector<std::string> operateNodes(remoteNodeIdSet.begin(), remoteNodeIdSet.end());
    task.preOnLineNodes = operateNodes;
    task.timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    preOnLineTask[taskName] = task;
    preOnLineTaskMutexMap.unlock();
    ret = ReqPreOnLine(nodeId, taskName, ptr, remoteNodeIdSet);
    RemoveTask(taskName);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << nodeId << ", send pre online msg failed, " << FormatRetCode(ret);
    } else {
        UBSE_LOG_INFO << "nodeId=" << nodeId << ", send pre online msg success.";
    }
    return ret;
}

UbseTaskExecutorPtr GetPreOnlineTask()
{
    auto taskModule = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskModule == nullptr) {
        UBSE_LOG_WARN << "task module not load when pre online";
        return nullptr;
    }
    return taskModule->Get(ubseMemController);
}

void OperatePreOnLine(PreOnLineReq req)
{
    auto ptr = GetPreOnlineTask();
    if (ptr == nullptr) {
        UBSE_LOG_WARN << "pre online task null";
        return;
    }
    ptr->Execute([req]() -> void {
        if (g_globalStop.load()) {
            UBSE_LOG_WARN << "process already stop.";
            return;
        }
        UbseResult ret = UBSE_OK;
        PreOnLineResp resp{req.taskName, GetCurNodeId(), UBSE_OK};
        std::unique_lock<std::shared_mutex> lock(preOnLineOpreateMutex);
        uint64_t preOnlineSize = PreOnLineSize();
        auto module = UbseContext::GetInstance().GetModule<UbseMmiModule>();
        if (module == nullptr) {
            UBSE_LOG_ERROR << "mmi module not load.";
            resp.ret = ret;
            while (true) {
                if (PreOnLineReply(resp) == UBSE_OK) {
                    break;
                }
                UBSE_LOG_ERROR << "reply failed, task=" << req.taskName << ", will retry.";
                sleep(PRE_ONLINE_MSG_RETRY_DURATION);
            }
            return;
        }
        UBSE_LOG_INFO << "print pre online cpu, taskName=" << req.taskName;
        for (auto obj : req.cnas) {
            UBSE_LOG_INFO << "importNodeId=" << obj.importNodeId << ", importSocketId=" << obj.importSocketId
                          << ", exportNodeId=" << obj.exportNodeId << ", exportSocketId=" << obj.exportSocketId
                          << ", scna=" << obj.scna << ", dcna=" << obj.dcna << ", marId=" << obj.marId;
        }
        ret = UbseMmiInterface::GetInstance().PreOnline(req.cnas, preOnlineSize * MB_SIZE);
        resp.ret = ret;
        if (ret == UBSE_OK) {
            UBSE_LOG_INFO << "operate pre online success, task=" << req.taskName;
        } else {
            UBSE_LOG_ERROR << "operate pre online failed, " << FormatRetCode(ret) << ", task=" << req.taskName;
        }
        while (true) {
            if (PreOnLineReply(resp) == UBSE_OK) {
                break;
            }
            UBSE_LOG_ERROR << "reply failed, task=" << req.taskName << ", will retry.";
            sleep(PRE_ONLINE_MSG_RETRY_DURATION);
        }
    });
}

void handlePreOnLineTask(PreOnLineResp reply)
{
    UBSE_LOG_INFO << "task=" << reply.taskName << ", operate node=" << reply.operateNode
                  << " pre online ret=" << reply.ret;
    std::unique_lock<std::shared_mutex> lock(preOnLineTaskMutexMap);
    preOnLineMutexMap.lock();
    auto iter = preOnLineTask.find(reply.taskName);
    if (iter == preOnLineTask.end()) {
        UBSE_LOG_WARN << "task=" << reply.taskName << " not found.";
        preOnLineMutexMap.unlock();
        return;
    }
    if (reply.ret != UBSE_OK) {
        UBSE_LOG_ERROR << "task=" << reply.taskName << ", pre online node=" << reply.operateNode << " failed, "
                       << FormatRetCode(reply.ret);
        // 将任务期望状态置为 offline
        preOnLineTask[reply.taskName].expectState = PreOnLineState::OFFLINE;
        // 从task任务列表里面移除操作节点
        PreOnLineTask &task = iter->second;
        task.preOnLineNodes.erase(
            std::remove(task.preOnLineNodes.begin(), task.preOnLineNodes.end(), reply.operateNode));
        // 将节点预上线状态改为 offline
        UBSE_LOG_ERROR << "task=" << reply.taskName << ", node=" << task.nodeId << " set offline.";
        nodePreOnLine[task.nodeId] = PreOnLineState::OFFLINE;
        if (task.preOnLineNodes.empty()) {
            UBSE_LOG_ERROR << "task=" << reply.taskName << ", node=" << task.nodeId
                           << " all task done, already offline";
            preOnLineTask.erase(reply.taskName);
        }
        preOnLineMutexMap.unlock();
        return;
    }
    UBSE_LOG_INFO << "task=" << reply.taskName << ", pre online node=" << reply.operateNode << " success";
    // 从task任务列表里面移除操作节点
    PreOnLineTask &task = iter->second;
    task.preOnLineNodes.erase(std::remove(task.preOnLineNodes.begin(), task.preOnLineNodes.end(), reply.operateNode));
    if (task.preOnLineNodes.empty() && task.expectState == PreOnLineState::ONLINE) {
        UBSE_LOG_INFO << "task=" << reply.taskName << ", node=" << task.nodeId << " all task success, set online";
        nodePreOnLine[task.nodeId] = PreOnLineState::ONLINE;
        preOnLineTask.erase(reply.taskName);
    }
    preOnLineMutexMap.unlock();
}

void StopPreOnlineTask()
{
    auto taskExecutor = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        UBSE_LOG_ERROR << "task module not load when operate offline.";
        return;
    }
    taskExecutor->Remove(PRE_ONLINE_TASK_NAME);
}

void PreOnLineUnInit()
{
    StopPreOnlineTask();
    auto ret = UbseMmiInterface::GetInstance().UnPreOnline();
    if (ret == UBSE_OK) {
        UBSE_LOG_INFO << "operate offline success";
    } else {
        UBSE_LOG_ERROR << "operate offline failed, " << FormatRetCode(ret);
    }
}

uint32_t SerializePreOnLine(PreOnLineReq req, uint8_t *&buffer, size_t &size)
{
    UbseSerialization outStream;
    outStream << req.taskName;
    outStream << (right_v<size_t>(req.cnas.size()));
    if (!outStream.Check()) {
        UBSE_LOG_ERROR << "Ubse serialize pre online info failed.";
        return UBSE_ERROR;
    }
    for (size_t i = 0; i < req.cnas.size(); i++) {
        UbseSerialization item;
        item << req.cnas[i].importNodeId << req.cnas[i].importSocketId << req.cnas[i].exportNodeId
             << req.cnas[i].exportSocketId << req.cnas[i].scna << req.cnas[i].dcna << req.cnas[i].marId
             << req.cnas[i].seid << req.cnas[i].deid;
        outStream << item;
        if (!outStream.Check()) {
            UBSE_LOG_ERROR << "Ubse serialize cnas[" << i << "]  failed.";
            return UBSE_ERROR;
        }
    }
    size = outStream.GetLength();
    buffer = outStream.GetBuffer(true);
    return UBSE_OK;
}

uint32_t DeSerializePreOnLine(PreOnLineReq &req, uint8_t *buffer, size_t size)
{
    UbseDeSerialization inStream(buffer, size);
    inStream >> req.taskName;
    size_t num = 0;
    inStream >> num;
    if (num > MAX_CNA_LIST_SIZE) {
        UBSE_LOG_ERROR << "Deserialize cnas size failed, size=" << num;
        return UBSE_ERROR;
    }
    req.cnas.reserve(num);
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "Deserialize pre online info failed";
        return UBSE_ERROR;
    }
    for (size_t i = 0; i < num; i++) {
        UbseDeSerialization item;
        inStream >> item;
        SocketCnaInfo cna{};
        item >> cna.importNodeId >> cna.importSocketId >> cna.exportNodeId >> cna.exportSocketId >> cna.scna >>
            cna.dcna >> cna.marId >> cna.seid >> cna.deid;
        if (!inStream.Check()) {
            UBSE_LOG_ERROR << "Ubse deserialize cnas[" << i << "]  failed.";
            return UBSE_ERROR;
        }
        req.cnas.push_back(cna);
    }
    return UBSE_OK;
}

uint32_t SerializePreOnlineResp(PreOnLineResp resp, uint8_t *&buffer, size_t &size)
{
    UbseSerialization outStream;
    outStream << resp.taskName << resp.operateNode << resp.ret;
    if (!outStream.Check()) {
        UBSE_LOG_ERROR << "Ubse serialize pre resp failed.";
        return UBSE_ERROR;
    }
    size = outStream.GetLength();
    buffer = outStream.GetBuffer(true);
    return UBSE_OK;
}

uint32_t DeSerializePreOnLineResp(PreOnLineResp &resp, uint8_t *buffer, size_t size)
{
    UbseDeSerialization inStream(buffer, size);
    inStream >> resp.taskName >> resp.operateNode >> resp.ret;
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "Deserialize pre online resp failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void ClearOnLineMap()
{
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "elc module not load";
        return;
    }
    if (module->IsLeader()) {
        UBSE_LOG_INFO << "current module is leader, skip clear";
        return;
    }
    // 主备倒换后 重新预上线
    preOnLineMutexMap.lock();
    if (!nodePreOnLine.empty()) {
        nodePreOnLine.clear();
    }
    preOnLineMutexMap.unlock();
}

bool IsPreOnLineEnable()
{
    auto module = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "conf module not load.";
        return false;
    }
    bool enablePreOnLine = false;
    auto ret = module->GetConf<bool>("ubse.memory", "ubse.preonline.enable", enablePreOnLine);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get pre online failed.";
        return false;
    }
    return enablePreOnLine;
}

uint64_t PreOnLineSize()
{
    auto module = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "conf module not load.";
        return DEFAULT_PRE_ONLINE_SIZE;
    }
    uint64_t preOnLineSize = 0;
    auto ret = module->GetConf<uint64_t>("ubse.memory", "ubse.preonline.size", preOnLineSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "get pre online size failed.";
        return DEFAULT_PRE_ONLINE_SIZE;
    }
    if (preOnLineSize < MIN_PRE_ONLINE_SIZE || preOnLineSize > MAX_PRE_ONLINE_SIZE ||
        (preOnLineSize % 128 != 0)) { // 需要能被128整除
        UBSE_LOG_ERROR << "pre online not valid, should be in [128, 262144], size=" << preOnLineSize;
        return DEFAULT_PRE_ONLINE_SIZE;
    }
    return preOnLineSize;
}
} // namespace ubse::mem::controller