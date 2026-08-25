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

#include <cstring>
#include <functional>
#include <string>
#include <gtest/gtest.h>

#include <securec.h>

#include "ssu/ubs_ssu_pack.h"
#include "ubs_engine_ssu.h"
#include "ubs_error.h"
#include "util/ubs_engine_pack_util.h"

namespace ubse::sdk::ut {
using namespace ubs::ssu;
using namespace ubs::sdk;

// ============================================================================
// 辅助函数
// ============================================================================

/**
 * 分配指定容量的缓冲区, 通过 filler 填充内容, 并按实际写入字节数设置 length
 */
static ubse_api_buffer_t BuildBuffer(size_t capacity, const std::function<void(PackCtx &)> &filler)
{
    ubse_api_buffer_t buf{};
    buf.buffer = static_cast<uint8_t *>(malloc(capacity));
    if (buf.buffer == nullptr) {
        return buf;
    }
    PackCtx ctx = {buf.buffer, buf.buffer + capacity};
    filler(ctx);
    buf.length = static_cast<uint32_t>(ctx.ptr - buf.buffer);
    return buf;
}

/** 从 UnpackCtx 读取字符串并比较 */
static bool CheckString(UnpackCtx &ctx, const char *expected)
{
    char actual[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    if (unpackString(ctx, actual, UBS_SSU_MAX_DEV_PATH_LENGTH - 1) != UBS_SUCCESS) {
        return false;
    }
    return strcmp(actual, expected) == 0;
}

/** 构造一个完整的 NameSpaceInfo 二进制 (含 1 个 hostNqn) */
static void PackNamespaceInfo(PackCtx &ctx, const char *tgtEid, const char *tgtNqn, const char *nsUuid,
                              uint32_t nsId, const char *nsDevPath, uint64_t nsSize, uint32_t lbaFormat,
                              uint32_t nqnCount, const char *hostNqn)
{
    packString(ctx, tgtEid, UBS_SSU_MAX_EID_LENGTH - 1);
    packString(ctx, tgtNqn, UBS_SSU_MAX_NQN_LENGTH - 1);
    packString(ctx, nsUuid, UBS_SSU_MAX_UUID_LENGTH - 1);
    packValue(ctx, nsId);
    packString(ctx, nsDevPath, UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
    packValue(ctx, nsSize);
    packValue(ctx, lbaFormat);
    packValue(ctx, nqnCount);
    if (nqnCount > 0 && hostNqn != nullptr) {
        packString(ctx, hostNqn, UBS_SSU_MAX_NQN_LENGTH - 1);
    }
}

/** 构造一个 AllocResultPack 二进制 */
static void PackAllocResult(PackCtx &ctx, const char *name, uint8_t strategy, uint32_t nsCount)
{
    packString(ctx, name, UBS_SSU_MAX_NAME_LENGTH - 1);
    packValue(ctx, strategy);
    packValue(ctx, nsCount);
    if (nsCount > 0) {
        PackNamespaceInfo(ctx, "tgt_eid", "nqn.tgt", "uuid-ns-001", 1, "/dev/ns1", 1ULL * 1024 * 1024 * 1024,
                          static_cast<uint32_t>(UBS_SSU_LBA_FORMAT_4K), 1, "nqn.host1");
    }
}

// ============================================================================
// 打包函数测试 (请求打包: 验证返回值 + 二进制格式回读)
// ============================================================================

// ---------- ubs_ssu_space_alloc_pack ----------

TEST(SsuSpaceAllocPack, NullReqReturnsNullPointer)
{
    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_space_alloc_pack(nullptr, buf), UBS_ERR_NULL_POINTER);
}

TEST(SsuSpaceAllocPack, ValidReqPacksCorrectly)
{
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "alloc_test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.tenant, "tenant_1", UBS_SSU_MAX_TENANT_LENGTH - 1);
    req.ns_size = 2ULL * 1024 * 1024 * 1024;
    req.ns_num = 2;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = UBS_SSU_ALLOC_STRATEGY_STRIPED;

    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_space_alloc_pack(&req, buf), UBS_SUCCESS);
    EXPECT_NE(buf.buffer, nullptr);
    EXPECT_GT(buf.length, 0U);

    // 回读验证格式: name(string) + nsSize(u64) + nsNum(u32) + lbaFormat(u32) + strategy(u8) + tenant(string)
    UnpackCtx ctx = {buf.buffer, buf.length};
    EXPECT_TRUE(CheckString(ctx, "alloc_test"));
    uint64_t nsSize = 0;
    EXPECT_EQ(unpackValue(ctx, nsSize), UBS_SUCCESS);
    EXPECT_EQ(nsSize, 2ULL * 1024 * 1024 * 1024);
    uint32_t nsNum = 0;
    EXPECT_EQ(unpackValue(ctx, nsNum), UBS_SUCCESS);
    EXPECT_EQ(nsNum, 2U);
    uint32_t lbaFormat = 0;
    EXPECT_EQ(unpackValue(ctx, lbaFormat), UBS_SUCCESS);
    EXPECT_EQ(lbaFormat, static_cast<uint32_t>(UBS_SSU_LBA_FORMAT_4K));
    uint8_t strategy = 0;
    EXPECT_EQ(unpackValue(ctx, strategy), UBS_SUCCESS);
    EXPECT_EQ(strategy, static_cast<uint8_t>(UBS_SSU_ALLOC_STRATEGY_STRIPED));
    EXPECT_TRUE(CheckString(ctx, "tenant_1"));

    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_space_attach_pack / detach_pack ----------

TEST(SsuSpaceAttachPack, NullReqReturnsNullPointer)
{
    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_space_attach_pack(nullptr, buf), UBS_ERR_NULL_POINTER);
}

TEST(SsuSpaceAttachPack, ValidReqPacksCorrectly)
{
    ubs_ssu_space_req_t req = {};
    strncpy(req.name, "attach_test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.nqn, "nqn.host", UBS_SSU_MAX_NQN_LENGTH - 1);
    strncpy(req.src_eid, "eid1", UBS_SSU_MAX_EID_LENGTH - 1);

    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_space_attach_pack(&req, buf), UBS_SUCCESS);
    EXPECT_NE(buf.buffer, nullptr);

    UnpackCtx ctx = {buf.buffer, buf.length};
    EXPECT_TRUE(CheckString(ctx, "attach_test"));
    EXPECT_TRUE(CheckString(ctx, "nqn.host"));
    EXPECT_TRUE(CheckString(ctx, "eid1"));
    ubse_api_buffer_free(&buf);
}

TEST(SsuSpaceDetachPack, SameFormatAsAttach)
{
    ubs_ssu_space_req_t req = {};
    strncpy(req.name, "detach_test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.nqn, "nqn.host", UBS_SSU_MAX_NQN_LENGTH - 1);
    strncpy(req.src_eid, "eid1", UBS_SSU_MAX_EID_LENGTH - 1);

    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_space_detach_pack(&req, buf), UBS_SUCCESS);
    EXPECT_NE(buf.buffer, nullptr);
    EXPECT_GT(buf.length, 0U);
    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_linear_space_attach_pack / detach_pack ----------

TEST(SsuLinearSpaceAttachPack, NullReqReturnsNullPointer)
{
    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_linear_space_attach_pack(nullptr, buf), UBS_ERR_NULL_POINTER);
}

TEST(SsuLinearSpaceAttachPack, ValidReqPacksCorrectly)
{
    ubs_ssu_linear_space_req_t req = {};
    strncpy(req.name, "lin_test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.nqn, "nqn.host", UBS_SSU_MAX_NQN_LENGTH - 1);
    strncpy(req.src_eid, "eid1", UBS_SSU_MAX_EID_LENGTH - 1);
    strncpy(req.dev_name, "agg_dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);

    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_linear_space_attach_pack(&req, buf), UBS_SUCCESS);
    EXPECT_NE(buf.buffer, nullptr);

    UnpackCtx ctx = {buf.buffer, buf.length};
    EXPECT_TRUE(CheckString(ctx, "lin_test"));
    EXPECT_TRUE(CheckString(ctx, "nqn.host"));
    EXPECT_TRUE(CheckString(ctx, "eid1"));
    EXPECT_TRUE(CheckString(ctx, "agg_dev0"));
    ubse_api_buffer_free(&buf);
}

TEST(SsuLinearSpaceDetachPack, SameFormatAsAttach)
{
    ubs_ssu_linear_space_req_t req = {};
    strncpy(req.name, "lin_detach", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);

    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_linear_space_detach_pack(&req, buf), UBS_SUCCESS);
    EXPECT_GT(buf.length, 0U);
    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_striped_space_attach_pack / detach_pack ----------

TEST(SsuStripedSpaceAttachPack, NullReqReturnsNullPointer)
{
    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_striped_space_attach_pack(nullptr, buf), UBS_ERR_NULL_POINTER);
}

TEST(SsuStripedSpaceAttachPack, ValidReqPacksCorrectly)
{
    ubs_ssu_striped_space_req_t req = {};
    strncpy(req.name, "striped_test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.nqn, "nqn.host", UBS_SSU_MAX_NQN_LENGTH - 1);
    strncpy(req.src_eid, "eid1", UBS_SSU_MAX_EID_LENGTH - 1);
    strncpy(req.dev_name, "agg_dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    req.level = UBS_SSU_RAID5;
    req.chunk_size = UBS_SSU_CHUNK_SIZE_256K;

    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_striped_space_attach_pack(&req, buf), UBS_SUCCESS);
    EXPECT_NE(buf.buffer, nullptr);

    // 格式: name + nqn + srcEid + devName + level(u8) + chunkSize(u32)
    UnpackCtx ctx = {buf.buffer, buf.length};
    EXPECT_TRUE(CheckString(ctx, "striped_test"));
    EXPECT_TRUE(CheckString(ctx, "nqn.host"));
    EXPECT_TRUE(CheckString(ctx, "eid1"));
    EXPECT_TRUE(CheckString(ctx, "agg_dev0"));
    uint8_t level = 0;
    EXPECT_EQ(unpackValue(ctx, level), UBS_SUCCESS);
    EXPECT_EQ(level, static_cast<uint8_t>(UBS_SSU_RAID5));
    uint32_t chunkSize = 0;
    EXPECT_EQ(unpackValue(ctx, chunkSize), UBS_SUCCESS);
    EXPECT_EQ(chunkSize, static_cast<uint32_t>(UBS_SSU_CHUNK_SIZE_256K));
    ubse_api_buffer_free(&buf);
}

TEST(SsuStripedSpaceDetachPack, SameFormatAsAttach)
{
    ubs_ssu_striped_space_req_t req = {};
    strncpy(req.name, "str_detach", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    req.level = UBS_SSU_RAID0;
    req.chunk_size = UBS_SSU_CHUNK_SIZE_4K;

    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_striped_space_detach_pack(&req, buf), UBS_SUCCESS);
    EXPECT_GT(buf.length, 0U);
    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_space_free_pack ----------

TEST(SsuSpaceFreePack, NullNameReturnsNullPointer)
{
    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_space_free_pack(nullptr, buf), UBS_ERR_NULL_POINTER);
}

TEST(SsuSpaceFreePack, ValidNamePacksCorrectly)
{
    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_space_free_pack("free_test", buf), UBS_SUCCESS);
    EXPECT_NE(buf.buffer, nullptr);

    UnpackCtx ctx = {buf.buffer, buf.length};
    EXPECT_TRUE(CheckString(ctx, "free_test"));
    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_access_permission_add_pack / remove_pack ----------

TEST(SsuAccessPermissionAddPack, NullParamsReturnNullPointer)
{
    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_access_permission_add_pack(nullptr, "nqn", buf), UBS_ERR_NULL_POINTER);
    EXPECT_EQ(ubs_ssu_access_permission_add_pack("test", nullptr, buf), UBS_ERR_NULL_POINTER);
}

TEST(SsuAccessPermissionAddPack, ValidParamsPacksCorrectly)
{
    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_access_permission_add_pack("perm_test", "nqn.host", buf), UBS_SUCCESS);
    EXPECT_NE(buf.buffer, nullptr);

    UnpackCtx ctx = {buf.buffer, buf.length};
    EXPECT_TRUE(CheckString(ctx, "perm_test"));
    EXPECT_TRUE(CheckString(ctx, "nqn.host"));
    ubse_api_buffer_free(&buf);
}

TEST(SsuAccessPermissionRemovePack, ValidParamsPacksCorrectly)
{
    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_access_permission_remove_pack("perm_test", "nqn.host", buf), UBS_SUCCESS);
    EXPECT_GT(buf.length, 0U);
    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_ns_stats_get_pack ----------

TEST(SsuNsStatsGetPack, NullNameReturnsNullPointer)
{
    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_ns_stats_get_pack(nullptr, buf), UBS_ERR_NULL_POINTER);
}

TEST(SsuNsStatsGetPack, ValidNamePacksCorrectly)
{
    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_ns_stats_get_pack("stats_test", buf), UBS_SUCCESS);
    EXPECT_NE(buf.buffer, nullptr);

    UnpackCtx ctx = {buf.buffer, buf.length};
    EXPECT_TRUE(CheckString(ctx, "stats_test"));
    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_connect_info_get_pack ----------

TEST(SsuConnectInfoGetPack, NullNameReturnsNullPointer)
{
    ubs_ub_vfe_t vfe = {};
    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_connect_info_get_pack(nullptr, &vfe, buf), UBS_ERR_NULL_POINTER);
}

TEST(SsuConnectInfoGetPack, WithVfePacksCorrectly)
{
    ubs_ub_vfe_t vfe = {};
    vfe.slot_id = 1;
    vfe.chip_id = 2;
    vfe.die_id = 3;
    vfe.pfe_id = 4;
    vfe.vfe_id = 5;
    vfe.vfe_guid[0] = 0xAA;
    vfe.bind_bus_instance_guid[0] = 0xBB;

    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_connect_info_get_pack("conn_test", &vfe, buf), UBS_SUCCESS);
    EXPECT_NE(buf.buffer, nullptr);

    // 格式: name(string) + hasVfe(u8=1) + VfePack(slotId + chipId + dieId + pfeId + vfeId + vfeGuid(32) + bindGuid(32))
    UnpackCtx ctx = {buf.buffer, buf.length};
    EXPECT_TRUE(CheckString(ctx, "conn_test"));
    uint8_t hasVfe = 0;
    EXPECT_EQ(unpackValue(ctx, hasVfe), UBS_SUCCESS);
    EXPECT_EQ(hasVfe, 1U);
    uint8_t slotId = 0;
    EXPECT_EQ(unpackValue(ctx, slotId), UBS_SUCCESS);
    EXPECT_EQ(slotId, 1U);
    uint8_t chipId = 0;
    EXPECT_EQ(unpackValue(ctx, chipId), UBS_SUCCESS);
    EXPECT_EQ(chipId, 2U);
    ubse_api_buffer_free(&buf);
}

TEST(SsuConnectInfoGetPack, WithoutVfePacksCorrectly)
{
    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_connect_info_get_pack("conn_test", nullptr, buf), UBS_SUCCESS);
    EXPECT_NE(buf.buffer, nullptr);

    // 格式: name(string) + hasVfe(u8=0)
    UnpackCtx ctx = {buf.buffer, buf.length};
    EXPECT_TRUE(CheckString(ctx, "conn_test"));
    uint8_t hasVfe = 0;
    EXPECT_EQ(unpackValue(ctx, hasVfe), UBS_SUCCESS);
    EXPECT_EQ(hasVfe, 0U);
    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_fe_device_alloc_pack ----------

TEST(SsuFeDeviceAllocPack, NullParamsReturnNullPointer)
{
    ubs_ub_vfe_t vfe = {};
    uint8_t guid[UBS_SSU_GUID_LENGTH] = {};
    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_fe_device_alloc_pack(1, nullptr, guid, buf), UBS_ERR_NULL_POINTER);
    EXPECT_EQ(ubs_ssu_fe_device_alloc_pack(1, &vfe, nullptr, buf), UBS_ERR_NULL_POINTER);
}

TEST(SsuFeDeviceAllocPack, ValidParamsPacksCorrectly)
{
    ubs_ub_vfe_t vfe = {};
    vfe.slot_id = 1;
    vfe.vfe_id = 5;
    vfe.vfe_guid[0] = 0xAA;
    uint8_t guid[UBS_SSU_GUID_LENGTH] = {};
    guid[0] = 0xCC;

    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_fe_device_alloc_pack(1, &vfe, guid, buf), UBS_SUCCESS);
    EXPECT_NE(buf.buffer, nullptr);

    // 格式: upi(u32) + VfePack + busInstanceGuid(32 bytes)
    UnpackCtx ctx = {buf.buffer, buf.length};
    uint32_t upi = 0;
    EXPECT_EQ(unpackValue(ctx, upi), UBS_SUCCESS);
    EXPECT_EQ(upi, 1U);
    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_fe_device_free_pack ----------

TEST(SsuFeDeviceFreePack, NullVfeReturnsNullPointer)
{
    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_fe_device_free_pack(1, nullptr, buf), UBS_ERR_NULL_POINTER);
}

TEST(SsuFeDeviceFreePack, ValidParamsPacksCorrectly)
{
    ubs_ub_vfe_t vfe = {};
    vfe.slot_id = 1;
    vfe.vfe_id = 5;

    ubse_api_buffer_t buf{};
    EXPECT_EQ(ubs_ssu_fe_device_free_pack(1, &vfe, buf), UBS_SUCCESS);
    EXPECT_NE(buf.buffer, nullptr);

    // 格式: upi(u32) + VfePack
    UnpackCtx ctx = {buf.buffer, buf.length};
    uint32_t upi = 0;
    EXPECT_EQ(unpackValue(ctx, upi), UBS_SUCCESS);
    EXPECT_EQ(upi, 1U);
    ubse_api_buffer_free(&buf);
}

// ============================================================================
// 解包函数测试 (响应解包: 构造二进制 -> 解包 -> 验证输出字段)
// ============================================================================

// ---------- ubs_ssu_space_alloc_unpack ----------

TEST(SsuSpaceAllocUnpack, NullBufferReturnsNullPointer)
{
    ubse_api_buffer_t buf{};
    ubs_ssu_alloc_result_t *result = nullptr;
    EXPECT_EQ(ubs_ssu_space_alloc_unpack(buf, &result), UBS_ERR_NULL_POINTER);
}

TEST(SsuSpaceAllocUnpack, NullResultReturnsNullPointer)
{
    auto buf = BuildBuffer(512, [](PackCtx &ctx) {
        PackAllocResult(ctx, "alloc_result", static_cast<uint8_t>(UBS_SSU_ALLOC_STRATEGY_LINEAR), 1);
    });
    EXPECT_EQ(ubs_ssu_space_alloc_unpack(buf, nullptr), UBS_ERR_NULL_POINTER);
    ubse_api_buffer_free(&buf);
}

TEST(SsuSpaceAllocUnpack, BufferTooSmallReturnsError)
{
    ubse_api_buffer_t buf{};
    buf.length = 2;
    buf.buffer = static_cast<uint8_t *>(malloc(buf.length));
    ubs_ssu_alloc_result_t *result = nullptr;
    EXPECT_NE(ubs_ssu_space_alloc_unpack(buf, &result), UBS_SUCCESS);
    ubse_api_buffer_free(&buf);
}

TEST(SsuSpaceAllocUnpack, ValidBufferUnpacksCorrectly)
{
    auto buf = BuildBuffer(512, [](PackCtx &ctx) {
        PackAllocResult(ctx, "alloc_result", static_cast<uint8_t>(UBS_SSU_ALLOC_STRATEGY_STRIPED), 1);
    });
    ubs_ssu_alloc_result_t *result = nullptr;
    EXPECT_EQ(ubs_ssu_space_alloc_unpack(buf, &result), UBS_SUCCESS);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result->name, "alloc_result");
    EXPECT_EQ(result->strategy, UBS_SSU_ALLOC_STRATEGY_STRIPED);
    EXPECT_EQ(result->namespace_cnt, 1U);
    ASSERT_NE(result->namespaces, nullptr);
    EXPECT_EQ(result->namespaces[0].ns_id, 1U);
    EXPECT_EQ(result->namespaces[0].nqn_count, 1U);
    ASSERT_NE(result->namespaces[0].host_nqns, nullptr);
    EXPECT_NE(result->namespaces[0].host_nqns[0], nullptr);
    ubs_ssu_alloc_info_free(&result);
    EXPECT_EQ(result, nullptr);
    ubse_api_buffer_free(&buf);
}

TEST(SsuSpaceAllocUnpack, ZeroNamespacesUnpacksCorrectly)
{
    auto buf = BuildBuffer(128, [](PackCtx &ctx) {
        PackAllocResult(ctx, "empty_ns", static_cast<uint8_t>(UBS_SSU_ALLOC_STRATEGY_LINEAR), 0);
    });
    ubs_ssu_alloc_result_t *result = nullptr;
    EXPECT_EQ(ubs_ssu_space_alloc_unpack(buf, &result), UBS_SUCCESS);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->namespace_cnt, 0U);
    EXPECT_EQ(result->namespaces, nullptr);
    ubs_ssu_alloc_info_free(&result);
    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_alloc_info_list_unpack ----------

TEST(SsuAllocInfoListUnpack, NullBufferReturnsNullPointer)
{
    ubse_api_buffer_t buf{};
    ubs_ssu_alloc_result_t *results = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_alloc_info_list_unpack(buf, &results, &cnt), UBS_ERR_NULL_POINTER);
}

TEST(SsuAllocInfoListUnpack, NullOutputsReturnNullPointer)
{
    auto buf = BuildBuffer(64, [](PackCtx &ctx) {
        uint32_t listSize = 0;
        packValue(ctx, listSize);
    });
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_alloc_info_list_unpack(buf, nullptr, &cnt), UBS_ERR_NULL_POINTER);
    ubs_ssu_alloc_result_t *results = nullptr;
    EXPECT_EQ(ubs_ssu_alloc_info_list_unpack(buf, &results, nullptr), UBS_ERR_NULL_POINTER);
    ubse_api_buffer_free(&buf);
}

TEST(SsuAllocInfoListUnpack, EmptyListUnpacksCorrectly)
{
    auto buf = BuildBuffer(64, [](PackCtx &ctx) {
        uint32_t listSize = 0;
        packValue(ctx, listSize);
    });
    ubs_ssu_alloc_result_t *results = nullptr;
    uint32_t cnt = 99;
    EXPECT_EQ(ubs_ssu_alloc_info_list_unpack(buf, &results, &cnt), UBS_SUCCESS);
    EXPECT_EQ(cnt, 0U);
    EXPECT_EQ(results, nullptr);
    ubse_api_buffer_free(&buf);
}

TEST(SsuAllocInfoListUnpack, ValidListUnpacksCorrectly)
{
    auto buf = BuildBuffer(512, [](PackCtx &ctx) {
        uint32_t listSize = 1;
        packValue(ctx, listSize);
        PackAllocResult(ctx, "list_result_1", static_cast<uint8_t>(UBS_SSU_ALLOC_STRATEGY_STRIPED), 1);
    });
    ubs_ssu_alloc_result_t *results = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_alloc_info_list_unpack(buf, &results, &cnt), UBS_SUCCESS);
    EXPECT_EQ(cnt, 1U);
    ASSERT_NE(results, nullptr);
    EXPECT_STREQ(results[0].name, "list_result_1");
    ubs_ssu_alloc_info_list_free(&results, cnt);
    EXPECT_EQ(results, nullptr);
    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_ns_stats_get_unpack ----------

TEST(SsuNsStatsGetUnpack, NullBufferReturnsNullPointer)
{
    ubse_api_buffer_t buf{};
    ubs_ssu_ns_stats_t *stats = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_ns_stats_get_unpack(buf, &stats, &cnt), UBS_ERR_NULL_POINTER);
}

TEST(SsuNsStatsGetUnpack, BufferTooSmallReturnsError)
{
    ubse_api_buffer_t buf{};
    buf.length = 2;
    buf.buffer = static_cast<uint8_t *>(malloc(buf.length));
    ubs_ssu_ns_stats_t *stats = nullptr;
    uint32_t cnt = 0;
    EXPECT_NE(ubs_ssu_ns_stats_get_unpack(buf, &stats, &cnt), UBS_SUCCESS);
    ubse_api_buffer_free(&buf);
}

TEST(SsuNsStatsGetUnpack, EmptyListUnpacksCorrectly)
{
    auto buf = BuildBuffer(64, [](PackCtx &ctx) {
        uint32_t listSize = 0;
        packValue(ctx, listSize);
    });
    ubs_ssu_ns_stats_t *stats = nullptr;
    uint32_t cnt = 99;
    EXPECT_EQ(ubs_ssu_ns_stats_get_unpack(buf, &stats, &cnt), UBS_SUCCESS);
    EXPECT_EQ(cnt, 0U);
    EXPECT_EQ(stats, nullptr);
    ubse_api_buffer_free(&buf);
}

TEST(SsuNsStatsGetUnpack, ValidListUnpacksCorrectly)
{
    auto buf = BuildBuffer(256, [](PackCtx &ctx) {
        uint32_t listSize = 1;
        packValue(ctx, listSize);
        packString(ctx, "uuid-stats-1", UBS_SSU_MAX_UUID_LENGTH - 1);
        uint32_t nsId = 5;
        packValue(ctx, nsId);
        uint64_t totalSize = 3ULL * 1024 * 1024 * 1024;
        packValue(ctx, totalSize);
        uint64_t usedSize = 1ULL * 1024 * 1024 * 1024;
        packValue(ctx, usedSize);
    });
    ubs_ssu_ns_stats_t *stats = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_ns_stats_get_unpack(buf, &stats, &cnt), UBS_SUCCESS);
    EXPECT_EQ(cnt, 1U);
    ASSERT_NE(stats, nullptr);
    EXPECT_STREQ(stats[0].ns_uuid, "uuid-stats-1");
    EXPECT_EQ(stats[0].ns_id, 5U);
    EXPECT_EQ(stats[0].total_size, 3ULL * 1024 * 1024 * 1024);
    EXPECT_EQ(stats[0].used_size, 1ULL * 1024 * 1024 * 1024);
    ubs_ssu_ns_stats_free(&stats);
    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_connect_info_get_unpack ----------

TEST(SsuConnectInfoGetUnpack, NullBufferReturnsNullPointer)
{
    ubse_api_buffer_t buf{};
    ubs_ssu_connect_info_t *info = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_connect_info_get_unpack(buf, &info, &cnt), UBS_ERR_NULL_POINTER);
}

TEST(SsuConnectInfoGetUnpack, NullOutputsReturnNullPointer)
{
    auto buf = BuildBuffer(64, [](PackCtx &ctx) {
        uint32_t listSize = 0;
        packValue(ctx, listSize);
    });
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_connect_info_get_unpack(buf, nullptr, &cnt), UBS_ERR_NULL_POINTER);
    ubs_ssu_connect_info_t *info = nullptr;
    EXPECT_EQ(ubs_ssu_connect_info_get_unpack(buf, &info, nullptr), UBS_ERR_NULL_POINTER);
    ubse_api_buffer_free(&buf);
}

TEST(SsuConnectInfoGetUnpack, ValidListUnpacksCorrectly)
{
    auto buf = BuildBuffer(512, [](PackCtx &ctx) {
        uint32_t listSize = 1;
        packValue(ctx, listSize);
        packString(ctx, "src_eid", UBS_SSU_MAX_EID_LENGTH - 1);
        packString(ctx, "tgt_eid", UBS_SSU_MAX_EID_LENGTH - 1);
        packString(ctx, "nqn.tgt", UBS_SSU_MAX_NQN_LENGTH - 1);
        packString(ctx, "nqn.host", UBS_SSU_MAX_NQN_LENGTH - 1);
        packString(ctx, "uuid-conn-1", UBS_SSU_MAX_UUID_LENGTH - 1);
        uint32_t nsId = 7;
        packValue(ctx, nsId);
    });
    ubs_ssu_connect_info_t *info = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_connect_info_get_unpack(buf, &info, &cnt), UBS_SUCCESS);
    EXPECT_EQ(cnt, 1U);
    ASSERT_NE(info, nullptr);
    EXPECT_STREQ(info[0].src_eid, "src_eid");
    EXPECT_STREQ(info[0].tgt_eid, "tgt_eid");
    EXPECT_STREQ(info[0].tgt_nqn, "nqn.tgt");
    EXPECT_STREQ(info[0].host_nqn, "nqn.host");
    EXPECT_STREQ(info[0].ns_uuid, "uuid-conn-1");
    EXPECT_EQ(info[0].ns_id, 7U);
    ubs_ssu_connect_info_free(&info);
    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_fe_device_list_unpack ----------

TEST(SsuFeDeviceListUnpack, NullBufferReturnsNullPointer)
{
    ubse_api_buffer_t buf{};
    ubs_ub_fe_t *feList = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_fe_device_list_unpack(buf, &feList, &cnt), UBS_ERR_NULL_POINTER);
}

TEST(SsuFeDeviceListUnpack, EmptyListUnpacksCorrectly)
{
    auto buf = BuildBuffer(64, [](PackCtx &ctx) {
        uint32_t listSize = 0;
        packValue(ctx, listSize);
    });
    ubs_ub_fe_t *feList = nullptr;
    uint32_t cnt = 99;
    EXPECT_EQ(ubs_ssu_fe_device_list_unpack(buf, &feList, &cnt), UBS_SUCCESS);
    EXPECT_EQ(cnt, 0U);
    EXPECT_EQ(feList, nullptr);
    ubse_api_buffer_free(&buf);
}

TEST(SsuFeDeviceListUnpack, ValidListUnpacksCorrectly)
{
    auto buf = BuildBuffer(256, [](PackCtx &ctx) {
        uint32_t listSize = 1;
        packValue(ctx, listSize);
        // FePack
        uint8_t slotId = 1;
        uint8_t chipId = 2;
        uint8_t dieId = 3;
        uint16_t pfeId = 4;
        packValue(ctx, slotId);
        packValue(ctx, chipId);
        packValue(ctx, dieId);
        packValue(ctx, pfeId);
        uint8_t pfeGuid[UBS_SSU_GUID_LENGTH] = {0};
        pfeGuid[0] = 0xAA;
        packArray(ctx, pfeGuid, UBS_SSU_GUID_LENGTH);
        uint32_t vfeCount = 1;
        packValue(ctx, vfeCount);
        // VfePack
        uint8_t vSlotId = 1;
        uint8_t vChipId = 2;
        uint8_t vDieId = 3;
        uint16_t vPfeId = 4;
        uint16_t vfeId = 5;
        packValue(ctx, vSlotId);
        packValue(ctx, vChipId);
        packValue(ctx, vDieId);
        packValue(ctx, vPfeId);
        packValue(ctx, vfeId);
        uint8_t vfeGuid[UBS_SSU_GUID_LENGTH] = {0};
        vfeGuid[0] = 0xBB;
        packArray(ctx, vfeGuid, UBS_SSU_GUID_LENGTH);
        uint8_t bindGuid[UBS_SSU_GUID_LENGTH] = {0};
        bindGuid[0] = 0xCC;
        packArray(ctx, bindGuid, UBS_SSU_GUID_LENGTH);
    });
    ubs_ub_fe_t *feList = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_fe_device_list_unpack(buf, &feList, &cnt), UBS_SUCCESS);
    EXPECT_EQ(cnt, 1U);
    ASSERT_NE(feList, nullptr);
    EXPECT_EQ(feList[0].slot_id, 1U);
    EXPECT_EQ(feList[0].chip_id, 2U);
    EXPECT_EQ(feList[0].die_id, 3U);
    EXPECT_EQ(feList[0].pfe_id, 4U);
    EXPECT_EQ(feList[0].pfe_guid[0], 0xAA);
    EXPECT_EQ(feList[0].vfe_cnt, 1U);
    ASSERT_NE(feList[0].vfe_list, nullptr);
    EXPECT_EQ(feList[0].vfe_list[0].vfe_id, 5U);
    EXPECT_EQ(feList[0].vfe_list[0].vfe_guid[0], 0xBB);
    EXPECT_EQ(feList[0].vfe_list[0].bind_bus_instance_guid[0], 0xCC);
    ubs_ssu_fe_device_list_free(&feList, cnt);
    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_space_attach_unpack ----------

TEST(SsuSpaceAttachUnpack, NullBufferReturnsNullPointer)
{
    ubse_api_buffer_t buf{};
    char **paths = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_space_attach_unpack(buf, &paths, &cnt), UBS_ERR_NULL_POINTER);
}

TEST(SsuSpaceAttachUnpack, NullOutputsReturnNullPointer)
{
    auto buf = BuildBuffer(64, [](PackCtx &ctx) {
        uint32_t count = 0;
        packValue(ctx, count);
    });
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_space_attach_unpack(buf, nullptr, &cnt), UBS_ERR_NULL_POINTER);
    char **paths = nullptr;
    EXPECT_EQ(ubs_ssu_space_attach_unpack(buf, &paths, nullptr), UBS_ERR_NULL_POINTER);
    ubse_api_buffer_free(&buf);
}

TEST(SsuSpaceAttachUnpack, ValidListUnpacksCorrectly)
{
    auto buf = BuildBuffer(256, [](PackCtx &ctx) {
        uint32_t count = 2;
        packValue(ctx, count);
        packString(ctx, "/dev/ns0", UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
        packString(ctx, "/dev/ns1", UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
    });
    char **paths = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_space_attach_unpack(buf, &paths, &cnt), UBS_SUCCESS);
    EXPECT_EQ(cnt, 2U);
    ASSERT_NE(paths, nullptr);
    EXPECT_NE(paths[0], nullptr);
    EXPECT_STREQ(paths[0], "/dev/ns0");
    EXPECT_NE(paths[1], nullptr);
    EXPECT_STREQ(paths[1], "/dev/ns1");
    ubs_ssu_ns_dev_paths_free(&paths, cnt);
    ubse_api_buffer_free(&buf);
}

TEST(SsuSpaceAttachUnpack, EmptyListUnpacksCorrectly)
{
    auto buf = BuildBuffer(64, [](PackCtx &ctx) {
        uint32_t count = 0;
        packValue(ctx, count);
    });
    char **paths = nullptr;
    uint32_t cnt = 99;
    EXPECT_EQ(ubs_ssu_space_attach_unpack(buf, &paths, &cnt), UBS_SUCCESS);
    EXPECT_EQ(cnt, 0U);
    EXPECT_EQ(paths, nullptr);
    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_linear_space_attach_unpack / striped_space_attach_unpack ----------

TEST(SsuLinearSpaceAttachUnpack, NullBufferReturnsNullPointer)
{
    ubse_api_buffer_t buf{};
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    EXPECT_EQ(ubs_ssu_linear_space_attach_unpack(buf, &paths, &cnt, devPath), UBS_ERR_NULL_POINTER);
}

TEST(SsuLinearSpaceAttachUnpack, NullDevPathReturnsNullPointer)
{
    auto buf = BuildBuffer(64, [](PackCtx &ctx) {
        uint32_t count = 0;
        packValue(ctx, count);
        packString(ctx, "/dev/agg", UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
    });
    char **paths = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_linear_space_attach_unpack(buf, &paths, &cnt, nullptr), UBS_ERR_NULL_POINTER);
    ubse_api_buffer_free(&buf);
}

TEST(SsuLinearSpaceAttachUnpack, ValidBufferUnpacksCorrectly)
{
    auto buf = BuildBuffer(256, [](PackCtx &ctx) {
        uint32_t count = 2;
        packValue(ctx, count);
        packString(ctx, "/dev/ns0", UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
        packString(ctx, "/dev/ns1", UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
        packString(ctx, "/dev/mapper/agg0", UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
    });
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    EXPECT_EQ(ubs_ssu_linear_space_attach_unpack(buf, &paths, &cnt, devPath), UBS_SUCCESS);
    EXPECT_EQ(cnt, 2U);
    ASSERT_NE(paths, nullptr);
    EXPECT_STREQ(devPath, "/dev/mapper/agg0");
    ubs_ssu_ns_dev_paths_free(&paths, cnt);
    ubse_api_buffer_free(&buf);
}

TEST(SsuStripedSpaceAttachUnpack, SameFormatAsLinear)
{
    auto buf = BuildBuffer(256, [](PackCtx &ctx) {
        uint32_t count = 1;
        packValue(ctx, count);
        packString(ctx, "/dev/ns0", UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
        packString(ctx, "/dev/mapper/striped0", UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
    });
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    EXPECT_EQ(ubs_ssu_striped_space_attach_unpack(buf, &paths, &cnt, devPath), UBS_SUCCESS);
    EXPECT_EQ(cnt, 1U);
    EXPECT_STREQ(devPath, "/dev/mapper/striped0");
    ubs_ssu_ns_dev_paths_free(&paths, cnt);
    ubse_api_buffer_free(&buf);
}

// ---------- ubs_ssu_fe_device_alloc_unpack ----------

TEST(SsuFeDeviceAllocUnpack, NullBufferReturnsNullPointer)
{
    ubse_api_buffer_t buf{};
    uint8_t guid[UBS_SSU_GUID_LENGTH] = {};
    EXPECT_EQ(ubs_ssu_fe_device_alloc_unpack(buf, guid), UBS_ERR_NULL_POINTER);
}

TEST(SsuFeDeviceAllocUnpack, NullGuidReturnsNullPointer)
{
    auto buf = BuildBuffer(UBS_SSU_GUID_LENGTH, [](PackCtx &ctx) {
        uint8_t guid[UBS_SSU_GUID_LENGTH] = {0};
        guid[0] = 0xDD;
        packArray(ctx, guid, UBS_SSU_GUID_LENGTH);
    });
    EXPECT_EQ(ubs_ssu_fe_device_alloc_unpack(buf, nullptr), UBS_ERR_NULL_POINTER);
    ubse_api_buffer_free(&buf);
}

TEST(SsuFeDeviceAllocUnpack, BufferTooSmallReturnsError)
{
    ubse_api_buffer_t buf{};
    buf.length = 2;
    buf.buffer = static_cast<uint8_t *>(malloc(buf.length));
    uint8_t guid[UBS_SSU_GUID_LENGTH] = {};
    EXPECT_NE(ubs_ssu_fe_device_alloc_unpack(buf, guid), UBS_SUCCESS);
    ubse_api_buffer_free(&buf);
}

TEST(SsuFeDeviceAllocUnpack, ValidBufferUnpacksCorrectly)
{
    auto buf = BuildBuffer(UBS_SSU_GUID_LENGTH + 8, [](PackCtx &ctx) {
        uint8_t guid[UBS_SSU_GUID_LENGTH] = {0};
        guid[0] = 0xDD;
        guid[31] = 0xEE;
        packArray(ctx, guid, UBS_SSU_GUID_LENGTH);
    });
    uint8_t guid[UBS_SSU_GUID_LENGTH] = {};
    EXPECT_EQ(ubs_ssu_fe_device_alloc_unpack(buf, guid), UBS_SUCCESS);
    EXPECT_EQ(guid[0], 0xDD);
    EXPECT_EQ(guid[31], 0xEE);
    ubse_api_buffer_free(&buf);
}

// ============================================================================
// 异常输入测试 (恶意 count / 截断数据)
// ============================================================================

TEST(SsuAllocInfoListUnpack, MaliciousListCountReturnsOutOfRange)
{
    // 构造恶意回包: listSize 超过上限
    auto buf = BuildBuffer(16, [](PackCtx &ctx) {
        uint32_t listSize = 200000; // 超过 UBS_SSU_MAX_LIST_COUNT
        packValue(ctx, listSize);
    });
    ubs_ssu_alloc_result_t *results = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_alloc_info_list_unpack(buf, &results, &cnt), UBS_ERR_OUT_OF_RANGE);
    ubse_api_buffer_free(&buf);
}

TEST(SsuNsStatsGetUnpack, MaliciousListCountReturnsOutOfRange)
{
    auto buf = BuildBuffer(16, [](PackCtx &ctx) {
        uint32_t listSize = 200000;
        packValue(ctx, listSize);
    });
    ubs_ssu_ns_stats_t *stats = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_ns_stats_get_unpack(buf, &stats, &cnt), UBS_ERR_OUT_OF_RANGE);
    ubse_api_buffer_free(&buf);
}

TEST(SsuFeDeviceListUnpack, MaliciousListCountReturnsOutOfRange)
{
    auto buf = BuildBuffer(16, [](PackCtx &ctx) {
        uint32_t listSize = 200000;
        packValue(ctx, listSize);
    });
    ubs_ub_fe_t *feList = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_fe_device_list_unpack(buf, &feList, &cnt), UBS_ERR_OUT_OF_RANGE);
    ubse_api_buffer_free(&buf);
}

TEST(SsuSpaceAttachUnpack, MaliciousListCountReturnsOutOfRange)
{
    auto buf = BuildBuffer(16, [](PackCtx &ctx) {
        uint32_t count = 200000;
        packValue(ctx, count);
    });
    char **paths = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_space_attach_unpack(buf, &paths, &cnt), UBS_ERR_OUT_OF_RANGE);
    ubse_api_buffer_free(&buf);
}
} // namespace ubse::sdk::ut
