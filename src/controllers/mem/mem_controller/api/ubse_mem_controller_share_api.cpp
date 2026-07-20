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
#include "ubse_mem_controller_share_api.h"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

#include "../logging_lock_guard.h"
#include "../ubse_mem_controller_def.h"
#include "../ubse_mem_controller_ledger.h"
#include "../ubse_mem_controller_msg.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_logger_audit.h"
#include "ubse_mem_advice.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_controller_helper.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_mem_scheduler.h"
#include "ubse_mem_share_capabilities.h"
#include "ubse_mem_share_forward.h"
#include "ubse_mem_share_store.h"
#include "ubse_mem_sign_verifier.h"
#include "ubse_mem_util.h"
#include "ubse_mmi_def.h"
#include "ubse_node_controller_util.h"

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
using namespace ubse::mem::def;
using namespace ubse::mem::util;
using namespace ubse::mem::controller::debt;
const std::string ClusterHandlerKey = "NODE_CLUSTER_HDL";

static uint32_t ShareBorrowFailed(const UbseMemShareBorrowReq &req, UbseMemOperationResp &resp, const std::string &msg,
                                  uint32_t errCode, MemAdvice advice)
{
    BorrowFailedAdvice(ProcessType::BORROW_FAILED, req.name, "SHARE_BORROW", req.size, "", "", errCode, advice);
    return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, msg, errCode, MemOperationType::SHARED_BORROW);
}

static uint32_t PrepareBorrowExport(UbseMemShareBorrowReq &req, UbseMemOperationResp &resp, IShareStore &store,
                                    UbseMemShareBorrowExportObj &exportObj)
{
    NormalizeShareRegion(req);
    resp.name = req.name;
    resp.requestId = req.requestId;
    if (CheckBorrowDuplicate(req.name, store)) {
        ShareBorrowFailed(req, resp, "Resource Exist.", UBSE_ERR_EXISTED, MemAdvice::RESOURCE_EXIST);
        return UBSE_ERR_EXISTED;
    }
    if (!ValidateAffinityParams(req)) {
        ShareBorrowFailed(req, resp, "Invalid Affinity parameters", UBSE_ERR_SHM_AFFINITY_PARAMS_ABNORMAL,
                          MemAdvice::CHECK_FAILED);
        return UBSE_ERR_SHM_AFFINITY_PARAMS_ABNORMAL;
    }
    exportObj.req = req;
    exportObj.status.state = UBSE_MEM_SCHEDULING;

    if (SetNodeIndex(exportObj.req) != UBSE_OK) {
        UBSE_LOG_ERROR << "[MMC] Failed to SetNodeIndex, name=" << exportObj.req.name
                       << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << req.requestId;
        ShareBorrowFailed(req, resp, "SetNodeIndex Failed.", UBSE_ERR_INTERNAL, MemAdvice::SCHEDULE_FAILED);
        return UBSE_ERR_INTERNAL;
    }

    auto ret = ShareAllocate(req, exportObj);
    if (ret != UBSE_OK || exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "[MMC] Failed to allocate, name=" << exportObj.req.name
                       << ", requestNodeId=" << exportObj.req.requestNodeId << ", " << FormatRetCode(ret)
                       << ", requestId=" << resp.requestId;
        ShareBorrowFailed(req, resp, "Failed to allocate", UBSE_ERR_ALLOCATE, MemAdvice::SCHEDULE_FAILED);
        return UBSE_ERR_ALLOCATE;
    }
    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    store.PutExport(exportObj);
    return UBSE_OK;
}

static uint32_t HandleBorrowExportError(IShareStore &store, UbseMemShareBorrowExportObj &exportObj,
                                        const UbseMemShareBorrowReq &req, UbseMemOperationResp &resp)
{
    store.RemoveExport(exportObj);
    auto copy = exportObj;
    copy.status.state = UBSE_MEM_STATE_FAILED;
    UbseMemShmExportObjStateChangeHandler(copy);
    return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Failed to Send export", UBSE_ERR_INTERNAL,
                                      MemOperationType::SHARED_BORROW);
}

uint32_t UbseMemShareBorrow(UbseMemShareBorrowReq &req, UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "Share borrow begins, name=" << req.name << ", requestNodeId=" << req.requestNodeId
                  << ", requestId=" << req.requestId;
    auto lock = LoggingLockGuard(req.name);
    CascadeMasterStore store;
    UbseMemShareBorrowExportObj exportObj;
    if (auto ret = PrepareBorrowExport(req, resp, store, exportObj); ret != UBSE_OK) {
        return ret;
    }
    auto ret = ForwardExportObjToExecutor(exportObj, exportObj.algoResult.exportNumaInfos[0].nodeId);
    if (ret != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::BORROW_FAILED, req.name, "SHARE_BORROW", req.size,
                           exportObj.algoResult.exportNumaInfos[0].nodeId, "", ret, MemAdvice::COMM_FAILED);
        return HandleBorrowExportError(store, exportObj, req, resp);
    }
    return UBSE_OK;
}

void FindShareBorrowObjByName(const std::string &name, std::vector<UbseMemShareBorrowExportObj> &exportObjs,
                              std::vector<UbseMemShareBorrowImportObj> &importObjs)
{
    auto [exportObjTmp, importObjTmps] = GetMaxRefCountExportObj(name);
    if (exportObjTmp) {
        UBSE_LOG_INFO << "obj_state=" << static_cast<uint32_t>(exportObjTmp->status.state);
        if (exportObjTmp->status.state == UBSE_MEM_EXPORT_SUCCESS) {
            exportObjs.push_back(*exportObjTmp);
        }
    }
    for (const auto &importObjTmp : importObjTmps) {
        UBSE_LOG_INFO << "obj_state=" << static_cast<uint32_t>(importObjTmp->status.state);
        if (importObjTmp->status.state != UBSE_MEM_IMPORT_DESTROYED) {
            importObjs.push_back(*importObjTmp);
        }
    }
    if (!exportObjs.empty() || !importObjs.empty()) {
        UBSE_LOG_INFO << "export_obj_size=" << exportObjs.size() << ", import_obj_size=" << importObjs.size();
    }
}

uint32_t ExistImportObjHandler(const UbseMemShareAttachReq &req, UbseMemShareBorrowImportObj importObj,
                               UbseMemOperationResp &resp, const std::string &name, const std::string &requestNodeId)
{
    for (const auto importResult : importObj.status.importResults) {
        resp.memIdList.push_back(importResult.memId);
    }
    if (!importObj.status.importResults.empty()) {
        resp.remoteNumaId = importObj.status.importResults[0].numaId;
    }
    resp.name = name;
    resp.requestNodeId = requestNodeId;
    uint64_t realSize{};
    for (const auto &numaInfo : importObj.algoResult.exportNumaInfos) {
        SafeAdd(realSize, numaInfo.size, realSize);
    }
    resp.realSize = std::to_string(realSize);
    return BuildOperationRespWhenFail(resp, name, requestNodeId, "The importNodeId has attached.", UBSE_ERR_EXISTED,
                                      MemOperationType::SHARED_ATTACH);
}

void ShareImportUpdateState(UbseMemShareBorrowImportObj &importObj, const UbseMemState &state)
{
    importObj.status.state = state;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource(
        importObj.importNodeId, importObj.req.name, importObj);
}

static uint32_t PrepareAttachPreconditions(const UbseMemShareAttachReq &req,
                                           const std::vector<UbseMemShareBorrowExportObj> &exportObjs,
                                           const std::vector<UbseMemShareBorrowImportObj> &importObjs,
                                           UbseMemShareBorrowImportObj &importObj, UbseMemOperationResp &resp)
{
    if (exportObjs.size() != 1 || exportObjs[0].algoResult.exportNumaInfos.empty() ||
        exportObjs[0].status.exportObmmInfo.empty()) {
        BorrowFailedAdvice(ProcessType::IMPORT_FAILED, req.name, "SHARE_BORROW", req.size, "", req.importNodeId,
                           UBSE_ERR_NOT_EXIST, MemAdvice::CHECK_FAILED);
        BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "exportObj is Invaild.", UBSE_ERR_NOT_EXIST,
                                   MemOperationType::SHARED_ATTACH);
        return UBSE_ERR_NOT_EXIST;
    }
    if (!CheckRegions(req, exportObjs[0])) {
        BorrowFailedAdvice(ProcessType::IMPORT_FAILED, req.name, "SHARE_BORROW", req.size, "", req.importNodeId,
                           UBSE_ERR_INTERNAL, MemAdvice::CHECK_FAILED);
        BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "The node is not true region.", UBSE_ERR_INTERNAL,
                                   MemOperationType::SHARED_ATTACH);
        return UBSE_ERR_INTERNAL;
    }
    if (!exportObjs[0].req.udsInfo.CheckPermission(req.udsInfo)) {
        UBSE_LOG_ERROR << "name=" << req.name << " auth failed,req username=" << req.udsInfo.username
                       << ", req uid=" << req.udsInfo.uid
                       << ", export obj username=" << exportObjs[0].req.udsInfo.username
                       << ", export obj uid=" << exportObjs[0].req.udsInfo.uid;
        BorrowFailedAdvice(ProcessType::IMPORT_FAILED, req.name, "SHARE_BORROW", req.size, "", req.importNodeId,
                           UBSE_ERR_INTERNAL, MemAdvice::CHECK_FAILED);
        BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "Error auth", UBSE_ERR_AUTH_FAILED,
                                   MemOperationType::SHARED_ATTACH);
        return UBSE_ERR_AUTH_FAILED;
    }
    if (ExistImportObj(req.name, req.importNodeId, importObjs, importObj)) {
        ExistImportObjHandler(req, importObj, resp, req.name, req.importNodeId);
        return UBSE_ERR_EXISTED;
    }
    return UBSE_OK;
}

static uint32_t PrepareAttachImport(const UbseMemShareAttachReq &req, IShareStore &store,
                                    const UbseMemShareBorrowExportObj &exportObj, UbseMemOperationResp &resp,
                                    UbseMemShareBorrowImportObj &importObj)
{
    importObj.exportObmmInfo = exportObj.status.exportObmmInfo;
    importObj.algoResult = exportObj.algoResult;
    importObj.req = exportObj.req;
    if (auto ret = store.GetCnaTopo(req, exportObj, importObj); ret != UBSE_OK) {
        return ret;
    }
    importObj.req.trustRingData.ClearReqSignedDataMemory();
    UBSE_LOG_INFO << "import size=" << importObj.req.size << ", requestId=" << req.requestId;
    ConstructShareImportObj(importObj, req);
    store.PutImport(importObj);
    return UBSE_OK;
}

uint32_t UbseMemShareAttach(const UbseMemShareAttachReq &req, UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "Share attach begins, name=" << req.name << ", requestNodeId=" << req.requestNodeId
                  << ", requestId=" << req.requestId;
    auto lock = LoggingLockGuard(req.name + "_" + req.requestNodeId);
    // deleteAndBorrowLock 避免attach时获取到export对象后, delete紧跟着对export进行删除,导致单边导入账本
    auto deleteAndBorrowLock = LoggingLockGuard(req.name, LoggingLockGuard::LockType::READ);    
    resp.requestId = req.requestId;
    if (req.importNodeId.empty()) {
        return BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "attach with no node is valid.",
                                          UBSE_ERR_SHM_NODE_EMPTY, MemOperationType::SHARED_ATTACH);
    }
    CascadeMasterStore store;
    UbseNodeControllerLockMgr::WriteLock(ClusterHandlerKey);
    std::vector<UbseMemShareBorrowExportObj> exportObjs{};
    std::vector<UbseMemShareBorrowImportObj> importObjs{};
    FindShareBorrowObjByName(req.name, exportObjs, importObjs);
    UbseMemShareBorrowImportObj importObj{};
    if (auto ret = PrepareAttachPreconditions(req, exportObjs, importObjs, importObj, resp); ret != UBSE_OK) {
        UbseNodeControllerLockMgr::WriteUnLock(ClusterHandlerKey);
        return ret;
    }
    if (auto ret = PrepareAttachImport(req, store, exportObjs[0], resp, importObj); ret != UBSE_OK) {
        UbseNodeControllerLockMgr::WriteUnLock(ClusterHandlerKey);
        BorrowFailedAdvice(ProcessType::BORROW_FAILED, req.name, "SHARE_BORROW", req.size,
                           importObj.algoResult.exportNumaInfos[0].nodeId, req.importNodeId, ret,
                           MemAdvice::INTERNAL_FAILED);
        return BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "Failed to get cna info when import", ret,
                                          MemOperationType::SHARED_ATTACH);
    }
    UbseNodeControllerLockMgr::WriteUnLock(ClusterHandlerKey);
    if (auto ret = ForwardImportObjToExecutor(importObj, req.importNodeId); ret != UBSE_OK) {
        store.RemoveImport(importObj);
        BorrowFailedAdvice(ProcessType::BORROW_FAILED, req.name, "SHARE_BORROW", req.size,
                           importObj.algoResult.exportNumaInfos[0].nodeId, req.importNodeId, ret,
                           MemAdvice::COMM_FAILED);
        return BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "Failed to Send import", UBSE_ERR_INTERNAL,
                                          MemOperationType::SHARED_ATTACH);
    }
    return UBSE_OK;
}

static uint32_t ShareDetachFailed(const UbseMemShareDetachReq &req, UbseMemOperationResp &resp, const std::string &msg,
                                  uint32_t errCode, MemAdvice advice)
{
    BorrowFailedAdvice(ProcessType::RETURN_FAILED, req.name, "SHARE_BORROW", 0, "", req.unImportNodeId, errCode,
                       advice);
    return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, msg, errCode, MemOperationType::SHARED_DETACH);
}
static uint32_t ShareDetachRollback(UbseMemShareBorrowImportObj &importObj, const UbseMemShareDetachReq &req,
                                    UbseMemOperationResp &resp, uint32_t ret)
{
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    BorrowFailedAdvice(ProcessType::RETURN_FAILED, req.name, "SHARE_BORROW", 0, "", req.unImportNodeId, ret,
                       MemAdvice::COMM_FAILED);
    return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Failed to Send import", UBSE_ERR_INTERNAL,
                                      MemOperationType::SHARED_DETACH);
}

static uint32_t PrepareDetachPreconditions(const UbseMemShareDetachReq &req, const std::string &realRequestNodeId,
                                           UbseMemOperationResp &resp, UbseMemShareBorrowImportObj &importObj)
{
    std::vector<UbseMemShareBorrowExportObj> exportObjs;
    std::vector<UbseMemShareBorrowImportObj> importObjs;
    FindShareBorrowObjByName(req.name, exportObjs, importObjs);
    if (!ExistImportObj(req.name, req.unImportNodeId, importObjs, importObj)) {
        ShareDetachFailed(req, resp, "Detach is not allowed, no import obj.", UBSE_ERR_SHM_NO_ATTACH,
                          MemAdvice::RESOURCE_NOT_EXIST);
        return UBSE_ERR_SHM_NO_ATTACH;
    }
    UbseMemStage memStage = GetMemStageByShareImportObjState(importObj, true);
    if (memStage == UbseMemStage::UBSE_CREATING) {
        ShareDetachFailed(req, resp, "Import obj is in creating.", UBSE_ERR_CREATING,
                          MemAdvice::RESOURCE_OPERATION_CONFLICT);
        return UBSE_ERR_CREATING;
    }
    if (memStage == UbseMemStage::UBSE_DELETING) {
        ShareDetachFailed(req, resp, "Import obj is in deleting.", UBSE_ERR_DELETING,
                          MemAdvice::RESOURCE_OPERATION_CONFLICT);
        return UBSE_ERR_DELETING;
    }
    if (!CheckShareDetachPermission(importObj.req.udsInfo, req.udsInfo, realRequestNodeId, importObj.importNodeId)) {
        UBSE_LOG_ERROR << "name=" << req.name << " auth failed,req username=" << req.udsInfo.username
                       << ", req uid=" << req.udsInfo.uid
                       << ", importObj obj username=" << importObj.req.udsInfo.username
                       << ", import obj uid=" << importObj.req.udsInfo.uid << "importObj.importNodeId"
                       << importObj.importNodeId << ", realRequestNodeId=" << realRequestNodeId;
        ShareDetachFailed(req, resp, "Error auth.", UBSE_ERR_AUTH_FAILED, MemAdvice::UBSE_NO_OPERATION_PERMISSION);
        return UBSE_ERR_AUTH_FAILED;
    }
    return UBSE_OK;
}

uint32_t UbseMemShareDetach(const UbseMemShareDetachReq &req, UbseMemOperationResp &resp,
                            const std::string &realRequestNodeId)
{
    UBSE_LOG_INFO << "Share detach begins, name=" << req.name << ", requestNodeId=" << req.requestNodeId
                  << ", requestId=" << req.requestId << ", realRequestNodeId=" << realRequestNodeId;
    auto lock = LoggingLockGuard(req.name + "_" + req.requestNodeId);
    resp.requestId = req.requestId;
    if (req.unImportNodeId.empty()) {
        return ShareDetachFailed(req, resp, "Detach with no node is valid.", UBSE_ERR_SHM_NODE_EMPTY,
                                 MemAdvice::NODE_IN_MAINTENANCE);
    }
    auto waitResult = WaitNodeStateWork(req.unImportNodeId);
    if (waitResult != UBSE_OK) {
        return ShareDetachFailed(req, resp, "importNode is not ok", waitResult, MemAdvice::NODE_IN_MAINTENANCE);
    }
    UbseMemShareBorrowImportObj importObj{};
    if (auto ret = PrepareDetachPreconditions(req, realRequestNodeId, resp, importObj); ret != UBSE_OK) {
        return ret;
    }
    importObj.req.requestId = req.requestId;
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    importObj.isDestroyedReportReceived = false;
    ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    //  下发importObj;
    if (auto ret = ForwardImportObjToExecutor(importObj, req.unImportNodeId); ret != UBSE_OK) {
        return ShareDetachRollback(importObj, req, resp, ret);
    }
    return UBSE_OK;
}

void ShareExportUpdateState(UbseMemShareBorrowExportObj &exportObj, const UbseMemState &state)
{
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        return;
    }
    exportObj.status.state = state;
    UBSE_LOG_INFO << "Update share export state, name=" << exportObj.req.name << ", state=" << state
                  << ", requestId=" << exportObj.req.requestId;
    auto &ledger = UbseMemDebtLedger::GetInstance();
    ledger.GetDebtMap<UbseMemShareBorrowExportObj>().PutResource(exportObj.algoResult.exportNumaInfos[0].nodeId,
                                                                 exportObj.req.name, exportObj);
}

void EraseShareExport(const UbseMemShareBorrowExportObj &exportObj)
{
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        return;
    }
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto name = exportObj.req.name;
    UBSE_LOG_INFO << "Erase share export, name=" << exportObj.req.name << ", requestId=" << exportObj.req.requestId;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().RemoveResource(exportNodeId, name);
}

void EraseShareImport(const UbseMemShareBorrowImportObj &importObj)
{
    auto name = importObj.req.name;
    auto importNodeId = importObj.importNodeId;
    UBSE_LOG_INFO << "Erase share import, name=" << importObj.req.name << ", requestId=" << importObj.req.requestId;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().RemoveResource(importNodeId, name);
}

void ShareExportFillResp(UbseMemOperationResp &resp, const UbseMemShareBorrowExportObj &exportObj)
{
    for (const auto obmmInfo : exportObj.status.exportObmmInfo) {
        resp.memIdList.push_back(obmmInfo.memId);
    }
}

uint32_t SendShareExport(const UbseMemShareBorrowExportObj &exportObj, const std::string &name,
                         const std::string &exportNodeId, bool isMaster)
{
    auto res = isMaster ? ForwardExportObjToExecutor(exportObj, exportNodeId) : ReportExportObjToLocalMaster(exportObj);
    if (res != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::EXPORT_FAILED, name, "SHARE_BORROW", exportObj.req.size, exportNodeId, "", res,
                           MemAdvice::COMM_FAILED);
    }
    return res;
}

uint32_t ShareExportRunningAgentCallback(UbseMemOperationResp &resp, UbseMemShareBorrowExportObj &exportObj,
                                         const std::string &name, const std::string &requestNodeId,
                                         const std::string &exportNodeId)
{
    UBSE_LOG_INFO << "Share export running agent callback. name=" << name << ", requestId=" << exportObj.req.requestId;
    auto curNode = GetCurNodeId();
    auto &ledger = UbseMemDebtLedger::GetInstance();
    auto existingObj = ledger.GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(curNode, exportObj.req.name);
    if (existingObj && existingObj->status.state == ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS) {
        return UBSE_OK;
    }
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_RUNNING);
    if (auto ret = UbseMmiInterface::GetInstance().ShmExportExecutor(exportObj); ret != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::EXPORT_FAILED, exportObj.req.name, "SHARE_BORROW", exportObj.req.size,
                           exportNodeId, "", ret, MemAdvice::OBMM_FAILED);
        UBSE_LOG_ERROR << "Failed to export, name=" << name << ", requestNodeId=" << requestNodeId
                       << ", requestId=" << exportObj.req.requestId;
        exportObj.errorCode = ret;
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
        EraseShareExport(exportObj);
        return SendShareExport(exportObj, name, exportNodeId, false);
    }
    UBSE_LOG_INFO << "Success to export share, name=" << name << ", requestId=" << exportObj.req.requestId;
    UBSE_AUDIT_RUNTIME_ALLOC << name << " on Node: " << exportNodeId << " ShareMemory Export"
                             << std::to_string(exportObj.req.size) << "Bytes Success";
    // 高安配置下签名并验签
    if (IsHighSafety()) {
        UbseExportSignReq trustReq{exportObj.req.trustRingData.reqSignedData, "share"};
        trustReq.exportObmmInfo = exportObj.status.exportObmmInfo;
        trustReq.trustRingId = exportObj.req.trustRingData.trustRingId;
        if (const auto ret = UbseMemSignVerifier::SignAndVerify(trustReq, exportObj.req.trustRingData.lendSignedDatas);
            ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to sign for lend information, " << FormatRetCode(ret);
            EraseShareExport(exportObj);
            exportObj.errorCode = ret;
            exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
            return SendShareExport(exportObj, name, exportNodeId, false);
        }
    }
    exportObj.req.trustRingData.ClearReqSignedDataMemory();
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    return SendShareExport(exportObj, name, exportNodeId, false);
}

uint32_t ShareExportDestroyingAgentCallback(UbseMemOperationResp &resp, UbseMemShareBorrowExportObj &exportObj,
                                            const std::string &name, const std::string &requestNodeId,
                                            const std::string &exportNodeId)
{
    UBSE_LOG_INFO << "Share export Destroying agent callback. name=" << name
                  << ", requestId=" << exportObj.req.requestId;
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    auto objPtr =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(exportNodeId, name);
    bool directReply = (objPtr == nullptr);
    if (directReply) {
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
        if (auto ret = ReportExportObjToLocalMaster(exportObj); ret != UBSE_OK) {
            BorrowFailedAdvice(ProcessType::UNEXPORT_FAILED, name, "SHARE_BORROW", 0, exportNodeId, "", ret,
                                MemAdvice::COMM_FAILED);
            return ret;
        }
        return UBSE_OK;
    }
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
    if (auto ret = UbseMmiInterface::GetInstance().ShmUnExportExecutor(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to unexport, name=" << name << ", requestId=" << exportObj.req.requestId;
        exportObj.errorCode = ret;
        ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        BorrowFailedAdvice(ProcessType::UNEXPORT_FAILED, name, "SHARE_BORROW", 0, exportNodeId, "", ret,
                           MemAdvice::OBMM_FAILED);
        ret = ReportExportObjToLocalMaster(exportObj);
        if (ret != UBSE_OK) {
            BorrowFailedAdvice(ProcessType::UNEXPORT_FAILED, name, "SHARE_BORROW", 0, exportNodeId, requestNodeId, ret,
                               MemAdvice::COMM_FAILED);
        }
        return ret;
    }
    UBSE_LOG_INFO << "Success to unexport share, name=" << name << ", requestId=" << exportObj.req.requestId;
    UBSE_AUDIT_RUNTIME_DEALLOC << name << " on Node: " << exportNodeId << " ShareMemory UnExport "
                               << std::to_string(exportObj.req.size) << " Bytes Success";
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    EraseShareExport(exportObj);
    if (auto ret = ReportExportObjToLocalMaster(exportObj); ret != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::UNEXPORT_FAILED, name, "SHARE_BORROW", 0, exportNodeId, "", ret,
                           MemAdvice::COMM_FAILED);
        return ret;
    }
    return UBSE_OK;
}

uint32_t ShareExportAgentCallback(const std::string &exportNodeId, UbseMemShareBorrowExportObj &exportObj,
                                  const std::string &name)
{
    UBSE_LOG_INFO << "Share export agent callback name=" << name << ", state=" << exportObj.status.state
                  << ", requestId=" << exportObj.req.requestId;
    auto lock = LoggingLockGuard(name);
    UbseMemOperationResp resp{
        .name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId, .requestId = exportObj.req.requestId};
    auto requestNodeId = exportObj.req.requestNodeId;
    if (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING) {
        return ShareExportRunningAgentCallback(resp, exportObj, name, requestNodeId, exportNodeId);
    }
    return ShareExportDestroyingAgentCallback(resp, exportObj, name, requestNodeId, exportNodeId);
}

static uint32_t ShareExportReturnCallback(const std::string &exportNodeId, UbseMemShareBorrowExportObj &exportObj,
                                          UbseMemOperationResp &resp)
{
    if (!HasAgentAlreadyReported<UbseMemShareBorrowExportObj>(
            exportObj.req.name, exportNodeId, &UbseMemShareBorrowExportObj::isDestroyedReportReceived)) {
        UBSE_LOG_INFO << "No need to callback for export destroyed, name=" << exportObj.req.name
                      << ", requestId=" << exportObj.req.requestId;
        return UBSE_OK;
    }
    auto req = exportObj.returnReq;
    std::string requestNodeId = req.requestNodeId;
    resp.requestNodeId = requestNodeId;
    resp.requestId = req.requestId;
    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        EraseShareExport(exportObj);
        UBSE_LOG_INFO << "shm callback exportObjStateChange, name=" << exportObj.req.name
                      << ", requestId=" << exportObj.req.requestId;
        UbseMemShmExportObjStateChangeHandler(exportObj);
        UBSE_LOG_INFO << "shm return, name=" << exportObj.req.name << ", requestId=" << exportObj.req.requestId;
        // requestNodeId为空则当前场景为对账删除导出账本
        if (requestNodeId.empty()) {
            return UBSE_OK;
        }
        if (auto ret = BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_RETURN); ret != UBSE_OK) {
            BorrowFailedAdvice(ProcessType::RETURN_FAILED, exportObj.req.name, "SHARE_BORROW", 0, exportNodeId, "", ret,
                               MemAdvice::COMM_FAILED);
            return ret;
        }
        return UBSE_OK;
    }
    // 归还失败
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    // requestNodeId为空则当前场景为对账删除导出账本
    if (requestNodeId.empty()) {
        return UBSE_OK;
    }
    if (auto ret = BuildOperationRespWhenFail(resp, exportObj.req.name, requestNodeId, "Failed to unexport",
                                              exportObj.errorCode, MemOperationType::SHARED_RETURN);
        ret != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::RETURN_FAILED, exportObj.req.name, "SHARE_BORROW", 0, exportNodeId, "", ret,
                           MemAdvice::COMM_FAILED);
        return ret;
    }
    return UBSE_OK;
}

uint32_t ShareExportMasterCallback(const std::string &exportNodeId, UbseMemShareBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << "Share export master callback name=" << exportObj.req.name << ", state=" << exportObj.status.state
                  << ", requestId=" << exportObj.req.requestId;
    auto lock = LoggingLockGuard(exportObj.req.name);
    auto name = exportObj.req.name;
    UbseMemOperationResp resp{
        .name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId, .requestId = exportObj.req.requestId};
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_SUCCESS) {
        if (!HasAgentAlreadyReported<UbseMemShareBorrowExportObj>(
                exportObj.req.name, exportNodeId, &UbseMemShareBorrowExportObj::isCreateReportReceived)) {
            UBSE_LOG_INFO << "No need to callback for export created, name=" << exportObj.req.name
                          << ", requestId=" << exportObj.req.requestId;
            return UBSE_OK;
        }
        auto copy = exportObj;
        if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
            EraseShareExport(exportObj);
            copy.status.state = UBSE_MEM_STATE_FAILED;
            UbseMemShmExportObjStateChangeHandler(copy);
            return BuildOperationRespWhenFail(resp, exportObj.req.name, exportObj.req.requestNodeId, "Failed to export",
                                              exportObj.errorCode, MemOperationType::SHARED_BORROW);
        }
        ShareExportFillResp(resp, exportObj);
        ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        UbseMemShmExportObjStateChangeHandler(copy);
        UBSE_LOG_INFO << "this is shm callback before shm exportObjstateChange, name=" << exportObj.req.name
                      << ", requestId=" << exportObj.req.requestId;
        return BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_BORROW);
    }
    // 归还逻辑
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_DESTROYED) {
        return ShareExportReturnCallback(exportNodeId, exportObj, resp);
    }
    return UBSE_OK;
}

uint32_t UbseMemShareBorrowExportObjCallback(const UbseMemShareBorrowExportObj &exportObj)
{
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    UbseRoleInfo masterInfo{};
    auto ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "Failed to get master's info, " << FormatRetCode(ret);
        return ret;
    }
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_WARN << "The exportObj with no export numa info will be ignored.";
        return UBSE_ERROR;
    }
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto name = exportObj.req.name;
    auto copy = exportObj;

    if (exportNodeId == currentNodeInfo.nodeId &&
        (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING)) {
        return ShareExportAgentCallback(exportNodeId, copy, name);
    }
    return ShareExportMasterCallback(exportNodeId, copy);
}
void ShareImportFillResp(UbseMemOperationResp &resp, const UbseMemShareBorrowImportObj &importObj)
{
    std::stringstream ss;
    for (const auto &importResult : importObj.status.importResults) {
        resp.memIdList.push_back(importResult.memId);
        ss << importResult.memId << ", ";
    }
    UBSE_LOG_INFO << "importObj.status.importResults size=" << importObj.status.importResults.size()
                  << "memIds=" << ss.str();
    uint64_t realSize{};
    for (const auto &numaInfo : importObj.algoResult.exportNumaInfos) {
        realSize += numaInfo.size;
    }
    resp.realSize = std::to_string(realSize);
}

uint32_t RealImportDecoder(const std::pair<uint32_t, uint32_t> &chipDiePair, UbseMemShareBorrowImportObj &importObj)
{
    std::pair<uint32_t, uint32_t> remoteChipDiePair{};
    auto res = decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.exportNumaInfos[0].socketId,
                                                                remoteChipDiePair);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetChipAndDieId by socketId failed";
        return UBSE_ERR_INTERNAL;
    }
    decoder::utils::ImportDecoderParam importParam{};
    decoder::utils::MemDecoderUtils::SetImportDecoderParam(importParam, importObj.req.ubseMemPrivData);
    importParam.isHighSafety = IsHighSafety();
    importParam.trustRingData = importObj.req.trustRingData;
    importParam.type = "share";
    res = ImportToAddDecoderEntry(chipDiePair, importObj.exportObmmInfo, importParam, importObj.status);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "ImportToAddDecoderEntry failed, res=" << res;
        uint8_t decoderId = importObj.req.ubseMemPrivData.cacheableFlag == 1 ? 0 : 1;
        UnimportToDelDecoderEntry(chipDiePair, importObj.status, decoderId);
        return UBSE_ERR_INTERNAL;
    }
    importObj.req.trustRingData.ClearLendSignedDataMemory();
    return UBSE_OK;
}

uint32_t ShareImportRunningHandler(UbseMemOperationResp &resp, UbseMemShareBorrowImportObj &importObj,
                                   const std::string &name, const std::string &requestNodeId)
{
    UBSE_LOG_INFO << "ShareImportRunningAgent callback. name=" << name << ", requestId=" << importObj.req.requestId;
    auto oldImportObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(
        importObj.importNodeId, importObj.req.name);
    if (oldImportObjPtr && oldImportObjPtr->status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS) {
        return UBSE_OK;
    }
    bool realExe = importObj.importNodeId == importObj.algoResult.exportNumaInfos[0].nodeId ? false : true;
    std::pair<uint32_t, uint32_t> chipDiePair{};
    if (realExe) {
        auto res = decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.attachSocketId, chipDiePair);
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "GetChipAndDieId by socketId failed";
            return UBSE_ERR_INTERNAL;
        }
    }
    if (realExe) {
        auto res = RealImportDecoder(chipDiePair, importObj);
        if (res != UBSE_OK) {
            return res;
        }
    }
    ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_RUNNING);
    if (auto ret = UbseMmiInterface::GetInstance().ShmImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to import, name=" << name << ", requestNodeId=" << requestNodeId
                       << ", requestId=" << importObj.req.requestId;
        if (realExe) {
            uint8_t decoderId = importObj.req.ubseMemPrivData.cacheableFlag == 1 ? 0 : 1;
            UnimportToDelDecoderEntry(chipDiePair, importObj.status, decoderId);
        }
        EraseShareImport(importObj);
        return ret;
    }
    UBSE_LOG_INFO << "Success to import share, name=" << name << ", requestId=" << importObj.req.requestId;
    UBSE_AUDIT_RUNTIME_ALLOC << name << " on Node: " << importObj.importNodeId << " ShareMemory Import "
                             << std::to_string(importObj.req.size) << " Bytes Success";
    return UBSE_OK;
}

uint32_t ShareImportRunningAgentCallBack(UbseMemOperationResp &resp, UbseMemShareBorrowImportObj &importObj,
                                         const std::string &name, const std::string &requestNodeId)
{
    auto nowObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(
        importObj.importNodeId, importObj.req.name);
    if (nowObjPtr && nowObjPtr->status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS) {
        return UBSE_OK;
    }
    auto res = ShareImportRunningHandler(resp, importObj, name, requestNodeId);
    if (res != UBSE_OK) {
        importObj.errorCode = res;
        importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
        BorrowFailedAdvice(ProcessType::IMPORT_FAILED, name, "SHARE_BORROW", importObj.req.size,
                           importObj.algoResult.exportNumaInfos[0].nodeId, importObj.importNodeId, res,
                           MemAdvice::OBMM_FAILED);
    } else {
        ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    }
    if (res = ReportImportObjToLocalMaster(importObj); res != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::IMPORT_FAILED, name, "SHARE_BORROW", importObj.req.size,
                            importObj.algoResult.exportNumaInfos[0].nodeId, importObj.importNodeId, res,
                            MemAdvice::COMM_FAILED);
    }
    return res;
}

uint32_t ShareImportDestroyingHandler(UbseMemOperationResp &resp, UbseMemShareBorrowImportObj &importObj,
                                      const std::string &name, const std::string &requestNodeId)
{
    UBSE_LOG_INFO << "Share import destroying agent callback. name=" << name
                  << ", requestId=" << importObj.req.requestId;
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    auto objPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(
        importObj.importNodeId, name);
    bool directReply = (objPtr == nullptr);
    if (directReply) {
        return UBSE_OK;
    }
    ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    if (auto ret = UbseMmiInterface::GetInstance().ShmUnImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to unimport name=" << name << ", requestId=" << importObj.req.requestId;
        return ret;
    }
    UBSE_LOG_INFO << "Success to unimport share. name=" << name << ", requestId=" << importObj.req.requestId;
    UBSE_AUDIT_RUNTIME_DEALLOC << name << " on Node: " << importObj.importNodeId << " ShareMemory UnImport "
                               << std::to_string(importObj.req.size) << " Bytes Success";

    bool realExe = !(importObj.importNodeId == importObj.algoResult.exportNumaInfos[0].nodeId);
    if (realExe) {
        std::pair<uint32_t, uint32_t> chipDiePair{};
        auto res = decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.attachSocketId, chipDiePair);
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "GetChipAndDieId by socketId failed";
        }
        uint8_t decoderId = importObj.req.ubseMemPrivData.cacheableFlag == 1 ? 0 : 1;
        UnimportToDelDecoderEntry(chipDiePair, importObj.status, decoderId);
    }
    if (!importObj.status.decoderResult.empty()) {
        UBSE_LOG_ERROR << "UnimportToDelDecoderEntry failed";
    }
    return UBSE_OK;
}

uint32_t ShareImportDestroyingAgentCallBack(UbseMemOperationResp &resp, UbseMemShareBorrowImportObj &importObj,
                                            const std::string &name, const std::string &requestNodeId)
{
    auto res = ShareImportDestroyingHandler(resp, importObj, name, requestNodeId);
    if (res != UBSE_OK) {
        importObj.errorCode = res;
        ShareImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
        UBSE_LOG_ERROR << "ShareUnImport Failed, Failed count:" << ++g_shareUnimportFailedCount
                       << ". advice: Caller should clear memory and retry. "
                       << "If failures persist, migrate the workload and restart the host.";
        BorrowFailedAdvice(
            ProcessType::UNIMPORT_FAILED, name, "SHARE_BORROW", 0,
            importObj.algoResult.exportNumaInfos.empty() ? "" : importObj.algoResult.exportNumaInfos.begin()->nodeId,
            importObj.importNodeId, res, MemAdvice::OBMM_FAILED);
    } else {
        importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
        EraseShareImport(importObj);
    }
    if (auto ret = ReportImportObjToLocalMaster(importObj); ret != UBSE_OK) {
        BorrowFailedAdvice(
            ProcessType::UNIMPORT_FAILED, name, "SHARE_BORROW", 0,
            importObj.algoResult.exportNumaInfos.empty() ? "" : importObj.algoResult.exportNumaInfos.begin()->nodeId,
            importObj.importNodeId, ret, MemAdvice::COMM_FAILED);
        return ret;
    }
    return UBSE_OK;
}

uint32_t ShareImportAgentCallBack(UbseMemShareBorrowImportObj &importObj, const std::string &name,
                                  const std::string &requestNodeId)
{
    UBSE_LOG_INFO << "Share import agent callback. name=" << name << ", state=" << importObj.status.state
                  << ", requestId=" << importObj.req.requestId;
    auto lock = LoggingLockGuard(name);
    UbseMemOperationResp resp{
        .name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId, .requestId = importObj.req.requestId};
    if (importObj.status.state == UBSE_MEM_IMPORT_RUNNING) {
        return ShareImportRunningAgentCallBack(resp, importObj, name, requestNodeId);
    }
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    return ShareImportDestroyingAgentCallBack(resp, importObj, name, requestNodeId);
}

static uint32_t ShareImportMasterCreateCallback(UbseMemShareBorrowImportObj &importObj, UbseMemOperationResp &resp)
{
    if (!HasAgentAlreadyReported<UbseMemShareBorrowImportObj>(importObj.req.name, importObj.importNodeId,
                                                              &UbseMemShareBorrowImportObj::isCreateReportReceived)) {
        UBSE_LOG_INFO << "No need to callback for export created, name=" << importObj.req.name
                      << ", requestId=" << importObj.req.requestId;
        return UBSE_OK;
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_SUCCESS) {
        ShareImportUpdateState(importObj, importObj.status.state);
        ShareImportFillResp(resp, importObj);
        UBSE_LOG_INFO << "this is shm callback before shm importObjstateChange, name=" << importObj.req.name
                      << ", requestId=" << importObj.req.requestId;
        UbseMemShmImportObjStateChangeHandler(importObj);
        if (auto ret = BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_ATTACH); ret != UBSE_OK) {
            BorrowFailedAdvice(ProcessType::RETURN_FAILED, importObj.req.name, "SHARE_BORROW", 0,
                               importObj.algoResult.exportNumaInfos.empty() ?
                                   "" :
                                   importObj.algoResult.exportNumaInfos.begin()->nodeId,
                               importObj.importNodeId, ret, MemAdvice::COMM_FAILED);
            return ret;
        }
        return UBSE_OK;
    }
    EraseShareImport(importObj);
    return BuildOperationRespWhenFail(resp, importObj.req.name, importObj.req.requestNodeId, "Failed to import.",
                                      importObj.errorCode, MemOperationType::SHARED_ATTACH);
}

static uint32_t ShareImportMasterDestroyCallback(UbseMemShareBorrowImportObj &importObj, UbseMemOperationResp &resp)
{
    if (!HasAgentAlreadyReported<UbseMemShareBorrowImportObj>(
            importObj.req.name, importObj.importNodeId, &UbseMemShareBorrowImportObj::isDestroyedReportReceived)) {
        UBSE_LOG_INFO << "No need to callback for export destroyed, name=" << importObj.req.name
                      << ", requestId=" << importObj.req.requestId;
        return UBSE_OK;
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        EraseShareImport(importObj);
        UBSE_LOG_INFO << "this is shm callback before shm importObjstateChange, name=" << importObj.req.name
                      << ", requestId=" << importObj.req.requestId;
        UbseMemShmImportObjStateChangeHandler(importObj);
        return BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_DETACH);
    }
    ShareImportUpdateState(importObj, importObj.status.state);
    if (auto ret = BuildOperationRespWhenFail(resp, importObj.req.name, importObj.req.requestNodeId,
                                              "Failed to unimport.", importObj.errorCode,
                                              MemOperationType::SHARED_DETACH);
        ret != UBSE_OK) {
        BorrowFailedAdvice(
            ProcessType::RETURN_FAILED, importObj.req.name, "SHARE_BORROW", 0,
            importObj.algoResult.exportNumaInfos.empty() ? "" : importObj.algoResult.exportNumaInfos.begin()->nodeId,
            importObj.importNodeId, ret, MemAdvice::COMM_FAILED);
        return ret;
    }
    return UBSE_OK;
}

uint32_t ShareImportMasterCallBack(UbseMemShareBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << "Share import master callback. name=" << importObj.req.name << ", state=" << importObj.status.state
                  << ", requestId=" << importObj.req.requestId;
    auto lock = LoggingLockGuard(importObj.req.name);
    UbseMemOperationResp resp{
        .name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId, .requestId = importObj.req.requestId};

    if (importObj.status.expectState == UBSE_MEM_IMPORT_SUCCESS) {
        return ShareImportMasterCreateCallback(importObj, resp);
    }
    if (importObj.status.expectState == UBSE_MEM_IMPORT_DESTROYED) {
        return ShareImportMasterDestroyCallback(importObj, resp);
    }
    return UBSE_OK;
}

uint32_t UbseMemShareBorrowImportObjCallback(const UbseMemShareBorrowImportObj &importObj)
{
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);

    auto importNodeId = importObj.importNodeId;
    auto name = importObj.req.name;
    auto copy = importObj;
    if (importNodeId == currentNodeInfo.nodeId &&
        (importObj.status.state == UBSE_MEM_IMPORT_RUNNING || importObj.status.state == UBSE_MEM_IMPORT_DESTROYING)) {
        return ShareImportAgentCallBack(copy, name, importNodeId);
    }

    return ShareImportMasterCallBack(copy);
}
uint32_t DealSendShareUnExportObjFailed(UbseMemShareBorrowExportObj &exportObj, const UbseMemReturnReq &req,
                                        UbseMemOperationResp &resp, const std::string &name)
{
    resp.name = name;
    resp.requestNodeId = req.requestNodeId;
    ShareExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    return BuildOperationRespWhenFail(resp, name, req.requestNodeId, "Failed to send exportObj.",
                                      UBSE_ERR_UNIMPORT_SUCCESS);
}

static uint32_t ShareReturnFail(const UbseMemReturnReq &req, UbseMemOperationResp &resp, const std::string &msg,
                                uint32_t errCode, MemAdvice advice, const std::string &exportNodeId = "",
                                const std::string &importNodeId = "")
{
    BorrowFailedAdvice(ProcessType::RETURN_FAILED, req.name, "SHARE_BORROW", 0, exportNodeId, importNodeId, errCode,
                       advice);
    return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, msg, errCode, MemOperationType::SHARED_RETURN);
}

static uint32_t CheckReturnExportOperableState(const UbseMemShareBorrowExportObj &exportObj,
                                               const UbseMemReturnReq &req, UbseMemOperationResp &resp)
{
    auto memStage = GetMemStageByExportObjState(exportObj, true);
    if (memStage == UbseMemStage::UBSE_CREATING || memStage == UbseMemStage::UBSE_DELETING) {
        UBSE_LOG_INFO << "resource is being borrowed or returned, name=" << req.name;
        auto errCode = (memStage == UbseMemStage::UBSE_CREATING) ? UBSE_ERR_CREATING : UBSE_ERR_DELETING;
        BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "resource being borrowed or returned", errCode,
                                   MemOperationType::SHARED_RETURN);
        return errCode;
    }
    return UBSE_OK;
}

static uint32_t ValidateReturnNoActiveImports(IShareStore &store, const UbseMemReturnReq &req,
                                              UbseMemOperationResp &resp)
{
    std::vector<UbseMemShareBorrowImportObj> importItems;
    store.LoadAllImports(req.name, importItems);
    bool hasActiveImports = false;
    for (const auto &item : importItems) {
        if (item.status.state != UBSE_MEM_IMPORT_DESTROYED && item.status.state != UBSE_MEM_IMPORT_DESTROYING) {
            hasActiveImports = true;
            break;
        }
    }
    if (hasActiveImports) {
        UBSE_LOG_ERROR << "Cannot return when active imports exist, name=" << req.name;
        BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Active imports still exist.",
                                   UBSE_ERR_SHM_ATTACH_USING, MemOperationType::SHARED_RETURN);
        return UBSE_ERR_SHM_ATTACH_USING;
    }
    return UBSE_OK;
}

static uint32_t ValidateReqPermission(const UbseMemReturnReq &req, const std::string &realRequestNodeId,
                                      UbseMemShareBorrowExportObj &exportObj, UbseMemOperationResp &resp)
{
    if (!CheckShareReturnPermission(exportObj.req.udsInfo, req.udsInfo, realRequestNodeId, exportObj.req.shmRegion)) {
        std::string shmRegionIds;
        for (const auto &node : exportObj.req.shmRegion.nodelist) {
            shmRegionIds += node.nodeId + ", ";
        }
        UBSE_LOG_ERROR << "name=" << req.name << " auth failed, reqUid=" << req.udsInfo.uid
                       << ", objUid=" << exportObj.req.udsInfo.uid << ", realRequestNodeId=" << realRequestNodeId
                       << ", shmRegionIds=" << shmRegionIds;
        auto enode = exportObj.algoResult.exportNumaInfos.empty() ? "" : exportObj.algoResult.exportNumaInfos[0].nodeId;
        ShareReturnFail(req, resp, "Error auth", UBSE_ERR_AUTH_FAILED, MemAdvice::UBSE_NO_OPERATION_PERMISSION, enode);
        return UBSE_ERR_AUTH_FAILED;
    }
    return UBSE_OK;
}
static uint32_t PrepareReturnPreconditions(const UbseMemReturnReq &req, const std::string &realRequestNodeId,
                                           UbseMemShareBorrowExportObj &exportObj, UbseMemOperationResp &resp)
{
    std::vector<UbseMemShareBorrowExportObj> exportObjs;
    std::vector<UbseMemShareBorrowImportObj> importObjs;
    FindShareBorrowObjByName(req.name, exportObjs, importObjs);
    if (exportObjs.empty() || exportObjs[0].algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "resource not found, name=" << req.name << ", size of exportObjs = " << exportObjs.size();
        ShareReturnFail(req, resp, "resource not found.", UBSE_ERR_NOT_EXIST, MemAdvice::RESOURCE_NOT_EXIST);
        return UBSE_ERR_NOT_EXIST;
    }
    std::string enode = exportObjs[0].algoResult.exportNumaInfos.begin()->nodeId;
    exportObj = exportObjs[0];
    CascadeMasterStore store;
    if (auto ret = ValidateReturnNoActiveImports(store, req, resp); ret != UBSE_OK) {
        return ret;
    }
    if (auto ret = CheckReturnExportOperableState(exportObj, req, resp); ret != UBSE_OK) {
        return ret;
    }
    if (auto ret = ValidateReqPermission(req, realRequestNodeId, exportObj, resp); ret != UBSE_OK) {
        return ret;
    }
    return UBSE_OK;
}

static void PrepareReturnExport(const UbseMemReturnReq &req, IShareStore &store, UbseMemShareBorrowExportObj &exportObj)
{
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    exportObj.returnReq = req;
    exportObj.isDestroyedReportReceived = false;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    store.UpdateExportState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
}

uint32_t UbseMemShareReturn(const UbseMemReturnReq &req, UbseMemOperationResp &resp,
                            const std::string &realRequestNodeId)
{
    UBSE_LOG_INFO << "Start to share return, name=" << req.name << ", requestNodeId=" << req.requestNodeId
                  << ", requestId=" << req.requestId << ", realRequestNodeId=" << realRequestNodeId;
    auto lock = LoggingLockGuard(req.name);
    InitializeResponse(req, resp);

    UbseMemShareBorrowExportObj exportObj;
    CascadeMasterStore store;
    if (auto ret = PrepareReturnPreconditions(req, realRequestNodeId, exportObj, resp); ret != UBSE_OK) {
        return ret;
    }
    PrepareReturnExport(req, store, exportObj);
    if (auto ret = ForwardExportObjToExecutor(exportObj, exportObj.algoResult.exportNumaInfos[0].nodeId);
        ret != UBSE_OK) {
        BorrowFailedAdvice(
            ProcessType::RETURN_FAILED, req.name, "SHARE_BORROW", 0,
            exportObj.algoResult.exportNumaInfos.empty() ? "" : exportObj.algoResult.exportNumaInfos.begin()->nodeId,
            exportObj.algoResult.importNumaInfos.empty() ? "" : exportObj.algoResult.importNumaInfos.begin()->nodeId,
            ret, MemAdvice::COMM_FAILED);
        return DealSendShareUnExportObjFailed(exportObj, req, resp, req.name);
    }
    return UBSE_OK;
}

uint32_t UpdateFaultShareExportObj(const std::string &nodeId, uint64_t memId, const std::string &memName,
                                   UbMemFaultType type)
{
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Started to update shared memory debt due to fault.";
    auto &exportMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>();
    auto objPtr = exportMap.GetResource(nodeId, memName);
    if (!objPtr) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to find fault memory by name=" << memName << ".";
        return UBSE_ERROR;
    }

    auto obj = *objPtr;
    auto obmmItor = std::find_if(obj.status.exportObmmInfo.begin(), obj.status.exportObmmInfo.end(),
                                 [&](const UbseMemObmmInfo &info) -> bool { return info.memId == memId; });
    if (obmmItor == obj.status.exportObmmInfo.end()) {
        UBSE_LOG_ERROR << "[MEM_CONTROLLER] Failed to find fault memory by memId=" << memId << ".";
        return UBSE_ERROR;
    }
    obmmItor->memIdStatus = type;
    exportMap.PutResource(nodeId, memName, obj);
    UBSE_LOG_INFO << "[MEM_CONTROLLER] Succeed to update the state of export memId= " << memId
                  << " to fault type=" << static_cast<uint16_t>(obmmItor->memIdStatus) << ".";
    return UBSE_OK;
}

uint32_t AddShareImport(const UbseMemShareBorrowImportObj &importObj)
{
    auto copy = importObj;
    if (copy.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        EraseShareImport(copy);
        return UbseMemShmImportObjStateChangeHandler(copy);
    }
    UBSE_LOG_INFO << "Add share import, name=" << copy.req.name << ", import node=" << importObj.importNodeId;
    ShareImportUpdateState(copy, copy.status.state);
    return UbseMemShmImportObjStateChangeHandler(copy);
}

uint32_t AddShareExport(const UbseMemShareBorrowExportObj &exportObj)
{
    auto copy = exportObj;
    if (copy.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        EraseShareExport(copy);
        return UbseMemShmExportObjStateChangeHandler(copy);
    }
    UBSE_LOG_INFO << "Add share export, name=" << copy.req.name;
    ShareExportUpdateState(copy, copy.status.state);
    return UbseMemShmExportObjStateChangeHandler(copy);
}

uint32_t DeleteShareExport(const UbseMemShareBorrowExportObj &exportObj)
{
    auto copy = exportObj;
    copy.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    copy.status.state = UBSE_MEM_EXPORT_DESTROYING;
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_WARN << "The exportObj with no export numa info will be ignored.";
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "Force delete. name=" << copy.req.name;
    ShareExportUpdateState(copy, UBSE_MEM_EXPORT_DESTROYING);
    return ForwardExportObjToExecutor(copy, exportObj.algoResult.exportNumaInfos[0].nodeId);
}

uint32_t UbseMemShareBorrowClos(UbseMemShareBorrowReq &req, UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "clos share borrow begins, name=" << req.name << ", requestNodeId=" << req.requestNodeId
                  << ", requestId=" << req.requestId;
    auto lock = LoggingLockGuard(req.name);
    GlobalMasterStore store;
    UbseMemShareBorrowExportObj exportObj;
    if (auto ret = PrepareBorrowExport(req, resp, store, exportObj); ret != UBSE_OK) {
        return ret;
    }
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto ret = ForwardExportObjToCascade(exportObj);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send ExportObj to Cascade Master, name=" << req.name << ", " << FormatRetCode(ret);
        BorrowFailedAdvice(ProcessType::BORROW_FAILED, req.name, "SHARE_BORROW", req.size, exportNodeId, "", ret,
                           MemAdvice::COMM_FAILED);
        return HandleBorrowExportError(store, exportObj, req, resp);
    }
    return UBSE_OK;
}

uint32_t CascadeMasterHandlerGlobalBorrowExportCallback(const UbseMemShareBorrowExportObj &exportObj)
{
    auto lock = LoggingLockGuard(exportObj.req.name);
    UBSE_LOG_INFO << "CascadeMasterHandlerGlobalBorrowExportCallback, name=" << exportObj.req.name
                  << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << exportObj.req.requestId;
    auto name = exportObj.req.name;
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto &ledger = UbseMemDebtLedger::GetInstance();

    auto existingObj = ledger.GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(exportNodeId, name);
    if (existingObj && existingObj->status.state == UBSE_MEM_EXPORT_SUCCESS) {
        UBSE_LOG_INFO << "ExportObj already exists with EXPORT_SUCCESS, name=" << name
                      << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << exportObj.req.requestId;
        UbseMemShareBorrowExportObj callbackObj = *existingObj;
        callbackObj.req.requestNodeId = exportObj.req.requestNodeId;
        return ForwardExportCallbackToGlobal(callbackObj);
    }

    UbseMemShareBorrowExportObj storeObj = exportObj;
    storeObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    storeObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    ledger.GetDebtMap<UbseMemShareBorrowExportObj>().PutResource(exportNodeId, name, storeObj);

    auto ret = ForwardExportObjToExecutor(storeObj, exportNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send ExportObj to Agent, name=" << name
                       << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << exportObj.req.requestId
                       << ", " << FormatRetCode(ret);
        ledger.GetDebtMap<UbseMemShareBorrowExportObj>().RemoveResource(exportNodeId, name);
        UbseMemShareBorrowExportObj failObj = storeObj;
        failObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
        failObj.errorCode = ret;
        return ForwardExportCallbackToGlobal(failObj);
    }
    return UBSE_OK;
}

uint32_t CascadeMasterHandlerAgentBorrowExportCallback(const UbseMemShareBorrowExportObj &exportObj)
{
    auto lock = LoggingLockGuard(exportObj.req.name);
    UBSE_LOG_INFO << "name=" << exportObj.req.name
                  << ", state=" << exportObj.status.state << ", requestId=" << exportObj.returnReq.requestId;
    auto name = exportObj.req.name;
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto &ledger = UbseMemDebtLedger::GetInstance();

    if (exportObj.status.state == UBSE_MEM_EXPORT_SUCCESS) {
        UbseMemShareBorrowExportObj successObj = exportObj;
        successObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
        ledger.GetDebtMap<UbseMemShareBorrowExportObj>().PutResource(exportNodeId, name, successObj);
        UBSE_LOG_INFO << "Export success , name=" << name << ", requestNodeId=" << exportObj.req.requestNodeId
                      << ", requestId=" << exportObj.req.requestId;
        return ForwardExportCallbackToGlobal(successObj);
    }

    UBSE_LOG_ERROR << "Export failed , name=" << name << ", state=" << exportObj.status.state
                   << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << exportObj.req.requestId;
    ledger.GetDebtMap<UbseMemShareBorrowExportObj>().RemoveResource(exportNodeId, name);
    UbseMemShareBorrowExportObj failObj = exportObj;
    failObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    return ForwardExportCallbackToGlobal(failObj);
}

uint32_t GlobalMasterHandlerBorrowExportCallback(const UbseMemShareBorrowExportObj &exportObj)
{
    auto lock = LoggingLockGuard(exportObj.req.name);
    UBSE_LOG_INFO << "name=" << exportObj.req.name << ", state=" << exportObj.status.state
                  << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << exportObj.req.requestId;
    UbseMemOperationResp resp{
        .name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId, .requestId = exportObj.req.requestId};

    GlobalMasterStore store;
    if (exportObj.status.state == UBSE_MEM_EXPORT_SUCCESS) {
        store.UpdateExportState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        store.UpdateExportMemIds(exportObj);
        UBSE_LOG_INFO << "Export summary updated to SUCCESS , name=" << exportObj.req.name
                      << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << exportObj.req.requestId;
        auto copy = exportObj;
        UbseMemShmExportObjStateChangeHandler(copy);
        ShareExportFillResp(resp, exportObj);
        return BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_BORROW);
    }
    UBSE_LOG_ERROR << "Export failed , name=" << exportObj.req.name << ", state=" << exportObj.status.state
                   << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << exportObj.req.requestId;
    store.RemoveExport(exportObj);
    auto copy = exportObj;
    copy.status.state = UBSE_MEM_STATE_FAILED;
    UbseMemShmExportObjStateChangeHandler(copy);
    return BuildOperationRespWhenFail(resp, exportObj.req.name, exportObj.req.requestNodeId, "Failed to export",
                                      exportObj.errorCode, MemOperationType::SHARED_BORROW);
}

uint32_t GlobalMasterHandlerDeleteExportCallback(const UbseMemShareBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << "name=" << exportObj.req.name << ", state=" << exportObj.status.state
                  << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << exportObj.req.requestId;
    auto lock = LoggingLockGuard(exportObj.req.name);
    auto name = exportObj.req.name;
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto req = exportObj.returnReq;
    std::string requestNodeId = req.requestNodeId;
    UbseMemOperationResp resp{.name = exportObj.req.name, .requestNodeId = requestNodeId, .requestId = req.requestId};

    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        GlobalMasterStore().RemoveExport(exportObj);
        UBSE_LOG_INFO << "Export summary removed name=" << name << ", requestId=" << req.requestId;
        auto copy = exportObj;
        UbseMemShmExportObjStateChangeHandler(copy);
        if (auto ret = BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_RETURN); ret != UBSE_OK) {
            BorrowFailedAdvice(ProcessType::RETURN_FAILED, name, "SHARE_BORROW", 0, exportNodeId, "", ret,
                               MemAdvice::COMM_FAILED);
            return ret;
        }
        return UBSE_OK;
    }
    UBSE_LOG_ERROR << "unexport failed, name=" << name << ", state=" << exportObj.status.state
                   << ", requestId=" << req.requestId;
    GlobalMasterStore().UpdateExportState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    auto copy = exportObj;
    copy.status.state = UBSE_MEM_STATE_FAILED;
    UbseMemShmExportObjStateChangeHandler(copy);
    if (requestNodeId.empty()) {
        UBSE_LOG_INFO << "requestNodeId is empty name=" << name << ", requestId=" << req.requestId;
        return UBSE_ERR_INTERNAL;
    }
    if (auto ret = BuildOperationRespWhenFail(resp, name, requestNodeId, "Failed to unexport", exportObj.errorCode,
                                              MemOperationType::SHARED_RETURN);
        ret != UBSE_OK) {
        BorrowFailedAdvice(ProcessType::RETURN_FAILED, name, "SHARE_BORROW", 0, exportNodeId, "", ret,
                           MemAdvice::COMM_FAILED);
        return ret;
    }
    return UBSE_ERR_INTERNAL;
}

static uint32_t QueryFullExportForAttach(const UbseMemShareAttachReq &req,
                                         const UbseMemShareBorrowExportObj &summExport, UbseMemOperationResp &resp,
                                         UbseMemShareBorrowExportObj &fullExportObj)
{
    UbseMemDebtQueryRequest queryReq;
    queryReq.name = req.name;
    queryReq.exportNodeId = summExport.algoResult.exportNumaInfos[0].nodeId;
    std::string exportCascadeMasterId;
    auto gmRet = UbseGetCascadeMasterNodeIdByAgentNodeId(queryReq.exportNodeId, exportCascadeMasterId);
    if (gmRet != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get export Cascade Master nodeId, name=" << req.name
                       << ", requestNodeId=" << req.requestNodeId << ", requestId=" << req.requestId << ", "
                       << FormatRetCode(gmRet);
        BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "Failed to get export Cascade Master.",
                                   UBSE_ERR_INTERNAL, MemOperationType::SHARED_ATTACH);
        return UBSE_ERR_INTERNAL;
    }
    auto queryRet = QueryShareExport(queryReq, fullExportObj, true);
    if (queryRet != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query full export obj from Cascade Master, name=" << req.name
                       << ", requestNodeId=" << req.requestNodeId << ", requestId=" << req.requestId << ", "
                       << FormatRetCode(queryRet);
        BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "Failed to query export obj.", UBSE_ERR_NOT_EXIST,
                                   MemOperationType::SHARED_ATTACH);
        return UBSE_ERR_NOT_EXIST;
    }
    if (fullExportObj.algoResult.exportNumaInfos.empty() || fullExportObj.status.exportObmmInfo.empty()) {
        UBSE_LOG_ERROR << "Invalid export obj from Cascade Master, name=" << req.name
                       << ", requestNodeId=" << req.requestNodeId << ", requestId=" << req.requestId;
        BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "exportObj is invalid.", UBSE_ERR_NOT_EXIST,
                                   MemOperationType::SHARED_ATTACH);
        return UBSE_ERR_NOT_EXIST;
    }
    return UBSE_OK;
}

uint32_t UbseMemShareAttachClos(const UbseMemShareAttachReq &req, UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "UbseMemShareAttachClos, name=" << req.name << ", requestNodeId=" << req.requestNodeId
                  << ", importNodeId=" << req.importNodeId << ", requestId=" << req.requestId;
    auto lock = LoggingLockGuard(req.name + "_" + req.requestNodeId);
    // deleteAndBorrowLock 避免attach时获取到export对象后, delete紧跟着对export进行删除,导致单边导入账本
    auto deleteAndBorrowLock = LoggingLockGuard(req.name, LoggingLockGuard::LockType::READ);     
    resp.requestId = req.requestId;
    resp.name = req.name;
    if (req.importNodeId.empty()) {
        return BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "attach with no node is valid.",
                                          UBSE_ERR_SHM_NODE_EMPTY, MemOperationType::SHARED_ATTACH);
    }

    UbseNodeControllerLockMgr::WriteLock(ClusterHandlerKey);

    GlobalMasterStore store;

    UbseMemShareBorrowExportObj summExport;
    if (store.LoadExport(req.name, summExport) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query export item from global ledger, name=" << req.name;
        UbseNodeControllerLockMgr::WriteUnLock(ClusterHandlerKey);
        return BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "exportObj not found in global ledger.",
                                          UBSE_ERR_NOT_EXIST, MemOperationType::SHARED_ATTACH);
    }

    std::vector<UbseMemShareBorrowImportObj> importObjs;
    store.LoadAllImports(req.name, importObjs);
    std::vector<UbseMemShareBorrowExportObj> exportObjs = {summExport};
    UbseMemShareBorrowImportObj existImportObj;

    if (auto ret = PrepareAttachPreconditions(req, exportObjs, importObjs, existImportObj, resp); ret != UBSE_OK) {
        UbseNodeControllerLockMgr::WriteUnLock(ClusterHandlerKey);
        return ret;
    }

    UbseMemShareBorrowExportObj fullExportObj;
    if (auto ret = QueryFullExportForAttach(req, summExport, resp, fullExportObj); ret != UBSE_OK) {
        UbseNodeControllerLockMgr::WriteUnLock(ClusterHandlerKey);
        return ret;
    }

    UbseMemShareBorrowImportObj importObj;
    if (auto ret = PrepareAttachImport(req, store, fullExportObj, resp, importObj); ret != UBSE_OK) {
        UbseNodeControllerLockMgr::WriteUnLock(ClusterHandlerKey);
        return BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "Failed to get cna info when import.", ret,
                                          MemOperationType::SHARED_ATTACH);
    }
    UbseNodeControllerLockMgr::WriteUnLock(ClusterHandlerKey);

    auto ret = ForwardImportObjToCascade(importObj);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send ImportObj to Cascade Master, name=" << req.name
                       << ", requestNodeId=" << req.requestNodeId << ", requestId=" << req.requestId << ", "
                       << FormatRetCode(ret);
        store.RemoveImport(importObj);
        return BuildOperationRespWhenFail(resp, req.name, req.importNodeId, "Failed to send to import Cascade Master.",
                                          UBSE_ERR_INTERNAL, MemOperationType::SHARED_ATTACH);
    }
    return UBSE_OK;
}

uint32_t CascadeMasterHandleGlobalAttachImportCallback(const UbseMemShareBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << "CascadeMasterHandleGlobalAttachImportCallback, name=" << importObj.req.name
                  << ", importNodeId=" << importObj.importNodeId;
    auto lock = LoggingLockGuard(importObj.req.name + "_" + importObj.importNodeId);
    auto name = importObj.req.name;
    auto importNodeId = importObj.importNodeId;
    auto &ledger = UbseMemDebtLedger::GetInstance();

    auto existingObj = ledger.GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(importNodeId, name);
    if (existingObj && existingObj->status.state == UBSE_MEM_IMPORT_SUCCESS) {
        UBSE_LOG_INFO << "ImportObj already exists with IMPORT_SUCCESS, name=" << name
                      << ", importNodeId=" << importNodeId;
        UbseMemShareBorrowImportObj callbackObj = *existingObj;
        callbackObj.req = importObj.req;
        return ForwardImportCallbackToGlobal(callbackObj);
    }

    UbseMemShareBorrowImportObj storeObj = importObj;
    storeObj.status.state = UBSE_MEM_IMPORT_RUNNING;
    storeObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    ledger.GetDebtMap<UbseMemShareBorrowImportObj>().PutResource(importNodeId, name, storeObj);

    auto ret = ForwardImportObjToExecutor(storeObj, importNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send ImportObj to Agent, name=" << name << ", importNodeId=" << importNodeId
                       << ", " << FormatRetCode(ret);
        ledger.GetDebtMap<UbseMemShareBorrowImportObj>().RemoveResource(importNodeId, name);
        UbseMemShareBorrowImportObj failObj = storeObj;
        failObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
        failObj.errorCode = ret;
        return ForwardImportCallbackToGlobal(failObj);
    }
    return UBSE_OK;
}

uint32_t CascadeMasterHandlerAgentAttachImportCallback(const UbseMemShareBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << "CascadeMasterHandlerAgentAttachImportCallback, name=" << importObj.req.name
                  << ", state=" << importObj.status.state;
    auto lock = LoggingLockGuard(importObj.req.name + "_" + importObj.importNodeId);
    auto name = importObj.req.name;
    auto importNodeId = importObj.importNodeId;
    auto &ledger = UbseMemDebtLedger::GetInstance();

    if (importObj.status.state == UBSE_MEM_IMPORT_SUCCESS) {
        UbseMemShareBorrowImportObj successObj = importObj;
        successObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
        ledger.GetDebtMap<UbseMemShareBorrowImportObj>().PutResource(importNodeId, name, successObj);
        UBSE_LOG_INFO << "Import success, name=" << name;
        return ForwardImportCallbackToGlobal(successObj);
    }

    UBSE_LOG_ERROR << "Import failed, name=" << name << ", state=" << importObj.status.state;
    ledger.GetDebtMap<UbseMemShareBorrowImportObj>().RemoveResource(importNodeId, name);
    UbseMemShareBorrowImportObj failObj = importObj;
    failObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    return ForwardImportCallbackToGlobal(failObj);
}

uint32_t CascadeMasterHandlerAgentDetachImportCallback(const UbseMemShareBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << "CascadeMasterHandlerAgentDetachImportCallback, name=" << importObj.req.name
                  << ", state=" << importObj.status.state;
    auto lock = LoggingLockGuard(importObj.req.name + "_" + importObj.importNodeId);                  
    auto name = importObj.req.name;
    auto importNodeId = importObj.importNodeId;
    auto &ledger = UbseMemDebtLedger::GetInstance();

    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        EraseShareImport(importObj);
        UBSE_LOG_INFO << "Import destroyed, name=" << name;
        UbseMemShareBorrowImportObj successObj = importObj;
        successObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
        return ForwardDetachCallbackToGlobal(successObj);
    }

    UBSE_LOG_ERROR << "Import detach failed, name=" << name << ", state=" << importObj.status.state;
    auto storedObjPtr = ledger.GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(importNodeId, name);
    if (storedObjPtr) {
        UbseMemShareBorrowImportObj rollbackObj = *storedObjPtr;
        rollbackObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
        rollbackObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
        ledger.GetDebtMap<UbseMemShareBorrowImportObj>().PutResource(importNodeId, name, rollbackObj);
    }
    UbseMemShareBorrowImportObj failObj = importObj;
    failObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    failObj.errorCode = importObj.errorCode;
    return ForwardDetachCallbackToGlobal(failObj);
}

uint32_t GlobalMasterHandlerAttachImportObjCallback(const UbseMemShareBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << "GlobalMasterHandlerAttachImportObjCallback, name=" << importObj.req.name
                  << ", state=" << importObj.status.state;
    auto lock = LoggingLockGuard(importObj.req.name + "_" + importObj.importNodeId);                   
    auto name = importObj.req.name;
    UbseMemOperationResp resp{
        .name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId, .requestId = importObj.req.requestId};

    if (importObj.status.state == UBSE_MEM_IMPORT_SUCCESS) {
        GlobalMasterStore store;
        store.UpdateImportState(importObj, UBSE_MEM_IMPORT_SUCCESS);
        store.UpdateImportMemIds(importObj);
        UBSE_LOG_INFO << "Import summary updated to SUCCESS, name=" << name;
        auto copy = importObj;
        UbseMemShmImportObjStateChangeHandler(copy);
        ShareImportFillResp(resp, importObj);
        return BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_ATTACH);
    }

    UBSE_LOG_ERROR << "Import failed, name=" << name;
    GlobalMasterStore().RemoveImport(importObj);
    auto copy = importObj;
    copy.status.state = UBSE_MEM_STATE_FAILED;
    UbseMemShmImportObjStateChangeHandler(copy);
    return BuildOperationRespWhenFail(resp, importObj.req.name, importObj.req.requestNodeId, "Failed to import",
                                      importObj.errorCode, MemOperationType::SHARED_ATTACH);
}

uint32_t GlobalMasterHandlerDetachImportCallback(const UbseMemShareBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << "GlobalMasterHandlerDetachImportCallback, name=" << importObj.req.name
                  << ", state=" << importObj.status.state;
    auto lock = LoggingLockGuard(importObj.req.name + "_" + importObj.importNodeId);                  
    auto name = importObj.req.name;
    UbseMemOperationResp resp{
        .name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId, .requestId = importObj.req.requestId};

    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        GlobalMasterStore().RemoveImport(importObj);
        UBSE_LOG_INFO << "Import summary removed (detach callback SUCCESS), name=" << name;
        auto copy = importObj;
        UbseMemShmImportObjStateChangeHandler(copy);
        return BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::SHARED_DETACH);
    }

    UBSE_LOG_ERROR << "Detach failed at CascadeMaster (callback), name=" << name
                   << ", state=" << importObj.status.state;
    GlobalMasterStore().UpdateImportState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    return BuildOperationRespWhenFail(resp, name, importObj.req.requestNodeId, "Failed to detach", importObj.errorCode,
                                      MemOperationType::SHARED_DETACH);
}

uint32_t UbseMemShareCascadeDetach(const UbseMemShareDetachReq &req, UbseMemOperationResp &resp,
                                   const std::string &realRequestNodeId)
{
    UBSE_LOG_INFO << "UbseMemShareCascadeDetach, name=" << req.name << ", requestNodeId=" << req.requestNodeId;
    auto lock = LoggingLockGuard(req.name + "_" + req.unImportNodeId);
    resp.requestId = req.requestId;
    resp.name = req.name;

    if (req.unImportNodeId.empty()) {
        return ShareDetachFailed(req, resp, "Detach with no node is valid.", UBSE_ERR_SHM_NODE_EMPTY,
                                 MemAdvice::NODE_IN_MAINTENANCE);
    }

    UbseMemShareBorrowImportObj importObj{};
    if (auto ret = PrepareDetachPreconditions(req, realRequestNodeId, resp, importObj); ret != UBSE_OK) {
        return ret;
    }

    auto ret = ForwardDetachReqToPd(req);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send Detach to PD Master , name=" << req.name;
        return ShareDetachFailed(req, resp, "Failed to send Detach to PD Master.", UBSE_ERR_INTERNAL,
                                 MemAdvice::COMM_FAILED);
    }
    return UBSE_OK;
}

uint32_t UbseMemShareManageDetach(const UbseMemShareDetachReq &req, UbseMemOperationResp &resp,
                                  const std::string &realRequestNodeId)
{
    UBSE_LOG_INFO << "name=" << req.name << ", requestNodeId=" << req.requestNodeId
                  << ", cascadeMasterNodeId=" << realRequestNodeId << ", unImportNodeId=" << req.unImportNodeId
                  << ", requestId=" << req.requestId;
    auto lock = LoggingLockGuard(req.name + "_" + req.requestNodeId);
    resp.requestId = req.requestId;
    resp.name = req.name;
    if (!UbseDetachNodeIdInCascadeDomain(req.unImportNodeId, realRequestNodeId)) {
        UBSE_LOG_ERROR << "unImportNodeId=" << req.unImportNodeId
                       << " not in cascade of cascadeMasterNodeId=" << realRequestNodeId;
        return ShareDetachFailed(req, resp, "Detach auth failed, nodeId not in PD domain.", UBSE_ERR_AUTH_FAILED,
                                 MemAdvice::UBSE_NO_OPERATION_PERMISSION);
    }
    auto ret = ForwardDetachReqToGlobal(req);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send Detach to Global Master, name=" << req.name;
        return ShareDetachFailed(req, resp, "Failed to send Detach to Global Master.", UBSE_ERR_COM_FAILED,
                                 MemAdvice::COMM_FAILED);
    }
    return UBSE_OK;
}

uint32_t UbseMemShareGlobalDetach(const UbseMemShareDetachReq &req, UbseMemOperationResp &resp,
                                  const std::string &realRequestNodeId)
{
    UBSE_LOG_INFO << "name=" << req.name << ", unImportNodeId=" << req.unImportNodeId
                  << ", realRequestNodeId=" << realRequestNodeId << ", requestId=" << req.requestId;
    auto lock = LoggingLockGuard(req.name + "_" + req.unImportNodeId);
    resp.requestId = req.requestId;
    resp.name = req.name;
    if (!UbseCheckDetachNodeIdInManageDomain(req.unImportNodeId, realRequestNodeId)) {
        UBSE_LOG_ERROR << "detach nodeId validation failed, realRequestNodeId="
                       << realRequestNodeId << ", unImportNodeId=" << req.unImportNodeId;
        return ShareDetachFailed(req, resp, "Detach auth failed, nodeId not in global domain.", UBSE_ERR_AUTH_FAILED,
                                 MemAdvice::UBSE_NO_OPERATION_PERMISSION);
    }
    UbseMemShareBorrowImportObj importObj;
    GlobalMasterStore store;
    UbseResult getRet = store.LoadImport(req.unImportNodeId, req.name, importObj);
    if (getRet != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get import item from global ledger, name=" << req.name;
        return ShareDetachFailed(req, resp, "Import resource not found.",
                                          UBSE_ERR_SHM_NO_ATTACH, MemAdvice::RESOURCE_NOT_EXIST);
    }
    UbseMemStage memStage = GetMemStageByShareImportObjState(importObj, true);
    if (memStage == UbseMemStage::UBSE_CREATING) {
        return ShareDetachFailed(req, resp, "Import obj is in creating.", UBSE_ERR_CREATING,
                          MemAdvice::RESOURCE_OPERATION_CONFLICT);
    }
    if (memStage == UbseMemStage::UBSE_DELETING) {
        return ShareDetachFailed(req, resp, "Import obj is in deleting.", UBSE_ERR_DELETING,
                          MemAdvice::RESOURCE_OPERATION_CONFLICT);
    }
    if (!importObj.req.udsInfo.CheckPermission(req.udsInfo)) {
        UBSE_LOG_ERROR << "name=" << req.name << " auth failed,req username=" << req.udsInfo.username
                       << ", req uid=" << req.udsInfo.uid
                       << ", importObj obj username=" << importObj.req.udsInfo.username
                       << ", import obj uid=" << importObj.req.udsInfo.uid << "importObj.importNodeId"
                       << importObj.importNodeId;
        return ShareDetachFailed(req, resp, "Error auth.", UBSE_ERR_AUTH_FAILED, MemAdvice::UBSE_NO_OPERATION_PERMISSION);
    }
    importObj.importNodeId = req.unImportNodeId;
    importObj.req.name = req.name;
    store.UpdateImportState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    auto ret = ForwardDetachReqToCascade(req);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send Detach to import Cascade Master , name=" << req.name;
        store.UpdateImportState(importObj, UBSE_MEM_IMPORT_SUCCESS);
        return ShareDetachFailed(req, resp, "Failed to send detach to import Cascade Master.", UBSE_ERR_COM_FAILED, MemAdvice::COMM_FAILED);
    }
    return UBSE_OK;
}

uint32_t CascadeMasterHandleGlobalDetachImportCallback(const UbseMemShareDetachReq &req)
{
    UBSE_LOG_INFO << "name=" << req.name << ", unImportNodeId=" << req.unImportNodeId
                  << ", requestId=" << req.requestId;
    auto lock = LoggingLockGuard(req.name + "_" + req.unImportNodeId);
    auto name = req.name;
    auto importNodeId = req.unImportNodeId;
    auto &ledger = UbseMemDebtLedger::GetInstance();

    auto existingObj = ledger.GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(importNodeId, name);
    if (existingObj == nullptr) {
        UBSE_LOG_INFO << "ImportObj not found in local ledger, name=" << name << ", requestId=" << req.requestId;
        UbseMemShareBorrowImportObj failObj;
        failObj.req.name = name;
        failObj.req.requestNodeId = req.requestNodeId;
        failObj.req.requestId = req.requestId;
        failObj.importNodeId = importNodeId;
        failObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
        failObj.errorCode = UBSE_ERR_NOT_EXIST;
        return ForwardDetachCallbackToGlobal(failObj);
    }

    if (existingObj->status.state == UBSE_MEM_IMPORT_DESTROYED) {
        UBSE_LOG_INFO << "ImportObj already DESTROYED, name=" << name << ", requestId=" << req.requestId;
        UbseMemShareBorrowImportObj successObj = *existingObj;
        successObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
        return ForwardDetachCallbackToGlobal(successObj);
    }
    UbseMemShareBorrowImportObj storeObj = *existingObj;
    storeObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    storeObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    ledger.GetDebtMap<UbseMemShareBorrowImportObj>().PutResource(importNodeId, name, storeObj);

    auto ret = ForwardImportObjToExecutor(storeObj, importNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send UnImportObj to Agent , name=" << name << ", requestId=" << req.requestId;
        UbseMemShareBorrowImportObj rollbackObj = storeObj;
        rollbackObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
        rollbackObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
        rollbackObj.errorCode = ret;
        ledger.GetDebtMap<UbseMemShareBorrowImportObj>().PutResource(importNodeId, name, rollbackObj);
        UbseMemShareBorrowImportObj failObj = storeObj;
        failObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
        failObj.errorCode = ret;
        return ForwardDetachCallbackToGlobal(failObj);
    }
    return UBSE_OK;
}

static uint32_t LoadAndValidateClosReturnExportSummary(GlobalMasterStore &store, const UbseMemReturnReq &req,
                                                       UbseMemShareBorrowExportObj &summaryExportObj,
                                                       UbseMemOperationResp &resp)
{
    if (store.LoadExport(req.name, summaryExportObj) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to find export summary, name=" << req.name << ", requestId=" << req.requestId;
        BorrowFailedAdvice(ProcessType::RETURN_FAILED, req.name, "SHARE_BORROW", 0, "", "", UBSE_ERR_NOT_EXIST,
                           MemAdvice::RESOURCE_NOT_EXIST);
        BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Export resource not found.", UBSE_ERR_NOT_EXIST,
                                   MemOperationType::SHARED_RETURN);
        return UBSE_ERR_NOT_EXIST;
    }
    if (summaryExportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "Export summary has no export numa info, name=" << req.name;
        BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Export summary invalid.", UBSE_ERR_NOT_EXIST,
                                   MemOperationType::SHARED_RETURN);
        return UBSE_ERR_NOT_EXIST;
    }
    return UBSE_OK;
}

static uint32_t QueryClosReturnFullExport(const UbseMemReturnReq &req,
                                          const UbseMemShareBorrowExportObj &summaryExportObj,
                                          UbseMemShareBorrowExportObj &fullExportObj, UbseMemOperationResp &resp)
{
    UbseMemDebtQueryRequest queryReq;
    queryReq.name = req.name;
    queryReq.exportNodeId = summaryExportObj.algoResult.exportNumaInfos[0].nodeId;
    if (QueryShareExport(queryReq, fullExportObj, true) != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to query full export obj from Cascade Master, name=" << req.name
                       << ", requestId=" << req.requestId;
        BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Failed to query full export obj.",
                                   UBSE_ERR_NOT_EXIST, MemOperationType::SHARED_RETURN);
        return UBSE_ERR_NOT_EXIST;
    }
    return UBSE_OK;
}

static uint32_t ExecuteClosReturnForwardDelete(GlobalMasterStore &store, const UbseMemReturnReq &req,
                                               UbseMemShareBorrowExportObj &fullExportObj, UbseMemOperationResp &resp)
{
    PrepareReturnExport(req, store, fullExportObj);
    if (auto ret = ForwardDeleteToCascade(fullExportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send Delete to export Cascade Master, name=" << req.name;
        store.UpdateExportState(fullExportObj, UBSE_MEM_EXPORT_SUCCESS);
        BorrowFailedAdvice(ProcessType::RETURN_FAILED, req.name, "SHARE_BORROW", 0, "", "", ret,
                           MemAdvice::COMM_FAILED);
        BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Failed to send delete to export Group master.",
                                   ret, MemOperationType::SHARED_RETURN);
        return ret;
    }
    return UBSE_OK;
}

uint32_t UbseMemShareReturnClos(const UbseMemReturnReq &req, UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "UbseMemShareReturnClos, name=" << req.name << ", requestNodeId=" << req.requestNodeId
                  << ", requestId=" << req.requestId;
    auto lock = LoggingLockGuard(req.name);
    InitializeResponse(req, resp);

    GlobalMasterStore store;
    UbseMemShareBorrowExportObj summaryExportObj;
    if (auto ret = LoadAndValidateClosReturnExportSummary(store, req, summaryExportObj, resp); ret != UBSE_OK) {
        return ret;
    }
    if (auto ret = CheckReturnExportOperableState(summaryExportObj, req, resp); ret != UBSE_OK) {
        return ret;
    }
    if (auto ret = ValidateReqPermission(req, req.requestNodeId, summaryExportObj, resp); ret != UBSE_OK) {
        return ret;
    }
    if (auto ret = ValidateReturnNoActiveImports(store, req, resp); ret != UBSE_OK) {
        return ret;
    }
    UbseMemShareBorrowExportObj fullExportObj;
    if (auto ret = QueryClosReturnFullExport(req, summaryExportObj, fullExportObj, resp); ret != UBSE_OK) {
        return ret;
    }
    return ExecuteClosReturnForwardDelete(store, req, fullExportObj, resp);
}

uint32_t CascadeMasterHandlerGlobalDeleteCallback(const UbseMemShareBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << "CascadeMasterHandlerGlobalDeleteCallback, name=" << exportObj.req.name
                  << ", state=" << exportObj.status.state << ", requestId=" << exportObj.returnReq.requestId;
    auto lock = LoggingLockGuard(exportObj.req.name);
    auto name = exportObj.req.name;
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto &ledger = UbseMemDebtLedger::GetInstance();

    auto existingObj = ledger.GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(exportNodeId, name);
    if (existingObj && existingObj->status.state == UBSE_MEM_EXPORT_DESTROYED) {
        UBSE_LOG_INFO << "ExportObj already DESTROYED, name=" << name
                      << ", requestId=" << exportObj.returnReq.requestId;
        UbseMemShareBorrowExportObj callbackObj = *existingObj;
        ledger.GetDebtMap<UbseMemShareBorrowExportObj>().RemoveResource(exportNodeId, name);
        return ForwardDeleteCallbackToGlobal(callbackObj);
    }

    UbseMemShareBorrowExportObj storeObj = exportObj;
    storeObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    storeObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    ledger.GetDebtMap<UbseMemShareBorrowExportObj>().PutResource(exportNodeId, name, storeObj);

    auto ret = ForwardExportObjToExecutor(storeObj, exportNodeId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send UnExport to Agent, name=" << name
                       << ", requestId=" << exportObj.returnReq.requestId;
        UbseMemShareBorrowExportObj rollbackObj = storeObj;
        rollbackObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
        rollbackObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
        ledger.GetDebtMap<UbseMemShareBorrowExportObj>().PutResource(exportNodeId, name, rollbackObj);
        UbseMemShareBorrowExportObj failObj = storeObj;
        failObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
        failObj.errorCode = ret;
        return ForwardDeleteCallbackToGlobal(failObj);
    }
    return UBSE_OK;
}

uint32_t CascadeMasterHandlerAgentDeleteExportCallback(const UbseMemShareBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << "name=" << exportObj.req.name << ", state=" << exportObj.status.state << ", requestId=" << exportObj.returnReq.requestId;
    auto lock = LoggingLockGuard(exportObj.req.name);
    auto name = exportObj.req.name;
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto &ledger = UbseMemDebtLedger::GetInstance();

    auto storedObjPtr = ledger.GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(exportNodeId, name);
    UbseMemShareBorrowExportObj callbackObj = exportObj;
    if (storedObjPtr) {
        callbackObj.returnReq = storedObjPtr->returnReq;
    }

    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        UBSE_LOG_INFO << "Export destroyed , name=" << name << ", requestId=" << exportObj.returnReq.requestId;
        EraseShareExport(exportObj);
        return ForwardDeleteCallbackToGlobal(callbackObj);
    }

    UBSE_LOG_ERROR << "Export delete failed , name=" << name << ", state=" << exportObj.status.state
                   << ", requestId=" << exportObj.returnReq.requestId;
    UbseMemShareBorrowExportObj rollbackObj = exportObj;
    rollbackObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    rollbackObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    ledger.GetDebtMap<UbseMemShareBorrowExportObj>().PutResource(exportNodeId, name, rollbackObj);
    callbackObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    callbackObj.errorCode = exportObj.errorCode;
    return ForwardDeleteCallbackToGlobal(callbackObj);
}

// ==================== 分层转发编排（解析目标节点 + 调转发原子 + 失败响应） ====================

void CascadeMasterSendBorrowReqToGlobalMaster(const UbseMemShareBorrowReq &req, UbseMemOperationResp &resp)
{
    if (ForwardBorrowReqToGlobal(req) != UBSE_OK) {
        BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Failed to send borrow req to Global Master",
                                   UBSE_ERR_COM_FAILED, MemOperationType::SHARED_BORROW);
    }
}

void CascadeMasterSendAttachReqToGlobalMaster(const UbseMemShareAttachReq &req, UbseMemOperationResp &resp)
{
    if (ForwardAttachReqToGlobal(req) != UBSE_OK) {
        BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Failed to send attach req to Global Master",
                                   UBSE_ERR_COM_FAILED, MemOperationType::SHARED_ATTACH);
    }
}

void CascadeMasterSendReturnReqToGlobalMaster(const UbseMemReturnReq &req, UbseMemOperationResp &resp)
{
    if (ForwardReturnReqToGlobal(req) != UBSE_OK) {
        BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Failed to send return req to Global Master",
                                   UBSE_ERR_COM_FAILED, MemOperationType::SHARED_RETURN);
    }
}

} // namespace ubse::mem::controller
