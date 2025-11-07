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

#include "ubse_mem_controller_api.h"

#include <netinet/in.h>
#include <shared_mutex>

#include "logging_lock_guard.h"
#include "message/node_mem_debt_info_simpo.h"
#include "message/ubse_mem_debtInfo_query_req_simpo.h"
#include "message/ubse_mem_fd_borrow_exportobj_simpo.h"
#include "message/ubse_mem_fd_borrow_importobj_simpo.h"
#include "message/ubse_mem_numa_borrow_exportobj_simpo.h"
#include "message/ubse_mem_numa_borrow_importobj_simpo.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"
#include "message/ubse_mem_operation_resp_simpo.h"
#include "message/ubse_mem_return_req_simpo.h"
#include "src/controllers/mem/mem_controller/message/ubse_mem_fd_borrow_req_simpo.h"
#include "src/controllers/mem/mem_scheduler/ubse_mem_scheduler.h"
#include "src/sdk/include/ubs_error.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_ipc_server.h"
#include "ubse_logger_inner.h"
#include "ubse_mem_api.h"
#include "ubse_mem_api_convert.h"
#include "ubse_mem_controller_ledger.h"
#include "ubse_mem_controller_msg.h"
#include "ubse_mem_rpc.h"
#include "ubse_mem_rpc_to_controller.h"
#include "ubse_memory_interface.h"
#include "ubse_mgr_configuration.h"
#include "ubse_mmi_module.h"
#include "ubse_node_controller.h"
#include "ubse_node_topology.h"
#include "ubse_thread_pool_module.h"

namespace ubse::mem::controller {
static ubse::utils::ReadWriteLock mapLock;
static std::shared_mutex typeMapLock;
static std::shared_mutex returnReqMapLock;
static std::shared_mutex requestIdMapLock;
static std::map<std::string, BorrowedType> requestIdBorrowedTypeMap{};

static NodeMemDebtInfoMap nodeMemDebtInfoMap{};
static std::unordered_map<std::string, UbseMemReturnReq> returnReqMap{};
static std::unordered_map<std::string, uint64_t> requestIdMap;

const uint32_t SEND_RETRY_TIMES = 5;
const uint32_t SEND_RETRY_DURATION = 1;
const uint32_t MAX_LENDER_LOC_SIZE = 2;
const uint64_t MIN_LENDER_SIZE = 128ULL * 1024ULL * 1024ULL;
const uint64_t MAX_LENDER_SIZE = 100ULL * 1024ULL * 1024ULL * 1024ULL;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)
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
using namespace ubse::ipc;
using namespace api::server;
using namespace ubse::mem::controller;

const uint32_t INVALID_VALUE_CNA = 0;

UbseResult RegisterNodeCtlNotify()
{
    // 注册本地/集群状态通知回调
    UbseNodeController::GetInstance().RegLocalStateNotifyHandler(LoadLocalAllObjs);
    UbseNodeController::GetInstance().RegClusterStateNotifyHandler(LedgerHandler);

    // 获取 ApiServer 模块
    auto apiServer = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServer == nullptr) {
        UBSE_LOG_ERROR << "Failed to get UbseApiServerModule instance.";
        return UBS_ENGINE_ERR_INTERNAL;
    }

    // 定义要注册的 IPC handlers 表（模块、消息类型、处理函数）
    using IpcHandlerEntry = struct {
        uint32_t module;
        uint32_t msgType;
        UbseIpcHandler handler;
    };

    const std::array<IpcHandlerEntry, 6> handlers = {
        {{UBSE_MEM, UBSE_MEM_FD_CREATE, UbseMemFdBorrowDispatch},
         {UBSE_MEM, UBSE_MEM_FD_WITH_LEND_INFO, UbseMemFdBorrowWithLenderDispatch},
         {UBSE_MEM, UBSE_MEM_FD_DELETE, UbseMemFdReturnDispatch},
         {UBSE_MEM, UBSE_MEM_NUMA_CREATE, UbseMemNumaCreateHandler},
         {UBSE_MEM, UBSE_MEM_NUMA_WITH_LEND_INFO, UbseMemNumaCreateWithLender},
         {UBSE_MEM, UBSE_MEM_NUMA_DELETE, UbseMemNumaDelete}}};

    // 批量注册
    for (const auto &entry : handlers) {
        auto ret = apiServer->RegisterIpcHandler(entry.module, entry.msgType, entry.handler);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to register IPC handler: module=" << entry.module << ", msgType=" << entry.msgType
                           << ", error=" << FormatRetCode(ret);
            return ret;
        }
    }

    return UBS_SUCCESS;
}

uint32_t Init()
{
    auto excutorModule = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (excutorModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get executor module.";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = excutorModule->Create("ubseMemController", 64, 1000);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to create executor.";
        return UBSE_ERROR;
    }
    if (ret = ubse::mem::scheduler::Init(); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to init scheduler, " << FormatRetCode(ret);
        return ret;
    }

    ret = usbe::mem::api::UbseMemApi::Register();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register UbseMem IPC-API failed," << FormatRetCode(ret);
        return ret;
    }

    RegUbseMemControllerHandler();
    return UBSE_OK;
}
void SendParamSwitcher(const BorrowedType &type, SendParam &sendParam)
{
    switch (type) {
        case BorrowedType::FD:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_BORROW_RESP));
            break;
        case BorrowedType::NUMA:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_BORROW_RESP));
            break;
        case BorrowedType::FD_RETURN:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_RETURN));
            break;
        case BorrowedType::NUMA_RETURN:
            sendParam.SetOpCode(static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_RETURN));
            break;
        default:
            UBSE_LOG_ERROR << "Unknown type.";
            break;
    }
}

uint32_t BuildOperationRespWhenFail(UbseMemOperationResp &resp, const std::string &name,
                                    const std::string &requestNodeId, const std::string errMsg, uint32_t errorCode,
                                    const BorrowedType type = BorrowedType::FD)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get com module.";
        return UBSE_ERROR_NULLPTR;
    }
    UBSE_LOG_ERROR << errMsg;
    resp.errMsg = errMsg;
    resp.name = name;
    resp.requestNodeId = requestNodeId;
    resp.errorCode = errorCode;
    SendParam sendParam(resp.requestNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_CONTROLLER),
                        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_BORROW_RESP));
    SendParamSwitcher(type, sendParam);
    UbseMemOperationRespSimpoPtr ptr = new (std::nothrow) UbseMemOperationRespSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemOperationResp(resp);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    return comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
    UbseResult ret = UBSE_OK;
    for (int i = 0; i < 5; ++i) {
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
        if (ret == UBSE_OK) {
            UBSE_LOG_INFO << "Success to send resp when action is successful, name is " << resp.name;
            return ret;
        }
    }
    UBSE_LOG_ERROR << "Failed to send resp, name is " << resp.name;
    return ret;
}

uint32_t BuildOperationRespWhenSuccess(UbseMemOperationResp &resp, UbseMemErrorCode errorCode,
                                       const BorrowedType type = BorrowedType::FD)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get com module.";
        return UBSE_ERROR_NULLPTR;
    }
    resp.errorCode = UBS_SUCCESS;
    SendParam sendParam(resp.requestNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_CONTROLLER),
                        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_BORROW_RESP));
    SendParamSwitcher(type, sendParam);
    UbseMemOperationRespSimpoPtr ptr = new (std::nothrow) UbseMemOperationRespSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemOperationResp(resp);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseResult ret = UBSE_OK;
    for (int i = 0; i < 5; ++i) {
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
        if (ret == UBSE_OK) {
            UBSE_LOG_INFO << "Success to send resp when action is successful, name is " << resp.name;
            return ret;
        }
    }
    return ret;
}

UbseResult SendFdExportObj(const std::string &nodeId, const UbseMemFdBorrowExportObj &exportObj, const bool isMaster)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule.";
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_FD_BORROW_EXPORT_OBJ_CALLBACK),
                        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_BORROW_EXPORT_OBJ_CALLBACK));
    UbseMemFdBorrowExportobjSimpoPtr ptr = new (std::nothrow) UbseMemFdBorrowExportobjSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemFdBorrowExportobj(exportObj);
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
                UBSE_LOG_DEBUG << "Success to send exportObj, name is " << exportObj.req.name << "requestNodeId id is "
                               << exportObj.req.requestNodeId;
                return UBSE_OK;
            }
            UBSE_LOG_ERROR << "Failed to Send to exportObj, name is " << exportObj.req.name << "requestNodeId id is "
                           << exportObj.req.requestNodeId << ", wait to retry";
            sleep(SEND_RETRY_DURATION);
        }
        UBSE_LOG_ERROR << "Failed to Send to exportObj, name is " << exportObj.req.name << "requestNodeId id is "
                       << exportObj.req.requestNodeId;
        return ret;
    }

    // 履行侧发回
    ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
    while (ret != UBSE_OK) {
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
        UBSE_LOG_ERROR << "Failed to Send to exportObj, name is " << exportObj.req.name << "requestNodeId id is "
                       << exportObj.req.requestNodeId;
        sleep(SEND_RETRY_DURATION);
    }
    return ret;
}

UbseResult SendFdImportObj(const std::string &nodeId, const UbseMemFdBorrowImportObj &importObj, const bool isMaster)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule.";
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_FD_BORROW_IMPORT_OBJ_CALLBACK),
                        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_BORROW_IMPORT_OBJ_CALLBACK));
    UbseMemFdBorrowImportobjSimpoPtr ptr = new (std::nothrow) UbseMemFdBorrowImportobjSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemFdBorrowImportobj(importObj);
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
                UBSE_LOG_DEBUG << "Success to send importObj, name is " << importObj.req.name << "requestNodeId id is "
                               << importObj.req.requestNodeId;
                return UBSE_OK;
            }
            UBSE_LOG_ERROR << "Failed to Send to importObj, name is " << importObj.req.name << "requestNodeId id is "
                           << importObj.req.requestNodeId << ", wait to retry";
            sleep(SEND_RETRY_DURATION);
        }
        UBSE_LOG_ERROR << "Failed to Send to importObj, name is " << importObj.req.name << "requestNodeId id is "
                       << importObj.req.requestNodeId;
        return ret;
    }

    // 履行侧发回
    ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
    while (ret != UBSE_OK) {
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
        UBSE_LOG_ERROR << "Failed to Send to importObj, name is " << importObj.req.name << "requestNodeId id is "
                       << importObj.req.requestNodeId;
        sleep(SEND_RETRY_DURATION);
    }
    return ret;
}

UbseResult SendNumaExportObj(const std::string &nodeId, const UbseMemNumaBorrowExportObj &exportObj,
                             const bool isMaster)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule.";
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_NUMA_BORROW_EXPORT_OBJ_CALLBACK),
                        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_BORROW_EXPORT_OBJ_CALLBACK));
    UbseMemNumaBorrowExportobjSimpoPtr ptr = new (std::nothrow) UbseMemNumaBorrowExportobjSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemNumaBorrowExportobj(exportObj);
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
                UBSE_LOG_DEBUG << "Success to send exportObj, name is " << exportObj.req.name << "requestNodeId id is "
                               << exportObj.req.requestNodeId;
                return UBSE_OK;
            }
            UBSE_LOG_ERROR << "Failed to Send to exportObj, name is " << exportObj.req.name << "requestNodeId id is "
                           << exportObj.req.requestNodeId << ", wait to retry";
            sleep(SEND_RETRY_DURATION);
        }
        UBSE_LOG_ERROR << "Failed to Send to exportObj, name is " << exportObj.req.name << "requestNodeId id is "
                       << exportObj.req.requestNodeId;
        return ret;
    }

    // 履行侧发回
    ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
    while (ret != UBSE_OK) {
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
        UBSE_LOG_ERROR << "Failed to Send to exportObj, name is " << exportObj.req.name << "requestNodeId id is "
                       << exportObj.req.requestNodeId;
        sleep(SEND_RETRY_DURATION);
    }
    return ret;
}

UbseResult SendNumaImportObj(const std::string &nodeId, const UbseMemNumaBorrowImportObj &importObj,
                             const bool isMaster)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Failed to get comModule.";
        return UBSE_ERROR_NULLPTR;
    }
    SendParam sendParam(nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_NUMA_BORROW_IMPORT_OBJ_CALLBACK),
                        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_BORROW_IMPORT_OBJ_CALLBACK));
    UbseMemNumaBorrowImportobjSimpoPtr ptr = new (std::nothrow) UbseMemNumaBorrowImportobjSimpo();
    if (ptr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr.";
        return UBSE_ERROR_NULLPTR;
    }
    ptr->SetUbseMemNumaBorrowImportobj(importObj);
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
                UBSE_LOG_DEBUG << "Success to send importObj, name is " << importObj.req.name << "requestNodeId id is "
                               << importObj.req.requestNodeId;
                return UBSE_OK;
            }
            UBSE_LOG_ERROR << "Failed to Send to importObj, name is " << importObj.req.name << "requestNodeId id is "
                           << importObj.req.requestNodeId << ", wait to retry";
            sleep(SEND_RETRY_DURATION);
        }
        UBSE_LOG_ERROR << "Failed to Send to importObj, name is " << importObj.req.name << "requestNodeId id is "
                       << importObj.req.requestNodeId;
        return ret;
    }

    // 履行侧发回
    ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
    while (ret != UBSE_OK) {
        ret = comModule->RpcSend(sendParam, ptr, ubseResponsePtr);
        UBSE_LOG_ERROR << "Failed to Send to importObj, name is " << importObj.req.name << "requestNodeId id is "
                       << importObj.req.requestNodeId;
        sleep(SEND_RETRY_DURATION);
    }
    return ret;
}

void FdExportUpdateState(UbseMemFdBorrowExportObj &exportObj, const UbseMemState &state)
{
    exportObj.status.state = state;
    mapLock.LockWrite();
    nodeMemDebtInfoMap[exportObj.algoResult.exportNumaInfos[0].nodeId].fdExportObjMap[exportObj.req.name] = exportObj;
    mapLock.UnLock();
}

void FdImportUpdateState(UbseMemFdBorrowImportObj &importObj, const UbseMemState &state)
{
    importObj.status.state = state;
    mapLock.LockWrite();
    nodeMemDebtInfoMap[importObj.req.importNodeId].fdImportObjMap[importObj.req.name] = importObj;
    mapLock.UnLock();
}

void NumaExportUpdateState(UbseMemNumaBorrowExportObj &exportObj, const UbseMemState &state)
{
    exportObj.status.state = state;
    mapLock.LockWrite();
    nodeMemDebtInfoMap[exportObj.algoResult.exportNumaInfos[0].nodeId].numaExportObjMap[exportObj.req.name] = exportObj;
    mapLock.UnLock();
}

void NumaImportUpdateState(UbseMemNumaBorrowImportObj &importObj, const UbseMemState &state)
{
    importObj.status.state = state;
    mapLock.LockWrite();
    nodeMemDebtInfoMap[importObj.req.importNodeId].numaImportObjMap[importObj.req.name] = importObj;
    mapLock.UnLock();
}

uint64_t UbseMemRequestIdGet(const std::string &name)
{
    std::shared_lock<std::shared_mutex> lock(requestIdMapLock);
    auto it = requestIdMap.find(name);
    if (it != requestIdMap.end()) {
        return it->second;
    } else {
        return {};
    }
}

void UbseMemRequestIdSet(const std::string &name, const uint64_t &requestId)
{
    std::unique_lock<std::shared_mutex> lock(requestIdMapLock);
    requestIdMap[name] = requestId;
}

UbseMemReturnReq UbseMemReturnReqMap(const std::string &name)
{
    std::shared_lock<std::shared_mutex> lock(returnReqMapLock);
    auto it = returnReqMap.find(name);
    if (it != returnReqMap.end()) {
        UbseMemReturnReq result = it->second;
        returnReqMap.erase(name);
        requestIdBorrowedTypeMap.erase(name);
        return result;
    } else {
        return {};
    }
}

void UbseMemReturnReqMapSet(const std::string &name, const UbseMemReturnReq &ubseMemReturnReq)
{
    std::unique_lock<std::shared_mutex> lock(returnReqMapLock);
    returnReqMap[name] = ubseMemReturnReq;
}

BorrowedType UbseMemRequestIdBorrowedTypeGet(const std::string &name)
{
    std::shared_lock<std::shared_mutex> lock(typeMapLock);
    auto it = requestIdBorrowedTypeMap.find(name);
    if (it != requestIdBorrowedTypeMap.end()) {
        return it->second;
    } else {
        return {};
    }
}

void UbseMemRequestIdBorrowedTypeMapSet(const std::string &name, const BorrowedType &borrowedType)
{
    std::unique_lock<std::shared_mutex> lock(typeMapLock);
    requestIdBorrowedTypeMap[name] = borrowedType;
}

void ConstructFdObjs(UbseMemFdBorrowImportObj &importObj, UbseMemFdBorrowExportObj &exportObj,
                     const UbseMemFdBorrowReq &req, const uint64_t &currentTimestamp)
{
    exportObj.algoResult = importObj.algoResult;
    exportObj.req = req;
    exportObj.req.timestamp = currentTimestamp;
    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    importObj.req.timestamp = currentTimestamp;
    importObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    importObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
}
uint32_t UbseMemFdBorrowRpc(UbseMemFdBorrowReq &req, const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    UbseRoleInfo masterInfo{};
    if (auto ret = UbseGetMasterInfo(masterInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get master info, " << FormatRetCode(ret);
        return ret;
    }
    UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }
    req.requestNodeId = currentRoleInfo.nodeId;
    req.importNodeId = currentRoleInfo.nodeId;
    SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_FD_BORROW),
                        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_BORROW)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }

    UbseMemFdBorrowReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemFdBorrowReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemFdBorrowReq(req);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr";
        return UBSE_ERROR_NULLPTR;
    }
    UbseMemRequestIdSet(req.name, context.requestId);
    auto ret = comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to Send fd borrow request, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

bool CheckFdBorrowParameter(const UbseMemFdBorrowReq &req)
{
    if (req.name == "") {
        UBSE_LOG_ERROR << "Name is empty.";
        return false;
    }
    if (req.size < MIN_LENDER_SIZE || req.size > MAX_LENDER_SIZE) {
        UBSE_LOG_ERROR << "The size is out of range, req size=" << req.size;
        return false;
    }
    if (req.distance != UbseMemDistance::MEM_DISTANCE_L0) {
        UBSE_LOG_ERROR << "Non-directly connected node.";
        return false;
    }
    if (req.lenderLocs.size() > MAX_LENDER_LOC_SIZE) {
        UBSE_LOG_ERROR << "The account of lender location is invalid.";
        return false;
    }
    return true;
}

uint32_t UbseMemFdBorrowDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    UbseMemFdBorrowReq req;
    UbseMemCreateReqUnpack(buffer, req);
    UbseMemOperationResp resp{.name = req.name};
    if (!CheckFdBorrowParameter(req)) {
        return IPC_ERROR_INVALID_ARGUMENT;
    }
    return UbseMemFdBorrowRpc(req, buffer, context);
}

bool CheckFdBorrowLenderLocs(const UbseMemFdBorrowReq &req)
{
    if (req.lenderLocs.size() != req.lenderSizes.size()) {
        UBSE_LOG_ERROR << "LenderLocs' size and lenderSizes' size mismatch";
        return false;
    }
    uint64_t size = 0;
    for (const auto lenderSize : req.lenderSizes) {
        if (lenderSize < 128ULL * 1024ULL * 1024ULL || lenderSize > 100ULL * 1024ULL * 1024ULL * 1024ULL) {
            UBSE_LOG_ERROR << "Lendersize  is invalid, lendersize is " << lenderSize;
            return false;
        }
        size += lenderSize;
    }
    if (size != req.size) {
        UBSE_LOG_ERROR << "Total size and lenderSizes mismatch";
        return false;
    }
    return true;
}

uint32_t UbseMemFdBorrowWithLenderDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    UbseMemFdBorrowReq req;
    UbseMemCreateWithLenderReqUnpack(buffer, req);
    if (!CheckFdBorrowParameter(req) || !CheckFdBorrowLenderLocs(req)) {
        return IPC_ERROR_INVALID_ARGUMENT;
    }
    UbseMemOperationResp resp{.name = req.name};
    return UbseMemFdBorrowRpc(req, buffer, context);
}

uint32_t UbseMemFdBorrow(const UbseMemFdBorrowReq &req, UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "Fd borrow begins, name is" << req.name << ", requestNodeId is " << req.requestNodeId;
    auto lock = LoggingLockGuard(req.name);
    auto requestNodeId = req.requestNodeId;
    auto name = req.name;
    auto importNodeId = req.importNodeId;
    mapLock.LockRead();
    if (auto nodeInfo = nodeMemDebtInfoMap.find(importNodeId); nodeInfo != nodeMemDebtInfoMap.end()) {
        if (auto item = nodeInfo->second.fdImportObjMap.find(name);
            item != nodeInfo->second.fdImportObjMap.end() && item->second.status.state != UBSE_MEM_EXPORT_DESTROYED) {
            mapLock.UnLock();
            return BuildOperationRespWhenFail(resp, name, requestNodeId, "Resource Exist.", UBS_ENGINE_ERR_EXISTED);
        }
    }
    // 创建父对象
    mapLock.UnLock();
    UbseMemFdBorrowImportObj importObj{.req = req};
    importObj.status.state = UBSE_MEM_SCHEDULING;
    // 调用算法
    auto ret = UbseMemFdImportObjStateChangeHandler(importObj);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MMC] Failed to allocate, name is " << importObj.req.name << " ,requestNodeId is "
                       << importObj.req.requestNodeId << FormatRetCode(ret);
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "Failed to allocate", UBS_ENGINE_ERR_ALLOCATE);
    }
    // 下发对象执行
    uint64_t currentTimestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    UbseMemFdBorrowExportObj exportObj{};
    ConstructFdObjs(importObj, exportObj, req, currentTimestamp);
    mapLock.LockWrite();
    // 放入导出对象、导入对象
    nodeMemDebtInfoMap[exportObj.algoResult.exportNumaInfos[0].nodeId].fdExportObjMap[name] = exportObj;
    nodeMemDebtInfoMap[importNodeId].fdImportObjMap[name] = importObj;
    UbseMemRequestIdBorrowedTypeMapSet(req.name, BorrowedType::FD);
    mapLock.UnLock();
    UBSE_LOG_INFO << "FdExportObj and importObj stored, name is " << req.name << ", requestNodeId is "
                  << req.requestNodeId;
    ret = SendFdExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, exportObj, true);
    if (ret != UBSE_OK) {
        mapLock.LockWrite();
        nodeMemDebtInfoMap[exportObj.algoResult.exportNumaInfos[0].nodeId].fdExportObjMap.erase(name);
        nodeMemDebtInfoMap[importNodeId].fdImportObjMap.erase(name);
        mapLock.UnLock();
        BuildOperationRespWhenFail(
            resp, name, requestNodeId,
            "Failed to Send export, export node is " + exportObj.algoResult.exportNumaInfos[0].nodeId,
            UBS_ENGINE_ERR_INTERNAL);
        return ret;
    }
    return UBSE_OK;
}

void ConstructNumaObjs(UbseMemNumaBorrowImportObj &importObj, UbseMemNumaBorrowExportObj &exportObj,
                       const uint64_t &currentTimestamp, const UbseMemNumaBorrowReq &req)
{
    exportObj.algoResult = importObj.algoResult;
    exportObj.req = req;
    exportObj.req.timestamp = currentTimestamp;
    exportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    exportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    importObj.req.timestamp = currentTimestamp;
    importObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    importObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
}

uint32_t UbseMemNumaBorrow(const UbseMemNumaBorrowReq &req, UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "Numa borrow begins, name is" << req.name << ", requestNodeId is " << req.requestNodeId;
    auto lock = LoggingLockGuard(req.name);
    auto requestNodeId = req.requestNodeId;
    auto importNodeId = req.importNodeId;
    auto name = req.name;
    mapLock.LockRead();
    if (auto nodeInfo = nodeMemDebtInfoMap.find(importNodeId); nodeInfo != nodeMemDebtInfoMap.end()) {
        if (auto item = nodeInfo->second.numaImportObjMap.find(name);
            item != nodeInfo->second.numaImportObjMap.end() && item->second.status.state != UBSE_MEM_EXPORT_DESTROYED) {
            UBSE_LOG_ERROR << "Resource Exist.";
            mapLock.UnLock();
            return BuildOperationRespWhenFail(resp, name, requestNodeId, "Resource Exist.", UBS_ENGINE_ERR_EXISTED);
        }
    }
    mapLock.UnLock();
    UbseMemNumaBorrowImportObj importObj{};
    importObj.req = req;
    importObj.status.state = UBSE_MEM_SCHEDULING;

    // 调用算法
    auto ret = UbseMemNumaImportObjStateChangeHandler(importObj);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[MMC] Failed to allocate, name is " << importObj.req.name << " ,requestNodeId is "
                       << importObj.req.requestNodeId << FormatRetCode(ret);
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "Failed to allocate", UBS_ENGINE_ERR_ALLOCATE);
    }
    // 更改状态 存储并下发
    uint64_t currentTimestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    UbseMemNumaBorrowExportObj exportObj{};
    ConstructNumaObjs(importObj, exportObj, currentTimestamp, req);
    // 下发exportObj
    mapLock.LockWrite();
    nodeMemDebtInfoMap[exportObj.algoResult.exportNumaInfos[0].nodeId].numaExportObjMap[name] = exportObj;
    nodeMemDebtInfoMap[importNodeId].numaImportObjMap[name] = importObj;
    UbseMemRequestIdBorrowedTypeMapSet(req.name, BorrowedType::NUMA);
    mapLock.UnLock();
    ret = SendNumaExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, exportObj, true);
    if (ret != UBSE_OK) {
        mapLock.LockWrite();
        nodeMemDebtInfoMap[exportObj.algoResult.exportNumaInfos[0].nodeId].numaExportObjMap.erase(name);
        nodeMemDebtInfoMap[importNodeId].numaImportObjMap.erase(name);
        mapLock.UnLock();
        BuildOperationRespWhenFail(
            resp, name, requestNodeId,
            "Failed to Send export, exportNodeId is " + exportObj.algoResult.exportNumaInfos[0].nodeId,
            UBS_ENGINE_ERR_INTERNAL);
        return ret;
    }
    return UBSE_OK;
}

uint32_t FdExportRunningAgentCallback(UbseMemOperationResp &resp, UbseMemFdBorrowExportObj &exportObj,
                                      const std::string &name, const std::string &masterNodeId,
                                      const std::string &exportNodeId, const std::string &requestNodeId)
{
    auto mmiModule = UbseContext::GetInstance().GetModule<UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "Obmm instance is null.";
        exportObj.errorCode = UBS_ENGINE_ERR_INTERNAL;
        FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYED);
        // 返回主节点 更新
        return SendFdExportObj(masterNodeId, exportObj, false);
    }
    if (auto ret = mmiModule->UbseMemFdExportExecutor(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to export, name is " << name << ", requestNodeId is " << requestNodeId;
        exportObj.errorCode = UBS_ENGINE_ERR_INTERNAL;
        FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYED);
        // 返回主节点 更新
        return SendFdExportObj(masterNodeId, exportObj, false);
    }
    // 超时处理
    if (nodeMemDebtInfoMap[exportNodeId].fdExportObjMap[name].status.state == UBSE_MEM_EXPORT_DESTROYING_WAIT) {
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
        FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
        return SendFdExportObj(exportNodeId, exportObj, false);
    }
    UBSE_LOG_INFO << "Success to export, name is " << name;
    FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    return SendFdExportObj(masterNodeId, exportObj, false);
}

uint32_t FdExportDestroyingAgentCallback(UbseMemOperationResp &resp, UbseMemFdBorrowExportObj &exportObj,
                                         const std::string &name, const std::string &exportNodeId,
                                         const std::string &masterNodeId, const std::string &requestNodeId)
{
    UBSE_LOG_DEBUG << "Fd export destroying callback. name is " << name;
    auto mmiModule = UbseContext::GetInstance().GetModule<UbseMmiModule>();
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    bool directReply = nodeMemDebtInfoMap[exportNodeId].fdExportObjMap.find(name) ==
                           nodeMemDebtInfoMap[exportNodeId].fdExportObjMap.end() ||
                       nodeMemDebtInfoMap[exportNodeId].fdExportObjMap[name].status.state == UBSE_MEM_EXPORT_DESTROYED;
    if (directReply) {
        FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYED);
        return SendFdExportObj(masterNodeId, exportObj, false);
    }
    if (nodeMemDebtInfoMap[exportNodeId].fdExportObjMap[name].status.state == UBSE_MEM_EXPORT_RUNNING) {
        exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
        FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING_WAIT);
        return UBSE_OK;
    }
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        exportObj.errorCode = UBS_ENGINE_ERR_INTERNAL;
        FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        return SendFdExportObj(masterNodeId, exportObj, false);
    }
    if (auto ret = mmiModule->UbseMemFdUnExportExecutor(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to unexport name is " << name;
        exportObj.errorCode = UBS_ENGINE_ERR_INTERNAL;
        FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        // 返回主节点 更新
        return SendFdExportObj(masterNodeId, exportObj, false);
    }
    UBSE_LOG_ERROR << "Success to unexport, name is " << name;
    // 归还成功
    FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYED);
    return SendFdExportObj(masterNodeId, exportObj, false);
}

uint32_t FdExportAgentCallback(const std::string &exportNodeId, UbseMemFdBorrowExportObj &exportObj,
                               const std::string &name, const std::string masterNodeId)
{
    UBSE_LOG_INFO << "Fd export agent callback";
    auto lock = LoggingLockGuard(name);
    auto mmiModule = UbseContext::GetInstance().GetModule<UbseMmiModule>();
    FdExportUpdateState(exportObj, exportObj.status.state);
    auto requestNodeId = exportObj.req.requestNodeId;
    UbseMemOperationResp resp{.name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId};
    // 创建
    if (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING) {
        return FdExportRunningAgentCallback(resp, exportObj, name, masterNodeId, exportNodeId, requestNodeId);
    }
    // 归还
    if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING) {
        return FdExportDestroyingAgentCallback(resp, exportObj, name, exportNodeId, masterNodeId, requestNodeId);
    }
    return UBSE_OK;
}

uint32_t FdExportExpectSuccessMasterCallback(UbseMemOperationResp &resp, UbseMemFdBorrowExportObj &exportObj,
                                             UbseMemFdBorrowImportObj &importObj, const std::string name,
                                             const std::string exportNodeId, const std::string importNodeId)
{
    if (exportObj.status.state == UBSE_MEM_EXPORT_SUCCESS) { // 导出成功 开始导入
        UBSE_LOG_INFO << "Start to import";
        importObj.exportObmmInfo = exportObj.status.exportObmmInfo;
        auto ret = GetCnaInfoWhenImport(exportNodeId, importNodeId, importObj);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to get cna info when inport" << FormatRetCode(ret);
            exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
            FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
            importObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
            FdImportUpdateState(importObj, UBSE_MEM_EXPORT_DESTROYING);
            BuildOperationRespWhenFail(resp, name, exportObj.req.requestNodeId, "Failed to get cna info when import",
                                       UBS_ENGINE_ERR_INTERNAL);
            return SendFdExportObj(exportNodeId, exportObj, true);
        }
        FdExportUpdateState(exportObj, exportObj.status.state);
        importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
        importObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
        UbseMemFdExportObjStateChangeHandler(exportObj);
        FdImportUpdateState(importObj, UBSE_MEM_IMPORT_RUNNING);
        return SendFdImportObj(importNodeId, importObj, true);
    }
    // 导出失败，回滚
    FdImportUpdateState(importObj, UBSE_MEM_EXPORT_DESTROYED);
    FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYED);
    auto copy = exportObj;
    copy.status.state = UbseMemState::UBSE_MEM_STATE_FAILED; // 通知算法
    UbseMemFdExportObjStateChangeHandler(copy);
    return BuildOperationRespWhenFail(resp, name, exportObj.req.requestNodeId, "Failed to export", exportObj.errorCode);
}

uint32_t FdExportMasterCallback(const std::string &exportNodeId, UbseMemFdBorrowExportObj &exportObj,
                                const std::string &importNodeId, const std::string &name)
{
    auto lock = LoggingLockGuard(name);
    mapLock.LockRead();
    auto importObj = nodeMemDebtInfoMap[importNodeId].fdImportObjMap[name];
    mapLock.UnLock();
    UbseMemOperationResp resp{.name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId};
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_SUCCESS) {
        return FdExportExpectSuccessMasterCallback(resp, exportObj, importObj, name, exportNodeId, importNodeId);
    }
    // 归还逻辑
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_DESTROYED) { // 归还失败
        if (exportObj.status.state == UBSE_MEM_EXPORT_SUCCESS) {
            // 归还失败,后续由对账清理
            FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
            return BuildOperationRespWhenFail(resp, name, UbseMemReturnReqMap(name).requestNodeId, "Failed to unexport",
                                              exportObj.errorCode, BorrowedType::FD_RETURN);
        }
        // 归还成功
        FdImportUpdateState(importObj, UBSE_MEM_EXPORT_DESTROYED);
        FdExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYED);
        UbseMemFdExportObjStateChangeHandler(exportObj);

        resp.requestNodeId = UbseMemReturnReqMap(name).requestNodeId;
        if (resp.requestNodeId.empty()) {
            UBSE_LOG_ERROR << "Request node id is empty. ";
            return UBSE_ERROR;
        }
        return BuildOperationRespWhenSuccess(resp, UbseMemErrorCode::DELETE_SUCCESS, BorrowedType::FD_RETURN);
    }
    return UBSE_OK;
}

uint32_t UbseMemFdBorrowExportObjCallback(const UbseMemFdBorrowExportObj &exportObj)
{
    UBSE_LOG_INFO << "Fd export callback. name is " << exportObj.req.name;
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    UbseRoleInfo masterInfo{};
    UbseGetMasterInfo(masterInfo);

    auto copy = exportObj;
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto importNodeId = exportObj.req.importNodeId;
    auto name = exportObj.req.name;

    // 履行侧履行
    if (exportNodeId == currentNodeInfo.nodeId &&
        (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING)) {
        return FdExportAgentCallback(exportNodeId, copy, name, masterInfo.nodeId);
    }
    // 中心侧处理
    return FdExportMasterCallback(exportNodeId, copy, importNodeId, name);
}

uint32_t FdImportRunningCallback(UbseMemFdBorrowImportObj &importObj, const std::string &name,
                                 const std::string &masterNodeId, const std::string &requestNodeId)
{
    UBSE_LOG_DEBUG << "Fd import running agent callback, name is " << name;
    auto mmiModule = UbseContext::GetInstance().GetModule<UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        importObj.errorCode = UBS_ENGINE_ERR_INTERNAL;
        FdImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYED);
        return SendFdImportObj(masterNodeId, importObj, false);
    }
    if (auto ret = mmiModule->UbseMemFdImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to import, name is " << name << ", requestNodeId is " << requestNodeId;
        importObj.errorCode = UBS_ENGINE_ERR_INTERNAL;
        FdImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYED);
        return SendFdImportObj(masterNodeId, importObj, false);
    }
    // 超时处理
    if (nodeMemDebtInfoMap[requestNodeId].fdImportObjMap[name].status.state == UBSE_MEM_EXPORT_DESTROYING_WAIT) {
        FdImportUpdateState(importObj, UBSE_MEM_EXPORT_DESTROYING);
        return SendFdImportObj(requestNodeId, importObj, false);
    }
    UBSE_LOG_INFO << "Success to fd import";
    FdImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    return SendFdImportObj(masterNodeId, importObj, false);
}

uint32_t FdImportDestroyingAgentCallback(UbseMemFdBorrowImportObj &importObj, const std::string &name,
                                         const std::string &masterNodeId, const std::string &requestNodeId)
{
    UBSE_LOG_DEBUG << "Fd import destroying agent callback, name is " << name;
    auto mmiModule = UbseContext::GetInstance().GetModule<UbseMmiModule>();
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    bool directReply = nodeMemDebtInfoMap[importObj.req.importNodeId].fdImportObjMap.find(name) ==
                           nodeMemDebtInfoMap[importObj.req.importNodeId].fdImportObjMap.end() ||
                       nodeMemDebtInfoMap[importObj.req.importNodeId].fdImportObjMap[name].status.state ==
                           UBSE_MEM_IMPORT_DESTROYED;
    if (directReply) {
        FdImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYED);
        return SendFdImportObj(masterNodeId, importObj, false);
    }
    if (nodeMemDebtInfoMap[requestNodeId].fdImportObjMap[name].status.state == UBSE_MEM_IMPORT_RUNNING) {
        importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
        FdImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING_WAIT);
        return UBSE_OK;
    }
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        importObj.errorCode = UBS_ENGINE_ERR_INTERNAL;
        FdImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
        return SendFdImportObj(masterNodeId, importObj, false);
    }
    if (auto ret = mmiModule->UbseMemFdUnImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to unimport, name is " << name;
        importObj.errorCode = UBS_ENGINE_ERR_INTERNAL;
        FdImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
        return SendFdImportObj(masterNodeId, importObj, false);
    }
    FdImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYED);
    return SendFdImportObj(masterNodeId, importObj, false);
}

uint32_t FdImportAgentCallback(const std::string requestNodeId, UbseMemFdBorrowImportObj &importObj,
                               const std::string &name, const std::string masterNodeId)
{
    UBSE_LOG_INFO << "fd import agent callback. name is " << name;
    auto lock = LoggingLockGuard(name);
    UbseMemOperationResp resp{.name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId};
    FdImportUpdateState(importObj, importObj.status.state);
    if (importObj.status.state == UBSE_MEM_IMPORT_RUNNING) {
        return FdImportRunningCallback(importObj, name, masterNodeId, requestNodeId);
    }
    return FdImportDestroyingAgentCallback(importObj, name, masterNodeId, requestNodeId);
}

uint32_t FdImportExpectSuccessMasterCallback(UbseMemOperationResp &resp, UbseMemFdBorrowImportObj &importObj,
                                             const std::string &name, const std::string &importNodeId,
                                             const std::string &exportNodeId)
{
    UBSE_LOG_DEBUG << "Fd import expect success callback, name is " << name;
    if (importObj.status.state == UBSE_MEM_IMPORT_SUCCESS) { // 导入成功
        FdImportUpdateState(importObj, importObj.status.state);
        for (const auto importResult : importObj.status.importResults) {
            resp.memIdList.push_back(importResult.memId);
        }
        resp.remoteNumaId = importObj.status.importResults[0].numaId;
        UbseMemFdImportObjStateChangeHandler(importObj);
        return BuildOperationRespWhenSuccess(resp, UbseMemErrorCode::CREATE_SUCCESS);
    }
    // 导入失败 开始回滚
    UBSE_LOG_DEBUG << "Failed to import, begin to rollback, name is " << name;
    mapLock.LockWrite();
    importObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    importObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    nodeMemDebtInfoMap[importNodeId].fdImportObjMap[name] = importObj;
    auto &exportObj = nodeMemDebtInfoMap[exportNodeId].fdExportObjMap[name];
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    mapLock.UnLock();
    auto copy = importObj;
    copy.status.state = UbseMemState::UBSE_MEM_STATE_FAILED; // 通知算法
    UbseMemFdImportObjStateChangeHandler(copy);
    SendFdExportObj(exportNodeId, exportObj, true);
    return BuildOperationRespWhenFail(resp, name, exportObj.req.requestNodeId, "Failed to import.",
                                      importObj.errorCode);
}

uint32_t FdImportExpectDestroyMasterCallback(UbseMemOperationResp &resp, UbseMemFdBorrowImportObj &importObj,
                                             const std::string &name, const std::string &exportNodeId,
                                             const std::string &importNodeId)
{
    UBSE_LOG_DEBUG << "Fd import expect destroy callback, name is " << name;
    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        mapLock.LockWrite();
        nodeMemDebtInfoMap[importNodeId].fdImportObjMap[name] = importObj;
        if (nodeMemDebtInfoMap[exportNodeId].fdExportObjMap.find(name) !=
            nodeMemDebtInfoMap[exportNodeId].fdExportObjMap.end()) {
            auto &exportObj = nodeMemDebtInfoMap[exportNodeId].fdExportObjMap[name];
            exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
            exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
            mapLock.UnLock();
            UbseMemFdImportObjStateChangeHandler(importObj);
            return SendFdExportObj(exportNodeId, exportObj, true);
        } else {
            mapLock.UnLock();
            UbseMemFdImportObjStateChangeHandler(importObj);
            return UBSE_OK;
        }
    }
    UBSE_LOG_DEBUG << "Failed to unimport, name is " << name;
    FdImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    SendFdImportObj(importNodeId, importObj, true);
    return BuildOperationRespWhenFail(resp, name, UbseMemReturnReqMap(name).requestNodeId, "Failed to unimport.",
                                      importObj.errorCode, BorrowedType::FD_RETURN);
}

uint32_t FdImportMasterCallback(const std::string requestNodeId, UbseMemFdBorrowImportObj &importObj,
                                const std::string &name, const std::string masterNodeId)
{
    UBSE_LOG_DEBUG << "Fd import master callback. name is " << name;
    auto lock = LoggingLockGuard(name);
    UbseMemOperationResp resp{.name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId};
    auto importNodeId = importObj.req.importNodeId;

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

uint32_t UbseMemFdBorrowImportObjCallback(const UbseMemFdBorrowImportObj &importObj)
{
    UBSE_LOG_DEBUG << "Fd import callback, name is " << importObj.req.name;
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    UbseRoleInfo masterInfo{};
    UbseGetMasterInfo(masterInfo);

    auto importNodeId = importObj.req.importNodeId;
    auto exportNodeId = importObj.algoResult.exportNumaInfos[0].nodeId;
    auto requestNodeId = importObj.req.requestNodeId;
    auto name = importObj.req.name;

    auto copy = importObj;
    // 履行侧履行
    if (importObj.req.importNodeId == currentNodeInfo.nodeId &&
        (importObj.status.state == UBSE_MEM_IMPORT_RUNNING || importObj.status.state == UBSE_MEM_IMPORT_DESTROYING)) {
        return FdImportAgentCallback(requestNodeId, copy, name, masterInfo.nodeId);
    }
    // 中心侧处理
    return FdImportMasterCallback(requestNodeId, copy, name, masterInfo.nodeId);
}

uint32_t NumaExportRunningCallback(UbseMemOperationResp &resp, UbseMemNumaBorrowExportObj &exportObj,
                                   const std::string &name, const std::string &masterNodeId,
                                   const std::string &exportNodeId, const std::string &requestNodeId)
{
    UBSE_LOG_DEBUG << "Numa export running callback. name is " << name;
    auto mmiModule = UbseContext::GetInstance().GetModule<UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        exportObj.errorCode = static_cast<uint32_t>(UbseMemErrorCode::EXPORT_FAIL);
        NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYED);
        // 返回主节点 更新
        return SendNumaExportObj(masterNodeId, exportObj, false);
    }
    if (auto ret = mmiModule->UbseMemNumaExportExecutor(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to export, name is " << name << ", requestNodeId is " << requestNodeId;
        exportObj.errorCode = UBS_ENGINE_ERR_INTERNAL;
        NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYED);
        // 返回主节点 更新
        return SendNumaExportObj(masterNodeId, exportObj, false);
    }
    if (nodeMemDebtInfoMap[exportNodeId].numaExportObjMap[name].status.state == UBSE_MEM_EXPORT_DESTROYING_WAIT) {
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
        NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
        return SendNumaExportObj(exportNodeId, exportObj, false);
    }
    NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
    return SendNumaExportObj(masterNodeId, exportObj, false);
}

uint32_t NumaExportDestroyingCallback(UbseMemOperationResp &resp, UbseMemNumaBorrowExportObj &exportObj,
                                      const std::string &name, const std::string &masterNodeId,
                                      const std::string &exportNodeId, const std::string &requestNodeId)
{
    UBSE_LOG_DEBUG << "Numa export destroying callback. name is " << name;
    auto mmiModule = UbseContext::GetInstance().GetModule<UbseMmiModule>();
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    bool directReply = nodeMemDebtInfoMap[exportNodeId].numaExportObjMap.find(name) ==
                           nodeMemDebtInfoMap[exportNodeId].numaExportObjMap.end() ||
                       nodeMemDebtInfoMap[exportNodeId].numaExportObjMap[name].status.state ==
                           UBSE_MEM_EXPORT_DESTROYED;
    if (directReply) {
        NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYED);
        return SendNumaExportObj(masterNodeId, exportObj, false);
    }
    if (nodeMemDebtInfoMap[exportNodeId].numaExportObjMap[name].status.state == UBSE_MEM_EXPORT_RUNNING) {
        exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
        NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING_WAIT);
        return UBSE_OK;
    }
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        exportObj.errorCode = UBS_ENGINE_ERR_INTERNAL;
        NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        return SendNumaExportObj(masterNodeId, exportObj, false);
    }
    if (auto ret = mmiModule->UbseMemNumaUnExportExecutor(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to unexport name is " << name;
        exportObj.errorCode = UBS_ENGINE_ERR_INTERNAL;
        NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        // 返回主节点 更新
        return SendNumaExportObj(masterNodeId, exportObj, false);
    }
    // 归还成功
    UBSE_LOG_DEBUG << "Success to unexport. name is " << name;
    NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYED);
    return SendNumaExportObj(masterNodeId, exportObj, false);
}

uint32_t NumaExportAgentCallback(const std::string &exportNodeId, UbseMemNumaBorrowExportObj &exportObj,
                                 const std::string &name, const std::string masterNodeId)
{
    UBSE_LOG_DEBUG << "Numa export agent callback. name is " << name;
    auto lock = LoggingLockGuard(name);
    UbseMemOperationResp resp{.name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId};
    auto requestNodeId = exportObj.req.requestNodeId;
    NumaExportUpdateState(exportObj, exportObj.status.state);
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

uint32_t NumaExportExpectSuccessMasterCallback(UbseMemOperationResp &resp, UbseMemNumaBorrowExportObj &exportObj,
                                               UbseMemNumaBorrowImportObj &importObj, const std::string &name,
                                               const std::string &importNodeId, const std::string &exportNodeId)
{
    UBSE_LOG_DEBUG << "Numa export expect success callback. name is " << name;
    if (exportObj.status.state == UBSE_MEM_EXPORT_SUCCESS) { // 导出成功 开始导入
        UBSE_LOG_INFO << "start to import";
        importObj.exportObmmInfo = exportObj.status.exportObmmInfo;
        UBSE_LOG_DEBUG << "socket id is " << importObj.algoResult.exportNumaInfos[0].socketId;
        auto ret = GetCnaInfoWhenImport(exportNodeId, importNodeId, importObj);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to get cna info when import" << FormatRetCode(ret);
            UbseMemOperationResp resp{};
            exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
            NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYING);
            importObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
            NumaImportUpdateState(importObj, UBSE_MEM_EXPORT_DESTROYING);
            BuildOperationRespWhenFail(resp, name, exportObj.req.requestNodeId, "Failed to get cna info when import",
                                       UBS_ENGINE_ERR_INTERNAL, BorrowedType::NUMA);
            return SendNumaExportObj(exportNodeId, exportObj, true);
        }
        NumaExportUpdateState(exportObj, exportObj.status.state);
        importObj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
        importObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
        UbseMemNumaExportObjStateChangeHandler(exportObj);
        NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_RUNNING);
        return SendNumaImportObj(importNodeId, importObj, true);
    }
    // 导出失败，回滚
    UBSE_LOG_ERROR << "Failed to export. name is " << name;
    NumaImportUpdateState(importObj, UBSE_MEM_EXPORT_DESTROYED);
    NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYED);
    auto copy = exportObj;
    copy.status.state = UbseMemState::UBSE_MEM_STATE_FAILED; // 通知算法
    UbseMemNumaExportObjStateChangeHandler(copy);
    return BuildOperationRespWhenFail(resp, name, exportObj.req.requestNodeId, "Failed to unexport.",
                                      exportObj.errorCode, BorrowedType::NUMA);
}

uint32_t NumaExportExpectDestroyMasterCallback(UbseMemOperationResp &resp, UbseMemNumaBorrowExportObj &exportObj,
                                               UbseMemNumaBorrowImportObj &importObj, const std::string &exportNodeId,
                                               const std::string &name)
{
    UBSE_LOG_DEBUG << "Numa export expect destroy callback. name is " << name;
    if (exportObj.status.state == UBSE_MEM_EXPORT_SUCCESS) {
        // 归还失败,后续由对账处理
        NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_SUCCESS);
        return BuildOperationRespWhenFail(resp, name, UbseMemReturnReqMap(name).requestNodeId, "Failed to unexport",
                                          exportObj.errorCode, BorrowedType::NUMA_RETURN);
    }
    // 归还成功
    UBSE_LOG_DEBUG << "Success to export, name is " << name << ", exportNodeId is " << exportNodeId;
    NumaImportUpdateState(importObj, UBSE_MEM_EXPORT_DESTROYED);
    NumaExportUpdateState(exportObj, UBSE_MEM_EXPORT_DESTROYED);
    auto copy = exportObj;
    UbseMemNumaExportObjStateChangeHandler(copy);
    resp.requestNodeId = UbseMemReturnReqMap(name).requestNodeId;
    if (resp.requestNodeId.empty()) {
        UBSE_LOG_ERROR << "Request node id is empty. ";
        return UBSE_ERROR;
    }
    return BuildOperationRespWhenSuccess(resp, UbseMemErrorCode::DELETE_SUCCESS, BorrowedType::NUMA_RETURN);
}

uint32_t NumaExportMasterCallback(const std::string &exportNodeId, UbseMemNumaBorrowExportObj &exportObj,
                                  const std::string &importNodeId, const std::string &name)
{
    UBSE_LOG_DEBUG << "Numa export master callback. name is " << name;
    auto lock = LoggingLockGuard(name);
    UbseMemOperationResp resp{.name = exportObj.req.name, .requestNodeId = exportObj.req.requestNodeId};
    mapLock.LockRead();
    auto importObj = nodeMemDebtInfoMap[importNodeId].numaImportObjMap[name];
    mapLock.UnLock();
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_SUCCESS) {
        return NumaExportExpectSuccessMasterCallback(resp, exportObj, importObj, name, importNodeId, exportNodeId);
    }
    // 归还逻辑
    if (exportObj.status.expectState == UBSE_MEM_EXPORT_DESTROYED) { // 归还失败
        return NumaExportExpectDestroyMasterCallback(resp, exportObj, importObj, exportNodeId, name);
    }
    return UBSE_OK;
}

uint32_t UbseMemNumaBorrowExportObjCallback(const UbseMemNumaBorrowExportObj &exportObj)
{
    UBSE_LOG_DEBUG << "Numa borrow export callback. name is " << exportObj.req.name;
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    UbseRoleInfo masterInfo{};
    UbseGetMasterInfo(masterInfo);

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

uint32_t NumaImportRunningAgentCallback(UbseMemOperationResp &resp, UbseMemNumaBorrowImportObj &importObj,
                                        const std::string &masterNodeId, const std::string &name,
                                        const std::string requestNodeId)
{
    UBSE_LOG_DEBUG << "Numa import running agent callback. name is " << name;
    auto mmiModule = UbseContext::GetInstance().GetModule<UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        importObj.errorCode = UBS_ENGINE_ERR_INTERNAL;
        NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYED);
        return SendNumaImportObj(masterNodeId, importObj, false);
    }
    if (auto ret = mmiModule->UbseMemNumaImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to import, name is " << name << ", requestNodeId is " << requestNodeId;
        importObj.errorCode = UBS_ENGINE_ERR_INTERNAL;
        NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYED);
        return SendNumaImportObj(masterNodeId, importObj, false);
    }
    // 超时处理
    if (nodeMemDebtInfoMap[requestNodeId].numaImportObjMap[name].status.state == UBSE_MEM_EXPORT_DESTROYING_WAIT) {
        NumaImportUpdateState(importObj, UBSE_MEM_EXPORT_DESTROYING);
        return SendNumaImportObj(requestNodeId, importObj, false);
    }
    UBSE_LOG_DEBUG << "Success to import, name is " << name;
    NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
    return SendNumaImportObj(masterNodeId, importObj, false);
}

uint32_t NumaImportDestroyingAgentCallback(UbseMemOperationResp &resp, UbseMemNumaBorrowImportObj &importObj,
                                           const std::string &masterNodeId, const std::string &name,
                                           const std::string requestNodeId)
{
    UBSE_LOG_DEBUG << "Numa import destroying agent callback. name is " << name;
    auto mmiModule = UbseContext::GetInstance().GetModule<UbseMmiModule>();
    // 如果Agent侧不存在或DESTROYED，则直接返回已销毁.
    bool directReply = nodeMemDebtInfoMap[importObj.req.importNodeId].numaImportObjMap.find(name) ==
                           nodeMemDebtInfoMap[importObj.req.importNodeId].numaImportObjMap.end() ||
                       nodeMemDebtInfoMap[importObj.req.importNodeId].numaImportObjMap[name].status.state ==
                           UBSE_MEM_IMPORT_DESTROYED;
    if (directReply) {
        NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYED);
        return SendNumaImportObj(masterNodeId, importObj, false);
    }
    if (nodeMemDebtInfoMap[requestNodeId].numaImportObjMap[name].status.state == UBSE_MEM_IMPORT_RUNNING) {
        importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
        NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING_WAIT);
        return UBSE_OK;
    }
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        importObj.errorCode = UBS_ENGINE_ERR_INTERNAL;
        NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
        return SendNumaImportObj(masterNodeId, importObj, false);
    }
    if (auto ret = mmiModule->UbseMemNumaUnImportExecutor(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to unimport, name is " << name;
        importObj.errorCode = UBS_ENGINE_ERR_INTERNAL;
        NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_SUCCESS);
        return SendNumaImportObj(masterNodeId, importObj, false);
    }
    UBSE_LOG_DEBUG << "Success to unimport, name is " << name;
    NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYED);
    return SendNumaImportObj(masterNodeId, importObj, false);
}

uint32_t NumaImportAgentCallback(const std::string requestNodeId, UbseMemNumaBorrowImportObj &importObj,
                                 const std::string &name, const std::string masterNodeId)
{
    UBSE_LOG_DEBUG << "numa import agent callback. name is " << name;
    auto lock = LoggingLockGuard(name);
    NumaImportUpdateState(importObj, importObj.status.state);
    UbseMemOperationResp resp{.name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId};
    if (importObj.status.state == UBSE_MEM_IMPORT_RUNNING) {
        return NumaImportRunningAgentCallback(resp, importObj, masterNodeId, name, requestNodeId);
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYING) {
        return NumaImportDestroyingAgentCallback(resp, importObj, masterNodeId, name, requestNodeId);
    }
    return UBSE_OK;
}

uint32_t NumaImportExpectSuccessMasterCallBack(UbseMemOperationResp &resp, const std::string exportNodeId,
                                               const std::string &name, const std::string importNodeId,
                                               UbseMemNumaBorrowImportObj &importObj)
{
    UBSE_LOG_DEBUG << "Numa import expect success callback. name is " << name;
    if (importObj.status.state == UBSE_MEM_IMPORT_SUCCESS) { // 导入成功
        NumaImportUpdateState(importObj, importObj.status.state);
        for (const auto importResult : importObj.status.importResults) {
            resp.memIdList.push_back(importResult.memId);
        }
        resp.remoteNumaId = importObj.status.importResults[0].numaId;
        UbseMemNumaImportObjStateChangeHandler(importObj);
        return BuildOperationRespWhenSuccess(resp, UbseMemErrorCode::CREATE_SUCCESS, BorrowedType::NUMA);
    }
    // 导入失败 开始回滚
    UBSE_LOG_ERROR << "Failed to import, name is " << name << "importNodeId is " << importNodeId;
    mapLock.LockWrite();
    importObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    importObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    nodeMemDebtInfoMap[importNodeId].numaImportObjMap[name] = importObj;
    auto &exportObj = nodeMemDebtInfoMap[exportNodeId].numaExportObjMap[name];
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
    exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    mapLock.UnLock();
    auto copy = importObj;
    copy.status.state = UBSE_MEM_STATE_FAILED;
    UbseMemNumaImportObjStateChangeHandler(copy); // 通知算法
    SendNumaExportObj(exportNodeId, exportObj, true);
    return BuildOperationRespWhenFail(resp, name, importObj.req.requestNodeId, "Failed to import", importObj.errorCode,
                                      BorrowedType::NUMA);
}

uint32_t NumaImportExpectDestroyedMasterCallBack(UbseMemOperationResp &resp, const std::string exportNodeId,
                                                 const std::string name, const std::string importNodeId,
                                                 UbseMemNumaBorrowImportObj &importObj)
{
    UBSE_LOG_DEBUG << "Numa import expect destroy callback. name is " << name;
    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        mapLock.LockWrite();
        nodeMemDebtInfoMap[importNodeId].numaImportObjMap[name] = importObj;
        if (nodeMemDebtInfoMap[exportNodeId].numaExportObjMap.find(name) !=
            nodeMemDebtInfoMap[exportNodeId].numaExportObjMap.end()) {
            auto &exportObj = nodeMemDebtInfoMap[exportNodeId].numaExportObjMap[name];
            exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
            exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
            mapLock.UnLock();
            UbseMemNumaImportObjStateChangeHandler(importObj);
            return SendNumaExportObj(exportNodeId, exportObj, true);
        } else {
            mapLock.UnLock();
            UbseMemNumaImportObjStateChangeHandler(importObj);
            return UBSE_OK;
        }
    }
    UBSE_LOG_ERROR << "Failed to unimport, name is " << name << ", importNodeId is " << importNodeId;
    NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    SendNumaImportObj(importNodeId, importObj, true);
    return BuildOperationRespWhenFail(resp, name, UbseMemReturnReqMap(name).requestNodeId, "Failed to unimport.",
                                      importObj.errorCode, BorrowedType::NUMA_RETURN);
}

uint32_t NumaImportMasterCallback(const std::string requestNodeId, UbseMemNumaBorrowImportObj &importObj,
                                  const std::string &name, const std::string masterNodeId)
{
    UBSE_LOG_DEBUG << "Numa import master callback. name is " << name;
    auto lock = LoggingLockGuard(name);
    UbseMemOperationResp resp{.name = importObj.req.name, .requestNodeId = importObj.req.requestNodeId};
    auto importNodeId = importObj.req.importNodeId;
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
    UBSE_LOG_DEBUG << "Numa borrow import callback. name is " << importObj.req.name;
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    UbseRoleInfo masterInfo{};
    UbseGetMasterInfo(masterInfo);

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

void LoadObjState(NodeMemDebtInfo &nodeMemDebtInfo)
{
    for (const auto item : nodeMemDebtInfo.fdImportObjMap) {
        nodeMemDebtInfo.fdImportObjMap[item.first].status.state = UBSE_MEM_IMPORT_SUCCESS;
        UbseMemRequestIdBorrowedTypeMapSet(item.first, BorrowedType::FD);
    }
    for (const auto item : nodeMemDebtInfo.fdExportObjMap) {
        nodeMemDebtInfo.fdExportObjMap[item.first].status.state = UBSE_MEM_EXPORT_SUCCESS;
        UbseMemRequestIdBorrowedTypeMapSet(item.first, BorrowedType::FD);
    }
    for (const auto item : nodeMemDebtInfo.numaExportObjMap) {
        nodeMemDebtInfo.numaExportObjMap[item.first].status.state = UBSE_MEM_EXPORT_SUCCESS;
        UbseMemRequestIdBorrowedTypeMapSet(item.first, BorrowedType::NUMA);
    }
    for (const auto item : nodeMemDebtInfo.numaImportObjMap) {
        nodeMemDebtInfo.numaImportObjMap[item.first].status.state = UBSE_MEM_IMPORT_SUCCESS;
        UbseMemRequestIdBorrowedTypeMapSet(item.first, BorrowedType::NUMA);
    }
}

uint32_t LoadLocalAllObjs(const ubse::nodeController::UbseNodeInfo &node)
{
    UBSE_LOG_INFO << "local node state change, state=" << static_cast<uint32_t>(node.localState);
    if (node.localState == UbseNodeLocalState::UBSE_NODE_READY) {
        UBSE_LOG_INFO << "local node ready, skip load obj";
        return UBSE_OK;
    }
    UBSE_LOG_INFO << "local node state change, start load obj";
    auto mmiModule = UbseContext::GetInstance().GetModule<UbseMmiModule>();
    if (mmiModule == nullptr) {
        UBSE_LOG_ERROR << "mmi module is nullptr.";
        return UBSE_ERROR_NULLPTR;
    }
    NodeMemDebtInfo nodeMemDebtInfo{};
    if (auto ret = mmiModule->UbseMemGetObjData(nodeMemDebtInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to load all objs from obmm.";
        return ret;
    }
    LoadObjState(nodeMemDebtInfo);
    // 启动时，无并发请求
    nodeMemDebtInfoMap[node.nodeId] = nodeMemDebtInfo;
    return UBSE_OK;
}

uint32_t UbseMemReturn(const UbseMemReturnReq &req, UbseMemOperationResp &resp)
{
    auto name = req.name;
    auto requestNodeId = req.requestNodeId;
    mapLock.LockRead();
    auto borrowedType = UbseMemRequestIdBorrowedTypeGet(req.name);
    mapLock.UnLock();
    resp.name = name;
    resp.requestNodeId = requestNodeId;
    // 根据borrowedType；取出对应对象
    if (borrowedType == BorrowedType::FD) {
        return UbseMemFdReturn(req, resp);
    } else if (borrowedType == BorrowedType::NUMA) {
        return UbseMemNumaReturn(req, resp);
    } else {
        BuildOperationRespWhenFail(resp, name, requestNodeId, "borrowed type is not right.",
                                   static_cast<uint32_t>(UbseMemErrorCode::RESOURCE_NOT_CREATE));
        return UBSE_ERROR;
    }
}

bool CheckFdReturnParameter(const UbseMemReturnReq &req)
{
    if (req.name == "") {
        UBSE_LOG_ERROR << "name is empty.";
        return false;
    }
    return true;
}

uint32_t UbseMemFdReturnDispatch(const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    UbseMemReturnReq req{};
    UbseRoleInfo masterInfo{};
    if (auto ret = UbseGetMasterInfo(masterInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get master info, " << FormatRetCode(ret);
        return ret;
    }
    UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }
    UbseMemFdDeleteReqUnpack(buffer, req);
    if (!CheckFdReturnParameter(req)) {
        return IPC_ERROR_INVALID_ARGUMENT;
    }
    req.requestNodeId = currentRoleInfo.nodeId;
    SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RETURN),
                        static_cast<uint16_t>(UbseOpCode::UBSE_MEM_RETURN)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }

    UbseMemReturnReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemReturnReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemReturnReq(req);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr";
        return UBSE_ERROR_NULLPTR;
    }
    UbseMemRequestIdSet(req.name, context.requestId);
    auto ret = comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to Send fd borrow request, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

uint32_t FdReturnExistImport(UbseMemFdBorrowImportObj &importObj, UbseMemFdBorrowExportObj &exportObj, bool hasExport,
                             const UbseMemReturnReq &req, UbseMemOperationResp &resp)
{
    auto name = req.name;
    auto requestNodeId = req.requestNodeId;
    if (importObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "Resource has destroyed.",
                                          UBS_ENGINE_ERR_NOT_EXIST, BorrowedType::FD_RETURN);
    }
    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        if (!hasExport || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
            return BuildOperationRespWhenFail(resp, name, requestNodeId, "Single import has destroyed.",
                                              UBS_ENGINE_ERR_NOT_EXIST, BorrowedType::FD_RETURN);
        }
        UbseMemReturnReqMapSet(name, req);
        exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
        return SendFdExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, exportObj, true);
    }
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    FdImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    UbseMemReturnReqMapSet(name, req);
    return SendFdImportObj(importObj.req.importNodeId, importObj, true);
}

uint32_t UbseMemFdReturn(const UbseMemReturnReq &req, UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "Start to fd return, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    auto lock = LoggingLockGuard(req.name);
    auto name = req.name;
    auto requestNodeId = req.requestNodeId;
    UbseMemFdBorrowExportObj exportObj{};
    UbseMemFdBorrowImportObj importObj{};
    bool hasImport = false;
    bool hasExport = false;
    FindBorrowObjByName<UbseMemFdBorrowImportObj, UbseMemFdBorrowExportObj>(
        req.name, importObj, exportObj, hasImport, hasExport,
        [](const NodeMemDebtInfo &info) -> const UbseMemFdImportObjMap & { return info.fdImportObjMap; },
        [](const NodeMemDebtInfo &info) -> const UbseMemFdExportObjMap & { return info.fdExportObjMap; });
    if (!hasImport && !hasExport) {
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "resource not found.", UBS_ENGINE_ERR_NOT_EXIST,
                                          BorrowedType::FD_RETURN);
    }
    if (!hasImport) {
        if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
            return BuildOperationRespWhenFail(resp, name, requestNodeId, "single export has destroyed.",
                                              UBS_ENGINE_ERR_NOT_EXIST, BorrowedType::FD_RETURN);
        }
        exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
        return SendFdExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, exportObj, true);
    }
    // 有导入
    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "Single import has destroyed.",
                                          UBS_ENGINE_ERR_NOT_EXIST, BorrowedType::FD_RETURN);
    }
    return FdReturnExistImport(importObj, exportObj, hasExport, req, resp);
}

uint32_t UbseMemNumaReturn(const UbseMemReturnReq &req, UbseMemOperationResp &resp)
{
    UBSE_LOG_INFO << "Start to numa return, name is " << req.name << ", requestNodeId is " << req.requestNodeId;
    auto lock = LoggingLockGuard(req.name);
    auto name = req.name;
    auto requestNodeId = req.requestNodeId;
    UbseMemNumaBorrowExportObj exportObj{};
    UbseMemNumaBorrowImportObj importObj{};
    bool hasImport = false;
    bool hasExport = false;
    FindBorrowObjByName<UbseMemNumaBorrowImportObj, UbseMemNumaBorrowExportObj>(
        req.name, importObj, exportObj, hasImport, hasExport,
        [](const NodeMemDebtInfo &info) -> const UbseMemNumaImportObjMap & { return info.numaImportObjMap; },
        [](const NodeMemDebtInfo &info) -> const UbseMemNumaExportObjMap & { return info.numaExportObjMap; });

    if (!hasImport && !hasExport) {
        BuildOperationRespWhenFail(resp, name, requestNodeId, "resource not found.", UBS_ENGINE_ERR_NOT_EXIST,
                                   BorrowedType::NUMA_RETURN);
        return UBSE_ERROR;
    }
    if (!hasImport) {
        if (exportObj.status.state == UBSE_MEM_EXPORT_DESTROYED) {
            return BuildOperationRespWhenFail(resp, name, requestNodeId, "Single export has destroyed.",
                                              UBS_ENGINE_ERR_NOT_EXIST, BorrowedType::NUMA_RETURN);
        }
        exportObj.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
        exportObj.status.state = UBSE_MEM_EXPORT_DESTROYING;
        return SendNumaExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, exportObj, true);
    }
    // 有导入
    if (importObj.status.state == UBSE_MEM_IMPORT_DESTROYED) {
        return BuildOperationRespWhenFail(resp, name, requestNodeId, "Single import has destroyed.",
                                          UBS_ENGINE_ERR_NOT_EXIST, BorrowedType::NUMA_RETURN);
    }
    importObj.status.expectState = UBSE_MEM_IMPORT_DESTROYED;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYING;
    NumaImportUpdateState(importObj, UBSE_MEM_IMPORT_DESTROYING);
    UbseMemReturnReqMapSet(name, req);
    return SendNumaImportObj(importObj.req.importNodeId, importObj, true);
}

uint16_t GetMarId(const std::string &portGroupIdStr)
{
    if (portGroupIdStr.empty()) {
        UBSE_LOG_WARN << "The portGroupIdStr=" << portGroupIdStr;
        return 0;
    }
#ifdef UB_ENVIRONMENT
    uint64_t value{};
    auto ret = ubse::utils::ConvertStrToUint64(portGroupIdStr, value);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "portGroupIdStr(" << portGroupIdStr << ") is invalid.";
        return 0;
    }
    if (value > UINT16_MAX) {
        UBSE_LOG_ERROR << "portGroupIdStr(" << portGroupIdStr << ") is invalid.";
        return 0;
    }
    const auto marId = static_cast<uint16_t>(value) / 4u;
    return marId;
#else
    return 0;
#endif
}
uint32_t GetCnaInfoWhenImport(const std::string &exportNodeId, const std::string &importNodeId,
                              UbseMemBorrowImportBaseObj &importObj)
{
    UbseNodeMemCnaInfoInput cnaInput;
    cnaInput.exportNodeId = exportNodeId;
    UBSE_LOG_DEBUG << "exportNodeId=" << cnaInput.exportNodeId;

    cnaInput.borrowNodeId = importNodeId;
    UBSE_LOG_DEBUG << "importNodeId=" << cnaInput.borrowNodeId;
    if (!importObj.algoResult.exportNumaInfos.empty()) {
        cnaInput.exportSocketId = std::to_string(importObj.algoResult.exportNumaInfos[0].socketId);
    }
    if ((cnaInput.exportNodeId.empty() || cnaInput.borrowNodeId == cnaInput.exportNodeId) &&
        !importObj.algoResult.exportNumaInfos.empty()) {
        cnaInput.exportNodeId = importObj.algoResult.exportNumaInfos[0].nodeId;
    }
    UBSE_LOG_DEBUG << "exeSocketId=" << cnaInput.exportSocketId;
    UbseNodeMemCnaInfoOutput cnaOutput;
    auto ret = UbseNodeMemGetTopologyCnaInfo(cnaInput, cnaOutput);
    if (ret != 0) {
        UBSE_LOG_ERROR << "Failed to get cna, borrowNodeId=" << cnaInput.borrowNodeId
                       << ", exportNodeId=" << cnaInput.exportNodeId;
        return E_CODE_SCBUS_DAEMON;
    }
    for (auto &newObmmDesc : importObj.exportObmmInfo) {
        // mar_id为port_id除4。port 0-3对应mar_id 0，port 4-7对应mar_id 1, port 8对应mar_id 2
        newObmmDesc.desc.scna = cnaOutput.borrowNodeCna;
        newObmmDesc.desc.dcna = cnaOutput.exportNodeCna;
        newObmmDesc.desc.marId = GetMarId(cnaOutput.portGroupId);
        uint32_t srcSocketId = 0;
        auto ret = ConvertStrToUint32(cnaOutput.borrowSocketId, srcSocketId);
        ret |= UbseNodeController::GetInstance().GetEid(cnaInput.borrowNodeId, srcSocketId, newObmmDesc.desc.seid);
        if ((newObmmDesc.desc.scna == INVALID_VALUE_CNA) || (newObmmDesc.desc.dcna == INVALID_VALUE_CNA) ||
            ret != UBSE_OK) {
            UBSE_LOG_ERROR << "failed to get cna or mar id or eid, scna=" << newObmmDesc.desc.scna
                           << ", marId=" << newObmmDesc.desc.marId << ", dcna=" << newObmmDesc.desc.dcna
                           << ", seid=" << newObmmDesc.desc.seid << ", deid=" << newObmmDesc.desc.deid;
            return E_CODE_AGENT;
        }
        UBSE_LOG_DEBUG << "Obmm portGroupId=" << cnaOutput.portGroupId << ", scna=" << newObmmDesc.desc.scna
                       << ", marId=" << newObmmDesc.desc.marId << ", dcna=" << newObmmDesc.desc.dcna
                       << ", seid=" << newObmmDesc.desc.seid << ", deid=" << newObmmDesc.desc.deid;
    }
    return UBSE_OK;
}

template <typename ImportObjType, typename ExportObjType>
void FindBorrowObjByName(
    const std::string &name,
    ImportObjType &outImportObj, // 输出参数
    ExportObjType &outExportObj, // 输出参数
    bool &foundImport,           // 指示是否找到导入对象
    bool &foundExport,           // 指示是否找到导出对象
    const std::function<const std::unordered_map<std::string, ImportObjType> &(const NodeMemDebtInfo &)> &getImportMap,
    const std::function<const std::unordered_map<std::string, ExportObjType> &(const NodeMemDebtInfo &)> &getExportMap)
{
    mapLock.LockRead();
    foundImport = false;
    foundExport = false;
    for (const auto &[nodeId, info] : nodeMemDebtInfoMap) {
        if (!foundImport) {
            const auto &importMap = getImportMap(info);
            auto it = importMap.find(name);
            if (it != importMap.end()) {
                outImportObj = it->second; // 拷贝对象
                foundImport = true;
            }
        }

        if (!foundExport) {
            const auto &exportMap = getExportMap(info);
            auto it = exportMap.find(name);
            if (it != exportMap.end()) {
                outExportObj = it->second; // 拷贝对象
                foundExport = true;
            }
        }

        if (foundImport && foundExport) {
            break;
        }
    }
    mapLock.UnLock();
}

NodeMemDebtInfoMap GetNodeMemDebtInfoMap()
{
    mapLock.LockRead();
    NodeMemDebtInfoMap map = nodeMemDebtInfoMap;
    mapLock.UnLock();
    return std::move(map);
}

uint32_t GetNodeMemDebtInfoById(const std::string &nodeId, NodeMemDebtInfo &info)
{
    mapLock.LockRead();
    auto it = nodeMemDebtInfoMap.find(nodeId);
    if (it == nodeMemDebtInfoMap.end()) {
        mapLock.UnLock();
        return UBSE_ERROR_NULLPTR;
    }
    info = it->second;
    mapLock.UnLock();
    return UBSE_OK;
}

uint32_t DeleteFdExport(const UbseMemFdBorrowExportObj &exportObj)
{
    auto copy = exportObj;
    copy.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    copy.status.state = UBSE_MEM_EXPORT_DESTROYING;
    FdExportUpdateState(copy, UBSE_MEM_EXPORT_DESTROYED);
    UBSE_LOG_INFO << "Force delete. name=" << copy.req.name;
    return SendFdExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, copy, true);
}

uint32_t DeleteNumaExport(const UbseMemNumaBorrowExportObj &exportObj)
{
    auto copy = exportObj;
    copy.status.expectState = UBSE_MEM_EXPORT_DESTROYED;
    copy.status.state = UBSE_MEM_EXPORT_DESTROYING;
    NumaExportUpdateState(copy, UBSE_MEM_EXPORT_DESTROYED);
    UBSE_LOG_INFO << "Force delete. name=" << copy.req.name;
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "Export numa infos is empty";
        return UBSE_ERROR_NULLPTR;
    }
    return SendNumaExportObj(exportObj.algoResult.exportNumaInfos[0].nodeId, copy, true);
}

uint32_t AddFdImport(const UbseMemFdBorrowImportObj &importObj)
{
    auto copy = importObj;
    UBSE_LOG_INFO << "Add fd import, name=" << copy.req.name;
    FdImportUpdateState(copy, copy.status.state);
    UbseMemRequestIdBorrowedTypeMapSet(copy.req.name, BorrowedType::FD);
    return UBSE_OK;
}

uint32_t AddFdExport(const UbseMemFdBorrowExportObj &exportObj)
{
    auto copy = exportObj;
    UBSE_LOG_INFO << "Add fd export, name=" << copy.req.name;
    FdExportUpdateState(copy, copy.status.state);
    UbseMemRequestIdBorrowedTypeMapSet(copy.req.name, BorrowedType::FD);
    return UBSE_OK;
}

uint32_t AddNumaImport(const UbseMemNumaBorrowImportObj &importObj)
{
    auto copy = importObj;
    UBSE_LOG_INFO << "Add numa import, name=" << copy.req.name;
    NumaImportUpdateState(copy, copy.status.state);
    UbseMemRequestIdBorrowedTypeMapSet(copy.req.name, BorrowedType::NUMA);
    return UBSE_OK;
}

uint32_t AddNumaExport(const UbseMemNumaBorrowExportObj &exportObj)
{
    auto copy = exportObj;
    UBSE_LOG_INFO << "Add numa export, name=" << copy.req.name;
    NumaExportUpdateState(copy, copy.status.state);
    UbseMemRequestIdBorrowedTypeMapSet(copy.req.name, BorrowedType::NUMA);
    return UBSE_OK;
}

bool CheckNumaCreateParameter(const UbseMemNumaBorrowReq &req)
{
    if (req.name == "") {
        UBSE_LOG_ERROR << "Name is empty.";
        return false;
    }
    if (req.size < 128ULL * 1024ULL * 1024ULL || req.size > 100ULL * 1024ULL * 1024ULL * 1024ULL) {
        UBSE_LOG_ERROR << "Size is out of range";
        return false;
    }
    return true;
}

UbseResult UbseMemNumaCreateHandler(const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    // 获取主节点以及当前节点
    UbseRoleInfo masterInfo{};
    if (auto ret = UbseGetMasterInfo(masterInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get master info, " << FormatRetCode(ret);
        return ret;
    }
    UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }
    // buffer 转结构
    UbseMemNumaBorrowReq req{};
    UbseMemNumaCreateReqUnpack(buffer, req);
    if (!CheckNumaCreateParameter(req)) {
        return IPC_ERROR_INVALID_ARGUMENT;
    }
    req.importNodeId = currentRoleInfo.nodeId;
    req.requestNodeId = currentRoleInfo.nodeId;
    UbseMemNumaBorrowReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemNumaBorrowReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemNumaBorrowReq(req);
    UbseMemRequestIdSet(req.name, context.requestId);

    // 不是master调用RPC异步发送
    if (currentRoleInfo.nodeId != masterInfo.nodeId) {
        UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
        if (ubseResponsePtr == nullptr) {
            UBSE_LOG_ERROR << "Failed to new ptr";
            return UBSE_ERROR_NULLPTR;
        }
        SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_NUMA_BORROW),
                            static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_BORROW)};

        auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
        if (comModule == nullptr) {
            UBSE_LOG_ERROR << "Getting ComModule failed.";
            return UBSE_ERROR_NULLPTR;
        }
        auto ret = comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to Send to numa borrow req to master, ret " << FormatRetCode(ret);
            return ret;
        }
        UBSE_LOG_DEBUG << "Success to Send to numa borrow req to master";
        return ret;
    }
    // 是master，切换线程，提交给新线程
    DoNumaBorrowAsync(ubseRequestPtr.Get());
    return UBSE_OK;
}

UbseResult UbseMemNumaCreateWithLender(const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    // 获取主节点以及当前节点
    UbseRoleInfo masterInfo{};
    if (auto ret = UbseGetMasterInfo(masterInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get master info, " << FormatRetCode(ret);
        return ret;
    }
    UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }

    // buffer 转结构
    UbseMemNumaBorrowReq req{};
    UbseMemNumaCreateLenderReqUnpack(buffer, req);
    if (!CheckNumaCreateParameter(req)) {
        return IPC_ERROR_INVALID_ARGUMENT;
    }
    UbseMemNumaBorrowReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemNumaBorrowReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr";
        return UBSE_ERROR_NULLPTR;
    }
    req.importNodeId = currentRoleInfo.nodeId;
    req.requestNodeId = currentRoleInfo.nodeId;
    ubseRequestPtr->SetUbseMemNumaBorrowReq(req);
    UbseMemRequestIdSet(req.name, context.requestId);
    // 不是master调用RPC异步发送
    if (currentRoleInfo.nodeId != masterInfo.nodeId) {
        UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
        if (ubseResponsePtr == nullptr) {
            UBSE_LOG_ERROR << "Failed to new ptr";
            return UBSE_ERROR_NULLPTR;
        }
        SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_NUMA_BORROW),
                            static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_BORROW)};

        auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
        if (comModule == nullptr) {
            UBSE_LOG_ERROR << "Getting ComModule failed.";
            return UBSE_ERROR_NULLPTR;
        }
        auto ret = comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to Send to numa borrow req to master, ret " << FormatRetCode(ret);
            return ret;
        }
        UBSE_LOG_DEBUG << "Success to Send to numa borrow req to master";
        return ret;
    }
    // 是master，切换线程，提交给新线程
    DoNumaBorrowAsync(ubseRequestPtr.Get());
    return UBSE_OK;
}

UbseResult UbseMemNumaDelete(const UbseIpcMessage &buffer, const UbseRequestContext &context)
{
    // 获取主节点以及当前节点
    UbseRoleInfo masterInfo{};
    if (auto ret = UbseGetMasterInfo(masterInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get master info, " << FormatRetCode(ret);
        return ret;
    }
    UbseRoleInfo currentRoleInfo{};
    if (auto ret = UbseGetCurrentNodeInfo(currentRoleInfo); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to get current node info, " << FormatRetCode(ret);
        return ret;
    }
    // buffer 转结构
    UbseMemReturnReq req{};
    UbseMemNumaDeleteUnPack(buffer, req);
    UbseMemRequestIdSet(req.name, context.requestId);
    req.requestNodeId = currentRoleInfo.nodeId;
    UbseMemReturnReqSimpoPtr ubseRequestPtr = new (std::nothrow) UbseMemReturnReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetUbseMemReturnReq(req);
    // 不是master调用RPC异步发送
    if (currentRoleInfo.nodeId != masterInfo.nodeId) {
        UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) UbseMemCallbackMessage();
        if (ubseResponsePtr == nullptr) {
            UBSE_LOG_ERROR << "Failed to new ptr";
            return UBSE_ERROR_NULLPTR;
        }
        SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RETURN),
                            static_cast<uint16_t>(UbseOpCode::UBSE_MEM_RETURN)};

        auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
        if (comModule == nullptr) {
            UBSE_LOG_ERROR << "Getting ComModule failed.";
            return UBSE_ERROR_NULLPTR;
        }
        auto ret = comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "Failed to Send to numa return req to master, ret " << FormatRetCode(ret);
            return ret;
        }
        UBSE_LOG_DEBUG << "Success to Send to numa return req to master";
        return ret;
    }
    // 是master，切换线程，提交给新线程
    DoReturnAsync(ubseRequestPtr.Get());
    return UBSE_OK;
}

UbseResult UbseMemNumaBorrowRespHandler(const UbseMemOperationResp &resp)
{
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServer == nullptr) {
        UBSE_LOG_ERROR << "Failed to get api server";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage message{};
    // 取requestrId， 发给ApiServer
    auto requestId = UbseMemRequestIdGet(resp.name);
    // 结构转换
    UbseMemNumaCreateResponsePack(resp, message);
    auto ret = apiServer->SendResponse(resp.errorCode, requestId, message);
    delete[] message.buffer;
    return ret;
}

UbseResult UbseMemNumaReturnRespHandler(const UbseMemOperationResp &resp)
{
    auto apiServer = ubse::context::UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServer == nullptr) {
        UBSE_LOG_ERROR << "Failed to get api server";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage message{};
    // 取requestrId， 发给ApiServer
    auto requestId = UbseMemRequestIdGet(resp.name);
    const size_t size = sizeof(uint32_t);
    message.buffer = new (std::nothrow) uint8_t[size];
    if (message.buffer == nullptr) {
        UBSE_LOG_ERROR << "Malloc memory failed";
        return UBSE_ERROR_NULLPTR;
    }
    uint8_t *ptr = message.buffer;
    *(uint32_t *)ptr = htonl(resp.errorCode);
    ptr += sizeof(uint32_t);
    message.length = size;
    auto freeFunc = [](void *p) {
        delete[] static_cast<uint8_t *>(p);
        p = nullptr;
    };
    auto ret = apiServer->SendResponse(resp.errorCode, requestId, message);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send response, error code is " << resp.errorCode << "requestId is " << requestId;
    }
    freeFunc(message.buffer);
    return ret;
}

UbseResult GetNdeoMemDebtInfoMap(const std::string &nodeId, NodeMemDebtInfoMap &memDebtInfoMap)
{
    ubse::election::UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(res);
        return res;
    }
    ubse::com::SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_CONTROLLER),
                                   static_cast<uint16_t>(UbseOpCode::UBSE_MEM_DEBINFO_QUERY)};

    NodeMemDebtInfoQueryReqSimpoPtr ubseRequestPtr = new (std::nothrow) NodeMemDebtInfoQueryReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr";
        return UBSE_ERROR_NULLPTR;
    }
    ubseRequestPtr->SetNodeId(nodeId);
    UbseBaseMessagePtr ubseResponsePtr = new (std::nothrow) NodeMemDebtInfoSimpo();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to new ptr";
        return UBSE_ERROR_NULLPTR;
    }
    UbseContext &ubseContext = UbseContext::GetInstance();

    auto ubseComModule = ubseContext.GetModule<UbseComModule>();
    if (ubseComModule == nullptr) {
        UBSE_LOG_ERROR << "Communication module not init, " << FormatRetCode(UBSE_ERROR_MODULE_LOAD_FAILED);
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto retCode = ubseComModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr, false);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "rpc sync send metric failed, " << FormatRetCode(retCode);
        return retCode;
    }
    auto nodeMemDebtInfoSimpoPtr = UbseBaseMessage::DeConvert<NodeMemDebtInfoSimpo>(ubseResponsePtr);
    memDebtInfoMap = nodeMemDebtInfoSimpoPtr->GetNodeMemDebtInfoMap();
    return UBSE_OK;
}
} // namespace ubse::mem::controller