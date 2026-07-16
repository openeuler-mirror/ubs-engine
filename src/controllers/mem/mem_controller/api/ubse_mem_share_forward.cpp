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

#include "ubse_mem_share_forward.h"

#include <functional>
#include <sstream>
#include <string>
#include <unistd.h>

#include "../message/ubse_mem_share_borrow_exportobj_simpo.h"
#include "../message/ubse_mem_share_borrow_importobj_simpo.h"
#include "../message/ubse_mem_share_borrow_req_simpo.h"
#include "../message/ubse_mem_share_attach_req_simpo.h"
#include "../message/ubse_mem_return_req_simpo.h"
#include "../message/ubse_mem_share_detach_req_simpo.h"
#include "../ubse_mem_rpc_processor.h"
#include "ubse_com_module.h"
#include "ubse_com_op_code.h"
#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller_helper.h"
#include "ubse_mem_controller_api_common.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");

using ubse::com::SendParam;
using ubse::com::UbseComModule;
using ubse::com::UbseModuleCode;
using ubse::com::UbseMemBorrowCallbackOpCode;
using ubse::context::UbseContext;
using ubse::election::Node;
using ubse::election::UbseElectionModule;
using ubse::log::FormatRetCode;
using ubse::mem::controller::message::UbseMemShareBorrowExportobjSimpo;
using ubse::mem::controller::message::UbseMemShareBorrowImportobjSimpo;
using ubse::mem::controller::message::UbseMemShareBorrowReqSimpo;
using ubse::mem::controller::message::UbseMemShareAttachReqSimpo;
using ubse::mem::controller::message::UbseMemReturnReqSimpo;
using ubse::mem::controller::message::UbseMemShareDetachReqSimpo;

namespace {

// -------------------- opcode 统一 get 函数 --------------------

constexpr uint16_t ExportObjCallbackOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW_EXPORT_OBJ_CALLBACK);
}

constexpr uint16_t ImportObjCallbackOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW_IMPORT_OBJ_CALLBACK);
}

constexpr uint16_t ExportObjToCascadeOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW_GLOBAL_MASTER_TO_EXPORT_CASCADE_MASTER);
}

constexpr uint16_t ImportObjToCascadeOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_ATTACH_GLOBAL_MASTER_TO_IMPORT_CASCADE_MASTER);
}

constexpr uint16_t DetachReqToCascadeOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_DETACH_CASCADE_MASTER_HANDLE);
}

constexpr uint16_t DeleteToCascadeOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_DELETE_CASCADE_MASTER_HANDLE);
}

constexpr uint16_t BorrowReqForwardOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW_FORWARD_CASCADE_MASTER_TO_GLOBAL_MASTER);
}

constexpr uint16_t AttachReqForwardOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_ATTACH_FORWARD_CASCADE_MASTER_TO_GLOBAL_MASTER);
}

constexpr uint16_t ReturnReqForwardOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_RETURN_FORWARD_CASCADE_MASTER_TO_GLOBAL_MASTER);
}

constexpr uint16_t ExportCallbackToGlobalOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW_EXPORT_CASCADE_MASTER_TO_GLOBAL_MASTER);
}

constexpr uint16_t ImportCallbackToGlobalOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_ATTACH_IMPORT_CASCADE_MASTER_TO_GLOBAL_MASTER);
}

constexpr uint16_t DeleteCallbackToGlobalOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_DELETE_CASCADE_MASTER_TO_GLOBAL_MASTER);
}

constexpr uint16_t DetachCallbackToGlobalOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_DETACH_IMPORT_CASCADE_MASTER_TO_GLOBAL_MASTER);
}

constexpr uint16_t DetachToPdOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_DETACH_CASCADE_MASTER_TO_PD_MASTER);
}

constexpr uint16_t DetachPdToGlobalOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_DETACH_PD_MASTER_TO_GLOBAL_MASTER);
}

// -------------------- 发送底座 --------------------

// 定点发送：目标节点已知，固定次数重试。
template <typename TSimpo, typename TPayload>
UbseResult SendWithRetry(const std::string &remoteNodeId, uint16_t opCode, const TPayload &payload,
                         const std::function<void(TSimpo *, const TPayload &)> &setter, const std::string &logTag)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule, tag=" << logTag;
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(remoteNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW), opCode);
    Ref<TSimpo> ptr = new (std::nothrow) TSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new simpo, tag=" << logTag;
        return UBSE_ERROR_NULLPTR;
    }
    setter(ptr.Get(), payload);
    UbseBaseMessagePtr rspPtr = new (std::nothrow) UbseMemCallbackMessage();
    if (rspPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new response, tag=" << logTag;
        return UBSE_ERROR_NULLPTR;
    }
    UbseResult ret = UBSE_OK;
    for (uint32_t i = 0; i < SEND_RETRY_TIMES; i++) {
        ret = comModule->RpcSend(sendParam, ptr, rspPtr);
        if (ret == UBSE_OK) {
            UBSE_LOG_INFO << "Success to send " << logTag << ", remoteNodeId=" << sendParam.GetRemoteId();
            return UBSE_OK;
        }
        UBSE_LOG_ERROR << "Failed to send " << logTag << ", " << FormatRetCode(ret);
        sleep(SEND_RETRY_DURATION);
    }
    return ret;
}

// 上报本地主：目标为本地组内主，每次重试动态解析（agent 上报专用）。
template <typename TSimpo, typename TPayload>
UbseResult SendToLocalMasterWithRetry(const TPayload &payload,
                                      const std::function<void(TSimpo *, const TPayload &)> &setter, uint16_t opCode,
                                      const std::string &logTag)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule, tag=" << logTag;
        return UBSE_ERROR_NULLPTR;
    }
    Ref<TSimpo> ptr = new (std::nothrow) TSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new simpo, tag=" << logTag;
        return UBSE_ERROR_NULLPTR;
    }
    setter(ptr.Get(), payload);
    UbseBaseMessagePtr rspPtr = new (std::nothrow) UbseMemCallbackMessage();
    if (rspPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new response, tag=" << logTag;
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam("", static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW), opCode);
    const uint32_t maxRetryTimes = GetWaitTimeOut() / SEND_RETRY_DURATION;
    UbseResult ret = UBSE_ERROR;
    uint32_t retryCount = 0;
    auto electionModule = UbseContext::GetInstance().GetModule<UbseElectionModule>();
    if (electionModule == nullptr) {
        UBSE_LOG_ERROR << "[ELECTION] Getting the election module failed.";
        return UBSE_ERROR_NULLPTR;
    }
    while (ret != UBSE_OK && retryCount < maxRetryTimes) {
        Node masterNode{};
        ret = electionModule->GetLocalMasterNode(masterNode);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get local master nodeId failed, " << FormatRetCode(ret);
            retryCount++;
            sleep(SEND_RETRY_DURATION);
            continue;
        }
        sendParam.SetRemoteId(masterNode.id);
        ret = comModule->RpcSend(sendParam, ptr, rspPtr);
        if (ret == UBSE_OK) {
            break;
        }
        UBSE_LOG_ERROR << "Failed to report " << logTag << ", masterNodeId=" << sendParam.GetRemoteId()
                       << ", " << FormatRetCode(ret);
        retryCount++;
        sleep(SEND_RETRY_DURATION);
    }
    return ret;
}

// -------------------- simpo setter --------------------

void SetExportObj(UbseMemShareBorrowExportobjSimpo *simpo, const UbseMemShareBorrowExportObj &obj)
{
    simpo->SetUbseMemShareBorrowExportobj(obj);
}

void SetImportObj(UbseMemShareBorrowImportobjSimpo *simpo, const UbseMemShareBorrowImportObj &obj)
{
    simpo->SetUbseMemShareBorrowImportobj(obj);
}

void SetBorrowReq(UbseMemShareBorrowReqSimpo *simpo, const UbseMemShareBorrowReq &req)
{
    simpo->SetUbseMemShareBorrowReq(req);
}

void SetAttachReq(UbseMemShareAttachReqSimpo *simpo, const UbseMemShareAttachReq &req)
{
    simpo->SetUbseMemShareAttachReq(req);
}

void SetReturnReq(UbseMemReturnReqSimpo *simpo, const UbseMemReturnReq &req)
{
    simpo->SetUbseMemReturnReq(req);
}

void SetDetachReq(UbseMemShareDetachReqSimpo *simpo, const UbseMemShareDetachReq &req)
{
    simpo->SetUbseMemShareDetachReq(req);
}

// -------------------- 日志标识 --------------------

template <typename TObj>
std::string BuildLogTag(const std::string &action, const TObj &obj)
{
    std::stringstream ss;
    ss << action << ", name=" << obj.req.name << ", requestNodeId=" << obj.req.requestNodeId
       << ", requestId=" << obj.req.requestId;
    return ss.str();
}

template <typename TReq>
std::string BuildReqLogTag(const std::string &action, const TReq &req)
{
    std::stringstream ss;
    ss << action << ", name=" << req.name << ", requestNodeId=" << req.requestNodeId
       << ", requestId=" << req.requestId;
    return ss.str();
}

} // namespace

// ==================== 下发到执行者（agent） ====================

UbseResult ForwardExportObjToExecutor(const UbseMemShareBorrowExportObj &exportObj, const std::string &executorNodeId)
{
    return SendWithRetry<UbseMemShareBorrowExportobjSimpo, UbseMemShareBorrowExportObj>(
        executorNodeId, ExportObjCallbackOpCode(), exportObj, SetExportObj,
        BuildLogTag("exportObj to executor", exportObj));
}

UbseResult ForwardImportObjToExecutor(const UbseMemShareBorrowImportObj &importObj, const std::string &executorNodeId)
{
    return SendWithRetry<UbseMemShareBorrowImportobjSimpo, UbseMemShareBorrowImportObj>(
        executorNodeId, ImportObjCallbackOpCode(), importObj, SetImportObj,
        BuildLogTag("importObj to executor", importObj));
}

// ==================== 全局主下发到 cascade master（内部解析 cascade master） ====================

UbseResult ForwardExportObjToCascade(const UbseMemShareBorrowExportObj &exportObj)
{
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "empty exportNumaInfos, name=" << exportObj.req.name;
        return UBSE_ERROR;
    }
    std::string cascadeMasterNodeId;
    if (auto ret = UbseGetCascadeMasterNodeIdByAgentNodeId(exportObj.algoResult.exportNumaInfos[0].nodeId, cascadeMasterNodeId); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get cascade master for agent node=" << exportObj.algoResult.exportNumaInfos[0].nodeId << ", " << FormatRetCode(ret);
        return ret;
    }

    return SendWithRetry<UbseMemShareBorrowExportobjSimpo, UbseMemShareBorrowExportObj>(
        cascadeMasterNodeId, ExportObjToCascadeOpCode(), exportObj, SetExportObj,
        BuildLogTag("exportObj to cascade master", exportObj));
}

UbseResult ForwardImportObjToCascade(const UbseMemShareBorrowImportObj &importObj)
{
    std::string cascadeMasterNodeId;
    if (auto ret = UbseGetCascadeMasterNodeIdByAgentNodeId(importObj.importNodeId, cascadeMasterNodeId); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get cascade master for agent node=" << importObj.importNodeId << ", " << FormatRetCode(ret);
        return ret;
    }
    return SendWithRetry<UbseMemShareBorrowImportobjSimpo, UbseMemShareBorrowImportObj>(
        cascadeMasterNodeId, ImportObjToCascadeOpCode(), importObj, SetImportObj,
        BuildLogTag("importObj to cascade master", importObj));
}

UbseResult ForwardDetachReqToCascade(const UbseMemShareDetachReq &req)
{
    std::string cascadeMasterNodeId;
    if (auto ret = UbseGetCascadeMasterNodeIdByAgentNodeId(req.unImportNodeId, cascadeMasterNodeId); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get cascade master for agent node=" << req.unImportNodeId << ", " << FormatRetCode(ret);
        return ret;
    }
    return SendWithRetry<UbseMemShareDetachReqSimpo, UbseMemShareDetachReq>(
        cascadeMasterNodeId, DetachReqToCascadeOpCode(), req, SetDetachReq,
        BuildReqLogTag("detach to import Cascade Master", req));
}

UbseResult ForwardDeleteToCascade(const UbseMemShareBorrowExportObj &exportObj)
{
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "empty exportNumaInfos, name=" << exportObj.req.name;
        return UBSE_ERROR;
    }
    std::string cascadeMasterNodeId;
    if (auto ret = UbseGetCascadeMasterNodeIdByAgentNodeId(exportObj.algoResult.exportNumaInfos[0].nodeId, cascadeMasterNodeId); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get cascade master for agent node=" << exportObj.algoResult.exportNumaInfos[0].nodeId << ", " << FormatRetCode(ret);
        return ret;
    }
    return SendWithRetry<UbseMemShareBorrowExportobjSimpo, UbseMemShareBorrowExportObj>(
        cascadeMasterNodeId, DeleteToCascadeOpCode(), exportObj, SetExportObj,
        BuildLogTag("delete to export Cascade Master", exportObj));
}

// ==================== 执行者（agent）上报本地主 ====================

UbseResult ReportExportObjToLocalMaster(const UbseMemShareBorrowExportObj &exportObj)
{
    return SendToLocalMasterWithRetry<UbseMemShareBorrowExportobjSimpo, UbseMemShareBorrowExportObj>(
        exportObj, SetExportObj, ExportObjCallbackOpCode(), BuildLogTag("exportObj to local master", exportObj));
}

UbseResult ReportImportObjToLocalMaster(const UbseMemShareBorrowImportObj &importObj)
{
    return SendToLocalMasterWithRetry<UbseMemShareBorrowImportobjSimpo, UbseMemShareBorrowImportObj>(
        importObj, SetImportObj, ImportObjCallbackOpCode(), BuildLogTag("importObj to local master", importObj));
}

// ==================== cascade master 请求上转到全局主（内部解析全局主节点） ====================

UbseResult ForwardBorrowReqToGlobal(const UbseMemShareBorrowReq &req)
{
    std::string globalMasterNodeId;
    if (auto ret = UbseGetGlobalMasterNodeId(globalMasterNodeId); ret != UBSE_OK) {
        return ret;
    }
    return SendWithRetry<UbseMemShareBorrowReqSimpo, UbseMemShareBorrowReq>(
        globalMasterNodeId, BorrowReqForwardOpCode(), req, SetBorrowReq,
        BuildReqLogTag("borrow req to Global Master", req));
}

UbseResult ForwardAttachReqToGlobal(const UbseMemShareAttachReq &req)
{
    std::string globalMasterNodeId;
    if (auto ret = UbseGetGlobalMasterNodeId(globalMasterNodeId); ret != UBSE_OK) {
        return ret;
    }
    return SendWithRetry<UbseMemShareAttachReqSimpo, UbseMemShareAttachReq>(
        globalMasterNodeId, AttachReqForwardOpCode(), req, SetAttachReq,
        BuildReqLogTag("attach req to Global Master", req));
}

UbseResult ForwardReturnReqToGlobal(const UbseMemReturnReq &req)
{
    std::string globalMasterNodeId;
    if (auto ret = UbseGetGlobalMasterNodeId(globalMasterNodeId); ret != UBSE_OK) {
        return ret;
    }
    return SendWithRetry<UbseMemReturnReqSimpo, UbseMemReturnReq>(
        globalMasterNodeId, ReturnReqForwardOpCode(), req, SetReturnReq,
        BuildReqLogTag("return req to Global Master", req));
}

// ==================== cascade master 回调上报到全局主（内部解析全局主节点） ====================

UbseResult ForwardExportCallbackToGlobal(const UbseMemShareBorrowExportObj &exportObj)
{
    std::string globalMasterNodeId;
    if (auto ret = UbseGetGlobalMasterNodeId(globalMasterNodeId); ret != UBSE_OK) {
        return ret;
    }
    return SendWithRetry<UbseMemShareBorrowExportobjSimpo, UbseMemShareBorrowExportObj>(
        globalMasterNodeId, ExportCallbackToGlobalOpCode(), exportObj, SetExportObj,
        BuildLogTag("export callback to Global Master", exportObj));
}

UbseResult ForwardImportCallbackToGlobal(const UbseMemShareBorrowImportObj &importObj)
{
    std::string globalMasterNodeId;
    if (auto ret = UbseGetGlobalMasterNodeId(globalMasterNodeId); ret != UBSE_OK) {
        return ret;
    }
    return SendWithRetry<UbseMemShareBorrowImportobjSimpo, UbseMemShareBorrowImportObj>(
        globalMasterNodeId, ImportCallbackToGlobalOpCode(), importObj, SetImportObj,
        BuildLogTag("import callback to Global Master", importObj));
}

UbseResult ForwardDeleteCallbackToGlobal(const UbseMemShareBorrowExportObj &exportObj)
{
    std::string globalMasterNodeId;
    if (auto ret = UbseGetGlobalMasterNodeId(globalMasterNodeId); ret != UBSE_OK) {
        return ret;
    }
    return SendWithRetry<UbseMemShareBorrowExportobjSimpo, UbseMemShareBorrowExportObj>(
        globalMasterNodeId, DeleteCallbackToGlobalOpCode(), exportObj, SetExportObj,
        BuildLogTag("delete callback to Global Master", exportObj));
}

UbseResult ForwardDetachCallbackToGlobal(const UbseMemShareBorrowImportObj &importObj)
{
    std::string globalMasterNodeId;
    if (auto ret = UbseGetGlobalMasterNodeId(globalMasterNodeId); ret != UBSE_OK) {
        return ret;
    }
    return SendWithRetry<UbseMemShareBorrowImportobjSimpo, UbseMemShareBorrowImportObj>(
        globalMasterNodeId, DetachCallbackToGlobalOpCode(), importObj, SetImportObj,
        BuildLogTag("detach callback to Global Master", importObj));
}

// ==================== detach 三级链 ====================

UbseResult ForwardDetachReqToPd(const UbseMemShareDetachReq &req)
{
    std::string managerMasterNodeId;
    if (auto ret = UbseGetCurManagerMasterNodeId(managerMasterNodeId); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get manager master node id, name=" << req.name << ", requestId=" << req.requestId
                       << ", " << FormatRetCode(ret);
        return ret;
    }
    return SendWithRetry<UbseMemShareDetachReqSimpo, UbseMemShareDetachReq>(
        managerMasterNodeId, DetachToPdOpCode(), req, SetDetachReq, BuildReqLogTag("detach to manager Master", req));
}

UbseResult ForwardDetachReqToGlobal(const UbseMemShareDetachReq &req)
{
    std::string globalMasterNodeId;
    if (auto ret = UbseGetGlobalMasterNodeId(globalMasterNodeId); ret != UBSE_OK) {
        return ret;
    }
    return SendWithRetry<UbseMemShareDetachReqSimpo, UbseMemShareDetachReq>(
        globalMasterNodeId, DetachPdToGlobalOpCode(), req, SetDetachReq,
        BuildReqLogTag("detach to Global Master", req));
}

} // namespace ubse::mem::controller
