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

#include "ubse_mem_migrate_export_handler.h"
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
using namespace ubse::mem::util;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::com;
using namespace ubse::log;

UbseResult UbseMemMigrateExportHandler::RegUbseMemMigrateExportHandler(const std::shared_ptr<UbseComModule>& comModule)
{
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Migrate RPC message conversion or context missing";
        return UBSE_ERROR_NULLPTR;
    }
    UbseComBaseMessageHandlerPtr requestHandler = new (std::nothrow) UbseMemMigrateExportHandler();
    UbseComBaseMessageHandlerPtr resultHandler = new (std::nothrow) UbseMemMigrateNewExportResultHandler();
    if (requestHandler == nullptr || resultHandler == nullptr) {
        UBSE_LOG_ERROR << "Migrate RPC message conversion or context missing";
        return UBSE_ERROR_NULLPTR;
    }
    const auto ret = comModule->RegRpcService<UbseMemShareBorrowExportobjSimpo, UbseMemCallbackMessage>(requestHandler);
    if (ret != UBSE_OK) {
        return ret;
    }
    return comModule->RegRpcService<UbseMemShareBorrowExportobjSimpo, UbseMemCallbackMessage>(resultHandler);
}

UbseResult UbseMemMigrateExportHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                               UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemShareBorrowExportobjSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Migrate RPC message conversion or context missing";
        return UBSE_ERROR_NULLPTR;
    }
    const auto data = request->GetUbseMesgInfo();
    if (auto ret = ShareBorrowRpcObjCheck(data); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Invalid migrate export request, name=" << data.req.name << ", ret=" << FormatRetCode(ret);
        return ret;
    }
    const auto executor = GetExecutor("ubseMemController");
    if (executor == nullptr) {
        UBSE_LOG_ERROR << "Migrate RPC executor unavailable, name=" << data.req.name;
        return UBSE_ERROR_NULLPTR;
    }
    const auto traceId = TraceContext::GetTraceId();
    if (!executor->Execute([data, traceId]() {
            TraceContext::SetTraceId(traceId);
            UbseMemShmMigrateService::GetInstance().ProcessExportRequest(data);
            TraceContext::Clear();
        })) {
        UBSE_LOG_ERROR << "Queue migrate RPC work failed, name=" << data.req.name;
        return UBSE_ERROR;
    }
    response->data = "sync_success";
    return UBSE_OK;
}

UbseResult UbseMemMigrateNewExportResultHandler::Handle(const UbseBaseMessagePtr& req, const UbseBaseMessagePtr& rsp,
                                                        UbseComBaseMessageHandlerCtxPtr ctx)
{
    auto request = UbseBaseMessage::DeConvert<UbseMemShareBorrowExportobjSimpo>(req);
    auto response = UbseBaseMessage::DeConvert<UbseMemCallbackMessage>(rsp);
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Migrate RPC message conversion or context missing";
        return UBSE_ERROR_NULLPTR;
    }
    const auto data = request->GetUbseMesgInfo();
    if (auto ret = ShareBorrowRpcObjCheck(data); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Invalid migrate export result, name=" << data.req.name << ", ret=" << FormatRetCode(ret);
        return ret;
    }
    const auto executor = GetExecutor("ubseMemController");
    if (executor == nullptr) {
        UBSE_LOG_ERROR << "Migrate RPC executor unavailable, name=" << data.req.name;
        return UBSE_ERROR_NULLPTR;
    }
    const auto traceId = TraceContext::GetTraceId();
    if (!executor->Execute([data, traceId]() {
            TraceContext::SetTraceId(traceId);
            UbseMemShmMigrateService::GetInstance().NewExportResult(data);
            TraceContext::Clear();
        })) {
        UBSE_LOG_ERROR << "Queue migrate RPC work failed, name=" << data.req.name;
        return UBSE_ERROR;
    }
    response->data = "sync_success";
    return UBSE_OK;
}

} // namespace ubse::mem::controller::rpc
