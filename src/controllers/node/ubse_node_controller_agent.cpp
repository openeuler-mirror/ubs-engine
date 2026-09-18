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
#include "ubse_election_module.h"
#include "ubse_error.h"
#include "ubse_event.h"
#include "ubse_logger.h"
#include "ubse_node_com_urma_collector.h"
#include "ubse_node_controller_collector.h"
#include "ubse_serial_util.h"
#include "ubse_smbios.h"
#include "ubse_timer.h"

const uint32_t UBSE_NODE_COLLECT_RETRY_INTERVAL = 2;
const uint32_t UBSE_NODE_REPORT_INTERVAL = 2;
constexpr int UBSE_RPC_TIMEOUT_MS = 60000;

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
using namespace ubse::task_executor;
using namespace ubse::adapter_plugins::smbios;

constexpr UbseResult UBSE_ERROR_TIMEOUT = 0x80000001;
const std::string UBSE_NODE_AGENT_REPORT_TIMER = "UbseNodeReport";
std::string UBSE_TOPOLOGY_CHANGE_EVENT = UBSE_EVENT_TOPOLOGY_CHANGE;
// 备侧同步事件处理器序列号：需在节点建链后触发（建链优先级100）
const uint32_t AGENT_STANDBY_PULL_SEQUENCE = 102;
const std::string UBSE_NODE_STANDBY_PULL_HANDLER = "UbseStandbyPullNodeInfo";
const std::string UBSE_NODE_AGENT_CLEAR_MIRROR_HANDLER = "UbseAgentClearMirror";
// 全量快照节点数上限（防恶意/异常报文导致 reserve 异常）
constexpr size_t MAX_MIRROR_NODE_NUM = 1024;

// Agent端消息处理注册
UbseResult RegAgentMsgHandler()
{
    const ubse::com::UbseComEndpoint collectEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER),
        static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_COLLECT)};
    const ubse::com::UbseComEndpoint reportEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER),
        static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_REPORT)};
    const ubse::com::UbseComEndpoint nodeChangeEndpoint = {
        static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER),
        static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_NODE_CHANGE)};

    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "get com module failed";
        return UBSE_ERROR_NULLPTR;
    }

    // SYNC/FULL端点使用自定义handler（继承UbseComBaseMessageHandler），
    // 以便在Handle中通过ctx获取发送方节点Id并校验其为主节点（防任意节点注入镜像状态）
    UbseComBaseMessageHandlerPtr nodeInfoSyncHandler = new (std::nothrow) UbseNodeInfoSyncMsgHandler();
    if (nodeInfoSyncHandler == nullptr) {
        UBSE_LOG_ERROR << "new node info sync handler failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RegRpcService<UbseComBaseBufferMessage, UbseComBaseBufferMessage>(nodeInfoSyncHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register node info sync endpoint failed";
        return ret;
    }

    UbseComBaseMessageHandlerPtr nodeInfoSyncFullHandler = new (std::nothrow) UbseNodeInfoSyncFullMsgHandler();
    if (nodeInfoSyncFullHandler == nullptr) {
        UBSE_LOG_ERROR << "new node info sync full handler failed";
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RegRpcService<UbseComBaseBufferMessage, UbseComBaseBufferMessage>(nodeInfoSyncFullHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register node info sync full endpoint failed";
        return ret;
    }

    ret = UbseRegRpcService(collectEndpoint, CollectNodeInfoHandler);
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

    // 升备：主动拉取全量快照重建镜像（唯一拉取触发点，覆盖委派新备/主降备/重启即备/从转备）
    UbseElectionHandlerBuilder standbyBuilder;
    standbyBuilder.SetHandler([this](UbseElectionEventType, UBSE_ID_TYPE) -> UbseResult {
        if (taskExecutor_ != nullptr) {
            taskExecutor_->Execute([this]() -> void { PullNodeInfoFromMaster(); });
        }
        return UBSE_OK;
    });
    standbyBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    standbyBuilder.SetSequenceId(AGENT_STANDBY_PULL_SEQUENCE);
    standbyBuilder.SetType(UbseElectionEventType::CHANGE_TO_STANDBY);
    standbyBuilder.SetName(UBSE_NODE_STANDBY_PULL_HANDLER);
    UbseElectionChangeAttachHandler(standbyBuilder.Build());

    // 备降从：清空镜像与lastSyncSeq，再次升备时由拉取重建
    UbseElectionHandlerBuilder agentBuilder;
    agentBuilder.SetHandler([this](UbseElectionEventType, UBSE_ID_TYPE) -> UbseResult {
        ClearMirror();
        return UBSE_OK;
    });
    agentBuilder.SetPriority(UbseElectionHandlerPriority::HIGH);
    agentBuilder.SetSequenceId(AGENT_STANDBY_PULL_SEQUENCE);
    agentBuilder.SetType(UbseElectionEventType::CHANGE_TO_AGENT);
    agentBuilder.SetName(UBSE_NODE_AGENT_CLEAR_MIRROR_HANDLER);
    UbseElectionChangeAttachHandler(agentBuilder.Build());

    // 创建任务执行器
    taskExecutor_ = UbseTaskExecutor::Create("UbseNodeAgent", NO_1, NO_1024);
    if (taskExecutor_ == nullptr || !taskExecutor_->Start()) {
        UBSE_LOG_ERROR << "Create agent task executor failed";
        return UBSE_ERROR_NULLPTR;
    }

    UBSE_LOG_INFO << "UbseNodeControllerAgent initialized successfully";
    return UBSE_OK;
}

void CollectBaseInfo(UbseNodeInfo& info)
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

void CollectTopology(UbseNodeInfo& info)
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
    UbseNodeController::GetInstance().UpdateNodeInfo(info.nodeId, info);
    UbseNodeController::GetInstance().UpdateDevDirConnectInfo();
    return info;
}

UbseResult UbseNodeInfoReport()
{
    UbseNodeInfo info = UbseNodeInfoCollect();
    UbseRoleInfo masterInfo{};
    auto ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "ubse get master node failed, skip report.";
        return ret;
    }
    // 若当前节点为主节点，不上报
    if (masterInfo.nodeId == UbseNodeController::GetInstance().GetCurrentNodeId()) {
        return UBSE_OK;
    }
    return UbseNodeReportNodeInfo(masterInfo.nodeId, info);
}

UbseResult UbseNodeControllerAgent::UbseNodeInfoReportTimerHandler()
{
    // 节点上报，部分场景下可能通信超时，放线程池处理不阻塞定时器
    taskExecutor_->Execute([]() { UbseNodeInfoReport(); });
    return UBSE_OK;
}

void UbseNodeControllerAgent::StartExec()
{
    while (!g_globalStop.load()) {
        auto ret = SetUrmaUvs(true);
        if (ret == UBSE_OK) {
            UBSE_LOG_INFO << "set urma uvs successfully";
            break;
        }
        UBSE_LOG_ERROR << "set urma uvs_set_topo_info failed, will retry 3s later";
        sleep(NO_3);
    }

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
    if (!UbseSmbios::GetInstance().IsClosType()) {
        UbseTimerHandlerRegister(
            UBSE_NODE_AGENT_REPORT_TIMER, [this]() -> UbseResult { return UbseNodeInfoReportTimerHandler(); },
            UBSE_NODE_REPORT_INTERVAL);
    }
    // 注册LCNE变更回调
    UbseSubEvent(UBSE_TOPOLOGY_CHANGE_EVENT, UbseNodeInfoLcneNotifyHandler);
}

UbseResult UbseNodeControllerAgent::Start()
{
    taskExecutor_->Execute([this]() -> void { StartExec(); });
    return UBSE_OK;
}

UbseResult UbseNodeControllerAgent::UbseNodeInfoLcneNotifyHandler(std::string&, std::string& eventMsg)
{
    UBSE_LOG_INFO << "lcne change, start to collect";
    UbseNodeInfo info = UbseNodeInfoCollect();
    info.eventMessage = eventMsg;
    UbseNodeController::GetInstance().UpdateNodeInfo(info.nodeId, info);
    UbseNodeController::GetInstance().UpdateDevDirConnectInfo();
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
    // 解除注册 LCNE 变更回调
    UbseUnSubEvent(UBSE_TOPOLOGY_CHANGE_EVENT, UbseNodeInfoLcneNotifyHandler);
    UBSE_LOG_INFO << "ubse node agent stopped.";
}

// 安全的序列化辅助函数
static UbseResult SafeSerializeUbseNode(const UbseNodeInfo& info, UbseByteBuffer& buffer)
{
    uint8_t* data = nullptr;
    size_t size = 0;
    auto ret = SerializeUbseNode(info, data, size);
    if (ret != UBSE_OK) {
        if (data != nullptr) {
            SafeDeleteArray(data, size);
        }
        return ret;
    }

    buffer = {data, size, [size](uint8_t* p) noexcept {
                  SafeDeleteArray(p, size);
              }};
    return UBSE_OK;
}

// Agent向Master周期上报节点信息
UbseResult UbseNodeReportNodeInfo(const std::string& nodeId, const UbseNodeInfo& info)
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
                      [syncData, nodeId](void* ctx, const UbseByteBuffer& respData, uint32_t resCode) -> void {
                          if (resCode != UBSE_OK) {
                              UBSE_LOG_ERROR << "report node to nodeId=" << nodeId << " failed, "
                                             << FormatRetCode(resCode);
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
UbseResult LcneChangeReportNodeInfo(const std::string& nodeId, const UbseNodeInfo& info)
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
                      [syncData, nodeId](void* ctx, const UbseByteBuffer& respData, uint32_t resCode) -> void {
                          if (resCode != UBSE_OK) {
                              UBSE_LOG_ERROR << "lcne, report node to nodeId=" << nodeId << " failed, "
                                             << FormatRetCode(resCode);
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

// 创建错误响应
static UbseResult CreateErrorResponse(UbseResult errorCode, UbseByteBuffer& resp)
{
    uint8_t* errorBuffer = new (std::nothrow) uint8_t[4];
    if (errorBuffer != nullptr) {
        *reinterpret_cast<uint32_t*>(errorBuffer) =
            static_cast<uint32_t>(errorCode); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        resp = {errorBuffer, 4, [](uint8_t* p) noexcept {
                    delete[] p;
                }};
        return errorCode;
    } else {
        resp = {nullptr, 0, nullptr}; // 内存分配失败，只能返回空
        return UBSE_ERROR_NULLPTR;
    }
}

// Agent处理Master的采集请求
UbseResult CollectNodeInfoHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UbseNodeInfo info = UbseNodeController::GetInstance().GetCurNode();

    uint8_t* buffer = nullptr;
    size_t size = 0;
    auto ret = SerializeUbseNode(info, buffer, size);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SerializeUbseNode failed: " << FormatRetCode(ret);

        if (buffer != nullptr) {
            SafeDeleteArray(buffer, size);
        }

        return CreateErrorResponse(ret, resp);
    }

    resp = {buffer, size, [size](uint8_t* p) noexcept {
                SafeDeleteArray(p, size);
            }};
    return ret;
}

// GetAllNodeInfoFromRemote的辅助函数
static void GetAllNodeInfoFromRemoteRespHandler(const std::string& nodeId, const UbseByteBuffer& respData,
                                                uint32_t resCode, std::vector<UbseNodeInfo>& infos, UbseResult& getRet)
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

UbseResult nodeChangeHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOG_INFO << "Receive node change req from master";

    uint8_t* buffer = nullptr;
    size_t size = 0;
    resp = {buffer, size, [size](uint8_t* p) noexcept {
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
    auto ret = SetUrmaUvs(false);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "SetUrmaUvs failed: " << FormatRetCode(ret);
    }

    if (action == UBSE_EVENT_NODE_TOPO_LINK_CHANGE || action == UBSE_EVENT_NODE_JOIN) {
        ret = PubNodeUrmaChange(node, action);
    }

    return CreateErrorResponse(ret, resp);
}

namespace {
// 校验SYNC/FULL消息发送方是否为当前主节点，防止任意集群节点构造消息注入镜像状态（S2-01）
bool VerifySyncSenderIsMaster(const std::string& senderNodeId)
{
    std::string masterNodeId;
    if (UbseGetMasterNodeId(masterNodeId) != UBSE_OK) {
        UBSE_LOG_WARN << "[NODE_SYNC] get master node id failed, reject sync msg, sender=" << senderNodeId;
        return false;
    }
    if (senderNodeId.empty() || senderNodeId != masterNodeId) {
        UBSE_LOG_WARN << "[NODE_SYNC] reject sync msg from non-master node, sender=" << senderNodeId
                      << ", master=" << masterNodeId;
        return false;
    }
    return true;
}

// 将同步处理结果回填RPC响应体（与UbseNetMessageHandler的响应回填方式一致）
UbseResult WriteSyncResp(const ubse::message::UbseBaseMessagePtr& rsp, UbseByteBuffer& respData)
{
    auto respPtr = ubse::message::UbseBaseMessage::DeConvert<UbseComBaseBufferMessage>(rsp);
    if (respPtr.Get() == nullptr) {
        UBSE_LOG_ERROR << "[NODE_SYNC] deconvert response message failed";
        if (respData.freeFunc != nullptr) {
            respData.freeFunc(respData.data);
        }
        return UBSE_ERROR_NULLPTR;
    }
    if (respPtr->SetInputRawData(respData.data, static_cast<uint32_t>(respData.len)) != UBSE_OK) {
        std::string tmpData = UbseReplyResultToString(UbseReplyResult::ERR_NO_REPLY);
        respPtr->SetInputRawData(reinterpret_cast<uint8_t*>(tmpData.data()),
                                 static_cast<uint32_t>(tmpData.size())); // NOLINT
    }
    respPtr->Deserialize();
    if (respData.freeFunc != nullptr) {
        respData.freeFunc(respData.data);
    }
    return UBSE_OK;
}
} // namespace

UbseResult UbseNodeInfoSyncMsgHandler::Handle(const ubse::message::UbseBaseMessagePtr& req,
                                              const ubse::message::UbseBaseMessagePtr& rsp,
                                              com::UbseComBaseMessageHandlerCtxPtr ctx)
{
    if (g_globalStop) {
        UBSE_LOG_INFO << "ubse is stopped, ignore node info sync msg";
        return UBSE_OK;
    }
    // 校验发送方身份：ctx->GetDstId()为发送方节点Id（通信层ParseContextMsg按对端channel设置）
    if (ctx == nullptr || !VerifySyncSenderIsMaster(ctx->GetDstId())) {
        return UBSE_ERROR;
    }
    auto reqPtr = ubse::message::UbseBaseMessage::DeConvert<UbseComBaseBufferMessage>(req);
    if (reqPtr.Get() == nullptr) {
        UBSE_LOG_ERROR << "[NODE_SYNC] deconvert request message failed, req is nullptr";
        return UBSE_ERROR_NULLPTR;
    }
    UbseByteBuffer reqData{reqPtr->GetData(), reqPtr->GetDataLen(), nullptr};
    UbseByteBuffer respData{};
    auto ret = UbseNodeControllerAgent::GetInstance().HandleNodeInfoSync(reqData, respData);
    auto writeRet = WriteSyncResp(rsp, respData);
    return writeRet != UBSE_OK ? writeRet : ret;
}

UbseResult UbseNodeInfoSyncFullMsgHandler::Handle(const ubse::message::UbseBaseMessagePtr& req,
                                                  const ubse::message::UbseBaseMessagePtr& rsp,
                                                  com::UbseComBaseMessageHandlerCtxPtr ctx)
{
    if (g_globalStop) {
        UBSE_LOG_INFO << "ubse is stopped, ignore node info sync full msg";
        return UBSE_OK;
    }
    // 校验发送方身份：ctx->GetDstId()为发送方节点Id（通信层ParseContextMsg按对端channel设置）
    if (ctx == nullptr || !VerifySyncSenderIsMaster(ctx->GetDstId())) {
        return UBSE_ERROR;
    }
    auto reqPtr = ubse::message::UbseBaseMessage::DeConvert<UbseComBaseBufferMessage>(req);
    if (reqPtr.Get() == nullptr) {
        UBSE_LOG_ERROR << "[NODE_SYNC] deconvert request message failed, req is nullptr";
        return UBSE_ERROR_NULLPTR;
    }
    UbseByteBuffer reqData{reqPtr->GetData(), reqPtr->GetDataLen(), nullptr};
    UbseByteBuffer respData{};
    auto ret = UbseNodeControllerAgent::GetInstance().HandleNodeInfoSyncFull(reqData, respData);
    auto writeRet = WriteSyncResp(rsp, respData);
    return writeRet != UBSE_OK ? writeRet : ret;
}

uint16_t UbseNodeInfoSyncMsgHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER);
}

uint16_t UbseNodeInfoSyncMsgHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_NODE_INFO_SYNC);
}

uint16_t UbseNodeInfoSyncFullMsgHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::NODE_CONTROLLER);
}

uint16_t UbseNodeInfoSyncFullMsgHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_NODE_INFO_SYNC_FULL);
}

bool UbseNodeControllerAgent::IsStandbyNode() const
{
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        return false;
    }
    ubse::election::Node standbyNode{};
    if (module->UbseGetStandbyNode(standbyNode) != UBSE_OK || standbyNode.id.empty()) {
        return false;
    }
    return standbyNode.id == UbseNodeController::GetInstance().GetCurrentNodeId();
}

void UbseNodeControllerAgent::ClearMirror()
{
    std::unique_lock<std::shared_mutex> lock(mirrorMutex_);
    nodeInfoMirror_.clear();
    faultProtectMirror_.clear();
    lastSyncSeq_ = 0;
    UBSE_LOG_INFO << "node info mirror cleared";
}

void UbseNodeControllerAgent::GetMirrorSnapshot(std::unordered_map<std::string, UbseNodeInfo>& mirror,
                                                std::unordered_map<std::string, uint64_t>& faultProtect)
{
    std::shared_lock<std::shared_mutex> lock(mirrorMutex_);
    mirror = nodeInfoMirror_;
    faultProtect = faultProtectMirror_;
}

UbseResult UbseNodeControllerAgent::HandleNodeInfoSync(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    // 非备角色（已升主/从节点）忽略残留推送，防旧主残余消息污染
    if (!IsStandbyNode()) {
        UBSE_LOG_INFO << "current node not standby, ignore node info sync";
        return UBSE_OK;
    }
    if (req.data == nullptr || req.len == 0) {
        return UBSE_ERROR;
    }

    UbseDeSerialization inStream(req.data, req.len);
    uint64_t syncSeq = 0;
    uint64_t faultUpdateTimeMs = 0;
    UbseNodeInfo nodeInfo{};
    inStream >> syncSeq >> faultUpdateTimeMs;
    UbseDeSerialization item;
    inStream >> item;
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "deserialize node info sync head failed";
        return UBSE_ERROR;
    }
    auto ret = ParseNodeInfo(nodeInfo, item);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "parse node info sync node failed, " << FormatRetCode(ret);
        return ret;
    }

    // 双过滤：主节点自己/备节点自己不落镜像（旧主倒换后不在镜像，升主不会对其对账，防卡SMOOTHING）
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ubse::election::Node masterNode{};
    (void)module->UbseGetMasterNode(masterNode);
    auto selfId = UbseNodeController::GetInstance().GetCurrentNodeId();
    if (nodeInfo.nodeId == masterNode.id || nodeInfo.nodeId == selfId) {
        UBSE_LOG_INFO << "nodeId=" << nodeInfo.nodeId << " is master/self, skip mirror";
        return UBSE_OK;
    }

    std::unique_lock<std::shared_mutex> lock(mirrorMutex_);
    // syncSeq乱序过滤：仅应用seq > lastSyncSeq的消息
    if (syncSeq <= lastSyncSeq_) {
        UBSE_LOG_DEBUG << "node info sync out of order, syncSeq=" << syncSeq << ", lastSyncSeq=" << lastSyncSeq_;
        return UBSE_OK;
    }
    nodeInfoMirror_[nodeInfo.nodeId] = nodeInfo;
    if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT && faultUpdateTimeMs != 0) {
        faultProtectMirror_[nodeInfo.nodeId] = faultUpdateTimeMs;
    } else if (nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_FAULT) {
        faultProtectMirror_.erase(nodeInfo.nodeId);
    }
    lastSyncSeq_ = syncSeq;
    UBSE_LOG_INFO << "[NODE_SYNC] standby upsert mirror, nodeId=" << nodeInfo.nodeId
                  << ", state=" << NodeClusterStateToStr(nodeInfo.clusterState)
                  << ", faultUpdateTimeMs=" << faultUpdateTimeMs << ", syncSeq=" << syncSeq;
    return UBSE_OK;
}

UbseResult UbseNodeControllerAgent::HandleNodeInfoSyncFull(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    if (req.data == nullptr || req.len == 0) {
        return UBSE_ERROR;
    }
    return ProcessNodeInfoSyncFull(req.data, req.len);
}

UbseResult UbseNodeControllerAgent::ProcessNodeInfoSyncFull(const uint8_t* data, uint32_t len)
{
    // 非备角色忽略（拉取回调与周期收包共用本函数）
    if (!IsStandbyNode()) {
        UBSE_LOG_INFO << "current node not standby, ignore node info sync full";
        return UBSE_OK;
    }

    UbseDeSerialization inStream(data, len);
    uint64_t syncSeq = 0;
    size_t num = 0;
    inStream >> syncSeq >> num;
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "deserialize node info sync full head failed";
        return UBSE_ERROR;
    }
    if (num > MAX_MIRROR_NODE_NUM) {
        UBSE_LOG_ERROR << "node info sync full node num too large, num=" << num;
        return UBSE_ERROR;
    }

    std::vector<UbseNodeInfo> nodeList;
    nodeList.reserve(num);
    for (size_t i = 0; i < num; i++) {
        UbseDeSerialization item;
        inStream >> item;
        if (!inStream.Check()) {
            UBSE_LOG_ERROR << "deserialize node info sync full item failed, index=" << i;
            return UBSE_ERROR;
        }
        UbseNodeInfo info{};
        auto ret = ParseNodeInfo(info, item);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "parse node info sync full item failed, index=" << i << ", " << FormatRetCode(ret);
            return ret;
        }
        nodeList.push_back(info);
    }
    std::map<std::string, uint64_t> faultProtectMap;
    inStream >> faultProtectMap;
    if (!inStream.Check()) {
        UBSE_LOG_ERROR << "deserialize node info sync full fault map failed";
        return UBSE_ERROR;
    }

    // 双过滤：主节点自己/备节点自己不落镜像
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ubse::election::Node masterNode{};
    (void)module->UbseGetMasterNode(masterNode);
    auto selfId = UbseNodeController::GetInstance().GetCurrentNodeId();
    std::unordered_map<std::string, UbseNodeInfo> filteredMap;
    std::unordered_map<std::string, uint64_t> filteredFaultMap;
    for (const auto& node : nodeList) {
        if (node.nodeId == masterNode.id || node.nodeId == selfId) {
            continue;
        }
        filteredMap[node.nodeId] = node;
        auto iter = faultProtectMap.find(node.nodeId);
        if (iter != faultProtectMap.end()) {
            filteredFaultMap[node.nodeId] = iter->second;
        }
    }

    // FULL全量快照豁免seq过滤：全量覆盖天然幂等，无条件落地。
    // 防止主节点重启后syncSeq归零、而本侧lastSyncSeq保留重启前大值时，快照被误判乱序丢弃导致镜像冻结。
    // 单点推送SYNC仍按seq严格过滤防乱序覆盖。
    std::unique_lock<std::shared_mutex> lock(mirrorMutex_);
    nodeInfoMirror_ = std::move(filteredMap);
    faultProtectMirror_ = std::move(filteredFaultMap);
    lastSyncSeq_ = syncSeq;
    // 观测日志：全量快照落地（含镜像节点状态摘要）
    std::string stateSummary;
    for (const auto& iter : nodeInfoMirror_) {
        stateSummary += iter.first + ":" + NodeClusterStateToStr(iter.second.clusterState) + " ";
    }
    UBSE_LOG_INFO << "[NODE_SYNC_FULL] standby apply full snapshot, syncSeq=" << syncSeq
                  << ", mirrorSize=" << nodeInfoMirror_.size() << ", nodes=[" << stateSummary << "]";
    return UBSE_OK;
}

void UbseNodeControllerAgent::PullNodeInfoFromMaster()
{
    if (!IsStandbyNode()) {
        UBSE_LOG_INFO << "current node not standby, skip pull node info";
        return;
    }
    auto module = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "election module not load";
        return;
    }
    ubse::election::Node masterNode{};
    if (module->UbseGetMasterNode(masterNode) != UBSE_OK || masterNode.id.empty()) {
        UBSE_LOG_WARN << "get master node failed, skip pull node info";
        return;
    }

    // 清理镜像并重置lastSyncSeq，防止拉回的全量seq小于清理前的lastSyncSeq被乱序过滤丢弃
    ClearMirror();

    const ubse::com::UbseComEndpoint endpoint{
        .moduleId = static_cast<uint16_t>(ubse::com::UbseModuleCode::NODE_CONTROLLER),
        .serviceId = static_cast<uint32_t>(UbseNodeControllerOpCode::NODE_CONTROLLER_NODE_INFO_SYNC_REQ),
        .address = masterNode.id,
    };

    UbseSerialization outStream;
    outStream << UbseNodeController::GetInstance().GetCurrentNodeId();
    if (!outStream.Check()) {
        UBSE_LOG_ERROR << "serialize node info sync req failed";
        return;
    }
    size_t size = outStream.GetLength();
    uint8_t* buffer = outStream.GetBuffer(true);
    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t* p) noexcept {
                                 SafeDeleteArray(p, size);
                             }};

    // 回调共享数据：与 UbseNodeReportNodeInfo/LcneChangeReportNodeInfo 的 SyncData 模式保持一致，
    // 回调按值捕获 shared_ptr 延长生命周期，避免捕获栈引用依赖"同步内联回调"的隐含语义。
    struct SyncData {
        UbseResult pullRet = UBSE_OK;
        bool callbackCalled = false;
        std::mutex mtx;
        std::condition_variable cv;
    };
    auto syncData = std::make_shared<SyncData>();

    auto ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                           [this, syncData](void* ctx, const UbseByteBuffer& respData, uint32_t resCode) {
                               if (resCode != UBSE_OK) {
                                   UBSE_LOG_WARN << "pull node info from master failed, " << FormatRetCode(resCode);
                                   syncData->pullRet = resCode;
                               } else if (respData.data != nullptr && respData.len != 0) {
                                   syncData->pullRet = ProcessNodeInfoSyncFull(respData.data, respData.len);
                               } else {
                                   syncData->pullRet = UBSE_ERROR_NULLPTR;
                               }
                               {
                                   std::lock_guard<std::mutex> lock(syncData->mtx);
                                   syncData->callbackCalled = true;
                               }
                               syncData->cv.notify_one();
                           });
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "send node info sync req failed, " << FormatRetCode(ret);
        return;
    }
    // 等待回调完成：必须带超时。agent工作线程为单线程，若主节点RPC无响应而无限等待，
    // 会永久阻塞周期上报/节点采集等全部任务（与UbseNodeReportNodeInfo超时策略保持一致）
    {
        std::unique_lock<std::mutex> lock(syncData->mtx);
        auto timeout = std::chrono::milliseconds(UBSE_RPC_TIMEOUT_MS);
        if (!syncData->cv.wait_for(lock, timeout, [syncData] { return syncData->callbackCalled; })) {
            UBSE_LOG_WARN << "pull node info from master timeout after " << UBSE_RPC_TIMEOUT_MS << "ms";
            return;
        }
    }
    if (syncData->pullRet != UBSE_OK) {
        UBSE_LOG_WARN << "pull node info from master failed, " << FormatRetCode(syncData->pullRet);
        return;
    }
    // 观测日志：备节点首次全量拉取成功（含镜像节点状态摘要）
    std::string stateSummary;
    size_t mirrorSize = 0;
    {
        std::shared_lock<std::shared_mutex> lock(mirrorMutex_);
        mirrorSize = nodeInfoMirror_.size();
        for (const auto& iter : nodeInfoMirror_) {
            stateSummary += iter.first + ":" + NodeClusterStateToStr(iter.second.clusterState) + " ";
        }
    }
    UBSE_LOG_INFO << "[NODE_SYNC_REQ] standby pull full snapshot success, masterId=" << masterNode.id
                  << ", mirrorSize=" << mirrorSize << ", nodes=[" << stateSummary << "]";
}

// 向Master节点请求全量节点列表
UbseResult GetAllNodeInfoFromRemote(const std::string& nodeId, std::vector<UbseNodeInfo>& infos)
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

    uint8_t* buffer = nullptr;
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
    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t* p) noexcept {
                                 SafeDeleteArray(p, size);
                             }};

    ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                      [&infos, &getRet, &callbackCalled, &mtx, &cv, nodeId](void* ctx, const UbseByteBuffer& respData,
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
static void HandleGetDirConnectInfoCallback(const std::string& nodeId, const UbseByteBuffer& respData, uint32_t resCode,
                                            std::map<std::string, PhysicalLink>& devDirConnectInfoRemote,
                                            UbseResult& getRet, bool& callbackCalled, std::mutex& mtx,
                                            std::condition_variable& cv)
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
UbseResult UbseGetDirConnectInfoFromRemote(const std::string& nodeId,
                                           std::map<std::string, PhysicalLink>& devDirConnectInfoRemote)
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

    uint8_t* buffer = new (std::nothrow) uint8_t[1]; // com不允许空请求
    if (buffer == nullptr) {
        UBSE_LOG_ERROR << "Memory allocation failed.";
        return UBSE_ERROR_NULLPTR;
    }
    size_t size = 1;
    UbseByteBuffer reqBuffer{buffer, size, [size](uint8_t* p) noexcept {
                                 SafeDeleteArray(p, size);
                             }};

    auto ret = UbseRpcSend(endpoint, reqBuffer, nullptr,
                           [&devDirConnectInfoRemote, &getRet, &callbackCalled, &mtx, &cv,
                            nodeId](void* ctx, const UbseByteBuffer& respData, uint32_t resCode) -> void {
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

UbseResult FillLinkAndBondingFM(bool isBeforeElection, std::vector<PhysicalLink>& links)
{
    if (isBeforeElection) {
        auto ret = UbseNodeComUrmaCollector::GetInstance().GetCurNodeTopo(links);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "get cur node topo failed";
            return ret;
        }
        ret = UbseNodeComUrmaCollector::GetInstance().FillComUrmaInfo();
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "fill com urma info failed";
            return ret;
        }
        UBSE_LOG_INFO << "fill cur node topo and urma device success, set to urma uvs first time";
    } else {
        std::map<std::string, PhysicalLink> connectInfo = UbseNodeController::GetInstance().UbseGetDirConnectInfo();
        if (connectInfo.empty()) {
            UBSE_LOG_WARN << "get cur node link size = 0";
        }
        for (const auto& entry : connectInfo) {
            links.push_back(entry.second);
        }
        UBSE_LOG_INFO << "get cur node topo success, update urma uvs";
    }
    return UBSE_OK;
}

UbseResult FillLinkAndBondingClos(bool isBeforeElection = false)
{
    if (!isBeforeElection) {
        return UBSE_OK;
    }

    auto ret = UbseNodeComUrmaCollector::GetInstance().FillComUrmaInfoClos();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "fill com urma info failed";
    }
    return ret;
}

UbseResult SetUrmaUvs(bool isBeforeElection = false)
{
    std::vector<PhysicalLink> links;
    if (UbseSmbios::GetInstance().IsClosType()) {
        auto ret = FillLinkAndBondingClos(isBeforeElection);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "fill links and bonding clos failed";
            return ret;
        }
    } else {
        auto ret = FillLinkAndBondingFM(isBeforeElection, links);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "fill links and bonding failed";
            return ret;
        }
    }
    auto ret = UbseNodeComUrmaCollector::GetInstance().SetComUrma(links, isBeforeElection);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "set to urma uvs failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult PubNodeUrmaChange(std::string& nodeId, std::string action)
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