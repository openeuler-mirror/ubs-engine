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

#include "test_ubse_cli_ssu_mock_invoke.h"

#include <cstdlib>
#include <cstring>
#include <utility>

#include "ubse_com_op_code.h"
#include "ubse_error.h"
#include "ubse_pack_util.h"

using namespace ubse::cli::reg;
using namespace ubse::plugin::service::ssu;
using ubse::com::UbseSsuOpCode;
using ubse::utils::UbsePackUtil;
using ubse::utils::UbseUnpackUtil;

uint16_t g_ssuMockLastModuleCode = 0;
uint16_t g_ssuMockLastOpCode = 0;
UbseCliSsuAllocDetailReq g_ssuMockLastDetailReq{};
UbseCliSsuAllocCreateReq g_ssuMockLastCreateReq{};
std::vector<uint8_t> g_ssuMockLastRequestPayload;
bool g_ssuMockLastRequestDeserialized = false;

void ResetSsuMockCapture()
{
    g_ssuMockLastModuleCode = 0;
    g_ssuMockLastOpCode = 0;
    g_ssuMockLastDetailReq = {};
    g_ssuMockLastCreateReq = {};
    g_ssuMockLastRequestPayload.clear();
    g_ssuMockLastRequestDeserialized = false;
}

namespace {
constexpr uint32_t MAX_STRING_LEN = 1024;

constexpr uint16_t SsuOpCode(UbseSsuOpCode opCode)
{
    return static_cast<uint16_t>(opCode);
}

uint32_t StringSize(const std::string &value)
{
    return static_cast<uint32_t>(sizeof(uint32_t) + value.size());
}

uint32_t NameSpaceSize(const UbseCliSsuNameSpaceInfo &value)
{
    uint32_t size = StringSize(value.tgtEid) + StringSize(value.tgtNqn) + StringSize(value.nsUuid) +
                    static_cast<uint32_t>(sizeof(uint32_t)) + StringSize(value.nsDevPath) +
                    static_cast<uint32_t>(sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t));
    for (const auto &hostNqn : value.allowHostNqnList) {
        size += StringSize(hostNqn);
    }
    return size;
}

uint32_t AllocationSize(const UbseCliSsuAllocResult &value)
{
    uint32_t size = StringSize(value.name) + static_cast<uint32_t>(sizeof(uint8_t) + sizeof(uint32_t));
    for (const auto &nameSpace : value.nameSpaceList) {
        size += NameSpaceSize(nameSpace);
    }
    return size;
}

bool PackString(UbsePackUtil &pack, const std::string &value)
{
    return pack.UbsePackString(value, MAX_STRING_LEN);
}

bool PackNameSpace(UbsePackUtil &pack, const UbseCliSsuNameSpaceInfo &value)
{
    if (!PackString(pack, value.tgtEid) || !PackString(pack, value.tgtNqn) || !PackString(pack, value.nsUuid) ||
        !pack.UbsePackUint32(value.namespaceId) || !PackString(pack, value.nsDevPath) ||
        !pack.UbsePackUint64(value.nsSize) || !pack.UbsePackUint32(static_cast<uint32_t>(value.lbaFormat)) ||
        !pack.UbsePackUint32(static_cast<uint32_t>(value.allowHostNqnList.size()))) {
        return false;
    }
    for (const auto &hostNqn : value.allowHostNqnList) {
        if (!PackString(pack, hostNqn)) {
            return false;
        }
    }
    return true;
}

bool PackAllocation(UbsePackUtil &pack, const UbseCliSsuAllocResult &value)
{
    if (!PackString(pack, value.name) || !pack.UbsePackUint8(static_cast<uint8_t>(value.strategy)) ||
        !pack.UbsePackUint32(static_cast<uint32_t>(value.nameSpaceList.size()))) {
        return false;
    }
    for (const auto &nameSpace : value.nameSpaceList) {
        if (!PackNameSpace(pack, nameSpace)) {
            return false;
        }
    }
    return true;
}

template <typename Pack>
uint32_t BuildResponse(uint32_t size, ubse_api_buffer_t *responseData, Pack packFields)
{
    if (responseData == nullptr) {
        return UBSE_ERROR;
    }
    responseData->buffer = static_cast<uint8_t *>(malloc(size));
    responseData->length = 0;
    if (responseData->buffer == nullptr) {
        return UBSE_ERROR;
    }
    UbsePackUtil pack(responseData->buffer, size);
    if (!packFields(pack)) {
        free(responseData->buffer);
        responseData->buffer = nullptr;
        return UBSE_ERROR;
    }
    responseData->length = size;
    return UBSE_OK;
}

uint32_t BuildAllocationResponse(const UbseCliSsuAllocResult &value, ubse_api_buffer_t *responseData)
{
    return BuildResponse(AllocationSize(value), responseData,
                         [&value](UbsePackUtil &pack) { return PackAllocation(pack, value); });
}

uint32_t BuildAllocationListResponse(const std::vector<UbseCliSsuAllocResult> &values, ubse_api_buffer_t *responseData)
{
    uint32_t size = sizeof(uint32_t);
    for (const auto &value : values) {
        size += AllocationSize(value);
    }
    return BuildResponse(size, responseData, [&values](UbsePackUtil &pack) {
        if (!pack.UbsePackUint32(static_cast<uint32_t>(values.size()))) {
            return false;
        }
        for (const auto &value : values) {
            if (!PackAllocation(pack, value)) {
                return false;
            }
        }
        return true;
    });
}

UbseCliSsuAllocResult BuildSsuAllocResult()
{
    UbseCliSsuAllocResult result;
    result.name = "alloc-space-1";
    result.strategy = UbseSsuAllocStrategy::LINEAR;
    result.nameSpaceList = {
        {"e2",
         "nqn.2024-01:target",
         "uuid-aa",
         1,
         "/dev/nvme0n1",
         5368709120ULL,
         UbseSsuLBAFormat::LBA_FORMAT_4K,
         {"nqn.2026-07:host-a", "nqn.2026-07:host-b"}},
        {"e2",
         "nqn.2024-01:target",
         "uuid-bb",
         2,
         "/dev/nvme0n2",
         5368709120ULL,
         UbseSsuLBAFormat::LBA_FORMAT_4K,
         {"nqn.2026-07:host-c"}},
    };
    return result;
}

bool UnpackCreate(UbseUnpackUtil &unpack, UbseCliSsuAllocCreateReq &request)
{
    uint32_t lbaFormat = 0;
    uint8_t strategy = 0;
    if (!unpack.UnpackString(request.name, 48) || !unpack.UnpackUint64(request.nsSize) ||
        !unpack.UnpackUint32(request.nsNum) || !unpack.UnpackUint32(lbaFormat) ||
        (lbaFormat != 512 && lbaFormat != 4096) || !unpack.UnpackUint8(strategy) || strategy > 1 ||
        !unpack.UnpackString(request.tenant, 17)) {
        return false;
    }
    request.lbaFormat = static_cast<UbseSsuLBAFormat>(lbaFormat);
    request.strategy = static_cast<UbseSsuAllocStrategy>(strategy);
    return true;
}

void CaptureSsuRequest(uint16_t moduleCode, uint16_t opCode, const ubse_api_buffer_t *requestData)
{
    g_ssuMockLastModuleCode = moduleCode;
    g_ssuMockLastOpCode = opCode;
    if (requestData == nullptr) {
        return;
    }
    if (requestData->buffer != nullptr && requestData->length != 0) {
        g_ssuMockLastRequestPayload.assign(requestData->buffer, requestData->buffer + requestData->length);
    }
    if (opCode == SsuOpCode(UbseSsuOpCode::UBSE_SSU_LIST_ALLOC_INFO_REQ)) {
        g_ssuMockLastRequestDeserialized = requestData->buffer == nullptr && requestData->length == 0;
        return;
    }
    if (requestData->buffer == nullptr || requestData->length == 0) {
        return;
    }

    UbseUnpackUtil unpack(requestData->buffer, requestData->length);
    if (opCode == SsuOpCode(UbseSsuOpCode::UBSE_SSU_GET_ALLOC_INFO_BY_NAME_REQ)) {
        g_ssuMockLastRequestDeserialized = unpack.UnpackString(g_ssuMockLastDetailReq.name, 48);
    } else if (opCode == SsuOpCode(UbseSsuOpCode::UBSE_SSU_ALLOC_REQ)) {
        g_ssuMockLastRequestDeserialized = UnpackCreate(unpack, g_ssuMockLastCreateReq);
    }
}
} // namespace

uint32_t mock_ssu_alloc_summary_invoke_call_empty(uint16_t moduleCode, uint16_t opCode,
                                                  const ubse_api_buffer_t *requestData, ubse_api_buffer_t *responseData)
{
    CaptureSsuRequest(moduleCode, opCode, requestData);
    return BuildAllocationListResponse({}, responseData);
}

uint32_t mock_ssu_alloc_summary_invoke_call_bad_response(uint16_t moduleCode, uint16_t opCode,
                                                         const ubse_api_buffer_t *requestData,
                                                         ubse_api_buffer_t *responseData)
{
    CaptureSsuRequest(moduleCode, opCode, requestData);
    return BuildResponse(sizeof(uint32_t), responseData,
                         [](UbsePackUtil &pack) { return pack.UbsePackUint32(UINT32_MAX); });
}

uint32_t mock_ssu_alloc_summary_invoke_call_empty_body(uint16_t moduleCode, uint16_t opCode,
                                                       const ubse_api_buffer_t *requestData,
                                                       ubse_api_buffer_t *responseData)
{
    CaptureSsuRequest(moduleCode, opCode, requestData);
    responseData->buffer = nullptr;
    responseData->length = 0;
    return UBSE_OK;
}

uint32_t mock_ssu_alloc_summary_invoke_call_normal(uint16_t moduleCode, uint16_t opCode,
                                                   const ubse_api_buffer_t *requestData,
                                                   ubse_api_buffer_t *responseData)
{
    CaptureSsuRequest(moduleCode, opCode, requestData);
    auto alloc1 = BuildSsuAllocResult();
    UbseCliSsuAllocResult alloc2;
    alloc2.name = "alloc-space-2";
    alloc2.strategy = UbseSsuAllocStrategy::STRIPED;
    alloc2.nameSpaceList = {{"e3",
                             "nqn.2024-01:target",
                             "uuid-cc",
                             3,
                             "/dev/nvme0n3",
                             21474836480ULL,
                             UbseSsuLBAFormat::LBA_FORMAT_4K,
                             {"nqn.2026-07:host-d"}}};
    return BuildAllocationListResponse({alloc1, alloc2}, responseData);
}

uint32_t mock_ssu_alloc_summary_invoke_call_subgib(uint16_t moduleCode, uint16_t opCode,
                                                   const ubse_api_buffer_t *requestData,
                                                   ubse_api_buffer_t *responseData)
{
    CaptureSsuRequest(moduleCode, opCode, requestData);
    UbseCliSsuAllocResult alloc;
    alloc.name = "alloc-space-sub";
    alloc.strategy = UbseSsuAllocStrategy::LINEAR;
    alloc.nameSpaceList = {{"e2",
                            "nqn.2024-01:target",
                            "uuid-sub",
                            1,
                            "/dev/nvme0n3",
                            512ULL * 1024ULL * 1024ULL,
                            UbseSsuLBAFormat::LBA_FORMAT_512,
                            {}}};
    return BuildAllocationListResponse({alloc}, responseData);
}

uint32_t mock_ssu_alloc_detail_invoke_call_empty(uint16_t moduleCode, uint16_t opCode,
                                                 const ubse_api_buffer_t *requestData, ubse_api_buffer_t *responseData)
{
    CaptureSsuRequest(moduleCode, opCode, requestData);
    UbseCliSsuAllocResult response;
    response.name = "alloc-space-1";
    response.strategy = UbseSsuAllocStrategy::LINEAR;
    return BuildAllocationResponse(response, responseData);
}

uint32_t mock_ssu_alloc_detail_invoke_call_bad_response(uint16_t moduleCode, uint16_t opCode,
                                                        const ubse_api_buffer_t *requestData,
                                                        ubse_api_buffer_t *responseData)
{
    CaptureSsuRequest(moduleCode, opCode, requestData);
    return BuildResponse(sizeof(uint32_t), responseData,
                         [](UbsePackUtil &pack) { return pack.UbsePackUint32(UINT32_MAX); });
}

uint32_t mock_ssu_alloc_detail_invoke_call_normal(uint16_t moduleCode, uint16_t opCode,
                                                  const ubse_api_buffer_t *requestData, ubse_api_buffer_t *responseData)
{
    CaptureSsuRequest(moduleCode, opCode, requestData);
    return BuildAllocationResponse(BuildSsuAllocResult(), responseData);
}

uint32_t mock_ssu_alloc_detail_invoke_call_subgib(uint16_t moduleCode, uint16_t opCode,
                                                  const ubse_api_buffer_t *requestData, ubse_api_buffer_t *responseData)
{
    CaptureSsuRequest(moduleCode, opCode, requestData);
    auto response = BuildSsuAllocResult();
    response.nameSpaceList.resize(1);
    response.nameSpaceList[0].nsSize = 512ULL * 1024ULL * 1024ULL;
    return BuildAllocationResponse(response, responseData);
}

uint32_t mock_ssu_alloc_create_invoke_call_normal(uint16_t moduleCode, uint16_t opCode,
                                                  const ubse_api_buffer_t *requestData, ubse_api_buffer_t *responseData)
{
    CaptureSsuRequest(moduleCode, opCode, requestData);
    return BuildAllocationResponse(BuildSsuAllocResult(), responseData);
}
