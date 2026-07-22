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

#include "ubse_ssu_handler_message.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_ssu_obj_message.h"

namespace ubse::ssu::ipc::message {
UBSE_DEFINE_THIS_MODULE("ubse");

using namespace ubse::plugin::service::ssu;
using namespace common::def;
UbseResult SsuAllocResultListPack(const std::vector<UbseSsuAllocResult> &resultList,
                                  api::server::UbseIpcMessage &response)
{
    size_t requiredLength = sizeof(uint32_t);
    for (const auto &result : resultList) {
        requiredLength += AllocResultCalcSize(result);
    }
    if (requiredLength > UINT32_MAX) {
        UBSE_LOG_ERROR << "requiredLength overflow, requiredLength=" << requiredLength;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    response.length = static_cast<uint32_t>(requiredLength);
    response.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (response.buffer == nullptr) {
        UBSE_LOG_ERROR << "allocate response buffer failed, requiredLength=" << requiredLength;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    ubse::utils::UbsePackUtil packUtil(response.buffer, response.length);
    uint32_t listSize = static_cast<uint32_t>(resultList.size());
    if (!packUtil.UbsePackUint32(listSize)) {
        delete[] response.buffer;
        response.buffer = nullptr;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    for (const auto &result : resultList) {
        if (AllocResultPack(packUtil, result) != UBSE_OK) {
            delete[] response.buffer;
            response.buffer = nullptr;
            response.length = 0;
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    return UBSE_OK;
}

UbseResult SsuGetAllocInfoByNameUnpack(const api::server::UbseIpcMessage &buffer, std::string &name)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        UBSE_LOG_ERROR << "invalid buffer";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    utils::UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    return StringUnpack(unpackUtil, name, MAX_NAME_LEN);
}

UbseResult SsuGetAllocInfoByNamePack(const UbseSsuAllocResult &result, api::server::UbseIpcMessage &response)
{
    const size_t requiredLength = AllocResultCalcSize(result);
    if (requiredLength > UINT32_MAX) {
        UBSE_LOG_ERROR << "requiredLength overflow, requiredLength=" << requiredLength;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    response.length = static_cast<uint32_t>(requiredLength);
    response.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (response.buffer == nullptr) {
        UBSE_LOG_ERROR << "allocate response buffer failed, requiredLength=" << requiredLength;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    ubse::utils::UbsePackUtil packUtil(response.buffer, response.length);
    if (AllocResultPack(packUtil, result) != UBSE_OK) {
        delete[] response.buffer;
        response.buffer = nullptr;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}

UbseResult SsuGetNsStatsUnpack(const api::server::UbseIpcMessage &buffer, std::string &name)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        UBSE_LOG_ERROR << "invalid buffer";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    ubse::utils::UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    return StringUnpack(unpackUtil, name, MAX_NAME_LEN);
}

UbseResult SsuGetNsStatsPack(const std::vector<UbseSsuNsStats> &statsList, api::server::UbseIpcMessage &response)
{
    size_t requiredLength = sizeof(uint32_t);
    for (const auto &stats : statsList) {
        requiredLength += NsStatsCalcSize(stats);
    }
    if (requiredLength > UINT32_MAX) {
        UBSE_LOG_ERROR << "requiredLength overflow, requiredLength=" << requiredLength;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    response.length = static_cast<uint32_t>(requiredLength);
    response.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (response.buffer == nullptr) {
        UBSE_LOG_ERROR << "allocate response buffer failed, requiredLength=" << requiredLength;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    ubse::utils::UbsePackUtil packUtil(response.buffer, response.length);
    uint32_t listSize = static_cast<uint32_t>(statsList.size());
    if (!packUtil.UbsePackUint32(listSize)) {
        delete[] response.buffer;
        response.buffer = nullptr;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    for (const auto &stats : statsList) {
        if (NsStatsPack(packUtil, stats) != UBSE_OK) {
            delete[] response.buffer;
            response.buffer = nullptr;
            response.length = 0;
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    return UBSE_OK;
}

UbseResult SsuGetConnectInfoUnpack(const api::server::UbseIpcMessage &buffer, std::string &name,
                                   std::optional<UbseSsuVfe> &vfe)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        UBSE_LOG_ERROR << "invalid buffer";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    ubse::utils::UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    if (StringUnpack(unpackUtil, name, MAX_NAME_LEN) != UBSE_OK) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    uint8_t hasVfe = 0;
    if (!unpackUtil.UnpackUint8(hasVfe)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (hasVfe != 0) {
        UbseSsuVfe tempVfe;
        if (VfeUnpack(unpackUtil, tempVfe) != UBSE_OK) {
            return UBSE_ERROR_DESERIALIZE_FAILED;
        }
        vfe = std::move(tempVfe);
    } else {
        vfe.reset();
    }
    return UBSE_OK;
}

UbseResult SsuGetConnectInfoPack(const std::vector<UbseSsuConnectInfo> &connectInfoList,
                                 api::server::UbseIpcMessage &response)
{
    size_t requiredLength = sizeof(uint32_t);
    for (const auto &info : connectInfoList) {
        requiredLength += ConnectInfoCalcSize(info);
    }
    if (requiredLength > UINT32_MAX) {
        UBSE_LOG_ERROR << "requiredLength overflow, requiredLength=" << requiredLength;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    response.length = static_cast<uint32_t>(requiredLength);
    response.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (response.buffer == nullptr) {
        UBSE_LOG_ERROR << "allocate response buffer failed, requiredLength=" << requiredLength;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    ubse::utils::UbsePackUtil packUtil(response.buffer, response.length);
    uint32_t listSize = static_cast<uint32_t>(connectInfoList.size());
    if (!packUtil.UbsePackUint32(listSize)) {
        delete[] response.buffer;
        response.buffer = nullptr;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    for (const auto &info : connectInfoList) {
        if (ConnectInfoPack(packUtil, info) != UBSE_OK) {
            delete[] response.buffer;
            response.buffer = nullptr;
            response.length = 0;
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    return UBSE_OK;
}

UbseResult SsuAllocSpaceUnpack(const api::server::UbseIpcMessage &buffer, UbseSsuAllocSpaceReq &req)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        UBSE_LOG_ERROR << "invalid buffer";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    ubse::utils::UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    if (StringUnpack(unpackUtil, req.name, MAX_NAME_LEN) != UBSE_OK) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (!unpackUtil.UnpackUint64(req.nsSize)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (!unpackUtil.UnpackUint32(req.nsNum)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    uint32_t lbaFormat = 0;
    if (!unpackUtil.UnpackUint32(lbaFormat)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (!IsValidLBAFormat(lbaFormat)) {
        UBSE_LOG_ERROR << "invalid lbaFormat=" << lbaFormat;
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    req.lbaFormat = static_cast<UbseSsuLBAFormat>(lbaFormat);
    uint8_t strategy = 0;
    if (!unpackUtil.UnpackUint8(strategy)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (!IsValidAllocStrategy(strategy)) {
        UBSE_LOG_ERROR << "invalid strategy=" << static_cast<uint32_t>(strategy);
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    req.strategy = static_cast<UbseSsuAllocStrategy>(strategy);
    return StringUnpack(unpackUtil, req.tenant, MAX_TENANT_LEN);
}

UbseResult SsuAllocSpacePack(const UbseSsuAllocResult &result, api::server::UbseIpcMessage &response)
{
    const size_t requiredLength = AllocResultCalcSize(result);
    if (requiredLength > UINT32_MAX) {
        UBSE_LOG_ERROR << "requiredLength overflow, requiredLength=" << requiredLength;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    response.length = static_cast<uint32_t>(requiredLength);
    response.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (response.buffer == nullptr) {
        UBSE_LOG_ERROR << "allocate response buffer failed, requiredLength=" << requiredLength;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    ubse::utils::UbsePackUtil packUtil(response.buffer, response.length);
    if (AllocResultPack(packUtil, result) != UBSE_OK) {
        delete[] response.buffer;
        response.buffer = nullptr;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}

UbseResult SsuFreeSpaceUnpack(const api::server::UbseIpcMessage &buffer, std::string &name)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        UBSE_LOG_ERROR << "invalid buffer";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    ubse::utils::UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    return StringUnpack(unpackUtil, name, MAX_NAME_LEN);
}

UbseResult SsuAddAccessPermissionUnpack(const api::server::UbseIpcMessage &buffer, std::string &name, std::string &nqn)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        UBSE_LOG_ERROR << "invalid buffer";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    ubse::utils::UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    if (StringUnpack(unpackUtil, name, MAX_NAME_LEN) != UBSE_OK) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return StringUnpack(unpackUtil, nqn, MAX_NQN_LEN);
}

UbseResult SsuRemoveAccessPermissionUnpack(const api::server::UbseIpcMessage &buffer, std::string &name,
                                           std::string &nqn)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        UBSE_LOG_ERROR << "invalid buffer";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    ubse::utils::UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    if (StringUnpack(unpackUtil, name, MAX_NAME_LEN) != UBSE_OK) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return StringUnpack(unpackUtil, nqn, MAX_NQN_LEN);
}

UbseResult SsuAttachSpaceUnpack(const api::server::UbseIpcMessage &buffer, UbseSsuSpaceReq &req)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        UBSE_LOG_ERROR << "invalid buffer";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    ubse::utils::UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    return SpaceReqUnpack(unpackUtil, req);
}

UbseResult SsuAttachSpacePack(const std::vector<std::string> &nsDevPaths, api::server::UbseIpcMessage &response)
{
    size_t requiredLength = NsDevPathsCalcSize(nsDevPaths);
    if (requiredLength > UINT32_MAX) {
        UBSE_LOG_ERROR << "requiredLength overflow, requiredLength=" << requiredLength;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    response.length = static_cast<uint32_t>(requiredLength);
    response.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (response.buffer == nullptr) {
        UBSE_LOG_ERROR << "allocate response buffer failed, requiredLength=" << requiredLength;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    ubse::utils::UbsePackUtil packUtil(response.buffer, response.length);
    if (NsDevPathsPack(packUtil, nsDevPaths) != UBSE_OK) {
        delete[] response.buffer;
        response.buffer = nullptr;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}

UbseResult SsuDetachSpaceUnpack(const api::server::UbseIpcMessage &buffer, UbseSsuSpaceReq &req)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        UBSE_LOG_ERROR << "invalid buffer";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    ubse::utils::UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    return SpaceReqUnpack(unpackUtil, req);
}

UbseResult SsuAttachLinearSpaceUnpack(const api::server::UbseIpcMessage &buffer, UbseSsuLinearSpaceReq &req)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        UBSE_LOG_ERROR << "invalid buffer";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    ubse::utils::UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    return LinearSpaceReqUnpack(unpackUtil, req);
}

UbseResult SsuAttachLinearSpacePack(const std::vector<std::string> &nsDevPaths, const std::string &devPath,
                                    api::server::UbseIpcMessage &response)
{
    size_t requiredLength = NsDevPathsCalcSize(nsDevPaths) + StringCalcSize(devPath, MAX_DEV_PATH_LEN);
    if (requiredLength > UINT32_MAX) {
        UBSE_LOG_ERROR << "requiredLength overflow, requiredLength=" << requiredLength;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    response.length = static_cast<uint32_t>(requiredLength);
    response.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (response.buffer == nullptr) {
        UBSE_LOG_ERROR << "allocate response buffer failed, requiredLength=" << requiredLength;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    ubse::utils::UbsePackUtil packUtil(response.buffer, response.length);
    if (NsDevPathsPack(packUtil, nsDevPaths) != UBSE_OK) {
        delete[] response.buffer;
        response.buffer = nullptr;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    if (StringPack(packUtil, devPath, MAX_DEV_PATH_LEN) != UBSE_OK) {
        delete[] response.buffer;
        response.buffer = nullptr;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}

UbseResult SsuDetachLinearSpaceUnpack(const api::server::UbseIpcMessage &buffer, UbseSsuLinearSpaceReq &req)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        UBSE_LOG_ERROR << "invalid buffer";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    ubse::utils::UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    return LinearSpaceReqUnpack(unpackUtil, req);
}

UbseResult SsuAttachStripedSpaceUnpack(const api::server::UbseIpcMessage &buffer, UbseSsuStripedSpaceReq &req)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        UBSE_LOG_ERROR << "invalid buffer";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    ubse::utils::UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    return StripedSpaceReqUnpack(unpackUtil, req);
}

UbseResult SsuAttachStripedSpacePack(const std::vector<std::string> &nsDevPaths, const std::string &devPath,
                                     api::server::UbseIpcMessage &response)
{
    size_t requiredLength = NsDevPathsCalcSize(nsDevPaths) + StringCalcSize(devPath, MAX_DEV_PATH_LEN);
    if (requiredLength > UINT32_MAX) {
        UBSE_LOG_ERROR << "requiredLength overflow, requiredLength=" << requiredLength;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    response.length = static_cast<uint32_t>(requiredLength);
    response.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (response.buffer == nullptr) {
        UBSE_LOG_ERROR << "allocate response buffer failed, requiredLength=" << requiredLength;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    ubse::utils::UbsePackUtil packUtil(response.buffer, response.length);
    if (NsDevPathsPack(packUtil, nsDevPaths) != UBSE_OK) {
        delete[] response.buffer;
        response.buffer = nullptr;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    if (StringPack(packUtil, devPath, MAX_DEV_PATH_LEN) != UBSE_OK) {
        delete[] response.buffer;
        response.buffer = nullptr;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}

UbseResult SsuDetachStripedSpaceUnpack(const api::server::UbseIpcMessage &buffer, UbseSsuStripedSpaceReq &req)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        UBSE_LOG_ERROR << "invalid buffer";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    ubse::utils::UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    return StripedSpaceReqUnpack(unpackUtil, req);
}

UbseResult SsuGetFeDeviceListPack(const std::vector<UbseSsuFe> &feList, api::server::UbseIpcMessage &response)
{
    size_t requiredLength = sizeof(uint32_t);
    for (const auto &fe : feList) {
        requiredLength += FeCalcSize(fe);
    }
    if (requiredLength > UINT32_MAX) {
        UBSE_LOG_ERROR << "requiredLength overflow, requiredLength=" << requiredLength;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    response.length = static_cast<uint32_t>(requiredLength);
    response.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (response.buffer == nullptr) {
        UBSE_LOG_ERROR << "allocate response buffer failed, requiredLength=" << requiredLength;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    ubse::utils::UbsePackUtil packUtil(response.buffer, response.length);
    uint32_t listSize = static_cast<uint32_t>(feList.size());
    if (!packUtil.UbsePackUint32(listSize)) {
        delete[] response.buffer;
        response.buffer = nullptr;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    for (const auto &fe : feList) {
        if (FePack(packUtil, fe) != UBSE_OK) {
            delete[] response.buffer;
            response.buffer = nullptr;
            response.length = 0;
            return UBSE_ERROR_SERIALIZE_FAILED;
        }
    }
    return UBSE_OK;
}

UbseResult SsuFeDeviceAllocUnpack(const api::server::UbseIpcMessage &buffer, uint32_t &upi, UbseSsuVfe &vfe,
                                  std::string &busInstanceGuid)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        UBSE_LOG_ERROR << "invalid buffer";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    ubse::utils::UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    if (!unpackUtil.UnpackUint32(upi)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (VfeUnpack(unpackUtil, vfe) != UBSE_OK) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return StringUnpack(unpackUtil, busInstanceGuid, MAX_GUID_LEN);
}

UbseResult SsuFeDeviceAllocPack(const std::string &busInstanceGuid, api::server::UbseIpcMessage &response)
{
    const size_t requiredLength = StringCalcSize(busInstanceGuid, MAX_GUID_LEN);

    response.length = static_cast<uint32_t>(requiredLength);
    response.buffer = new (std::nothrow) uint8_t[requiredLength];
    if (response.buffer == nullptr) {
        UBSE_LOG_ERROR << "allocate response buffer failed, requiredLength=" << requiredLength;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }

    ubse::utils::UbsePackUtil packUtil(response.buffer, response.length);
    if (StringPack(packUtil, busInstanceGuid, MAX_GUID_LEN) != UBSE_OK) {
        delete[] response.buffer;
        response.buffer = nullptr;
        response.length = 0;
        return UBSE_ERROR_SERIALIZE_FAILED;
    }
    return UBSE_OK;
}

UbseResult SsuFeDeviceFreeUnpack(const api::server::UbseIpcMessage &buffer, uint32_t &upi, UbseSsuVfe &vfe)
{
    if (buffer.buffer == nullptr || buffer.length == 0) {
        UBSE_LOG_ERROR << "invalid buffer";
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    ubse::utils::UbseUnpackUtil unpackUtil(buffer.buffer, buffer.length);
    if (!unpackUtil.UnpackUint32(upi)) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    if (VfeUnpack(unpackUtil, vfe) != UBSE_OK) {
        return UBSE_ERROR_DESERIALIZE_FAILED;
    }
    return UBSE_OK;
}
} // namespace ubse::ssu::ipc::message
