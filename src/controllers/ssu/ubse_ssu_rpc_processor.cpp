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
#include "message/ubse_ssu_mount_reconcile_msg.h"
#include "message/ubse_ssu_perm_msg.h"
#include "message/ubse_ssu_query_verify_msg.h"
#include "message/ubse_ssu_status_update_msg.h"
#include "message/ubse_ssu_sync_resp_msg.h"
#include "framework/misc/ubse_logging_lock_guard.h"
#include "trace_context.h"
#include "ubse_com.h"
#include "ubse_com_op_code.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_ssu_attach_record_store.h"
#include "ubse_ssu_mount_ops.h"
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
        SetReplySyncResp(resp, UBSE_SSU_ERROR_DESERIALIZE_FAILED);
        return;
    }

    std::string traceId = TraceContext::GetTraceId();
    auto allocRpcReq = request.GetAllocRequest();
    std::string requestId = allocRpcReq.requestId;
    std::string requestNodeId = allocRpcReq.requestNodeId;
    auto executor = utils::GetSsuExecutor();
    if (executor == nullptr) {
        auto respData = BuildErrorResp(requestId, UBSE_SSU_ERROR_EXECUTOR_NULL);
        SendSsuAllocRespToAgent(requestNodeId, respData);
        UBSE_LOG_ERROR << "Get ubseSsuController executor failed";
        SetReplySyncResp(resp, UBSE_SSU_ERROR_EXECUTOR_NULL);
        return;
    }

    UbseSsuLedgerEntry entry;
    entry.name = allocRpcReq.allocReq.name;
    entry.allocReq = allocRpcReq.allocReq;
    entry.state = UbseSsuNsState::CREATING;
    if (!UbseSsuDebtLedger::GetInstance().Put(entry.name, std::make_shared<const UbseSsuLedgerEntry>(entry))) {
        UBSE_LOG_ERROR << "AllocReq: ledger entry already exists, reject duplicate alloc: name="
                       << allocRpcReq.allocReq.name;
        UbseSsuAllocResp respData = BuildErrorResp(requestId, UBSE_ERR_ALREADY_ALLOCATED);
        SendSsuAllocRespToAgent(requestNodeId, respData);
        SetReplySyncResp(resp);
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
        SetReplySyncResp(resp, UBSE_SSU_ERROR_DESERIALIZE_FAILED);
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
// - 同状态转换（ATTACHED->ATTACHED/CREATED->CREATED等）：恢复期重报幂等消化
//   （重启恢复的agent按本地记录重报ATTACHED，insert(nodeId)/erase本身幂等）
static bool IsStatusTransitionValid(UbseSsuNsState from, UbseSsuNsState to)
{
    if (from == to) {
        return true; // 同状态上报幂等消化（恢复重报场景）
    }
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

// NORMAL策略多节点共挂：对账式处理节点级上报。
// 返回true表示已消化该上报（账本被修改）；false表示拒收（nodeId缺失/状态非法/不支持的上报类型）
static bool ApplyPerNodeReconcile(UbseSsuLedgerEntry &e, const UbseSsuStatusUpdateReq &statusReq)
{
    if (statusReq.nodeId.empty()) {
        // nodeId是per-node对账的主键，缺失时拒收（agent端必传，属防御）
        UBSE_LOG_ERROR << "StatusUpdate: empty nodeId, name=" << statusReq.requestName;
        return false;
    }
    // 白名单式校验：仅ATTACHED/CREATED可接受节点级上报。
    // NORMAL空间不存在ATTACHING（无置位路径），CREATING/IDLE及未来新增状态一律拒绝
    if (e.state != UbseSsuNsState::ATTACHED && e.state != UbseSsuNsState::CREATED) {
        UBSE_LOG_WARN << "StatusUpdate: invalid state, name=" << statusReq.requestName
                      << ", state=" << static_cast<int>(e.state);
        return false;
    }
    if (statusReq.state == UbseSsuNsState::ATTACHED) {
        if (e.attachNodes.insert(statusReq.nodeId).second) {
            UBSE_LOG_INFO << "StatusUpdate: node attached, name=" << statusReq.requestName
                          << ", nodeId=" << statusReq.nodeId << ", attachedNodes=" << e.attachNodes.size();
        }
        e.state = UbseSsuNsState::ATTACHED;
    } else if (statusReq.state == UbseSsuNsState::CREATED) {
        if (e.attachNodes.erase(statusReq.nodeId) == 0) {
            // Rebuild后map为空或重复上报，仍需消化：本节点卸载属实，不能拒绝
            UBSE_LOG_WARN << "StatusUpdate: node not in attachNodes, name=" << statusReq.requestName
                          << ", nodeId=" << statusReq.nodeId;
        }
        // 仅最后一个节点卸载后才回退空间级state，其余节点仍挂载时保持ATTACHED
        if (e.attachNodes.empty()) {
            e.state = UbseSsuNsState::CREATED;
        }
    } else {
        UBSE_LOG_WARN << "StatusUpdate: unsupported state for per-node reconcile, name="
                      << statusReq.requestName << ", state=" << static_cast<int>(statusReq.state);
        return false;
    }
    return true;
}

// handler: master端处理来自agent端的SSU状态更新请求，更新账本
static void HandleStatusReceiver(const uint8_t *reqData, uint32_t reqSize, std::unique_ptr<UbseRpcMessage> &resp)
{
    UbseSsuStatusReqMsg request;
    if (request.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize status update, reqSize=" << reqSize;
        SetReplySyncResp(resp, UBSE_SSU_ERROR_DESERIALIZE_FAILED);
        return;
    }
    auto statusReq = request.GetStatusUpdateReq();
    bool modified = false;
    UbseSsuNsState fromState;
    uint32_t rejectCode = UBSE_SSU_ERROR_STATE_INVALID; // modified=false时的回复错误码
    UBSE_LOG_INFO << "Received ssu status update, requestId=" << statusReq.requestName
                  << ", state=" << static_cast<int>(statusReq.state) << ", nodeId=" << statusReq.nodeId;
    // 账本未就绪（master重启后首次采集/Rebuild未完成）：返回MASTER_NOT_READY与SPACE_NOT_FOUND区分，
    // 杜绝agent误判space已被free而错误自愈清理本地挂载
    if (!UbseSsuDebtLedger::GetInstance().IsReady()) {
        UBSE_LOG_WARN << "StatusUpdate: master ledger not ready, name=" << statusReq.requestName;
        SetReplySyncResp(resp, UBSE_SSU_ERROR_MASTER_NOT_READY);
        return;
    }
    if (!UbseSsuDebtLedger::GetInstance().Modify(statusReq.requestName, [&statusReq, &modified,
                                                                         &fromState, &rejectCode](UbseSsuLedgerEntry &e) {
            fromState = e.state;
            // 设备故障拦截：FAULTED空间的状态上报一律拒收，
            // 防止故障期间的上报覆盖FAULTED状态掩盖告警；设备恢复由collector对账回正后
            // 重报再收敛（agent侧有重试/运营对账兜底）
            if (e.state == UbseSsuNsState::FAULTED) {
                UBSE_LOG_WARN << "StatusUpdate: space faulted, report rejected, name=" << statusReq.requestName
                              << ", reporter=" << statusReq.nodeId;
                rejectCode = UBSE_SSU_ERROR_SPACE_FAULTED;
                return;
            }
            // NORMAL多节点共挂 ：对账式处理节点级上报。
            if (e.allocReq.strategy == UbseSsuAllocStrategy::NORMAL) {
                modified = ApplyPerNodeReconcile(e, statusReq);
                return;
            }
            // LINEAR/STRIPED独占挂载：保持原有空间级状态机，并按上报nodeId维护attachNodes（attacher记录，至多1个元素），
            // 供verify强校验当前节点是否为attacher
            // 重报污染防护：ATTACHED上报时attachNodes已含其他节点
            // → 拒收（恢复窗口双重挂载的最后防线），维持attacher单节点不变式
            if (statusReq.state == UbseSsuNsState::ATTACHED && !e.attachNodes.empty()
                && e.attachNodes.count(statusReq.nodeId) == 0) {
                UBSE_LOG_WARN << "StatusUpdate: attacher mismatch report rejected, name=" << statusReq.requestName
                              << ", reporter=" << statusReq.nodeId
                              << ", attacher=" << *e.attachNodes.begin();
                rejectCode = UBSE_SSU_ERROR_ATTACHER_MISMATCH;
                return;
            }
            if (!IsStatusTransitionValid(e.state, statusReq.state)) {
                UBSE_LOG_WARN << "StatusUpdate: invalid state transition, name=" << statusReq.requestName
                              << ", from=" << static_cast<int>(e.state) << ", to=" << static_cast<int>(statusReq.state);
                return;
            }
            e.state = statusReq.state;
            if (statusReq.state == UbseSsuNsState::ATTACHED) {
                e.attachNodes.insert(statusReq.nodeId);
            } else if (statusReq.state == UbseSsuNsState::CREATED) {
                e.attachNodes.erase(statusReq.nodeId);
            }
            // 同步上报的聚合块设备名（attach成功时由agent携带，非空才覆盖）。
            // 上报值会拼接为/dev/ssu/{devName}路径，需过白名单校验防路径穿越（agent为可信节点，属纵深防御）。
            // detach（state==CREATED）后聚合块设备仅 Stop 保留元数据，devName 不再清空，
            // 供 re-attach 时 Probe + Assemble 复用；attach 失败回退时 devName 为空保持不变。
            if (statusReq.state == UbseSsuNsState::ATTACHED) {
                if (!statusReq.devName.empty() && ubse::ssu::utils::IsValidDevName(statusReq.devName)) {
                    e.devName = statusReq.devName;
                } else if (!statusReq.devName.empty()) {
                    UBSE_LOG_WARN << "StatusUpdate: invalid devName ignored, name=" << statusReq.requestName;
                }
            }
            modified = true;
        })) {
        UBSE_LOG_WARN << "StatusUpdate: ledger entry not found: name=" << statusReq.requestName;
        SetReplySyncResp(resp, UBSE_SSU_ERROR_SPACE_NOT_FOUND);
        return;
    }
    if (!modified) {
        UBSE_LOG_WARN << "StatusUpdate: report rejected, name=" << statusReq.requestName
                      << ", from=" << static_cast<int>(fromState) << " to=" << static_cast<int>(statusReq.state)
                      << ", ret=" << FormatRetCode(rejectCode);
        SetReplySyncResp(resp, rejectCode);
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
        SetReplySyncResp(resp, UBSE_SSU_ERROR_DESERIALIZE_FAILED);
        return;
    }

    std::string traceId = TraceContext::GetTraceId();
    auto freeRpcReq = request.GetSsuFreeRequest();
    auto executor = utils::GetSsuExecutor();
    if (executor == nullptr) {
        SendSsuFreeRespToAgent(freeRpcReq.requestNodeId,
                               BuildFreeResp(freeRpcReq.requestId, UBSE_SSU_ERROR_EXECUTOR_NULL));
        UBSE_LOG_ERROR << "Get ubseSsuController executor failed";
        SetReplySyncResp(resp, UBSE_SSU_ERROR_EXECUTOR_NULL);
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
        SetReplySyncResp(resp, UBSE_SSU_ERROR_DESERIALIZE_FAILED);
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

// handler: master端处理来自agent端的attach/detach前置条件校验请求：校验identity、账本state、条带化参数，
// 并返回构造AttachDevNameSpace所需字段。
// 查询耗时较短（纯内存账本+设备缓存读取），直接在handler线程同步执行，通过sync resp直接返回nsVerifyList，
// agent端无需future等待
static void HandleAttachDetachVerifyReqReceiver(const uint8_t *reqData, uint32_t reqSize,
                                                std::unique_ptr<UbseRpcMessage> &resp)
{
    UbseSsuAttachDetachVerifyReqMsg request;
    if (request.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize attach verify req, reqSize=" << reqSize;
        UbseSsuAttachDetachVerifyResp errResp;
        errResp.errorCode = UBSE_SSU_ERROR_DESERIALIZE_FAILED;
        resp = std::make_unique<UbseSsuAttachDetachVerifyRespMsg>(errResp);
        return;
    }

    auto verifyReq = request.GetAttachDetachVerifyReq();
    UBSE_LOG_INFO << "Received attach verify req, requestId=" << verifyReq.requestId << ", name=" << verifyReq.name
                  << ", requestNodeId=" << verifyReq.requestNodeId;

    UbseSsuAttachDetachVerifyResp respData;
    respData.requestId = verifyReq.requestId;
    auto ret = UbseSsuServiceImp::GetInstance().VerifyAttachDetachPrecondition(verifyReq.name, verifyReq.identityInfo,
                                                                               verifyReq.requestNodeId,
                                                                               verifyReq.option, respData);
    respData.errorCode = ret;
    if (ret == UBSE_OK) {
        // agent无本地账本，一并返回namespace列表供agent端attach/detach使用
        auto entryPtr = UbseSsuDebtLedger::GetInstance().Get(verifyReq.name);
        if (entryPtr != nullptr) {
            respData.nameSpaceList = entryPtr->allocResult.nameSpaceList;
            // 聚合块设备场景下devPath恒为/dev/ssu/{devName}，devName由master账本带回（agent无本地账本）
            respData.devName = entryPtr->devName;
        }
    } else {
        UBSE_LOG_ERROR << "VerifyAttachDetachPrecondition failed, " << FormatRetCode(ret)
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
        SetReplySyncResp(resp, UBSE_SSU_ERROR_DESERIALIZE_FAILED);
        return;
    }

    std::string traceId = TraceContext::GetTraceId();
    auto permRpcReq = request.GetSsuPermRequest();
    auto executor = utils::GetSsuExecutor();
    if (executor == nullptr) {
        SendSsuPermRespToAgent(permRpcReq.requestNodeId,
                               BuildPermResp(permRpcReq.requestId, UBSE_SSU_ERROR_EXECUTOR_NULL), respOpCode);
        UBSE_LOG_ERROR << "Get ubseSsuController executor failed";
        SetReplySyncResp(resp, UBSE_SSU_ERROR_EXECUTOR_NULL);
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
        SetReplySyncResp(resp, UBSE_SSU_ERROR_DESERIALIZE_FAILED);
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
        errResp.errorCode = UBSE_SSU_ERROR_DESERIALIZE_FAILED;
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
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
    }
    auto respEndpoint = UbseRpcEndpointFactory::Build(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                                      static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_ALLOC_RESP),
                                                      HandleAllocRespReceiver);
    if (respEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register alloc resp receiver";
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
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
        errResp.errorCode = UBSE_SSU_ERROR_DESERIALIZE_FAILED;
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
        errResp.errorCode = UBSE_SSU_ERROR_DESERIALIZE_FAILED;
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
        errResp.errorCode = UBSE_SSU_ERROR_DESERIALIZE_FAILED;
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

// ===== 运营期持续对账平账 =====

// 平账单项执行：per-name resourceLock下复用既有原语（不新增执行路径）。
// REMOUNT：探测就绪→幂等收敛；未就绪→RemountLocal重挂；本地记录缺失回NEED_FULL_INFO
//         （master以账本补齐完整记录后重发，hasRecord=true时直接使用master记录）
// CLEANUP：复用SPACE_NOT_FOUND清理语义（CleanupLocalMount+删记录，幂等）
// REPORT：经SendStatusUpdate补报，master按幂等语义收敛；SPACE_NOT_FOUND→残留自愈清理
static uint32_t ExecuteReconcileItem(UbseSsuReconcileItem &item)
{
    std::string selfNodeId;
    auto nodeIdRet = election::UbseGetCurrentNodeId(selfNodeId);
    if (nodeIdRet != UBSE_OK) {
        UBSE_LOG_ERROR << "ReconcileItem: get current node id failed, " << FormatRetCode(nodeIdRet)
                       << ", name=" << item.name;
        return nodeIdRet;
    }
    auto resourceLock = ubse::utils::UbseLoggingLockGuard(item.name);
    // 本地记录快照（per-name锁内读取，与SDK attach/detach主流程互斥，无TOCTOU）
    std::optional<UbseSsuAttachRecord> localRecord;
    for (const auto &record : ubse::ssu::persist::UbseSsuAttachRecordStore::GetInstance().GetAll()) {
        if (record.name == item.name) {
            localRecord = record;
            break;
        }
    }
    switch (item.action) {
        case UbseSsuReconcileAction::REMOUNT: {
            if (!localRecord.has_value() && !item.hasRecord) {
                // 记录字段缺失：master以账本补齐完整记录后重发
                return UBSE_SSU_ERROR_NEED_FULL_INFO;
            }
            UbseSsuAttachRecord record = localRecord.has_value() ? *localRecord : item.record;
            if (!ProbeLocalMountReady(record)) {
                auto ret = RemountLocal(record);
                if (ret != UBSE_OK) {
                    UBSE_LOG_ERROR << "ReconcileItem: remount failed, name=" << item.name
                                   << ", ret=" << FormatRetCode(ret);
                    return ret; // 下轮定时器兜底重试
                }
            }
            // 就绪/重挂成功：记录收敛为ATTACHED（master账本裁决补挂，SUSPECT亦随之解除）
            record.recordState = UbseSsuAttachRecordState::ATTACHED;
            ubse::ssu::persist::UbseSsuAttachRecordStore::GetInstance().Upsert(record);
            // 执行结果经SendStatusUpdate回报
            const std::string devName = record.blockDeviceOptions.has_value() ? record.devName : "";
            auto reportRet = SendStatusUpdate(item.name, UbseSsuNsState::ATTACHED, devName, selfNodeId);
            if (reportRet != UBSE_OK) {
                UBSE_LOG_WARN << "ReconcileItem: report after remount failed, name=" << item.name
                              << ", ret=" << FormatRetCode(reportRet);
            }
            return UBSE_OK;
        }
        case UbseSsuReconcileAction::CLEANUP: {
            if (localRecord.has_value()) {
                CleanupLocalMount(*localRecord);
                ubse::ssu::persist::UbseSsuAttachRecordStore::GetInstance().Remove(item.name);
            }
            // 记录已不存在：幂等成功（可能已被主流程清理）
            auto reportRet = SendStatusUpdate(item.name, UbseSsuNsState::CREATED, "", selfNodeId);
            // 账本无该空间（已free）时master回SPACE_NOT_FOUND属预期，其余错误仅记录
            if (reportRet != UBSE_OK && reportRet != UBSE_SSU_ERROR_SPACE_NOT_FOUND) {
                UBSE_LOG_WARN << "ReconcileItem: report after cleanup failed, name=" << item.name
                              << ", ret=" << FormatRetCode(reportRet);
            }
            return UBSE_OK;
        }
        case UbseSsuReconcileAction::REPORT: {
            if (!localRecord.has_value()) {
                // 记录已丢失：转由master按REMOUNT补齐裁决
                return UBSE_SSU_ERROR_NEED_FULL_INFO;
            }
            if (localRecord->recordState != UbseSsuAttachRecordState::ATTACHED) {
                UBSE_LOG_WARN << "ReconcileItem: report skip, recordState=" << static_cast<uint32_t>(
                    localRecord->recordState) << ", name=" << item.name;
                return UBSE_OK; // PENDING在途/SUSPECT人工介入，下轮再收敛
            }
            const std::string devName = localRecord->blockDeviceOptions.has_value() ? localRecord->devName : "";
            auto ret = SendStatusUpdate(item.name, UbseSsuNsState::ATTACHED, devName, selfNodeId);
            if (ret == UBSE_SSU_ERROR_SPACE_NOT_FOUND) {
                // master账本无该空间（diff后又被free）：残留自愈清理
                UBSE_LOG_WARN << "ReconcileItem: space freed on master, cleanup local, name=" << item.name;
                CleanupLocalMount(*localRecord);
                ubse::ssu::persist::UbseSsuAttachRecordStore::GetInstance().Remove(item.name);
            } else if (ret != UBSE_OK) {
                UBSE_LOG_WARN << "ReconcileItem: report failed, name=" << item.name
                              << ", ret=" << FormatRetCode(ret); // 下轮兜底重试
            }
            return UBSE_OK;
        }
        default:
            UBSE_LOG_ERROR << "ReconcileItem: unknown action=" << static_cast<uint32_t>(item.action)
                           << ", name=" << item.name;
            return UBSE_ERR_INVALID_ARG;
    }
}

// agent端：运营对账挂载摘要请求。
// agent/standby均可响应，返回本机AttachRecordStore全量摘要（{name, recordState, devName}，
// 不含nsList明细，报文轻量），经sync resp直接返回
static void HandleMountSummaryReqReceiver(const uint8_t *reqData, uint32_t reqSize,
                                          std::unique_ptr<UbseRpcMessage> &resp)
{
    UbseSsuMountSummaryReqMsg request;
    if (request.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize mount summary req, reqSize=" << reqSize;
        UbseSsuMountSummaryResp errResp;
        errResp.errorCode = UBSE_SSU_ERROR_DESERIALIZE_FAILED;
        resp = std::make_unique<UbseSsuMountSummaryRespMsg>(errResp);
        return;
    }
    UbseSsuMountSummaryResp respData;
    for (const auto &record : ubse::ssu::persist::UbseSsuAttachRecordStore::GetInstance().GetAll()) {
        UbseSsuMountSummaryItem item;
        item.name = record.name;
        item.recordState = record.recordState;
        item.devName = record.devName;
        respData.items.emplace_back(std::move(item));
    }
    resp = std::make_unique<UbseSsuMountSummaryRespMsg>(respData);
    UBSE_LOG_INFO << "Received mount summary req, recordCount=" << respData.items.size();
}

// agent端：运营对账平账请求。逐项执行，结果经sync resp逐项返回
static void HandleReconcileReqReceiver(const uint8_t *reqData, uint32_t reqSize,
                                       std::unique_ptr<UbseRpcMessage> &resp)
{
    UbseSsuReconcileReqMsg request;
    if (request.Deserialize(reqData, reqSize) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to deserialize reconcile req, reqSize=" << reqSize;
        UbseSsuReconcileResp errResp;
        errResp.errorCode = UBSE_SSU_ERROR_DESERIALIZE_FAILED;
        resp = std::make_unique<UbseSsuReconcileRespMsg>(errResp);
        return;
    }
    auto req = request.GetReconcileReq();
    UBSE_LOG_INFO << "Received reconcile req, itemCount=" << req.items.size();
    UbseSsuReconcileResp respData;
    respData.items = req.items;
    for (auto &item : respData.items) {
        item.result = ExecuteReconcileItem(item);
    }
    resp = std::make_unique<UbseSsuReconcileRespMsg>(respData);
}

// 注册SSU状态更新请求处理器
uint32_t UbseSsuRpcProcessor::RegisterStatusHandler()
{
    auto statusEndpoint = UbseRpcEndpointFactory::Build(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                                        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_STATUS_UPDATE),
                                                        HandleStatusReceiver);
    if (statusEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register status update receiver";
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
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
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
    }
    auto freeRespEndpoint = UbseRpcEndpointFactory::Build(static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
                                                          static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_FREE_RESP),
                                                          HandleFreeRespReceiver);
    if (freeRespEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register free resp receiver";
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
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
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
    }
    auto addPermRespEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_ADD_ACCESS_PERMISSION_RESP), HandlePermRespReceiver);
    if (addPermRespEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register add perm resp receiver";
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
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
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
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
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
    }
    auto removePermRespEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_REMOVE_ACCESS_PERMISSION_RESP), HandlePermRespReceiver);
    if (removePermRespEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register remove perm resp receiver";
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
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
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
    }

    // ListAllocInfo（查询响应直接通过sync resp返回，无需注册RESP端点）
    auto listAllocReqEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_LIST_ALLOC_INFO_REQ), HandleListAllocInfoReqReceiver);
    if (listAllocReqEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register list alloc info req receiver";
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
    }

    // GetAllocInfoByName（查询响应直接通过sync resp返回，无需注册RESP端点）
    auto getAllocReqEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_GET_ALLOC_INFO_BY_NAME_REQ), HandleGetAllocInfoReqReceiver);
    if (getAllocReqEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register get alloc info req receiver";
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
    }

    // GetConnectInfo（查询响应直接通过sync resp返回，无需注册RESP端点）
    auto getConnectInfoReqEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_GET_CONNECT_INFO_REQ), HandleGetConnectInfoReqReceiver);
    if (getConnectInfoReqEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register get connect info req receiver";
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
    }

    return UBSE_OK;
}

// 注册运营对账请求处理器：
// 摘要与平账响应均经sync resp直接返回，无需注册RESP端点
uint32_t UbseSsuRpcProcessor::RegisterMountReconcileHandlers()
{
    auto summaryEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_MOUNT_SUMMARY_REQ), HandleMountSummaryReqReceiver);
    if (summaryEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register mount summary req receiver";
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
    }
    auto reconcileEndpoint = UbseRpcEndpointFactory::Build(
        static_cast<uint16_t>(UbseModuleCode::UBSE_SSU),
        static_cast<uint16_t>(UbseSsuOpCode::UBSE_SSU_RECONCILE_REQ), HandleReconcileReqReceiver);
    if (reconcileEndpoint == nullptr) {
        UBSE_LOG_ERROR << "Unable to register reconcile req receiver";
        return UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED;
    }
    return UBSE_OK;
}

// 注册所有SSU相关的RPC处理器
uint32_t UbseSsuRpcProcessor::RegHandler()
{
    auto ret = RegisterAllocHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterAllocHandlers failed, " << FormatRetCode(ret);
        return ret;
    }

    ret = RegisterStatusHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterStatusHandler failed, " << FormatRetCode(ret);
        return ret;
    }

    ret = RegisterFreeHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterFreeHandlers failed, " << FormatRetCode(ret);
        return ret;
    }

    ret = RegisterAttachDetachVerifyHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterAttachDetachVerifyHandlers failed, " << FormatRetCode(ret);
        return ret;
    }

    ret = RegisterAddPermHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterAddPermHandlers failed, " << FormatRetCode(ret);
        return ret;
    }

    ret = RegisterRemovePermHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterRemovePermHandlers failed, " << FormatRetCode(ret);
        return ret;
    }

    ret = RegisterQueryHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterQueryHandlers failed, " << FormatRetCode(ret);
        return ret;
    }

    ret = RegisterMountReconcileHandlers();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "RegisterMountReconcileHandlers failed, " << FormatRetCode(ret);
        return ret;
    }

    return UBSE_OK;
}
} // namespace ubse::ssu::controller
