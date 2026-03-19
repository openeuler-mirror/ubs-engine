// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#include "ubse_mem_rpc_processor.h"

#include <filesystem>
#include <string>
#include <vector>

#include "message/ubse_mem_addr_borrow_exportobj_simpo.h"
#include "message/ubse_mem_addr_borrow_importobj_simpo.h"
#include "message/ubse_mem_addr_borrow_req_simpo.h"
#include "message/ubse_mem_fd_borrow_exportobj_simpo.h"
#include "message/ubse_mem_fd_borrow_importobj_simpo.h"
#include "message/ubse_mem_fd_borrow_req_simpo.h"
#include "message/ubse_mem_numa_borrow_exportobj_simpo.h"
#include "message/ubse_mem_numa_borrow_importobj_simpo.h"
#include "message/ubse_mem_numa_borrow_req_simpo.h"
#include "message/ubse_mem_operation_resp_simpo.h"
#include "message/ubse_mem_return_req_simpo.h"
#include "message/ubse_mem_share_attach_req_simpo.h"
#include "message/ubse_mem_share_borrow_exportobj_simpo.h"
#include "message/ubse_mem_share_borrow_importobj_simpo.h"
#include "message/ubse_mem_share_borrow_req_simpo.h"
#include "message/ubse_mem_share_detach_req_simpo.h"
#include "trace_context.h"
#include "ubse_api_server_module.h"
#include "ubse_com_module.h"
#include "ubse_election.h"
#include "ubse_mem_agent_task_manager.h"
#include "ubse_mem_async_processor.h"
#include "ubse_mem_buffer_convert.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_query_api.h"
#include "ubse_mem_util.h"
#include "ubse_mmi_interface.h"
#include "ubse_str_util.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::com;
using namespace ubse::utils;
using namespace ubse::election;
using namespace ubse::mem::controller::message;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::mem::controller;
using namespace ubse::context;
using namespace api::server;
using namespace ubse::mem::util;
using namespace ubse::mem::serial;

const std::string SYNC_SUCCESS = "sync_success";
const std::string SYNC_FAILED = "sync_failed";

UbseResult UbseMemFdBorrowMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                 UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemFdBorrowReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Failed to get ubseMemController";
        return UBSE_ERROR;
    }

    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    resourceExecutor->Execute([request, traceId]() {
        TraceContext::SetTraceId(traceId);
        UbseMemOperationResp resp{};
        UbseMemFdBorrow(request->GetUbseMemFdBorrowReq(), resp);
        TraceContext::Clear();
    });
    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemFdBorrowMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_FD_BORROW);
}

uint16_t UbseMemFdBorrowMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
}

UbseResult UbseMemNumaBorrowMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                   UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemNumaBorrowReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    resourceExecutor->Execute([request, traceId]() {
        TraceContext::SetTraceId(traceId);
        UbseMemOperationResp resp{};
        UbseMemNumaBorrow(request->GetUbseMemNumaBorrowReq(), resp);
        TraceContext::Clear();
    });
    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemNumaBorrowMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_NUMA_BORROW);
}

uint16_t UbseMemNumaBorrowMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
}

UbseResult UbseMemAddrBorrowMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                   UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemAddrBorrowReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    resourceExecutor->Execute([request, traceId]() {
        TraceContext::SetTraceId(traceId);
        UbseMemOperationResp resp{};
        UbseMemAddrBorrow(request->GetUbseMemAddrBorrowReq(), resp);
        TraceContext::Clear();
    });
    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemAddrBorrowMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_ADDR_BORROW);
}

uint16_t UbseMemAddrBorrowMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
}

UbseResult UbseMemShareBorrowMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                    UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemShareBorrowReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    resourceExecutor->Execute([request, traceId]() {
        TraceContext::SetTraceId(traceId);
        UbseMemOperationResp resp{};
        UbseMemShareBorrow(request->GetUbseMemShareBorrowReq(), resp);
        TraceContext::Clear();
    });
    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemShareBorrowMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW);
}

uint16_t UbseMemShareBorrowMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
}

UbseResult UbseMemShareAttachMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                    UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemShareAttachReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }

    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    resourceExecutor->Execute([request, traceId]() {
        TraceContext::SetTraceId(traceId);
        UbseMemOperationResp resp{};
        UbseMemShareAttach(request->GetUbseMemShareAttachReq(), resp);
        TraceContext::Clear();
    });
    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemShareAttachMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_ATTACH);
}

uint16_t UbseMemShareAttachMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
}

UbseResult UbseMemShareDetachMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                    UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemShareDetachReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    resourceExecutor->Execute([request, traceId, ctx]() {
        TraceContext::SetTraceId(traceId);
        UbseMemOperationResp resp{};
        UbseMemShareDetach(request->GetUbseMemShareDetachReq(), resp, ctx->GetDstId());
        TraceContext::Clear();
    });
    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemShareDetachMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_DETACH);
}

uint16_t UbseMemShareDetachMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
}

UbseResult UbseMemFdReturnHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                          UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemReturnReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    resourceExecutor->Execute([request, traceId, ctx]() {
        TraceContext::SetTraceId(traceId);
        UbseMemOperationResp resp{};
        UbseMemFdReturn(request->GetUbseMemReturnReq(), resp, ctx->GetDstId());
        TraceContext::Clear();
    });

    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemFdReturnHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_RETURN);
}

uint16_t UbseMemFdReturnHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
}

UbseResult UbseMemNumaReturnHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                            UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemReturnReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    resourceExecutor->Execute([request, traceId, ctx]() {
        TraceContext::SetTraceId(traceId);
        UbseMemOperationResp resp{};
        UbseMemNumaReturn(request->GetUbseMemReturnReq(), resp, ctx->GetDstId());
        TraceContext::Clear();
    });

    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemNumaReturnHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_NUMA_RETURN);
}

uint16_t UbseMemNumaReturnHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
}

UbseResult UbseMemShareReturnHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                             UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemReturnReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    resourceExecutor->Execute([request, traceId, ctx]() {
        TraceContext::SetTraceId(traceId);
        UbseMemOperationResp resp{};
        UbseMemShareReturn(request->GetUbseMemReturnReq(), resp, ctx->GetDstId());
        TraceContext::Clear();
    });

    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemShareReturnHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_SHARE_RETURN);
}

uint16_t UbseMemShareReturnHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
}

UbseResult UbseMemAddrReturnHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                            UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemReturnReqSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    resourceExecutor->Execute([request, traceId]() {
        TraceContext::SetTraceId(traceId);
        UbseMemOperationResp resp{};
        UbseMemAddrReturn(request->GetUbseMemReturnReq(), resp);
        TraceContext::Clear();
    });

    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemAddrReturnHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_ADDR_RETURN);
}

uint16_t UbseMemAddrReturnHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
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
                                                                  const UbseBaseMessagePtr &rsp,
                                                                  UbseComBaseMessageHandlerCtxPtr ctx)
{
    UBSE_LOG_INFO << "receive fd export";
    auto request = UbseBaseMessage::DeConvert<UbseMemFdBorrowExportobjSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto exportObj = request->GetUbseMemFdBorrowExportObj();
    if (auto ret = MemoryBorrowRpcObjCheck(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fd borrow exportObj is invalid, please check the exportObj";
        return ret;
    }
    UBSE_LOG_INFO << "Received FdexportObj, name is " << exportObj.req.name << ", requestNodeId is "
                  << exportObj.req.requestNodeId;

    response->data = SYNC_SUCCESS;
    //   履行侧
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    if ((exportObj.status.state == UBSE_MEM_EXPORT_RUNNING || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING) &&
        exportObj.algoResult.exportNumaInfos[0].nodeId == GetCurNodeId()) {
        auto taskId = UbseMemAgentTaskManager::GenerateTaskId();
        UbseMemAgentTaskManager::AddTaskObj(taskId, exportObj);
        resourceExecutor->Execute([request, traceId, exportObj, taskId]() {
            TraceContext::SetTraceId(traceId);
            UbseMemFdBorrowExportObjCallback(exportObj);
            UbseMemAgentTaskManager::DeleteTaskObj(taskId);
            TraceContext::Clear();
        });
    } else {
        resourceExecutor->Execute([request, traceId, exportObj]() {
            TraceContext::SetTraceId(traceId);
            UbseMemFdBorrowExportObjCallback(exportObj);
            TraceContext::Clear();
        });
    }

    return UBSE_OK;
}

uint16_t UbseMemFdBorrowExportObjCallbackMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_FD_BORROW_EXPORT_OBJ_CALLBACK);
}

uint16_t UbseMemFdBorrowExportObjCallbackMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
}

UbseResult UbseMemFdBorrowImportObjCallbackMessageHandler::Handle(const UbseBaseMessagePtr &req,
                                                                  const UbseBaseMessagePtr &rsp,
                                                                  UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemFdBorrowImportobjSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto importObj = request->GetUbseMemFdBorrowImportObj();
    if (auto ret = MemoryBorrowRpcObjCheck(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fd borrow importObj is invalid, please check the importObj";
        return ret;
    }
    UBSE_LOG_INFO << "Received FdImportObj, name is " << importObj.req.name << ", requestNodeId is "
                  << importObj.req.requestNodeId << ", request_id=" << importObj.req.requestId;
    response->data = SYNC_SUCCESS;
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    if (importObj.req.importNodeId == GetCurNodeId() &&
        (importObj.status.state == UBSE_MEM_IMPORT_RUNNING || importObj.status.state == UBSE_MEM_IMPORT_DESTROYING)) {
        auto taskId = UbseMemAgentTaskManager::GenerateTaskId();
        UbseMemAgentTaskManager::AddTaskObj(taskId, importObj);
        resourceExecutor->Execute([request, traceId, importObj, taskId]() {
            TraceContext::SetTraceId(traceId);
            UbseMemFdBorrowImportObjCallback(importObj);
            UbseMemAgentTaskManager::DeleteTaskObj(taskId);
            TraceContext::Clear();
        });
    } else {
        resourceExecutor->Execute([importObj, traceId]() {
            TraceContext::SetTraceId(traceId);
            UbseMemFdBorrowImportObjCallback(importObj);
            TraceContext::Clear();
        });
    }
    return UBSE_OK;
}

uint16_t UbseMemFdBorrowImportObjCallbackMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_FD_BORROW_IMPORT_OBJ_CALLBACK);
}

uint16_t UbseMemFdBorrowImportObjCallbackMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
}

UbseResult UbseMemFdBorrowImportObjForPermissionCallbackMessageHandler::Handle(const UbseBaseMessagePtr &req,
                                                                               const UbseBaseMessagePtr &rsp,
                                                                               UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemFdBorrowImportobjSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemOperationRespSimpo>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto memFdBorrowImportObj = request->GetUbseMemFdBorrowImportObj();
    UBSE_LOG_INFO << "Received FdImportObjForPermission, name is " << memFdBorrowImportObj.req.name;
    UbseMemOperationResp resp{};
    resp.errorCode = UbseMemFdBorrowImportObjForPermissionCallback(memFdBorrowImportObj);
    resp.requestId = memFdBorrowImportObj.req.requestId;
    response->SetUbseMemOperationResp(resp);
    return UBSE_OK;
}

uint16_t UbseMemFdBorrowImportObjForPermissionCallbackMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_BORROW_IMPORT_OBJ_FOR_PERMISSION_CALLBACK);
}

uint16_t UbseMemFdBorrowImportObjForPermissionCallbackMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
}

UbseResult UbseMemNumaBorrowExportObjCallbackMessageHandler::Handle(const UbseBaseMessagePtr &req,
                                                                    const UbseBaseMessagePtr &rsp,
                                                                    UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemNumaBorrowExportobjSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto exportObj = request->GetUbseMemNumaBorrowExportObj();
    if (auto ret = MemoryBorrowRpcObjCheck(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Numa borrow exportObj is invalid, please check the exportObj";
        return ret;
    }

    response->data = SYNC_SUCCESS;
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    if ((exportObj.status.state == UBSE_MEM_EXPORT_RUNNING || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING) &&
        exportObj.algoResult.exportNumaInfos[0].nodeId == GetCurNodeId()) {
        auto taskId = UbseMemAgentTaskManager::GenerateTaskId();
        UbseMemAgentTaskManager::AddTaskObj(taskId, exportObj);
        resourceExecutor->Execute([request, traceId, exportObj, taskId]() {
            TraceContext::SetTraceId(traceId);
            UbseMemNumaBorrowExportObjCallback(exportObj);
            UbseMemAgentTaskManager::DeleteTaskObj(taskId);
            TraceContext::Clear();
        });
    } else {
        resourceExecutor->Execute([request, traceId, exportObj]() {
            TraceContext::SetTraceId(traceId);
            UbseMemNumaBorrowExportObjCallback(exportObj);
            TraceContext::Clear();
        });
    }
    return UBSE_OK;
}

uint16_t UbseMemNumaBorrowExportObjCallbackMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_NUMA_BORROW_EXPORT_OBJ_CALLBACK);
}

uint16_t UbseMemNumaBorrowExportObjCallbackMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
}

UbseResult UbseMemNumaBorrowImportObjCallbackMessageHandler::Handle(const UbseBaseMessagePtr &req,
                                                                    const UbseBaseMessagePtr &rsp,
                                                                    UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemNumaBorrowImportobjSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    response->data = SYNC_SUCCESS;
    auto importObj = request->GetUbseMemNumaBorrowImportObj();
    if (auto ret = MemoryBorrowRpcObjCheck(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Numa borrow importObj is invalid, please check the importObj";
        return ret;
    }
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    if ((importObj.status.state == UBSE_MEM_IMPORT_RUNNING || importObj.status.state == UBSE_MEM_IMPORT_DESTROYING) &&
        importObj.req.importNodeId == GetCurNodeId()) {
        auto taskId = UbseMemAgentTaskManager::GenerateTaskId();
        UbseMemAgentTaskManager::AddTaskObj(taskId, importObj);
        resourceExecutor->Execute([request, traceId, importObj, taskId]() {
            TraceContext::SetTraceId(traceId);
            UbseMemNumaBorrowImportObjCallback(importObj);
            UbseMemAgentTaskManager::DeleteTaskObj(taskId);
            TraceContext::Clear();
        });
    } else {
        resourceExecutor->Execute([importObj, traceId]() {
            TraceContext::SetTraceId(traceId);
            UbseMemNumaBorrowImportObjCallback(importObj);
            TraceContext::Clear();
        });
    }
    return UBSE_OK;
}

uint16_t UbseMemNumaBorrowImportObjCallbackMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_NUMA_BORROW_IMPORT_OBJ_CALLBACK);
}

uint16_t UbseMemNumaBorrowImportObjCallbackMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
}

UbseResult UbseMemShareBorrowExportObjCallbackMessageHandler::Handle(const UbseBaseMessagePtr &req,
                                                                     const UbseBaseMessagePtr &rsp,
                                                                     UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemShareBorrowExportobjSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    response->data = SYNC_SUCCESS;
    auto exportObj = request->GetUbseMemShareBorrowExportObj();
    if (auto ret = ShareBorrowRpcObjCheck(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Share borrow exportObj is invalid, please check the exportObj";
        return ret;
    }
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    if (exportObj.algoResult.exportNumaInfos[0].nodeId == GetCurNodeId() &&
        (exportObj.status.state == UBSE_MEM_EXPORT_RUNNING || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING)) {
        auto taskId = UbseMemAgentTaskManager::GenerateTaskId();
        UbseMemAgentTaskManager::AddTaskObj(taskId, exportObj);
        resourceExecutor->Execute([request, traceId, exportObj, taskId]() {
            TraceContext::SetTraceId(traceId);
            UbseMemShareBorrowExportObjCallback(exportObj);
            UbseMemAgentTaskManager::DeleteTaskObj(taskId);
            TraceContext::Clear();
        });
    } else {
        resourceExecutor->Execute([request, traceId, exportObj]() {
            TraceContext::SetTraceId(traceId);
            UbseMemShareBorrowExportObjCallback(exportObj);
            TraceContext::Clear();
        });
    }
    return UBSE_OK;
}

uint16_t UbseMemShareBorrowExportObjCallbackMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW_EXPORT_OBJ_CALLBACK);
}

uint16_t UbseMemShareBorrowExportObjCallbackMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
}

UbseResult UbseMemShareBorrowImportObjCallbackMessageHandler::Handle(const UbseBaseMessagePtr &req,
                                                                     const UbseBaseMessagePtr &rsp,
                                                                     UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemShareBorrowImportobjSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    response->data = SYNC_SUCCESS;
    auto importObj = request->GetUbseMemShareBorrowImportObj();
    if (auto ret = ShareBorrowRpcObjCheck(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Share borrow importObj is invalid, please check the importObj";
        return ret;
    }
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    if (importObj.importNodeId == GetCurNodeId() &&
        (importObj.status.state == UBSE_MEM_IMPORT_RUNNING || importObj.status.state == UBSE_MEM_IMPORT_DESTROYING)) {
        auto taskId = UbseMemAgentTaskManager::GenerateTaskId();
        UbseMemAgentTaskManager::AddTaskObj(taskId, importObj);
        resourceExecutor->Execute([request, traceId, importObj, taskId]() {
            TraceContext::SetTraceId(traceId);
            UbseMemShareBorrowImportObjCallback(importObj);
            UbseMemAgentTaskManager::DeleteTaskObj(taskId);
            TraceContext::Clear();
        });
    } else {
        resourceExecutor->Execute([importObj, traceId]() {
            TraceContext::SetTraceId(traceId);
            UbseMemShareBorrowImportObjCallback(importObj);
            TraceContext::Clear();
        });
    }
    return UBSE_OK;
}

uint16_t UbseMemShareBorrowImportObjCallbackMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_SHARE_BORROW_IMPORT_OBJ_CALLBACK);
}

uint16_t UbseMemShareBorrowImportObjCallbackMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
}

UbseResult UbseMemAddrBorrowExportObjCallbackMessageHandler::Handle(const UbseBaseMessagePtr &req,
                                                                    const UbseBaseMessagePtr &rsp,
                                                                    UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemAddrBorrowExportobjSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    response->data = SYNC_SUCCESS;
    auto exportObj = request->GetUbseMemAddrBorrowExportObj();
    if (auto ret = MemoryBorrowRpcObjCheck(exportObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Addr borrow exportObj is invalid, please check the exportObj";
        return ret;
    }
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Failed to get ubseMemController.";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    if ((exportObj.status.state == UBSE_MEM_EXPORT_RUNNING || exportObj.status.state == UBSE_MEM_EXPORT_DESTROYING) &&
        exportObj.algoResult.exportNumaInfos[0].nodeId == GetCurNodeId()) {
        auto taskId = UbseMemAgentTaskManager::GenerateTaskId();
        UbseMemAgentTaskManager::AddTaskObj(taskId, exportObj);
        resourceExecutor->Execute([request, traceId, exportObj, taskId]() {
            TraceContext::SetTraceId(traceId);
            UbseMemAddrBorrowExportObjCallback(exportObj);
            UbseMemAgentTaskManager::DeleteTaskObj(taskId);
            TraceContext::Clear();
        });
    } else {
        resourceExecutor->Execute([request, traceId, exportObj]() {
            TraceContext::SetTraceId(traceId);
            UbseMemAddrBorrowExportObjCallback(exportObj);
            TraceContext::Clear();
        });
    }
    return UBSE_OK;
}

uint16_t UbseMemAddrBorrowExportObjCallbackMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_ADDR_BORROW_EXPORT_OBJ_CALLBACK);
}

uint16_t UbseMemAddrBorrowExportObjCallbackMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
}

UbseResult UbseMemAddrBorrowImportObjCallbackMessageHandler::Handle(const UbseBaseMessagePtr &req,
                                                                    const UbseBaseMessagePtr &rsp,
                                                                    UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemAddrBorrowImportobjSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    response->data = SYNC_SUCCESS;
    auto importObj = request->GetUbseMemAddrBorrowImportobj();
    if (auto ret = MemoryBorrowRpcObjCheck(importObj); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Addr borrow importObj is invalid, please check the importObj";
        return ret;
    }
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Get ubseMemController fail";
        return UBSE_ERROR;
    }
    // 使用线程池异步执行
    std::string traceId = TraceContext::GetTraceId();
    if (importObj.req.importNodeId == GetCurNodeId() &&
        (importObj.status.state == UBSE_MEM_IMPORT_RUNNING || importObj.status.state == UBSE_MEM_IMPORT_DESTROYING)) {
        auto taskId = UbseMemAgentTaskManager::GenerateTaskId();
        UbseMemAgentTaskManager::AddTaskObj(taskId, importObj);
        resourceExecutor->Execute([request, traceId, importObj, taskId]() {
            TraceContext::SetTraceId(traceId);
            UbseMemAddrBorrowImportObjCallback(importObj);
            UbseMemAgentTaskManager::DeleteTaskObj(taskId);
            TraceContext::Clear();
        });
    } else {
        resourceExecutor->Execute([importObj, traceId]() {
            TraceContext::SetTraceId(traceId);
            UbseMemAddrBorrowImportObjCallback(importObj);
            TraceContext::Clear();
        });
    }
    return UBSE_OK;
}

uint16_t UbseMemAddrBorrowImportObjCallbackMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemBorrowCallbackOpCode::UBSE_MEM_ADDR_BORROW_IMPORT_OBJ_CALLBACK);
}

uint16_t UbseMemAddrBorrowImportObjCallbackMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW);
}

UbseResult MemScheduleHandler::RegHandler()
{
    auto comModule = UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        return UBSE_ERROR;
    }

    auto ret = RegisterFdBorrowHandlers(comModule);
    if (ret != UBSE_OK) {
        return UBSE_ERROR;
    }

    ret = RegisterNumaBorrowHandlers(comModule);
    if (ret != UBSE_OK) {
        return UBSE_ERROR;
    }

    ret = RegisterAddrBorrowHandlers(comModule);
    if (ret != UBSE_OK) {
        return UBSE_ERROR;
    }

    ret = RegisterShareBorrowHandlers(comModule);
    if (ret != UBSE_OK) {
        return UBSE_ERROR;
    }

    ret = RegisterReturnHandler(comModule);
    if (ret != UBSE_OK) {
        return UBSE_ERROR;
    }
    ret = RegisterShmCreateRespHandlers(comModule);
    if (ret != UBSE_OK) {
        return UBSE_ERROR;
    }
    ret = RegisterShmAttachRespHandlers(comModule);
    if (ret != UBSE_OK) {
        return UBSE_ERROR;
    }
    ret = RegisterShmDetachRespHandlers(comModule);
    if (ret != UBSE_OK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult MemScheduleHandler::RegisterFdBorrowHandlers(const std::shared_ptr<UbseComModule> &comModule)
{
    UbseComBaseMessageHandlerPtr hdl = new (std::nothrow) UbseMemFdBorrowMessageHandler();
    auto ret = comModule->RegRpcService<UbseMemFdBorrowReqSimpo, UbseMemCallbackMessage>(hdl);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemFdBorrowMessageHandler";
        return ret;
    }

    UbseComBaseMessageHandlerPtr exportHandler = new (std::nothrow) UbseMemFdBorrowExportObjCallbackMessageHandler();
    ret = comModule->RegRpcService<UbseMemFdBorrowExportobjSimpo, UbseMemCallbackMessage>(exportHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemFdBorrowExportObjCallbackMessageHandler";
        return ret;
    }

    UbseComBaseMessageHandlerPtr importHandler = new (std::nothrow) UbseMemFdBorrowImportObjCallbackMessageHandler();
    ret = comModule->RegRpcService<UbseMemFdBorrowImportobjSimpo, UbseMemCallbackMessage>(importHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemFdBorrowImportObjCallbackMessageHandler";
        return ret;
    }

    UbseComBaseMessageHandlerPtr importForPermissionHandler = new (std::nothrow)
        UbseMemFdBorrowImportObjForPermissionCallbackMessageHandler();
    ret =
        comModule->RegRpcService<UbseMemFdBorrowImportobjSimpo, UbseMemOperationRespSimpo>(importForPermissionHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemFdBorrowImportObjForPermissionCallbackMessageHandler";
        return ret;
    }

    UbseComBaseMessageHandlerPtr fdPermissionHandler = new (std::nothrow) UbseMemFdPermissionHandler();
    ret = comModule->RegRpcService<UbseMemFdPermissionReqMessage, UbseMemFdPermissionRespMessage>(fdPermissionHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemFdPermissionHandler";
        return ret;
    }

    UbseComBaseMessageHandlerPtr fdBorrowRespHandler = new (std::nothrow) UbseMemFdBorrowRespMessageHandler();
    ret = comModule->RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>(fdBorrowRespHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemFdBorrowRespMessageHandler";
        return ret;
    }
    UbseComBaseMessageHandlerPtr fdReturnRespHandler = new (std::nothrow) UbseMemFdReturnRespMessageHandler();
    ret = comModule->RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>(fdReturnRespHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemFdReturnRespMessageHandler";
        return ret;
    }
    return UBSE_OK;
}

UbseResult MemScheduleHandler::RegisterNumaBorrowHandlers(const std::shared_ptr<UbseComModule> &comModule)
{
    UbseComBaseMessageHandlerPtr handler = new (std::nothrow) UbseMemNumaBorrowMessageHandler();
    auto ret = comModule->RegRpcService<UbseMemNumaBorrowReqSimpo, UbseMemCallbackMessage>(handler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemNumaBorrowMessageHandler";
        return ret;
    }

    UbseComBaseMessageHandlerPtr exportHandler = new (std::nothrow) UbseMemNumaBorrowExportObjCallbackMessageHandler();
    ret = comModule->RegRpcService<UbseMemNumaBorrowExportobjSimpo, UbseMemCallbackMessage>(exportHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemNumaBorrowExportObjCallbackMessageHandler";
        return ret;
    }

    UbseComBaseMessageHandlerPtr importHandler = new (std::nothrow) UbseMemNumaBorrowImportObjCallbackMessageHandler();
    ret = comModule->RegRpcService<UbseMemNumaBorrowImportobjSimpo, UbseMemCallbackMessage>(importHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemNumaBorrowImportObjCallbackMessageHandler";
        return ret;
    }
    UbseComBaseMessageHandlerPtr numaBorrowRespHandler = new (std::nothrow) UbseMemNumaBorrowRespMessageHandler();
    ret = comModule->RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>(numaBorrowRespHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemFdBorrowRespMessageHandler";
        return ret;
    }
    UbseComBaseMessageHandlerPtr numaReturnRespHandler = new (std::nothrow) UbseMemNumaReturnRespMessageHandler();
    ret = comModule->RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>(numaReturnRespHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemFdReturnRespMessageHandler";
        return ret;
    }
    return UBSE_OK;
}

UbseResult MemScheduleHandler::RegisterAddrBorrowHandlers(const std::shared_ptr<UbseComModule> &comModule)
{
    UbseComBaseMessageHandlerPtr handler = new (std::nothrow) UbseMemAddrBorrowMessageHandler();
    auto ret = comModule->RegRpcService<UbseMemAddrBorrowReqSimpo, UbseMemCallbackMessage>(handler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemAddrBorrowMessageHandler";
        return ret;
    }

    UbseComBaseMessageHandlerPtr exportHandler = new (std::nothrow) UbseMemAddrBorrowExportObjCallbackMessageHandler();
    ret = comModule->RegRpcService<UbseMemAddrBorrowExportobjSimpo, UbseMemCallbackMessage>(exportHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemAddrBorrowExportObjCallbackMessageHandler";
        return ret;
    }

    UbseComBaseMessageHandlerPtr importHandler = new (std::nothrow) UbseMemAddrBorrowImportObjCallbackMessageHandler();
    ret = comModule->RegRpcService<UbseMemAddrBorrowImportobjSimpo, UbseMemCallbackMessage>(importHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemAddrBorrowImportObjCallbackMessageHandler";
        return ret;
    }

    return UBSE_OK;
}

UbseResult MemScheduleHandler::RegisterShareBorrowHandlers(const std::shared_ptr<UbseComModule> &comModule)
{
    UbseComBaseMessageHandlerPtr borrowHandler = new (std::nothrow) UbseMemShareBorrowMessageHandler();
    auto ret = comModule->RegRpcService<UbseMemShareBorrowReqSimpo, UbseMemCallbackMessage>(borrowHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemShareBorrowMessageHandler";
        return ret;
    }

    UbseComBaseMessageHandlerPtr attachHandler = new (std::nothrow) UbseMemShareAttachMessageHandler();
    ret = comModule->RegRpcService<UbseMemShareAttachReqSimpo, UbseMemCallbackMessage>(attachHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemShareAttachMessageHandler";
        return ret;
    }

    UbseComBaseMessageHandlerPtr detachHandler = new (std::nothrow) UbseMemShareDetachMessageHandler();
    ret = comModule->RegRpcService<UbseMemShareDetachReqSimpo, UbseMemCallbackMessage>(detachHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemShareDetachMessageHandler";
        return ret;
    }

    UbseComBaseMessageHandlerPtr exportHandler = new (std::nothrow) UbseMemShareBorrowExportObjCallbackMessageHandler();
    ret = comModule->RegRpcService<UbseMemShareBorrowExportobjSimpo, UbseMemCallbackMessage>(exportHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemShareBorrowExportObjCallbackMessageHandler";
        return ret;
    }

    UbseComBaseMessageHandlerPtr importHandler = new (std::nothrow) UbseMemShareBorrowImportObjCallbackMessageHandler();
    ret = comModule->RegRpcService<UbseMemShareBorrowImportobjSimpo, UbseMemCallbackMessage>(importHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemShareBorrowImportObjCallbackMessageHandler";
        return ret;
    }

    return UBSE_OK;
}

UbseResult MemScheduleHandler::RegisterReturnHandler(const std::shared_ptr<UbseComModule> &comModule)
{
    UbseComBaseMessageHandlerPtr fdHandler = new (std::nothrow) UbseMemFdReturnHandler();
    auto ret = comModule->RegRpcService<UbseMemReturnReqSimpo, UbseMemCallbackMessage>(fdHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemFdReturnHandler";
        return ret;
    }
    UbseComBaseMessageHandlerPtr numaHandler = new (std::nothrow) UbseMemNumaReturnHandler();
    ret = comModule->RegRpcService<UbseMemReturnReqSimpo, UbseMemCallbackMessage>(numaHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemNumaReturnHandler";
        return ret;
    }
    UbseComBaseMessageHandlerPtr shareHandler = new (std::nothrow) UbseMemShareReturnHandler();
    ret = comModule->RegRpcService<UbseMemReturnReqSimpo, UbseMemCallbackMessage>(shareHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemShareReturnHandler";
        return ret;
    }
    UbseComBaseMessageHandlerPtr addrHandler = new (std::nothrow) UbseMemAddrReturnHandler();
    ret = comModule->RegRpcService<UbseMemReturnReqSimpo, UbseMemCallbackMessage>(addrHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemAddrReturnHandler";
        return ret;
    }
    UbseComBaseMessageHandlerPtr respHandler = new (std::nothrow) UbseMemReturnRespHandler();
    ret = comModule->RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>(respHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemReturnRespHandler";
        return ret;
    }
    return UBSE_OK;
}

UbseResult MemScheduleHandler::RegisterShmCreateRespHandlers(const std::shared_ptr<UbseComModule> &comModule)
{
    UbseComBaseMessageHandlerPtr handler = new (std::nothrow) UbseMemShmCreateRespMessageHandler();
    auto ret = comModule->RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>(handler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemReturnMessageHandler";
        return ret;
    }
    return UBSE_OK;
}

UbseResult MemScheduleHandler::RegisterShmAttachRespHandlers(const std::shared_ptr<UbseComModule> &comModule)
{
    UbseComBaseMessageHandlerPtr handler = new (std::nothrow) UbseMemShmAttachRespMessageHandler();
    auto ret = comModule->RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>(handler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemReturnMessageHandler";
        return ret;
    }
    return UBSE_OK;
}

UbseResult MemScheduleHandler::RegisterShmDetachRespHandlers(const std::shared_ptr<UbseComModule> &comModule)
{
    UbseComBaseMessageHandlerPtr handler = new (std::nothrow) UbseMemShmDetachRespMessageHandler();
    auto ret = comModule->RegRpcService<UbseMemOperationRespSimpo, UbseMemCallbackMessage>(handler);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unable to register UbseMemReturnMessageHandler";
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseMemShmCreateRespMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                      UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemOperationRespSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = AsyncMemShmBorrowRespProcessor(request);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "do numa borrow resp async failed. ret:" << FormatRetCode(ret);
        response->data = SYNC_FAILED;
        return ret;
    }
    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemShmCreateRespMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_SHARE_BORROW_RESP);
}

uint16_t UbseMemShmCreateRespMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
}

UbseResult UbseMemShmAttachRespMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                      UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemOperationRespSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = AsyncMemShmAttachRespProcessor(request);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "do numa borrow resp async failed. ret:" << FormatRetCode(ret);
        response->data = SYNC_FAILED;
        return ret;
    }
    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemShmAttachRespMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_SHARE_ATTACH_RESP);
}

uint16_t UbseMemShmAttachRespMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
}

UbseResult UbseMemShmDetachRespMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                      UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemOperationRespSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = AsyncMemShmDetachRespProcessor(request);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "do numa borrow resp async failed. ret:" << FormatRetCode(ret);
        response->data = SYNC_FAILED;
        return ret;
    }
    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemShmDetachRespMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_SHARE_DETACH_RESP);
}

uint16_t UbseMemShmDetachRespMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
}

UbseResult UbseMemReturnRespHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                            UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemOperationRespSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = AsyncMemCommonReturnRespProcessor(request);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "do numa borrow resp async failed. ret:" << FormatRetCode(ret);
        response->data = SYNC_FAILED;
        return ret;
    }
    response->data = SYNC_SUCCESS;
    return UBSE_OK;
}

uint16_t UbseMemReturnRespHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_COMMON_RETURN_RESP);
}

uint16_t UbseMemReturnRespHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
}
UbseResult UbseMemFdPermissionHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                              UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemFdPermissionReqMessage>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemFdPermissionRespMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = UbseMemFdPermission(request->fdPermissionReq, ctx->GetDstId());
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "ubse mem fd permission set failed. ret:" << FormatRetCode(ret);
    }
    response->fdPermissionResp = {ret, request->fdPermissionReq.requestId};
    return UBSE_OK;
}

uint16_t UbseMemFdPermissionHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
    ;
}

uint16_t UbseMemFdPermissionHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_PERMISSION);
    ;
}

UbseMemFdPermissionReqMessage::UbseMemFdPermissionReqMessage(UbseMemFdPermissionReq fdPermissionReq)
    : fdPermissionReq(std::move(fdPermissionReq))
{
}

UbseResult UbseMemFdPermissionReqMessage::Serialize()
{
    UbseSerialization out;
    if (!serial::UbseMemFdPermissionReqSerialize(out, fdPermissionReq)) {
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemFdPermissionReqMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    if (!serial::UbseMemFdPermissionReqDeserialize(in, fdPermissionReq)) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseMemFdPermissionRespMessage::UbseMemFdPermissionRespMessage(UbseMemFdPermissionResp fdPermissionResp)
    : fdPermissionResp(fdPermissionResp)
{
}

UbseResult UbseMemFdPermissionRespMessage::Serialize()
{
    UbseSerialization out;
    out << fdPermissionResp.result << fdPermissionResp.requestId;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "Failed to serialize.";
        return UBSE_ERROR;
    }
    mOutputRawDataSize = out.GetLength();
    mOutputRawData = std::unique_ptr<uint8_t[]>(out.GetBuffer(true));
    return UBSE_OK;
}

UbseResult UbseMemFdPermissionRespMessage::Deserialize()
{
    if (mInputRawData == nullptr) {
        UBSE_LOG_ERROR << "InputRawData is null.";
        return UBSE_ERROR;
    }
    UbseDeSerialization in(mInputRawData.get(), mInputRawDataSize);
    in >> fdPermissionResp.result >> fdPermissionResp.requestId;
    if (!in.Check()) {
        UBSE_LOG_ERROR << "Failed to deserialize.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

def::UbseMemFdDesc ConvertOperationRespToFdDesc(const UbseMemOperationResp &resp)
{
    def::UbseMemFdDesc fdDesc{};
    fdDesc.name = resp.name;
    uint64_t size;
    auto ret = ConvertStrToUint64(resp.realSize, size);
    if (ret == UBSE_OK) {
        fdDesc.totalMemSize = size;
    }
    uint32_t importSlotId;
    ret = ConvertStrToUint32(resp.requestNodeId, importSlotId);
    if (ret == UBSE_OK) {
        fdDesc.importNode.slotId = importSlotId;
    }
    fdDesc.memIds = resp.memIdList;
    return fdDesc;
};

UbseResult UbseMemFdBorrowRespMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                     UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemOperationRespSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    response->data = SYNC_SUCCESS;
    auto operationResp = request->GetUbseMemOperationResp();
    UBSE_LOG_INFO << "Receive fd borrow resp. name is " << operationResp.name << "requestId is "
                  << operationResp.requestId << "errorCode=" << operationResp.errorCode;

    auto apiServer = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServer == nullptr) {
        UBSE_LOG_ERROR << "Failed to get api server";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage message{nullptr, 0};
    auto requestId = operationResp.requestId;
    uint32_t status = operationResp.errorCode;
    if (status != UBSE_OK) {
        return apiServer->SendResponse(status, requestId, message);
    }
    auto fdDesc = def::UbseMemFdDesc();
    auto ret = UbseMemFdGet(operationResp.name, fdDesc, nullptr);
    if (ret != UBSE_OK) {
        // 获取借用信息失败, 使用UbseMemOperationResp构建返回信息
        UBSE_LOG_ERROR << "Failed to get fd desc, " << FormatRetCode(ret);
        fdDesc = ConvertOperationRespToFdDesc(operationResp);
    }
    // 使用借用信息构建返回信息
    auto packRet = UbseMemFdDescPack(fdDesc, message);
    if (packRet != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to pack UbseMemFdDesc, " << FormatRetCode(ret);
        status = UBSE_ERROR_SERIALIZE_FAILED;
    }
    ret = apiServer->SendResponse(status, requestId, message);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send response, error code is " << operationResp.errorCode << ", requestId is "
                       << requestId;
    }
    delete[] message.buffer;
    return ret;
}

uint16_t UbseMemFdBorrowRespMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_BORROW_RESP);
}
uint16_t UbseMemFdBorrowRespMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
}

UbseResult UbseMemFdReturnRespMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                     UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemOperationRespSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    response->data = SYNC_SUCCESS;
    auto operationResp = request->GetUbseMemOperationResp();
    UBSE_LOG_INFO << "Receive fd return resp. name is " << operationResp.name << ", requestId is "
                  << operationResp.requestId << ", error code is " << operationResp.errorCode;
    auto apiServer = UbseContext::GetInstance().GetModule<api::server::UbseApiServerModule>();
    if (apiServer == nullptr) {
        UBSE_LOG_ERROR << "Failed to get api server";
        return UBSE_ERROR_NULLPTR;
    }
    ubse::ipc::UbseIpcMessage message{};
    auto requestId = operationResp.requestId;
    message.buffer = nullptr;
    message.length = 0;
    auto ret = apiServer->SendResponse(operationResp.errorCode, requestId, message);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to send response, error code is " << operationResp.errorCode << "requestId is "
                       << requestId;
    }
    return ret;
}

uint16_t UbseMemFdReturnRespMessageHandler::GetOpCode()
{
    return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_FD_RETURN_RESP);
}
uint16_t UbseMemFdReturnRespMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
}

UbseResult UbseMemNumaBorrowRespMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                       UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemOperationRespSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = DoNumaBorrowRespAsync(request->GetUbseMemOperationResp());
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
    return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_NUMA_BORROW_RESP);
}

uint16_t UbseMemNumaBorrowRespMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
}

UbseResult UbseMemNumaReturnRespMessageHandler::Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                                                       UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemOperationRespSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Failed to convert ptr";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = DoReturnRespAsync(request->GetUbseMemOperationResp());
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
    return static_cast<uint16_t>(UbseMemRespCtrlOpCode::UBSE_MEM_NUMA_RETURN_RESP);
}

uint16_t UbseMemNumaReturnRespMessageHandler::GetModuleCode()
{
    return static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_RESP);
}
} // namespace ubse::mem::controller