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

#include "ubse_ssu_rpc_processor.h"

#include <string>
#include "debt/ubse_ssu_debt_ledger.h"
#include "framework/misc/ubse_future_mgr.h"
#include "message/ubse_ssu_alloc_msg.h"
#include "message/ubse_ssu_attach_detach_verify_msg.h"
#include "message/ubse_ssu_free_msg.h"
#include "message/ubse_ssu_perm_msg.h"
#include "message/ubse_ssu_query_verify_msg.h"
#include "message/ubse_ssu_status_update_msg.h"
#include "message/ubse_ssu_sync_resp_msg.h"
#include "trace_context.h"
#include "ubse_com.h"
#include "ubse_com_op_code.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_ssu_service_imp.h"
#include "ubse_ssu_utils.h"

namespace ubse::ssu::controller {

using namespace ubse::log;
using namespace ubse::com;
using namespace ubse::misc::future;
using namespace ubse::ssu::message;
using namespace ubse::plugin::service::ssu;
using namespace ubse::task_executor;
using namespace ubse::ssu::service;
using namespace ubse::ssu::debt;

UBSE_DEFINE_THIS_MODULE("ubse");

// 向agent端发送SSU分配响应
static void SendSsuAllocRespToAgent(const std::string &agentNodeId, const UbseSsuAllocResp &resp)
{
    auto endpoint = UbseRpcEndpointFactory::GetRpcEndpoint(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                                           static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_ALLOC_RESP));
    if (endpoint == nullptr) {
        UBSE_LOG_ERROR << "SendSsuAllocRespToAgent: get ssu alloc resp endpoint failed, requestId=" << resp.requestId;
        return;
    }
    UbseSsuAllocRespMsg respMsg(resp);
    UbseSsuSyncRespMsg ackMsg;
    auto ret = endpoint->UbseRpcSend(agentNodeId, respMsg, ackMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SendSsuAllocRespToAgent failed, " << FormatRetCode(ret) << ", requestId=" << resp.requestId;
    }
}

// 构建SSU分配失败响应
static UbseSsuAllocResp BuildErrorResp(const std::string &requestId, uint32_t errorCode)
{
    UbseSsuAllocResp resp;
    resp.requestId = requestId;
    resp.errorCode = errorCode;
    resp.state = UbseSsuNsState::IDLE;
    return resp;
}

// 为同步RPC构造回复，防止sender端框架层阻塞等待
static void SetReplySyncResp(std::unique_ptr<UbseRpcMessage> &resp, uint32_t errorCode = UBSE_OK)
{
    auto respMsg = std::make_unique<ubse::ssu::message::UbseSsuSyncRespMsg>(errorCode);
    resp = std::move(respMsg);
}

// handler: master端处理来自agent端的SSU分配请求
static void HandleAllocReqReceiver(const uint8_t *reqData, uint32_t reqSize, std::unique_ptr<UbseRpcMessage> &resp)
{
    UbseSsuAllocReqMsg request;
    if (request.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize alloc req, reqSize=" << reqSize;
        SetReplySyncResp(resp, UBSE_ERROR);
        return;
    }

    std::string traceId = TraceContext::GetTraceId();
    auto allocRpcReq = request.GetAllocRequest();
    std::string requestId = allocRpcReq.requestId;
    std::string requestNodeId = allocRpcReq.requestNodeId;
    auto executor = utils::GetSsuExecutor();
    if (executor == nullptr) {
        auto respData = BuildErrorResp(requestId, UBSE_ERROR);
        SendSsuAllocRespToAgent(requestNodeId, respData);
        UBSE_LOG_ERROR << "Get ubseSsuController executor failed";
        SetReplySyncResp(resp, UBSE_ERROR);
        return;
    }

    UbseSsuLedgerEntry entry;
    entry.name = allocRpcReq.allocReq.name;
    entry.allocReq = allocRpcReq.allocReq;
    entry.state = UbseSsuNsState::CREATING;
    if (!UbseSsuDebtLedger::GetInstance().Put(entry.name, std::make_shared<const UbseSsuLedgerEntry>(entry))) {
        UBSE_LOG_ERROR << "AllocReq: ledger entry already exists, reject duplicate alloc: name="
                       << allocRpcReq.allocReq.name;
        UbseSsuAllocResp respData = BuildErrorResp(requestId, UBSE_ERR_EXISTED);
        SendSsuAllocRespToAgent(requestNodeId, respData);
        SetReplySyncResp(resp, UBSE_ERR_EXISTED);
        return;
    }

    executor->Execute([allocRpcReq = std::move(allocRpcReq), requestId = std::move(requestId),
                       requestNodeId = std::move(requestNodeId), traceId = std::move(traceId)]() {
        TraceContext::SetTraceId(traceId);
        auto &controller = UbseSsuServiceImp::GetInstance();
        UbseSsuAllocResult result;
        auto ret = controller.AllocSpace(allocRpcReq.allocReq, allocRpcReq.identityInfo, result);
        UbseSsuAllocResp respData;
        if (ret == UBSE_OK) {
            if (!UbseSsuDebtLedger::GetInstance().Modify(allocRpcReq.allocReq.name, [&result](UbseSsuLedgerEntry &e) {
                    e.state = UbseSsuNsState::CREATED;
                    e.allocResult = result;
                })) {
                UBSE_LOG_ERROR << "AllocReq: ledger entry not found after alloc success: name="
                               << allocRpcReq.allocReq.name;
            }
            respData.requestId = requestId;
            respData.errorCode = ret;
            respData.state = UbseSsuNsState::CREATED;
            respData.allocResult = result;
        } else {
            UBSE_LOG_ERROR << "AllocSpace failed, " << FormatRetCode(ret) << ", requestId=" << requestId;
            if (!UbseSsuDebtLedger::GetInstance().Remove(allocRpcReq.allocReq.name)) {
                UBSE_LOG_ERROR << "AllocReq: ledger entry not found after alloc failed: name="
                               << allocRpcReq.allocReq.name;
            }
            respData = BuildErrorResp(requestId, ret);
        }
        SendSsuAllocRespToAgent(requestNodeId, respData);
        TraceContext::Clear();
    });
    SetReplySyncResp(resp);
}

// handler: agent端处理SSU分配响应
static void HandleAllocRespReceiver(const uint8_t *reqData, uint32_t reqSize, std::unique_ptr<UbseRpcMessage> &resp)
{
    UbseSsuAllocRespMsg response;
    if (response.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize alloc resp";
        SetReplySyncResp(resp, UBSE_ERROR);
        return;
    }
    auto respData = response.GetSsuAllocResp();
    UBSE_LOG_INFO << "Received ssu alloc response, requestId=" << respData.requestId
                  << ", state=" << static_cast<int>(respData.state) << ", errorCode=" << respData.errorCode;
    if (!UbseFutureMgr::SetResult(respData.requestId, respData)) {
        UBSE_LOG_ERROR << "Can not find requestId[" << respData.requestId << "] for ssu alloc response";
    }

    SetReplySyncResp(resp);
}

// 校验master端账本状态转换是否合法：
// - CREATING -> CREATED: master alloc成功
// - CREATED -> ATTACHING: agent开始attach
// - ATTACHING -> ATTACHED: agent attach成功
// - ATTACHING -> CREATED: agent attach失败回退
// - CREATED -> ATTACHED: 容忍ATTACHING通知丢失的兜底转换
// - ATTACHED -> CREATED: agent detach成功
static bool IsStatusTransitionValid(UbseSsuNsState from, UbseSsuNsState to)
{
    if (from == UbseSsuNsState::CREATING && to == UbseSsuNsState::CREATED) {
        return true;
    }
    if (from == UbseSsuNsState::CREATED && (to == UbseSsuNsState::ATTACHING || to == UbseSsuNsState::ATTACHED)) {
        return true;
    }
    if (from == UbseSsuNsState::ATTACHING && (to == UbseSsuNsState::ATTACHED || to == UbseSsuNsState::CREATED)) {
        return true;
    }
    return from == UbseSsuNsState::ATTACHED && to == UbseSsuNsState::CREATED;
}

// handler: master端处理来自agent端的SSU状态更新请求，更新账本
static void HandleStatusReceiver(const uint8_t *reqData, uint32_t reqSize, std::unique_ptr<UbseRpcMessage> &resp)
{
    UbseSsuStatusReqMsg request;
    if (request.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize status update, reqSize=" << reqSize;
        SetReplySyncResp(resp, UBSE_ERROR);
        return;
    }
    auto statusReq = request.GetStatusUpdateReq();
    bool modified = false;
    UbseSsuNsState fromState;
    UBSE_LOG_INFO << "Received ssu status update, requestId=" << statusReq.requestName
                  << ", state=" << static_cast<int>(statusReq.state);
    if (!UbseSsuDebtLedger::GetInstance().Modify(statusReq.requestName, [&statusReq, &modified,
                                                                         &fromState](UbseSsuLedgerEntry &e) {
            fromState = e.state;
            if (!IsStatusTransitionValid(e.state, statusReq.state)) {
                UBSE_LOG_WARN << "StatusUpdate: invalid state transition, name=" << statusReq.requestName
                              << ", from=" << static_cast<int>(e.state) << ", to=" << static_cast<int>(statusReq.state);
                return;
            }
            e.state = statusReq.state;
            modified = true;
        })) {
        UBSE_LOG_WARN << "StatusUpdate: ledger entry not found: name=" << statusReq.requestName;
        SetReplySyncResp(resp, UBSE_ERROR);
        return;
    }
    if (!modified) {
        UBSE_LOG_WARN << "StatusUpdate: invalid state transition, name=" << statusReq.requestName
                      << ", from=" << static_cast<int>(fromState) << " to=" << static_cast<int>(statusReq.state);
        SetReplySyncResp(resp, UBSE_ERROR);
        return;
    }
    SetReplySyncResp(resp);
}

// 构建SSU释放成功响应
static UbseSsuFreeResp BuildFreeResp(const std::string &requestId, uint32_t errorCode)
{
    UbseSsuFreeResp resp;
    resp.requestId = requestId;
    resp.errorCode = errorCode;
    return resp;
}

// 发送SSU释放响应
static void SendSsuFreeRespToAgent(const std::string &agentNodeId, const UbseSsuFreeResp &resp)
{
    auto endpoint = UbseRpcEndpointFactory::GetRpcEndpoint(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                                           static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_FREE_RESP));
    if (endpoint == nullptr) {
        UBSE_LOG_ERROR << "SendSsuFreeRespToAgent: get ssu free resp endpoint failed, requestId=" << resp.requestId;
        return;
    }
    UbseSsuFreeRespMsg respMsg(resp);
    UbseSsuSyncRespMsg ackMsg;
    auto ret = endpoint->UbseRpcSend(agentNodeId, respMsg, ackMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SendSsuFreeRespToAgent failed, " << FormatRetCode(ret) << ", requestId=" << resp.requestId;
    }
}

// handler: master端处理来自agent端的SSU释放请求
static void HandleFreeReqReceiver(const uint8_t *reqData, uint32_t reqSize, std::unique_ptr<UbseRpcMessage> &resp)
{
    UbseSsuFreeReqMsg request;
    if (request.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize free req, reqSize=" << reqSize;
        SetReplySyncResp(resp, UBSE_ERROR);
        return;
    }

    std::string traceId = TraceContext::GetTraceId();
    auto freeRpcReq = request.GetSsuFreeRequest();
    auto executor = utils::GetSsuExecutor();
    if (executor == nullptr) {
        SendSsuFreeRespToAgent(freeRpcReq.requestNodeId, BuildFreeResp(freeRpcReq.requestId, UBSE_ERROR));
        UBSE_LOG_ERROR << "Get ubseSsuController executor failed";
        SetReplySyncResp(resp, UBSE_ERROR);
        return;
    }

    executor->Execute([freeRpcReq = std::move(freeRpcReq), traceId = std::move(traceId)]() {
        TraceContext::SetTraceId(traceId);
        auto &controller = UbseSsuServiceImp::GetInstance();
        auto ret = controller.FreeSpace(freeRpcReq.name, freeRpcReq.identityInfo);
        if (ret != UBSE_OK) {
            SendSsuFreeRespToAgent(freeRpcReq.requestNodeId, BuildFreeResp(freeRpcReq.requestId, ret));
            UBSE_LOG_ERROR << "FreeSpace failed, " << FormatRetCode(ret) << ", requestId=" << freeRpcReq.requestId;
            TraceContext::Clear();
            return;
        }
        // 释放成功后从账本中移除对应entry
        if (!UbseSsuDebtLedger::GetInstance().Remove(freeRpcReq.name)) {
            UBSE_LOG_ERROR << "FreeReq: ledger entry not found after free success: name=" << freeRpcReq.name;
        }
        SendSsuFreeRespToAgent(freeRpcReq.requestNodeId, BuildFreeResp(freeRpcReq.requestId, ret));
        TraceContext::Clear();
    });
    SetReplySyncResp(resp);
}

// handler: agent端处理SSU释放响应
static void HandleFreeRespReceiver(const uint8_t *reqData, uint32_t reqSize, std::unique_ptr<UbseRpcMessage> &resp)
{
    UbseSsuFreeRespMsg response;
    if (response.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize free resp";
        SetReplySyncResp(resp, UBSE_ERROR);
        return;
    }
    auto respData = response.GetSsuFreeResponse();
    UBSE_LOG_INFO << "Received ssu free response, requestId=" << respData.requestId
                  << ", errorCode=" << respData.errorCode;
    // 释放agent端的future wait
    if (!UbseFutureMgr::SetResult(respData.requestId, respData)) {
        UBSE_LOG_ERROR << "Can not find requestId[" << respData.requestId << "] for ssu free response";
    }

    SetReplySyncResp(resp);
}

// handler: master端处理来自agent端的attach验证请求：验证identity并返回构造AttachDevNameSpace所需字段。
// 查询耗时较短（纯内存账本+设备缓存读取），直接在handler线程同步执行，通过sync resp直接返回nsVerifyList，
// agent端无需future等待
static void HandleAttachDetachVerifyReqReceiver(const uint8_t *reqData, uint32_t reqSize,
                                                std::unique_ptr<UbseRpcMessage> &resp)
{
    UbseSsuAttachDetachVerifyReqMsg request;
    if (request.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize attach verify req, reqSize=" << reqSize;
        UbseSsuAttachDetachVerifyResp errResp;
        errResp.errorCode = UBSE_ERROR;
        resp = std::make_unique<UbseSsuAttachDetachVerifyRespMsg>(errResp);
        return;
    }

    auto verifyReq = request.GetAttachDetachVerifyReq();
    UBSE_LOG_INFO << "Received attach verify req, requestId=" << verifyReq.requestId << ", name=" << verifyReq.name
                  << ", requestNodeId=" << verifyReq.requestNodeId;

    UbseSsuAttachDetachVerifyResp respData;
    respData.requestId = verifyReq.requestId;
    std::vector<UbseSsuNsVerifyInfo> nsVerifyList;
    auto ret = UbseSsuServiceImp::GetInstance().VerifyAttachDetachIdentity(verifyReq.name, verifyReq.identityInfo,
                                                                           nsVerifyList);
    respData.errorCode = ret;
    if (ret == UBSE_OK) {
        respData.nsVerifyList = std::move(nsVerifyList);
        // agent无本地账本，一并返回namespace列表供agent端attach/detach使用
        auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(verifyReq.name);
        if (entryPtr != nullptr) {
            respData.nameSpaceList = entryPtr->allocResult.nameSpaceList;
        }
    } else {
        UBSE_LOG_ERROR << "VerifyAttachDetachIdentity failed, " << FormatRetCode(ret)
                       << ", requestId=" << verifyReq.requestId << ", name=" << verifyReq.name;
    }
    resp = std::make_unique<UbseSsuAttachDetachVerifyRespMsg>(respData);
}

// 构建SSU访问权限响应
static UbseSsuPermResp BuildPermResp(const std::string &requestId, uint32_t errorCode)
{
    UbseSsuPermResp resp;
    resp.requestId = requestId;
    resp.errorCode = errorCode;
    return resp;
}

// 发送SSU访问权限响应（添加/移除共用，通过respOpCode区分）
static void SendSsuPermRespToAgent(const std::string &agentNodeId, const UbseSsuPermResp &resp, uint16_t respOpCode)
{
    auto endpoint = UbseRpcEndpointFactory::GetRpcEndpoint(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU), respOpCode);
    if (endpoint == nullptr) {
        UBSE_LOG_ERROR << "SendSsuPermRespToAgent: get ssu perm resp endpoint failed, requestId=" << resp.requestId;
        return;
    }
    UbseSsuPermRespMsg respMsg(resp);
    UbseSsuSyncRespMsg ackMsg;
    auto ret = endpoint->UbseRpcSend(agentNodeId, respMsg, ackMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SendSsuPermRespToAgent failed, " << FormatRetCode(ret) << ", requestId=" << resp.requestId;
    }
}

// master端处理来自agent端的SSU访问权限请求（添加/移除共用，通过isAdd区分）
static void HandlePermReqReceiver(const uint8_t *reqData, uint32_t reqSize, std::unique_ptr<UbseRpcMessage> &resp,
                                  uint16_t respOpCode, bool isAdd)
{
    UbseSsuPermReqMsg request;
    if (request.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize perm req, reqSize=" << reqSize;
        SetReplySyncResp(resp, UBSE_ERROR);
        return;
    }

    std::string traceId = TraceContext::GetTraceId();
    auto permRpcReq = request.GetSsuPermRequest();
    auto executor = utils::GetSsuExecutor();
    if (executor == nullptr) {
        SendSsuPermRespToAgent(permRpcReq.requestNodeId, BuildPermResp(permRpcReq.requestId, UBSE_ERROR), respOpCode);
        UBSE_LOG_ERROR << "Get ubseSsuController executor failed";
        SetReplySyncResp(resp, UBSE_ERROR);
        return;
    }

    executor->Execute([permRpcReq = std::move(permRpcReq), traceId = std::move(traceId), respOpCode, isAdd]() {
        TraceContext::SetTraceId(traceId);
        auto &controller = UbseSsuServiceImp::GetInstance();
        uint32_t ret;
        if (isAdd) {
            ret = controller.AddAccessPermission(permRpcReq.name, permRpcReq.nqn, permRpcReq.identityInfo);
        } else {
            ret = controller.RemoveAccessPermission(permRpcReq.name, permRpcReq.nqn, permRpcReq.identityInfo);
        }
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "PermReq failed, " << FormatRetCode(ret) << ", requestId=" << permRpcReq.requestId;
        }
        SendSsuPermRespToAgent(permRpcReq.requestNodeId, BuildPermResp(permRpcReq.requestId, ret), respOpCode);
        TraceContext::Clear();
    });
    SetReplySyncResp(resp);
}

// handler: master端处理来自agent端的SSU访问权限添加请求
static void HandleAddPermReqReceiver(const uint8_t *reqData, uint32_t reqSize, std::unique_ptr<UbseRpcMessage> &resp)
{
    HandlePermReqReceiver(reqData, reqSize, resp,
                          static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_ADD_ACCESS_PERMISSION_RESP), true);
}

// handler: master端处理来自agent端的SSU访问权限移除请求
static void HandleRemovePermReqReceiver(const uint8_t *reqData, uint32_t reqSize, std::unique_ptr<UbseRpcMessage> &resp)
{
    HandlePermReqReceiver(reqData, reqSize, resp,
                          static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_REMOVE_ACCESS_PERMISSION_RESP), false);
}

// handler: agent端处理SSU访问权限响应（添加/移除共用）
static void HandlePermRespReceiver(const uint8_t *reqData, uint32_t reqSize, std::unique_ptr<UbseRpcMessage> &resp)
{
    UbseSsuPermRespMsg response;
    if (response.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize perm resp";
        SetReplySyncResp(resp, UBSE_ERROR);
        return;
    }
    auto respData = response.GetSsuPermResponse();
    UBSE_LOG_INFO << "Received ssu perm response, requestId=" << respData.requestId
                  << ", errorCode=" << respData.errorCode;
    // 释放agent端的future wait
    if (!UbseFutureMgr::SetResult(respData.requestId, respData)) {
        UBSE_LOG_ERROR << "Can not find requestId[" << respData.requestId << "] for ssu perm response";
    }
    SetReplySyncResp(resp);
}

// ====== 查询类接口处理器 ======

// master端处理GetNsStats查询请求：查询耗时较短，直接在handler线程同步执行，
// 通过sync resp直接返回statsList，agent端无需future等待
static void HandleGetNsStatsReqReceiver(const uint8_t *reqData, uint32_t reqSize, std::unique_ptr<UbseRpcMessage> &resp)
{
    UbseSsuGetNsStatsReqMsg request;
    if (request.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize get ns stats req, reqSize=" << reqSize;
        UbseSsuGetNsStatsResp errResp;
        errResp.errorCode = UBSE_ERROR;
        resp = std::make_unique<UbseSsuGetNsStatsRespMsg>(errResp);
        return;
    }

    auto req = request.GetGetNsStatsReq();
    UBSE_LOG_INFO << "Received get ns stats req, requestId=" << req.requestId << ", name=" << req.name
                  << ", requestNodeId=" << req.requestNodeId;

    UbseSsuGetNsStatsResp respData;
    respData.requestId = req.requestId;
    respData.errorCode = UbseSsuServiceImp::GetInstance().GetNsStats(req.name, respData.statsList, req.identityInfo);
    if (respData.errorCode != UBSE_OK) {
        UBSE_LOG_ERROR << "GetNsStats failed, " << FormatRetCode(respData.errorCode) << ", requestId=" << req.requestId
                       << ", name=" << req.name;
    }
    resp = std::make_unique<UbseSsuGetNsStatsRespMsg>(respData);
}

// 注册SSU分配请求和响应处理器
uint32_t UbseSsuRpcProcessor::RegisterAllocHandlers()
{
    auto allocEndpoint = UbseRpcEndpointFactory::Build(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                                       static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_ALLOC_REQ),
                                                       HandleAllocReqReceiver);
    if (allocEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register alloc req receiver";
        return UBSE_ERROR;
    }
    auto respEndpoint = UbseRpcEndpointFactory::Build(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                                      static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_ALLOC_RESP),
                                                      HandleAllocRespReceiver);
    if (respEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register alloc resp receiver";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

// master端处理ListAllocInfo查询请求：查询耗时较短，直接在handler线程同步执行，
// 通过sync resp直接返回完整分配结果列表，agent无需本地账本
static void HandleListAllocInfoReqReceiver(const uint8_t *reqData, uint32_t reqSize,
                                           std::unique_ptr<UbseRpcMessage> &resp)
{
    UbseSsuListAllocInfoReqMsg request;
    if (request.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize list alloc info req, reqSize=" << reqSize;
        UbseSsuListAllocInfoResp errResp;
        errResp.errorCode = UBSE_ERROR;
        resp = std::make_unique<UbseSsuListAllocInfoRespMsg>(errResp);
        return;
    }

    auto req = request.GetListAllocInfoReq();
    UBSE_LOG_INFO << "Received list alloc info req, requestId=" << req.requestId
                  << ", requestNodeId=" << req.requestNodeId;

    UbseSsuListAllocInfoResp respData;
    respData.requestId = req.requestId;
    // master端直接返回完整分配结果列表，agent无本地账本
    respData.errorCode = UbseSsuServiceImp::GetInstance().ListAllocInfo(respData.results, req.identityInfo);
    if (respData.errorCode != UBSE_OK) {
        UBSE_LOG_ERROR << "ListAllocInfo failed, " << FormatRetCode(respData.errorCode)
                       << ", requestId=" << req.requestId;
    }
    resp = std::make_unique<UbseSsuListAllocInfoRespMsg>(respData);
}

// master端处理GetAllocInfoByName查询请求：查询耗时较短，直接在handler线程同步执行，
// 通过sync resp直接返回完整的分配结果，agent无需本地账本
static void HandleGetAllocInfoReqReceiver(const uint8_t *reqData, uint32_t reqSize,
                                          std::unique_ptr<UbseRpcMessage> &resp)
{
    UbseSsuGetAllocInfoReqMsg request;
    if (request.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize get alloc info req, reqSize=" << reqSize;
        UbseSsuGetAllocInfoResp errResp;
        errResp.errorCode = UBSE_ERROR;
        resp = std::make_unique<UbseSsuGetAllocInfoRespMsg>(errResp);
        return;
    }

    auto req = request.GetGetAllocInfoReq();
    UBSE_LOG_INFO << "Received get alloc info req, requestId=" << req.requestId << ", name=" << req.name
                  << ", requestNodeId=" << req.requestNodeId;

    UbseSsuGetAllocInfoResp respData;
    respData.requestId = req.requestId;
    // master端直接返回完整分配结果，agent无本地账本
    respData.errorCode =
        UbseSsuServiceImp::GetInstance().GetAllocInfoByName(req.name, respData.result, req.identityInfo);
    if (respData.errorCode != UBSE_OK) {
        UBSE_LOG_ERROR << "GetAllocInfoByName failed, " << FormatRetCode(respData.errorCode)
                       << ", requestId=" << req.requestId << ", name=" << req.name;
    }
    resp = std::make_unique<UbseSsuGetAllocInfoRespMsg>(respData);
}

// master端处理GetConnectInfo查询请求：查询耗时较短，直接在handler线程同步执行，
// 通过sync resp直接返回connectInfoList，agent端无需future等待
static void HandleGetConnectInfoReqReceiver(const uint8_t *reqData, uint32_t reqSize,
                                            std::unique_ptr<UbseRpcMessage> &resp)
{
    UbseSsuGetConnectInfoReqMsg request;
    if (request.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize get connect info req, reqSize=" << reqSize;
        UbseSsuGetConnectInfoResp errResp;
        errResp.errorCode = UBSE_ERROR;
        resp = std::make_unique<UbseSsuGetConnectInfoRespMsg>(errResp);
        return;
    }

    auto req = request.GetGetConnectInfoReq();
    UBSE_LOG_INFO << "Received get connect info req, requestId=" << req.requestId << ", name=" << req.name
                  << ", requestNodeId=" << req.requestNodeId;

    UbseSsuGetConnectInfoResp respData;
    respData.requestId = req.requestId;
    const UbseSsuVfe *vfePtr = req.hasVfe ? &req.vfe : nullptr;
    respData.errorCode =
        UbseSsuServiceImp::GetInstance().GetConnectInfo(req.name, vfePtr, respData.connectInfoList, req.identityInfo);
    if (respData.errorCode != UBSE_OK) {
        UBSE_LOG_ERROR << "GetConnectInfo failed, " << FormatRetCode(respData.errorCode)
                       << ", requestId=" << req.requestId << ", name=" << req.name;
    }
    resp = std::make_unique<UbseSsuGetConnectInfoRespMsg>(respData);
}

// 注册SSU状态更新请求处理器
uint32_t UbseSsuRpcProcessor::RegisterStatusHandler()
{
    auto statusEndpoint = UbseRpcEndpointFactory::Build(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                                        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_STATUS_UPDATE),
                                                        HandleStatusReceiver);
    if (statusEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register status update receiver";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

// 注册SSU释放请求和响应处理器
uint32_t UbseSsuRpcProcessor::RegisterFreeHandlers()
{
    auto freeEndpoint = UbseRpcEndpointFactory::Build(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                                      static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_FREE_REQ),
                                                      HandleFreeReqReceiver);
    if (freeEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register free req receiver";
        return UBSE_ERROR;
    }
    auto freeRespEndpoint = UbseRpcEndpointFactory::Build(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                                          static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_FREE_RESP),
                                                          HandleFreeRespReceiver);
    if (freeRespEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register free resp receiver";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

// 注册SSU访问权限添加请求和响应处理器
uint32_t UbseSsuRpcProcessor::RegisterAddPermHandlers()
{
    auto addPermEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_ADD_ACCESS_PERMISSION_REQ), HandleAddPermReqReceiver);
    if (addPermEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register add perm req receiver";
        return UBSE_ERROR;
    }
    auto addPermRespEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_ADD_ACCESS_PERMISSION_RESP), HandlePermRespReceiver);
    if (addPermRespEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register add perm resp receiver";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

// 注册attach验证请求处理器（master端处理agent的验证请求）
// 验证响应直接通过sync resp返回，无需注册RESP端点
uint32_t UbseSsuRpcProcessor::RegisterAttachDetachVerifyHandlers()
{
    auto reqEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_ATTACH_DETACH_VERIFY_REQ), HandleAttachDetachVerifyReqReceiver);
    if (reqEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register attach verify req receiver";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

// 注册SSU访问权限移除请求和响应处理器
uint32_t UbseSsuRpcProcessor::RegisterRemovePermHandlers()
{
    auto removePermEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_REMOVE_ACCESS_PERMISSION_REQ), HandleRemovePermReqReceiver);
    if (removePermEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register remove perm req receiver";
        return UBSE_ERROR;
    }
    auto removePermRespEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_REMOVE_ACCESS_PERMISSION_RESP), HandlePermRespReceiver);
    if (removePermRespEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register remove perm resp receiver";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

// 注册SSU查询类接口请求和响应处理器
uint32_t UbseSsuRpcProcessor::RegisterQueryHandlers()
{
    // GetNsStats（查询响应直接通过sync resp返回，无需注册RESP端点）
    auto getNsStatsReqEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_GET_NS_STATS_REQ), HandleGetNsStatsReqReceiver);
    if (getNsStatsReqEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register get ns stats req receiver";
        return UBSE_ERROR;
    }

    // ListAllocInfo（查询响应直接通过sync resp返回，无需注册RESP端点）
    auto listAllocReqEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_LIST_ALLOC_INFO_REQ), HandleListAllocInfoReqReceiver);
    if (listAllocReqEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register list alloc info req receiver";
        return UBSE_ERROR;
    }

    // GetAllocInfoByName（查询响应直接通过sync resp返回，无需注册RESP端点）
    auto getAllocReqEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_GET_ALLOC_INFO_BY_NAME_REQ), HandleGetAllocInfoReqReceiver);
    if (getAllocReqEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register get alloc info req receiver";
        return UBSE_ERROR;
    }

    // GetConnectInfo（查询响应直接通过sync resp返回，无需注册RESP端点）
    auto getConnectInfoReqEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_GET_CONNECT_INFO_REQ), HandleGetConnectInfoReqReceiver);
    if (getConnectInfoReqEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register get connect info req receiver";
        return UBSE_ERROR;
    }

    return UBSE_OK;
}

// 注册所有SSU相关的RPC处理器
uint32_t UbseSsuRpcProcessor::RegHandler()
{
    auto ret = RegisterAllocHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterAllocHandlers failed, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    ret = RegisterStatusHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterStatusHandler failed, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    ret = RegisterFreeHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterFreeHandlers failed, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    ret = RegisterAttachDetachVerifyHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterAttachDetachVerifyHandlers failed, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    ret = RegisterAddPermHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterAddPermHandlers failed, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    ret = RegisterRemovePermHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterRemovePermHandlers failed, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    ret = RegisterQueryHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterQueryHandlers failed, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    return UBSE_OK;
}
} // namespace ubse::ssu::controller
