/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
* UbseEngine is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/

#include "ubse_npu_controller_dispatcher.h"
#include "ubse_api_server_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_logger.h"
#include "ubse_logger_inner.h"
#include "ubse_npu_msg_execute.h"
#include "ubse_npu_source_def.h"
namespace ubse::npu::controller {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID);
using namespace ubse::context;
using namespace ubse::log;
using namespace ::api::server;
using namespace ubse::ipc;
using namespace ubse::common::def;
/**
 * 执行函数
 * @param req 请求参数
 * @param context 上下文
 * @param func 执行函数，申请的内存，函数执行失败时，由函数内部自行释放
 * @return 执行结果
 */
static UbseResult ExecuteDispatcher(const UbseIpcMessage &req, const UbseRequestContext &context,
                                    const std::function<uint32_t(TransReqMsg, TransRespMsg &)> &func)
{
    if (!func) {
        UBSE_LOG_ERROR << "func is null";
        return UBSE_ERROR_NULLPTR;
    }
    TransRespMsg transRespMsg;
    auto ret = func({req.buffer, req.length}, transRespMsg);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Execute failed, " << FormatRetCode(ret);
        return ret;
    }
    UbseIpcMessage resp{transRespMsg.buffer, transRespMsg.length};
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        delete[] resp.buffer;
        return UBSE_ERROR_NULLPTR;
    }
    ret = apiServerModule->SendResponse(UBSE_OK, context.requestId, resp);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "SendResponse failed, " << FormatRetCode(ret);
    }
    delete[] resp.buffer;
    return ret;
}

UbseResult QueryLocalUbDevices(const UbseIpcMessage &req,
                                                            const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "Received QueryLocalUbDevices request";
    auto ret = ExecuteDispatcher(req, context, QueryDeviceExecute);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "QueryLocalUbDevices failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "QueryLocalUbDevices success";
    return UBSE_OK;
}

UbseResult AllocUbDevice(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "Received AllocUbDevice request";
    auto ret = ExecuteDispatcher(req, context, AllocDeviceExecute);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "AllocUbDevice failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "AllocUbDevice success";
    return UBSE_OK;
}

UbseResult FreeUbDevice(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "Received FreeUbDevice request";
    auto ret = ExecuteDispatcher(req, context, FreeDeviceExecute);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "FreeUbDevice failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "FreeUbDevice success";
    return UBSE_OK;
}

UbseResult QueryTidUbaSize(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    UBSE_LOG_INFO << "Received QueryTidUbaSize request";
    auto ret = ExecuteDispatcher(req, context, QueryTidUbaSizeExecute);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "QueryTidUbaSize failed, " << FormatRetCode(ret);
        return ret;
    }
    UBSE_LOG_INFO << "QueryTidUbaSize success";
    return UBSE_OK;
}

UbseResult RegisterSdkDispatcher()
{
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = apiServerModule->RegisterIpcHandler(UBSE_NPU, UBSE_NPU_GET_HOST_DEVICES, QueryLocalUbDevices);
    ret |= apiServerModule->RegisterIpcHandler(UBSE_NPU, UBSE_NPU_ALLOC_UB_DEVICES, AllocUbDevice);
    ret |= apiServerModule->RegisterIpcHandler(UBSE_NPU, UBSE_NPU_FREE_UB_DEVICES, FreeUbDevice);
    ret |= apiServerModule->RegisterIpcHandler(UBSE_NPU, UBSE_NPU_QUERY_UBA_TID_SIZE, QueryTidUbaSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of Npu SDK IPC-API failed.";
        return ret;
    }
    return UBSE_OK;
}

} // namespace ubse::npu::controller