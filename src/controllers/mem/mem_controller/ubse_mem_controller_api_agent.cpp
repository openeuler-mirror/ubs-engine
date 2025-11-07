/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#include "ubse_mem_controller_api_agent.h"
#include "ubse_com_module.h"
#include "ubse_thread_pool_module.h"
#include "message/ubse_mem_fd_borrow_req_simpo.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"
#include "message/ubse_mem_return_req_simpo.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_handler.h"
#include "ubse_mem_rpc.h"
#include "ubse_mem_utils.h"
#include "request_helper.h"
#include "request_id.h"
#include "src/controllers/include/ubse_mem_resource.h"
#include "ubse_conf.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger_inner.h"

namespace ubse::mem::controller::agent {
using namespace ubse::election;
using namespace ubse::mem::controller;
using namespace ubse::log;
using namespace ubse::com;
using namespace ubse::resource::mem;
using namespace ubse::mem_controller;
using namespace ubse::config;
using namespace ubse::mem::controller::message;
using namespace ubse::task_executor;
using namespace ubse::mem::utils;
using namespace ubse::context;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)
uint32_t Init()
{
    auto executorModule = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (executorModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get executor module.";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = executorModule->Create("MemBorrowWaitTimeOutExecutor", 64, 1000);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to create executor:MemBorrowWaitTimeOutExecutor," << FormatRetCode(ret);
        return ret;
    }
    return UbseMemOperationRespHandler::RegUbseMemOperationRespHandlerToServer();
}

std::chrono::seconds GetWaitTimeout()
{
    return WAIT_TIMEOUT;
}

static UbseResult SendRpcRequestForFdBorrow(const UbseMemFdBorrowReq &req)
{
    UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(res);
        return res;
    }
    SendParam sendParam{ masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_FD_BORROW),
        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_BORROW) };
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseMemFdBorrowReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemFdBorrowReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse request ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemFdBorrowReq(req);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse response ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    return comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
}

void DealBorrowWaitTimeOut(const std::string &name, const std::string &requestNodeId)
{
    auto memBorrowWaitTimeOutExecutor = GetExecutor("MemBorrowWaitTimeOutExecutor");
    UbseMemReturnReq returnReq;
    returnReq.name = name;
    returnReq.requestNodeId = requestNodeId;
    memBorrowWaitTimeOutExecutor->Execute([returnReq] {
        UbseMemOperationResp returnResp;
        ubse::mem::controller::agent::UbseMemReturn(returnReq, returnResp);
    });
}

uint32_t UbseMemFdBorrow(const UbseMemFdBorrowReq &req, UbseMemOperationResp &resp)
{
    // 创建请求
    auto requestId = GetRequestIdNew(req.name, req.requestNodeId);
    auto respMgr = FutureMgr::CreateInstance(requestId);
    UbseResult ret = SendRpcRequestForFdBorrow(req);
    if (ret != UBSE_OK) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = static_cast<uint16_t>(UbseMemErrorCode::ERROR_RPC_FAIL);
        UBSE_LOG_ERROR << "requestId=" << requestId << "RpcSend dispatch failed";
        return ret;
    }
    auto respFuture = respMgr->GetFuture<UbseMemOperationResp>();
    if (!respFuture.valid()) {
        UBSE_LOG_ERROR << "get future failed";
        return UBSE_ERROR;
    }
    UBSE_LOG_DEBUG << "begin wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    if (respFuture.wait_for(GetWaitTimeout()) != std::future_status::ready) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = static_cast<uint16_t>(UbseMemErrorCode::ERROR_WAIT_FAIL);
        UBSE_LOG_ERROR << "requestId=" << requestId << " borrow timeout.";
        DealBorrowWaitTimeOut(req.name, req.requestNodeId);
        return UBSE_ERROR;
    }
    UBSE_LOG_DEBUG << "End to wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    resp = respFuture.get();
    return ret;
}

static UbseResult SendRpcRequestForNumaBorrow(const UbseMemNumaBorrowReq &req)
{
    UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(res);
        return res;
    }
    SendParam sendParam{ masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_NUMA_BORROW),
        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_BORROW) };
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseMemNumaBorrowReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemNumaBorrowReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse request ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemNumaBorrowReq(req);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse response ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    return comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
}

uint32_t UbseMemNumaBorrow(const UbseMemNumaBorrowReq &req, UbseMemOperationResp &resp)
{
    // 创建请求
    auto requestId = GetRequestId(req.name, req.requestNodeId);
    auto respMgr = FutureMgr::CreateInstance(requestId);
    UbseResult ret = SendRpcRequestForNumaBorrow(req);
    if (ret != UBSE_OK) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = static_cast<uint16_t>(UbseMemErrorCode::ERROR_RPC_FAIL);
        UBSE_LOG_ERROR << "requestId=" << requestId << "RpcSend dispatch failed";
        return ret;
    }
    auto respFuture = respMgr->GetFuture<UbseMemOperationResp>();
    if (!respFuture.valid()) {
        UBSE_LOG_ERROR << "get future failed";
        return UBSE_ERROR;
    }
    UBSE_LOG_DEBUG << "begin wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    if (respFuture.wait_for(GetWaitTimeout()) != std::future_status::ready) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = static_cast<uint16_t>(UbseMemErrorCode::ERROR_WAIT_FAIL);
        UBSE_LOG_ERROR << "requestId=" << requestId << " borrow timeout.";
        DealBorrowWaitTimeOut(req.name, req.requestNodeId);
        return UBSE_ERROR;
    }
    UBSE_LOG_DEBUG << "End to wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    resp = respFuture.get();
    return ret;
}

static UbseResult SendRpcRequestForReturn(const UbseMemReturnReq &req)
{
    UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(res);
        return res;
    }
    SendParam sendParam{ masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RETURN),
        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_RETURN) };
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseMemReturnReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemReturnReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse request ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemReturnReq(req);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse response ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    return comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
}

uint32_t UbseMemReturn(const UbseMemReturnReq &req, UbseMemOperationResp &resp)
{
    // 创建请求
    auto requestId = GetRequestIdNew(req.name, req.requestNodeId);
    auto respMgr = FutureMgr::CreateInstance(requestId);
    UbseResult ret = SendRpcRequestForReturn(req);
    if (ret != UBSE_OK) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = static_cast<uint16_t>(UbseMemErrorCode::ERROR_RPC_FAIL);
        UBSE_LOG_ERROR << "requestId=" << requestId << "RpcSend dispatch failed";
        return ret;
    }
    auto respFuture = respMgr->GetFuture<UbseMemOperationResp>();
    if (!respFuture.valid()) {
        UBSE_LOG_ERROR << "get future failed";
        return UBSE_ERROR;
    }
    UBSE_LOG_DEBUG << "begin wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    if (respFuture.wait_for(GetWaitTimeout()) != std::future_status::ready) {
        resp.name = req.name;
        resp.requestNodeId = req.requestNodeId;
        resp.errorCode = static_cast<uint16_t>(UbseMemErrorCode::ERROR_WAIT_FAIL);
        UBSE_LOG_ERROR << "requestId=" << requestId << " borrow timeout.";
        auto memBorrowWaitTimeOutExecutor = GetExecutor("MemBorrowWaitTimeOutExecutor");
        memBorrowWaitTimeOutExecutor->Execute([req, resp] { SendRpcRequestForReturn(req); });
        return UBSE_ERROR;
    }
    UBSE_LOG_DEBUG << "End to wait resp, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    resp = respFuture.get();
    return ret;
}
} // namespace ubse::mem::controller::agent