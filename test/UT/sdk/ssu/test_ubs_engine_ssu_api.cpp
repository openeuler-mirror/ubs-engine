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
#include <mockcpp/mockcpp.hpp>

#include "ssu/ubs_ssu_pack.h"
#include "ssu/ubs_ssu_validate.h"
#include "ubs_engine_ssu.h"
#include "ubs_error.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "util/ubs_engine_pack_util.h"

namespace ubse::sdk::ut {
using namespace ubs::ssu;
using namespace ubs::sdk;

// ============================================================================
// 响应缓冲区构造辅助函数
// 模拟服务端打包格式, 用于构造正常回包供解包测试使用
// ============================================================================

/**
 * 分配指定容量的缓冲区, 通过 filler 填充内容, 并按实际写入字节数设置 length
 */
static ubse_api_buffer_t BuildResponse(size_t capacity, const std::function<void(PackCtx &)> &filler)
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

/** 打包一个 NameSpaceInfo (含 1 个 hostNqn) */
static void PackNamespaceInfo(PackCtx &ctx, const char *tgtEid, const char *tgtNqn, const char *nsUuid,
                              uint32_t nsId, const char *nsDevPath, uint64_t nsSize, uint32_t lbaFormat,
                              const char *hostNqn)
{
    packString(ctx, tgtEid, UBS_SSU_MAX_EID_LENGTH - 1);
    packString(ctx, tgtNqn, UBS_SSU_MAX_NQN_LENGTH - 1);
    packString(ctx, nsUuid, UBS_SSU_MAX_UUID_LENGTH - 1);
    packValue(ctx, nsId);
    packString(ctx, nsDevPath, UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
    packValue(ctx, nsSize);
    packValue(ctx, lbaFormat);
    // hostNqn 列表: count(uint32) + [hostNqn(string)]*count
    uint32_t nqnCount = (hostNqn != nullptr) ? 1 : 0;
    packValue(ctx, nqnCount);
    if (hostNqn != nullptr) {
        packString(ctx, hostNqn, UBS_SSU_MAX_NQN_LENGTH - 1);
    }
}

/** 打包一个 AllocResultPack */
static void PackAllocResult(PackCtx &ctx, const char *name, uint8_t strategy)
{
    packString(ctx, name, UBS_SSU_MAX_NAME_LENGTH - 1);
    packValue(ctx, strategy);
    uint32_t nsListSize = 1;
    packValue(ctx, nsListSize);
    PackNamespaceInfo(ctx, "tgt_eid", "nqn.tgt", "uuid-ns-001", 1, "/dev/ns1", 1ULL * 1024 * 1024 * 1024,
                      static_cast<uint32_t>(UBS_SSU_LBA_FORMAT_4K), "nqn.host1");
}

/** 构造 ubs_ssu_alloc_info_list 的响应: allocResultListSize(uint32) + [AllocResultPack]*size */
static ubse_api_buffer_t BuildAllocInfoListResponse()
{
    return BuildResponse(512, [](PackCtx &ctx) {
        uint32_t listSize = 1;
        packValue(ctx, listSize);
        PackAllocResult(ctx, "alloc_result_1", static_cast<uint8_t>(UBS_SSU_ALLOC_STRATEGY_STRIPED));
    });
}

/** 构造 ubs_ssu_space_alloc 的响应: AllocResultPack (单个, 无列表前缀) */
static ubse_api_buffer_t BuildSpaceAllocResponse()
{
    return BuildResponse(512, [](PackCtx &ctx) {
        PackAllocResult(ctx, "alloc_result_1", static_cast<uint8_t>(UBS_SSU_ALLOC_STRATEGY_LINEAR));
    });
}

/** 构造 ubs_ssu_ns_stats_get 的响应: listSize(uint32) + [NsStats]*size */
static ubse_api_buffer_t BuildNsStatsResponse()
{
    return BuildResponse(256, [](PackCtx &ctx) {
        uint32_t listSize = 1;
        packValue(ctx, listSize);
        packString(ctx, "uuid-stats-001", UBS_SSU_MAX_UUID_LENGTH - 1);
        uint32_t nsId = 1;
        packValue(ctx, nsId);
        uint64_t totalSize = 2ULL * 1024 * 1024 * 1024;
        packValue(ctx, totalSize);
        uint64_t usedSize = 512ULL * 1024 * 1024;
        packValue(ctx, usedSize);
    });
}

/** 构造 ubs_ssu_connect_info_get 的响应: listSize(uint32) + [ConnectInfo]*size */
static ubse_api_buffer_t BuildConnectInfoResponse()
{
    return BuildResponse(512, [](PackCtx &ctx) {
        uint32_t listSize = 1;
        packValue(ctx, listSize);
        packString(ctx, "src_eid", UBS_SSU_MAX_EID_LENGTH - 1);
        packString(ctx, "tgt_eid", UBS_SSU_MAX_EID_LENGTH - 1);
        packString(ctx, "nqn.tgt", UBS_SSU_MAX_NQN_LENGTH - 1);
        packString(ctx, "nqn.host", UBS_SSU_MAX_NQN_LENGTH - 1);
        packString(ctx, "uuid-conn-001", UBS_SSU_MAX_UUID_LENGTH - 1);
        uint32_t nsId = 1;
        packValue(ctx, nsId);
    });
}

/** 构造 ubs_ssu_fe_device_list 的响应: feListSize(uint32) + [FePack]*size (含 1 个 VFE) */
static ubse_api_buffer_t BuildFeDeviceListResponse()
{
    return BuildResponse(256, [](PackCtx &ctx) {
        uint32_t listSize = 1;
        packValue(ctx, listSize);
        // FePack: slotId(u8) + chipId(u8) + dieId(u8) + pfeId(u16) + pfeGuid(32 bytes) + vfeCount(u32) + [VfePack]
        uint8_t slotId = 1;
        uint8_t chipId = 1;
        uint8_t dieId = 1;
        uint16_t pfeId = 1;
        packValue(ctx, slotId);
        packValue(ctx, chipId);
        packValue(ctx, dieId);
        packValue(ctx, pfeId);
        uint8_t pfeGuid[UBS_SSU_GUID_LENGTH] = {0};
        pfeGuid[0] = 0xAA;
        packArray(ctx, pfeGuid, UBS_SSU_GUID_LENGTH);
        uint32_t vfeCount = 1;
        packValue(ctx, vfeCount);
        // VfePack: slotId(u8) + chipId(u8) + dieId(u8) + pfeId(u16) + vfeId(u16) + vfeGuid(32) + bindBusInstanceGuid(32)
        uint8_t vSlotId = 1;
        uint8_t vChipId = 1;
        uint8_t vDieId = 1;
        uint16_t vPfeId = 1;
        uint16_t vfeId = 1;
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
}

/** 构造 ubs_ssu_space_attach 的响应: nsDevPathsCount(uint32) + [nsDevPath(string)]*count */
static ubse_api_buffer_t BuildNsDevPathsResponse()
{
    return BuildResponse(256, [](PackCtx &ctx) {
        uint32_t count = 2;
        packValue(ctx, count);
        packString(ctx, "/dev/ns0", UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
        packString(ctx, "/dev/ns1", UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
    });
}

/** 构造 ubs_ssu_linear/striped_space_attach 的响应: nsDevPathsCount + [nsDevPath]*count + devPath(string) */
static ubse_api_buffer_t BuildLinearAttachResponse()
{
    return BuildResponse(256, [](PackCtx &ctx) {
        uint32_t count = 2;
        packValue(ctx, count);
        packString(ctx, "/dev/ns0", UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
        packString(ctx, "/dev/ns1", UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
        packString(ctx, "/dev/mapper/agg0", UBS_SSU_MAX_DEV_PATH_LENGTH - 1);
    });
}

/** 构造 ubs_ssu_fe_device_alloc 的响应: busInstanceGuid(定长 32 字节) */
static ubse_api_buffer_t BuildFeDeviceAllocResponse()
{
    return BuildResponse(UBS_SSU_GUID_LENGTH + 8, [](PackCtx &ctx) {
        uint8_t guid[UBS_SSU_GUID_LENGTH] = {0};
        guid[0] = 0xDD;
        guid[31] = 0xEE;
        packArray(ctx, guid, UBS_SSU_GUID_LENGTH);
    });
}

/** 构造非法响应缓冲区 (长度不足, 用于解包失败测试) */
static ubse_api_buffer_t BuildInvalidResponse()
{
    ubse_api_buffer_t buf{};
    buf.length = 2; // 长度不足, 必然解包失败
    buf.buffer = static_cast<uint8_t *>(malloc(buf.length));
    return buf;
}

/**
 * 构造请求结构体 (用于 API 参数), 通过 filler 填充
 * 使用完由调用方负责释放 result 相关内存
 */
class TestUbsEngineSsu : public testing::Test {
public:
    TestUbsEngineSsu() = default;

    void SetUp() override
    {
        Test::SetUp();
    }

    void TearDown() override
    {
        GlobalMockObject::verify();
        Test::TearDown();
    }

    static ubs_ssu_alloc_space_req_t MakeAllocSpaceReq()
    {
        ubs_ssu_alloc_space_req_t req = {};
        strncpy(req.name, "test_alloc", UBS_SSU_MAX_NAME_LENGTH - 1);
        req.ns_size = 2ULL * 1024 * 1024 * 1024;
        req.ns_num = 2;
        req.lba_format = UBS_SSU_LBA_FORMAT_4K;
        req.strategy = UBS_SSU_ALLOC_STRATEGY_STRIPED;
        return req;
    }

    static ubs_ssu_space_req_t MakeSpaceReq()
    {
        ubs_ssu_space_req_t req = {};
        strncpy(req.name, "test_space", UBS_SSU_MAX_NAME_LENGTH - 1);
        strncpy(req.nqn, "nqn.2024-01.com.huawei:host", UBS_SSU_MAX_NQN_LENGTH - 1);
        strncpy(req.src_eid, "eid_src", UBS_SSU_MAX_EID_LENGTH - 1);
        return req;
    }

    static ubs_ssu_linear_space_req_t MakeLinearSpaceReq()
    {
        ubs_ssu_linear_space_req_t req = {};
        strncpy(req.name, "test_linear", UBS_SSU_MAX_NAME_LENGTH - 1);
        strncpy(req.nqn, "nqn.2024-01.com.huawei:host", UBS_SSU_MAX_NQN_LENGTH - 1);
        strncpy(req.src_eid, "eid_src", UBS_SSU_MAX_EID_LENGTH - 1);
        strncpy(req.dev_name, "agg_dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
        return req;
    }

    static ubs_ssu_striped_space_req_t MakeStripedSpaceReq()
    {
        ubs_ssu_striped_space_req_t req = {};
        strncpy(req.name, "test_striped", UBS_SSU_MAX_NAME_LENGTH - 1);
        strncpy(req.nqn, "nqn.2024-01.com.huawei:host", UBS_SSU_MAX_NQN_LENGTH - 1);
        strncpy(req.src_eid, "eid_src", UBS_SSU_MAX_EID_LENGTH - 1);
        strncpy(req.dev_name, "agg_dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
        req.level = UBS_SSU_RAID0;
        req.chunk_size = UBS_SSU_CHUNK_SIZE_64K;
        return req;
    }

    static ubs_ub_vfe_t MakeVfe()
    {
        ubs_ub_vfe_t vfe = {};
        vfe.slot_id = 1;
        vfe.chip_id = 1;
        vfe.die_id = 1;
        vfe.pfe_id = 1;
        vfe.vfe_id = 1;
        vfe.vfe_guid[0] = 0xBB;
        vfe.bind_bus_instance_guid[0] = 0xCC;
        return vfe;
    }
};

// ============================================================================
// ubs_ssu_alloc_info_list
// ============================================================================

TEST_F(TestUbsEngineSsu, AllocInfoListWhenNullParameters)
{
    ubs_ssu_alloc_result_t *results = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_alloc_info_list(nullptr, &cnt), UBS_ERR_NULL_POINTER);
    EXPECT_EQ(ubs_ssu_alloc_info_list(&results, nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsu, AllocInfoListWhenInvokeCallFailed)
{
    ubs_ssu_alloc_result_t *results = nullptr;
    uint32_t cnt = 0;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_CONNECTION_FAILED));
    auto ret = ubs_ssu_alloc_info_list(&results, &cnt);
    EXPECT_EQ(ret, UBS_ERR_IPC_CONNECTION_FAILED);
}

TEST_F(TestUbsEngineSsu, AllocInfoListWhenDaemonInternalErrorMapped)
{
    // 内部错误码 (>=10000) 应统一映射为 UBS_ENGINE_ERR_INTERNAL
    ubs_ssu_alloc_result_t *results = nullptr;
    uint32_t cnt = 0;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(static_cast<uint32_t>(UBSE_ERROR)));
    auto ret = ubs_ssu_alloc_info_list(&results, &cnt);
    EXPECT_EQ(ret, UBS_ENGINE_ERR_INTERNAL);
}

TEST_F(TestUbsEngineSsu, AllocInfoListWhenUnpackFailed)
{
    ubs_ssu_alloc_result_t *results = nullptr;
    uint32_t cnt = 0;
    auto respBuffer = BuildInvalidResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_alloc_info_list(&results, &cnt);
    EXPECT_NE(ret, UBS_SUCCESS);
}

TEST_F(TestUbsEngineSsu, AllocInfoListWhenSuccess)
{
    ubs_ssu_alloc_result_t *results = nullptr;
    uint32_t cnt = 0;
    auto respBuffer = BuildAllocInfoListResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_alloc_info_list(&results, &cnt);
    EXPECT_EQ(ret, UBS_SUCCESS);
    EXPECT_EQ(cnt, 1U);
    EXPECT_NE(results, nullptr);
    ubs_ssu_alloc_info_list_free(&results, cnt);
    EXPECT_EQ(results, nullptr);
}

// ============================================================================
// ubs_ssu_ns_stats_get
// ============================================================================

TEST_F(TestUbsEngineSsu, NsStatsGetWhenInvalidName)
{
    ubs_ssu_ns_stats_t *stats = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_ns_stats_get(nullptr, &stats, &cnt), UBS_ERR_NULL_POINTER);
    EXPECT_EQ(ubs_ssu_ns_stats_get("", &stats, &cnt), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsu, NsStatsGetWhenNullParameters)
{
    ubs_ssu_ns_stats_t *stats = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_ns_stats_get("test", nullptr, &cnt), UBS_ERR_NULL_POINTER);
    EXPECT_EQ(ubs_ssu_ns_stats_get("test", &stats, nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsu, NsStatsGetWhenInvokeCallFailed)
{
    ubs_ssu_ns_stats_t *stats = nullptr;
    uint32_t cnt = 0;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_TIMEOUT));
    auto ret = ubs_ssu_ns_stats_get("test", &stats, &cnt);
    EXPECT_EQ(ret, UBS_ERR_IPC_TIMEOUT);
}

TEST_F(TestUbsEngineSsu, NsStatsGetWhenUnpackFailed)
{
    ubs_ssu_ns_stats_t *stats = nullptr;
    uint32_t cnt = 0;
    auto respBuffer = BuildInvalidResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_ns_stats_get("test", &stats, &cnt);
    EXPECT_NE(ret, UBS_SUCCESS);
}

TEST_F(TestUbsEngineSsu, NsStatsGetWhenSuccess)
{
    ubs_ssu_ns_stats_t *stats = nullptr;
    uint32_t cnt = 0;
    auto respBuffer = BuildNsStatsResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_ns_stats_get("test", &stats, &cnt);
    EXPECT_EQ(ret, UBS_SUCCESS);
    EXPECT_EQ(cnt, 1U);
    EXPECT_NE(stats, nullptr);
    EXPECT_EQ(stats[0].ns_id, 1U);
    ubs_ssu_ns_stats_free(&stats);
    EXPECT_EQ(stats, nullptr);
}

// ============================================================================
// ubs_ssu_connect_info_get
// ============================================================================

TEST_F(TestUbsEngineSsu, ConnectInfoGetWhenInvalidName)
{
    ubs_ssu_connect_info_t *info = nullptr;
    uint32_t cnt = 0;
    ubs_ub_vfe_t vfe = MakeVfe();
    EXPECT_EQ(ubs_ssu_connect_info_get(nullptr, &vfe, &info, &cnt), UBS_ERR_NULL_POINTER);
    EXPECT_EQ(ubs_ssu_connect_info_get("", &vfe, &info, &cnt), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsu, ConnectInfoGetWhenNullParameters)
{
    ubs_ssu_connect_info_t *info = nullptr;
    uint32_t cnt = 0;
    ubs_ub_vfe_t vfe = MakeVfe();
    EXPECT_EQ(ubs_ssu_connect_info_get("test", &vfe, nullptr, &cnt), UBS_ERR_NULL_POINTER);
    EXPECT_EQ(ubs_ssu_connect_info_get("test", &vfe, &info, nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsu, ConnectInfoGetWhenInvokeCallFailed)
{
    ubs_ssu_connect_info_t *info = nullptr;
    uint32_t cnt = 0;
    ubs_ub_vfe_t vfe = MakeVfe();
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_IPC_SERVICE_UNAVAILABLE));
    auto ret = ubs_ssu_connect_info_get("test", &vfe, &info, &cnt);
    EXPECT_EQ(ret, UBS_ERR_IPC_SERVICE_UNAVAILABLE);
}

TEST_F(TestUbsEngineSsu, ConnectInfoGetWhenUnpackFailed)
{
    ubs_ssu_connect_info_t *info = nullptr;
    uint32_t cnt = 0;
    ubs_ub_vfe_t vfe = MakeVfe();
    auto respBuffer = BuildInvalidResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_connect_info_get("test", &vfe, &info, &cnt);
    EXPECT_NE(ret, UBS_SUCCESS);
}

TEST_F(TestUbsEngineSsu, ConnectInfoGetWhenSuccess)
{
    ubs_ssu_connect_info_t *info = nullptr;
    uint32_t cnt = 0;
    ubs_ub_vfe_t vfe = MakeVfe();
    auto respBuffer = BuildConnectInfoResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_connect_info_get("test", &vfe, &info, &cnt);
    EXPECT_EQ(ret, UBS_SUCCESS);
    EXPECT_EQ(cnt, 1U);
    EXPECT_NE(info, nullptr);
    ubs_ssu_connect_info_free(&info);
    EXPECT_EQ(info, nullptr);
}

// ============================================================================
// ubs_ssu_fe_device_list
// ============================================================================

TEST_F(TestUbsEngineSsu, FeDeviceListWhenNullParameters)
{
    ubs_ub_fe_t *feList = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_fe_device_list(nullptr, &cnt), UBS_ERR_NULL_POINTER);
    EXPECT_EQ(ubs_ssu_fe_device_list(&feList, nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsu, FeDeviceListWhenInvokeCallFailed)
{
    ubs_ub_fe_t *feList = nullptr;
    uint32_t cnt = 0;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_DAEMON_UNREACHABLE));
    auto ret = ubs_ssu_fe_device_list(&feList, &cnt);
    EXPECT_EQ(ret, UBS_ERR_DAEMON_UNREACHABLE);
}

TEST_F(TestUbsEngineSsu, FeDeviceListWhenUnpackFailed)
{
    ubs_ub_fe_t *feList = nullptr;
    uint32_t cnt = 0;
    auto respBuffer = BuildInvalidResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_fe_device_list(&feList, &cnt);
    EXPECT_NE(ret, UBS_SUCCESS);
}

TEST_F(TestUbsEngineSsu, FeDeviceListWhenSuccess)
{
    ubs_ub_fe_t *feList = nullptr;
    uint32_t cnt = 0;
    auto respBuffer = BuildFeDeviceListResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_fe_device_list(&feList, &cnt);
    EXPECT_EQ(ret, UBS_SUCCESS);
    EXPECT_EQ(cnt, 1U);
    EXPECT_NE(feList, nullptr);
    EXPECT_EQ(feList[0].vfe_cnt, 1U);
    ubs_ssu_fe_device_list_free(&feList, cnt);
    EXPECT_EQ(feList, nullptr);
}

// ============================================================================
// ubs_ssu_space_alloc
// ============================================================================

TEST_F(TestUbsEngineSsu, SpaceAllocWhenNullResult)
{
    auto req = MakeAllocSpaceReq();
    EXPECT_EQ(ubs_ssu_space_alloc(&req, nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsu, SpaceAllocWhenInvalidReq)
{
    ubs_ssu_alloc_result_t *result = nullptr;
    // null req
    EXPECT_EQ(ubs_ssu_space_alloc(nullptr, &result), UBS_ERR_NULL_POINTER);
    // invalid name (empty)
    ubs_ssu_alloc_space_req_t req = {};
    req.ns_size = 1ULL * 1024 * 1024 * 1024;
    req.ns_num = 1;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = UBS_SSU_ALLOC_STRATEGY_LINEAR;
    EXPECT_EQ(ubs_ssu_space_alloc(&req, &result), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsu, SpaceAllocWhenInvokeCallFailed)
{
    auto req = MakeAllocSpaceReq();
    ubs_ssu_alloc_result_t *result = nullptr;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_RESOURCE_BUSY));
    auto ret = ubs_ssu_space_alloc(&req, &result);
    EXPECT_EQ(ret, UBS_ERR_RESOURCE_BUSY);
    EXPECT_EQ(result, nullptr); // 失败时出参预置为 nullptr
}

TEST_F(TestUbsEngineSsu, SpaceAllocWhenUnpackFailed)
{
    auto req = MakeAllocSpaceReq();
    ubs_ssu_alloc_result_t *result = nullptr;
    auto respBuffer = BuildInvalidResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_space_alloc(&req, &result);
    EXPECT_NE(ret, UBS_SUCCESS);
}

TEST_F(TestUbsEngineSsu, SpaceAllocWhenSuccess)
{
    auto req = MakeAllocSpaceReq();
    ubs_ssu_alloc_result_t *result = nullptr;
    auto respBuffer = BuildSpaceAllocResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_space_alloc(&req, &result);
    EXPECT_EQ(ret, UBS_SUCCESS);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->namespace_cnt, 1U);
    ubs_ssu_alloc_info_free(&result);
    EXPECT_EQ(result, nullptr);
}

// ============================================================================
// ubs_ssu_space_free
// ============================================================================

TEST_F(TestUbsEngineSsu, SpaceFreeWhenInvalidName)
{
    EXPECT_EQ(ubs_ssu_space_free(nullptr), UBS_ERR_NULL_POINTER);
    EXPECT_EQ(ubs_ssu_space_free(""), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsu, SpaceFreeWhenInvokeCallFailed)
{
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_OPERATION_FAILED));
    EXPECT_EQ(ubs_ssu_space_free("test"), UBSE_ERR_OPERATION_FAILED);
}

TEST_F(TestUbsEngineSsu, SpaceFreeWhenSuccess)
{
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubs_ssu_space_free("test"), UBS_SUCCESS);
}

// ============================================================================
// ubs_ssu_space_attach / ubs_ssu_space_detach
// ============================================================================

TEST_F(TestUbsEngineSsu, SpaceAttachWhenInvalidReq)
{
    char **paths = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_space_attach(nullptr, &paths, &cnt), UBS_ERR_NULL_POINTER);
    ubs_ssu_space_req_t req = {}; // name 为空
    EXPECT_EQ(ubs_ssu_space_attach(&req, &paths, &cnt), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsu, SpaceAttachWhenInvokeCallFailed)
{
    auto req = MakeSpaceReq();
    char **paths = nullptr;
    uint32_t cnt = 0;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_PERMISSION_DENIED));
    auto ret = ubs_ssu_space_attach(&req, &paths, &cnt);
    EXPECT_EQ(ret, UBS_ERR_PERMISSION_DENIED);
}

TEST_F(TestUbsEngineSsu, SpaceAttachWhenUnpackFailed)
{
    auto req = MakeSpaceReq();
    char **paths = nullptr;
    uint32_t cnt = 0;
    auto respBuffer = BuildInvalidResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_space_attach(&req, &paths, &cnt);
    EXPECT_NE(ret, UBS_SUCCESS);
}

TEST_F(TestUbsEngineSsu, SpaceAttachWhenSuccess)
{
    auto req = MakeSpaceReq();
    char **paths = nullptr;
    uint32_t cnt = 0;
    auto respBuffer = BuildNsDevPathsResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_space_attach(&req, &paths, &cnt);
    EXPECT_EQ(ret, UBS_SUCCESS);
    EXPECT_EQ(cnt, 2U);
    EXPECT_NE(paths, nullptr);
    ubs_ssu_ns_dev_paths_free(&paths, cnt);
    EXPECT_EQ(paths, nullptr);
}

TEST_F(TestUbsEngineSsu, SpaceDetachWhenInvalidReq)
{
    EXPECT_EQ(ubs_ssu_space_detach(nullptr), UBS_ERR_NULL_POINTER);
    ubs_ssu_space_req_t req = {}; // name 为空
    EXPECT_EQ(ubs_ssu_space_detach(&req), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsu, SpaceDetachWhenInvokeCallFailed)
{
    auto req = MakeSpaceReq();
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_TIMED_OUT));
    EXPECT_EQ(ubs_ssu_space_detach(&req), UBS_ERR_TIMED_OUT);
}

TEST_F(TestUbsEngineSsu, SpaceDetachWhenSuccess)
{
    auto req = MakeSpaceReq();
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubs_ssu_space_detach(&req), UBS_SUCCESS);
}

// ============================================================================
// ubs_ssu_linear_space_attach / ubs_ssu_linear_space_detach
// ============================================================================

TEST_F(TestUbsEngineSsu, LinearSpaceAttachWhenInvalidReq)
{
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    EXPECT_EQ(ubs_ssu_linear_space_attach(nullptr, &paths, &cnt, devPath), UBS_ERR_NULL_POINTER);
    ubs_ssu_linear_space_req_t req = {}; // name/dev_name 为空
    EXPECT_EQ(ubs_ssu_linear_space_attach(&req, &paths, &cnt, devPath), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsu, LinearSpaceAttachWhenInvokeCallFailed)
{
    auto req = MakeLinearSpaceReq();
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_ACCESS_DENIED));
    auto ret = ubs_ssu_linear_space_attach(&req, &paths, &cnt, devPath);
    EXPECT_EQ(ret, UBS_ERR_ACCESS_DENIED);
}

TEST_F(TestUbsEngineSsu, LinearSpaceAttachWhenSuccess)
{
    auto req = MakeLinearSpaceReq();
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    auto respBuffer = BuildLinearAttachResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_linear_space_attach(&req, &paths, &cnt, devPath);
    EXPECT_EQ(ret, UBS_SUCCESS);
    EXPECT_EQ(cnt, 2U);
    EXPECT_NE(paths, nullptr);
    EXPECT_GT(strlen(devPath), 0U);
    ubs_ssu_ns_dev_paths_free(&paths, cnt);
}

TEST_F(TestUbsEngineSsu, LinearSpaceDetachWhenInvalidReq)
{
    EXPECT_EQ(ubs_ssu_linear_space_detach(nullptr), UBS_ERR_NULL_POINTER);
    ubs_ssu_linear_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    // dev_name 为空
    EXPECT_EQ(ubs_ssu_linear_space_detach(&req), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsu, LinearSpaceDetachWhenSuccess)
{
    auto req = MakeLinearSpaceReq();
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubs_ssu_linear_space_detach(&req), UBS_SUCCESS);
}

// ============================================================================
// ubs_ssu_striped_space_attach / ubs_ssu_striped_space_detach
// ============================================================================

TEST_F(TestUbsEngineSsu, StripedSpaceAttachWhenInvalidReq)
{
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    EXPECT_EQ(ubs_ssu_striped_space_attach(nullptr, &paths, &cnt, devPath), UBS_ERR_NULL_POINTER);
    ubs_ssu_striped_space_req_t req = {}; // name 为空
    EXPECT_EQ(ubs_ssu_striped_space_attach(&req, &paths, &cnt, devPath), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsu, StripedSpaceAttachWhenInvalidLevel)
{
    auto req = MakeStripedSpaceReq();
    req.level = static_cast<ubs_ssu_raid_level_t>(99); // 非法 level
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    EXPECT_EQ(ubs_ssu_striped_space_attach(&req, &paths, &cnt, devPath), UBS_ERR_INVALID_ARG);
}

TEST_F(TestUbsEngineSsu, StripedSpaceAttachWhenInvokeCallFailed)
{
    auto req = MakeStripedSpaceReq();
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_AUTHENTICATION_FAILED));
    auto ret = ubs_ssu_striped_space_attach(&req, &paths, &cnt, devPath);
    EXPECT_EQ(ret, UBS_ERR_AUTHENTICATION_FAILED);
}

TEST_F(TestUbsEngineSsu, StripedSpaceAttachWhenSuccess)
{
    auto req = MakeStripedSpaceReq();
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    auto respBuffer = BuildLinearAttachResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_striped_space_attach(&req, &paths, &cnt, devPath);
    EXPECT_EQ(ret, UBS_SUCCESS);
    EXPECT_EQ(cnt, 2U);
    EXPECT_NE(paths, nullptr);
    EXPECT_GT(strlen(devPath), 0U);
    ubs_ssu_ns_dev_paths_free(&paths, cnt);
}

TEST_F(TestUbsEngineSsu, StripedSpaceDetachWhenInvalidReq)
{
    EXPECT_EQ(ubs_ssu_striped_space_detach(nullptr), UBS_ERR_NULL_POINTER);
    ubs_ssu_striped_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    // detach 不校验 level/chunk_size, 应成功通过校验
    req.level = static_cast<ubs_ssu_raid_level_t>(99);
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubs_ssu_striped_space_detach(&req), UBS_SUCCESS);
}

TEST_F(TestUbsEngineSsu, StripedSpaceDetachWhenSuccess)
{
    auto req = MakeStripedSpaceReq();
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubs_ssu_striped_space_detach(&req), UBS_SUCCESS);
}

// ============================================================================
// ubs_ssu_access_permission_add / ubs_ssu_access_permission_remove
// ============================================================================

TEST_F(TestUbsEngineSsu, AccessPermissionAddWhenInvalidParams)
{
    EXPECT_EQ(ubs_ssu_access_permission_add(nullptr, "nqn"), UBS_ERR_NULL_POINTER);
    EXPECT_EQ(ubs_ssu_access_permission_add("test", nullptr), UBS_ERR_NULL_POINTER);
    EXPECT_EQ(ubs_ssu_access_permission_add("", "nqn"), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsu, AccessPermissionAddWhenInvokeCallFailed)
{
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_NOT_EXIST));
    EXPECT_EQ(ubs_ssu_access_permission_add("test", "nqn.host"), UBSE_ERR_NOT_EXIST);
}

TEST_F(TestUbsEngineSsu, AccessPermissionAddWhenSuccess)
{
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubs_ssu_access_permission_add("test", "nqn.host"), UBS_SUCCESS);
}

TEST_F(TestUbsEngineSsu, AccessPermissionRemoveWhenInvalidParams)
{
    EXPECT_EQ(ubs_ssu_access_permission_remove(nullptr, "nqn"), UBS_ERR_NULL_POINTER);
    EXPECT_EQ(ubs_ssu_access_permission_remove("test", nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsu, AccessPermissionRemoveWhenInvokeCallFailed)
{
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_NOT_EXIST));
    EXPECT_EQ(ubs_ssu_access_permission_remove("test", "nqn.host"), UBSE_ERR_NOT_EXIST);
}

TEST_F(TestUbsEngineSsu, AccessPermissionRemoveWhenSuccess)
{
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubs_ssu_access_permission_remove("test", "nqn.host"), UBS_SUCCESS);
}

// ============================================================================
// ubs_ssu_fe_device_alloc / ubs_ssu_fe_device_free
// ============================================================================

TEST_F(TestUbsEngineSsu, FeDeviceAllocWhenInvalidParams)
{
    ubs_ub_vfe_t vfe = MakeVfe();
    uint8_t guid[UBS_SSU_GUID_LENGTH] = {};
    EXPECT_EQ(ubs_ssu_fe_device_alloc(1, nullptr, guid), UBS_ERR_NULL_POINTER);
    EXPECT_EQ(ubs_ssu_fe_device_alloc(1, &vfe, nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsu, FeDeviceAllocWhenInvokeCallFailed)
{
    ubs_ub_vfe_t vfe = MakeVfe();
    uint8_t guid[UBS_SSU_GUID_LENGTH] = {};
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_RESOURCE_EXHAUSTED));
    EXPECT_EQ(ubs_ssu_fe_device_alloc(1, &vfe, guid), UBS_ERR_RESOURCE_EXHAUSTED);
}

TEST_F(TestUbsEngineSsu, FeDeviceAllocWhenUnpackFailed)
{
    ubs_ub_vfe_t vfe = MakeVfe();
    uint8_t guid[UBS_SSU_GUID_LENGTH] = {};
    auto respBuffer = BuildInvalidResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_fe_device_alloc(1, &vfe, guid);
    EXPECT_NE(ret, UBS_SUCCESS);
}

TEST_F(TestUbsEngineSsu, FeDeviceAllocWhenSuccess)
{
    ubs_ub_vfe_t vfe = MakeVfe();
    uint8_t guid[UBS_SSU_GUID_LENGTH] = {};
    auto respBuffer = BuildFeDeviceAllocResponse();
    MOCKER(ubse_invoke_call).stubs().with(_, _, _, outBoundP(&respBuffer)).will(returnValue(UBSE_OK));
    auto ret = ubs_ssu_fe_device_alloc(1, &vfe, guid);
    EXPECT_EQ(ret, UBS_SUCCESS);
    EXPECT_EQ(guid[0], 0xDD);
    EXPECT_EQ(guid[31], 0xEE);
}

TEST_F(TestUbsEngineSsu, FeDeviceFreeWhenInvalidParams)
{
    EXPECT_EQ(ubs_ssu_fe_device_free(1, nullptr), UBS_ERR_NULL_POINTER);
    ubs_ub_vfe_t vfe = {}; // bind_bus_instance_guid 全 0
    EXPECT_EQ(ubs_ssu_fe_device_free(1, &vfe), UBS_ERR_INVALID_ARG);
}

TEST_F(TestUbsEngineSsu, FeDeviceFreeWhenInvokeCallFailed)
{
    ubs_ub_vfe_t vfe = MakeVfe();
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_QUOTA_EXCEEDED));
    EXPECT_EQ(ubs_ssu_fe_device_free(1, &vfe), UBS_ERR_QUOTA_EXCEEDED);
}

TEST_F(TestUbsEngineSsu, FeDeviceFreeWhenSuccess)
{
    ubs_ub_vfe_t vfe = MakeVfe();
    MOCKER(ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(ubs_ssu_fe_device_free(1, &vfe), UBS_SUCCESS);
}

// ============================================================================
// 释放接口测试
// ============================================================================

TEST_F(TestUbsEngineSsu, AllocInfoListFreeHandlesNullSafely)
{
    ubs_ssu_alloc_result_t *results = nullptr;
    ubs_ssu_alloc_info_list_free(nullptr, 0);
    ubs_ssu_alloc_info_list_free(&results, 0);
    EXPECT_EQ(results, nullptr);
}

TEST_F(TestUbsEngineSsu, NsDevPathsFreeHandlesNullSafely)
{
    char **paths = nullptr;
    ubs_ssu_ns_dev_paths_free(nullptr, 0);
    ubs_ssu_ns_dev_paths_free(&paths, 0);
    EXPECT_EQ(paths, nullptr);
}

TEST_F(TestUbsEngineSsu, NsStatsFreeHandlesNullSafely)
{
    ubs_ssu_ns_stats_t *stats = nullptr;
    ubs_ssu_ns_stats_free(nullptr);
    ubs_ssu_ns_stats_free(&stats);
    EXPECT_EQ(stats, nullptr);
}

TEST_F(TestUbsEngineSsu, ConnectInfoFreeHandlesNullSafely)
{
    ubs_ssu_connect_info_t *info = nullptr;
    ubs_ssu_connect_info_free(nullptr);
    ubs_ssu_connect_info_free(&info);
    EXPECT_EQ(info, nullptr);
}

TEST_F(TestUbsEngineSsu, FeDeviceListFreeHandlesNullSafely)
{
    ubs_ub_fe_t *feList = nullptr;
    ubs_ssu_fe_device_list_free(nullptr, 0);
    ubs_ssu_fe_device_list_free(&feList, 0);
    EXPECT_EQ(feList, nullptr);
}

TEST_F(TestUbsEngineSsu, AllocInfoFreeHandlesNullSafely)
{
    ubs_ssu_alloc_result_t *result = nullptr;
    ubs_ssu_alloc_info_free(nullptr);
    ubs_ssu_alloc_info_free(&result);
    EXPECT_EQ(result, nullptr);
}
} // namespace ubse::sdk::ut
