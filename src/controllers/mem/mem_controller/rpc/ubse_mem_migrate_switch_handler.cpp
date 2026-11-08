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

#include "ubse_mem_migrate_switch_handler.h"
#include "ubse_logger.h"
#include "ubse_mem_util.h"
#include "message/ubse_mem_callback_message.h"
#include "message/ubse_mem_simpo_types.h"
#include "migrate/ubse_mem_controller_migrate_api.h"
#include "trace_context.h"

namespace ubse::mem::controller::rpc {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::mem::controller;
using namespace ubse::mem::controller::message;
using ubse::adapter_plugins::mmi::UbseMemMigrateBeginExportResp;
using ubse::adapter_plugins::mmi::UbseMemMigrateOpResp;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj;
using namespace ubse::com;
using namespace ubse::mem::util;
using namespace ubse::log;

namespace {
template <typename Message, typename Work>
UbseResult QueueMigrateRequest(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                               const UbseComBaseMessageHandlerCtxPtr& ctx, Work work)
{
    auto request = UbseBaseMessage::DeConvert<Message>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr || ctx == nullptr) {
        UBSE_LOG_ERROR << "Migrate RPC message conversion or context missing";
        return UBSE_ERROR_NULLPTR;
    }
    auto data = request->GetUbseMesgInfo();
    const auto peerNodeId = ctx->GetDstId();
    const auto executor = GetExecutor("ubseMemController");
    const auto traceId = TraceContext::GetTraceId();
    if (executor == nullptr || !executor->Execute([data, peerNodeId, traceId, work]() mutable {
            TraceContext::SetTraceId(traceId);
            work(data, peerNodeId);
            TraceContext::Clear();
        })) {
        UBSE_LOG_ERROR << "Queue migrate RPC failed, name=" << data.rpcInfo.name
                       << ", indexId=" << data.rpcInfo.blockIndex << ", peerNodeId=" << peerNodeId;
        return UBSE_ERROR;
    }
    response->data = "sync_success";
    return UBSE_OK;
}
} // namespace

UbseResult UbseMemMigrateSwitchBeginImportHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                                          UbseComBaseMessageHandlerCtxPtr ctx)
{
    return QueueMigrateRequest<UbseMemMigrateSwitchBeginImportReqSimpo>(
        req, rsp, ctx, [](auto& data, const auto& peerNodeId) {
            UbseMemShmMigrateService::GetInstance().ProcessSwitchBeginImportRequest(data, peerNodeId);
        });
}

UbseResult UbseMemMigrateBeginExportHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                                    UbseComBaseMessageHandlerCtxPtr ctx)
{
    return QueueMigrateRequest<UbseMemMigrateBeginExportReqSimpo>(
        req, rsp, ctx, [](auto& data, const auto& peerNodeId) {
            UbseMemShmMigrateService::GetInstance().ProcessSwitchBeginExportRequest(data, peerNodeId);
        });
}

UbseResult UbseMemMigrateEndExportHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                                  UbseComBaseMessageHandlerCtxPtr ctx)
{
    return QueueMigrateRequest<UbseMemMigrateEndExportReqSimpo>(req, rsp, ctx, [](auto& data, const auto& peerNodeId) {
        UbseMemShmMigrateService::GetInstance().ProcessSwitchEndExportRequest(data, peerNodeId);
    });
}

UbseResult UbseMemMigrateDataCopyHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                                 UbseComBaseMessageHandlerCtxPtr ctx)
{
    return QueueMigrateRequest<UbseMemMigrateDataCopyReqSimpo>(req, rsp, ctx, [](auto& data, const auto& peerNodeId) {
        UbseMemShmMigrateService::GetInstance().ProcessCopyRequest(data, peerNodeId);
    });
}

UbseResult UbseMemMigrateSwitchEndImportHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                                        UbseComBaseMessageHandlerCtxPtr ctx)
{
    return QueueMigrateRequest<UbseMemMigrateSwitchEndImportReqSimpo>(
        req, rsp, ctx, [](auto& data, const auto& peerNodeId) {
            UbseMemShmMigrateService::GetInstance().ProcessSwitchEndImportRequest(data, peerNodeId);
        });
}

UbseResult UbseMemMigrateBeginImportResultHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                                          UbseComBaseMessageHandlerCtxPtr ctx)
{
    return QueueMigrateRequest<UbseMemMigrateOpRespSimpo>(req, rsp, ctx, [](auto& data, const auto& peerNodeId) {
        data.rpcInfo.srcNodeId = peerNodeId; // 结果归属只信任 RPC 链路
        UbseMemShmMigrateService::GetInstance().BeginImportResult(data);
    });
}

UbseResult UbseMemMigrateBeginExportResultHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                                          UbseComBaseMessageHandlerCtxPtr ctx)
{
    return QueueMigrateRequest<UbseMemMigrateBeginExportRespSimpo>(
        req, rsp, ctx, [](auto& data, const auto& peerNodeId) {
            data.rpcInfo.srcNodeId = peerNodeId; // 结果归属只信任 RPC 链路
            UbseMemShmMigrateService::GetInstance().BeginExportResult(data);
        });
}

UbseResult UbseMemMigrateCopyResultHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                                   UbseComBaseMessageHandlerCtxPtr ctx)
{
    return QueueMigrateRequest<UbseMemMigrateOpRespSimpo>(req, rsp, ctx, [](auto& data, const auto& peerNodeId) {
        data.rpcInfo.srcNodeId = peerNodeId; // 结果归属只信任 RPC 链路
        UbseMemShmMigrateService::GetInstance().CopyResult(data);
    });
}

UbseResult UbseMemMigrateEndResultHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                                  UbseComBaseMessageHandlerCtxPtr ctx)
{
    return QueueMigrateRequest<UbseMemMigrateOpRespSimpo>(req, rsp, ctx, [](auto& data, const auto& peerNodeId) {
        data.rpcInfo.srcNodeId = peerNodeId; // 结果归属只信任 RPC 链路
        UbseMemShmMigrateService::GetInstance().EndResult(data);
    });
}

UbseResult RegisterMigrateSwitchHandlers(const std::shared_ptr<UbseComModule>& comModule)
{
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "get UbseComModule failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }
    UbseComBaseMessageHandlerPtr beginHandler = new (std::nothrow) UbseMemMigrateSwitchBeginImportHandler();
    auto retCode =
        comModule->RegRpcService<UbseMemMigrateSwitchBeginImportReqSimpo, UbseMemCallbackMessage>(beginHandler);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemMigrateSwitchBeginImportHandler register fail, " << FormatRetCode(retCode);
        return retCode;
    }
    UbseComBaseMessageHandlerPtr exportBeginHandler = new (std::nothrow) UbseMemMigrateBeginExportHandler();
    retCode = comModule->RegRpcService<UbseMemMigrateBeginExportReqSimpo, UbseMemCallbackMessage>(exportBeginHandler);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemMigrateBeginExportHandler register fail, " << FormatRetCode(retCode);
        return retCode;
    }
    UbseComBaseMessageHandlerPtr exportEndHandler = new (std::nothrow) UbseMemMigrateEndExportHandler();
    retCode = comModule->RegRpcService<UbseMemMigrateEndExportReqSimpo, UbseMemCallbackMessage>(exportEndHandler);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemMigrateEndExportHandler register fail, " << FormatRetCode(retCode);
        return retCode;
    }
    UbseComBaseMessageHandlerPtr copyHandler = new (std::nothrow) UbseMemMigrateDataCopyHandler();
    retCode = comModule->RegRpcService<UbseMemMigrateDataCopyReqSimpo, UbseMemCallbackMessage>(copyHandler);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemMigrateDataCopyHandler register fail, " << FormatRetCode(retCode);
        return retCode;
    }
    UbseComBaseMessageHandlerPtr endHandler = new (std::nothrow) UbseMemMigrateSwitchEndImportHandler();
    retCode = comModule->RegRpcService<UbseMemMigrateSwitchEndImportReqSimpo, UbseMemCallbackMessage>(endHandler);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemMigrateSwitchEndImportHandler register fail, " << FormatRetCode(retCode);
        return retCode;
    }
    UbseComBaseMessageHandlerPtr beginResultHandler = new (std::nothrow) UbseMemMigrateBeginImportResultHandler();
    retCode = comModule->RegRpcService<UbseMemMigrateOpRespSimpo, UbseMemCallbackMessage>(beginResultHandler);
    if (retCode != UBSE_OK) {
        return retCode;
    }
    UbseComBaseMessageHandlerPtr beginExportResultHandler = new (std::nothrow) UbseMemMigrateBeginExportResultHandler();
    retCode =
        comModule->RegRpcService<UbseMemMigrateBeginExportRespSimpo, UbseMemCallbackMessage>(beginExportResultHandler);
    if (retCode != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseMemMigrateBeginExportResultHandler register fail, " << FormatRetCode(retCode);
        return retCode;
    }
    UbseComBaseMessageHandlerPtr copyResultHandler = new (std::nothrow) UbseMemMigrateCopyResultHandler();
    retCode = comModule->RegRpcService<UbseMemMigrateOpRespSimpo, UbseMemCallbackMessage>(copyResultHandler);
    if (retCode != UBSE_OK) {
        return retCode;
    }
    UbseComBaseMessageHandlerPtr endResultHandler = new (std::nothrow) UbseMemMigrateEndResultHandler();
    retCode = comModule->RegRpcService<UbseMemMigrateOpRespSimpo, UbseMemCallbackMessage>(endResultHandler);
    if (retCode != UBSE_OK) {
        return retCode;
    }
    return UBSE_OK;
}
} // namespace ubse::mem::controller::rpc
