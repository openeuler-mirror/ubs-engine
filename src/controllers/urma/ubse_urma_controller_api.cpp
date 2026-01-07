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

#include "ubse_urma_controller_api.h"
#include <netinet/in.h>
#include <securec.h>
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
using namespace api::server;
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::election;
using namespace ubse::com;
using namespace ubse::serial;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_URMA_CONTROLLER_MID)

const uint32_t UBSE_URMA_NAME_MAX = 32;        // 包含结束符长度
const uint32_t UBSE_MAX_URMA_PATH_LENGTH = 64; // 包含结束符长度
const size_t MAX_BUFFER_SIZE = 10 * 1024; // 10 KB

size_t UbseStringCalcSize(const std::string &str, size_t maxLen)
{
    size_t len = 0;
    len += sizeof(uint32_t); // 字符串长度指示
    len += std::min(maxLen, str.size());
    return len;
}

UbseResult LocalDevPack(std::vector<std::string> &nameInfos, std::vector<uint32_t> status, UbseIpcMessage &response)
{
    if (nameInfos.size() != status.size()) {
        UBSE_LOG_ERROR << "nameInfos and status size mismatch";
        return IPC_ERROR_INVALID_ARGUMENT;
    }
    size_t infoSize = nameInfos.size();
    size_t rspSize = sizeof(uint32_t);
    for (auto &s : nameInfos) {
        /* 每个名字后面需要增加一个status占用4个字节 */
        rspSize += UbseStringCalcSize(s, UBSE_URMA_NAME_MAX - 1) + sizeof(uint32_t);
    }
    response.buffer = new (std::nothrow) uint8_t[rspSize];
    response.length = rspSize;
    if (response.buffer == nullptr) {
        UBSE_LOG_ERROR << "Failed to allocate response buffer for size=" << rspSize;
        return IPC_ERROR_SERIALIZATION_FAILED;
    }
    UbsePackUtil packUtil(response.buffer, response.length);
    if (!packUtil.UbsePackUint32(static_cast<uint32_t>(infoSize))) {
        delete[] response.buffer;
        UBSE_LOG_ERROR << "Failed to pack infoSize=" << infoSize;
        return IPC_ERROR_SERIALIZATION_FAILED;
    }

    for (size_t i = 0; i < nameInfos.size(); i++) {
        if (!packUtil.UbsePackString(nameInfos[i], UBSE_URMA_NAME_MAX - 1)) {
            delete[] response.buffer;
            UBSE_LOG_ERROR << "Failed to pack nameInfo[" << i << "]=" << nameInfos[i];
            return IPC_ERROR_SERIALIZATION_FAILED;
        }
        if (!packUtil.UbsePackUint32(status[i])) {
            delete[] response.buffer;
            UBSE_LOG_ERROR << "Failed to pack status[" << i << "]=" << status[i];
            return IPC_ERROR_SERIALIZATION_FAILED;
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
    const char *str = reinterpret_cast<const char *>(buffer);
    uint32_t strlen = strnlen(str, UBSE_URMA_NAME_MAX);
    if (strlen >= UBSE_URMA_NAME_MAX) {
        return UBSE_ERROR;
    }
    std::string name(str, strlen);
    buffer += strlen + 1;

    uint32_t minBandWidth;
    uint32_t maxBandWidth;
    memcpy_s(&minBandWidth, sizeof(minBandWidth), buffer, sizeof(uint32_t));
    buffer += sizeof(minBandWidth);
    memcpy_s(&maxBandWidth, sizeof(maxBandWidth), buffer, sizeof(uint32_t));
    minBandWidth = ntohl(minBandWidth);
    maxBandWidth = ntohl(maxBandWidth);

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
    const char *str = reinterpret_cast<const char *>(buffer);
    uint32_t strlen = strnlen(str, UBSE_URMA_NAME_MAX);
    if (strlen >= UBSE_URMA_NAME_MAX) {
        return UBSE_ERROR;
    }
    std::string name(str, strlen);
    uint32_t minBandWidth;
    uint32_t maxBandWidth;
    uint32_t ret = UrmaController::GetInstance().UbseUrmaBandWidthGet(name, minBandWidth, maxBandWidth);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaBandWidthGet failed," << FormatRetCode(ret);
        return ret;
    }
    return UbseUrmaSendQosRsp(context.requestId, minBandWidth, maxBandWidth);
}

uint32_t UbseUrmaControllerApi::UbseUrmaBandWidthCliGet(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    uint32_t nodeId;
    UbseDeSerialization reqSerial(req.buffer, req.length);
    reqSerial >> nodeId;
    if (!reqSerial.Check()) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }

    std::vector<UbseUrmaInfoForQuery> urmaInfo;
    uint32_t ret =
        UrmaController::GetInstance().UbseGetUrmaDevInfoByNodeIdAndType(UrmaDevType::UNIQUE, nodeId, urmaInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UrmaController::UbseGetUrmaDevInfoByNodeIdAndType failed," << FormatRetCode(ret);
        return UBSE_ERROR_NOT_EXIST;
    }
    UbseSerialization qosSerial;
    uint32_t urmaSize = 0;
    for (uint32_t i = 0; i < urmaInfo.size(); ++i) {
        if (urmaInfo[i].qosProfile.profileName != "")
            qosSerial << urmaInfo[i].urmaName << urmaInfo[i].fe1Name << urmaInfo[i].fe2Name
                      << urmaInfo[i].qosProfile.minBandWidth << urmaInfo[i].qosProfile.maxBandWidth;
        urmaSize++;
    }

    UbseSerialization responseSerial;
    responseSerial << urmaSize;
    if (urmaSize != 0) {
        responseSerial << qosSerial;
    }
    if (!responseSerial.Check()) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    UbseIpcMessage response = {responseSerial.GetBuffer(), static_cast<uint32_t>(responseSerial.GetLength())};
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " UbseUrmaBandWidthCliGet response send failed," << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseUrmaControllerApi::UbseUrmaBandWidthReset(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "UbseUrmaBandWidthReset request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    const char *str = reinterpret_cast<const char *>(req.buffer);
    uint32_t strlen = strnlen(str, UBSE_URMA_NAME_MAX);
    if (strlen >= UBSE_URMA_NAME_MAX) {
        return UBSE_ERROR;
    }
    std::string name(str, strlen);
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

uint32_t UbseUrmaControllerApi::UbseUrmaSendQosRsp(const uint64_t requestId, uint32_t minBandWidth,
                                                   uint32_t maxBandWidth)
{
    auto apiServerModule = UbseContext::GetInstance().GetModule<UbseApiServerModule>();
    if (apiServerModule == nullptr) {
        UBSE_LOG_ERROR << "Get api server module failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseIpcMessage response = {nullptr, 0};
    uint32_t minBandWidthOut = htonl(minBandWidth);
    uint32_t maxBandWidthOut = htonl(maxBandWidth);
    response.length = sizeof(minBandWidth) + sizeof(maxBandWidth);
    uint8_t *buffer = (uint8_t *)malloc(response.length);
    if (buffer == nullptr) {
        return UBSE_ERROR;
    }
    response.buffer = buffer;
    memcpy_s(buffer, sizeof(minBandWidth), &minBandWidthOut, sizeof(minBandWidthOut));
    buffer += sizeof(minBandWidth);
    memcpy_s(buffer, sizeof(maxBandWidth), &maxBandWidthOut, sizeof(maxBandWidthOut));
    uint32_t ret = apiServerModule->SendResponse(IPC_SUCCESS, requestId, response);
    if (ret != UBSE_OK) {
        free(response.buffer);
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
    UbseDeSerialization out{req.buffer, req.length};
    uint32_t nodeId;
    uint32_t urmaType;

    out >> nodeId >> urmaType;
    if (!out.Check()) {
        UBSE_LOG_ERROR << "UbseUrmaControllerApi::UbseUrmaDevGet deserialiazation fail";
        return UBSE_ERROR_SRCH;
    }
    std::vector<UbseUrmaInfoForQuery> urmaInfo;
    uint32_t ret = UrmaController::GetInstance().UbseGetUrmaDevInfoByNodeIdAndType(static_cast<UrmaDevType>(urmaType),
                                                                                   nodeId, urmaInfo);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseUrmaControllerApi::UbseGetUrmaDevInfoByNodeIdAndType failed," << FormatRetCode(ret);
        return UBSE_ERROR_SRCH;
    }
    UbseSerialization ubse_req_serial;
    const uint32_t urmaSize = static_cast<uint32_t>(urmaInfo.size());
    ubse_req_serial << urmaSize;
    for (uint32_t i = 0; i < urmaInfo.size(); ++i) {
        const uint32_t urmaState = static_cast<uint32_t>(urmaInfo[i].state);
        ubse_req_serial << urmaInfo[i].urmaName << urmaInfo[i].fe1Name << urmaInfo[i].fe2Name << urmaState;
        UBSE_LOG_DEBUG << "Urma Info - urmaName: " << urmaInfo[i].urmaName << ", FE1 Name: " << urmaInfo[i].fe1Name
                       << ", FE2 Name: " << urmaInfo[i].fe2Name << ", State: " << urmaState;
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
    auto bufferSize = UbseStringCalcSize(pathInfos.bondingPath, UBSE_MAX_URMA_PATH_LENGTH - 1);
    for (const auto &path : pathInfos.vfePaths) {
        bufferSize += UbseStringCalcSize(path, UBSE_MAX_URMA_PATH_LENGTH - 1);
    }
    bufferSize += UbseStringCalcSize(pathInfos.bondingEid, UBSE_MAX_URMA_PATH_LENGTH - 1);
    if (bufferSize > MAX_BUFFER_SIZE) {
        UBSE_LOG_ERROR << "Requested buffer size " << bufferSize << " exceeds limit " << MAX_BUFFER_SIZE;
        return UBSE_ERROR_INVAL;
    }
    response.buffer = new (std::nothrow) uint8_t[bufferSize];
    if (response.buffer == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    response.length = bufferSize;
    UbsePackUtil packUtil(response.buffer, response.length);

    if (!packUtil.UbsePackString(pathInfos.bondingPath, UBSE_MAX_URMA_PATH_LENGTH - 1)) {
        delete[] response.buffer;
        return UBSE_ERROR;
    }
    if (!packUtil.UbsePackString(pathInfos.vfePaths[0], UBSE_MAX_URMA_PATH_LENGTH - 1)) {
        delete[] response.buffer;
        return UBSE_ERROR;
    }
    if (!packUtil.UbsePackString(pathInfos.vfePaths[1], UBSE_MAX_URMA_PATH_LENGTH - 1)) {
        delete[] response.buffer;
        return UBSE_ERROR;
    }
    if (!packUtil.UbsePackString(pathInfos.bondingEid, UBSE_MAX_URMA_PATH_LENGTH - 1)) {
        delete[] response.buffer;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t UbseUrmaControllerApi::UbseUrmaDevAlloc(const UbseIpcMessage &req, const UbseRequestContext &context)
{
    if (req.buffer == nullptr) {
        UBSE_LOG_ERROR << "Ubse Urma Dev Alloc IPC request info is null.";
        return UBSE_ERROR_NULLPTR;
    }
    const char *str = reinterpret_cast<const char *>(req.buffer);
    uint32_t strlen = strnlen(str, UBSE_URMA_NAME_MAX);
    if (strlen >= UBSE_URMA_NAME_MAX) {
        return UBSE_ERROR;
    }
    std::string name(str, strlen);
    UbseUrmaDevPath devInfos;
    uint32_t ret = UrmaController::GetInstance().UbseAllocUrmaDev(name, devInfos);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaDevAlloc failed," << FormatRetCode(ret);
        return UBSE_ERROR_NOT_EXIST;
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
    const char *str = reinterpret_cast<const char *>(req.buffer);
    uint32_t strlen = strnlen(str, UBSE_URMA_NAME_MAX);
    if (strlen >= UBSE_URMA_NAME_MAX) {
        return UBSE_ERROR;
    }
    std::string name(str, strlen);
    uint32_t ret = UrmaController::GetInstance().UbseFreeUrmaDev(name);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UrmaController::UbseUrmaDevFree failed," << FormatRetCode(ret);
        return UBSE_ERROR_NOT_EXIST;
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
        return UBSE_ERROR;
    }
    std::vector<std::string> nameInfos;
    std::vector<uint32_t> status;
    uint32_t ret =
        UrmaController::GetInstance().UbseGetLocalUrmaDevInfoByType(static_cast<UrmaDevType>(type), nameInfos, status);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "UbseGetLocalUrmaDevInfoByType failed," << FormatRetCode(ret);
        return UBSE_ERROR_NOT_EXIST;
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
        return UBSE_ERROR;
    }
    ret = apiServerModule->SendResponse(IPC_SUCCESS, context.requestId, response);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << " UrmaLoaclInfoQuery response send failed." << FormatRetCode(ret);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::urmaController