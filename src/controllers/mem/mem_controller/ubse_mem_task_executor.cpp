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

#include "ubse_mem_task_executor.h"

#include "ubse_api_server_def.h"
#include "ubse_api_server_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mem_controller_def.h"
#include "ubse_mem_controller_def_serial.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_mem_rpc_processor.h"
#include "ubse_mem_util.h"
#include "ubse_serial_util.h"

namespace ubse::mem::controller {
using namespace ubse::log;
using namespace ubse::mem::def;
using namespace ubse::serial;
using namespace api::server;
using namespace ubse::mem::controller::message;

UBSE_DEFINE_THIS_MODULE("ubse");

const char* UbseMemNumaCreateOperation::GetOperationName() const
{
    return "NumaCreate";
}
uint32_t UbseMemNumaCreateOperation::Execute(UbseMemOperationResp& resp)
{
    req_.importNodeId = req_.requestNodeId;
    if (uint32_t ret = agent::UbseMemNumaBorrow(req_, resp) != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemNumaBorrow failed," << FormatRetCode(ret);
        return ret;
    }
    this->resp = resp;
    return UBSE_OK;
}
bool UbseMemNumaCreateOperation::BuildResponseData(UbseIpcMessage& responseMessage)
{
    UbseSerialization serialization{};
    serialization << resp.name << resp.requestNodeId << resp.errorCode << resp.errMsg << resp.realSize << resp.memIdList
                  << resp.remoteNumaId << resp.requestId;
    UBSE_LOG_INFO << "name: " << resp.name << "  requestNodeId: " << resp.requestNodeId
                  << "  errorCode: " << resp.errorCode << "  errMsg: " << resp.errMsg << "  realSize: " << resp.realSize
                  << "  remoteNumaId: " << resp.remoteNumaId << "  requestId: " << resp.requestId;
    if (!serialization.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize response information.";
        return false;
    }
    responseMessage.buffer = serialization.GetBuffer(true);
    responseMessage.length = static_cast<uint32_t>(serialization.GetLength());
    return true;
}

const char* UbseMemCliFdCreateDispatch::GetOperationName() const
{
    return "FdCreate";
}

uint32_t UbseMemCliFdCreateDispatch::Execute(UbseMemOperationResp& resp)
{
    req_.importNodeId = req_.requestNodeId;
    if (uint32_t ret = agent::UbseMemFdBorrow(req_, resp) != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemFdBorrow failed," << FormatRetCode(ret);
        return ret;
    }
    this->resp = resp;
    return UBSE_OK;
}

bool UbseMemCliFdCreateDispatch::BuildResponseData(UbseIpcMessage& responseMessage)
{
    UbseSerialization serialization{};
    serialization << resp.name << resp.requestNodeId << resp.errorCode << resp.errMsg << resp.realSize << resp.memIdList
                  << resp.remoteNumaId << resp.requestId;
    UBSE_LOG_INFO << "name: " << resp.name << "  requestNodeId: " << resp.requestNodeId
                  << "  errorCode: " << resp.errorCode << "  errMsg: " << resp.errMsg << "  realSize: " << resp.realSize
                  << "  memIdStr: "
                  << "  remoteNumaId: " << resp.remoteNumaId << "  requestId: " << resp.requestId;
    if (!serialization.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize response information.";
        return false;
    }
    responseMessage.buffer = serialization.GetBuffer(true);
    responseMessage.length = static_cast<uint32_t>(serialization.GetLength());
    return true;
}

const char* UbseMemShmCreateOperation::GetOperationName() const
{
    return "Create";
}

uint32_t UbseMemShmCreateOperation::Execute(UbseMemOperationResp& resp)
{
    req_.baseNodeId = req_.requestNodeId;
    return agent::UbseMemShareBorrow(req_, resp);
}

bool BuildShmResponse(const std::string& name, const std::string& nodeId, UbseIpcMessage& msg)
{
    ubse::mem::def::UbseMemShmDesc shmDesc{};
    UBSE_LOG_INFO << "UbseMemShmGet, name=" << name;
    auto ret = UbseMemShmGet(name, shmDesc, nullptr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get shm desc, name=" << name << ", " << FormatRetCode(ret);
        return false;
    }

    UbseSerialization serialization;
    if (!UbseCliShmDescSerialize(shmDesc, nodeId, serialization)) {
        UBSE_LOG_ERROR << "Failed to serialize shm desc";
        return false;
    }

    msg.buffer = serialization.GetBuffer(true);
    msg.length = serialization.GetLength();
    return true;
}

bool UbseMemShmCreateOperation::BuildResponseData(UbseIpcMessage& responseMessage)
{
    return BuildShmResponse(GetName(), GetRequestNodeId(), responseMessage);
}

const char* UbseMemShmAttachOperation::GetOperationName() const
{
    return "Attach";
}

uint32_t UbseMemShmAttachOperation::Execute(UbseMemOperationResp& resp)
{
    req_.importNodeId = req_.requestNodeId;
    return agent::UbseMemShareAttach(req_, resp);
}

bool UbseMemShmAttachOperation::BuildResponseData(UbseIpcMessage& responseMessage)
{
    return BuildShmResponse(GetName(), GetRequestNodeId(), responseMessage);
}

const char* UbseMemShmDetachOperation::GetOperationName() const
{
    return "Detach";
}

uint32_t UbseMemShmDetachOperation::Execute(UbseMemOperationResp& resp)
{
    req_.unImportNodeId = req_.requestNodeId;
    return agent::UbseMemShareDetach(req_, resp);
}

bool UbseMemShmDetachOperation::BuildResponseData(UbseIpcMessage& responseMessage)
{
    // Detach 不需要返回数据
    return true;
}

// MemoryTaskExecutor实现
void UbseMemTaskExecutor::ExecuteTask(std::shared_ptr<UbseMemOperation> operation, uint64_t requestId)
{
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServer == nullptr) {
        UBSE_LOG_ERROR << "Failed to get api server";
        return;
    }

    UbseIpcMessage responseMessage{nullptr, 0};
    UbseMemOperationResp resp{};

    const char* opName = operation->GetOperationName();
    const std::string& name = operation->GetName();

    UBSE_LOG_INFO << "UbseMemShare" << opName << ", name=" << name << ", requestId=" << requestId;

    auto ret = operation->Execute(resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to " << opName << ", name=" << name << ", requestId=" << requestId << ", "
                       << FormatRetCode(ret);
        apiServer->SendResponse(ret, requestId, responseMessage);
        return;
    }

    UBSE_LOG_INFO << "UbseMemShare" << opName << " success, name=" << name << ", requestId=" << requestId
                  << ", errCode=" << resp.errorCode << ", errMsg=" << resp.errMsg;

    uint32_t status = resp.errorCode;
    if (status != UBSE_OK) {
        apiServer->SendResponse(status, requestId, responseMessage);
        return;
    }

    // 构造返回数据
    if (!operation->BuildResponseData(responseMessage)) {
        UBSE_LOG_ERROR << "Failed to build response data";
        apiServer->SendResponse(UBSE_ERR_INTERNAL, requestId, responseMessage);
        return;
    }

    apiServer->SendResponse(status, requestId, responseMessage);
    if (responseMessage.buffer != nullptr) {
        delete[] responseMessage.buffer;
    }
}
uint32_t UbseMemTaskExecutor::PrepareRequest(const UbseRequestContext& context, UbseMemBaseBorrowReq& req)
{
    // 获取当前节点信息
    ubse::election::UbseRoleInfo currentRoleInfo{};
    auto ret = UbseGetCurrentNodeInfo(currentRoleInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }

    // 设置请求节点ID
    req.requestNodeId = currentRoleInfo.nodeId;

    // 设置UDS信息
    req.udsInfo = util::GenUdsInfo(context);

    // 转换节点ID为slot ID
    uint32_t slotId;
    ret = ubse::utils::ConvertStrToUint32(currentRoleInfo.nodeId, slotId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to convert, nodeId is " << currentRoleInfo.nodeId;
        return ret;
    }

    // 生成请求ID
    req.requestId = UbseRequestIdUtil(ubse::utils::UbseRequestType::CLI_REQUEST).GenerateRequestId(slotId);
    return UBSE_OK;
}

// 异步执行辅助函数
uint32_t UbseMemTaskExecutor::ExecuteOperationAsync(uint64_t requestId, std::unique_ptr<UbseMemOperation> operation)
{
    auto resourceExecutor = util::GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    auto sharedOp = std::shared_ptr<UbseMemOperation>(std::move(operation));
    if (!resourceExecutor->Execute(
            [sharedOp, requestId]() { UbseMemTaskExecutor::ExecuteTask(std::move(sharedOp), requestId); })) {
        UBSE_LOG_ERROR << "Submit task fail";
        return UBSE_ERROR;
    }

    return UBSE_OK;
}
} // namespace ubse::mem::controller
