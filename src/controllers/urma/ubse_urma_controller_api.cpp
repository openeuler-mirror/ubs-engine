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
#include "ubse_pack_util.h"
#include "ubse_serial_util.h"
#include "ubse_urma_controller.h"

namespace ubse::urmaController {
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ::api::server;
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::election;
using namespace ubse::com;
using namespace ubse::serial;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_URMA_CONTROLLER_MID)

size_t UbseStringCalcSize(const std::string &str, size_t maxLen)
{
    size_t len = 0;
    len += sizeof(uint32_t);
    len += std::min(maxLen, str.size());
    return len;
}

UbseResult LocalDevPack(std::vector<std::string> &nameInfos, std::vector<uint32_t> status, UbseIpcMessage &response)
{
    size_t infoSize = nameInfos.size();
    size_t rspSize = sizeof(uint32_t);
    for (auto &s : nameInfos) {
        rspSize += UbseStringCalcSize(s, UBS_URMA_NAME_MAX - 1) + sizeof(uint32_t);
        response.buffer = new (std::nothrow) uint8_t[rspSize];
        response.length = rspSize;
        if (response.buffer == nullptr) {
            return IPC_ERROR_SERIALIZATION_FAILED;
        }
        UbsePackUtil packUtil(response.buffer, response.length);
        if (!packUtil.UbsePackUint32(static_cast<uint32_t>(infoSize)))
            return IPC_ERROR_SERIALIZATION_FAILED;
        for (size_t i = 0; i < nameInfos.size(); i++) {
            if (!packUtil.UbsePackString(nameInfos[i], UBS_URMA_NAME_MAX - 1)) {
                return IPC_ERROR_SERIALIZATION_FAILED;
            }
            if (!packUtil.UbsePackUint32(status[i])) {
                return IPC_ERROR_SERIALIZATION_FAILED;
            }
        }
    }
    return UBSE_OK;
}

UbseResult UbseUrmaControllerApi::Register()
{
    auto ubse_api_server_module = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (ubse_api_server_module == nullptr) {
        UBSE_LOG_ERROR << "Get api server module  failed";
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = ubse_api_server_module->RegisterIpcHandler(UBSE_URMA, UBSE_URMA_QOS_SET, UbseUrmaBandWidthSet);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_URMA, UBSE_URMA_QOS_GET, UbseUrmaBandWidthGet);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_URMA, UBSE_URMA_QOS_RESET, UbseUrmaBandWidthReset);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_URMA, UBSE_URMA_CLI_QOS_GET, UbseUrmaBandWidthCliGet);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_URMA, UBSE_URMA_DEV_GET, UbseUrmaDevGet);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_URMA, UBSE_URMA_CLI_DEV_GET, UbseUrmaCliDevGet);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_URMA, UBSE_URMA_DEV_ALLOC, UbseUrmaDevAlloc);
    ret |= ubse_api_server_module->RegisterIpcHandler(UBSE_URMA, UBSE_URMA_DEV_FREE, UbseUrmaDevFree);
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
    std::string name(static_cast<const char*>(static_cast<const void*>(buffer)));
    buffer += name.length() + 1;
    uint32_t networkOrderValue = 0;
    errno_t memret = memcpy_s(&networkOrderValue, sizeof(networkOrderValue), buffer, sizeof(uint32_t));
    if (memret != 0) {
        UBSE_LOG_ERROR << "memcpy_s failed to read uint32_t from buffer.";
        return UBSE_ERROR;
    }
    uint32_t minBandWidth = ntohl(networkOrderValue);
    buffer += sizeof(minBandWidth);
    networkOrderValue = 0;
    memret = memcpy_s(&networkOrderValue, sizeof(networkOrderValue), buffer, sizeof(uint32_t));
    if (memret != 0) {
        UBSE_LOG_ERROR << "memcpy_s failed to read uint32_t from buffer.";
        return UBSE_ERROR;
    }
    uint32_t maxBandWidth = ntohl(networkOrderValue);
    uint32_t ret = UrmaController::GetInstance().UbseUrmaBandWidthSet(name, minBandWidth, maxBandWidth);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthSet failed," << FormatRetCode(ret);
        return ret;
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

uint32_t UbseUrmaControllerApi::UbseUrmaBandWidthGet(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    uint8_t *buffer = req.buffer;
    if (buffer == nullptr) {
        UBSE_LOG_ERROR << "UbseUrmaBandWidthGet request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    std::string name(static_cast<const char*>(static_cast<const void*>(buffer)));
    UrmaQosRpcRsp urmaQosRsp;
    uint32_t ret =
        UrmaController::GetInstance().UbseUrmaBandWidthGet(name, urmaQosRsp.minBandWidth, urmaQosRsp.maxBandWidth);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthGet failed," << FormatRetCode(ret);
        return ret;
    }
    return UbseUrmaSendQosRsp(context.requestId, urmaQosRsp);
}

uint32_t UbseUrmaControllerApi::UbseUrmaBandWidthCliGet(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    uint32_t nodeId;
    std::string name;
    UbseDeSerialization reqSerial(req.buffer, req.length);
    reqSerial >> nodeId >> name;
    if (!reqSerial.Check()) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    UbseRoleInfo currentNodeInfo{};
    UbseGetCurrentNodeInfo(currentNodeInfo);
    if (std::to_string(nodeId) != currentNodeInfo.nodeId) {
        return UbseUrmaBandWidthGetNeighber(nodeId, name, context);
    }
    UrmaQosRpcRsp urmaQosRsp;
    uint32_t ret =
        UrmaController::GetInstance().UbseUrmaBandWidthGet(name, urmaQosRsp.minBandWidth, urmaQosRsp.maxBandWidth);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthGet failed," << FormatRetCode(ret);
        return ret;
    }
    return UbseUrmaSendCliQosRsp(context.requestId, urmaQosRsp);
}

uint32_t UbseUrmaControllerApi::UbseUrmaBandWidthReset(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "UbseUrmaBandWidthReset request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    std::string name(static_cast<const char*>(static_cast<const void*>(req.buffer)));
    uint32_t ret = UrmaController::GetInstance().UbseUrmaBandWidthReset(name);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthReset failed," << FormatRetCode(ret);
        return ret;
    }

    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage response = {nullptr, 0};
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " UbseUrmaBandWidthReset response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseUrmaControllerApi::UbseUrmaBandWidthGetNeighber(const uint32_t nodeId, const std::string name,
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

    return UbseUrmaSendCliQosRsp(context.requestId, rsp);
}

uint32_t UbseUrmaControllerApi::UbseUrmaSendQosRsp(const uint64_t requestId, UrmaQosRpcRsp urmaQosRsp)
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
    // 安全序列化 uint32_t 值（网络字节序）
    uint32_t minVal = htonl(urmaQosRsp.minBandWidth);
    uint32_t maxVal = htonl(urmaQosRsp.maxBandWidth);
    errno_t ret1 = memcpy_s(buffer, response.length, &minVal, sizeof(minVal));
    errno_t ret2 = memcpy_s(buffer + sizeof(minVal), response.length - sizeof(minVal), &maxVal, sizeof(maxVal));
    if (ret1 != 0 || ret2 != 0) {
        std::free(buffer);
        UBSE_LOG_ERROR << "memcpy_s failed";
        return UBSE_ERROR;
    }
    uint32_t ret = apiServerModule->SendResponse(IPC_SUCCESS, requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " UbseUrmaSendQosRsp response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseUrmaControllerApi::UbseUrmaSendCliQosRsp(const uint64_t requestId, UrmaQosRpcRsp urmaQosRsp)
{
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseSerialization rspSerial;
    rspSerial << urmaQosRsp.minBandWidth << urmaQosRsp.maxBandWidth;
    if (!rspSerial.Check()) {
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    UbseIpcMessage response = {rspSerial.GetBuffer(), static_cast<uint32_t>(rspSerial.GetLength())};
    uint32_t ret = apiServerModule->SendResponse(IPC_SUCCESS, requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " UbseUrmaSendQosRsp response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseUrmaControllerApi::UbseUrmaCliDevGet(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "Ubse Urma Dev Get IPC request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    ubse::serial::UbseDeSerialization out{req.buffer, req.length};
    uint32_t nodeId;
    uint32_t type;

    out >> nodeId >> type;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "UbseUrmaApi::UbseUrmaDevGet deserialiazation fail";
        return UBSE_ERROR_SRCH;
    }
    std::vector<UbseUrmaInfoForQuery> urmaInfo;
    uint32_t ret =
        UrmaController::GetInstance().UbseGetUrmaDevInfoByNodeIdAndType(UrmaDevType::UNIQUE, nodeId, urmaInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthDisable failed," << FormatRetCode(ret);
        return UBSE_ERROR_SRCH;
    }
    ubse::serial::UbseSerialization ubse_req_serial(urmaInfo.size() * sizeof(UbseUrmaInfoForQuery) + sizeof(uint32_t));
    const uint32_t tmpSize = static_cast<uint32_t>(urmaInfo.size());
    ubse_req_serial << tmpSize;
    for (uint32_t i = 0; i < urmaInfo.size(); ++i) {
        const uint32_t tmpstate = static_cast<uint32_t>(urmaInfo[i].state);
        ubse_req_serial << urmaInfo[i].bondingName << urmaInfo[i].fe1Name << urmaInfo[i].fe2Name << tmpstate;
    }
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage response = {ubse_req_serial.GetBuffer(), static_cast<uint32_t>(ubse_req_serial.GetLength())};
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult AllocRspPack(UbseUrmaDevPath &pathInfos, UbseIpcMessage &response)
{
    auto boundingSize = UbseStringCalcSize(pathInfos.bondingPath, UBSE_MAX_URMA_PATH_LENGTH - 1);
    auto vfe0Size = UbseStringCalcSize(pathInfos.vfe0Path, UBSE_MAX_URMA_PATH_LENGTH - 1);
    auto vfe1Size = UbseStringCalcSize(pathInfos.vfe1Path, UBSE_MAX_URMA_PATH_LENGTH - 1);
    auto eidSize = UbseStringCalcSize(pathInfos.bondingEid, UBSE_MAX_URMA_PATH_LENGTH - 1);
    response.buffer = new (std::nothrow) uint8_t[boundingSize + vfe0Size + vfe1Size + eidSize];
    if (response.buffer == nullptr) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }
    response.length = boundingSize + vfe0Size + vfe1Size;
    UbsePackUtil packUtil(response.buffer, response.length);

    if (!packUtil.UbsePackString(pathInfos.bondingPath, UBSE_MAX_URMA_PATH_LENGTH - 1)) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }
    if (!packUtil.UbsePackString(pathInfos.vfe0Path, UBSE_MAX_URMA_PATH_LENGTH - 1)) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }
    if (!packUtil.UbsePackString(pathInfos.vfe1Path, UBSE_MAX_URMA_PATH_LENGTH - 1)) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }
    if (!packUtil.UbsePackString(pathInfos.bondingEid, UBSE_MAX_URMA_PATH_LENGTH - 1)) {
        return IPC_ERROR_SERIALIZATION_FAILED;
    }
    return UBSE_OK;
}

uint32_t UbseUrmaControllerApi::UbseUrmaDevAlloc(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "Ubse Urma Dev Alloc IPC request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    std::string name(static_cast<const char*>(static_cast<const void*>(req.buffer)));
    UbseUrmaDevPath devInfos;
    uint32_t ret = UrmaController::GetInstance().UbseAllocUrmaDev(name, devInfos);
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
    ret = AllocRspPack(devInfos, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Alloc RspPack failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseUrmaControllerApi::UbseUrmaDevFree(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "Ubse Urma Dev Free IPC request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    std::string name(static_cast<const char*>(static_cast<const void*>(req.buffer)));
    uint32_t ret = UrmaController::GetInstance().UbseFreeUrmaDev(name);
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
        UBSE_LOG_ERROR << " TopologyInfoQuery response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseUrmaControllerApi::UbseUrmaDevGet(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "Ubse Urma LocalDevGet IPC request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    uint32_t type;
    errno_t retCode = memcpy_s(req.buffer, sizeof(uint32_t), &type, sizeof(uint32_t));
    if (retCode != EOK) {
        UBSE_LOG_ERROR << "memcpy_s failed," << FormatRetCode(errno);
        return UBSE_ERROR_NOSPC;
    }
    std::vector<std::string> nameInfos;
    std::vector<uint32_t> status;
    uint32_t ret = UrmaController::GetInstance().UbseGetLocalUrmaDevInfoByType(static_cast<UrmaDevType>(type),
                                                                               nameInfos, status);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseGetLocalUrmaDevInfoByType failed," << FormatRetCode(ret);
        return UBSE_ERROR_SRCH;
    }

    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage response = {nullptr, 0};
    ret = LocalDevPack(nameInfos, status, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "LoaclDevRspPack failed," << FormatRetCode(ret);
        return UBSE_ERROR_SRCH;
    }
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " UrmaLoaclInfoQuery response send failed." << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::urmaController