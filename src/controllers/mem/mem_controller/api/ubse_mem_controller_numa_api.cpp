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
#include "ubse_mem_controller_numa_api.h"

#include "../logging_lock_guard.h"
#include "../message/ubse_mem_numa_borrow_exportobj_simpo.h"
#include "../message/ubse_mem_numa_borrow_importobj_simpo.h"
#include "../ubse_mem_account.h"
#include "../ubse_mem_controller_ledger.h"
#include "../ubse_mem_rpc_processor.h"
#include "src/controllers/mem/mem_controller/ubse_mem_controller_pre_online.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_logger_audit.h"
#include "ubse_mem_advice.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_scheduler.h"
#include "ubse_mem_sign_verifier.h"
#include "ubse_mmi_module.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_util.h"
#include "ubse_topo_util.h"

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

bool CheckSpecifyLink(const UbseMemNumaBorrowReq &req)
{
    if (IsSameSocketMultiPortTopo()) {
        if (req.linkInfo.lenderPort != -1) {
            return true;
        }
        return false;
    }
    return true;
}

static UbseResult NumaAllocate(const UbseMemNumaBorrowReq &req, UbseMemNumaBorrowImportObj &importObj)
{
    uint8_t retryTimes = ALLOCATE_RETRY_TIME;
    auto ret = UBSE_OK;
    while (retryTimes--) {
        UbseNodeControllerLockMgr::ReadLock(req.importNodeId);
        ret = UbseMemNumaImportObjStateChangeHandler(importObj);
        UbseNodeControllerLockMgr::ReadUnLock(req.importNodeId);
        if (ret == UBSE_SCHEDULER_ERROR_NODE_RECONCILE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
            UBSE_LOG_WARN << "retry time is " << ALLOCATE_RETRY_TIME - retryTimes << ", no node can be lend";
            continue;
        }
        break;
    }
    return ret;
}

uint32_t GetPortId(const UbseCpuInfo &cpuInfo, UbseMemDebtNumaInfo &numaInfo, const std::string &importNodeId)
{
    for (const auto &portInfo : cpuInfo.portInfos) {
        if (portInfo.second.portStatus == PortStatus::UP && portInfo.second.remoteSlotId == importNodeId) {
            UBSE_LOG_INFO << "port id is " << portInfo.second.portId;
            if (ubse::utils::ConvertStrToUint32(portInfo.second.portId, numaInfo.portId) != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to convert portId from string to int, portId is " << portInfo.second.portId;
                return UBSE_ERROR;
            }
            return UBSE_OK;
        }
    }
    UBSE_LOG_ERROR << "Can not find portId.";
    return UBSE_ERROR;
}

uint32_t FillChipIdAndPortIdForExport(const ubse::nodeController::UbseNodeInfo &nodeInfo, UbseMemDebtNumaInfo &numaInfo,
                                      const std::string &importNodeId)
{
    for (const auto &cpuInfo : nodeInfo.cpuInfos) {
        if (cpuInfo.second.socketId == numaInfo.socketId) {
            auto ret = ubse::utils::ConvertStrToUint32(cpuInfo.second.chipId, numaInfo.chipId);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to conver chipId from string to int, nodeId is " << numaInfo.nodeId
                               << ", socketId is " << numaInfo.socketId << ", chipId is " << numaInfo.chipId;
                return UBSE_ERROR;
            }
            UBSE_LOG_INFO << "SocketId is " << numaInfo.socketId << ", ChipId is " << numaInfo.chipId;
            ret = GetPortId(cpuInfo.second, numaInfo, importNodeId);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to get portId, nodeId is " << numaInfo.nodeId << ", socketId is "
                               << numaInfo.socketId;
                return UBSE_ERROR;
            }
            return UBSE_OK;
        }
    }
    UBSE_LOG_ERROR << "Failed to find corresponding socket,"
                      " node id is "
                   << numaInfo.nodeId << ", socket id is " << numaInfo.socketId;
    return UBSE_ERROR;
}

uint32_t FillChipIdForImport(const ubse::nodeController::UbseNodeInfo &nodeInfo, UbseMemDebtNumaInfo &numaInfo)
{
    for (const auto &cpuInfo : nodeInfo.cpuInfos) {
        if (cpuInfo.second.socketId == numaInfo.socketId) {
            auto ret = ubse::utils::ConvertStrToUint32(cpuInfo.second.chipId, numaInfo.chipId);
            if (ret != UBSE_OK) {
                UBSE_LOG_ERROR << "Failed to get portId, nodeId is " << numaInfo.nodeId << ", socketId is "
                               << numaInfo.socketId;
                return UBSE_ERROR;
            }
            return UBSE_OK;
        }
    }
    UBSE_LOG_ERROR << "Failed to find corresponding socket,"
                      " node id is "
                   << numaInfo.nodeId << ", socket id is " << numaInfo.socketId;
    return UBSE_ERROR;
}

uint32_t ConstructNumaObjs(UbseMemNumaBorrowImportObj &importObj, UbseMemNumaBorrowExportObj &exportObj,
                           const UbseMemNumaBorrowReq &req)
{
    auto importNodeId = importObj.req.importNodeId;
    // 填入chipId
    for (auto &numaInfo : importObj.algoResult.exportNumaInfos) {
        auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(numaInfo.nodeId);
        if (FillChipIdAndPortIdForExport(nodeInfo, numaInfo, importNodeId) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to fill chipId and portId";
            return UBSE_ERROR;
        }
        // 指定链路借用
        if (req.linkInfo.lenderPort != -1) {
            numaInfo.portId = req.linkInfo.lenderPort;
            UBSE_LOG_INFO << "Specify link to borrow. The portId is " << numaInfo.portId;
        }
    }
    for (auto &numaInfo : importObj.algoResult.importNumaInfos) {
        auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(numaInfo.nodeId);
        if (FillChipIdForImport(nodeInfo, numaInfo) != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to fill chipId";
            return UBSE_ERROR;
        }
    }
    exportObj.algoResult = importObj.algoResult;
    exportObj.req = req;
    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    importObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    importObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    return UBSE_OK;
}

void UpdateNumaMemDebtInfoMap(const UbseMemNumaBorrowImportObj &importObj, const UbseMemNumaBorrowExportObj &exportObj,
                              const std::string &name)
{
    auto exportKey = GenerateExportObjKey(name, importObj.req.importNodeId);
    mapLock.LockWrite();
    nodeMemDebtInfoMap[exportObj.algoResult.exportNumaInfos[0].nodeId].numaExportObjMap[exportKey] = exportObj;
    nodeMemDebtInfoMap[importObj.req.importNodeId].numaImportObjMap[name] = importObj;
    mapLock.UnLock();
}

UbseResult AgentSendNumaExportObj(const std::shared_ptr<UbseComModule> &comModule, SendParam &sendParam,
                                  UbseMemNumaBorrowExportobjSimpoPtr &ptr, UbseBaseMessagePtr &ubseResponsePtr,
                                  const UbseMemNumaBorrowExportObj &exportObj)
{
    const uint32_t maxRetryTimes = GetWaitTimeOut() / SEND_RETRY_DURATION;
    auto ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
    uint32_t retryCount = 0;
    while (ret != UBSE_OK && retryCount < maxRetryTimes) {
        UBSE_LOG_ERROR << "Send to exportObj, name=" << exportObj.req.name
                       << ", requestNodeId=" << exportObj.req.requestNodeId << ", requestId=" << exportObj.req.requestId
                       << ", masterNodeId=" << sendParam.GetRemoteId() << " failed, " << FormatRetCode(ret);
        retryCount++;
        sleep(SEND_RETRY_DURATION);
        std::string masterId{};
        ret = UbseGetMasterNodeId(masterId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get master nodeId failed, " << FormatRetCode(ret);
            continue;
        }
        sendParam.SetRemoteId(masterId);
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
    }
    return ret;
}

UbseResult SendNumaExportObj(const std::string &nodeId, const UbseMemNumaBorrowExportObj &exportObj,
                             const bool isMaster)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule."
                       << ";requestId: " << exportObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_NUMA_BORROW_EXPORT_OBJ_CALLBACK));
    UbseMemNumaBorrowExportobjSimpoPtr ptr = new (std::nothrow) UbseMemNumaBorrowExportobjSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr."
                       << ";requestId: " << exportObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemNumaBorrowExportobj(exportObj);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr."
                       << ";requestId: " << exportObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    // 主节点向从履行侧发送
    UbseResult ret = UBSE_OK;
    if (isMaster) {
        for (int i = 0; i < SEND_RETRY_TIMES; i++) {
            ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
            if (ret == UBSE_OK) {
                UBSE_LOG_INFO << "Success to send exportObj, name is " << exportObj.req.name << "requestNodeId id is "
                              << exportObj.req.requestNodeId << ";requestId: " << exportObj.req.requestId;
                return UBSE_OK;
            }
            UBSE_LOG_ERROR << "Failed to Send to exportObj, name is " << exportObj.req.name << "requestNodeId id is "
                           << exportObj.req.requestNodeId << ", wait to retry"
                           << ";requestId: " << exportObj.req.requestId;
            sleep(SEND_RETRY_DURATION);
        }
        UBSE_LOG_ERROR << "Failed to Send to exportObj, name is " << exportObj.req.name << "requestNodeId id is "
                       << exportObj.req.requestNodeId << ";requestId: " << exportObj.req.requestId;
        UBSE_LOG_ERROR << BorrowFailedAdvice("Borrow Schedule failed", exportObj.req.name, "APP_NUMA_BORROW",
                                             exportObj.req.size, nodeId, exportObj.req.importNodeId, ret, MemAdvice::COMM_FAILED);
        return ret;
    }

    // 履行侧发回
    return AgentSendNumaExportObj(comModule, sendParam, ptr, ubseResponsePtr, exportObj);
}

uint32_t HandleSendNumaExportError(UbseMemOperationResp &resp, const UbseMemNumaBorrowReq &req,
                                   const UbseMemNumaBorrowImportObj &importObj,
                                   const UbseMemNumaBorrowExportObj &exportObj)
{
    auto exportKey = GenerateExportObjKey(importObj.req.name, importObj.req.importNodeId);
    mapLock.LockWrite();
    nodeMemDebtInfoMap[exportObj.algoResult.exportNumaInfos[0].nodeId].numaExportObjMap.erase(exportKey);
    nodeMemDebtInfoMap[importObj.req.importNodeId].numaImportObjMap.erase(req.name);
    mapLock.UnLock();

    auto copy = exportObj;
    copy.status.state = UBSE_MEM_STATE_FAILED;
    UbseMemNumaExportObjStateChangeHandler(copy);

    return BuildOperationRespWhenFail(
        resp, req.name, req.requestNodeId,
        "Failed to Send export, exportNodeId is " + exportObj.algoResult.exportNumaInfos[0].nodeId, UBSE_ERR_INTERNAL,
        ubse::adapter_plugins::mmi::MemOperationType::NUMA_BORROW);
}

bool ValidSocketAndNumaIdParams(const UbseMemNumaBorrowReq &req)
{
    if (req.linkInfo.lenderSocketId == ubse::adapter_plugins::mmi::INVALID_SOCKET_ID) {
        return true;
    }
    if (req.lenderLocs.empty()) {
        return true;
    }
    std::string lenderNodeId = req.lenderLocs[0].nodeId;
    auto nodeInfo = UbseNodeController::GetInstance().GetNodeById(lenderNodeId);
    for (const auto &lenderLoc : req.lenderLocs) {
        bool found = false;
        for (const auto &[location, info] : nodeInfo.numaInfos) {
            if (lenderLoc.numaId == location.numaId && location.nodeId == lenderLoc.nodeId &&
                req.linkInfo.lenderSocketId == info.socketId) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

uint32_t CheckReqValid(const UbseMemNumaBorrowReq &req, UbseMemOperationResp &resp)
{
    auto requestNodeId = req.requestNodeId;
    auto name = req.name;
    if (!CheckSpecifyLink(req)) {
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "The current networking only support "
                                                                     "borrowing memory via a specific link.", UBSE_ERR_LINK_NOT_ALLOWED);
    }
    if (!ValidSocketAndNumaIdParams(req)) {
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "Invalid NumaId",
                                          UBSE_ERR_NUMA_ID_IS_NOT_IN_SOCKET);
    }
    return UBSE_OK;
}

uint32_t UbseMemNumaBorrow(const UbseMemNumaBorrowReq &req, UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "Numa borrow begins, name is" << req.name << ", requestNodeId is " << req.requestNodeId
                  << ", request_id=" << req.requestId;
    auto exportKey = GenerateExportObjKey(req.name, req.importNodeId);
    auto lock = LoggingLockGuard(exportKey);
    auto requestNodeId = req.requestNodeId;
    auto importNodeId = req.importNodeId;
    auto name = req.name;
    resp.requestId = req.requestId;
    if (auto ret = CheckReqValid(req, resp); ret != UBSE_OK) {
        return ret;
    }
    mapLock.LockRead();
    auto errCode = CheckNumaResourceState(name, importNodeId);
    mapLock.UnLock();
    if (errCode != UBSE_ERR_NOT_EXIST) {
        UBSE_LOG_ERROR << BorrowFailedAdvice("Borrow Schedule failed", name, "APP_NUMA_BORROW", req.size, "",
                                             requestNodeId, errCode, MemAdvice::RESOURCE_EXIST);
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "Resource Exist.", errCode);
    }

    UbseMemNumaBorrowImportObj importObj{};
    importObj.req = req;
    importObj.req.trustRingData.ClearReqSignedDataMemory();
    importObj.status.state = UBSE_MEM_SCHEDULING;

    // 调用算法
    auto ret = NumaAllocate(req, importObj);
    if (ret != UBSE_OK || importObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "[MMC] Failed to allocate, name is " << importObj.req.name << " ,requestNodeId is "
                       << importObj.req.requestNodeId << FormatRetCode(ret) << ", request_id=" << req.requestId;
        UBSE_LOG_ERROR << BorrowFailedAdvice("Borrow Schedule failed", name, "APP_NUMA_BORROW", req.size, "",
                                             requestNodeId, ret, MemAdvice::SCHEDULE_FAILED);
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "Failed to allocate", UBSE_ERR_ALLOCATE,
                                          ubse::adapter_plugins::mmi::MemOperationType::NUMA_BORROW);
    }
    // 更改状态 存储并下发
    UbseMemNumaBorrowExportObj exportObj{};
    if (const auto res = ConstructNumaObjs(importObj, exportObj, req); res != UBSE_OK) {
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "Failed to Construct numa objs", UBSE_ERR_INTERNAL,
                                          ubse::adapter_plugins::mmi::MemOperationType::NUMA_BORROW);
    }
    // 下发exportObj
    UpdateNumaMemDebtInfoMap(importObj, exportObj, name);
    if (const auto res = SendNumaExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, exportObj, true);
        res != UBSE_OK) {
        UBSE_LOG_ERROR << BorrowFailedAdvice("Borrow Schedule failed", name, "APP_NUMA_BORROW", req.size,
                                             importObj.algoResult.exportNumaInfos[0].nodeId, requestNodeId, ret,
                                             MemAdvice::COMM_FAILED);
        return HandleSendNumaExportError(resp, req, importObj, exportObj);
    }
    return UBSE_OK;
}

void NumaExportUpdateState(UbseMemNumaBorrowExportObj &exportObj, const UbseMemState &state)
{
    exportObj.status.state = state;
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        return;
    }
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto name = exportObj.req.name;
    auto importNodeId = exportObj.req.importNodeId;
    auto exportKey = GenerateExportObjKey(name, importNodeId);
    mapLock.LockWrite();
    nodeMemDebtInfoMap[exportNodeId].numaExportObjMap[exportKey] = exportObj;
    mapLock.UnLock();
}

void EraseNumaExport(const UbseMemNumaBorrowExportObj &exportObj)
{
    auto name = exportObj.req.name;
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        return;
    }
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto importNodeId = exportObj.req.importNodeId;
    auto exportKey = GenerateExportObjKey(name, importNodeId);
    mapLock.LockWrite();
    // 避免主/履行节点共节点，先判断是否存在
    if (nodeMemDebtInfoMap[exportNodeId].numaExportObjMap.find(exportKey) !=
        nodeMemDebtInfoMap[exportNodeId].numaExportObjMap.end()) {
        nodeMemDebtInfoMap[exportNodeId].numaExportObjMap.erase(exportKey);
    }
    mapLock.UnLock();
}

void NumaImportUpdateState(UbseMemNumaBorrowImportObj &importObj, const UbseMemState &state)
{
    importObj.status.state = state;
    mapLock.LockWrite();
    nodeMemDebtInfoMap[importObj.req.importNodeId].numaImportObjMap[importObj.req.name] = importObj;
    mapLock.UnLock();
}

void EraseNumaImport(const UbseMemNumaBorrowImportObj &importObj)
{
    auto name = importObj.req.name;
    auto importNodeId = importObj.req.importNodeId;
    mapLock.LockWrite();
    // 避免主/履行节点共节点，先判断是否存在
    if (nodeMemDebtInfoMap[importNodeId].numaImportObjMap.find(name) !=
        nodeMemDebtInfoMap[importNodeId].numaImportObjMap.end()) {
        nodeMemDebtInfoMap[importNodeId].numaImportObjMap.erase(name);
    }
    mapLock.UnLock();
}

uint32_t NumaExportRunningCallback(UbseMemOperationResp &resp, UbseMemNumaBorrowExportObj &exportObj,
                                   const std::string &name, const std::string &masterNodeId,
                                   const std::string &exportNodeId, const std::string &requestNodeId)
{
    UBSE_LOG_INFO << "Numa export running callback. name is " << name << ";requestId: " << exportObj.req.requestId;
    mapLock.LockRead();
    if (nodeMemDebtInfoMap[exportObj.req.importNodeId].numaExportObjMap.find(exportObj.req.name) !=
        nodeMemDebtInfoMap[exportObj.req.importNodeId].numaExportObjMap.end()) {
        auto nowObj = nodeMemDebtInfoMap[exportObj.req.importNodeId].numaExportObjMap[exportObj.req.name];
        if (nowObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS) {
            mapLock.UnLock();
            return SendNumaExportObj(masterNodeId, nowObj, false);
        }
    }
    mapLock.UnLock();
    NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_RUNNING);
    if (auto ret = UbseMmiInterface::GetInstance().NumaExportExecutor(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to export, name is " << name << ", requestNodeId is " << requestNodeId
                       << ";requestId: " << exportObj.req.requestId;
        UBSE_LOG_ERROR << BorrowFailedAdvice("Export failed", name, "APP_NUMA_BORROW", exportObj.req.size, exportNodeId,
                                             exportObj.req.importNodeId, ret, MemAdvice::OBMM_FAILED);
        exportObj.errorCode = ret;
        // 导出失败，从节点不做存储操作，返回通知主节点。
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
        EraseNumaExport(exportObj);
        // 返回主节点 更新
        return SendNumaExportObj(masterNodeId, exportObj, false);
    }
    UBSE_LOG_INFO << "Success to export numa, name is " << name << ";requestId: " << exportObj.req.requestId;
    UBSE_AUDIT_RUNTIME_ALLOC << name << " on Node: " << exportNodeId << " NumaMemory Export "
                             << std::to_string(exportObj.req.size) << "Bytes Success";
    // 高安配置下签名并验签
    if (IsHighSafety()) {
        UbseExportSignReq trustReq{exportObj.req.trustRingData.reqSignedData, "numa"};
        trustReq.exportObmmInfo = exportObj.status.exportObmmInfo;
        trustReq.trustRingId = exportObj.req.trustRingData.trustRingId;
        if (const auto ret = UbseMemSignVerifier::SignAndVerify(trustReq, exportObj.req.trustRingData.lendSignedDatas);
            ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to sign for lend information, " << FormatRetCode(ret);
            EraseNumaExport(exportObj);
            exportObj.errorCode = ret;
            exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
            return SendNumaExportObj(masterNodeId, exportObj, false);
        }
    }
    exportObj.req.trustRingData.ClearReqSignedDataMemory();
    NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    return SendNumaExportObj(masterNodeId, exportObj, false);
}

uint32_t NumaExportDestroyingCallback(UbseMemOperationResp &resp, UbseMemNumaBorrowExportObj &exportObj,
                                      const std::string &name, const std::string &masterNodeId,
                                      const std::string &exportNodeId, const std::string &requestNodeId)
{
    UBSE_LOG_INFO << "Numa export destroying callback. name is " << name << ";requestId: " << exportObj.req.requestId;
    auto exportKey = GenerateExportObjKey(name, exportObj.req.importNodeId);
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    bool directReply = nodeMemDebtInfoMap[exportNodeId].numaExportObjMap.find(exportKey) ==
                           nodeMemDebtInfoMap[exportNodeId].numaExportObjMap.end() ||
                       nodeMemDebtInfoMap[exportNodeId].numaExportObjMap[exportKey].status.state ==
                           UBSE_MEM_EXPORT_DESTROYED;
    if (directReply) {
        EraseNumaExport(exportObj);
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
        return SendNumaExportObj(masterNodeId, exportObj, false);
    }
    NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
    if (auto ret = UbseMmiInterface::GetInstance().NumaUnExportExecutor(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to unexport name is " << name << ";requestId: " << exportObj.req.requestId;
        exportObj.errorCode = ret;
        NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        // 返回主节点 更新
        return SendNumaExportObj(masterNodeId, exportObj, false);
    }
    // 归还成功,履行节点直接擦除导出对象
    UBSE_LOG_INFO << "Success to unexport numa, name is " << name << ";requestId: " << exportObj.req.requestId;
    UBSE_AUDIT_RUNTIME_DEALLOC << name << " on Node: " << exportNodeId << " NumaMemory UnExport "
                               << std::to_string(exportObj.req.size) << " Bytes Success";
    EraseNumaExport(exportObj);
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    return SendNumaExportObj(masterNodeId, exportObj, false);
}

uint32_t NumaExportAgentCallback(const std::string &exportNodeId, UbseMemNumaBorrowExportObj &exportObj,
                                 const std::string &name, const std::string &masterNodeId)
{
    UBSE_LOG_INFO << "Numa export agent callback. name=" << name << ", state=" << exportObj.status.state
                  << ";requestId: " << exportObj.req.requestId;
    auto exportKey = GenerateExportObjKey(name, exportObj.req.importNodeId);
    auto lock = LoggingLockGuard(exportKey);
    UbseMemOperationResp resp{
        .name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId, .requestId = exportObj.req.requestId};
    auto requestNodeId = exportObj.req.requestNodeId;
    // 创建
    if (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING) {
        return NumaExportRunningCallback(resp, exportObj, name, masterNodeId, exportNodeId, requestNodeId);
    }
    // 归还
    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING) {
        return NumaExportDestroyingCallback(resp, exportObj, name, masterNodeId, exportNodeId, requestNodeId);
    }
    return UBSE_OK;
}

uint32_t NumaExportRollback(UbseMemNumaBorrowExportObj &exportObj, UbseMemNumaBorrowImportObj &importObj,
                            UbseMemOperationResp &resp, const std::string &name, const std::string &exportNodeId)
{
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
    EraseNumaImport(importObj);
    importObj.status.state = UBSE_MEM_STATE_FAILED;
    UbseMemNumaImportObjStateChangeHandler(importObj); // 通知算法
    BuildOperationRespWhenFail(resp, name, exportObj.req.requestNodeId, "Failed to import", UBSE_ERR_INTERNAL,
                               MemOperationType::NUMA_BORROW);
    return SendNumaExportObj(exportNodeId, exportObj, true);
}

UbseResult AgentSendNumaImportObj(const std::shared_ptr<UbseComModule> &comModule, SendParam &sendParam,
                                  UbseMemNumaBorrowImportobjSimpoPtr &ptr, UbseBaseMessagePtr &ubseResponsePtr,
                                  const UbseMemNumaBorrowImportObj &importObj)
{
    const uint32_t maxRetryTimes = GetWaitTimeOut() / SEND_RETRY_DURATION;
    auto ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
    uint32_t retryCount = 0;
    while (ret != UBSE_OK && retryCount < maxRetryTimes) {
        UBSE_LOG_ERROR << "Send to importObj, name=" << importObj.req.name
                       << ", requestNodeId=" << importObj.req.requestNodeId << ", requestId=" << importObj.req.requestId
                       << ", masterNodeId=" << sendParam.GetRemoteId() << " failed, " << FormatRetCode(ret);
        retryCount++;
        sleep(SEND_RETRY_DURATION);
        std::string masterId{};
        ret = UbseGetMasterNodeId(masterId);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Get master nodeId failed, " << FormatRetCode(ret);
            continue;
        }
        sendParam.SetRemoteId(masterId);
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
    }
    return ret;
}

UbseResult SendNumaImportObj(const std::string &nodeId, const UbseMemNumaBorrowImportObj &importObj,
                             const bool isMaster)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule."
                       << ";requestId: " << importObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                        static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_NUMA_BORROW_IMPORT_OBJ_CALLBACK));
    UbseMemNumaBorrowImportobjSimpoPtr ptr = new (std::nothrow) UbseMemNumaBorrowImportobjSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr."
                       << ";requestId: " << importObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemNumaBorrowImportobj(importObj);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr."
                       << ";requestId: " << importObj.req.requestId;
        return UBSE_ERROR_NULLPTR;
    }
    // 主节点向从履行侧发送
    UbseResult ret = UBSE_OK;
    if (isMaster) {
        for (int i = 0; i < SEND_RETRY_TIMES; i++) {
            ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
            if (ret == UBSE_OK) {
                UBSE_LOG_INFO << "Success to send importObj, name is " << importObj.req.name << "requestNodeId id is "
                              << importObj.req.requestNodeId << ";requestId: " << importObj.req.requestId;
                return UBSE_OK;
            }
            UBSE_LOG_ERROR << "Failed to Send to importObj, name is " << importObj.req.name << "requestNodeId id is "
                           << importObj.req.requestNodeId << ", wait to retry"
                           << ";requestId: " << importObj.req.requestId;
            sleep(SEND_RETRY_DURATION);
        }
        UBSE_LOG_ERROR << "Failed to Send to importObj, name is " << importObj.req.name << "requestNodeId id is "
                       << importObj.req.requestNodeId << ";requestId: " << importObj.req.requestId;
        UBSE_LOG_ERROR << BorrowFailedAdvice("Borrow Schedule failed", importObj.req.name, "APP_NUMA_BORROW",
                                             importObj.req.size, importObj.algoResult.exportNumaInfos[0].nodeId,
                                             nodeId, ret, MemAdvice::COMM_FAILED);
        return ret;
    }

    // 履行侧发回
    return AgentSendNumaImportObj(comModule, sendParam, ptr, ubseResponsePtr, importObj);
}

uint32_t NumaExportExpectSuccessMasterCallback(UbseMemOperationResp &resp, UbseMemNumaBorrowExportObj &exportObj,
                                               UbseMemNumaBorrowImportObj &importObj, const std::string &name,
                                               const std::string &importNodeId, const std::string &exportNodeId)
{
    UBSE_LOG_INFO << "Numa export expect success callback. name is " << name
                  << ";requestId: " << exportObj.req.requestId;
    if (exportObj.status.state == UBSE_MEM_EXPORT_SUCCESS) { // 导出成功 开始导入
        UBSE_LOG_INFO << "start to import"
                      << ";requestId: " << exportObj.req.requestId;
        importObj.exportObmmInfo = exportObj.status.exportObmmInfo;
        UBSE_LOG_INFO << "socket id is " << importObj.algoResult.exportNumaInfos[0].socketId;
        auto ret = GetCnaInfoForNumaBorrow(exportNodeId, importNodeId, importObj);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to get cna info when import" << FormatRetCode(ret)
                           << ";requestId: " << exportObj.req.requestId;
            return NumaExportRollback(exportObj, importObj, resp, name, exportNodeId);
        }
        NumaExportUpdateState(exportObj, exportObj.status.state);
        importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
        importObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
        importObj.req.trustRingData.lendSignedDatas = exportObj.req.trustRingData.lendSignedDatas;
        UbseMemNumaExportObjStateChangeHandler(exportObj);
        NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_RUNNING);
        if (ret = SendNumaImportObj(importNodeId, importObj, true); ret != UBSE_OK) {
            UBSE_LOG_ERROR << BorrowFailedAdvice("Borrow Schedule failed", name, "APP_NUMA_BORROW", importObj.req.size,
                                                 exportNodeId, importNodeId, ret, MemAdvice::COMM_FAILED);
            UBSE_LOG_ERROR << "Failed to send numa import, name is " << name
                           << ";requestId: " << exportObj.req.requestId;
            return NumaExportRollback(exportObj, importObj, resp, name, exportNodeId);
        }
        return UBSE_OK;
    }
    // 导出失败，删除导入/导出记录
    UBSE_LOG_ERROR << "Failed to export numa. name is " << name << ";requestId: " << exportObj.req.requestId;
    EraseNumaExport(exportObj);
    EraseNumaImport(importObj);
    auto copy = exportObj;
    copy.status.state = UbseMemState::UBSE_MEM_STATE_FAILED; // 通知算法
    UbseMemNumaExportObjStateChangeHandler(copy);
    return BuildOperationRespWhenFail(resp, name, exportObj.req.requestNodeId, "Failed to export.", exportObj.errorCode,
                                      ubse::adapter_plugins::mmi::MemOperationType::NUMA_BORROW);
}

uint32_t NumaExportExpectDestroyMasterCallback(UbseMemOperationResp &resp, UbseMemNumaBorrowExportObj &exportObj,
                                               const std::string &exportNodeId, const std::string &name)
{
    auto req = exportObj.returnReq;
    std::string requestNodeId = req.requestNodeId;
    resp.requestNodeId = requestNodeId;
    resp.requestId = req.requestId;
    UBSE_LOG_INFO << "Numa export expect destroy callback. name is " << name
                  << ";requestId: " << exportObj.req.requestId;
    if (exportObj.status.state == UBSE_MEM_EXPORT_SUCCESS) {
        // 归还失败,后续由对账处理
        NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        // requestNodeId为空则当前场景为对账删除导出账本或者借用失败回滚
        return requestNodeId.empty() ? UBSE_OK :
                                       BuildOperationRespWhenFail(resp, name, requestNodeId, "Failed to unexport",
                                                                  exportObj.errorCode, MemOperationType::NUMA_RETURN);
    }
    UbseMemNumaBorrowImportObj importObj{};
    auto importNodeId = exportObj.req.importNodeId;
    // 归还成功,删除导出对象/导入对象
    // 导入对象在unimport时，已经删掉。如还存在，就是删除单导出时，对账将导入账本重新加入主节点
    UBSE_LOG_INFO << "Success to unexport, name is " << name << ", exportNodeId is " << exportNodeId
                  << ";requestId: " << exportObj.req.requestId;
    EraseNumaExport(exportObj);
    auto copy = exportObj;
    UbseMemNumaExportObjStateChangeHandler(copy);
    // requestNodeId为空则当前场景为对账删除导出账本或者借用失败回滚
    return requestNodeId.empty() ? UBSE_OK :
                                   BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::NUMA_RETURN);
}

uint32_t NumaExportMasterCallback(const std::string &exportNodeId, UbseMemNumaBorrowExportObj &exportObj,
                                  const std::string &importNodeId, const std::string &name)
{
    UBSE_LOG_INFO << "Numa export master callback. name is " << name << ", state=" << exportObj.status.state
                  << ";requestId: " << exportObj.req.requestId;
    auto exportKey = GenerateExportObjKey(name, exportObj.req.importNodeId);
    auto lock = LoggingLockGuard(exportKey);
    UbseMemOperationResp resp{
        .name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId, .requestId = exportObj.req.requestId};
    UbseMemNumaBorrowImportObj importObj{};
    // 归还逻辑
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_DESTROYED) { // 归还失败
        return NumaExportExpectDestroyMasterCallback(resp, exportObj, exportNodeId, name);
    }
    mapLock.LockRead();
    if (nodeMemDebtInfoMap.find(importNodeId) != nodeMemDebtInfoMap.end() &&
        nodeMemDebtInfoMap[importNodeId].numaImportObjMap.find(name) !=
        nodeMemDebtInfoMap[importNodeId].numaImportObjMap.end()) {
        importObj = nodeMemDebtInfoMap[importNodeId].numaImportObjMap[name];
    } else {
        UBSE_LOG_ERROR << "Failed to find import obj, name is " << name << ";requestId: " << exportObj.req.requestId;
        mapLock.UnLock();
        return UBSE_ERROR;
    }
    mapLock.UnLock();
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_SUCCESS) {
        return NumaExportExpectSuccessMasterCallback(resp, exportObj, importObj, name, importNodeId, exportNodeId);
    }
    return UBSE_OK;
}

uint32_t UbseMemNumaBorrowExportObjCallback(const UbseMemNumaBorrowExportObj &exportObj)
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
    auto copy = exportObj;

    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto importNodeId = exportObj.req.importNodeId;
    auto name = exportObj.req.name;

    // 履行侧履行
    if (exportNodeId == currentNodeInfo.nodeId &&
        (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING)) {
        return NumaExportAgentCallback(exportNodeId, copy, name, masterInfo.nodeId);
    }
    // 中心侧处理
    return NumaExportMasterCallback(exportNodeId, copy, importNodeId, name);
}

uint32_t NumaImportRunningHandler(UbseMemOperationResp &resp, UbseMemNumaBorrowImportObj &importObj,
                                  const std::string &masterNodeId, const std::string &name,
                                  const std::string &requestNodeId)
{
    UBSE_LOG_INFO << "Numa import running agent callback. name is " << name
                  << ";requestId: " << importObj.req.requestId;
    std::pair<uint32_t, uint32_t> chipDiePair{};
    std::pair<uint32_t, uint32_t> remoteChipDiePair{};
    auto res = decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.attachSocketId, chipDiePair);
    res |= decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.exportNumaInfos[0].socketId,
                                                            remoteChipDiePair);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetChipAndDieId by socketId failed";
        return UBSE_ERR_INTERNAL;
    }

    decoder::utils::ImportDecoderParam importParam{};
    decoder::utils::MemDecoderUtils::SetImportDecoderParam(importParam);
    res = SetMarIdByLinkInfo(importObj.algoResult.importNumaInfos[0].nodeId,
                             importObj.algoResult.exportNumaInfos[0].nodeId, chipDiePair, remoteChipDiePair,
                             importParam);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "SetParamMarId by socketId failed";
        return UBSE_ERR_INTERNAL;
    }
    importParam.isHighSafety = IsHighSafety();
    importParam.trustRingData = importObj.req.trustRingData;
    importParam.type = "numa";
    res = ImportToAddDecoderEntry(chipDiePair, importObj.exportObmmInfo, importParam, importObj.status);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "ImportToAddDecoderEntry failed, res is " << res;
        UnimportToDelDecoderEntry(chipDiePair, importObj.status, 0);
        return UBSE_ERR_INTERNAL;
    }
    importObj.req.trustRingData.ClearLendSignedDataMemory();
    NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_RUNNING);
    if (auto ret = UbseMmiInterface::GetInstance().NumaImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to import, name is " << name << ", requestNodeId is " << requestNodeId
                       << ";requestId: " << importObj.req.requestId;
        UBSE_LOG_ERROR << BorrowFailedAdvice("Import failed", name, "APP_NUMA_BORROW", importObj.req.size,
                                             importObj.algoResult.exportNumaInfos[0].nodeId, importObj.req.importNodeId,
                                             ret, MemAdvice::OBMM_FAILED);
        UnimportToDelDecoderEntry(chipDiePair, importObj.status, 0);
        EraseNumaImport(importObj);
        return ret;
    }
    UBSE_LOG_INFO << "Success to import numa, name is " << name << ";requestId: " << importObj.req.requestId;
    UBSE_AUDIT_RUNTIME_ALLOC << name << " on Node: " << importObj.req.importNodeId << " NumaMemory Import "
                             << std::to_string(importObj.req.size) << " Bytes Success";
    return UBSE_OK;
}

uint32_t NumaImportRunningAgentCallback(UbseMemOperationResp &resp, UbseMemNumaBorrowImportObj &importObj,
                                        const std::string &masterNodeId, const std::string &name,
                                        const std::string &requestNodeId)
{
    mapLock.LockRead();
    if (nodeMemDebtInfoMap[importObj.req.importNodeId].numaImportObjMap.find(importObj.req.name) !=
        nodeMemDebtInfoMap[importObj.req.importNodeId].numaImportObjMap.end()) {
        auto nowObj = nodeMemDebtInfoMap[importObj.req.importNodeId].numaImportObjMap[importObj.req.name];
        if (nowObj.status.state == ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_SUCCESS) {
            mapLock.UnLock();
            return SendNumaImportObj(masterNodeId, nowObj, false);
        }
    }
    mapLock.UnLock();
    auto res = NumaImportRunningHandler(resp, importObj, masterNodeId, name, requestNodeId);
    if (res != UBSE_OK) {
        importObj.errorCode = res;
        importObj.status.state = ubse::adapter_plugins::mmi::UBSE_MEM_IMPORT_DESTROYED;
    } else {
        NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    }
    return SendNumaImportObj(masterNodeId, importObj, false);
}

uint32_t NumaImportDestroyingHandler(UbseMemOperationResp &resp, UbseMemNumaBorrowImportObj &importObj,
                                     const std::string &masterNodeId, const std::string &name,
                                     const std::string &requestNodeId)
{
    UBSE_LOG_INFO << "Numa import destroying agent callback. name is " << name
                  << ";requestId: " << importObj.req.requestId;
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    mapLock.LockRead();
    bool directReply = nodeMemDebtInfoMap[importObj.req.importNodeId].numaImportObjMap.find(name) ==
                           nodeMemDebtInfoMap[importObj.req.importNodeId].numaImportObjMap.end() ||
                       nodeMemDebtInfoMap[importObj.req.importNodeId].numaImportObjMap[name].status.state ==
                           UBSE_MEM_IMPORT_DESTROYED;
    mapLock.UnLock();
    if (directReply) {
        return UBSE_OK;
    }
    std::pair<uint32_t, uint32_t> chipDiePair{};
    auto res = decoder::utils::MemDecoderUtils::GetChipAndDieId(importObj.algoResult.attachSocketId, chipDiePair);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "GetChipAndDieId by socketId failed";
        return UBSE_MEMCONTROLLER_ERROR_UNIMPORT_FAILED;
    }
    NumaImportUpdateState(importObj, UBSE_MEM_EXPORT_DESTROYING);
    if (auto ret = UbseMmiInterface::GetInstance().NumaUnImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to unimport, name is " << name << ";requestId: " << resp.requestId;
        return ret;
    }
    UBSE_LOG_INFO << "Success to unimport numa, name is " << name << ";requestId: " << resp.requestId;
    UBSE_AUDIT_RUNTIME_DEALLOC << name << " on Node: " << importObj.req.importNodeId << " NumaMemory UnImport "
                               << std::to_string(importObj.req.size) << " Bytes Success";
    UnimportToDelDecoderEntry(chipDiePair, importObj.status, 0);
    if (!importObj.status.decoderResult.empty()) {
        UBSE_LOG_ERROR << "UnimportToDelDecoderEntry failed";
    }
    return UBSE_OK;
}

uint32_t NumaImportDestroyingAgentCallback(UbseMemOperationResp &resp, UbseMemNumaBorrowImportObj &importObj,
                                           const std::string &masterNodeId, const std::string &name,
                                           const std::string &requestNodeId)
{
    auto res = NumaImportDestroyingHandler(resp, importObj, masterNodeId, name, requestNodeId);
    if (res != UBSE_OK) {
        importObj.errorCode = res;
        NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
        UBSE_LOG_ERROR << "NumaUnImport Failed, Failed count:" << ++g_numaUnimportFailedCount << ". advice: Caller should clear memory and retry. "
                       << "If failures persist, migrate the workload and restart the host.";
    } else {
        importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
        EraseNumaImport(importObj);
    }
    return SendNumaImportObj(masterNodeId, importObj, false);
}

uint32_t NumaImportAgentCallback(const std::string &requestNodeId, UbseMemNumaBorrowImportObj &importObj,
                                 const std::string &name, const std::string &masterNodeId)
{
    UBSE_LOG_INFO << "numa import agent callback. name is " << name << ", state=" << importObj.status.state;
    auto exportKey = GenerateExportObjKey(name, importObj.req.importNodeId);
    auto lock = LoggingLockGuard(exportKey);
    UbseMemOperationResp resp{
        .name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId, .requestId = importObj.req.requestId};
    if (importObj.status.state == UBSE_MEM_IMPORT_RUNNING) {
        return NumaImportRunningAgentCallback(resp, importObj, masterNodeId, name, requestNodeId);
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYING) {
        return NumaImportDestroyingAgentCallback(resp, importObj, masterNodeId, name, requestNodeId);
    }
    return UBSE_OK;
}

uint32_t NumaImportExpectSuccessMasterCallBack(UbseMemOperationResp &resp, const std::string &exportNodeId,
                                               const std::string &name, const std::string &importNodeId,
                                               UbseMemNumaBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << "Numa import expect success callback. name is " << name << ";requestId: " << resp.requestId;
    if (importObj.status.state == UBSE_MEM_IMPORT_SUCCESS) { // 导入成功
        NumaImportUpdateState(importObj, importObj.status.state);
        for (const auto importResult : importObj.status.importResults) {
            resp.memIdList.push_back(importResult.memId);
        }
        if (importObj.status.importResults.empty()) {
            UBSE_LOG_WARN << "The importObj with no import result will be ignored.";
            return UBSE_ERROR;
        }
        resp.remoteNumaId = importObj.status.importResults[0].numaId;
        UbseMemNumaImportObjStateChangeHandler(importObj);
        return BuildOperationRespWhenSuccess(resp, UBSE_OK, MemOperationType::NUMA_BORROW);
    }
    // 导入失败 开始回滚
    UBSE_LOG_ERROR << "Failed to import, name is " << name << "importNodeId is " << importNodeId
                   << ";requestId: " << importObj.req.requestId;
    EraseNumaImport(importObj);
    auto exportKey = GenerateExportObjKey(name, importNodeId);
    mapLock.LockWrite();
    auto &exportObj = nodeMemDebtInfoMap[exportNodeId].numaExportObjMap[exportKey];
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    mapLock.UnLock();
    auto copy = importObj;
    copy.status.state = UBSE_MEM_STATE_FAILED;
    UbseMemNumaImportObjStateChangeHandler(copy); // 通知算法
    if (auto ret = SendNumaExportObj(exportNodeId, exportObj, true); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send rollback export. name is " << name << ";requestId: "
                       << ";requestId: " << importObj.req.requestId;
        NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    }
    return BuildOperationRespWhenFail(resp, name, importObj.req.requestNodeId, "Failed to import", importObj.errorCode);
}

uint32_t DealSendNumaUnExportObjFailed(UbseMemOperationResp &resp, const std::string &name,
                                       UbseMemNumaBorrowExportObj &exportObj)
{
    resp.requestNodeId = exportObj.returnReq.requestNodeId;
    resp.name = name;
    resp.requestId = exportObj.returnReq.requestId;
    NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    return BuildOperationRespWhenFail(resp, name, resp.requestNodeId, "Failed to send unexport.",
                                      UBSE_ERR_UNIMPORT_SUCCESS,
                                      ubse::adapter_plugins::mmi::MemOperationType::NUMA_RETURN);
}

uint32_t NumaImportExpectDestroyedMasterCallBack(UbseMemOperationResp &resp, const std::string &exportNodeId,
                                                 const std::string &name, const std::string &importNodeId,
                                                 UbseMemNumaBorrowImportObj &importObj)
{
    UBSE_LOG_INFO << "Numa import expect destroy callback. name is " << name
                  << ";requestId: " << importObj.req.requestId;
    auto exportKey = GenerateExportObjKey(name, importNodeId);
    auto req = importObj.returnReq;
    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        EraseNumaImport(importObj);
        auto waitResult = WaitNodeStateWork(exportNodeId);
        if (waitResult != UBSE_OK) {
            return BuildOperationRespWhenFail(resp, name, req.requestNodeId, "exportNode is not working.", UBSE_ERR_UNIMPORT_SUCCESS,
                MemOperationType::FD_RETURN);
        }
        mapLock.LockWrite();
        if (nodeMemDebtInfoMap[exportNodeId].numaExportObjMap.find(exportKey) !=
            nodeMemDebtInfoMap[exportNodeId].numaExportObjMap.end()) {
            auto exportObj = nodeMemDebtInfoMap[exportNodeId].numaExportObjMap[exportKey];
            exportObj.req.requestId = importObj.req.requestId;
            exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
            exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
            exportObj.returnReq = req;
            nodeMemDebtInfoMap[exportNodeId].numaExportObjMap[exportKey] = exportObj;
            mapLock.UnLock();
            UbseMemNumaImportObjStateChangeHandler(importObj);
            if (SendNumaExportObj(exportNodeId, exportObj, true) != UBSE_OK) {
                return DealSendNumaUnExportObjFailed(resp, name, exportObj);
            }
            return UBSE_OK;
        } else {
            mapLock.UnLock();
            UBSE_LOG_INFO << "Success to delete single import."
                          << ";requestId: " << importObj.req.requestId;
            UbseMemNumaImportObjStateChangeHandler(importObj);
            resp.name = name;
            resp.requestNodeId = req.requestNodeId;
            return BuildOperationRespWhenSuccess(resp, UBSE_OK,
                                                 ubse::adapter_plugins::mmi::MemOperationType::NUMA_RETURN);
        }
    }
    UBSE_LOG_ERROR << "Failed to unimport, name is " << name << ", importNodeId is "
                   << ";requestId: " << importObj.req.requestId;
    NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    return BuildOperationRespWhenFail(resp, name, req.requestNodeId, "Failed to unimport.", importObj.errorCode,
                                      MemOperationType::NUMA_RETURN);
}

uint32_t NumaImportMasterCallback(const std::string &requestNodeId, UbseMemNumaBorrowImportObj &importObj,
                                  const std::string &name, const std::string &masterNodeId)
{
    UBSE_LOG_INFO << "Numa import master callback. name is " << name << ", state=" << importObj.status.state
                  << ";requestId: " << importObj.req.requestId;
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
        return NumaImportExpectSuccessMasterCallBack(resp, exportNodeId, name, importNodeId, importObj);
    }
    // 归还成功
    if (importObj.status.expectState == UBSE_MEM_IMPORT_DESTROYED) {
        return NumaImportExpectDestroyedMasterCallBack(resp, exportNodeId, name, importNodeId, importObj);
    }
    return UBSE_OK;
}

uint32_t UbseMemNumaBorrowImportObjCallback(const UbseMemNumaBorrowImportObj &importObj)
{
    UbseRoleInfo currentNodeInfo{};
    auto ret = UbseGetCurrentNodeInfo(currentNodeInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "Failed to get local node's info, " << FormatRetCode(ret);
        return ret;
    }
    UbseRoleInfo masterInfo{};
    ret = UbseGetMasterInfo(masterInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_INFO << "Failed to get master's info, " << FormatRetCode(ret);
        return ret;
    }

    auto requestNodeId = importObj.req.requestNodeId;
    auto name = importObj.req.name;

    auto copy = importObj;
    // 履行侧履行
    if (importObj.req.importNodeId == currentNodeInfo.nodeId &&
        (importObj.status.state == UBSE_MEM_IMPORT_RUNNING || importObj.status.state == UBSE_MEM_IMPORT_DESTROYING)) {
        return NumaImportAgentCallback(requestNodeId, copy, name, masterInfo.nodeId);
    }
    // 中心侧处理
    return NumaImportMasterCallback(requestNodeId, copy, name, masterInfo.nodeId);
}

uint32_t DealSendNumaUnImportObjFailed(UbseMemNumaBorrowImportObj &importObj, const UbseMemReturnReq &req,
                                       UbseMemOperationResp &resp, const std::string &name)
{
    resp.name = name;
    resp.requestNodeId = req.requestNodeId;
    NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    return BuildOperationRespWhenFail(resp, name, req.requestNodeId, "Failed to send importObj.",
                                      UBSE_MEMCONTROLLER_ERROR_UNIMPORT_FAILED, MemOperationType::NUMA_RETURN);
}

uint32_t NumaReturnExistImport(UbseMemNumaBorrowImportObj &importObj, bool hasExport,
                               UbseMemNumaBorrowExportObj &exportObj, const UbseMemReturnReq &req,
                               UbseMemOperationResp &resp)
{
    auto name = req.name;
    auto requestNodeId = req.requestNodeId;
    // 有导入
    if (importObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "Resource not create.", UBSE_ERR_NOT_EXIST,
                                          MemOperationType::NUMA_RETURN);
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        if (!hasExport || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
            return BuildOperationRespWhenFail(resp, name, requestNodeId, "Single import has destroyed.",
                                              UBSE_ERR_NOT_EXIST, MemOperationType::NUMA_RETURN);
        }
        exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
        exportObj.req.requestId = req.requestId;
        return SendNumaExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, exportObj, true);
    }
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    importObj.req.requestId = req.requestId;
    NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    if (SendNumaImportObj(importObj.req.importNodeId, importObj, true) != UBSE_OK) {
        return DealSendNumaUnImportObjFailed(importObj, req, resp, name);
    }
    return UBSE_OK;
}

// 查找借用的对象
bool FindBorrowObjects(const UbseMemReturnReq &req, UbseMemNumaBorrowImportObj &importObj,
                       UbseMemNumaBorrowExportObj &exportObj, bool &hasImport, bool &hasExport)
{
    FindBorrowObjByName<UbseMemNumaBorrowImportObj, UbseMemNumaBorrowExportObj>(
        req.name, req.importNodeId, importObj, exportObj, hasImport, hasExport,
        [](const NodeMemDebtInfo &info) -> const UbseMemNumaImportObjMap& { return info.numaImportObjMap; },
        [](const NodeMemDebtInfo &info) -> const UbseMemNumaExportObjMap& { return info.numaExportObjMap; });

    return hasImport || hasExport;
}

// 处理只有导出对象的情况
uint32_t HandleSingleExportReturn(const UbseMemReturnReq &req, UbseMemOperationResp &resp,
                                  UbseMemNumaBorrowExportObj &exportObj)
{
    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Single export has destroyed.",
                                          UBSE_ERR_NOT_EXIST, MemOperationType::NUMA_RETURN);
    }
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    exportObj.req.requestId = req.requestId;
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId,
                                          "Single export failed get exportNumaInfos.", UBSE_ERR_INTERNAL,
                                          MemOperationType::NUMA_RETURN);
    }
    return SendNumaExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, exportObj, true);
}

uint32_t UbseMemNumaReturn(const UbseMemReturnReq &req, UbseMemOperationResp &resp,
                           const std::string &realRequestNodeId)
{
    UBSE_LOG_INFO << "Start to numa return, name is " << req.name << ", requestNodeId is " << req.requestNodeId
                  << ";requestId: " << req.requestId << ", realRequestNodeId:" << realRequestNodeId;
    auto exportKey = GenerateExportObjKey(req.name, req.requestNodeId);
    auto lock = LoggingLockGuard(exportKey);
    InitializeResponse(req, resp);
    UbseMemNumaBorrowExportObj exportObj{};
    UbseMemNumaBorrowImportObj importObj{};
    bool hasImport = false;
    bool hasExport = false;
    auto waitResult = WaitNodeStateWork(req.importNodeId);
    if (waitResult != UBSE_OK) {
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "importNode is not ok",
                                          waitResult,
                                          MemOperationType::FD_RETURN);
    }
    if (!FindBorrowObjects(req, importObj, exportObj, hasImport, hasExport)) {
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Resource not found.", UBSE_ERR_NOT_EXIST,
                                          MemOperationType::NUMA_RETURN);
    }
    UbseMemStage memStage = GetMemStageByImportObjState(importObj, hasImport);
    if (memStage != UbseMemStage::UBSE_CREATING && memStage != UbseMemStage::UBSE_DELETING) {
        memStage = GetMemStageByExportObjState(exportObj, hasExport);
    }
    if (memStage == UbseMemStage::UBSE_CREATING || memStage == UbseMemStage::UBSE_DELETING) {
        UBSE_LOG_INFO << "resource is being borrowed or returned, name is " << req.name;
        auto ret = (memStage == UbseMemStage::UBSE_CREATING) ? UBSE_ERR_CREATING : UBSE_ERR_DELETING;
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "resource being borrowed or returned", ret,
                                          MemOperationType::NUMA_RETURN);
    }
    auto udsInfo = hasExport ? exportObj.req.udsInfo : importObj.req.udsInfo;
    auto exportNodeId = hasExport ? exportObj.algoResult.exportNumaInfos[0].nodeId : "";
    if (!CheckCommonReturnPermission(udsInfo, req.udsInfo, realRequestNodeId, importObj.req.importNodeId,
                                     exportNodeId)) {
        UBSE_LOG_ERROR << "Error auth, object username: " << udsInfo.username << "uid: " << udsInfo.uid
                       << ", current req username: " << req.udsInfo.username << "uid: " << req.udsInfo.uid
                       << ", realRequestNodeId:" << realRequestNodeId << ", importNodeId:" << importObj.req.importNodeId
                       << ", exportNodeId: " << exportNodeId;
        return BuildOperationRespWhenFail(resp, req.name, req.requestNodeId, "Error auth", UBSE_ERR_AUTH_FAILED,
                                          MemOperationType::NUMA_RETURN);
    }
    exportObj.returnReq = req;
    importObj.returnReq = req;
    if (!hasImport) {
        return HandleSingleExportReturn(req, resp, exportObj);
    }
    return NumaReturnExistImport(importObj, hasExport, exportObj, req, resp);
}

uint32_t CheckNumaResourceState(const std::string &name, const std::string &importNodeId)
{
    UbseMemNumaBorrowImportObj numaImportObj{};
    UbseMemNumaBorrowExportObj numaExportObj{};
    bool importObjExist = false;
    bool exportObjExist = false;

    if (importNodeId.empty()) {
        for (auto [nodeId, nodeInfo] : nodeMemDebtInfoMap) {
            if (nodeInfo.numaImportObjMap.find(name) == nodeInfo.numaImportObjMap.end()) {
                continue;
            }
            numaImportObj = nodeInfo.numaImportObjMap[name];
            importObjExist = true;
        }
    } else if (nodeMemDebtInfoMap.find(importNodeId) != nodeMemDebtInfoMap.end()) {
        if (nodeMemDebtInfoMap[importNodeId].numaImportObjMap.find(name) !=
            nodeMemDebtInfoMap[importNodeId].numaImportObjMap.end()) {
            numaImportObj = nodeMemDebtInfoMap[importNodeId].numaImportObjMap[name];
            importObjExist = true;
        }
    }
    auto exportKey = GenerateExportObjKey(name, importNodeId);
    for (auto [nodeId, nodeInfo] : nodeMemDebtInfoMap) {
        if (nodeInfo.numaExportObjMap.find(exportKey) == nodeInfo.numaExportObjMap.end()) {
            continue;
        }
        numaExportObj = nodeInfo.numaExportObjMap[exportKey];
        exportObjExist = numaExportObj.status.state != UBSE_MEM_EXPORT_DESTROYED;
    }

    if (!importObjExist && !exportObjExist) {
        return UBSE_ERR_NOT_EXIST;
    }

    if (!importObjExist && exportObjExist) {
        return UBSE_ERR_UNIMPORT_SUCCESS;
    }

    return GetErrorCodeByObjState(numaImportObj, exportObjExist);
}
uint32_t DeleteNumaExport(const UbseMemNumaBorrowExportObj &exportObj)
{
    auto copy = exportObj;
    copy.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    copy.status.state = UBSE_MEM_EXPORT_DESTROYING;
    NumaExportUpdateState(copy, UBSE_MEM_EXPORT_DESTROYING);
    UBSE_LOG_INFO << "Force delete. name=" << copy.req.name << ", importNodeId is " << copy.req.importNodeId;
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_WARN << "The exportObj with no export numa info will be ignored.";
        return UBSE_ERROR;
    }
    return SendNumaExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, copy, true);
}

uint32_t AddNumaImport(const UbseMemNumaBorrowImportObj &importObj)
{
    auto copy = importObj;
    if (copy.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        EraseNumaImport(copy);
        return UbseMemNumaImportObjStateChangeHandler(copy);
    }
    UBSE_LOG_INFO << "Add numa import, name=" << copy.req.name << ", import node=" << copy.req.importNodeId;
    NumaImportUpdateState(copy, copy.status.state);
    return UbseMemNumaImportObjStateChangeHandler(copy);
}

uint32_t AddNumaExport(const UbseMemNumaBorrowExportObj &exportObj)
{
    auto copy = exportObj;
    if (copy.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        EraseNumaExport(copy);
        return UbseMemNumaExportObjStateChangeHandler(copy);
    }
    UBSE_LOG_INFO << "Add numa export, name=" << copy.req.name << ", import node=" << copy.req.importNodeId;
    NumaExportUpdateState(copy, copy.status.state);
    return UbseMemNumaExportObjStateChangeHandler(copy);
}
} // namespace ubse::mem::controller
