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

#include "ubse_node_controller_agent.h"

#include <unistd.h>
#include <condition_variable>
#include <mutex>

#include "ubse_common_def.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_event.h"
#include "ubse_logger.h"
#include "ubse_node_com_urma_collector.h"
#include "ubse_node_controller_collector.h"
#include "ubse_node_controller_util.h"
#include "ubse_serial_util.h"
#include "adapter_plugins/mti/ubse_smbios.h"
#include "ubse_timer.h"

const uint32_t UBSE_NODE_COLLECT_RETRY_INTERVAL = 2;  // 节点侧采集失败重试周期，单位/s
const uint32_t UBSE_NODE_REPORT_INTERVAL = 2;         // 节点侧主动向中心侧上报节点内存，拓扑周期；单位秒
constexpr int UBSE_RPC_TIMEOUT_MS = 60000;            // 60秒超时
constexpr UbseResult UBSE_ERROR_TIMEOUT = 0x80000001; // 超时错误码

const std::string UBSE_NODE_AGENT_REPORT_TIMER = "UbseNodeReport";
const std::string UBSE_NODE_CABINET_REPORT_TIMER = "UbseCabinetReport";
const std::string UBSE_NODE_GLOBAL_REPORT_TIMER = "UbseGlobalReport";

std::string UBSE_TOPOLOGY_CHANGE_EVENT = UBSE_EVENT_TOPOLOGY_CHANGE;

UBSE_DEFINE_THIS_MODULE("ubse");
namespace ubse::nodeController {
using namespace ubse::context;
using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::event;
using namespace ubse::timer;
using namespace ubse::common::def;
using namespace ubse::com;
using namespace ubse::serial;
using namespace ubse::adapter_plugins::smbios;

// Agent端消息处理注册
UbseResult RegAgentMsgHandler()
{
    const ubse::com::UbseComEndpoint collectEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER),
        static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_COLLECT)};

    const ubse::com::UbseComEndpoint nodeChangeEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER),
        static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_NODE_CHANGE)};

    auto ret = UbseRegRpcService(collectEndpoint, CollectNodeInfoHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register collect endpoint failed";
        return ret;
    }

    ret = UbseRegRpcService(nodeChangeEndpoint, nodeChangeHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register join endpoint failed";
        return ret;
    }

    UBSE_LOG_INFO << "Agent message handler registered successfully";
    return UBSE_OK;
}

UbseResult UbseNodeControllerAgent::Initialize()
{
    UBSE_LOG_INFO << "UbseNodeControllerAgent init";

    // 注册消息处理器
    auto ret = RegAgentMsgHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register agent message handler failed, " << FormatRetCode(ret);
        return ret;
    }

    // 创建任务执行器
    taskExecutor_ = UbseTaskExecutor::Create("UbseNodeAgent", NO_1, NO_1024);
    if (taskExecutor_ == nullptr || !taskExecutor_->Start()) {
        UBSE_LOG_ERROR << "Create agent task executor failed";
        return UBSE_ERROR_NULLPTR;
    }

    GetClosRole();

    UBSE_LOG_INFO << "UbseNodeControllerAgent initialized successfully";
    return UBSE_OK;
}

void CollectBaseInfo(UbseNodeInfo &info)
{
    while (!g_globalStop.load()) {
        auto ret = CollectNodeBaseInfo(info);
        if (ret == UBSE_OK) {
            return;
        }
        UBSE_LOG_ERROR << "collect node base info failed, " << FormatRetCode(ret);
        sleep(UBSE_NODE_COLLECT_RETRY_INTERVAL);
    }
}

void CollectTopology(UbseNodeInfo &info)
{
    while (!g_globalStop.load()) {
        auto ret = CollectNodeTopology(info);
        if (ret == UBSE_OK) {
            return;
        }
        UBSE_LOG_ERROR << "collect node topology failed, " << FormatRetCode(ret);
        sleep(UBSE_NODE_COLLECT_RETRY_INTERVAL);
    }
}

UbseNodeInfo UbseNodeInfoCollect()
{
    UbseNodeInfo info{};
    auto ret = CollectNodeBaseInfo(info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << UbseNodeController::GetInstance().GetCurrentNodeId()
                       << "collect base info failed, " << FormatRetCode(ret) << ", will return last collect info";
        return UbseNodeController::GetInstance().GetCurNode();
    }
    ret = CollectNodeTopology(info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << UbseNodeController::GetInstance().GetCurrentNodeId()
                       << "collect topology info failed, " << FormatRetCode(ret) << ", will return last collect info";
        return UbseNodeController::GetInstance().GetCurNode();
    }
    ret = CollectSysSentryState(info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << UbseNodeController::GetInstance().GetCurrentNodeId()
                       << "collect sentry info failed, " << FormatRetCode(ret) << ", will return last collect info";
        return UbseNodeController::GetInstance().GetCurNode();
    }
    ret = CollectObmmKernelState(info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << UbseNodeController::GetInstance().GetCurrentNodeId()
                       << "collect obmm kernel info failed, " << FormatRetCode(ret)
                       << ", will return last collect info";
        return UbseNodeController::GetInstance().GetCurNode();
    }

    UBSE_LOG_INFO << "[MEM_COLLECT] success, nodeId=" << info.nodeId << ", podId=" << info.podId
                  << ", numaCount=" << info.numaInfos.size() << ", cpuCount=" << info.cpuInfos.size();

    ret = UbseNodeController::GetInstance().UpdateNodeInfo(info.nodeId, info);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "nodeId=" << info.nodeId << " update node info failed, " << FormatRetCode(ret);
        return UbseNodeController::GetInstance().GetCurNode();
    }

    UbseNodeController::GetInstance().UpdateDevDirConnectInfo();
    return info;
}

static void FillCachedNodeState(UbseNodeInfo &info)
{
    auto cachedInfo = UbseNodeController::GetInstance().GetNodeById(info.nodeId);
    if (cachedInfo.nodeId.empty()) {
        return;
    }

    info.localState = cachedInfo.localState;
    info.clusterState = cachedInfo.clusterState;
}

UbseResult UbseNodeInfoReport()
{
    return UBSE_OK;
}

UbseResult UbseNodeControllerAgent::UbseNodeInfoReportTimerHandler()
{
    // 节点上报，部分场景下可能通信超时，放线程池处理不阻塞定时器
    taskExecutor_->Execute([]() { UbseNodeInfoReport(); });
    return UBSE_OK;
}

UbseResult UbseNodeControllerAgent::UbseCabinetInfoReportTimerHandler()
{
    return UBSE_OK;
}

UbseResult UbseNodeControllerAgent::UbseGlobalInfoReportTimerHandler()
{
    return UBSE_OK;
}

void UbseNodeControllerAgent::StartExec()
{
    UbseNodeInfo info{};
    CollectBaseInfo(info);
    CollectTopology(info);
    UbseNodeController::GetInstance().SetCurrentNodeId(info.nodeId);
    // 将节点刷新至内存，mem ctl从obmm采集账本；海量账本场景下，采集时间较长；需要交由子线程处理

    UbseNodeController::GetInstance().UpdateNodeInfo(info.nodeId, info);
    // 更新 link id
    UbseNodeController::GetInstance().UpdateDevDirConnectInfo();
    // 将节点本地状态刷新至 ready，加入集群选主
    UbseNodeController::GetInstance().UpdateNodeInfoLocalState(UbseNodeLocalState::UBSE_NODE_READY);
    // 注册采集定时器并启动
    UbseTimerHandlerRegister(
        UBSE_NODE_AGENT_REPORT_TIMER, [this]() -> UbseResult { return UbseNodeInfoReportTimerHandler(); },
        UBSE_NODE_REPORT_INTERVAL);

    auto role = GetClosRole();
    UBSE_LOG_INFO << "[CLOS_ROLE] agent start, nodeId=" << info.nodeId << ", role=" << static_cast<uint32_t>(role);

    UbseTimerHandlerRegister(
        UBSE_NODE_CABINET_REPORT_TIMER, [this]() -> UbseResult { return UbseCabinetInfoReportTimerHandler(); },
        UBSE_NODE_REPORT_INTERVAL);

    UbseTimerHandlerRegister(
        UBSE_NODE_GLOBAL_REPORT_TIMER, [this]() -> UbseResult { return UbseGlobalInfoReportTimerHandler(); },
        UBSE_NODE_REPORT_INTERVAL);

    // 注册LCNE变更回调
    UbseSubEvent(UBSE_TOPOLOGY_CHANGE_EVENT, UbseNodeInfoLcneNotifyHandler);
}

UbseResult UbseNodeControllerAgent::Start()
{
    taskExecutor_->Execute([this]() -> void { StartExec(); });
    return UBSE_OK;
}

UbseResult UbseNodeControllerAgent::UbseNodeInfoLcneNotifyHandler(std::string &, std::string &eventMsg)
{
    UBSE_LOG_INFO << "lcne change, start to collect";
    UbseNodeInfo info = UbseNodeInfoCollect();
    FillCachedNodeState(info);
    info.eventMessage = eventMsg;
    UbseNodeController::GetInstance().UpdateNodeInfo(info.nodeId, info);
    UbseNodeController::GetInstance().UpdateDevDirConnectInfo();

    if (GetClosRole() != UbseClosNodeRole::UNKNOWN && !IsGlobalMaster()) {
        std::string prevNodeId;
        auto ret = GetPrevReportNodeId(prevNodeId);
        if (ret == UBSE_OK) {
            return LcneChangeReportNodeInfo(prevNodeId, info);
        }
        UBSE_LOG_WARN << "get prev node failed for lcne report, fallback to single master, " << FormatRetCode(ret);
    }

    UbseRoleInfo masterInfo{};
    auto ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "ubse get master node failed, skip report.";
        return ret;
    }
    return LcneChangeReportNodeInfo(masterInfo.nodeId, info);
}

void UbseNodeControllerAgent::UnInitialize() {}

void UbseNodeControllerAgent::Stop()
{
    UBSE_LOG_INFO << "ubse node agent start to stop executor.";
    if (taskExecutor_ != nullptr) {
        taskExecutor_->Stop();
    }
    UBSE_LOG_INFO << "ubse node agent start to stop report timer.";
    // 停止 内存&拓扑上报定时器
    UbseTimerHandlerUnregister(UBSE_NODE_AGENT_REPORT_TIMER);

    UbseTimerHandlerUnregister(UBSE_NODE_CABINET_REPORT_TIMER);

    UbseTimerHandlerUnregister(UBSE_NODE_GLOBAL_REPORT_TIMER);

    // 解除注册 LCNE 变更回调
    UbseUnSubEvent(UBSE_TOPOLOGY_CHANGE_EVENT, UbseNodeInfoLcneNotifyHandler);
    UBSE_LOG_INFO << "ubse node agent stopped.";
}

// 安全的序列化辅助函数
static UbseResult SafeSerializeUbseNode(const UbseNodeInfo &info, UbseByteBuffer &buffer)
{
    uint8_t *data = nullptr;
    size_t size = 0;
    auto ret = SerializeUbseNode(info, data, size);
    if (ret != UBSE_OK) {
        if (data != nullptr) {
            SafeDeleteArray(data, size);
        }
        return ret;
    }

    buffer = {data, size, [size](uint8_t *p) noexcept {
        SafeDeleteArray(p, size);
    }};
    return UBSE_OK;
}

static UbseResult SafeSerializeUbseNodeList(const std::vector<UbseNodeInfo> &infos, UbseByteBuffer &buffer)
{
    (void)infos;
    (void)buffer;
    return UBSE_OK;
}

static UbseResult SendSingleNodeReportByOpCode(const std::string &nodeId, const UbseNodeInfo &info,
                                               UbseNodeControllerOpCode opCode, const std::string &action)
{
    (void)nodeId;
    (void)info;
    (void)opCode;
    (void)action;
    return UBSE_OK;
}

static UbseResult SendNodeListReportByOpCode(const std::string &nodeId, const std::vector<UbseNodeInfo> &infos,
                                             UbseNodeControllerOpCode opCode, const std::string &action)
{
    (void)nodeId;
    (void)infos;
    (void)opCode;
    (void)action;
    return UBSE_OK;
}

// Agent向Master周期上报节点信息
UbseResult UbseNodeReportNodeInfo(const std::string &nodeId, const UbseNodeInfo &info)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_REPORT),
        .address = nodeId,
    };

    // 使用智能指针管理同步对象
    struct SyncData {
        UbseResult reportRet = UBSE_OK;
        bool callbackCalled = false;
        std::mutex mtx;
        std::condition_variable cv;
    };

    auto syncData = std::make_shared<SyncData>();

    // 使用辅助函数序列化
    UbseByteBuffer reqBuffer;
    auto ret = SafeSerializeUbseNode(info, reqBuffer);
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                      [syncData, nodeId](void *ctx, const UbseByteBuffer &respData, uint32_t resCode) -> void {
        if (resCode != UBSE_OK) {
            UBSE_LOG_ERROR << "report node to nodeId=" << nodeId << " failed, " << FormatRetCode(resCode);
            syncData->reportRet = resCode;
        }

        {
            std::lock_guard<std::mutex> lock(syncData->mtx);
            syncData->callbackCalled = true;
        }
        syncData->cv.notify_one();
    });

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send report nodeId=" << nodeId << " msg failed, " << FormatRetCode(ret);
        return ret;
    }

    // 等待回调完成
    {
        std::unique_lock<std::mutex> lock(syncData->mtx);
        auto timeout = std::chrono::milliseconds(UBSE_RPC_TIMEOUT_MS);
        if (!syncData->cv.wait_for(lock, timeout, [syncData] { return syncData->callbackCalled; })) {
            UBSE_LOG_ERROR << "report node to " << nodeId << " timeout after " << UBSE_RPC_TIMEOUT_MS << "ms";
            return UBSE_ERROR_TIMEOUT;
        }
    }

    return syncData->reportRet;
}

// Agent向Master上报LCNE拓扑变化
UbseResult LcneChangeReportNodeInfo(const std::string &nodeId, const UbseNodeInfo &info)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_LCNE_CHANGE_REPORT_TOPOLOGY),
        .address = nodeId,
    };

    // 使用智能指针
    struct SyncData {
        UbseResult reportRet = UBSE_OK;
        bool callbackCalled = false;
        std::mutex mtx;
        std::condition_variable cv;
    };

    auto syncData = std::make_shared<SyncData>();

    // 使用辅助函数序列化
    UbseByteBuffer reqBuffer;
    auto ret = SafeSerializeUbseNode(info, reqBuffer);
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                      [syncData, nodeId](void *ctx, const UbseByteBuffer &respData, uint32_t resCode) -> void {
        if (resCode != UBSE_OK) {
            UBSE_LOG_ERROR << "lcne, report node to nodeId=" << nodeId << " failed, " << FormatRetCode(resCode);
            syncData->reportRet = resCode;
        }

        {
            std::lock_guard<std::mutex> lock(syncData->mtx);
            syncData->callbackCalled = true;
        }
        syncData->cv.notify_one();
    });

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "lcne change, send report nodeId=" << nodeId << " msg failed, " << FormatRetCode(ret);
        return ret;
    }

    // 等待回调完成
    {
        std::unique_lock<std::mutex> lock(syncData->mtx);
        auto timeout = std::chrono::milliseconds(UBSE_RPC_TIMEOUT_MS);
        if (!syncData->cv.wait_for(lock, timeout, [syncData] { return syncData->callbackCalled; })) {
            UBSE_LOG_ERROR << "lcne change report to " << nodeId << " timeout after " << UBSE_RPC_TIMEOUT_MS << "ms";
            return UBSE_ERROR_TIMEOUT;
        }
    }

    return syncData->reportRet;
}

UbseResult UbseCabinetReportSingleNode(const std::string &nodeId, const UbseNodeInfo &info)
{
    return SendSingleNodeReportByOpCode(nodeId, info, UbseNodeControllerOpCode::NODE_CONTROLLER_SINGLE_NODE_REPORT,
                                        "cabinet single report");
}

UbseResult UbseCabinetReportFullInfo(const std::string &nodeId, const std::vector<UbseNodeInfo> &infos)
{
    return SendNodeListReportByOpCode(nodeId, infos, UbseNodeControllerOpCode::NODE_CONTROLLER_CABINET_FULL_REPORT,
                                      "cabinet full report");
}

UbseResult UbseGlobalReportSingleNode(const std::string &nodeId, const UbseNodeInfo &info)
{
    return SendSingleNodeReportByOpCode(nodeId, info, UbseNodeControllerOpCode::NODE_CONTROLLER_SINGLE_NODE_REPORT,
                                        "global single report");
}

UbseResult UbseGlobalReportFullInfo(const std::string &nodeId, const std::vector<UbseNodeInfo> &infos)
{
    return SendNodeListReportByOpCode(nodeId, infos, UbseNodeControllerOpCode::NODE_CONTROLLER_GLOBAL_FULL_REPORT,
                                      "global full report");
}

// 创建错误响应
static UbseResult CreateErrorResponse(UbseResult errorCode, UbseByteBuffer &resp)
{
    uint8_t *errorBuffer = new (std::nothrow) uint8_t[4];
    if (errorBuffer != nullptr) {
        *reinterpret_cast<uint32_t *>(errorBuffer) = static_cast<uint32_t>(errorCode);
        resp = {errorBuffer, 4, [](uint8_t *p) noexcept {
                    delete[] p;
                }};
        return errorCode;
    } else {
        resp = {nullptr, 0, nullptr}; // 内存分配失败，只能返回空
        return UBSE_ERROR_NULLPTR;
    }
}

// Agent处理Master的采集请求
UbseResult CollectNodeInfoHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UbseNodeInfo info = UbseNodeController::GetInstance().GetCurNode();

    uint8_t *buffer = nullptr;
    size_t size = 0;
    auto ret = SerializeUbseNode(info, buffer, size);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SerializeUbseNode failed: " << FormatRetCode(ret);

        if (buffer != nullptr) {
            SafeDeleteArray(buffer, size);
        }

        return CreateErrorResponse(ret, resp);
    }

    resp = {buffer, size, [size](uint8_t *p) noexcept {
                SafeDeleteArray(p, size);
            }};
    return ret;
}

// GetAllNodeInfoFromRemote的辅助函数
static void GetAllNodeInfoFromRemoteRespHandler(const std::string &nodeId, const UbseByteBuffer &respData,
                                                uint32_t resCode, std::vector<UbseNodeInfo> &infos, UbseResult &getRet)
{
    if (resCode != UBSE_OK) {
        UBSE_LOG_ERROR << "get all node info failed, " << FormatRetCode(resCode);
        getRet = resCode;
        return;
    }
    if (respData.data == nullptr || respData.len == 0) {
        UBSE_LOG_ERROR << "get all node resp null";
        getRet = UBSE_ERROR_NULLPTR;
        return;
    }
    getRet = DeSerializeUbseNodeList(infos, respData.data, respData.len);
    if (getRet != UBSE_OK) {
        UBSE_LOG_ERROR << "get all node deserialize failed, " << FormatRetCode(getRet);
    }
}

UbseResult nodeChangeHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOG_INFO << "Receive node change req from master";

    uint8_t *buffer = nullptr;
    size_t size = 0;
    resp = {buffer, size, [size](uint8_t *p) noexcept {
                SafeDeleteArray(p, size);
            }};

    UbseDeSerialization inStream(req.data, req.len);
    std::string node;
    std::string action;
    inStream >> node >> action;

    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize node join response";
        return CreateErrorResponse(UBSE_ERROR, resp);
    }
    auto ret = SetUrmaUvs();
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "SetUrmaUvs failed: " << FormatRetCode(ret);
    }

    if (action == UBSE_EVENT_NODE_TOPO_LINK_CHANGE || action == UBSE_EVENT_NODE_JOIN) {
        ret = PubNodeUrmaChange(node, action);
    }

    return CreateErrorResponse(ret, resp);
}

// 向Master节点请求全量节点列表
UbseResult GetAllNodeInfoFromRemote(const std::string &nodeId, std::vector<UbseNodeInfo> &infos)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_ALL_NODE),
        .address = nodeId,
    };

    // 同步机制
    UbseResult getRet = UBSE_OK;
    bool callbackCalled = false;
    std::mutex mtx;
    std::condition_variable cv;

    uint8_t *buffer = nullptr;
    size_t size = 0;
    auto ret = SerializeUbseNodeList(std::vector<UbseNodeInfo>{}, buffer, size);
    if (ret != UBSE_OK) {
        // 错误路径：如果buffer已被分配，需要释放
        if (buffer != nullptr) {
            SafeDeleteArray(buffer, size);
        }
        return ret;
    }

    // 只有成功时，用UbseByteBuffer管理buffer
    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t *p) noexcept {
                                 SafeDeleteArray(p, size);
                             }};

    ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                      [&infos, &getRet, &callbackCalled, &mtx, &cv, nodeId](void *ctx, const UbseByteBuffer &respData,
                                                                            uint32_t resCode) -> void {
        GetAllNodeInfoFromRemoteRespHandler(nodeId, respData, resCode, infos, getRet);
        {
            std::lock_guard<std::mutex> lock(mtx);
            callbackCalled = true;
        }
        cv.notify_one();
    });

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send get all node msg failed, " << FormatRetCode(ret);
        return ret;
    }

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&callbackCalled] { return callbackCalled; });
    }

    return getRet;
}

// 处理获取链路信息的回调
static void HandleGetDirConnectInfoCallback(const std::string &nodeId, const UbseByteBuffer &respData, uint32_t resCode,
                                            std::map<std::string, PhysicalLink> &devDirConnectInfoRemote,
                                            UbseResult &getRet, bool &callbackCalled, std::mutex &mtx,
                                            std::condition_variable &cv)
{
    if (resCode != UBSE_OK) {
        UBSE_LOG_ERROR << "get all node info failed, " << FormatRetCode(resCode);
        getRet = resCode;
    } else if (respData.data == nullptr || respData.len == 0) {
        UBSE_LOG_ERROR << "get all node resp null";
        getRet = UBSE_ERROR_NULLPTR;
    } else {
        getRet = DeSerializeDevDirConnectInfo(devDirConnectInfoRemote, respData.data, respData.len);
        if (getRet != UBSE_OK) {
            UBSE_LOG_ERROR << "get devDirConnectInfo deserialize failed, " << FormatRetCode(getRet);
        }
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        callbackCalled = true;
    }
    cv.notify_one();
}

// 向Master节点请求全量链路信息
UbseResult UbseGetDirConnectInfoFromRemote(const std::string &nodeId,
                                           std::map<std::string, PhysicalLink> &devDirConnectInfoRemote)
{
    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_GET_DEV_CONNECT),
        .address = nodeId,
    };

    UbseResult getRet = UBSE_OK;
    bool callbackCalled = false;
    std::mutex mtx;
    std::condition_variable cv;

    uint8_t *buffer = new (std::nothrow) uint8_t[1]; // com不允许空请求
    if (buffer == nullptr) {
        UBSE_LOG_ERROR << "Memory allocation failed.";
        return UBSE_ERROR_NULLPTR;
    }
    size_t size = 1;
    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t *p) noexcept {
        SafeDeleteArray(p, size);
    }};

    auto ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                           [&devDirConnectInfoRemote, &getRet, &callbackCalled, &mtx, &cv,
                            nodeId](void *ctx, const UbseByteBuffer &respData, uint32_t resCode) -> void {
        HandleGetDirConnectInfoCallback(nodeId, respData, resCode, devDirConnectInfoRemote,
                                        getRet, callbackCalled, mtx, cv);
    });

    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "send get all node msg failed, " << FormatRetCode(ret);
        return ret;
    }

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&callbackCalled] { return callbackCalled; });
    }

    return getRet;
}

UbseResult UbseNodeControllerAgent::ReportSingleNodeToPrev(const UbseNodeInfo &info)
{
    (void)info;
    return UBSE_OK;
}

UbseResult UbseNodeControllerAgent::ReportCabinetFullInfo()
{
    return UBSE_OK;
}

UbseResult UbseNodeControllerAgent::ForwardSingleNodeToPrev(const UbseNodeInfo &info)
{
    (void)info;
    return UBSE_OK;
}

UbseResult UbseNodeControllerAgent::ForwardCabinetFullToPrev()
{
    return UBSE_OK;
}

UbseResult SetUrmaUvs()
{
    std::vector<PhysicalLink> links;

    if (!UbseSmbios::GetInstance().IsClosType()) {
        std::map<std::string, PhysicalLink> connectInfo = UbseNodeController::GetInstance().UbseGetDirConnectInfo();
        if (connectInfo.empty()) {
            UBSE_LOG_WARN << "get cur node link size = 0";
        }

        for (const auto &entry : connectInfo) {
            links.push_back(entry.second);
        }

        UBSE_LOG_INFO << "get cur node topo success, update urma uvs";
    }

    auto ret = UbseNodeComUrmaCollector::GetInstance().SetComUrma(links, false);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "set to urma uvs failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult PubNodeUrmaChange(std::string &nodeId, std::string action)
{
    if (action != UBSE_EVENT_NODE_TOPO_LINK_CHANGE && action != UBSE_EVENT_NODE_JOIN) {
        UBSE_LOG_ERROR << "PubEvent " << action << " is not supported.";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "PubEvent " << action << " to urma when change";
    auto ret = UbsePubEvent(action, nodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to PubEvent" << action << ", nodeId = " << nodeId << "," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}
} // namespace ubse::nodeController