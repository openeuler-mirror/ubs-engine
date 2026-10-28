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

#include "ubse_ipc_client.h"

#include <pthread.h>
#include <securec.h>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>

#include "ubse_common_def.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_common_def.h"
#include "ubse_ipc_log.h"
#include "ubse_ipc_message.h"
#include "ubse_ipc_utils.h"
#include "ubse_uds_client.h"
#include "ubse_vsock_client.h"

using namespace ubse::ipc;
using UbseClientIpcHandler = std::function<uint32_t(const UbseRequestMessage&)>;

struct PairHash {
    size_t operator()(const std::pair<uint16_t, uint16_t>& p) const
    {
        // 将两个 uint16_t 组合成一个 size_t 作为哈希值
        return (static_cast<size_t>(p.first) << 16) | p.second; // // 移动16位
    }
};

using UbseClientIpcHandlerMap = std::unordered_map<std::pair<uint16_t, uint16_t>, UbseClientIpcHandler, PairHash>;
std::mutex clientIpcHandlerMutex; // mutex锁
static UbseClientIpcHandlerMap clientIpcHandlerMap{};
thread_local std::string ubseSocketPath = ubse::common::def::UBSE_UDS_SOCKET_PATH;
constexpr uint32_t MAX_RESPONSE_SIZE = 10 * 1024 * 1024;

static bool IsVsockEnabled()
{
    auto ubseConfModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::config::UbseConfModule>();
    if (ubseConfModule == nullptr) {
        return false;
    }
    uint32_t cid = 0;
    uint32_t port = 0;
    auto retCid = ubseConfModule->GetConf<uint32_t>("ubse.proxy", "proxy.server.cid", cid);
    auto retPort = ubseConfModule->GetConf<uint32_t>("ubse.proxy", "proxy.server.port", port);
    return (retCid == UBSE_OK && retPort == UBSE_OK && cid > 0 && port > 0);
}

static ubse::ipc::UbseUDSClient& GetLongLinkClient()
{
    if (IsVsockEnabled()) {
        return ubse::ipc::UbseVsockClient::GetInstance();
    }
    return ubse::ipc::UbseUDSClient::GetInstance();
}

/* ================== SDK runtime 状态机(支持fork前finalize/子进程initialize) ================== */
// 排空上限(秒): 对齐默认业务总超时(ubse::ipc::DEFAULT_TOTAL_TIMEOUT=1800s)并留60秒余量,
// 避免正常在途调用(等慢服务端响应)误触发超时。定义为非static可写全局, UT可extern声明后
// 直接改写以覆盖drain超时分支, 无需等待默认上限
uint32_t g_sdkDrainTimeoutSec = ubse::ipc::DEFAULT_TOTAL_TIMEOUT / 1000 + 60;

namespace {
enum class UbseSdkRuntimeState : uint32_t
{
    READY,      // 运行中, 业务调用放行
    FINALIZING, // finalize 排空/拆除中, 新业务调用拒绝
    FINALIZED   // finalize 完成, 等待 initialize 重新拉起
};

std::mutex g_sdkRuntimeMtx;           // 保护状态字与活跃调用计数
std::condition_variable g_sdkDrainCv; // finalize 排空 in-flight 调用
UbseSdkRuntimeState g_sdkRuntimeState = UbseSdkRuntimeState::READY;
uint32_t g_sdkActiveCalls = 0;                // in-flight 业务调用计数(活跃引用)
thread_local uint32_t g_tlsInSdkCallback = 0; // SDK回调上下文标记, 拦截回调内finalize防自死锁

// 回调上下文RAII守卫: HandleRequest派发用户回调期间置TLS标记
class UbseSdkCallbackScope {
public:
    UbseSdkCallbackScope()
    {
        ++g_tlsInSdkCallback;
    }
    ~UbseSdkCallbackScope()
    {
        --g_tlsInSdkCallback;
    }
};

// 尝试进入finalize: READY则迁移到FINALIZING并返回true; FINALIZING/FINALIZED下立即返回false
bool SdkRuntimeTryBeginFinalize()
{
    std::lock_guard<std::mutex> lock(g_sdkRuntimeMtx);
    if (g_sdkRuntimeState == UbseSdkRuntimeState::FINALIZING) {
        IPC_LOG_ERROR << "[SDK_RUNTIME] finalize rejected: another finalize in progress, return immediately";
        return false;
    }
    if (g_sdkRuntimeState == UbseSdkRuntimeState::FINALIZED) {
        IPC_LOG_INFO << "[SDK_RUNTIME] finalize: already finalized, ignore";
        return false;
    }
    g_sdkRuntimeState = UbseSdkRuntimeState::FINALIZING;
    return true;
}

// 排空活跃调用: 等待所有 in-flight 业务调用完成(fork前提, 保证子进程不继承半途调用);
// 超时上限与最长业务总超时对齐, 超时放弃等待并告警, 防finalize调用方(常为退出路径)无限挂起
void SdkRuntimeDrainCalls()
{
    std::unique_lock<std::mutex> lock(g_sdkRuntimeMtx);
    bool drained =
        g_sdkDrainCv.wait_for(lock, std::chrono::seconds(g_sdkDrainTimeoutSec), []() { return g_sdkActiveCalls == 0; });
    if (drained) {
        IPC_LOG_INFO << "[SDK_RUNTIME] in-flight calls drained";
        return;
    }
    // 超时不清零计数: UbseSdkRuntimeExit已有下溢保护, 无需此处防下溢;
    // 保留残余计数可保证"超时→重拉→新调用"序列下, 旧调用的Exit不会误扣新调用的活跃计数
    IPC_LOG_ERROR << "[SDK_RUNTIME] drain timeout(" << g_sdkDrainTimeoutSec
                  << "s), residual activeCalls=" << g_sdkActiveCalls << ", continue teardown";
}

// 拆除: 停长连接(join事件循环/重连/任务执行线程, 含 in-flight回调), 清理已注册handler
void SdkRuntimeTeardown()
{
    GetLongLinkClient().Stop();
    {
        std::lock_guard<std::mutex> lock(clientIpcHandlerMutex);
        clientIpcHandlerMap.clear();
    }
}
} // namespace

// 占用一个活跃引用, READY状态下返回true(业务调用放行), 否则返回false(拒绝)
bool UbseSdkRuntimeEnter()
{
    std::lock_guard<std::mutex> lock(g_sdkRuntimeMtx);
    if (g_sdkRuntimeState != UbseSdkRuntimeState::READY) {
        return false;
    }
    ++g_sdkActiveCalls;
    return true;
}

// 释放活跃引用并唤醒排空等待者
void UbseSdkRuntimeExit()
{
    {
        std::lock_guard<std::mutex> lock(g_sdkRuntimeMtx);
        if (g_sdkActiveCalls > 0) {
            --g_sdkActiveCalls;
        }
    }
    g_sdkDrainCv.notify_all();
}

namespace {
// 活跃引用RAII守卫: 构造时过闸计数, 析构时释放并唤醒finalize排空等待
class UbseSdkCallGate {
public:
    UbseSdkCallGate() : entered_(UbseSdkRuntimeEnter()) {}
    ~UbseSdkCallGate()
    {
        if (entered_) {
            UbseSdkRuntimeExit();
        }
    }
    bool Entered() const
    {
        return entered_;
    }

private:
    bool entered_;
};

// fork防御(S3-02): 使用方违约(未先finalize)fork时, prepare持锁穿越fork保证子进程获得
// 干净锁(否则子进程SDK全接口在持锁处死锁); 子进程仅剩fork调用线程, 未完finalize视同完成。
// 锁覆盖范围: runtime状态锁(g_sdkRuntimeMtx)与handler表锁(clientIpcHandlerMutex)两把,
// 加锁顺序与业务路径一致(先runtime锁后handler表锁), 长连接客户端内部锁不在此承诺范围
void SdkRuntimeAtforkPrepare()
{
    g_sdkRuntimeMtx.lock();       // 先runtime锁
    clientIpcHandlerMutex.lock(); // 后handler表锁
}

void SdkRuntimeAtforkParent()
{
    clientIpcHandlerMutex.unlock(); // 解锁顺序与加锁相反
    g_sdkRuntimeMtx.unlock();
}

void SdkRuntimeAtforkChild()
{
    g_sdkActiveCalls = 0; // 其余在途线程未被fork复制, 计数归零
    if (g_sdkRuntimeState == UbseSdkRuntimeState::FINALIZING) {
        g_sdkRuntimeState = UbseSdkRuntimeState::FINALIZED; // 供子进程initialize重拉
    }
    clientIpcHandlerMutex.unlock();
    g_sdkRuntimeMtx.unlock();
}

std::once_flag g_atforkOnce;

void SdkRuntimeRegisterAtfork()
{
    std::call_once(g_atforkOnce, []() {
        int rc = pthread_atfork(SdkRuntimeAtforkPrepare, SdkRuntimeAtforkParent, SdkRuntimeAtforkChild);
        if (rc != 0) {
            IPC_LOG_ERROR << "[SDK_RUNTIME] pthread_atfork register failed, rc=" << rc;
        }
    });
}
} // namespace

uint32_t ubse_sdk_runtime_initialize(void)
{
    SdkRuntimeRegisterAtfork();
    std::lock_guard<std::mutex> lock(g_sdkRuntimeMtx);
    if (g_sdkRuntimeState == UbseSdkRuntimeState::FINALIZING) {
        IPC_LOG_ERROR << "[SDK_RUNTIME] initialize rejected: finalize in progress";
        return UBSE_SDK_RUNTIME_INIT_REJECTED;
    }
    if (g_sdkRuntimeState == UbseSdkRuntimeState::FINALIZED) {
        g_sdkRuntimeState = UbseSdkRuntimeState::READY;
        IPC_LOG_INFO << "[SDK_RUNTIME] runtime re-initialized after finalize";
    }
    return UBSE_OK;
}

void ubse_sdk_runtime_finalize(void)
{
    // TLS护栏先行: 回调上下文内调用finalize会与Stop()的线程join形成自死锁, 直接拒绝
    if (g_tlsInSdkCallback > 0) {
        IPC_LOG_ERROR << "[SDK_RUNTIME] finalize rejected: called inside sdk callback";
        return;
    }
    if (!SdkRuntimeTryBeginFinalize()) {
        return; // 并发调用违反契约, 防御性软失败; FINALIZED下幂等返回
    }
    SdkRuntimeDrainCalls();
    SdkRuntimeTeardown();
    {
        std::lock_guard<std::mutex> lock(g_sdkRuntimeMtx);
        g_sdkRuntimeState = UbseSdkRuntimeState::FINALIZED;
    }
    IPC_LOG_INFO << "[SDK_RUNTIME] finalize completed";
}

static uint32_t CopyResponseBody(const UbseResponseMessage& src, ubse_api_buffer_t* dest)
{
    dest->buffer = nullptr;
    dest->length = 0;
    if (src.header.bodyLen == 0 || !src.body) {
        return UBSE_OK;
    }
    if (src.header.bodyLen > MAX_RESPONSE_SIZE) {
        IPC_LOG_ERROR << "IPC response body length too large, bodyLen=" << src.header.bodyLen
                      << ", maxResponseSize=" << MAX_RESPONSE_SIZE;
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    dest->buffer = static_cast<uint8_t*>(malloc(src.header.bodyLen));
    if (!dest->buffer) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    dest->length = src.header.bodyLen;

    if (memcpy_s(dest->buffer, src.header.bodyLen, src.body, src.header.bodyLen) != EOK) {
        free(dest->buffer);
        dest->buffer = nullptr;
        dest->length = 0;
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }

    return UBSE_OK;
}

void ubse_socket_path_set(const char* socket_path)
{
    if (socket_path == nullptr || strlen(socket_path) == 0) {
        ubseSocketPath = ubse::common::def::UBSE_UDS_SOCKET_PATH;
        return;
    }

    ubseSocketPath = std::string(socket_path);
}

static uint32_t InvokeCallInternal(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t* request_data,
                                   ubse_api_buffer_t* response_data, uint32_t totalTimeout)
{
    if (request_data == nullptr || response_data == nullptr) {
        IPC_LOG_ERROR << "Request_data or response_data pointer is null";
        return UBSE_ERROR_INVAL;
    }
    if (totalTimeout == 0) {
        IPC_LOG_ERROR << "Invalid totalTimeout=0, expect a positive value in milliseconds";
        return UBSE_ERROR_INVAL;
    }
    // 过闸: finalize进行中/完成后拒绝新业务调用
    UbseSdkCallGate gate;
    if (!gate.Entered()) {
        IPC_LOG_ERROR << "[SDK_RUNTIME] invoke rejected, module_code=" << module_code << ", op_code=" << op_code;
        return UBSE_ERR_RESOURCE_BUSY;
    }
    response_data->buffer = nullptr;
    response_data->length = 0;
    // Establish connection
    std::unique_ptr<ubse::ipc::UbseUDSClient> client;
    if (IsVsockEnabled()) {
        client = std::make_unique<ubse::ipc::UbseVsockClient>(ubseSocketPath);
    } else {
        client = std::make_unique<ubse::ipc::UbseUDSClient>(ubseSocketPath);
    }
    auto ret = client->Connect();
    if (ret != UBSE_OK) {
        IPC_LOG_ERROR << "Failed to connect, error code: " << ret;
        return ret;
    }
    // Pack request data
    UbseRequestMessage requestMessage{{module_code, op_code, request_data->length}, request_data->buffer};
    UbseResponseMessage responseMessage{};
    // Send request
    IPC_LOG_INFO << "Sending request, module_code=" << module_code << ", op_code=" << op_code;
    ret = client->Send(requestMessage, responseMessage, totalTimeout);
    if (ret != UBSE_OK) {
        IPC_LOG_ERROR << "Failed to send request, error code: " << ret;
        client->Disconnect();
        return ret;
    }
    IPC_LOG_INFO << "Sending request successfully, module_code=" << module_code << ", op_code=" << op_code;
    ret = CopyResponseBody(responseMessage, response_data);
    if (responseMessage.freeFunc && responseMessage.body) {
        responseMessage.freeFunc(responseMessage.body);
        responseMessage.body = nullptr;
    }
    if (ret != UBSE_OK) {
        IPC_LOG_ERROR << "Failed to copy response body, error code: " << ret;
        client->Disconnect();
        return ret;
    }
    client->Disconnect();
    return responseMessage.header.statusCode;
}

uint32_t ubse_invoke_call(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t* request_data,
                          ubse_api_buffer_t* response_data)
{
    return InvokeCallInternal(module_code, op_code, request_data, response_data, ubse::ipc::DEFAULT_TOTAL_TIMEOUT);
}

uint32_t ubse_invoke_call_timeout(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t* request_data,
                                  ubse_api_buffer_t* response_data, uint32_t timeout_ms)
{
    return InvokeCallInternal(module_code, op_code, request_data, response_data, timeout_ms);
}

void ubse_api_buffer_free(ubse_api_buffer_t* apiBuffer)
{
    if (apiBuffer == nullptr) {
        return;
    }
    if (apiBuffer->buffer != nullptr) {
        free(apiBuffer->buffer);
        apiBuffer->buffer = nullptr;
        apiBuffer->length = 0;
    }
}

void ubse_api_buffer_delete(ubse_api_buffer_t* apiBuffer)
{
    if (apiBuffer == nullptr) {
        return;
    }
    if (apiBuffer->buffer != nullptr) {
        delete[] apiBuffer->buffer;
        apiBuffer->buffer = nullptr;
        apiBuffer->length = 0;
    }
}

uint32_t ubse_long_link_connect(void)
{
    // 过闸: finalize进行中/完成后拒绝新业务调用
    UbseSdkCallGate gate;
    if (!gate.Entered()) {
        IPC_LOG_ERROR << "[SDK_RUNTIME] long link connect rejected";
        return UBSE_ERR_RESOURCE_BUSY;
    }
    GetLongLinkClient().SetSocketPath(ubseSocketPath);
    auto ret = GetLongLinkClient().PerSistentConnect();
    if (ret != UBSE_OK) {
        IPC_LOG_ERROR << "long link failed, err=" << ret;
        GetLongLinkClient().Stop();
        return ret;
    } else {
        IPC_LOG_INFO << "long link success.";
    }
    return ret;
}

void HandleRequest(const UbseRequestMessage& request, UbseResponseMessage& resp)
{
    IPC_LOG_INFO << "Start handling API request, moduleCode = " << request.header.moduleCode
                 << ", opCode = " << request.header.opCode << " reqId=" << request.header.clientRequestId;
    const auto key = std::make_pair(request.header.moduleCode, request.header.opCode);
    UbseClientIpcHandler handler;
    // 获取handler
    {
        std::lock_guard<std::mutex> lock(clientIpcHandlerMutex);
        auto it = clientIpcHandlerMap.find(key);
        if (it == clientIpcHandlerMap.end()) {
            IPC_LOG_ERROR << "The API interface does not exist, moduleCode= " << request.header.moduleCode
                          << ", opCode:= " << request.header.opCode << " reqId=" << request.header.clientRequestId;
            resp = {{UBSE_ERR_DAEMON_UNREACHABLE, 0, request.header.clientRequestId}, nullptr};
            return;
        }
        handler = it->second;
    }

    // 调用handler: 置TLS回调标记, 拦截回调上下文内的finalize调用(防Stop join自死锁)
    uint32_t handlerRet = UBSE_ERR_DAEMON_UNREACHABLE;
    {
        UbseSdkCallbackScope callbackScope;
        handlerRet = handler(request);
    }
    IPC_LOG_INFO << "Complete handling API request, moduleCode= " << request.header.moduleCode
                 << ", opCode= " << request.header.opCode << ", handlerRet= " << handlerRet
                 << " reqId=" << request.header.clientRequestId;
    resp = {{handlerRet, 0, request.header.clientRequestId}, nullptr};
}

void register_handler()
{
    GetLongLinkClient().RegisterClientRequestHandler(HandleRequest);
}

uint32_t ubse_register_listen_event(uint16_t moduleCode, uint16_t opCode)
{
    // 向服务端注册监听的事件(长连接服务端需要知道事件发给哪个客户端)
    auto ret = GetLongLinkClient().RegisterLongLinkNotify(moduleCode, opCode);
    if (ret != UBSE_OK) {
        IPC_LOG_ERROR << "register long link listen failed, moduleCode=" << moduleCode << ", opCode=" << opCode
                      << " ret=" << ret;
    }
    register_handler();
    return ret;
}

uint32_t ubse_shm_fault_register(ubs_mem_shm_fault_handler handler)
{
    // 过闸: finalize进行中/完成后拒绝新业务调用
    UbseSdkCallGate gate;
    if (!gate.Entered()) {
        IPC_LOG_ERROR << "[SDK_RUNTIME] shm fault register rejected";
        return UBSE_ERR_RESOURCE_BUSY;
    }
    // 向服务端注册故障监听事件
    uint32_t ret = ubse_register_listen_event(UBSE_LONG_LINK_REGISTER, UBSE_LONGLINK_FAULT_SHM);
    if (ret != UBSE_OK) {
        GetLongLinkClient().Stop();
        return ret;
    }
    std::lock_guard<std::mutex> lock(clientIpcHandlerMutex);
    UbseClientIpcHandler ipcHandler = [handler](const UbseRequestMessage& msg) -> uint32_t {
        UbseMemFault fault{};
        auto ret = DeSerializeMemFault(fault, msg.body, size_t(msg.header.bodyLen));
        if (ret != UBSE_OK) {
            IPC_LOG_ERROR << "deserialize fault info failed.";
            return ret;
        }
        IPC_LOG_INFO << "shm fault name=" << fault.memName;
        return handler(fault.memName.c_str(), fault.handleId, static_cast<ubs_mem_fault_type_t>(fault.type));
    };
    clientIpcHandlerMap[{UBSE_LONG_LINK_REGISTER, UBSE_LONGLINK_FAULT_SHM}] = ipcHandler;
    return UBSE_OK;
}

uint32_t ubse_fd_fault_register(ubs_mem_fd_fault_handler handler)
{
    // 过闸: finalize进行中/完成后拒绝新业务调用
    UbseSdkCallGate gate;
    if (!gate.Entered()) {
        IPC_LOG_ERROR << "[SDK_RUNTIME] fd fault register rejected";
        return UBSE_ERR_RESOURCE_BUSY;
    }
    uint32_t ret = ubse_register_listen_event(UBSE_LONG_LINK_REGISTER, UBSE_LONGLINK_FAULT_FD);
    if (ret != UBSE_OK) {
        GetLongLinkClient().Stop();
        return ret;
    }
    std::lock_guard<std::mutex> lock(clientIpcHandlerMutex);
    UbseClientIpcHandler ipcHandler = [handler](const UbseRequestMessage& msg) -> uint32_t {
        UbseMemFault fault{};
        auto ret = DeSerializeMemFault(fault, msg.body, size_t(msg.header.bodyLen));
        if (ret != UBSE_OK) {
            IPC_LOG_ERROR << "deserialize fd fault info failed.";
            return ret;
        }
        IPC_LOG_INFO << "fd fault name=" << fault.memName;
        return handler(fault.memName.c_str(), fault.handleId, static_cast<ubs_mem_fault_type_t>(fault.type));
    };
    clientIpcHandlerMap[{UBSE_LONG_LINK_REGISTER, UBSE_LONGLINK_FAULT_FD}] = ipcHandler;
    return UBSE_OK;
}

uint32_t ubse_numa_fault_register(ubs_mem_numa_fault_handler handler)
{
    // 过闸: finalize进行中/完成后拒绝新业务调用
    UbseSdkCallGate gate;
    if (!gate.Entered()) {
        IPC_LOG_ERROR << "[SDK_RUNTIME] numa fault register rejected";
        return UBSE_ERR_RESOURCE_BUSY;
    }
    uint32_t ret = ubse_register_listen_event(UBSE_LONG_LINK_REGISTER, UBSE_LONGLINK_FAULT_NUMA);
    if (ret != UBSE_OK) {
        GetLongLinkClient().Stop();
        return ret;
    }
    std::lock_guard<std::mutex> lock(clientIpcHandlerMutex);
    UbseClientIpcHandler ipcHandler = [handler](const UbseRequestMessage& msg) -> uint32_t {
        UbseMemFault fault{};
        auto ret = DeSerializeMemFault(fault, msg.body, size_t(msg.header.bodyLen));
        if (ret != UBSE_OK) {
            IPC_LOG_ERROR << "deserialize numa fault info failed.";
            return ret;
        }
        IPC_LOG_INFO << "numa fault name=" << fault.memName;
        return handler(fault.memName.c_str(), fault.handleId, static_cast<ubs_mem_fault_type_t>(fault.type));
    };
    clientIpcHandlerMap[{UBSE_LONG_LINK_REGISTER, UBSE_LONGLINK_FAULT_NUMA}] = ipcHandler;
    return UBSE_OK;
}