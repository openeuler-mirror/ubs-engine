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

#include "ubse_urma_controller_api.h"
#include <netinet/in.h>
#include <securec.h>
#include "ubs_engine_urma.h"
#include "ubse_api_server_module.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_logger_inner.h"
#include "ubse_serial_util.h"
#include "ubse_urma_controller.h"

namespace ubse::urmaController {
using namespace ubse::common::def;
using namespace ubse::ipc;
using namespace ubse::context;
using namespace ::api::server;
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::election;
using namespace ubse::com;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_URMA_CONTROLLER_MID)

UbseResult UbseUrmaControllerApi::Register()
{
    auto ubse_api_server_module = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (ubse_api_server_module == nullptr) {
        UBSE_LOG_ERROR << "Get api server module  failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = ubse_api_server_module->RegisterIpcHandler(UBSE_URMA, UBSE_URMA_QOS_SET, UbseUrmaBandWidthSet);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_URMA, UBSE_URMA_QOS_GET, UbseUrmaBandWidthQuery);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_URMA, UBSE_URMA_QOS_RESET, UbseUrmaBandWidthDisable);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of Urma Controller-API failed," << FormatRetCode(ret);
        return ret;
    }
    return UBSE_OK;
}

uint32_t UbseUrmaControllerApi::UbseUrmaBandWidthSet(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "UbseUrmaBandWidthSet request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    uint8_t *buffer = req.buffer;
    std::string name = std::string(reinterpret_cast<const char *>(buffer));
    buffer += name.length();
    uint32_t minBandWidth = ntohl(*(uint32_t *)buffer);
    buffer += sizeof(minBandWidth);
    uint32_t maxBandWidth = ntohl(*(uint32_t *)buffer);
    uint32_t ret = UrmaController::GetInstance().UbseUrmaBandWidthSet(name, minBandWidth, maxBandWidth);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthSet failed," << FormatRetCode(ret);
        return UBSE_ERROR_CONF_INVALID;
    }
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage response = {nullptr, 0};
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " UbseUrmaBandWidthSet response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseUrmaControllerApi::UbseUrmaBandWidthQuery(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    uint8_t *buffer = req.buffer;
    if (buffer == nullptr) {
        UBSE_LOG_ERROR << "UbseUrmaBandWidthQuery request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    uint32_t nodeId = ntohl(*(uint32_t *)buffer);
    buffer += sizeof(nodeId);
    std::string name = std::string(reinterpret_cast<const char *>(buffer));
    /* 如果不是本节点或者0xFFFFFFFF就是查询其他节点需要调用rpc查询 */
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    if (nodeId != UINT32_MAX && std::to_string(nodeId) != currentNodeInfo.nodeId) {
        return UbseUrmaBandWidthQueryNeighber(nodeId, name, context);
    }
    UrmaQosRpcRsp urmaQosRsp;
    uint32_t ret =
        UrmaController::GetInstance().UbseUrmaBandWidthQuery(name, urmaQosRsp.minBandWidth, urmaQosRsp.maxBandWidth);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthQuery failed," << FormatRetCode(ret);
        return UBSE_ERROR_SRCH;
    }
    return UbseUrmaSendQosRsp(context.requestId, urmaQosRsp);
}

uint32_t UbseUrmaControllerApi::UbseUrmaBandWidthDisable(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "UbseUrmaBandWidthDisable request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    std::string name = std::string(reinterpret_cast<const char *>(req.buffer));
    uint32_t ret = UrmaController::GetInstance().UbseUrmaBandWidthDisable(name);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthDisable failed," << FormatRetCode(ret);
        return UBSE_ERROR_SRCH;
    }

    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage response = {nullptr, 0};
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " UbseUrmaBandWidthDisable response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseUrmaControllerApi::UbseUrmaBandWidthQueryNeighber(const uint32_t nodeId, const std::string name,
                                                               const UbseRequestContext &context)
{
    UbseRoleInfo masterInfo{};
    auto res = UbseGetMasterInfo(masterInfo);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "Get master info failed, " << FormatRetCode(res);
        return res;
    }
    SendParam sendParam{masterInfo.nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_URMA),
                        static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_QOS_QUERY)};
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UbseUrmaQosReqPtr ubseRequestPtr = new (std::nothrow) UbseUrmaQosReqSimpo();
    if (ubseRequestPtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse request ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    UrmaQosRpcReq req = {nodeId, name};
    ubseRequestPtr->SetUbseUrmaQosReq(req);
    UbseUrmaQosRspPtr ubseResponsePtr = new (std::nothrow) UbseUrmaQosRspSimpo();
    if (ubseResponsePtr == nullptr) {
        UBSE_LOG_ERROR << "Getting ubse response ptr failed.";
        return UBSE_ERROR_NULLPTR;
    }
    res = comModule->RpcSend(sendParam, ubseRequestPtr, ubseResponsePtr);
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "comModule->RpcSend failed, " << FormatRetCode(res);
        return res;
    }
    UrmaQosRpcRsp rsp = ubseResponsePtr->GetUbseUrmaQosRsp();

    return UbseUrmaSendQosRsp(context.requestId, rsp);
}

uint32_t UbseUrmaControllerApi::UbseUrmaSendQosRsp(const uint32_t requestId, UrmaQosRpcRsp urmaQosRsp)
{
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage response = {nullptr, 0};
    response.length = sizeof(urmaQosRsp.minBandWidth) + sizeof(urmaQosRsp.maxBandWidth);
    uint8_t *buffer = (uint8_t *)malloc(response.length);
    response.buffer = buffer;
    *(uint32_t *)buffer = htonl(urmaQosRsp.minBandWidth);
    buffer += sizeof(urmaQosRsp.minBandWidth);
    *(uint32_t *)buffer = htonl(urmaQosRsp.maxBandWidth);
    uint32_t ret = apiServerModule->SendResponse(IPC_SUCCESS, requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " UbseUrmaSendQosRsp response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

} // namespace ubse::urmaController