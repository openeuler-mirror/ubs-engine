/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <cstring>
#include <string>
#include <gtest/gtest.h>

#include <securec.h>
#include <mockcpp/mockcpp.hpp>

#include "ubs_engine_ssu.h"
#include "ubs_error.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"

namespace ubse::sdk::ut {

// ============================================================================
// 通过 ubs_engine_ssu.h 的对外 API 接口触发参数校验 (模拟真实调用入口)
// 校验失败时, API 在 IPC 调用前直接返回错误码, 不会走到 ubse_invoke_call,
// 因此校验失败用例无需 mock ubse_invoke_call。
// ============================================================================

class TestUbsEngineSsuApiValidate : public testing::Test {
public:
    void TearDown() override { GlobalMockObject::verify(); }
};

// ==================== ubs_ssu_alloc_info_list ====================

TEST_F(TestUbsEngineSsuApiValidate, AllocInfoListNullResultsReturnsNullPointer)
{
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_alloc_info_list(nullptr, &cnt), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, AllocInfoListNullCntReturnsNullPointer)
{
    ubs_ssu_alloc_result_t *results = nullptr;
    EXPECT_EQ(ubs_ssu_alloc_info_list(&results, nullptr), UBS_ERR_NULL_POINTER);
}

// ==================== ubs_ssu_ns_stats_get ====================

TEST_F(TestUbsEngineSsuApiValidate, NsStatsGetNullNameReturnsNullPointer)
{
    ubs_ssu_ns_stats_t *stats = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_ns_stats_get(nullptr, &stats, &cnt), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, NsStatsGetEmptyNameReturnsOutOfRange)
{
    ubs_ssu_ns_stats_t *stats = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_ns_stats_get("", &stats, &cnt), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, NsStatsGetTooLongNameReturnsOutOfRange)
{
    ubs_ssu_ns_stats_t *stats = nullptr;
    uint32_t cnt = 0;
    std::string name(48, 'a');
    EXPECT_EQ(ubs_ssu_ns_stats_get(name.c_str(), &stats, &cnt), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, NsStatsGetInvalidCharNameReturnsInvalidArg)
{
    ubs_ssu_ns_stats_t *stats = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_ns_stats_get("test@name", &stats, &cnt), UBS_ERR_INVALID_ARG);
}

TEST_F(TestUbsEngineSsuApiValidate, NsStatsGetNullStatsReturnsNullPointer)
{
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_ns_stats_get("test", nullptr, &cnt), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, NsStatsGetNullCntReturnsNullPointer)
{
    ubs_ssu_ns_stats_t *stats = nullptr;
    EXPECT_EQ(ubs_ssu_ns_stats_get("test", &stats, nullptr), UBS_ERR_NULL_POINTER);
}

// ==================== ubs_ssu_connect_info_get ====================

TEST_F(TestUbsEngineSsuApiValidate, ConnectInfoGetNullNameReturnsNullPointer)
{
    ubs_ssu_connect_info_t *info = nullptr;
    uint32_t cnt = 0;
    ubs_ub_vfe_t vfe = {};
    EXPECT_EQ(ubs_ssu_connect_info_get(nullptr, &vfe, &info, &cnt), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, ConnectInfoGetEmptyNameReturnsOutOfRange)
{
    ubs_ssu_connect_info_t *info = nullptr;
    uint32_t cnt = 0;
    ubs_ub_vfe_t vfe = {};
    EXPECT_EQ(ubs_ssu_connect_info_get("", &vfe, &info, &cnt), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, ConnectInfoGetTooLongNameReturnsOutOfRange)
{
    ubs_ssu_connect_info_t *info = nullptr;
    uint32_t cnt = 0;
    ubs_ub_vfe_t vfe = {};
    std::string name(48, 'a');
    EXPECT_EQ(ubs_ssu_connect_info_get(name.c_str(), &vfe, &info, &cnt), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, ConnectInfoGetNullListReturnsNullPointer)
{
    uint32_t cnt = 0;
    ubs_ub_vfe_t vfe = {};
    EXPECT_EQ(ubs_ssu_connect_info_get("test", &vfe, nullptr, &cnt), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, ConnectInfoGetNullCntReturnsNullPointer)
{
    ubs_ssu_connect_info_t *info = nullptr;
    ubs_ub_vfe_t vfe = {};
    EXPECT_EQ(ubs_ssu_connect_info_get("test", &vfe, &info, nullptr), UBS_ERR_NULL_POINTER);
}

// ==================== ubs_ssu_fe_device_list ====================

TEST_F(TestUbsEngineSsuApiValidate, FeDeviceListNullListReturnsNullPointer)
{
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_fe_device_list(nullptr, &cnt), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, FeDeviceListNullCntReturnsNullPointer)
{
    ubs_ub_fe_t *feList = nullptr;
    EXPECT_EQ(ubs_ssu_fe_device_list(&feList, nullptr), UBS_ERR_NULL_POINTER);
}

// ==================== ubs_ssu_space_alloc ====================

TEST_F(TestUbsEngineSsuApiValidate, SpaceAllocNullReqReturnsNullPointer)
{
    ubs_ssu_alloc_result_t *result = nullptr;
    EXPECT_EQ(ubs_ssu_space_alloc(nullptr, &result), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceAllocNullResultReturnsNullPointer)
{
    ubs_ssu_alloc_space_req_t req = {};
    EXPECT_EQ(ubs_ssu_space_alloc(&req, nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceAllocEmptyNameReturnsOutOfRange)
{
    ubs_ssu_alloc_result_t *result = nullptr;
    ubs_ssu_alloc_space_req_t req = {};
    req.ns_size = 1ULL * 1024 * 1024 * 1024;
    req.ns_num = 1;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = UBS_SSU_ALLOC_STRATEGY_LINEAR;
    EXPECT_EQ(ubs_ssu_space_alloc(&req, &result), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceAllocInvalidCharNameReturnsInvalidArg)
{
    ubs_ssu_alloc_result_t *result = nullptr;
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "test@name", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.ns_size = 1ULL * 1024 * 1024 * 1024;
    req.ns_num = 1;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = UBS_SSU_ALLOC_STRATEGY_LINEAR;
    EXPECT_EQ(ubs_ssu_space_alloc(&req, &result), UBS_ERR_INVALID_ARG);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceAllocNsNumZeroReturnsInvalidArg)
{
    ubs_ssu_alloc_result_t *result = nullptr;
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.ns_size = 1ULL * 1024 * 1024 * 1024;
    req.ns_num = 0;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = UBS_SSU_ALLOC_STRATEGY_LINEAR;
    EXPECT_EQ(ubs_ssu_space_alloc(&req, &result), UBS_ERR_INVALID_ARG);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceAllocNsSizeZeroReturnsInvalidArg)
{
    ubs_ssu_alloc_result_t *result = nullptr;
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.ns_size = 0;
    req.ns_num = 1;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = UBS_SSU_ALLOC_STRATEGY_LINEAR;
    EXPECT_EQ(ubs_ssu_space_alloc(&req, &result), UBS_ERR_INVALID_ARG);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceAllocNsSizeNot1GMultipleReturnsInvalidArg)
{
    ubs_ssu_alloc_result_t *result = nullptr;
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.ns_size = 500ULL * 1024 * 1024; // 500MB, not 1G multiple
    req.ns_num = 1;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = UBS_SSU_ALLOC_STRATEGY_LINEAR;
    EXPECT_EQ(ubs_ssu_space_alloc(&req, &result), UBS_ERR_INVALID_ARG);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceAllocStripedNotDivisibleReturnsInvalidArg)
{
    ubs_ssu_alloc_result_t *result = nullptr;
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.ns_size = 3ULL * 1024 * 1024 * 1024; // 3GB, not divisible by ns_num=2
    req.ns_num = 7;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = UBS_SSU_ALLOC_STRATEGY_STRIPED;
    EXPECT_EQ(ubs_ssu_space_alloc(&req, &result), UBS_ERR_INVALID_ARG);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceAllocInvalidLbaFormatReturnsInvalidArg)
{
    ubs_ssu_alloc_result_t *result = nullptr;
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.ns_size = 1ULL * 1024 * 1024 * 1024;
    req.ns_num = 1;
    req.lba_format = static_cast<ubs_ssu_lba_format_t>(999);
    req.strategy = UBS_SSU_ALLOC_STRATEGY_LINEAR;
    EXPECT_EQ(ubs_ssu_space_alloc(&req, &result), UBS_ERR_INVALID_ARG);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceAllocInvalidStrategyReturnsInvalidArg)
{
    ubs_ssu_alloc_result_t *result = nullptr;
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.ns_size = 1ULL * 1024 * 1024 * 1024;
    req.ns_num = 1;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = static_cast<ubs_ssu_alloc_strategy_t>(99);
    EXPECT_EQ(ubs_ssu_space_alloc(&req, &result), UBS_ERR_INVALID_ARG);
}

// ==================== ubs_ssu_space_free ====================

TEST_F(TestUbsEngineSsuApiValidate, SpaceFreeNullNameReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_space_free(nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceFreeEmptyNameReturnsOutOfRange)
{
    EXPECT_EQ(ubs_ssu_space_free(""), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceFreeTooLongNameReturnsOutOfRange)
{
    std::string name(48, 'a');
    EXPECT_EQ(ubs_ssu_space_free(name.c_str()), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceFreeInvalidCharNameReturnsInvalidArg)
{
    EXPECT_EQ(ubs_ssu_space_free("test@name"), UBS_ERR_INVALID_ARG);
}

// ==================== ubs_ssu_space_attach / detach ====================

TEST_F(TestUbsEngineSsuApiValidate, SpaceAttachNullReqReturnsNullPointer)
{
    char **paths = nullptr;
    uint32_t cnt = 0;
    EXPECT_EQ(ubs_ssu_space_attach(nullptr, &paths, &cnt), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceAttachEmptyNameReturnsOutOfRange)
{
    char **paths = nullptr;
    uint32_t cnt = 0;
    ubs_ssu_space_req_t req = {};
    EXPECT_EQ(ubs_ssu_space_attach(&req, &paths, &cnt), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceAttachInvalidCharNameReturnsInvalidArg)
{
    char **paths = nullptr;
    uint32_t cnt = 0;
    ubs_ssu_space_req_t req = {};
    strncpy(req.name, "test@name", UBS_SSU_MAX_NAME_LENGTH - 1);
    EXPECT_EQ(ubs_ssu_space_attach(&req, &paths, &cnt), UBS_ERR_INVALID_ARG);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceDetachNullReqReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_space_detach(nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, SpaceDetachEmptyNameReturnsOutOfRange)
{
    ubs_ssu_space_req_t req = {};
    EXPECT_EQ(ubs_ssu_space_detach(&req), UBS_ERR_OUT_OF_RANGE);
}

// ==================== ubs_ssu_linear_space_attach / detach ====================

TEST_F(TestUbsEngineSsuApiValidate, LinearSpaceAttachNullReqReturnsNullPointer)
{
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    EXPECT_EQ(ubs_ssu_linear_space_attach(nullptr, &paths, &cnt, devPath), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, LinearSpaceAttachEmptyNameReturnsOutOfRange)
{
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    ubs_ssu_linear_space_req_t req = {};
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    EXPECT_EQ(ubs_ssu_linear_space_attach(&req, &paths, &cnt, devPath), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, LinearSpaceAttachEmptyDevNameReturnsOutOfRange)
{
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    ubs_ssu_linear_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    // dev_name is empty
    EXPECT_EQ(ubs_ssu_linear_space_attach(&req, &paths, &cnt, devPath), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, LinearSpaceAttachInvalidDevNameCharReturnsInvalidArg)
{
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    ubs_ssu_linear_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.dev_name, "dev@name", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    EXPECT_EQ(ubs_ssu_linear_space_attach(&req, &paths, &cnt, devPath), UBS_ERR_INVALID_ARG);
}

TEST_F(TestUbsEngineSsuApiValidate, LinearSpaceDetachNullReqReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_linear_space_detach(nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, LinearSpaceDetachEmptyNameReturnsOutOfRange)
{
    ubs_ssu_linear_space_req_t req = {};
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    EXPECT_EQ(ubs_ssu_linear_space_detach(&req), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, LinearSpaceDetachEmptyDevNameReturnsOutOfRange)
{
    ubs_ssu_linear_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    EXPECT_EQ(ubs_ssu_linear_space_detach(&req), UBS_ERR_OUT_OF_RANGE);
}

// ==================== ubs_ssu_striped_space_attach / detach ====================

TEST_F(TestUbsEngineSsuApiValidate, StripedSpaceAttachNullReqReturnsNullPointer)
{
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    EXPECT_EQ(ubs_ssu_striped_space_attach(nullptr, &paths, &cnt, devPath), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, StripedSpaceAttachEmptyNameReturnsOutOfRange)
{
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    ubs_ssu_striped_space_req_t req = {};
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    req.level = UBS_SSU_RAID0;
    req.chunk_size = UBS_SSU_CHUNK_SIZE_64K;
    EXPECT_EQ(ubs_ssu_striped_space_attach(&req, &paths, &cnt, devPath), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, StripedSpaceAttachEmptyDevNameReturnsOutOfRange)
{
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    ubs_ssu_striped_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.level = UBS_SSU_RAID0;
    req.chunk_size = UBS_SSU_CHUNK_SIZE_64K;
    EXPECT_EQ(ubs_ssu_striped_space_attach(&req, &paths, &cnt, devPath), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, StripedSpaceAttachInvalidLevelReturnsInvalidArg)
{
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    ubs_ssu_striped_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    req.level = static_cast<ubs_ssu_raid_level_t>(99);
    req.chunk_size = UBS_SSU_CHUNK_SIZE_64K;
    EXPECT_EQ(ubs_ssu_striped_space_attach(&req, &paths, &cnt, devPath), UBS_ERR_INVALID_ARG);
}

TEST_F(TestUbsEngineSsuApiValidate, StripedSpaceAttachInvalidChunkSizeReturnsInvalidArg)
{
    char **paths = nullptr;
    uint32_t cnt = 0;
    char devPath[UBS_SSU_MAX_DEV_PATH_LENGTH] = {};
    ubs_ssu_striped_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    req.level = UBS_SSU_RAID0;
    req.chunk_size = static_cast<ubs_ssu_chunk_size_t>(100);
    EXPECT_EQ(ubs_ssu_striped_space_attach(&req, &paths, &cnt, devPath), UBS_ERR_INVALID_ARG);
}

TEST_F(TestUbsEngineSsuApiValidate, StripedSpaceDetachNullReqReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_striped_space_detach(nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, StripedSpaceDetachEmptyNameReturnsOutOfRange)
{
    ubs_ssu_striped_space_req_t req = {};
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    EXPECT_EQ(ubs_ssu_striped_space_detach(&req), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, StripedSpaceDetachEmptyDevNameReturnsOutOfRange)
{
    ubs_ssu_striped_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    EXPECT_EQ(ubs_ssu_striped_space_detach(&req), UBS_ERR_OUT_OF_RANGE);
}

// ==================== ubs_ssu_access_permission_add / remove ====================

TEST_F(TestUbsEngineSsuApiValidate, AccessPermissionAddNullNameReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_access_permission_add(nullptr, "nqn"), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, AccessPermissionAddNullNqnReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_access_permission_add("test", nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, AccessPermissionAddEmptyNameReturnsOutOfRange)
{
    EXPECT_EQ(ubs_ssu_access_permission_add("", "nqn"), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, AccessPermissionAddTooLongNameReturnsOutOfRange)
{
    std::string longName(48, 'a');
    EXPECT_EQ(ubs_ssu_access_permission_add(longName.c_str(), "nqn"), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, AccessPermissionAddTooLongNqnReturnsOutOfRange)
{
    std::string longNqn(69, 'a');
    EXPECT_EQ(ubs_ssu_access_permission_add("test", longNqn.c_str()), UBS_ERR_OUT_OF_RANGE);
}

TEST_F(TestUbsEngineSsuApiValidate, AccessPermissionRemoveNullNameReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_access_permission_remove(nullptr, "nqn"), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, AccessPermissionRemoveNullNqnReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_access_permission_remove("test", nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, AccessPermissionRemoveEmptyNameReturnsOutOfRange)
{
    EXPECT_EQ(ubs_ssu_access_permission_remove("", "nqn"), UBS_ERR_OUT_OF_RANGE);
}

// ==================== ubs_ssu_fe_device_alloc / free ====================

TEST_F(TestUbsEngineSsuApiValidate, FeDeviceAllocNullVfeReturnsNullPointer)
{
    uint8_t guid[UBS_SSU_GUID_LENGTH] = {};
    EXPECT_EQ(ubs_ssu_fe_device_alloc(1, nullptr, guid), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, FeDeviceAllocNullGuidReturnsNullPointer)
{
    ubs_ub_vfe_t vfe = {};
    EXPECT_EQ(ubs_ssu_fe_device_alloc(1, &vfe, nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, FeDeviceFreeNullVfeReturnsInvalidArg)
{
    EXPECT_EQ(ubs_ssu_fe_device_free(1, nullptr), UBS_ERR_NULL_POINTER);
}

TEST_F(TestUbsEngineSsuApiValidate, FeDeviceFreeAllZeroGuidReturnsInvalidArg)
{
    ubs_ub_vfe_t vfe = {}; // bind_bus_instance_guid all zeros
    EXPECT_EQ(ubs_ssu_fe_device_free(1, &vfe), UBS_ERR_INVALID_ARG);
}

} // namespace ubse::sdk::ut
