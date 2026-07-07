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

#include <securec.h>

#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_serial_util.h"

using namespace ubse::serial;
using namespace ubse::cli::reg;
using namespace ubse::plugin::service::ssu;

// 全局捕获区：记录最近一次 ubse_invoke_call 的入参及反序列化结果，供用例断言 CLI 实际发出的请求。
// 测试为单线程串行执行，用例起始由 ResetSsuMockCapture 清空，故全局可变状态在此上下文安全。
uint16_t g_ssuMockLastModuleCode = 0;
uint16_t g_ssuMockLastOpCode = 0;
UbseCliSsuAllocSummaryReq g_ssuMockLastSummaryReq{};
UbseCliSsuAllocDetailReq g_ssuMockLastDetailReq{};
UbseCliSsuAllocCreateReq g_ssuMockLastCreateReq{};
bool g_ssuMockLastRequestDeserialized = false;

void ResetSsuMockCapture()
{
    g_ssuMockLastModuleCode = 0;
    g_ssuMockLastOpCode = 0;
    g_ssuMockLastSummaryReq = {};
    g_ssuMockLastDetailReq = {};
    g_ssuMockLastCreateReq = {};
    g_ssuMockLastRequestDeserialized = false;
}

namespace {
// 将序列化结果拷贝到 IPC 响应缓冲：模拟服务端 malloc 一段缓冲并填充报文的真实行为，
// 调用方（CLI 侧 UbseCliBufferGuard）负责释放。失败路径需回滚已分配缓冲避免泄漏。
uint32_t CopySerializedResponse(UbseSerialization &ser, ubse_api_buffer_t *response_data)
{
    if (response_data == nullptr || !ser.Check()) {
        return UBSE_ERROR;
    }
    response_data->buffer = static_cast<uint8_t *>(malloc(ser.GetLength()));
    response_data->length = 0;
    if (!response_data->buffer) {
        return UBSE_ERROR;
    }
    response_data->length = static_cast<uint32_t>(ser.GetLength());
    if (memcpy_s(response_data->buffer, ser.GetLength(), ser.GetBuffer(), ser.GetLength()) != EOK) {
        free(response_data->buffer);
        response_data->buffer = nullptr;
        response_data->length = 0;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

// 构造一份典型的分配结果响应，供 detail/create 的"正常路径"用例复用。
// 字段对齐服务层 UbseSsuNameSpaceInfo：tgtEid/tgtNqn/nsUuid/namespaceId/nsDevPath/nsSize/lbaFormat。
UbseCliSsuAllocResult BuildSsuAllocResult()
{
    UbseCliSsuAllocResult result;
    result.name = "alloc-space-1";
    result.strategy = UbseSsuAllocStrategy::LINEAR;
    result.nameSpaceList = {
        {"e2", "nqn.2024-01:target", "uuid-aa", 1, "/dev/nvme0n1", 10737418240ULL, UbseSsuLBAFormat::LBA_FORMAT_4K},
        {"e2", "nqn.2024-01:target", "uuid-bb", 2, "/dev/nvme0n2", 10737418240ULL, UbseSsuLBAFormat::LBA_FORMAT_4K},
    };
    return result;
}

// 捕获 CLI 发出的请求：记录 module/op code，并按 op code 反序列化对应请求结构体，
// 同时记录反序列化是否成功，供用例验证 CLI 编码正确性。
void CaptureSsuRequest(uint16_t module_code, uint16_t op_code, const ubse_api_buffer_t *request_data)
{
    g_ssuMockLastModuleCode = module_code;
    g_ssuMockLastOpCode = op_code;
    if (request_data == nullptr || request_data->buffer == nullptr || request_data->length == 0) {
        return;
    }
    UbseDeSerialization deser(request_data->buffer, request_data->length);
    if (op_code == UBSE_SSU_CLI_ALLOC_SUMMARY) {
        g_ssuMockLastSummaryReq.Deserialize(deser);
    } else if (op_code == UBSE_SSU_CLI_ALLOC_DETAIL) {
        g_ssuMockLastDetailReq.Deserialize(deser);
    } else if (op_code == UBSE_SSU_CLI_ALLOC_CREATE) {
        g_ssuMockLastCreateReq.Deserialize(deser);
    }
    g_ssuMockLastRequestDeserialized = deser.Check();
}
} // namespace

// 各 mock_ssu_*_invoke_call_* 桩函数签名与真实 ubse_invoke_call 一致，由 mockcpp 注入。
// 命名约定：<子命令>_<场景>：empty=空结果、bad_response=损坏报文、normal=正常、subgib=子 GiB 容量。

// summary 空响应：allocations 为空，触发 CLI 的 INFO_EMPTY 分支。
uint32_t mock_ssu_alloc_summary_invoke_call_empty(uint16_t module_code, uint16_t op_code,
                                                  const ubse_api_buffer_t *request_data,
                                                  ubse_api_buffer_t *response_data)
{
    CaptureSsuRequest(module_code, op_code, request_data);
    UbseCliSsuAllocListRsp response;
    UbseSerialization ser;
    response.Serialize(ser);
    return CopySerializedResponse(ser, response_data);
}

// summary 损坏响应：写入非预期布局报文，触发 CLI 的反序列化失败分支。
uint32_t mock_ssu_alloc_summary_invoke_call_bad_response(uint16_t module_code, uint16_t op_code,
                                                         const ubse_api_buffer_t *request_data,
                                                         ubse_api_buffer_t *response_data)
{
    CaptureSsuRequest(module_code, op_code, request_data);
    UbseSerialization ser;
    ser << std::string("bad-response");
    return CopySerializedResponse(ser, response_data);
}

// summary 正常响应：两条分配（10G/Linear、20G/Striped），用于摘要表内容断言。
// size 由各分配下 nameSpaceList[*].nsSize 求和得出，与服务层结果模型一致。
uint32_t mock_ssu_alloc_summary_invoke_call_normal(uint16_t module_code, uint16_t op_code,
                                                   const ubse_api_buffer_t *request_data,
                                                   ubse_api_buffer_t *response_data)
{
    CaptureSsuRequest(module_code, op_code, request_data);
    UbseCliSsuAllocListRsp response;
    UbseCliSsuAllocResult alloc1;
    alloc1.name = "alloc-space-1";
    alloc1.strategy = UbseSsuAllocStrategy::LINEAR;
    alloc1.nameSpaceList = {
        {"e2", "nqn.2024-01:target", "uuid-aa", 1, "/dev/nvme0n1", 10737418240ULL, UbseSsuLBAFormat::LBA_FORMAT_4K}};
    UbseCliSsuAllocResult alloc2;
    alloc2.name = "alloc-space-2";
    alloc2.strategy = UbseSsuAllocStrategy::STRIPED;
    alloc2.nameSpaceList = {
        {"e3", "nqn.2024-01:target", "uuid-bb", 1, "/dev/nvme0n2", 21474836480ULL, UbseSsuLBAFormat::LBA_FORMAT_4K}};
    response.allocations = {alloc1, alloc2};
    UbseSerialization ser;
    response.Serialize(ser);
    return CopySerializedResponse(ser, response_data);
}

// 返回 512MiB（非 GiB 整数倍）容量的分配，用于验证输出统一为 G
uint32_t mock_ssu_alloc_summary_invoke_call_subgib(uint16_t module_code, uint16_t op_code,
                                                   const ubse_api_buffer_t *request_data,
                                                   ubse_api_buffer_t *response_data)
{
    CaptureSsuRequest(module_code, op_code, request_data);
    UbseCliSsuAllocListRsp response;
    constexpr uint64_t halfGib = 512ULL * 1024ULL * 1024ULL;
    UbseCliSsuAllocResult alloc;
    alloc.name = "alloc-space-sub";
    alloc.strategy = UbseSsuAllocStrategy::LINEAR;
    alloc.nameSpaceList = {
        {"e2", "nqn.2024-01:target", "uuid-sub", 1, "/dev/nvme0n3", halfGib, UbseSsuLBAFormat::LBA_FORMAT_512}};
    response.allocations = {alloc};
    UbseSerialization ser;
    response.Serialize(ser);
    return CopySerializedResponse(ser, response_data);
}

// detail 空响应：nameSpaceList 为空，触发 INFO_EMPTY 分支。
uint32_t mock_ssu_alloc_detail_invoke_call_empty(uint16_t module_code, uint16_t op_code,
                                                 const ubse_api_buffer_t *request_data,
                                                 ubse_api_buffer_t *response_data)
{
    CaptureSsuRequest(module_code, op_code, request_data);
    UbseCliSsuAllocResult response;
    response.name = "alloc-space-1";
    response.strategy = UbseSsuAllocStrategy::LINEAR;
    UbseSerialization ser;
    response.Serialize(ser);
    return CopySerializedResponse(ser, response_data);
}

// detail 损坏响应：触发反序列化失败分支。
uint32_t mock_ssu_alloc_detail_invoke_call_bad_response(uint16_t module_code, uint16_t op_code,
                                                        const ubse_api_buffer_t *request_data,
                                                        ubse_api_buffer_t *response_data)
{
    CaptureSsuRequest(module_code, op_code, request_data);
    UbseSerialization ser;
    ser << std::string("bad-response");
    return CopySerializedResponse(ser, response_data);
}

// detail 正常响应：复用 BuildSsuAllocResult 的典型两命名空间结果。
uint32_t mock_ssu_alloc_detail_invoke_call_normal(uint16_t module_code, uint16_t op_code,
                                                  const ubse_api_buffer_t *request_data,
                                                  ubse_api_buffer_t *response_data)
{
    CaptureSsuRequest(module_code, op_code, request_data);
    auto response = BuildSsuAllocResult();
    UbseSerialization ser;
    response.Serialize(ser);
    return CopySerializedResponse(ser, response_data);
}
// 返回 512MiB 命名空间容量，用于验证详情表格 size 列统一为 G
uint32_t mock_ssu_alloc_detail_invoke_call_subgib(uint16_t module_code, uint16_t op_code,
                                                  const ubse_api_buffer_t *request_data,
                                                  ubse_api_buffer_t *response_data)
{
    CaptureSsuRequest(module_code, op_code, request_data);
    UbseCliSsuAllocResult response;
    response.name = "alloc-space-1";
    response.strategy = UbseSsuAllocStrategy::LINEAR;
    constexpr uint64_t halfGib = 512ULL * 1024ULL * 1024ULL;
    response.nameSpaceList = {
        {"e2", "nqn.2024-01:target", "uuid-aa", 1, "/dev/nvme0n1", halfGib, UbseSsuLBAFormat::LBA_FORMAT_4K},
    };
    UbseSerialization ser;
    response.Serialize(ser);
    return CopySerializedResponse(ser, response_data);
}

// create 正常响应：分配成功后回显典型结果，供 create 路径的内容断言。
uint32_t mock_ssu_alloc_create_invoke_call_normal(uint16_t module_code, uint16_t op_code,
                                                  const ubse_api_buffer_t *request_data,
                                                  ubse_api_buffer_t *response_data)
{
    CaptureSsuRequest(module_code, op_code, request_data);
    auto response = BuildSsuAllocResult();
    UbseSerialization ser;
    response.Serialize(ser);
    return CopySerializedResponse(ser, response_data);
}
