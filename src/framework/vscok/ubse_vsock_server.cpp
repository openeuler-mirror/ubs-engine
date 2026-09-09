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

#include "ubse_vsock_server.h"

#include <utility>

#include "ubse_logger.h"
#include "src/include/ubse_error.h"

UBSE_DEFINE_THIS_MODULE("ubse");

namespace ubse::vsock {
using namespace ubse::log;

UbseVsockServer::UbseVsockServer(const UbseVsockConfig& config)
    : transport_(std::make_unique<UbseVsockTransport>(config))
{
    // 注入请求回调:传输层解码出完整请求后回调服务层路由
    transport_->RegisterRequestHandler([this](const UbseRequestMessage& request, const UbseRequestContext& context) {
        HandleRequest(request, context);
    });
}

UbseVsockServer::~UbseVsockServer()
{
    Stop();
}

uint32_t UbseVsockServer::Start()
{
    // transport_ 在构造函数初始化列表中创建,生命周期内保证非空
    return transport_->Start();
}

void UbseVsockServer::Stop()
{
    transport_->Stop();
}

uint32_t UbseVsockServer::RegisterHandler(uint16_t moduleCode, uint16_t opCode, UbseVsockHandler handler)
{
    if (handler == nullptr) {
        UBSE_LOG_ERROR << "Register vsock handler failed, handler is null, moduleCode=" << moduleCode
                       << ", opCode=" << opCode;
        return UBSE_ERR_INVALID_ARG;
    }
    std::lock_guard<std::mutex> lock(handlersMutex_);
    auto key = std::make_pair(moduleCode, opCode);
    if (handlerMap_.find(key) != handlerMap_.end()) {
        UBSE_LOG_ERROR << "Vsock handler already registered, moduleCode=" << moduleCode << ", opCode=" << opCode;
        return UBSE_ERR_EXISTED;
    }
    handlerMap_[key] = std::move(handler);
    UBSE_LOG_INFO << "Vsock handler registered, moduleCode=" << moduleCode << ", opCode=" << opCode;
    return UBSE_OK;
}

uint32_t UbseVsockServer::SendResponse(uint32_t statusCode, uint64_t requestId, const UbseIpcMessage& response)
{
    UbseResponseMessage resp = {};
    resp.header.statusCode = statusCode;
    resp.header.bodyLen = response.length;
    resp.body = response.buffer;
    // 所有权契约见 UbseVsockTransport::SendResponse 注释:传输层拷贝 body,不接管所有权,
    // freeFunc 不会被调用;response.buffer 仍归 handler 侧所有并自行管理
    resp.freeFunc = nullptr;
    return transport_->SendResponse(requestId, resp);
}

UbseVsockConnState UbseVsockServer::GetConnectionStatus() const
{
    return transport_->GetConnectionStatus();
}

uint32_t UbseVsockServer::SendEmptyResponse(uint32_t statusCode, uint64_t requestId)
{
    UbseIpcMessage emptyResponse = {};
    emptyResponse.buffer = nullptr;
    emptyResponse.length = 0;
    return SendResponse(statusCode, requestId, emptyResponse);
}

void UbseVsockServer::HandleRequest(const UbseRequestMessage& request, const UbseRequestContext& context)
{
    // 查找 handler 时加锁,调用在锁外执行(支持 handler 内动态注册)
    UbseVsockHandler handler = nullptr;
    {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        auto it = handlerMap_.find(std::make_pair(request.header.moduleCode, request.header.opCode));
        if (it != handlerMap_.end()) {
            handler = it->second;
        }
    }
    if (handler == nullptr) {
        // 未注册的 moduleCode+opCode:回复错误响应
        UBSE_LOG_WARN << "No vsock handler for moduleCode=" << request.header.moduleCode
                      << ", opCode=" << request.header.opCode;
        (void)SendEmptyResponse(UBSE_ERR_NOT_EXIST, context.requestId);
        return;
    }
    UbseIpcMessage message = {};
    message.buffer = request.body;
    message.length = request.header.bodyLen;
    uint32_t ret = UBSE_OK;
    try {
        ret = handler(message, context);
    } catch (const std::exception& e) {
        // 本函数运行在线程池线程,handler 异常若逃逸线程函数将触发 std::terminate
        // 对齐 IPC 框架惯例:记录日志并回错误响应,保证进程存活
        UBSE_LOG_WARN << "Vsock handler threw exception, moduleCode=" << request.header.moduleCode
                      << ", opCode=" << request.header.opCode << ", request_id=" << context.requestId
                      << ", what=" << e.what();
        // handler 业务异常与传输层连接状态无关,回通用内部错误,避免对端误解为连接断开而触发重连
        (void)SendEmptyResponse(UBSE_ERR_INTERNAL, context.requestId);
        return;
    } catch (...) {
        // 非 std::exception 异常(如 throw 42)同样不能逃逸线程函数,兜底保证进程存活
        UBSE_LOG_WARN << "Vsock handler threw non-standard exception, moduleCode=" << request.header.moduleCode
                      << ", opCode=" << request.header.opCode << ", request_id=" << context.requestId;
        (void)SendEmptyResponse(UBSE_ERR_INTERNAL, context.requestId);
        return;
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Vsock handler returned error, moduleCode=" << request.header.moduleCode
                      << ", opCode=" << request.header.opCode << ", ret=" << ret;
        // 对齐 IPC 框架:handler 返回非 UBSE_OK 时框架兜底回错误响应,
        // 保证自 IPC 迁移的业务模块不自行应答时对端也能收到响应;
        // 若 handler 已自行应答,此处因 requestId 映射已清而快速失败,无副作用
        uint32_t sendRet = SendEmptyResponse(ret, context.requestId);
        if (sendRet != UBSE_OK) {
            UBSE_LOG_ERROR << "Vsock server send error response failed, moduleCode=" << request.header.moduleCode
                           << ", opCode=" << request.header.opCode << ", request_id=" << context.requestId;
        }
    }
}
} // namespace ubse::vsock
