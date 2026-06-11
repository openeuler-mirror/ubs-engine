/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "ubse_mem_controller_fd_api.h"

#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_logger_audit.h"
#include "ubse_mem_advice.h"
#include "ubse_mem_common_utils.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_mem_decoder_utils.h"
#include "ubse_mem_scheduler.h"
#include "ubse_mem_sign_verifier.h"
#include "ubse_mem_util.h"
#include "ubse_node_controller_util.h"
#include "../logging_lock_guard.h"
#include "../message/ubse_mem_fd_borrow_exportobj_simpo.h"
#include "../message/ubse_mem_fd_borrow_importobj_simpo.h"
#include "../message/ubse_mem_operation_resp_simpo.h"
#include "../ubse_mem_account.h"
#include "../ubse_mem_controller_api.h"
#include "../ubse_mem_controller_ledger.h"
#include "../ubse_mem_rpc_processor.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::election;
using namespace ubse::log;
using namespace ubse::mem::scheduler;
using namespace ubse::nodeController;
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::task_executor;
using namespace ubse::com;
using namespace message;
using namespace ubse::mmi;
using namespace ubse::mem::strategy;
using namespace ubse::mem::util;
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;

UbseResult AgentSendFdExportObj(const std::shared_ptr<UbseComModule>& comModule, SendParam& sendParam,
                                UbseMemFdBorrowExportobjSimpoPtr& ptr, UbseBaseMessagePtr& ubseResponsePtr,
                                const UbseMemFdBorrowExportObj& exportObj)
{
    const uint32_t maxRetryTimes = GetWaitTimeOut() / SEND_RETRY_DURATION;
    auto ret = UBSE_ERROR;
    uint32_t retryCount = 0;
    while (ret != UBSE_OK && retryCount < maxRetryTimes) {
        std::string masterId{};
        ret = UbseGetMasterNodeId(masterId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get master nodeId failed, " << FormatRetCode(ret);
            retryCount++;
            sleep(SEND_RETRY_DURATION);
            continue;
        }
        sendParam.SetRemoteId(masterId);
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
        if (ret == UBSE_OK) {
            break;
        }
        retryCount++;
        sleep(SEND_RETRY_DURATION);
        UBSE_LOG_ERROR << "Send to exportObj, name=" << exportObj.req.name
                       << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << exportObj.req.requestId
                       << ", masterNodeId=" << sendParam.GetRemoteId() << " failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult SendFdExportObj(const UbseMemFdBorrowExportObj& exportObj, const bool isMaster,
                           const std::string& nodeId = "")
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule, requestId=" << exportObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_FD_BORROW_EXPORT_OBJ_CALLBACK));
    UbseMemFdBorrowExportobjSimpoPtr ptr = new (std::nothrow) UbseMemFdBorrowExportobjSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr, requestId=" << exportObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemFdBorrowExportobj(exportObj);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr, requestId=" << exportObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    // 主节点向从履行侧发送
    UbseResult ret = UBSE_OK;
    if (isMaster) {
        for (int i = 0; i < SEND_RETRY_TIMES; i++) {
            ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
            if (ret == UBSE_OK) {
                UBSE_LOG_INFO << "Success to send exportObj, name=" << exportObj.req.name
                              << ", requestNodeId=" << exportObj.req.requestNodeId
                              << ", requestId=" << exportObj.req.requestId;
                return UBSE_OK;
            }
            UBSE_LOG_ERROR << "Failed to Send to exportObj, name=" << exportObj.req.name
                           << ", requestNodeId=" << exportObj.req.requestNodeId << ", wait to retry"
                           << ", requestId=" << exportObj.req.requestId;
            sleep(SEND_RETRY_DURATION);
        }
        UBSE_LOG_ERROR << "Failed to Send to exportObj, name=" << exportObj.req.name
                       << ", requestNodeId=" << exportObj.req.requestNodeId
                       << ", requestId=" << exportObj.req.requestId;
        return ret;
    }

    // 履行侧发回
    return AgentSendFdExportObj(comModule, sendParam, ptr, ubseResponsePtr, exportObj);
}

void ConstructFdObjs(UbseMemFdBorrowImportObj& importObj, UbseMemFdBorrowExportObj& exportObj,
                     const UbseMemFdBorrowReq& req)
{
    exportObj.algoResult = importObj.algoResult;
    exportObj.req = req;
    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    importObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
}

UbseResult DoSendFdExportObj(const UbseMemFdBorrowReq& req, UbseMemOperationResp& resp,
                             UbseMemFdBorrowImportObj importObj)
{
    UbseMemFdBorrowExportObj exportObj{};
    ConstructFdObjs(importObj, exportObj, req);
    auto exportObjKey = GenerateExportObjKey(req.name, req.importNodeId);
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    // 放入导出对象、导入对象
    UBSE_LOG_INFO << "FdExportObj and importObj stored, name=" << req.name << ", requestNodeId=" << req.requestNodeId
                  << ", requestId=" << req.requestId;

    auto& ledger = UbseMemDebtLedger::GetInstance();
    ledger.GetDebtMap<UbseMemFdBorrowExportObj>().PutResource(exportNodeId, exportObjKey, exportObj);
    ledger.GetDebtMap<UbseMemFdBorrowImportObj>().PutResource(req.importNodeId, req.name, importObj);

    auto ret = SendFdExportObj(exportObj, true, exportNodeId);
    if (ret != UBSE_OK) {
        ledger.GetDebtMap<UbseMemFdBorrowExportObj>().RemoveResource(exportNodeId, exportObjKey);
        ledger.GetDebtMap<UbseMemFdBorrowImportObj>().RemoveResource(req.importNodeId, req.name);
        exportObj.status.state = UBSE_MEM_STATE_FAILED;
        UbseMemFdExportObjStateChangeHandler(exportObj);
        BorrowFailedAdvice(ProcessType::BORROW_FAILED, req.name, "WATER_BORROW", req.size, exportNodeId,
                           req.importNodeId, ret, MemAdvice::COMM_FAILED);
        BuildOperationRespWhenFail(resp, req.name, req.requestNodeId,
                                   "Failed to Send export, export node is " + exportNodeId, UBSE_ERR_INTERNAL,
                                   MemOperationType::FD_BORROW);
        return ret;
    }
    return UBSE_OK;
}

uint32_t UbseMemFdBorrow(const UbseMemFdBorrowReq& req, UbseMemOperationResp& resp)
{
    UBSE_LOG_INFO << "Fd borrow begins, name=" << req.name << ", requestNodeId=" << req.requestNodeId
                  << ", requestId=" << req.requestId;
    auto lock = LoggingLockGuard(GenerateExportObjKey(req.name, req.importNodeId));
    resp.requestId = req.requestId;
    if (!IsMemBorrowFeatureSupported()) {
        return BuildMemFeatureNotSupportedResp(resp, req.name, req.requestNodeId, MemOperationType::FD_BORROW);
    }

    auto errCode = CheckFdResourceState(req.name, req.importNodeId);
    if (errCode != UBSE_ERR_NOT_EXIST) {
        BorrowFailedAdvice(ProcessType::BORROW_FAILED, req.name, "WATER_BORROW", req.size, "", req.importNodeId,
                           errCode, MemAdvice::RESOURCE_EXIST);
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Resource Exist.", errCode);
    }
    // 创建父对象
    UbseMemFdBorrowImportObj importObj{.req = req};
    importObj.req.trustRingData.ClearReqSignedDataMemory(); // 清除import对象里请求签名信息
    importObj.status.state = UBSE_MEM_SCHEDULING;
    // 调用算法
    uint8_t retryTimes = ALLOCATE_RETRY_TIME;
    auto ret = UBSE_OK;
    while (retryTimes--) {
        UbseNodeControllerLockMgr::ReadLock(req.importNodeId);
        ret = UbseMemFdImportObjStateChangeHandler(importObj);
        UbseNodeControllerLockMgr::ReadUnLock(req.importNodeId);
        if (ret == UBSE_SCHEDULER_ERROR_NODE_RECONCILE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
            UBSE_LOG_WARN << "retry time is " << ALLOCATE_RETRY_TIME - retryTimes << ", no node can be lend";
            continue;
        }
        break;
    }
    if (ret != UBSE_OK || importObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "[MMC] Failed to allocate, name=" << importObj.req.name
                       << ", requestNodeId=" << importObj.req.requestNodeId << ", " << FormatRetCode(ret)
                       << ", requestId=" << req.requestId;
        BorrowFailedAdvice(ProcessType::BORROW_FAILED, req.name, "WATER_BORROW", req.size, "", req.importNodeId,
                           UBSE_ERR_ALLOCATE, MemAdvice::SCHEDULE_FAILED);
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Failed to allocate", UBSE_ERR_ALLOCATE,
                                          MemOperationType::FD_BORROW);
    }
    FillImportNumaPortAndChipId(importObj.algoResult.exportNumaInfos[0].nodeId,
                                importObj.algoResult.exportNumaInfos[0].socketId, req.importNodeId,
                                importObj.algoResult.importNumaInfos);
    // 下发对象执行
    return DoSendFdExportObj(req, resp, importObj);
}

bool FindFdBorrowObjByName(const UbseMemFdPermissionReq& req, UbseMemFdBorrowImportObj& importObj)
{
    auto importObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().GetResource(
        req.requestNodeId, req.name);
    if (!importObjPtr) {
        return false;
    }
    if (importObjPtr->status.state != UBSE_MEM_IMPORT_SUCCESS) {
        return false;
    }
    importObj = *importObjPtr;
    return true;
}

UbseResult SendFdImportObjForPermission(const std::string& nodeId, const UbseMemFdBorrowImportObj& importObj)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule, requestId=" << importObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(
        nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP),
        static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_BORROW_IMPORT_OBJ_FOR_PERMISSION_CALLBACK));
    UbseMemFdBorrowImportobjSimpoPtr ptr = new (std::nothrow) UbseMemFdBorrowImportobjSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr, requestId=" << importObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemFdBorrowImportobj(importObj);
    UbseMemOperationRespSimpoPtr ubseResponsePtr = new (std::nothrow) UbseMemOperationRespSimpo();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr, requestId=" << importObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    // 主节点向从借用侧发送
    UbseResult ret = UBSE_OK;
    for (int i = 0; i < SEND_RETRY_TIMES; i++) {
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
        if (ret == UBSE_OK) {
            auto resp = ubseResponsePtr->GetUbseMemOperationResp();
            if (resp.errorCode != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to importObj for permission, name=" << importObj.req.name
                               << ", requestId=" << importObj.req.requestId;
                return UBSE_ERROR;
            }
            UBSE_LOG_INFO << "Success to importObj for permission, name=" << importObj.req.name
                          << ", requestId=" << importObj.req.requestId;
            return UBSE_OK;
        }
        UBSE_LOG_ERROR << "Failed to Send to importObj for permission, name=" << importObj.req.name
                       << ", wait to retry, requestId=" << importObj.req.requestId;
        sleep(SEND_RETRY_DURATION);
    }
    UBSE_LOG_ERROR << "Failed to Send to importObj for permission, name=" << importObj.req.name
                   << ", requestId=" << importObj.req.requestId;
    return ret;
}

void FdImportUpdatePermission(UbseMemFdBorrowImportObj& importObj)
{
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().PutResource(importObj.req.importNodeId,
                                                                                        importObj.req.name, importObj);
}

uint32_t UbseMemFdPermission(const UbseMemFdPermissionReq& req, const std::string& realRequestNodeId)
{
    auto name = req.name;
    UBSE_LOG_INFO << "Fd permission begins, name=" << req.name << ", requestId=" << req.requestId;
    auto exportKey = GenerateExportObjKey(req.name, req.requestNodeId);
    auto lock = LoggingLockGuard(exportKey);
    UbseMemFdBorrowImportObj importObj{};
    if (!IsMemBorrowFeatureSupported()) {
        return UBSE_ERR_NOT_SUPPORTED;
    }

    if (!FindFdBorrowObjByName(req, importObj)) {
        UBSE_LOG_ERROR << "name=" << req.name << " not exist successfully borrow";
        return UBSE_ERR_NOT_EXIST;
    }
    if (!CheckCommonReturnPermission(importObj.req.udsInfo, req.udsInfo, realRequestNodeId,
                                     importObj.req.importNodeId)) {
        UBSE_LOG_ERROR << "name=" << req.name << " auth failed, req username=" << req.udsInfo.username
                       << ", req uid=" << req.udsInfo.uid << ", import obj username=" << importObj.req.udsInfo.username
                       << ", import obj uid=" << importObj.req.udsInfo.uid
                       << ", realRequestNodeId=" << realRequestNodeId;
        return UBSE_ERR_AUTH_FAILED;
    }
    importObj.req.owner = req.fdOwner;
    //  下发importObj,设置属主和权限
    auto ret = SendFdImportObjForPermission(importObj.req.importNodeId, importObj);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "name=" << req.name << " set permission failed";
        return UBSE_ERR_INTERNAL;
    }
    // 更新FdImport
    FdImportUpdatePermission(importObj);
    return UBSE_OK;
}

void FdExportUpdateState(UbseMemFdBorrowExportObj& exportObj, const UbseMemState& state)
{
    exportObj.status.state = state;
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        return;
    }
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto name = exportObj.req.name;
    auto importNodeId = exportObj.req.importNodeId;
    auto exportKey = GenerateExportObjKey(name, importNodeId);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>().PutResource(exportNodeId, exportKey,
                                                                                        exportObj);
}

void EraseFdExport(const UbseMemFdBorrowExportObj& exportObj)
{
    auto name = exportObj.req.name;
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        return;
    }
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto importNodeId = exportObj.req.importNodeId;
    auto exportKey = GenerateExportObjKey(name, importNodeId);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>().RemoveResource(exportNodeId, exportKey);
}

UbseResult SendFdExport(const UbseMemFdBorrowExportObj& exportObj, const std::string& name,
                        const std::string& exportNodeId, bool isMaster)
{
    auto res = SendFdExportObj(exportObj, isMaster);
    if (res != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::EXPORT_FAILED, name, "WATER_BORROW", exportObj.req.size, exportNodeId,
                           exportObj.req.requestNodeId, res, MemAdvice::COMM_FAILED);
    }
    return res;
}

uint32_t FdExportRunningAgentCallback(UbseMemOperationResp& resp, UbseMemFdBorrowExportObj& exportObj,
                                      const std::string& name, const std::string& exportNodeId,
                                      const std::string& requestNodeId)
{
    UBSE_LOG_INFO << "Fd export running callback. name=" << name << ", requestId=" << exportObj.req.requestId;
    auto exportKey = GenerateExportObjKey(exportObj.req.name, exportObj.req.importNodeId);
    auto existingObj =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>().GetResource(exportNodeId, exportKey);
    if (existingObj && existingObj->status.state == ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS) {
        return UBSE_OK;
    }
    FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_RUNNING);
    if (auto ret = UbseMmiInterface::GetInstance().FdExportExecutor(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to export, name=" << name << ", requestNodeId=" << requestNodeId
                       << ", requestId=" << exportObj.req.requestId;
        BorrowFailedAdvice(ProcessType::BORROW_FAILED, name, "WATER_BORROW", exportObj.req.size, exportNodeId,
                           exportObj.req.importNodeId, ret, MemAdvice::OBMM_FAILED);
        exportObj.errorCode = ret;
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
        EraseFdExport(exportObj);
        // 导出失败，从节点不做存储操作，返回通知主节点。
        return SendFdExport(exportObj, name, exportNodeId, false);
    }
    UBSE_LOG_INFO << "Success to export fd, name=" << name << ", requestId=" << exportObj.req.requestId;
    UBSE_AUDIT_RUNTIME_ALLOC << name << " on Node: " << exportNodeId << " FdMemory Export "
                             << std::to_string(exportObj.req.size) << " Bytes Success";
    // 高安配置下签名并验签
    if (IsHighSafety()) {
        UbseExportSignReq trustReq{exportObj.req.trustRingData.reqSignedData, "fd"};
        trustReq.exportObmmInfo = exportObj.status.exportObmmInfo;
        trustReq.trustRingId = exportObj.req.trustRingData.trustRingId;
        if (const auto ret = UbseMemSignVerifier::SignAndVerify(trustReq, exportObj.req.trustRingData.lendSignedDatas);
            ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to sign for lend information, " << FormatRetCode(ret);
            UbseMmiInterface::GetInstance().FdExportExecutor(exportObj);
            exportObj.errorCode = ret;
            exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
            return SendFdExport(exportObj, name, exportNodeId, false);
        }
    }
    exportObj.req.trustRingData.ClearReqSignedDataMemory();
    FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    return SendFdExport(exportObj, name, exportNodeId, false);
}

uint32_t FdExportDestroyingAgentCallback(UbseMemOperationResp& resp, UbseMemFdBorrowExportObj& exportObj,
                                         const std::string& name, const std::string& exportNodeId,
                                         const std::string& requestNodeId)
{
    UBSE_LOG_INFO << "Fd export destroying callback. name=" << name << ", requestId=" << exportObj.req.requestId;
    auto exportKey = GenerateExportObjKey(name, exportObj.req.importNodeId);
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    auto existingObj =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>().GetResource(exportNodeId, exportKey);
    if (!existingObj || existingObj->status.state == UBSE_MEM_EXPORT_DESTROYED) {
        EraseFdExport(exportObj);
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
        if (auto ret = SendFdExportObj(exportObj, false); ret != UBSE_OK) {
            BorrowFailedAdvice(ProcessType::UNEXPORT_FAILED, name, "WATER_BORROW", 0, exportNodeId,
                               exportObj.req.importNodeId, ret, MemAdvice::COMM_FAILED);
            return ret;
        }
        return UBSE_OK;
    }
    FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
    if (auto ret = UbseMmiInterface::GetInstance().FdUnExportExecutor(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to unexport name=" << name << ", requestId=" << exportObj.req.requestId;
        exportObj.errorCode = ret;
        BorrowFailedAdvice(ProcessType::UNEXPORT_FAILED, name, "WATER_BORROW", 0, exportNodeId,
                           exportObj.req.importNodeId, ret, MemAdvice::COMM_FAILED);
        FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        // 返回主节点 更新
        if (ret = SendFdExportObj(exportObj, false); ret != UBSE_OK) {
            BorrowFailedAdvice(ProcessType::UNEXPORT_FAILED, name, "WATER_BORROW", 0, exportNodeId,
                               exportObj.req.importNodeId, ret, MemAdvice::COMM_FAILED);
        }
        return ret;
    }
    UBSE_LOG_INFO << "Success to unexport fd, name=" << name << ", requestId=" << exportObj.req.requestId;
    UBSE_AUDIT_RUNTIME_DEALLOC << name << " on Node: " << exportNodeId << " FdMemory UnExport "
                               << std::to_string(exportObj.req.size) << " Bytes Success";
    // 归还成功
    EraseFdExport(exportObj);
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    if (auto ret = SendFdExportObj(exportObj, false); ret != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::UNEXPORT_FAILED, name, "WATER_BORROW", 0, exportNodeId, requestNodeId, ret,
                           MemAdvice::COMM_FAILED);
        return ret;
    }
    return UBSE_OK;
}

uint32_t FdExportAgentCallback(const std::string& exportNodeId, UbseMemFdBorrowExportObj& exportObj,
                               const std::string& name)
{
    UBSE_LOG_INFO << "Fd export agent callback, name=" << name << ", state=" << exportObj.status.state
                  << ", requestId=" << exportObj.req.requestId;
    auto exportKey = GenerateExportObjKey(name, exportObj.req.importNodeId);
    auto lock = LoggingLockGuard(exportKey);
    auto requestNodeId = exportObj.req.requestNodeId;
    UbseMemOperationResp resp{
        .name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId, .requestId = exportObj.req.requestId};
    // 创建
    if (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING) {
        return FdExportRunningAgentCallback(resp, exportObj, name, exportNodeId, requestNodeId);
    }
    // 归还
    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING) {
        return FdExportDestroyingAgentCallback(resp, exportObj, name, exportNodeId, requestNodeId);
    }
    return UBSE_OK;
}

uint32_t FdExportExpectDestroyMasterCallback(UbseMemOperationResp& resp, UbseMemFdBorrowExportObj& exportObj,
                                             const std::string& exportNodeId, const std::string& name)
{
    if (!HasAgentAlreadyReported<UbseMemFdBorrowExportObj>(
            GenerateExportObjKey(exportObj.req.name, exportObj.req.importNodeId), exportNodeId,
            &UbseMemFdBorrowExportObj::isDestroyedReportReceived)) {
        UBSE_LOG_INFO << "No need to callback for export destroyed, name=" << name
                      << ", requestId=" << exportObj.req.requestId;
        return UBSE_OK;
    }
    UbseMemFdBorrowImportObj importObj{};
    auto req = exportObj.returnReq;
    std::string requestNodeId = req.requestNodeId;
    resp.requestNodeId = requestNodeId;
    resp.requestId = req.requestId;
    // 归还逻辑
    if (exportObj.status.state == UBSE_MEM_EXPORT_SUCCESS) {
        // 归还失败,后续由对账清理
        UBSE_LOG_INFO << "Export return is unsuccessful, wait to ledger, name=" << name
                      << ", requestId=" << exportObj.req.requestId;
        FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        // requestNodeId为空则当前场景为对账删除导出账本或者借用失败回滚
        if (requestNodeId.empty()) {
            return UBSE_OK;
        }
        if (auto ret = BuildOperationRespWhenFail(resp, name, requestNodeId, "Failed to unexport", exportObj.errorCode,
                                                  MemOperationType::FD_RETURN);
            ret != UBSE_OK) {
            BorrowFailedAdvice(ProcessType::RETURN_FAILED, name, "WATER_BORROW", 0, exportNodeId,
                               exportObj.req.importNodeId, ret, MemAdvice::COMM_FAILED);
            return ret;
        }
        return UBSE_OK;
    }
    // 归还成功,删除导出对象/导入对象
    UBSE_LOG_INFO << "Export return is successful, name=" << name << ", requestId=" << exportObj.req.requestId;
    EraseFdExport(exportObj);
    // 导入对象在unimport时，已经删掉。如还存在，就是删除单导出时，对账将导入账本重新加入主节点
    UbseMemFdExportObjStateChangeHandler(exportObj);
    // requestNodeId为空则当前场景为对账删除导出账本或者借用失败回滚
    if (requestNodeId.empty()) {
        return UBSE_OK;
    }
    if (auto ret = BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::FD_RETURN); ret != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::RETURN_FAILED, name, "WATER_BORROW", 0, exportNodeId,
                           exportObj.req.importNodeId, ret, MemAdvice::COMM_FAILED);
        return ret;
    }
    return UBSE_OK;
}

void FdImportUpdateState(UbseMemFdBorrowImportObj& importObj, const UbseMemState& state)
{
    importObj.status.state = state;
    auto name = importObj.req.name;
    auto importNodeId = importObj.req.importNodeId;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().PutResource(importNodeId, name, importObj);
}

void EraseFdImport(const UbseMemFdBorrowImportObj& importObj)
{
    auto name = importObj.req.name;
    auto importNodeId = importObj.req.importNodeId;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().RemoveResource(importNodeId, name);
}

uint32_t FdExportRollback(UbseMemOperationResp& resp, UbseMemFdBorrowExportObj& exportObj,
                          UbseMemFdBorrowImportObj& importObj, const std::string& name, const std::string& exportNodeId)
{
    // 回滚
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
    EraseFdImport(importObj);
    importObj.status.state = UBSE_MEM_STATE_FAILED;
    UbseMemFdImportObjStateChangeHandler(importObj); // 通知算法
    BuildOperationRespWhenFail(resp, name, exportObj.req.requestNodeId, "Failed to import", UBSE_ERR_INTERNAL);
    return SendFdExportObj(exportObj, true, exportNodeId);
}

UbseResult AgentSendFdImportObj(const std::shared_ptr<UbseComModule>& comModule, SendParam& sendParam,
                                UbseMemFdBorrowImportobjSimpoPtr& ptr, UbseBaseMessagePtr& ubseResponsePtr,
                                const UbseMemFdBorrowImportObj& importObj)
{
    const uint32_t maxRetryTimes = GetWaitTimeOut() / SEND_RETRY_DURATION;
    auto ret = UBSE_ERROR;
    uint32_t retryCount = 0;
    while (ret != UBSE_OK && retryCount < maxRetryTimes) {
        std::string masterId{};
        ret = UbseGetMasterNodeId(masterId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get master nodeId failed, " << FormatRetCode(ret);
            retryCount++;
            sleep(SEND_RETRY_DURATION);
            continue;
        }
        sendParam.SetRemoteId(masterId);
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
        if (ret == UBSE_OK) {
            break;
        }
        UBSE_LOG_ERROR << "Send to importObj, name=" << importObj.req.name
                       << ", requestNodeId=" << importObj.req.requestNodeId << ", requestId=" << importObj.req.requestId
                       << ", masterNodeId=" << sendParam.GetRemoteId() << " failed, " << FormatRetCode(ret);
        retryCount++;
        sleep(SEND_RETRY_DURATION);
    }
    return ret;
}

UbseResult SendFdImportObj(const UbseMemFdBorrowImportObj& importObj, const bool isMaster,
                           const std::string& nodeId = "")
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule, requestId=" << importObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_FD_BORROW_IMPORT_OBJ_CALLBACK));
    UbseMemFdBorrowImportobjSimpoPtr ptr = new (std::nothrow) UbseMemFdBorrowImportobjSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr, requestId=" << importObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemFdBorrowImportobj(importObj);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr, requestId=" << importObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    // 主节点向从履行侧发送
    UbseResult ret = UBSE_OK;
    if (isMaster) {
        for (int i = 0; i < SEND_RETRY_TIMES; i++) {
            ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
            if (ret == UBSE_OK) {
                UBSE_LOG_INFO << "Success to send importObj, name=" << importObj.req.name
                              << ", requestNodeId=" << importObj.req.requestNodeId
                              << ", requestId=" << importObj.req.requestId;
                return UBSE_OK;
            }
            UBSE_LOG_ERROR << "Failed to Send to importObj, name=" << importObj.req.name
                           << ", requestNodeId=" << importObj.req.requestNodeId << ", wait to retry"
                           << ", requestId=" << importObj.req.requestId;
            sleep(SEND_RETRY_DURATION);
        }
        UBSE_LOG_ERROR << "Failed to Send to importObj, name=" << importObj.req.name
                       << ", requestNodeId=" << importObj.req.requestNodeId
                       << ", requestId=" << importObj.req.requestId;
        return ret;
    }

    // 履行侧发回
    return AgentSendFdImportObj(comModule, sendParam, ptr, ubseResponsePtr, importObj);
}

uint32_t FdExportExpectSuccessMasterCallback(UbseMemOperationResp& resp, UbseMemFdBorrowExportObj& exportObj,
                                             UbseMemFdBorrowImportObj& importObj, const std::string& name,
                                             const std::string& exportNodeId, const std::string& importNodeId)
{
    if (!HasAgentAlreadyReported<UbseMemFdBorrowExportObj>(
            GenerateExportObjKey(exportObj.req.name, exportObj.req.importNodeId), exportNodeId,
            &UbseMemFdBorrowExportObj::isCreateReportReceived)) {
        UBSE_LOG_INFO << "No need to callback for export Created, name=" << name
                      << ", requestId=" << exportObj.req.requestId;
        return UBSE_OK;
    }
    if (exportObj.status.state == UBSE_MEM_EXPORT_SUCCESS) { // 导出成功 开始导入
        UBSE_LOG_INFO << "Export is successful, start to import. name=" << name
                      << ", requestId=" << exportObj.req.requestId;
        importObj.exportObmmInfo = exportObj.status.exportObmmInfo;
        auto ret = GetCnaInfoWhenImport(exportNodeId, importNodeId, importObj, true);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to get cna info when inport" << FormatRetCode(ret)
                           << ", requestId=" << exportObj.req.requestId;
            BorrowFailedAdvice(ProcessType::BORROW_FAILED, name, "WATER_BORROW", exportObj.req.size, exportNodeId,
                               importNodeId, ret, MemAdvice::INTERNAL_FAILED);
            return FdExportRollback(resp, exportObj, importObj, name, exportNodeId);
        }
        FdExportUpdateState(exportObj, exportObj.status.state);
        importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
        importObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
        importObj.req.trustRingData.lendSignedDatas = exportObj.req.trustRingData.lendSignedDatas;
        importObj.isCreateReportReceived = false;
        UbseMemFdExportObjStateChangeHandler(exportObj);
        FdImportUpdateState(importObj, UBSE_MEM_IMPORT_RUNNING);
        if (ret = SendFdImportObj(importObj, true, importNodeId); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to send import, name=" << name << ", requestId=" << exportObj.req.requestId;
            BorrowFailedAdvice(ProcessType::BORROW_FAILED, name, "WATER_BORROW", exportObj.req.size, exportNodeId,
                               importNodeId, ret, MemAdvice::COMM_FAILED);
            return FdExportRollback(resp, exportObj, importObj, name, exportNodeId);
        }
        return UBSE_OK;
    }
    // 导出失败，删除导入/导出记录
    UBSE_LOG_INFO << "Export is unsuccessful, start to rollback. name=" << name
                  << ", requestId=" << exportObj.req.requestId;
    EraseFdExport(exportObj);
    EraseFdImport(importObj);
    auto copy = exportObj;
    copy.status.state = UbseMemState::UBSE_MEM_STATE_FAILED; // 通知算法
    UbseMemFdExportObjStateChangeHandler(copy);
    return BuildOperationRespWhenFail(resp, name, exportObj.req.requestNodeId, "Failed to export", exportObj.errorCode);
}

uint32_t FdExportMasterCallback(const std::string& exportNodeId, UbseMemFdBorrowExportObj& exportObj,
                                const std::string& importNodeId, const std::string& name)
{
    UBSE_LOG_INFO << "Fd export master callback, name=" << name << ", state=" << exportObj.status.state
                  << ", requestId=" << exportObj.req.requestId;
    auto exportKey = GenerateExportObjKey(name, exportObj.req.importNodeId);
    auto lock = LoggingLockGuard(exportKey);
    UbseMemOperationResp resp{
        .name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId, .requestId = exportObj.req.requestId};
    UbseMemFdBorrowImportObj importObj{};
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_DESTROYED) {
        return FdExportExpectDestroyMasterCallback(resp, exportObj, exportNodeId, name);
    }
    auto importObjPtr =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().GetResource(importNodeId, name);
    if (!importObjPtr) {
        UBSE_LOG_ERROR << "Failed to find import obj, name=" << name << ", requestId=" << exportObj.req.requestId;
        return UBSE_ERROR;
    }
    importObj = *importObjPtr;
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_SUCCESS) {
        return FdExportExpectSuccessMasterCallback(resp, exportObj, importObj, name, exportNodeId, importNodeId);
    }
    return UBSE_OK;
}

uint32_t FdImportRunningHandler(UbseMemFdBorrowImportObj& importObj, const std::string& name,
                                const std::string& requestNodeId)
{
    UBSE_LOG_INFO << "Fd import running agent callback, name=" << name << ", requestId=" << importObj.req.requestId;
    std::pair<uint32_t, uint32_t> chipDiePair{};
    std::pair<uint32_t, uint32_t> remoteChipDiePair{};
    std::string exportNodeId = importObj.algoResult.exportNumaInfos[0].nodeId;
    auto res = decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.attachSocketId, chipDiePair);
    res |= decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.exportNumaInfos[0].socketId,
                                                            remoteChipDiePair);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetChipAndDieId by socketId failed";
        return UBSE_ERR_INTERNAL;
    }

    decoder::utils::ImportDecoderParam importParam{};
    const uint8_t decoderId = decoder::utils::MemDecoderUtils::GetDecoderIdByPrivData(importObj.req.ubseMemPrivData);
    decoder::utils::MemDecoderUtils::SetImportDecoderParam(importParam);
    importParam.decoderIdx = decoderId;
    importParam.flag |= UB_MEMORY_IMPORT_SHARE_TYPE;
    importParam.isHighSafety = IsHighSafety();
    importParam.trustRingData = importObj.req.trustRingData;
    importParam.type = "fd";
    {
        std::shared_lock lock(GetDecoderImportMutex());
        res = ImportToAddDecoderEntry(chipDiePair, importObj.exportObmmInfo, importParam, importObj.status);
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "ImportToAddDecoderEntry failed, res=" << res;
            UnimportToDelDecoderEntry(chipDiePair, importObj.status, decoderId);
            return UBSE_ERR_INTERNAL;
        }
        importObj.req.trustRingData.ClearLendSignedDataMemory();
        FdImportUpdateState(importObj, UBSE_MEM_IMPORT_RUNNING);
    }
    if (auto ret = UbseMmiInterface::GetInstance().FdImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to import, name=" << name << ", requestNodeId=" << requestNodeId
                       << ", requestId=" << importObj.req.requestId;
        UnimportToDelDecoderEntry(chipDiePair, importObj.status, decoderId);
        EraseFdImport(importObj);
        return ret;
    }
    UBSE_LOG_INFO << "Success to import fd, name=" << name << ", requestId=" << importObj.req.requestId;
    UBSE_AUDIT_RUNTIME_ALLOC << name << " on Node: " << importObj.req.importNodeId << " FdMemory Import "
                             << std::to_string(importObj.req.size) << " Bytes Success";
    return UBSE_OK;
}

uint32_t SendFdImport(const UbseMemFdBorrowImportObj& importObj, const std::string& name,
                      const std::string& exportNodeId, bool isMaster)
{
    auto ret = SendFdImportObj(importObj, false);
    if (ret != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::IMPORT_FAILED, name, "WATER_BORROW", importObj.req.size, exportNodeId,
                           importObj.req.requestNodeId, ret, MemAdvice::COMM_FAILED);
    }
    return ret;
}

uint32_t FdImportRunningCallback(UbseMemFdBorrowImportObj& importObj, const std::string& name,
                                 const std::string& requestNodeId)
{
    auto existingObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().GetResource(
        importObj.req.importNodeId, importObj.req.name);
    if (existingObjPtr && existingObjPtr->status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS) {
        return UBSE_OK;
    }

    auto res = FdImportRunningHandler(importObj, name, requestNodeId);
    if (res != UBSE_OK) {
        importObj.errorCode = res;
        importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
        BorrowFailedAdvice(ProcessType::IMPORT_FAILED, name, "WATER_BORROW", importObj.req.size,
                           importObj.algoResult.exportNumaInfos[0].nodeId, importObj.req.importNodeId, res,
                           MemAdvice::OBMM_FAILED);
    } else {
        FdImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    }
    return SendFdImport(importObj, name, importObj.algoResult.exportNumaInfos[0].nodeId, false);
}

uint32_t FdImportDestroyingHandler(UbseMemFdBorrowImportObj& importObj, const std::string& name,
                                   const std::string& requestNodeId)
{
    UBSE_LOG_INFO << "Fd import destroying agent callback, name=" << name << ", requestId=" << importObj.req.requestId;
    auto existingObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().GetResource(
        importObj.req.importNodeId, name);
    bool directReply = !existingObjPtr || existingObjPtr->status.state == UBSE_MEM_IMPORT_DESTROYED;
    if (directReply) {
        return UBSE_OK;
    }

    std::pair<uint32_t, uint32_t> chipDiePair{};
    auto res = decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.attachSocketId, chipDiePair);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetChipAndDieId by socketId failed";
        return UBSE_MEMCONTROLLER_ERROR_UNIMPORT_FAILED;
    }
    FdImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    if (auto ret = UbseMmiInterface::GetInstance().FdUnImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to unimport, name=" << name << ", requestId=" << importObj.req.requestId;
        return ret;
    }
    UBSE_LOG_INFO << "Success to unimport fd, name=" << name << ", requestId=" << importObj.req.requestId;
    UBSE_AUDIT_RUNTIME_DEALLOC << name << " on Node: " << importObj.req.importNodeId << " FdMemory UnImport "
                               << std::to_string(importObj.req.size) << " Bytes Success";
    const uint8_t decoderId = decoder::utils::MemDecoderUtils::GetDecoderIdByPrivData(importObj.req.ubseMemPrivData);
    UnimportToDelDecoderEntry(chipDiePair, importObj.status, decoderId);
    if (!importObj.status.decoderResult.empty()) {
        UBSE_LOG_ERROR << "UnimportToDelDecoderEntry failed";
    }
    return UBSE_OK;
}

uint32_t FdImportDestroyingAgentCallback(UbseMemFdBorrowImportObj& importObj, const std::string& name,
                                         const std::string& requestNodeId)
{
    auto res = FdImportDestroyingHandler(importObj, name, requestNodeId);
    if (res != UBSE_OK) {
        importObj.errorCode = res;
        FdImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
        UBSE_LOG_ERROR << "FdUnImport Failed, Failed count:" << ++g_fdUnimportFailedCount
                       << ". advice: Caller should clear memory and retry. "
                       << "If failures persist, migrate the workload and restart the host.";
        BorrowFailedAdvice(
            ProcessType::UNIMPORT_FAILED, name, "WATER_BORROW", 0,
            importObj.algoResult.exportNumaInfos.empty() ? "" : importObj.algoResult.exportNumaInfos.begin()->nodeId,
            importObj.req.importNodeId, res, MemAdvice::OBMM_FAILED);
    } else {
        importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
        EraseFdImport(importObj);
    }
    if (auto ret = SendFdImportObj(importObj, false); ret != UBSE_OK) {
        BorrowFailedAdvice(
            ProcessType::UNIMPORT_FAILED, name, "WATER_BORROW", 0,
            importObj.algoResult.exportNumaInfos.empty() ? "" : importObj.algoResult.exportNumaInfos.begin()->nodeId,
            importObj.req.importNodeId, res, MemAdvice::COMM_FAILED);
        return ret;
    }
    return UBSE_OK;
}

uint32_t UbseMemFdBorrowExportObjCallback(const UbseMemFdBorrowExportObj& exportObj)
{
    UBSE_LOG_INFO << "Fd export callback, requestId=" << exportObj.req.requestId;
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    auto copy = exportObj;
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto importNodeId = exportObj.req.importNodeId;
    auto name = exportObj.req.name;

    // 履行侧履行
    if (exportNodeId == currentNodeInfo.nodeId &&
        (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING)) {
        return FdExportAgentCallback(exportNodeId, copy, name);
    }
    // 中心侧处理
    return FdExportMasterCallback(exportNodeId, copy, importNodeId, name);
}

uint32_t FdImportAgentCallback(const std::string& requestNodeId, UbseMemFdBorrowImportObj& importObj,
                               const std::string& name)
{
    UBSE_LOG_INFO << "Fd import agent callback. name=" << name << ", state=" << importObj.status.state
                  << ", requestId=" << importObj.req.requestId;
    auto exportKey = GenerateExportObjKey(name, importObj.req.importNodeId);
    auto lock = LoggingLockGuard(exportKey);
    UbseMemOperationResp resp{
        .name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId, .requestId = importObj.req.requestId};
    if (importObj.status.state == UBSE_MEM_IMPORT_RUNNING) {
        return FdImportRunningCallback(importObj, name, requestNodeId);
    }
    return FdImportDestroyingAgentCallback(importObj, name, requestNodeId);
}

void FdImportFillResp(UbseMemOperationResp& resp, const UbseMemFdBorrowImportObj& importObj)
{
    for (const auto importResult : importObj.status.importResults) {
        resp.memIdList.push_back(importResult.memId);
    }
    resp.remoteNumaId = importObj.status.importResults[0].numaId;
    uint64_t realSize{};
    for (const auto& numaInfo : importObj.algoResult.exportNumaInfos) {
        SafeAdd(realSize, numaInfo.size, realSize);
    }
    resp.realSize = std::to_string(realSize);
}

uint32_t FdImportExpectSuccessMasterCallback(UbseMemOperationResp& resp, UbseMemFdBorrowImportObj& importObj,
                                             const std::string& name, const std::string& importNodeId,
                                             const std::string& exportNodeId)
{
    UBSE_LOG_INFO << "Fd import expect success callback, name=" << name << ", requestId=" << importObj.req.requestId;
    if (!HasAgentAlreadyReported<UbseMemFdBorrowImportObj>(importObj.req.name, importObj.req.importNodeId,
                                                           &UbseMemFdBorrowImportObj::isCreateReportReceived)) {
        UBSE_LOG_INFO << "No need to callback for import created, name=" << name
                      << ", requestId=" << importObj.req.requestId;
        return UBSE_OK;
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_SUCCESS) { // 导入成功
        FdImportUpdateState(importObj, importObj.status.state);
        FdImportFillResp(resp, importObj);
        UbseMemFdImportObjStateChangeHandler(importObj);
        return BuildOperationRespWhenSuccess(resp, UBSE_OK);
    }
    // 导入失败 开始回滚
    UBSE_LOG_INFO << "Failed to import, begin to rollback, name=" << name << ", requestId=" << importObj.req.requestId;
    EraseFdImport(importObj);
    auto copy = importObj;
    copy.status.state = UbseMemState::UBSE_MEM_STATE_FAILED; // 通知算法
    UbseMemFdImportObjStateChangeHandler(copy);
    auto exportKey = GenerateExportObjKey(name, importNodeId);
    auto exportObjPtr =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>().GetResource(exportNodeId, exportKey);
    if (exportObjPtr) {
        UbseMemFdBorrowExportObj exportObj = *exportObjPtr;
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
        exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
        exportObj.req.requestId = importObj.req.requestId;
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>().PutResource(exportNodeId, exportKey,
                                                                                            exportObj);
        // 回滚发送失败
        if (auto ret = SendFdExportObj(exportObj, true, exportNodeId); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to send rollback export, requestId=" << importObj.req.requestId;
            FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        }
        return BuildOperationRespWhenFail(resp, name, exportObj.req.requestNodeId, "Failed to import.",
                                          importObj.errorCode, MemOperationType::FD_BORROW);
    }
    return BuildOperationRespWhenFail(resp, name, importObj.req.requestNodeId, "Failed to import.", importObj.errorCode,
                                      MemOperationType::FD_BORROW);
}

uint32_t DealSendFdUnImportObjFailed(UbseMemFdBorrowImportObj& importObj, const UbseMemReturnReq& req,
                                     UbseMemOperationResp& resp, const std::string& name)
{
    resp.name = name;
    resp.requestNodeId = req.requestNodeId;
    FdImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    return BuildOperationRespWhenFail(resp, name, req.requestNodeId, "Failed to send importObj.",
                                      UBSE_MEMCONTROLLER_ERROR_UNIMPORT_FAILED, MemOperationType::FD_RETURN);
}

uint32_t DealSendFdUnExportObjFailed(UbseMemOperationResp& resp, const UbseMemReturnReq& req, const std::string& name,
                                     UbseMemFdBorrowExportObj& exportObj)
{
    resp.name = name;
    resp.requestNodeId = req.requestNodeId;
    FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    return BuildOperationRespWhenFail(resp, name, req.requestNodeId, "Failed to send unimport.",
                                      UBSE_ERR_UNIMPORT_SUCCESS, MemOperationType::FD_RETURN);
}

static uint32_t FdImportExpectDestroySuccessPath(UbseMemOperationResp& resp, UbseMemFdBorrowImportObj& importObj,
                                                 const std::string& name, const std::string& exportNodeId,
                                                 const std::string& importNodeId)
{
    auto req = importObj.returnReq;

    EraseFdImport(importObj);
    UbseMemFdImportObjStateChangeHandler(importObj);
    auto exportKey = GenerateExportObjKey(name, importNodeId);
    auto waitResult = WaitInitLedgerSuccess(exportNodeId);
    if (waitResult != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::RETURN_FAILED, name, "WATER_BORROW", 0, exportNodeId, importNodeId, waitResult,
                           MemAdvice::NODE_IN_MAINTENANCE);
        return BuildOperationRespWhenFail(resp, name, req.requestNodeId, "exportNode is not working.",
                                          UBSE_ERR_UNIMPORT_SUCCESS, MemOperationType::FD_RETURN);
    }
    auto exportObjPtr =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>().GetResource(exportNodeId, exportKey);
    if (exportObjPtr) {
        auto exportObj = *exportObjPtr;
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
        exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
        exportObj.req.requestId = importObj.req.requestId;
        exportObj.returnReq = req;
        exportObj.isDestroyedReportReceived = false;
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowExportObj>().PutResource(exportNodeId, exportKey,
                                                                                            exportObj);
        if (auto ret = SendFdExportObj(exportObj, true, exportNodeId); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to send export, name=" << name << ", requestId=" << importObj.req.requestId;
            BorrowFailedAdvice(ProcessType::RETURN_FAILED, name, "WATER_BORROW", 0, exportNodeId, importNodeId, ret,
                               MemAdvice::COMM_FAILED);
            return DealSendFdUnExportObjFailed(resp, req, name, exportObj);
        }
        return UBSE_OK;
    } else {
        // 删除单导入
        UBSE_LOG_INFO << "Success to delete single import, requestId=" << importObj.req.requestId;
        resp.name = name;
        resp.requestNodeId = req.requestNodeId;
        if (auto ret = BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::FD_RETURN); ret != UBSE_OK) {
            BorrowFailedAdvice(ProcessType::RETURN_FAILED, name, "WATER_BORROW", 0, exportNodeId, importNodeId, ret,
                               MemAdvice::COMM_FAILED);
            return ret;
        }
        return UBSE_OK;
    }
}

// 失败路径：import 未成功销毁时的处理
static uint32_t FdImportExpectDestroyFailPath(UbseMemOperationResp& resp, UbseMemFdBorrowImportObj& importObj,
                                              const std::string& name, const std::string& exportNodeId,
                                              const std::string& importNodeId)
{
    auto req = importObj.returnReq;

    UBSE_LOG_INFO << "Failed to unimport, name=" << name << ", requestId=" << importObj.req.requestId;
    FdImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);

    if (auto ret = BuildOperationRespWhenFail(resp, name, req.requestNodeId, "Failed to unimport.", importObj.errorCode,
                                              MemOperationType::FD_RETURN);
        ret != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::RETURN_FAILED, name, "WATER_BORROW", 0, exportNodeId, importNodeId, ret,
                           MemAdvice::COMM_FAILED);
        return ret;
    }
    return UBSE_OK;
}

uint32_t FdImportExpectDestroyMasterCallback(UbseMemOperationResp& resp, UbseMemFdBorrowImportObj& importObj,
                                             const std::string& name, const std::string& exportNodeId,
                                             const std::string& importNodeId)
{
    auto req = importObj.returnReq;
    UBSE_LOG_INFO << "Fd import expect destroy callback, name=" << name << ", requestId=" << req.requestId;
    if (!HasAgentAlreadyReported<UbseMemFdBorrowImportObj>(importObj.req.name, importObj.req.importNodeId,
                                                           &UbseMemFdBorrowImportObj::isDestroyedReportReceived)) {
        UBSE_LOG_INFO << "No need to callback for import destroyed, name=" << name
                      << ", requestId=" << importObj.req.requestId;
        return UBSE_OK;
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        return FdImportExpectDestroySuccessPath(resp, importObj, name, exportNodeId, importNodeId);
    }
    return FdImportExpectDestroyFailPath(resp, importObj, name, exportNodeId, importNodeId);
}

uint32_t FdImportMasterCallback(const std::string& requestNodeId, UbseMemFdBorrowImportObj& importObj,
                                const std::string& name)
{
    UBSE_LOG_INFO << "Fd import master callback. name=" << name << ", state=" << importObj.status.state
                  << ", requestId=" << importObj.req.requestId;
    auto exportKey = GenerateExportObjKey(name, importObj.req.importNodeId);
    auto lock = LoggingLockGuard(exportKey);
    UbseMemOperationResp resp{
        .name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId, .requestId = importObj.req.requestId};
    auto importNodeId = importObj.req.importNodeId;

    if (importObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_WARN << "The importObj with no export numa info will be ignored.";
        return UBSE_ERROR;
    }
    auto exportNodeId = importObj.algoResult.exportNumaInfos[0].nodeId;
    if (importObj.status.expectState == UBSE_MEM_IMPORT_SUCCESS) {
        return FdImportExpectSuccessMasterCallback(resp, importObj, name, importNodeId, exportNodeId);
    }
    // 归还成功
    if (importObj.status.expectState == UBSE_MEM_IMPORT_DESTROYED) {
        return FdImportExpectDestroyMasterCallback(resp, importObj, name, exportNodeId, importNodeId);
    }
    return UBSE_OK;
}

uint32_t UbseMemFdBorrowImportObjCallback(const UbseMemFdBorrowImportObj& importObj)
{
    UBSE_LOG_INFO << "Fd import callback"
                  << ", requestId=" << importObj.req.requestId;
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    auto requestNodeId = importObj.req.requestNodeId;
    auto name = importObj.req.name;

    auto copy = importObj;
    // 履行侧履行
    if (importObj.req.importNodeId == currentNodeInfo.nodeId &&
        (importObj.status.state == UBSE_MEM_IMPORT_RUNNING || importObj.status.state == UBSE_MEM_IMPORT_DESTROYING)) {
        return FdImportAgentCallback(requestNodeId, copy, name);
    }
    // 中心侧处理
    return FdImportMasterCallback(requestNodeId, copy, name);
}

uint32_t UbseMemFdBorrowImportObjForPermissionCallback(const UbseMemFdBorrowImportObj& importObj)
{
    auto name = importObj.req.name;
    UBSE_LOG_INFO << "Fd import for permission agent callback. name=" << name << ", state=" << importObj.status.state
                  << ", requestId=" << importObj.req.requestId;
    UbseRoleInfo masterInfo{};
    uint32_t ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        return ret;
    }
    std::string masterNodeId = masterInfo.nodeId;
    auto requestNodeId = importObj.req.requestNodeId;
    auto copy = importObj;
    ret = UbseMmiInterface::GetInstance().FdImportPermissionExecutor(copy);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to permission, name=" << name << ", requestId=" << importObj.req.requestId;
        return ret;
    }
    UBSE_LOG_INFO << "Success to permission fd, name=" << name << ", requestId=" << importObj.req.requestId;
    UBSE_AUDIT_RUNTIME_ALLOC << name << " on Node: " << importObj.req.importNodeId << " FdMemory Set Import permission"
                             << "uid:" << std::to_string(importObj.req.owner.uid)
                             << "gid:" << std::to_string(importObj.req.owner.gid)
                             << "pid:" << std::to_string(importObj.req.owner.pid)
                             << "mode:" << std::to_string(importObj.req.owner.mode) << " Success";
    FdImportUpdatePermission(copy);
    return UBSE_OK;
}

uint32_t HandleSingleExportReturn(const UbseMemReturnReq& req, UbseMemOperationResp& resp,
                                  UbseMemFdBorrowExportObj& exportObj)
{
    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "single export has destroyed.",
                                          UBSE_ERR_NOT_EXIST);
    }
    exportObj.req.requestId = req.requestId;
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    exportObj.isDestroyedReportReceived = false;
    return SendFdExportObj(exportObj, true, exportObj.algoResult.exportNumaInfos[0].nodeId);
}

uint32_t FdReturnExistImport(UbseMemFdBorrowImportObj& importObj, UbseMemFdBorrowExportObj& exportObj, bool hasExport,
                             const UbseMemReturnReq& req, UbseMemOperationResp& resp)
{
    auto name = req.name;
    auto requestNodeId = req.requestNodeId;
    // 有导入
    if (importObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "Resource has destroyed.", UBSE_ERR_NOT_EXIST,
                                          MemOperationType::FD_RETURN);
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        if (!hasExport || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
            return BuildOperationRespWhenFail(resp, name, requestNodeId, "Single import has destroyed.",
                                              UBSE_ERR_NOT_EXIST);
        }
        exportObj.req.requestId = req.requestId;
        exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
        return SendFdExportObj(exportObj, true, exportObj.algoResult.exportNumaInfos[0].nodeId);
    }
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    importObj.req.requestId = req.requestId;
    importObj.isDestroyedReportReceived = false;
    FdImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    if (SendFdImportObj(importObj, true, importObj.req.importNodeId) != UBSE_OK) {
        return DealSendFdUnImportObjFailed(importObj, req, resp, name);
    }
    return UBSE_OK;
}

struct BorrowObjResult {
    UbseMemFdBorrowExportObj exportObj{};
    UbseMemFdBorrowImportObj importObj{};
    bool hasImport = false;
    bool hasExport = false;
    uint32_t comErrorCode = UBSE_OK;
};

static uint32_t ReturnFailed(const UbseMemReturnReq& req, UbseMemOperationResp& resp, const std::string& msg,
                             uint32_t errCode, MemAdvice advice)
{
    BorrowFailedAdvice(ProcessType::RETURN_FAILED, req.name, "WATER_BORROW", 0, "", req.importNodeId, errCode, advice);
    return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, msg, errCode, MemOperationType::FD_RETURN);
}

static uint32_t ValidateBorrowResource(const UbseMemReturnReq& req, UbseMemOperationResp& resp,
                                       const std::string& realRequestNodeId, BorrowObjResult& result)
{
    auto exportKey = GenerateExportObjKey(req.name, req.requestNodeId);
    auto lock = LoggingLockGuard(exportKey);
    InitializeResponse(req, resp);
    // 等待导入节点对账完成
    if (auto ret = WaitInitLedgerSuccess(req.importNodeId); ret != UBSE_OK) {
        result.comErrorCode = ReturnFailed(req, resp, "importNode is not ok", ret, MemAdvice::NODE_IN_MAINTENANCE);
        return ret;
    }
    // 查找导入/导出借用对象
    auto [importObjPtr, exportObjPtr] =
        FindBorrowObjPair<UbseMemFdBorrowImportObj, UbseMemFdBorrowExportObj>(req.name, req.importNodeId);
    result.hasImport = importObjPtr != nullptr;
    result.hasExport = exportObjPtr != nullptr;
    if (!result.hasImport && !result.hasExport) {
        result.comErrorCode =
            ReturnFailed(req, resp, "Memory does not exist", UBSE_ERR_NOT_EXIST, MemAdvice::RESOURCE_NOT_EXIST);
        return UBSE_ERR_NOT_EXIST;
    }
    // 检查资源状态（是否正在借用/归还中）
    UbseMemStage memStage = GetMemStageByImportObjState(importObjPtr);
    if (memStage != UbseMemStage::UBSE_CREATING && memStage != UbseMemStage::UBSE_DELETING) {
        memStage = GetMemStageByExportObjState(exportObjPtr);
    }
    if (memStage == UbseMemStage::UBSE_CREATING || memStage == UbseMemStage::UBSE_DELETING) {
        UBSE_LOG_INFO << "resource is being borrowed or returned, name=" << req.name;
        auto ret = (memStage == UbseMemStage::UBSE_CREATING ? UBSE_ERR_CREATING : UBSE_ERR_DELETING);
        result.comErrorCode =
            ReturnFailed(req, resp, "resource being borrowed or returned", ret, MemAdvice::RESOURCE_OPERATION_CONFLICT);
        return ret;
    }
    auto udsInfo = result.hasExport ? exportObjPtr->req.udsInfo : importObjPtr->req.udsInfo;
    auto exportNodeId = result.hasExport ? exportObjPtr->algoResult.exportNumaInfos[0].nodeId : "";
    auto importNodeId = result.hasExport ? exportObjPtr->req.importNodeId : importObjPtr->req.importNodeId;
    if (!CheckCommonReturnPermission(udsInfo, req.udsInfo, realRequestNodeId, importNodeId, exportNodeId)) {
        UBSE_LOG_ERROR << "Error auth, object username=" << udsInfo.username << ", uid=" << udsInfo.uid
                       << ", current req username=" << req.udsInfo.username << ", uid=" << req.udsInfo.uid
                       << ", realRequestNodeId=" << realRequestNodeId << ", importNodeId=" << req.importNodeId
                       << ", exportNodeId=" << exportNodeId;
        result.comErrorCode =
            ReturnFailed(req, resp, "Error auth", UBSE_ERR_AUTH_FAILED, MemAdvice::UBSE_NO_OPERATION_PERMISSION);
        return UBSE_ERR_AUTH_FAILED;
    }
    result.importObj = result.hasImport ? *importObjPtr : UbseMemFdBorrowImportObj{};
    result.exportObj = result.hasExport ? *exportObjPtr : UbseMemFdBorrowExportObj{};
    return UBSE_OK;
}

uint32_t UbseMemFdReturn(const UbseMemReturnReq& req, UbseMemOperationResp& resp, const std::string& realRequestNodeId)
{
    UBSE_LOG_INFO << "Start to fd return, name=" << req.name << ", requestNodeId=" << req.requestNodeId
                  << ", requestId=" << req.requestId << ", realRequestNodeId=" << realRequestNodeId;
    InitializeResponse(req, resp);
    if (!IsMemBorrowFeatureSupported()) {
        return BuildMemFeatureNotSupportedResp(resp, req.name, req.requestNodeId, MemOperationType::FD_RETURN);
    }
    BorrowObjResult result{};
    if (auto ret = ValidateBorrowResource(req, resp, realRequestNodeId, result); ret != UBSE_OK) {
        return result.comErrorCode;
    }
    result.exportObj.returnReq = req;
    result.importObj.returnReq = req;
    uint32_t ret = UBSE_OK;
    if (!result.hasImport) {
        ret = HandleSingleExportReturn(req, resp, result.exportObj);
    } else {
        ret = FdReturnExistImport(result.importObj, result.exportObj, result.hasExport, req, resp);
    }
    if (ret != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::RETURN_FAILED, req.name, "WATER_BORROW", 0, "", req.importNodeId, ret,
                           MemAdvice::COMM_FAILED);
    }
    return ret;
}

uint32_t CheckFdResourceState(const std::string& name, const std::string& importNodeId)
{
    auto [importObjPtr, exportObjPtr] =
        FindBorrowObjPair<UbseMemFdBorrowImportObj, UbseMemFdBorrowExportObj>(name, importNodeId);

    bool importObjExist = false;
    bool exportObjExist = false;

    if (importObjPtr) {
        importObjExist = true;
    }

    if (exportObjPtr) {
        exportObjExist = exportObjPtr->status.state != UBSE_MEM_EXPORT_DESTROYED;
    }

    if (!importObjExist && !exportObjExist) {
        return UBSE_ERR_NOT_EXIST;
    }

    if (!importObjExist && exportObjExist) {
        return UBSE_ERR_UNIMPORT_SUCCESS;
    }

    return GetErrorCodeByObjState(*importObjPtr, exportObjExist);
}

uint32_t DeleteFdExport(const UbseMemFdBorrowExportObj& exportObj)
{
    auto copy = exportObj;
    copy.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    copy.status.state = UBSE_MEM_EXPORT_DESTROYING;
    FdExportUpdateState(copy, UBSE_MEM_EXPORT_DESTROYING);
    UBSE_LOG_INFO << "Force delete. name=" << copy.req.name << ", importNodeId=" << copy.req.importNodeId;
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_WARN << "The exportObj with no export numa info will be ignored.";
        return UBSE_ERROR;
    }
    return SendFdExportObj(copy, true, copy.algoResult.exportNumaInfos[0].nodeId);
}

uint32_t AddFdImport(const UbseMemFdBorrowImportObj& importObj)
{
    auto copy = importObj;
    if (copy.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        EraseFdImport(copy);
        return UbseMemFdImportObjStateChangeHandler(copy);
    }
    UBSE_LOG_INFO << "Add fd import, name=" << copy.req.name << ", importNodeId=" << importObj.req.importNodeId;
    FdImportUpdateState(copy, copy.status.state);
    return UbseMemFdImportObjStateChangeHandler(copy);
}

uint32_t AddFdExport(const UbseMemFdBorrowExportObj& exportObj)
{
    auto copy = exportObj;
    if (copy.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        EraseFdExport(copy);
        return UbseMemFdExportObjStateChangeHandler(copy);
    }
    UBSE_LOG_INFO << "Add fd export, name=" << copy.req.name << ", importNodeId=" << exportObj.req.importNodeId;
    FdExportUpdateState(copy, copy.status.state);
    return UbseMemFdExportObjStateChangeHandler(copy);
}
} // namespace ubse::mem::controller
