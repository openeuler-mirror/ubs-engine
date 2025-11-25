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

#include "ubse_mem_rpc.h"
#include <netinet/in.h>
#include <filesystem>
#include <string>
#include <vector>
#include "src/framework/ha/ubse_election_module.h"
#include "ubse_com_module.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_obj.h"
#include "ubse_mem_resource.h"
#include "ubse_mem_utils.h"
#include "ubse_topology_interface.h"

#include "message/ubse_mem_fd_borrow_exportobj_simpo.h"
#include "message/ubse_mem_fd_borrow_importobj_simpo.h"
#include "message/ubse_mem_fd_borrow_req_simpo.h"
#include "message/ubse_mem_numa_borrow_exportobj_simpo.h"
#include "message/ubse_mem_numa_borrow_importobj_simpo.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"
#include "message/ubse_mem_operation_resp_simpo.h"
#include "message/ubse_mem_return_req_simpo.h"
#include "ubse_mem_api_convert.h"
#include "ubse_mem_rpc_to_controller.h"

namespace ubse::mem::controller {
using namespace ubse::com;
using namespace ubse::utils;
using namespace ubse::election;
using namespace ubse::mem::controller::message;
using namespace ubse::mem::utils;
using namespace ubse::mem::obj;
using namespace ubse::mem::controller;
using namespace ubse::context;
using namespace api::server;

const std::string SYNC_SUCCESS = "sync_success";
const std::string SYNC_FAILED = "sync_failed";

UbseResult UbseMemFdBorrowMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                 UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemFdBorrowReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);

    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }

    // 使用线程池异步执行
    auto fdBorrowReq = request->GetUbseMemFdBorrowReq();
    resourceExecutor->Execute([fdBorrowReq]() {
        UbseMemOperationResp resp{};
        UbseMemFdBorrow(fdBorrowReq, resp);
    });
    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemFdBorrowMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_BORROW);
}

uint16_t UbseMemFdBorrowMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_FD_BORROW);
}

UbseResult UbseMemNumaBorrowMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                   UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemNumaBorrowReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    auto ret = DoNumaBorrowAsync(request.Get());
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "do numa borrow async failed. ret:" << FormatRetCode(ret);
        response->data = SYNC_FAILED;
        return ret;
    }
    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemNumaBorrowMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_BORROW);
}

uint16_t UbseMemNumaBorrowMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_NUMA_BORROW);
}

UbseResult UbseMemReturnMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                               UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemReturnReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    auto ret = DoReturnAsync(request.Get());
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "do mem return async failed. ret:" << FormatRetCode(ret);
        response->data = SYNC_FAILED;
        return ret;
    }
    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemReturnMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseOpCode::UBSE_MEM_RETURN);
}

uint16_t UbseMemReturnMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RETURN);
}

UbseMemCallbackMessage::UbseMemCallbackMessage() {}

UbseMemCallbackMessage::UbseMemCallbackMessage(std::string dataRaw) : data(dataRaw) {}

UbseResult UbseMemCallbackMessage::Serialize()
{
    mOutputRawDataSize = data.size();
    mOutputRawData = std::make_unique<uint8_t[]>(mOutputRawDataSize);
    if (mOutputRawDataSize == 0) {
        return UBSE_OK;
    }
    if (auto ret = memcpy_s(mOutputRawData.get(), mOutputRawDataSize, data.data(), mOutputRawDataSize); ret != 0) {
        UBSE_LOG_ERROR << "Serialize failed with memcpy_s error code: " << ret;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseMemCallbackMessage::Deserialize()
{
    const uint8_t *inputBuffer = mInputRawData.get();
    for (int i = 0; i < mInputRawDataSize; i++) {
        data.push_back(static_cast<char>(inputBuffer[i]));
    }
    return UBSE_OK;
}

UbseResult UbseMemFdBorrowExportObjCallbackMessageHandler::Handle(const UbseBaseMessagePtr &req,
    const UbseBaseMessagePtr &rsp, UbseComBaseMessageHandlerCtxPtr ctx)
{
    UBSE_LOG_INFO << "receive fd export";
    auto request = UbseBaseMessage::DeConvert<UbseMemFdBorrowExportobjSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);

    UBSE_LOG_INFO << "Received FdexportObj, name is " << request->GetUbseMemFdBorrowExportObj().req.name <<
        ", requestNodeId is " << request->GetUbseMemFdBorrowExportObj().req.requestNodeId;

    auto exportObj = request->GetUbseMemFdBorrowExportObj();
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "Get exportNodeId fail, " << FormatRetCode(UBSE_MASTER_EMPTY_VECTOR_ERROR);
        return UBSE_MASTER_EMPTY_VECTOR_ERROR;
    }
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;

    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    response->data = SYNC_SUCCESS;

    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    auto fdBorrowExportObj = request->GetUbseMemFdBorrowExportObj();
    resourceExecutor->Execute([fdBorrowExportObj]() { UbseMemFdBorrowExportObjCallback(fdBorrowExportObj); });
    return UBSE_OK;
}

uint16_t UbseMemFdBorrowExportObjCallbackMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_BORROW_EXPORT_OBJ_CALLBACK);
}

uint16_t UbseMemFdBorrowExportObjCallbackMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_FD_BORROW_EXPORT_OBJ_CALLBACK);
}

UbseResult UbseMemFdBorrowImportObjCallbackMessageHandler::Handle(const UbseBaseMessagePtr &req,
                                                                  const UbseBaseMessagePtr &rsp,
                                                                  UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemFdBorrowImportobjSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);

    UBSE_LOG_INFO << "Received FdImportObj, name is " << request->GetUbseMemFdBorrowImportObj().req.name
                  << ", requestNodeId is " << request->GetUbseMemFdBorrowImportObj().req.requestNodeId;

    auto importObj = request->GetUbseMemFdBorrowImportObj();
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    response->data = SYNC_SUCCESS;

    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    auto fdBorrowImportObj = request->GetUbseMemFdBorrowImportObj();
    resourceExecutor->Execute([fdBorrowImportObj]() { UbseMemFdBorrowImportObjCallback(fdBorrowImportObj); });
    return UBSE_OK;
}

uint16_t UbseMemFdBorrowImportObjCallbackMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_FD_BORROW_IMPORT_OBJ_CALLBACK);
}

uint16_t UbseMemFdBorrowImportObjCallbackMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_FD_BORROW_IMPORT_OBJ_CALLBACK);
}

UbseResult UbseMemNumaBorrowExportObjCallbackMessageHandler::Handle(const UbseBaseMessagePtr &req,
    const UbseBaseMessagePtr &rsp, UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemNumaBorrowExportobjSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);

    auto exportObj = request->GetUbseMemNumaBorrowExportObj();
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    response->data = SYNC_SUCCESS;
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "Get exportNodeId fail, " << FormatRetCode(UBSE_MASTER_EMPTY_VECTOR_ERROR);
        return UBSE_MASTER_EMPTY_VECTOR_ERROR;
    }
    auto exportNodeId = exportObj.algoResult.exportNumaInfos[0].nodeId;
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    auto numaBorrowExportObj = request->GetUbseMemNumaBorrowExportObj();
    resourceExecutor->Execute([numaBorrowExportObj]() { UbseMemNumaBorrowExportObjCallback(numaBorrowExportObj); });
    return UBSE_OK;
}

uint16_t UbseMemNumaBorrowExportObjCallbackMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_BORROW_EXPORT_OBJ_CALLBACK);
}

uint16_t UbseMemNumaBorrowExportObjCallbackMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_NUMA_BORROW_EXPORT_OBJ_CALLBACK);
}

UbseResult UbseMemNumaBorrowImportObjCallbackMessageHandler::Handle(const UbseBaseMessagePtr &req,
                                                                    const UbseBaseMessagePtr &rsp,
                                                                    UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemNumaBorrowImportobjSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);

    auto importObj = request->GetUbseMemNumaBorrowImportObj();
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    response->data = SYNC_SUCCESS;
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    auto numaBorrowImportObj = request->GetUbseMemNumaBorrowImportObj();
    resourceExecutor->Execute([numaBorrowImportObj]() { UbseMemNumaBorrowImportObjCallback(numaBorrowImportObj); });
    return UBSE_OK;
}

uint16_t UbseMemNumaBorrowImportObjCallbackMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_BORROW_IMPORT_OBJ_CALLBACK);
}

uint16_t UbseMemNumaBorrowImportObjCallbackMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_NUMA_BORROW_IMPORT_OBJ_CALLBACK);
}

UbseResult UbseMemFdBorrowRespMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                     UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemOperationRespSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    response->data = SYNC_SUCCESS;
    auto operationResp = request->GetUbseMemOperationResp();
    UBSE_LOG_DEBUG << "Receive fd borrow resp. name is " << operationResp.name << "requestId is "
                   << operationResp.requestNodeId;

    auto apiServer = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServer == nullptr) {
        UBSE_LOG_ERROR << "Failed to get api server";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage message{};
    auto requestId = UbseMemRequestIdGet(operationResp.name);
    UbseMemFdCreateResponsePack(operationResp, message);
    auto freeFunc = [](void *p) {
        delete[] static_cast<uint8_t *>(p);
        p = nullptr;
    };
    auto ret = apiServer->SendResponse(operationResp.errorCode, requestId, message);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send response, error code is " << operationResp.errorCode << "requestId is "
                       << requestId;
    }
    freeFunc(message.buffer);
    return ret;
}

uint16_t UbseMemFdBorrowRespMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_BORROW_RESP);
}
uint16_t UbseMemFdBorrowRespMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_CONTROLLER);
}

UbseResult UbseMemFdReturnRespMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                     UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemOperationRespSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    response->data = SYNC_SUCCESS;
    auto operationResp = request->GetUbseMemOperationResp();
    UBSE_LOG_DEBUG << "Receive fd return resp. name is " << operationResp.name << "requestId is "
                   << operationResp.requestNodeId;
    auto apiServer = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServer == nullptr) {
        UBSE_LOG_ERROR << "Failed to get api server";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage message{};
    auto requestId = UbseMemRequestIdGet(operationResp.name);
    const size_t size = sizeof(uint32_t);
    message.buffer = new uint8_t[size];
    if (message.buffer == nullptr) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }
    uint8_t *ptr = message.buffer;
    *(uint32_t *)ptr = htonl(operationResp.errorCode);
    ptr += sizeof(uint32_t);
    message.length = size;
    auto freeFunc = [](void *p) {
        delete[] static_cast<uint8_t *>(p);
        p = nullptr;
    };
    auto ret = apiServer->SendResponse(operationResp.errorCode, requestId, message);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send response, error code is " << operationResp.errorCode << "requestId is "
                       << requestId;
    }
    freeFunc(message.buffer);
    return ret;
}

uint16_t UbseMemFdReturnRespMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseOpCode::UBSE_MEM_FD_RETURN);
}
uint16_t UbseMemFdReturnRespMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_CONTROLLER);
}

UbseResult RegFdHandler()
{
    auto comModule = UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        return UBSE_ERROR;
    }
    UbseComBaseMessageHandlerPtr hdl = new (std::nothrow) UbseMemFdBorrowMessageHandler();
    if (hdl == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RegRpcService<UbseMemFdBorrowReqSimpo, UbseMemCallbackMessage>(hdl);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemFdBorrowMessageHandler";
        return ret;
    }
    UbseComBaseMessageHandlerPtr fdExportObjHandler = new (std::nothrow)
        UbseMemFdBorrowExportObjCallbackMessageHandler();
    if (fdExportObjHandler == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RegRpcService<UbseMemFdBorrowExportobjSimpo, UbseMemCallbackMessage>(fdExportObjHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemFdBorrowExportObjCallbackMessageHandler";
        return ret;
    }
    UbseComBaseMessageHandlerPtr fdImportObjHandler = new (std::nothrow)
        UbseMemFdBorrowImportObjCallbackMessageHandler();
    if (fdImportObjHandler == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RegRpcService<UbseMemFdBorrowImportobjSimpo, UbseMemCallbackMessage>(fdImportObjHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemFdBorrowImportObjCallbackMessageHandler";
        return ret;
    }
    UbseComBaseMessageHandlerPtr fdBorrowRespHandler = new (std::nothrow) UbseMemFdBorrowRespMessageHandler();
    if (fdBorrowRespHandler == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>(fdBorrowRespHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemFdBorrowRespMessageHandler";
        return ret;
    }
    return UBSE_OK;
}

UbseResult RegNumaHandler()
{
    auto comModule = UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        return UBSE_ERROR;
    }
    UbseComBaseMessageHandlerPtr numaBorrowMessageHandler = new (std::nothrow) UbseMemNumaBorrowMessageHandler();
    if (numaBorrowMessageHandler == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RegRpcService<UbseMemNumaBorrowReqSimpo, UbseMemCallbackMessage>(numaBorrowMessageHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemNumaBorrowMessageHandler";
        return ret;
    }
    UbseComBaseMessageHandlerPtr numaExportObjHander = new (std::nothrow)
        UbseMemNumaBorrowExportObjCallbackMessageHandler();
    if (numaExportObjHander == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RegRpcService<UbseMemNumaBorrowExportobjSimpo, UbseMemCallbackMessage>(numaExportObjHander);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemNumaBorrowExportObjCallbackMessageHandler";
        return ret;
    }
    UbseComBaseMessageHandlerPtr numaImportObjHandler = new (std::nothrow)
        UbseMemNumaBorrowImportObjCallbackMessageHandler();
    if (numaImportObjHandler == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RegRpcService<UbseMemNumaBorrowImportobjSimpo, UbseMemCallbackMessage>(numaImportObjHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemNumaBorrowImportObjCallbackMessageHandler";
        return ret;
    }
    UbseComBaseMessageHandlerPtr numaBorrowRespHandler = new (std::nothrow) UbseMemNumaBorrowRespMessageHandler();
    if (numaBorrowRespHandler == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>(numaBorrowRespHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemNumaBorrowRespMessageHandler";
        return ret;
    }
    return UBSE_OK;
}

UbseResult MemScheduleHandler::RegHandler()
{
    auto comModule = UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        return UBSE_ERROR;
    }
    UbseComBaseMessageHandlerPtr returnMessageHandler = new (std::nothrow) UbseMemReturnMessageHandler();
    if (returnMessageHandler == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = comModule->RegRpcService<UbseMemReturnReqSimpo, UbseMemCallbackMessage>(returnMessageHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemShareDetachMessageHandler";
        return ret;
    }
    UbseComBaseMessageHandlerPtr fdreturnRespHandler = new (std::nothrow) UbseMemFdReturnRespMessageHandler();
    if (fdreturnRespHandler == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>(fdreturnRespHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemFdReturnRespMessageHandler";
        return ret;
    }
    UbseComBaseMessageHandlerPtr numaReturnRespHandler = new (std::nothrow) UbseMemNumaReturnRespMessageHandler();
    if (numaReturnRespHandler == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ret = comModule->RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>(numaReturnRespHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemNumaReturnRespMessageHandler";
        return ret;
    }
    ret = RegFdHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to reg fd handler, " << FormatRetCode(ret);
        return ret;
    }
    ret = RegNumaHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to reg numa handler, " << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseMemNumaBorrowRespMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                       UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemOperationRespSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    auto ret = DoNumaBorrowRespAsync(request.Get());
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "do numa borrow resp async failed. ret:" << FormatRetCode(ret);
        response->data = SYNC_FAILED;
        return ret;
    }
    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemNumaBorrowRespMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_BORROW_RESP);
}

uint16_t UbseMemNumaBorrowRespMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_CONTROLLER);
}

UbseResult UbseMemNumaReturnRespMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                       UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemOperationRespSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    auto ret = DoReturnRespAsync(request.Get());
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "do return resp async failed. ret:" << FormatRetCode(ret);
        response->data = SYNC_FAILED;
        return ret;
    }
    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemNumaReturnRespMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseOpCode::UBSE_MEM_NUMA_RETURN);
}

uint16_t UbseMemNumaReturnRespMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_CONTROLLER);
}
} // namespace ubse::mem::controller