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
#include <string>
#include <gtest/gtest.h>

#include "ssu/ubs_ssu_validate.h"
#include "ubs_error.h"

namespace ubse::sdk::ut {
using namespace ubs::ssu;

// ==================== ubs_ssu_name_is_valid ====================

TEST(SsuNameValidate, ValidName_ReturnsSuccess)
{
    EXPECT_EQ(ubs_ssu_name_is_valid("test_name"), UBS_SUCCESS);
}

TEST(SsuNameValidate, ValidNameWithSpecialChars_ReturnsSuccess)
{
    EXPECT_EQ(ubs_ssu_name_is_valid("test.name-1:2"), UBS_SUCCESS);
}

TEST(SsuNameValidate, ValidNameMaxLength_ReturnsSuccess)
{
    std::string name(47, 'a'); // 47 chars (max valid)
    EXPECT_EQ(ubs_ssu_name_is_valid(name.c_str()), UBS_SUCCESS);
}

TEST(SsuNameValidate, NullName_ReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_name_is_valid(nullptr), UBS_ERR_NULL_POINTER);
}

TEST(SsuNameValidate, EmptyName_ReturnsOutOfRange)
{
    EXPECT_EQ(ubs_ssu_name_is_valid(""), UBS_ERR_OUT_OF_RANGE);
}

TEST(SsuNameValidate, TooLongName_ReturnsOutOfRange)
{
    std::string name(48, 'a'); // 48 chars (exceeds max 47)
    EXPECT_EQ(ubs_ssu_name_is_valid(name.c_str()), UBS_ERR_OUT_OF_RANGE);
}

TEST(SsuNameValidate, InvalidChar_ReturnsInvalidArg)
{
    EXPECT_EQ(ubs_ssu_name_is_valid("test@name"), UBS_ERR_INVALID_ARG);
    EXPECT_EQ(ubs_ssu_name_is_valid("test name"), UBS_ERR_INVALID_ARG);
    EXPECT_EQ(ubs_ssu_name_is_valid("test/name"), UBS_ERR_INVALID_ARG);
}

// ==================== ubs_ssu_alloc_space_req_validate ====================

TEST(SsuAllocSpaceReqValidate, ValidStriped_ReturnsSuccess)
{
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "test_striped", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.ns_size = 2ULL * 1024 * 1024 * 1024; // 2GB, divisible by ns_num=2
    req.ns_num = 2;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = UBS_SSU_ALLOC_STRATEGY_STRIPED;
    EXPECT_EQ(ubs_ssu_alloc_space_req_validate(&req), UBS_SUCCESS);
}

TEST(SsuAllocSpaceReqValidate, ValidLinear_ReturnsSuccess)
{
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "test_linear", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.ns_size = 1ULL * 1024 * 1024 * 1024; // 1GB
    req.ns_num = 1;
    req.lba_format = UBS_SSU_LBA_FORMAT_512;
    req.strategy = UBS_SSU_ALLOC_STRATEGY_LINEAR;
    EXPECT_EQ(ubs_ssu_alloc_space_req_validate(&req), UBS_SUCCESS);
}

TEST(SsuAllocSpaceReqValidate, NullReq_ReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_alloc_space_req_validate(nullptr), UBS_ERR_NULL_POINTER);
}

TEST(SsuAllocSpaceReqValidate, EmptyName_ReturnsOutOfRange)
{
    ubs_ssu_alloc_space_req_t req = {}; // name is empty
    req.ns_size = 1ULL * 1024 * 1024 * 1024;
    req.ns_num = 1;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = UBS_SSU_ALLOC_STRATEGY_LINEAR;
    EXPECT_EQ(ubs_ssu_alloc_space_req_validate(&req), UBS_ERR_OUT_OF_RANGE);
}

TEST(SsuAllocSpaceReqValidate, NsNumZero_ReturnsInvalidArg)
{
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.ns_size = 1ULL * 1024 * 1024 * 1024;
    req.ns_num = 0;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = UBS_SSU_ALLOC_STRATEGY_LINEAR;
    EXPECT_EQ(ubs_ssu_alloc_space_req_validate(&req), UBS_ERR_INVALID_ARG);
}

TEST(SsuAllocSpaceReqValidate, NsSizeZero_ReturnsInvalidArg)
{
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.ns_size = 0;
    req.ns_num = 1;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = UBS_SSU_ALLOC_STRATEGY_LINEAR;
    EXPECT_EQ(ubs_ssu_alloc_space_req_validate(&req), UBS_ERR_INVALID_ARG);
}

TEST(SsuAllocSpaceReqValidate, NsSizeNot1GMultiple_ReturnsInvalidArg)
{
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.ns_size = 500ULL * 1024 * 1024; // 500MB, not 1G multiple
    req.ns_num = 1;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = UBS_SSU_ALLOC_STRATEGY_LINEAR;
    EXPECT_EQ(ubs_ssu_alloc_space_req_validate(&req), UBS_ERR_INVALID_ARG);
}

TEST(SsuAllocSpaceReqValidate, StripedNotDivisible_ReturnsInvalidArg)
{
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.ns_size = 3ULL * 1024 * 1024 * 1024; // 3GB, not divisible by ns_num=2
    req.ns_num = 7;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = UBS_SSU_ALLOC_STRATEGY_STRIPED;
    EXPECT_EQ(ubs_ssu_alloc_space_req_validate(&req), UBS_ERR_INVALID_ARG);
}

TEST(SsuAllocSpaceReqValidate, InvalidLbaFormat_ReturnsInvalidArg)
{
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.ns_size = 1ULL * 1024 * 1024 * 1024;
    req.ns_num = 1;
    req.lba_format = static_cast<ubs_ssu_lba_format_t>(999);
    req.strategy = UBS_SSU_ALLOC_STRATEGY_LINEAR;
    EXPECT_EQ(ubs_ssu_alloc_space_req_validate(&req), UBS_ERR_INVALID_ARG);
}

TEST(SsuAllocSpaceReqValidate, InvalidStrategy_ReturnsInvalidArg)
{
    ubs_ssu_alloc_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    req.ns_size = 1ULL * 1024 * 1024 * 1024;
    req.ns_num = 1;
    req.lba_format = UBS_SSU_LBA_FORMAT_4K;
    req.strategy = static_cast<ubs_ssu_alloc_strategy_t>(99);
    EXPECT_EQ(ubs_ssu_alloc_space_req_validate(&req), UBS_ERR_INVALID_ARG);
}

// ==================== ubs_ssu_access_permission_add_req_validate ====================

TEST(SsuAccessPermissionAddValidate, Valid_ReturnsSuccess)
{
    EXPECT_EQ(ubs_ssu_access_permission_add_req_validate("test", "nqn.2024-01.com.huawei:uuid"), UBS_SUCCESS);
}

TEST(SsuAccessPermissionAddValidate, NullName_ReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_access_permission_add_req_validate(nullptr, "nqn"), UBS_ERR_NULL_POINTER);
}

TEST(SsuAccessPermissionAddValidate, NullNqn_ReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_access_permission_add_req_validate("test", nullptr), UBS_ERR_NULL_POINTER);
}

TEST(SsuAccessPermissionAddValidate, NameTooLong_ReturnsOutOfRange)
{
    std::string longName(48, 'a');
    EXPECT_EQ(ubs_ssu_access_permission_add_req_validate(longName.c_str(), "nqn"), UBS_ERR_OUT_OF_RANGE);
}

TEST(SsuAccessPermissionAddValidate, NqnTooLong_ReturnsOutOfRange)
{
    std::string longNqn(69, 'a');
    EXPECT_EQ(ubs_ssu_access_permission_add_req_validate("test", longNqn.c_str()), UBS_ERR_OUT_OF_RANGE);
}

// ==================== ubs_ssu_access_permission_remove_req_validate ====================

TEST(SsuAccessPermissionRemoveValidate, Valid_ReturnsSuccess)
{
    EXPECT_EQ(ubs_ssu_access_permission_remove_req_validate("test", "nqn.2024-01.com.huawei:uuid"), UBS_SUCCESS);
}

TEST(SsuAccessPermissionRemoveValidate, NullName_ReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_access_permission_remove_req_validate(nullptr, "nqn"), UBS_ERR_NULL_POINTER);
}

TEST(SsuAccessPermissionRemoveValidate, NullNqn_ReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_access_permission_remove_req_validate("test", nullptr), UBS_ERR_NULL_POINTER);
}

// ==================== ubs_ssu_space_attach_req_validate ====================

TEST(SsuSpaceAttachValidate, Valid_ReturnsSuccess)
{
    ubs_ssu_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    EXPECT_EQ(ubs_ssu_space_attach_req_validate(&req), UBS_SUCCESS);
}

TEST(SsuSpaceAttachValidate, ValidWithOptionalFields_ReturnsSuccess)
{
    ubs_ssu_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.nqn, "nqn.2024-01.com.huawei", UBS_SSU_MAX_NQN_LENGTH - 1);
    strncpy(req.src_eid, "eid123", UBS_SSU_MAX_EID_LENGTH - 1);
    EXPECT_EQ(ubs_ssu_space_attach_req_validate(&req), UBS_SUCCESS);
}

TEST(SsuSpaceAttachValidate, NullReq_ReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_space_attach_req_validate(nullptr), UBS_ERR_NULL_POINTER);
}

TEST(SsuSpaceAttachValidate, EmptyName_ReturnsOutOfRange)
{
    ubs_ssu_space_req_t req = {}; // name is empty
    EXPECT_EQ(ubs_ssu_space_attach_req_validate(&req), UBS_ERR_OUT_OF_RANGE);
}

// ==================== ubs_ssu_space_detach_req_validate ====================

TEST(SsuSpaceDetachValidate, Valid_ReturnsSuccess)
{
    ubs_ssu_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    EXPECT_EQ(ubs_ssu_space_detach_req_validate(&req), UBS_SUCCESS);
}

TEST(SsuSpaceDetachValidate, NullReq_ReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_space_detach_req_validate(nullptr), UBS_ERR_NULL_POINTER);
}

// ==================== ubs_ssu_linear_space_attach_req_validate ====================

TEST(SsuLinearSpaceAttachValidate, Valid_ReturnsSuccess)
{
    ubs_ssu_linear_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    EXPECT_EQ(ubs_ssu_linear_space_attach_req_validate(&req), UBS_SUCCESS);
}

TEST(SsuLinearSpaceAttachValidate, NullReq_ReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_linear_space_attach_req_validate(nullptr), UBS_ERR_NULL_POINTER);
}

TEST(SsuLinearSpaceAttachValidate, EmptyDevName_ReturnsOutOfRange)
{
    ubs_ssu_linear_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    // dev_name is empty
    EXPECT_EQ(ubs_ssu_linear_space_attach_req_validate(&req), UBS_ERR_OUT_OF_RANGE);
}

TEST(SsuLinearSpaceAttachValidate, InvalidDevNameChar_ReturnsInvalidArg)
{
    ubs_ssu_linear_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.dev_name, "dev@name", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    EXPECT_EQ(ubs_ssu_linear_space_attach_req_validate(&req), UBS_ERR_INVALID_ARG);
}

// ==================== ubs_ssu_linear_space_detach_req_validate ====================

TEST(SsuLinearSpaceDetachValidate, Valid_ReturnsSuccess)
{
    ubs_ssu_linear_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    EXPECT_EQ(ubs_ssu_linear_space_detach_req_validate(&req), UBS_SUCCESS);
}

TEST(SsuLinearSpaceDetachValidate, NullReq_ReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_linear_space_detach_req_validate(nullptr), UBS_ERR_NULL_POINTER);
}

// ==================== ubs_ssu_striped_space_attach_req_validate ====================

TEST(SsuStripedSpaceAttachValidate, ValidRaid0_ReturnsSuccess)
{
    ubs_ssu_striped_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    req.level = UBS_SSU_RAID0;
    req.chunk_size = UBS_SSU_CHUNK_SIZE_64K;
    EXPECT_EQ(ubs_ssu_striped_space_attach_req_validate(&req), UBS_SUCCESS);
}

TEST(SsuStripedSpaceAttachValidate, ValidRaid5_ReturnsSuccess)
{
    ubs_ssu_striped_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    req.level = UBS_SSU_RAID5;
    req.chunk_size = UBS_SSU_CHUNK_SIZE_256K;
    EXPECT_EQ(ubs_ssu_striped_space_attach_req_validate(&req), UBS_SUCCESS);
}

TEST(SsuStripedSpaceAttachValidate, NullReq_ReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_striped_space_attach_req_validate(nullptr), UBS_ERR_NULL_POINTER);
}

TEST(SsuStripedSpaceAttachValidate, InvalidLevel_ReturnsInvalidArg)
{
    ubs_ssu_striped_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    req.level = static_cast<ubs_ssu_raid_level_t>(99);
    req.chunk_size = UBS_SSU_CHUNK_SIZE_64K;
    EXPECT_EQ(ubs_ssu_striped_space_attach_req_validate(&req), UBS_ERR_INVALID_ARG);
}

TEST(SsuStripedSpaceAttachValidate, InvalidChunkSize_ReturnsInvalidArg)
{
    ubs_ssu_striped_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    req.level = UBS_SSU_RAID0;
    req.chunk_size = static_cast<ubs_ssu_chunk_size_t>(100);
    EXPECT_EQ(ubs_ssu_striped_space_attach_req_validate(&req), UBS_ERR_INVALID_ARG);
}

// ==================== ubs_ssu_striped_space_detach_req_validate ====================

TEST(SsuStripedSpaceDetachValidate, Valid_ReturnsSuccess)
{
    ubs_ssu_striped_space_req_t req = {};
    strncpy(req.name, "test", UBS_SSU_MAX_NAME_LENGTH - 1);
    strncpy(req.dev_name, "dev0", UBS_SSU_MAX_DEV_NAME_LENGTH - 1);
    // detach does not check level/chunk_size
    req.level = static_cast<ubs_ssu_raid_level_t>(99);
    req.chunk_size = static_cast<ubs_ssu_chunk_size_t>(100);
    EXPECT_EQ(ubs_ssu_striped_space_detach_req_validate(&req), UBS_SUCCESS);
}

TEST(SsuStripedSpaceDetachValidate, NullReq_ReturnsNullPointer)
{
    EXPECT_EQ(ubs_ssu_striped_space_detach_req_validate(nullptr), UBS_ERR_NULL_POINTER);
}

// ==================== ubs_ssu_fe_device_alloc_validate ====================

TEST(SsuFeDeviceAllocValidate, Valid_ReturnsSuccess)
{
    ubs_ub_vfe_t vfe = {};
    uint8_t guid[UBS_SSU_GUID_LENGTH] = {};
    EXPECT_EQ(ubs_ssu_fe_device_alloc_validate(&vfe, guid), UBS_SUCCESS);
}

TEST(SsuFeDeviceAllocValidate, NullVfe_ReturnsNullPointer)
{
    uint8_t guid[UBS_SSU_GUID_LENGTH] = {};
    EXPECT_EQ(ubs_ssu_fe_device_alloc_validate(nullptr, guid), UBS_ERR_NULL_POINTER);
}

TEST(SsuFeDeviceAllocValidate, NullGuid_ReturnsNullPointer)
{
    ubs_ub_vfe_t vfe = {};
    EXPECT_EQ(ubs_ssu_fe_device_alloc_validate(&vfe, nullptr), UBS_ERR_NULL_POINTER);
}

// ==================== ubs_ssu_fe_device_free_validate ====================

TEST(SsuFeDeviceFreeValidate, Valid_ReturnsSuccess)
{
    ubs_ub_vfe_t vfe = {};
    vfe.bind_bus_instance_guid[0] = 1; // non-zero guid
    EXPECT_EQ(ubs_ssu_fe_device_free_validate(&vfe), UBS_SUCCESS);
}

TEST(SsuFeDeviceFreeValidate, NullVfe_ReturnsInvalidArg)
{
    EXPECT_EQ(ubs_ssu_fe_device_free_validate(nullptr), UBS_ERR_NULL_POINTER);
}

TEST(SsuFeDeviceFreeValidate, AllZeroGuid_ReturnsInvalidArg)
{
    ubs_ub_vfe_t vfe = {}; // bind_bus_instance_guid is all zeros
    EXPECT_EQ(ubs_ssu_fe_device_free_validate(&vfe), UBS_ERR_INVALID_ARG);
}

} // namespace ubse::sdk::ut
