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
#include "ubse_mem_controller_addr_api.h"

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
#include "ubse_mem_controller_msg.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_mem_decoder_utils.h"
#include "ubse_mem_scheduler_impl.h"
#include "ubse_mem_sign_verifier.h"
#include "ubse_node_controller_util.h"
#include "../logging_lock_guard.h"
#include "../message/ubse_mem_addr_borrow_exportobj_simpo.h"
#include "../message/ubse_mem_addr_borrow_importobj_simpo.h"
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
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;

bool NodeControllerTryReadLock(const UbseMemAddrBorrowReq& req)
{
    if (req.importNodeId == req.exportNodeId) {
        UBSE_LOG_ERROR << "req.importNodeId=" << req.importNodeId << ", req.exportNodeId=" << req.exportNodeId;
        return false;
    }
    UbseNodeControllerLockMgr::ReadLock(req.importNodeId);
    if (!UbseNodeControllerLockMgr::TryReadLock(req.exportNodeId)) {
        UbseNodeControllerLockMgr::ReadUnLock(req.importNodeId);
        UBSE_LOG_ERROR << "TryReadLock exportNode " << req.exportNodeId << " failed.";
        return false;
    }
    return true;
}

void NodeControllerReadUnLock(const UbseMemAddrBorrowReq& req)
{
    UbseNodeControllerLockMgr::ReadUnLock(req.exportNodeId);
    UbseNodeControllerLockMgr::ReadUnLock(req.importNodeId);
}

void ConstructAddrObjs(UbseMemAddrBorrowImportObj& importObj, UbseMemAddrBorrowExportObj& exportObj,
                       const UbseMemAddrBorrowReq& req)
{
    importObj.req = req;
    importObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    importObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.req = req;
    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    exportObj.algoResult = importObj.algoResult;
}

size_t GetAddrReqSize(const UbseMemAddrBorrowReq& req)
{
    size_t reqSize = 0;
    for (auto& addr : req.exportAddrList) {
        reqSize += addr.size;
    }
    return reqSize;
}

UbseResult AgentSendAddrExportObj(const std::shared_ptr<UbseComModule>& comModule, SendParam& sendParam,
                                  UbseMemAddrBorrowExportobjSimpoPtr& ptr, UbseBaseMessagePtr& ubseResponsePtr,
                                  const UbseMemAddrBorrowExportObj& exportObj)
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
        UBSE_LOG_ERROR << "Send to exportObj, name=" << exportObj.req.name
                       << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << exportObj.req.requestId
                       << ", masterNodeId=" << sendParam.GetRemoteId() << " failed, " << FormatRetCode(ret);
        retryCount++;
        sleep(SEND_RETRY_DURATION);
    }
    return ret;
}

UbseResult SendAddrExportObj(const UbseMemAddrBorrowExportObj& exportObj, const bool isMaster,
                             const std::string& nodeId = "")
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule.";
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_ADDR_BORROW_EXPORT_OBJ_CALLBACK));
    UbseMemAddrBorrowExportobjSimpoPtr ptr = new (std::nothrow) UbseMemAddrBorrowExportobjSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new UbseMemAddrBorrowExportobjSimpoPtr.";
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemAddrBorrowExportobj(exportObj);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    // 主节点向从履行侧发送
    UbseResult ret = UBSE_OK;
    if (isMaster) {
        for (int i = 0; i < SEND_RETRY_TIMES; i++) {
            ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
            if (ret == UBSE_OK) {
                UBSE_LOG_INFO << "Success to send exportObj, name=" << exportObj.req.name
                              << ", requestNodeId=" << exportObj.req.requestNodeId;
                return UBSE_OK;
            }
            UBSE_LOG_ERROR << "Failed to Send to exportObj, name=" << exportObj.req.name
                           << ", requestNodeId=" << exportObj.req.requestNodeId << ", wait to retry";
            sleep(SEND_RETRY_DURATION);
        }
        UBSE_LOG_ERROR << "Failed to Send to exportObj, name=" << exportObj.req.name
                       << ", requestNodeId=" << exportObj.req.requestNodeId;
        return ret;
    }

    // 履行侧发回
    return AgentSendAddrExportObj(comModule, sendParam, ptr, ubseResponsePtr, exportObj);
}

uint32_t DoUbseMemAddrBorrow(const std::string& exportKey, const UbseMemAddrBorrowReq& req, UbseMemOperationResp& resp)
{
    UbseMemAddrBorrowImportObj importObj{};
    importObj.req.trustRingData.ClearReqSignedDataMemory(); // 清除import对象里请求签名信息
    importObj.status.state = UBSE_MEM_SCHEDULING;
    importObj.req = req;
    UbseMemAddrBorrowExportObj exportObj{};
    // 算法判断是否成环
    if (!NodeControllerTryReadLock(req)) {
        BorrowFailedAdvice({MemFault::BORROW_SCHEDULE_FAILED, req.name, MemType::ADDR, GetAddrReqSize(req),
                            req.exportNodeId, req.importNodeId, req.requestNodeId});
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "node is online.", UBSE_ERR_INTERNAL,
                                          MemOperationType::ADDR_BORROW);
    }
    if (auto ret = SchedulerImpl::GetInstance().MemoryObjChangeHandler(importObj); ret != UBSE_OK) {
        NodeControllerReadUnLock(req);
        UBSE_LOG_ERROR << "[MMC] Failed to allocate, name=" << importObj.req.name
                       << ", requestNodeId=" << importObj.req.requestNodeId << ", " << FormatRetCode(ret);
        BorrowFailedAdvice({MemFault::BORROW_SCHEDULE_FAILED, req.name, MemType::ADDR, GetAddrReqSize(req),
                            req.exportNodeId, req.importNodeId, req.requestNodeId});
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Failed to allocate", UBSE_ERR_ALLOCATE,
                                          MemOperationType::ADDR_BORROW);
    }
    NodeControllerReadUnLock(req);
    FillImportNumaPortAndChipId(importObj.algoResult.exportNumaInfos[0].nodeId,
                                importObj.algoResult.exportNumaInfos[0].socketId, req.importNodeId,
                                importObj.algoResult.importNumaInfos);
    ConstructAddrObjs(importObj, exportObj, req);
    UBSE_LOG_INFO << "[MMC] send  export obj, exportId=" << req.exportNodeId << ", importNodeId=" << req.importNodeId;

    auto& ledger = UbseMemDebtLedger::GetInstance();
    ledger.GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource(req.exportNodeId, exportKey, exportObj);
    ledger.GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(req.importNodeId, req.name, importObj);

    if (auto ret = SendAddrExportObj(exportObj, true, req.exportNodeId); ret != UBSE_OK) {
        ledger.GetDebtMap<UbseMemAddrBorrowExportObj>().RemoveResource(req.exportNodeId, exportKey);
        ledger.GetDebtMap<UbseMemAddrBorrowImportObj>().RemoveResource(req.importNodeId, req.name);
        exportObj.status.state = UBSE_MEM_STATE_FAILED;
        SchedulerImpl::GetInstance().MemoryObjChangeHandler(exportObj);
        BorrowFailedAdvice({MemFault::BORROW_MASTER_SEND_FAILED, req.name, MemType::ADDR, GetAddrReqSize(req),
                            req.exportNodeId, req.importNodeId, req.requestNodeId});
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Failed to Send export.",
                                          UBSE_ERR_INTERNAL, MemOperationType::ADDR_BORROW);
    }
    return UBSE_OK;
}

uint32_t UbseMemAddrBorrow(const UbseMemAddrBorrowReq& req, UbseMemOperationResp& resp)
{
    UBSE_LOG_INFO << "[MMC] Addr borrow begins, name=" << req.name << ", requestNodeId=" << req.requestNodeId;
    resp.requestId = req.requestId;
    if (!IsMemBorrowFeatureSupported()) {
        return BuildMemFeatureNotSupportedResp(resp, req.name, req.requestNodeId, MemOperationType::ADDR_BORROW);
    }
    // 根据pid获取sockectId, numaId
    uint32_t dstNuma{};
    uint32_t dstSocket{};
    if (WaitInitLedgerSuccess(req.importNodeId) != UBSE_OK) {
        BorrowFailedAdvice({MemFault::BORROW_IMPORT_IN_MAINTENANCE, req.name, MemType::ADDR, GetAddrReqSize(req), "",
                            req.requestNodeId, req.requestNodeId});
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "importNode is not working.",
                                          UBSE_ENGINE_ERR_IMPORT_LEDGERING, MemOperationType::ADDR_BORROW);
    }
    auto ret = GetNumaInfoFromAgent(req.exportNodeId, req.exportPid, dstNuma, dstSocket);
    if (ret != UBSE_OK) {
        BorrowFailedAdvice({MemFault::BORROW_CHECK_FAILED, req.name, MemType::ADDR, GetAddrReqSize(req), "",
                            req.requestNodeId, req.requestNodeId});
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Failed to get DstNuma by pid",
                                          UBSE_ERR_INTERNAL, MemOperationType::ADDR_BORROW);
    }
    auto copyReq = req;
    copyReq.dstNuma = dstNuma;
    copyReq.dstSocket = dstSocket;
    auto exportKey = GenerateExportObjKey(req.name, req.importNodeId);
    auto lock = LoggingLockGuard(exportKey);
    // Addr指定了exportNodeId  和 importNodeId
    auto errCode = CheckAddrResourceState(req.name, req.importNodeId);
    if (errCode != UBSE_ERR_NOT_EXIST) {
        BorrowFailedAdvice({MemFault::BORROW_NAME_EXIST, req.name, MemType::ADDR, GetAddrReqSize(req), "",
                            req.requestNodeId, req.requestNodeId});
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Resource Exist.", UBSE_ERR_EXISTED,
                                          MemOperationType::ADDR_BORROW);
    }

    return DoUbseMemAddrBorrow(exportKey, copyReq, resp);
}

void AddrExportUpdateState(UbseMemAddrBorrowExportObj& exportObj, const UbseMemState& state)
{
    exportObj.status.state = state;
    auto exportKey = GenerateExportObjKey(exportObj.req.name, exportObj.req.importNodeId);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource(exportObj.req.exportNodeId,
                                                                                          exportKey, exportObj);
}

void EraseAddrImport(const UbseMemAddrBorrowImportObj& importObj)
{
    auto name = importObj.req.name;
    auto importNodeId = importObj.req.importNodeId;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().RemoveResource(importNodeId, name);
}

uint32_t AddrExportRollback(UbseMemOperationResp& resp, UbseMemAddrBorrowExportObj& exportObj,
                            UbseMemAddrBorrowImportObj& importObj, const std::string& exportNodeId,
                            const std::string& name)
{
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    AddrExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
    EraseAddrImport(importObj);
    importObj.status.state = UBSE_MEM_STATE_FAILED;
    SchedulerImpl::GetInstance().MemoryObjChangeHandler(importObj); // 通知算法
    BuildOperationRespWhenFail(resp, name, exportObj.req.requestNodeId, "Failed to import", UBSE_ERR_INTERNAL,
                               MemOperationType::ADDR_BORROW);
    return SendAddrExportObj(exportObj, true, exportNodeId);
}

UbseResult AgentSendAddrImportObj(const std::shared_ptr<UbseComModule>& comModule, SendParam& sendParam,
                                  UbseMemAddrBorrowImportobjSimpoPtr& ptr, UbseBaseMessagePtr& ubseResponsePtr,
                                  const UbseMemAddrBorrowImportObj& importObj)
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
        UBSE_LOG_ERROR << "Send to importObj, name=" << importObj.req.name
                       << ", requestNodeId=" << importObj.req.requestNodeId << ", requestId=" << importObj.req.requestId
                       << ", masterNodeId=" << sendParam.GetRemoteId() << " failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult SendAddrImportObj(const UbseMemAddrBorrowImportObj& importObj, const bool isMaster,
                             const std::string& nodeId = "")
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule.";
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_ADDR_BORROW_IMPORT_OBJ_CALLBACK));
    UbseMemAddrBorrowImportobjSimpoPtr ptr = new (std::nothrow) UbseMemAddrBorrowImportobjSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemAddrBorrowImportobj(importObj);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    // 主节点向从履行侧发送
    UbseResult ret = UBSE_OK;
    if (isMaster) {
        for (int i = 0; i < SEND_RETRY_TIMES; i++) {
            ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
            if (ret == UBSE_OK) {
                UBSE_LOG_INFO << "Success to send importObj, name=" << importObj.req.name
                              << ", requestNodeId=" << importObj.req.requestNodeId;
                return UBSE_OK;
            }
            UBSE_LOG_ERROR << "Failed to Send to importObj, name=" << importObj.req.name
                           << ", requestNodeId=" << importObj.req.requestNodeId << ", wait to retry";
            sleep(SEND_RETRY_DURATION);
        }
        UBSE_LOG_ERROR << "Failed to Send to importObj, name=" << importObj.req.name
                       << ", requestNodeId=" << importObj.req.requestNodeId;
        return ret;
    }

    // 履行侧发回
    return AgentSendAddrImportObj(comModule, sendParam, ptr, ubseResponsePtr, importObj);
}

void EraseAddrExport(const UbseMemAddrBorrowExportObj& exportObj)
{
    auto name = exportObj.req.name;
    auto exportNodeId = exportObj.req.exportNodeId;
    auto exportKey = GenerateExportObjKey(name, exportObj.req.importNodeId);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().RemoveResource(exportNodeId, exportKey);
}

void AddrImportUpdateState(UbseMemAddrBorrowImportObj& importObj, const UbseMemState& state)
{
    importObj.status.state = state;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().PutResource(
        importObj.req.importNodeId, importObj.req.name, importObj);
}

uint32_t FilterSocketId(UbseMemBorrowImportBaseObj& importObj)
{
    std::vector<account::UbseNumaNodeInfo> numaNodeInfos{};
    UbseResult ret = ubse::mem::account::UbseAllNumaInfo(numaNodeInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MMC] Get All Numa Info Failed, " << FormatRetCode(ret);
        return ret;
    }
    for (auto& item : importObj.algoResult.exportNumaInfos) {
        for (const auto& numaInfo : numaNodeInfos) {
            if (item.nodeId == numaInfo.nodeId && item.numaId == numaInfo.numaId) {
                item.socketId = static_cast<int>(numaInfo.socketId);
                break;
            }
        }
    }
    return UBSE_OK;
}

void FillAddrImportObjAfterExportSuccess(const UbseMemAddrBorrowExportObj& exportObj,
                                         UbseMemAddrBorrowImportObj& importObj)
{
    importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    importObj.exportObmmInfo = exportObj.status.exportObmmInfo;
    importObj.algoResult.exportNumaInfos = exportObj.algoResult.exportNumaInfos;
    importObj.algoResult.importNumaInfos = exportObj.algoResult.importNumaInfos;
    importObj.algoResult.attachSocketId = exportObj.algoResult.attachSocketId;
    importObj.req.trustRingData.lendSignedDatas = exportObj.req.trustRingData.lendSignedDatas;
    FilterSocketId(importObj);
}

uint32_t AddrExportExpectSuccessCallback(UbseMemOperationResp& resp, UbseMemAddrBorrowExportObj& exportObj,
                                         UbseMemAddrBorrowImportObj& importObj, const std::string& exportNodeId,
                                         const std::string& importNodeId, const std::string& name)
{
    if (!HasAgentAlreadyReported<UbseMemAddrBorrowExportObj>(
            GenerateExportObjKey(exportObj.req.name, exportObj.req.importNodeId), exportNodeId,
            &UbseMemAddrBorrowExportObj::isCreateReportReceived)) {
        UBSE_LOG_INFO << "No need to callback for export created, name=" << name
                      << ", requestId=" << exportObj.req.requestId;
        return UBSE_OK;
    }
    if (exportObj.status.state == UBSE_MEM_EXPORT_SUCCESS) { // 导出成功 开始导入
        UBSE_LOG_INFO << "Addr export expect success callback. name=" << name;
        AddrExportUpdateState(exportObj, exportObj.status.state);
        FillAddrImportObjAfterExportSuccess(exportObj, importObj);
        auto ret = GetCnaInfoWhenImport(exportNodeId, importNodeId, importObj, true);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to get cna info when inport, " << FormatRetCode(ret);
            BorrowFailedAdvice({MemFault::BORROW_FAULT_INTERNAL, name, MemType::ADDR, GetAddrReqSize(importObj.req),
                                exportNodeId, importNodeId, importNodeId});
            return AddrExportRollback(resp, exportObj, importObj, exportNodeId, name);
        }
        importObj.isCreateReportReceived = false;
        AddrImportUpdateState(importObj, UBSE_MEM_IMPORT_RUNNING);
        SchedulerImpl::GetInstance().MemoryObjChangeHandler(exportObj);
        if (ret = SendAddrImportObj(importObj, true, importNodeId); ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to send addr import. name=" << name;
            BorrowFailedAdvice({MemFault::BORROW_MASTER_SEND_FAILED, name, MemType::ADDR, GetAddrReqSize(importObj.req),
                                exportNodeId, importNodeId, importNodeId});
            return AddrExportRollback(resp, exportObj, importObj, exportNodeId, name);
        }
        return UBSE_OK;
    }
    // 导出失败，删除导入/导出记录
    UBSE_LOG_INFO << "[MMC] import failed, start to rollback with deleting export and import";
    EraseAddrExport(exportObj);
    EraseAddrImport(importObj);
    importObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    SchedulerImpl::GetInstance().MemoryObjChangeHandler(importObj);
    return BuildOperationRespWhenFail(resp, name, exportObj.req.requestNodeId, "Failed to export.", exportObj.errorCode,
                                      MemOperationType::ADDR_BORROW);
}

uint32_t AddrExportExpectDestroyMasterCallback(UbseMemAddrBorrowExportObj& exportObj, UbseMemOperationResp& resp,
                                               const std::string& name)
{
    if (!HasAgentAlreadyReported<UbseMemAddrBorrowExportObj>(
            GenerateExportObjKey(exportObj.req.name, exportObj.req.importNodeId), exportObj.req.exportNodeId,
            &UbseMemAddrBorrowExportObj::isDestroyedReportReceived)) {
        UBSE_LOG_INFO << "No need to callback for export destroyed, name=" << name
                      << ", requestId=" << exportObj.req.requestId;
        return UBSE_OK;
    }
    UbseMemAddrBorrowImportObj importObj{};
    auto returnKey = GenerateExportObjKey(name, exportObj.req.importNodeId);
    auto req = exportObj.returnReq;
    std::string requestNodeId = req.requestNodeId;
    resp.requestNodeId = requestNodeId;
    resp.requestId = req.requestId;
    if (exportObj.status.state == UBSE_MEM_EXPORT_SUCCESS) {
        // 归还失败，后续由对账处理
        AddrExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        // requestNodeId为空则当前场景为对账删除导出账本或者借用失败回滚
        if (requestNodeId.empty()) {
            return UBSE_OK;
        }
        if (auto ret = BuildOperationRespWhenFail(resp, name, requestNodeId, "Failed to unexport.", exportObj.errorCode,
                                                  MemOperationType::ADDR_BORROW);
            ret != UBSE_OK) {
            BorrowFailedAdvice({MemFault::RETURN_MASTER_SEND_FAILED, name, MemType::ADDR, 0, exportObj.req.exportNodeId,
                                requestNodeId, requestNodeId});
            return ret;
        }
        return UBSE_OK;
    }
    // 归还成功,删除导出对象/导入对象
    // 导入对象在unimport时，已经删掉。如还存在，就是删除单导出时，对账将导入账本重新加入主节点
    EraseAddrExport(exportObj);
    UBSE_LOG_INFO << "[MMC] return success";
    SchedulerImpl::GetInstance().MemoryObjChangeHandler(exportObj); // 通知算法
    // requestNodeId为空则当前场景为对账删除导出账本或者借用失败回滚
    if (requestNodeId.empty()) {
        return UBSE_OK;
    }
    if (auto ret = BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::ADDR_BORROW); ret != UBSE_OK) {
        BorrowFailedAdvice({MemFault::RETURN_MASTER_SEND_FAILED, name, MemType::ADDR, 0, exportObj.req.exportNodeId,
                            requestNodeId, requestNodeId});
        return ret;
    }
    return UBSE_OK;
}

uint32_t AddrExportMasterCallback(const std::string& exportNodeId, UbseMemAddrBorrowExportObj& exportObj,
                                  const std::string& importNodeId, const std::string& name)
{
    UBSE_LOG_INFO << "Addr export master callback, name=" << name << ", state=" << exportObj.status.state
                  << ", importNodeId=" << importNodeId;
    auto exportKey = GenerateExportObjKey(name, exportObj.req.importNodeId);
    auto lock = LoggingLockGuard(exportKey);
    UbseMemOperationResp resp{.name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId};
    // 归还逻辑
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_DESTROYED) {
        return AddrExportExpectDestroyMasterCallback(exportObj, resp, name);
    }
    auto importObjPtr =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().GetResource(importNodeId, name);
    if (!importObjPtr) {
        UBSE_LOG_ERROR << "Failed to find import obj, name=" << name;
        return UBSE_ERROR;
    }
    auto importObj = *importObjPtr;
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_SUCCESS) {
        return AddrExportExpectSuccessCallback(resp, exportObj, importObj, exportNodeId, importNodeId, name);
    }
    return UBSE_OK;
}

uint32_t SendAddrExport(UbseMemAddrBorrowExportObj& exportObj, const std::string& name, const std::string& exportNodeId,
                        bool unexport)
{
    auto res = SendAddrExportObj(exportObj, false);
    if (res != UBSE_OK) {
        auto fault = unexport ? MemFault::RETURN_EXPORT_SEND_FAILED : MemFault::BORROW_EXPORT_SEND_FAILED;
        BorrowFailedAdvice({fault, name, MemType::ADDR, GetAddrReqSize(exportObj.req), exportNodeId,
                            exportObj.req.importNodeId, exportObj.req.importNodeId});
    }
    return res;
}

uint32_t AddrExportRunningAgentCallback(UbseMemAddrBorrowExportObj& exportObj, const std::string& name,
                                        const std::string& exportNodeId, const std::string& requestNodeId)
{
    UBSE_LOG_INFO << "Addr export running callback. name=" << name;
    auto exportKey = GenerateExportObjKey(exportObj.req.name, exportObj.req.importNodeId);
    auto existingObj =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().GetResource(exportNodeId, exportKey);
    if (existingObj && existingObj->status.state == ubse::adapter_plugins::mmi::UBSE_MEM_EXPORT_SUCCESS) {
        return UBSE_OK;
    }
    AddrExportUpdateState(exportObj, UBSE_MEM_EXPORT_RUNNING);

    if (auto ret = UbseMmiInterface::GetInstance().AddrExportExecutor(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to export, name=" << name << ", requestNodeId=" << requestNodeId;
        BorrowFailedAdvice({MemFault::BORROW_OBMM_EXPORT_FAILED, name, MemType::ADDR, GetAddrReqSize(exportObj.req),
                            exportNodeId, exportObj.req.importNodeId, requestNodeId});
        exportObj.errorCode = ret;
        // 导出失败，从节点不做存储操作，返回通知主节点。
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
        EraseAddrExport(exportObj);
        // 返回主节点 更新
        return SendAddrExport(exportObj, name, exportNodeId, false);
    }
    UBSE_LOG_INFO << "Success to export addr, name=" << name;
    UBSE_AUDIT_RUNTIME_ALLOC << name << " on Node: " << exportNodeId << " AddrMemory Export Success";
    // 高安配置下签名并验签
    if (IsHighSafety()) {
        UbseExportSignReq trustReq{exportObj.req.trustRingData.reqSignedData, "addr"};
        trustReq.exportObmmInfo = exportObj.status.exportObmmInfo;
        trustReq.trustRingId = exportObj.req.trustRingData.trustRingId;
        if (const auto ret = UbseMemSignVerifier::SignAndVerify(trustReq, exportObj.req.trustRingData.lendSignedDatas);
            ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to sign for lend information, " << FormatRetCode(ret);
            EraseAddrExport(exportObj);
            exportObj.errorCode = ret;
            exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
            return SendAddrExport(exportObj, name, exportNodeId, false);
        }
    }
    exportObj.req.trustRingData.ClearReqSignedDataMemory();
    AddrExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    return SendAddrExport(exportObj, name, exportNodeId, false);
}

uint32_t AddrExportDestroyingAgentCallback(UbseMemAddrBorrowExportObj& exportObj, const std::string& name,
                                           const std::string& exportNodeId)
{
    UBSE_LOG_INFO << "Addr export destroying callback. name=" << name;
    auto exportKey = GenerateExportObjKey(name, exportObj.req.importNodeId);
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    auto existingObj =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().GetResource(exportNodeId, exportKey);
    if (!existingObj || existingObj->status.state == UBSE_MEM_EXPORT_DESTROYED) {
        EraseAddrExport(exportObj);
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
        return SendAddrExport(exportObj, name, exportNodeId, true);
    }
    AddrExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
    if (auto ret = UbseMmiInterface::GetInstance().AddrUnExportExecutor(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to unexport name=" << name;
        BorrowFailedAdvice({MemFault::RETURN_OBMM_EXPORT_FAILED, name, MemType::ADDR, GetAddrReqSize(exportObj.req),
                            exportNodeId, exportObj.req.importNodeId, exportObj.req.importNodeId});
        exportObj.errorCode = ret;
        AddrExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        // 返回主节点 更新
        return SendAddrExport(exportObj, name, exportNodeId, true);
    }
    // 归还成功
    UBSE_LOG_INFO << "Success to unexport addr, name=" << name;
    UBSE_AUDIT_RUNTIME_DEALLOC << name << " on Node: " << exportNodeId << " AddrMemory UnExport Success";
    EraseAddrExport(exportObj);
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    return SendAddrExport(exportObj, name, exportNodeId, true);
}

uint32_t AddrExportAgentCallback(const std::string& exportNodeId, UbseMemAddrBorrowExportObj& exportObj,
                                 const std::string& name)
{
    UBSE_LOG_INFO << "Addr export agent callback name=" << name << ", state=" << exportObj.status.state;
    auto exportKey = GenerateExportObjKey(name, exportObj.req.importNodeId);
    auto lock = LoggingLockGuard(exportKey);
    auto requestNodeId = exportObj.req.requestNodeId;
    // 创建
    if (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING) {
        return AddrExportRunningAgentCallback(exportObj, name, exportNodeId, requestNodeId);
    }
    // 归还
    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING) {
        return AddrExportDestroyingAgentCallback(exportObj, name, exportNodeId);
    }
    return UBSE_OK;
}

uint32_t UbseMemAddrBorrowExportObjCallback(const UbseMemAddrBorrowExportObj& exportObj)
{
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    auto copy = exportObj;

    auto exportNodeId = exportObj.req.exportNodeId;
    auto importNodeId = exportObj.req.importNodeId;
    auto name = exportObj.req.name;
    UBSE_LOG_INFO << "[MMC] start to addr borrow export callback, exportNodeId=" << exportNodeId
                  << ", importNodeId= " << importNodeId << ", name=" << name;
    // 履行侧履行
    if (exportNodeId == currentNodeInfo.nodeId &&
        (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING)) {
        return AddrExportAgentCallback(exportNodeId, copy, name);
    }
    // 中心侧处理
    return AddrExportMasterCallback(exportNodeId, copy, importNodeId, name);
}

uint32_t DealAddrAgentImport(const std::string& requestNodeId, UbseMemAddrBorrowImportObj& importObj,
                             const std::string& name)
{
    auto importNodeId = importObj.req.importNodeId;
    std::pair<uint32_t, uint32_t> chipDiePair{};
    std::pair<uint32_t, uint32_t> remoteChipDiePair{};
    auto res = decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.attachSocketId, chipDiePair);
    res |= decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.exportNumaInfos[0].socketId,
                                                            remoteChipDiePair);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetChipAndDieId by socketId failed";
        BorrowFailedAdvice({MemFault::BORROW_FAULT_IMPORT_INTERNAL, name, MemType::ADDR, GetAddrReqSize(importObj.req),
                            importObj.req.exportNodeId, importObj.req.importNodeId, requestNodeId});
        return UBSE_ERR_INTERNAL;
    }

    decoder::utils::ImportDecoderParam importParam{};
    const uint8_t decoderId = decoder::utils::MemDecoderUtils::GetDecoderIdByPrivData(importObj.req.ubseMemPrivData);
    decoder::utils::MemDecoderUtils::SetImportDecoderParam(importParam, importObj.req.ubseMemPrivData.wrDelayComp);
    importParam.decoderIdx = decoderId;
    importParam.isHighSafety = IsHighSafety();
    importParam.trustRingData = importObj.req.trustRingData;
    importParam.type = decoder::utils::DecoderBorrowType::ADDR;
    {
        std::shared_lock lock(GetDecoderImportMutex());
        res = ImportToAddDecoderEntry(chipDiePair, importObj.exportObmmInfo, importParam, importObj.status);
        if (res != UBSE_OK) {
            UBSE_LOG_ERROR << "ImportToAddDecoderEntry failed, res=" << res;
            BorrowFailedAdvice({MemFault::BORROW_DECODE_FAILED, name, MemType::ADDR, GetAddrReqSize(importObj.req),
                                importObj.req.exportNodeId, importObj.req.importNodeId, requestNodeId});
            UnimportToDelDecoderEntry(chipDiePair, importObj.status, decoderId);
            return UBSE_ERR_INTERNAL;
        }
        importObj.req.trustRingData.ClearLendSignedDataMemory();
        AddrImportUpdateState(importObj, UBSE_MEM_IMPORT_RUNNING);
    }
    if (auto ret = UbseMmiInterface::GetInstance().AddrImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to import, name=" << name << ", requestNodeId=" << requestNodeId;
        BorrowFailedAdvice({MemFault::BORROW_OBMM_IMPORT_FAILED, name, MemType::ADDR, GetAddrReqSize(importObj.req),
                            importObj.req.exportNodeId, importObj.req.importNodeId, importObj.req.importNodeId});
        UnimportToDelDecoderEntry(chipDiePair, importObj.status, decoderId);
        EraseAddrImport(importObj);
        return ret;
    }
    UBSE_LOG_INFO << "Success to import, name=" << name;
    UBSE_AUDIT_RUNTIME_ALLOC << name << " on Node: " << importNodeId << " AddrMemory Import Success";
    return UBSE_OK;
}

uint32_t AddrImportDestroyingHandler(const std::string& requestNodeId, UbseMemAddrBorrowImportObj& importObj,
                                     const std::string& name)
{
    auto existingObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().GetResource(
        importObj.req.importNodeId, name);
    if (!existingObjPtr) {
        return UBSE_OK;
    }
    std::pair<uint32_t, uint32_t> chipDiePair{};
    auto res = decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.attachSocketId, chipDiePair);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetChipAndDieId by socketId failed";
        BorrowFailedAdvice({MemFault::RETURN_FAULT_INTERNAL, name, MemType::ADDR, GetAddrReqSize(importObj.req),
                            importObj.req.exportNodeId, importObj.req.importNodeId, importObj.req.importNodeId});
        return UBSE_MEMCONTROLLER_ERROR_UNIMPORT_FAILED;
    }
    AddrImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    if (auto ret = UbseMmiInterface::GetInstance().AddrUnImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to unimport, name=" << name;
        BorrowFailedAdvice({MemFault::RETURN_OBMM_IMPORT_FAILED, name, MemType::ADDR, GetAddrReqSize(importObj.req),
                            importObj.req.exportNodeId, importObj.req.importNodeId, importObj.req.importNodeId});
        return ret;
    }
    UBSE_LOG_INFO << "Success to unimport addr, name=" << name;
    UBSE_AUDIT_RUNTIME_DEALLOC << name << " on Node: " << importObj.req.importNodeId << " AddrMemory UnImport Success";
    const uint8_t decoderId = decoder::utils::MemDecoderUtils::GetDecoderIdByPrivData(importObj.req.ubseMemPrivData);
    UnimportToDelDecoderEntry(chipDiePair, importObj.status, decoderId);
    if (!importObj.status.decoderResult.empty()) {
        UBSE_LOG_ERROR << "UnimportToDelDecoderEntry failed";
    }
    return UBSE_OK;
}

uint32_t SendAddrImport(UbseMemAddrBorrowImportObj& importObj, const std::string& name,
                        const std::string& requestNodeId, bool unimport)
{
    auto res = SendAddrImportObj(importObj, false);
    if (res != UBSE_OK) {
        auto fault = unimport ? MemFault::RETURN_IMPORT_SEND_FAILED : MemFault::BORROW_IMPORT_SEND_FAILED;
        BorrowFailedAdvice({fault, name, MemType::ADDR, GetAddrReqSize(importObj.req), importObj.req.exportNodeId,
                            importObj.req.importNodeId, requestNodeId});
    }
    return res;
}

uint32_t AddrImportRunningHandler(const std::string& requestNodeId, UbseMemAddrBorrowImportObj& importObj,
                                  const std::string& name)
{
    auto existingObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowImportObj>().GetResource(
        importObj.req.importNodeId, importObj.req.name);
    if (existingObjPtr && existingObjPtr->status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS) {
        return UBSE_OK;
    }
    auto res = DealAddrAgentImport(requestNodeId, importObj, name);
    if (res != UBSE_OK) {
        importObj.errorCode = res;
        importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    } else {
        AddrImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    }
    return SendAddrImport(importObj, name, importObj.req.importNodeId, false);
}

uint32_t AddrImportDestroyingCallback(const std::string& requestNodeId, UbseMemAddrBorrowImportObj& importObj,
                                      const std::string& name)
{
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    auto res = AddrImportDestroyingHandler(requestNodeId, importObj, name);
    if (res != UBSE_OK) {
        importObj.errorCode = res;
        AddrImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
        UBSE_LOG_ERROR << "AddrUnImport Failed, Failed count:" << ++g_addrUnimportFailedCount
                       << ". advice: Caller should clear memory and retry. "
                       << "If failures persist, migrate the workload and restart the host.";
    } else {
        importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
        EraseAddrImport(importObj);
    }
    return SendAddrImport(importObj, name, importObj.req.importNodeId, true);
}

uint32_t AddrImportAgentCallback(const std::string& requestNodeId, UbseMemAddrBorrowImportObj& importObj,
                                 const std::string& name)
{
    UBSE_LOG_INFO << "Addr import agent callback name=" << name << ", state=" << importObj.status.state;
    auto exportKey = GenerateExportObjKey(name, importObj.req.importNodeId);
    auto lock = LoggingLockGuard(exportKey);
    UbseMemOperationResp resp{.name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId};
    if (importObj.status.state == UBSE_MEM_IMPORT_RUNNING) {
        return AddrImportRunningHandler(requestNodeId, importObj, name);
    }
    return AddrImportDestroyingCallback(requestNodeId, importObj, name);
}

uint32_t DealSendAddrUnExportObjFailed(UbseMemOperationResp& resp, const std::string& name,
                                       UbseMemAddrBorrowExportObj& exportObj)
{
    resp.name = name;
    UbseMemReturnReq req = exportObj.returnReq;
    resp.requestNodeId = req.requestNodeId;
    AddrExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    return BuildOperationRespWhenFail(resp, name, resp.requestNodeId, "Failed to send unimport.",
                                      UBSE_MEMCONTROLLER_ERROR_UNIMPORT_FAILED, MemOperationType::ADDR_RETURN);
}
uint32_t AddrRollbackAfterImportFailed(UbseMemOperationResp& resp, UbseMemAddrBorrowImportObj& importObj,
                                       const std::string& name, const std::string& importNodeId,
                                       const std::string& exportNodeId)
{
    UBSE_LOG_INFO << "[MMC] import failed, start rollback";
    auto exportKey = GenerateExportObjKey(name, importNodeId);
    EraseAddrImport(importObj);
    auto exportObjPtr =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().GetResource(exportNodeId, exportKey);
    if (exportObjPtr) {
        auto exportObj = *exportObjPtr;
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
        exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource(exportNodeId, exportKey,
                                                                                              exportObj);
        auto ret = SendAddrExportObj(exportObj, true, exportNodeId);
        // 回滚发送失败
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to send rollback addr export, requestId=" << importObj.req.requestId;
            AddrExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        }
    }
    return BuildOperationRespWhenFail(resp, name, importObj.req.requestNodeId, "Failed to import.", importObj.errorCode,
                                      MemOperationType::ADDR_BORROW);
}

uint32_t AddrImportExpectSuccessMasterCallBack(UbseMemOperationResp& resp, const std::string& exportNodeId,
                                               const std::string& name, const std::string& importNodeId,
                                               UbseMemAddrBorrowImportObj& importObj)
{
    if (!HasAgentAlreadyReported<UbseMemAddrBorrowImportObj>(importObj.req.name, importObj.req.importNodeId,
                                                             &UbseMemAddrBorrowImportObj::isCreateReportReceived)) {
        UBSE_LOG_INFO << "No need to callback for export created, name=" << name
                      << ", requestId=" << importObj.req.requestId;
        return UBSE_OK;
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_SUCCESS) { // 导入成功
        UBSE_LOG_INFO << "[MMC] addr import successful" << importObj.status.state;
        AddrImportUpdateState(importObj, importObj.status.state);
        for (const auto importResult : importObj.status.importResults) {
            resp.memIdList.push_back(importResult.memId);
        }
        if (!importObj.status.importResults.empty()) {
            resp.remoteNumaId = importObj.status.importResults[0].numaId;
        }
        SchedulerImpl::GetInstance().MemoryObjChangeHandler(importObj);
        auto ret = BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::ADDR_BORROW);
        if (ret != UBSE_OK) {
            BorrowFailedAdvice({MemFault::BORROW_MASTER_SEND_FAILED, name, MemType::ADDR, GetAddrReqSize(importObj.req),
                                exportNodeId, importNodeId, importObj.req.requestNodeId});
        }
        return ret;
    }
    UBSE_LOG_INFO << "[MMC] import failed, start rollback";
    // 导入失败 开始回滚
    return AddrRollbackAfterImportFailed(resp, importObj, name, importNodeId, exportNodeId);
}

uint32_t AddrImportExpectDestroyedMasterCallBack(UbseMemOperationResp& resp, const std::string& exportKey,
                                                 const std::string& exportNodeId, const std::string& name,
                                                 UbseMemAddrBorrowImportObj& importObj)
{
    auto req = importObj.returnReq;
    if (!HasAgentAlreadyReported<UbseMemAddrBorrowImportObj>(importObj.req.name, importObj.req.importNodeId,
                                                             &UbseMemAddrBorrowImportObj::isDestroyedReportReceived)) {
        UBSE_LOG_INFO << "No need to callback for export destroyed, name=" << name
                      << ", requestId=" << importObj.req.requestId;
        return UBSE_OK;
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        EraseAddrImport(importObj);
        SchedulerImpl::GetInstance().MemoryObjChangeHandler(importObj);
        if (auto waitResult = WaitInitLedgerSuccess(exportNodeId); waitResult != UBSE_OK) {
            BorrowFailedAdvice({MemFault::RETURN_EXPORT_IN_MAINTENANCE, name, MemType::ADDR, 0, exportNodeId,
                                req.requestNodeId, req.requestNodeId});
            return BuildOperationRespWhenFail(resp, name, req.requestNodeId, "exportNode is not working.",
                                              UBSE_ERR_UNIMPORT_SUCCESS, MemOperationType::ADDR_RETURN);
        }
        auto exportObjPtr = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().GetResource(
            exportNodeId, exportKey);
        if (exportObjPtr) {
            auto exportObj = *exportObjPtr;
            exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
            exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
            exportObj.returnReq = req;
            exportObj.isDestroyedReportReceived = false;
            UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemAddrBorrowExportObj>().PutResource(exportNodeId,
                                                                                                  exportKey, exportObj);
            if (auto ret = SendAddrExportObj(exportObj, true, exportNodeId); ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to send addr export, name=" << name;
                BorrowFailedAdvice({MemFault::RETURN_MASTER_SEND_FAILED, name, MemType::ADDR, 0, exportNodeId,
                                    req.requestNodeId, req.requestNodeId});
                return DealSendAddrUnExportObjFailed(resp, name, exportObj);
            }
            return UBSE_OK;
        } else {
            UBSE_LOG_INFO << "Success to delete single export.";
            resp.requestNodeId = req.requestNodeId;
            auto ret = BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::ADDR_RETURN);
            if (ret != UBSE_OK) {
                BorrowFailedAdvice({MemFault::RETURN_MASTER_SEND_FAILED, name, MemType::ADDR, 0, exportNodeId,
                                    req.requestNodeId, req.requestNodeId});
            }
            return ret;
        }
    }
    AddrImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    return BuildOperationRespWhenFail(resp, name, req.requestNodeId, "Failed to unimport", importObj.errorCode,
                                      MemOperationType::ADDR_RETURN);
}

uint32_t AddrImportMasterCallback(UbseMemAddrBorrowImportObj& importObj, const std::string& name)
{
    UbseMemOperationResp resp{.name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId};
    UBSE_LOG_INFO << "Addr import master callback name=" << name << ", state=" << importObj.status.state;
    auto exportKey = GenerateExportObjKey(name, importObj.req.importNodeId);
    auto lock = LoggingLockGuard(exportKey);
    auto importNodeId = importObj.req.importNodeId;
    auto exportNodeId = importObj.req.exportNodeId;
    if (importObj.status.expectState == UBSE_MEM_IMPORT_SUCCESS) {
        return AddrImportExpectSuccessMasterCallBack(resp, exportNodeId, name, importNodeId, importObj);
    }
    // 归还成功
    if (importObj.status.expectState == UBSE_MEM_IMPORT_DESTROYED) {
        return AddrImportExpectDestroyedMasterCallBack(resp, exportKey, exportNodeId, name, importObj);
    }
    return UBSE_OK;
}

uint32_t UbseMemAddrBorrowImportObjCallback(const UbseMemAddrBorrowImportObj& importObj)
{
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    auto requestNodeId = importObj.req.requestNodeId;
    auto name = importObj.req.name;

    auto copy = importObj;
    // 履行侧履行
    if (importObj.req.importNodeId == currentNodeInfo.nodeId &&
        (importObj.status.state == UBSE_MEM_IMPORT_RUNNING || importObj.status.state == UBSE_MEM_IMPORT_DESTROYING)) {
        return AddrImportAgentCallback(requestNodeId, copy, name);
    }
    // 中心侧处理
    return AddrImportMasterCallback(copy, name);
}

uint32_t DealSendAddrUnImportObjFailed(UbseMemAddrBorrowImportObj& importObj, const UbseMemReturnReq& req,
                                       UbseMemOperationResp& resp, const std::string& name)
{
    resp.name = name;
    resp.requestNodeId = req.requestNodeId;
    AddrImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    return BuildOperationRespWhenFail(resp, name, req.requestNodeId, "Failed to send importObj.",
                                      UBSE_MEMCONTROLLER_ERROR_UNIMPORT_FAILED, MemOperationType::ADDR_BORROW);
}

uint32_t AddrReturnExistImport(UbseMemAddrBorrowImportObj& importObj, UbseMemAddrBorrowExportObj& exportObj,
                               bool hasExport, const UbseMemReturnReq& req, UbseMemOperationResp& resp)
{
    auto name = req.name;
    auto requestNodeId = req.requestNodeId;
    // 有导入
    if (importObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        BorrowFailedAdvice(
            {MemFault::RETURN_NAME_NOT_EXIST, name, MemType::ADDR, 0, "", req.importNodeId, requestNodeId});
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "Resource not create.", UBSE_ERR_NOT_EXIST,
                                          MemOperationType::ADDR_BORROW);
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        if (!hasExport || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
            BorrowFailedAdvice(
                {MemFault::RETURN_NAME_NOT_EXIST, name, MemType::ADDR, 0, "", req.importNodeId, requestNodeId});
            return BuildOperationRespWhenFail(resp, name, requestNodeId, "Single import has destroyed.",
                                              UBSE_ERR_NOT_EXIST, MemOperationType::ADDR_BORROW);
        }

        exportObj.req.requestId = req.requestId;
        exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
        exportObj.isDestroyedReportReceived = false;
        auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
        if (auto ret = SendAddrExportObj(exportObj, true, exportNodeId); ret != UBSE_OK) {
            BorrowFailedAdvice({MemFault::RETURN_MASTER_SEND_FAILED, name, MemType::ADDR, 0, exportNodeId,
                                requestNodeId, requestNodeId});
            return DealSendAddrUnExportObjFailed(resp, name, exportObj);
        }
        return UBSE_OK;
    }
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    importObj.isDestroyedReportReceived = false;
    AddrImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    auto ret = SendAddrImportObj(importObj, true, importObj.req.importNodeId);
    if (ret != UBSE_OK) {
        BorrowFailedAdvice({MemFault::RETURN_MASTER_SEND_FAILED, name, MemType::ADDR, 0,
                            exportObj.algoResult.exportNumaInfos[0].nodeId, requestNodeId, requestNodeId});
        return DealSendAddrUnImportObjFailed(importObj, req, resp, name);
    }
    return UBSE_OK;
}

static uint32_t ReturnFailed(const UbseMemReturnReq& req, UbseMemOperationResp& resp, uint32_t errCode,
                             const std::string& errMsg)
{
    auto ret =
        BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, errMsg, errCode, MemOperationType::ADDR_RETURN);
    if (ret != UBSE_OK) {
        BorrowFailedAdvice({MemFault::RETURN_MASTER_SEND_FAILED, req.name, MemType::ADDR, 0, "", req.requestNodeId,
                            req.requestNodeId});
    }
    return ret;
}

uint32_t CheckAddrReturn(const UbseMemReturnReq& req, UbseMemBorrowStatus& status, std::string& errMsg,
                         UbseMemAddrBorrowExportObj& exportObj, UbseMemAddrBorrowImportObj& importObj)
{
    if (auto waitResult = WaitInitLedgerSuccess(req.importNodeId); waitResult != UBSE_OK) {
        BorrowFailedAdvice({MemFault::RETURN_IMPORT_IN_MAINTENANCE, req.name, MemType::ADDR, 0, "", req.requestNodeId,
                            req.requestNodeId});
        errMsg = "importNode is not ok";
        return waitResult;
    }
    auto [importObjPtr, exportObjPtr] =
        FindBorrowObjPair<UbseMemAddrBorrowImportObj, UbseMemAddrBorrowExportObj>(req.name, req.importNodeId);
    if (!importObjPtr && !exportObjPtr) {
        BorrowFailedAdvice(
            {MemFault::RETURN_NAME_NOT_EXIST, req.name, MemType::ADDR, 0, "", req.requestNodeId, req.requestNodeId});
        errMsg = "Memory does not exist";
        return UBSE_ERR_NOT_EXIST;
    }
    status.hasImport = importObjPtr != nullptr;
    status.hasExport = exportObjPtr != nullptr;
    UbseMemStage memStage = GetMemStageByImportObjState(importObjPtr);
    if (memStage != UbseMemStage::UBSE_CREATING && memStage != UbseMemStage::UBSE_DELETING) {
        memStage = GetMemStageByExportObjState(exportObjPtr);
    }
    if (memStage == UbseMemStage::UBSE_CREATING || memStage == UbseMemStage::UBSE_DELETING) {
        UBSE_LOG_INFO << "resource is being borrowed or returned, name=" << req.name;
        auto ret = (memStage == UbseMemStage::UBSE_CREATING) ? UBSE_ERR_CREATING : UBSE_ERR_DELETING;
        BorrowFailedAdvice(
            {MemFault::RETURN_REQ_CONFLICT, req.name, MemType::ADDR, 0, "", req.requestNodeId, req.requestNodeId});
        errMsg = "resource being borrowed or returned";
        return ret;
    }
    exportObj = status.hasExport ? *exportObjPtr : UbseMemAddrBorrowExportObj{};
    importObj = status.hasImport ? *importObjPtr : UbseMemAddrBorrowImportObj{};
    return UBSE_OK;
}

uint32_t UbseMemAddrReturn(const UbseMemReturnReq& req, UbseMemOperationResp& resp,
                           const std::string& realRequestNodeId)
{
    UBSE_LOG_INFO << "Start to addr return, name=" << req.name << ", requestNodeId=" << req.requestNodeId
                  << ", realRequestNodeId=" << realRequestNodeId;
    auto exportKey = GenerateExportObjKey(req.name, req.importNodeId);
    auto lock = LoggingLockGuard(exportKey);
    InitializeResponse(req, resp);
    if (!IsMemBorrowFeatureSupported()) {
        BorrowFailedAdvice({MemFault::RETURN_CHIP_NOT_SUPPORTED, req.name, MemType::ADDR, 0, "", req.requestNodeId,
                            req.requestNodeId});
        return BuildMemFeatureNotSupportedResp(resp, req.name, req.requestNodeId, MemOperationType::ADDR_RETURN);
    }
    UbseMemAddrBorrowExportObj exportObj{};
    UbseMemAddrBorrowImportObj importObj{};
    UbseMemBorrowStatus status{};
    std::string errMsg{};
    if (auto ret = CheckAddrReturn(req, status, errMsg, exportObj, importObj); ret != UBSE_OK) {
        return ReturnFailed(req, resp, ret, errMsg);
    }
    auto importNodeId = status.hasExport ? exportObj.req.importNodeId : importObj.req.importNodeId;
    if (importNodeId != realRequestNodeId) {
        UBSE_LOG_INFO << "Error auth, importNodeId=" << importNodeId << ", realRequestNodeId=" << realRequestNodeId;
        BorrowFailedAdvice(
            {MemFault::RETURN_AUTH_FAILED, req.name, MemType::ADDR, 0, "", importNodeId, req.requestNodeId});
        return ReturnFailed(req, resp, UBSE_ERR_AUTH_FAILED, "Error auth");
    }
    importObj.returnReq = req;
    exportObj.returnReq = req;
    if (!status.hasImport) {
        if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
            BorrowFailedAdvice(
                {MemFault::RETURN_NAME_NOT_EXIST, req.name, MemType::ADDR, 0, "", importNodeId, req.requestNodeId});
            return ReturnFailed(req, resp, UBSE_ERR_NOT_EXIST, "Memory does not exist");
        }
        exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
        exportObj.isDestroyedReportReceived = false;
        if (auto ret = SendAddrExportObj(exportObj, true, exportObj.req.exportNodeId); ret != UBSE_OK) {
            BorrowFailedAdvice({MemFault::RETURN_MASTER_SEND_FAILED, req.name, MemType::ADDR, 0,
                                exportObj.req.exportNodeId, importNodeId, req.requestNodeId});
            return DealSendAddrUnExportObjFailed(resp, req.name, exportObj);
        }
        return UBSE_OK;
    }
    return AddrReturnExistImport(importObj, exportObj, status.hasExport, req, resp);
}

UbseResult CheckAddrResourceState(const std::string& name, const std::string& importNodeId)
{
    auto [importObjPtr, exportObjPtr] =
        FindBorrowObjPair<UbseMemAddrBorrowImportObj, UbseMemAddrBorrowExportObj>(name, importNodeId);

    bool importObjExist = importObjPtr != nullptr;
    bool exportObjExist = false;

    if (exportObjPtr) {
        exportObjExist = exportObjPtr->status.state != UBSE_MEM_EXPORT_DESTROYED;
    }

    if (!importObjExist && !exportObjExist) {
        return UBSE_ERR_NOT_EXIST;
    }

    if (!importObjExist) {
        return UBSE_ERR_UNIMPORT_SUCCESS;
    }

    return GetErrorCodeByObjState(*importObjPtr, exportObjExist);
}
uint32_t DeleteAddrExport(const UbseMemAddrBorrowExportObj& exportObj)
{
    auto copy = exportObj;
    copy.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    copy.status.state = UBSE_MEM_EXPORT_DESTROYING;
    UBSE_LOG_INFO << "Force delete, name=" << copy.req.name << ", importNodeId=" << copy.req.importNodeId;
    AddrExportUpdateState(copy, UBSE_MEM_EXPORT_DESTROYING);
    return SendAddrExportObj(copy, true, copy.req.exportNodeId);
}

uint32_t AddAddrImport(const UbseMemAddrBorrowImportObj& importObj)
{
    auto copy = importObj;
    if (copy.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        EraseAddrImport(copy);
        return SchedulerImpl::GetInstance().MemoryObjChangeHandler(copy);
    }
    UBSE_LOG_INFO << "Add addr import, name=" << copy.req.name << ", importNodeId=" << copy.req.importNodeId;
    AddrImportUpdateState(copy, copy.status.state);
    return SchedulerImpl::GetInstance().MemoryObjChangeHandler(copy);
}

uint32_t AddAddrExport(const UbseMemAddrBorrowExportObj& exportObj)
{
    auto copy = exportObj;
    if (copy.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        EraseAddrExport(copy);
        return SchedulerImpl::GetInstance().MemoryObjChangeHandler(copy);
    }
    UBSE_LOG_INFO << "Add addr export, name=" << copy.req.name << ", importNodeId=" << copy.req.importNodeId;
    AddrExportUpdateState(copy, copy.status.state);
    return SchedulerImpl::GetInstance().MemoryObjChangeHandler(copy);
}
} // namespace ubse::mem::controller
