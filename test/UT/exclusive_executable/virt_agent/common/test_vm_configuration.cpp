/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "test_vm_configuration.h"
#include "ubse_conf.h"
#include "vm_configuration.h"

using namespace vm;
using namespace ubse::config;
namespace ubse::vm::ut {
TestVmConfiguration::TestVmConfiguration() = default;

void TestVmConfiguration::SetUp()
{
    Test::SetUp();
}

void TestVmConfiguration::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestVmConfiguration, CheckRangeUint)
{
    VmConfigRange<uint32_t> vmConfigRange = {
        .defaultValue = 3,
        .valueRange = { 1, 60 }
    };
    std::string fileName = "conf file";
    std::string confName = "testConf";
    uint32_t val;

    VmConfiguration::GetConfigFunc<uint32_t> getFuncPtr = [](const std::string &file, const std::string &conf,
        uint32_t &_) -> uint32_t { return VM_ERROR; };
    VmConfiguration::GetInstance().GetConfigWithCheckRange(getFuncPtr, vmConfigRange, fileName, confName, val);
    EXPECT_EQ(vmConfigRange.defaultValue, val);

    val = 0; // 0 小于取值范围
    getFuncPtr = [](const std::string &file, const std::string &conf, uint32_t &_) -> uint32_t { return VM_OK; };
    VmConfiguration::GetInstance().GetConfigWithCheckRange(getFuncPtr, vmConfigRange, fileName, confName, val);
    EXPECT_EQ(vmConfigRange.defaultValue, val);

    val = 61; // 61 大于取值范围
    VmConfiguration::GetInstance().GetConfigWithCheckRange(getFuncPtr, vmConfigRange, fileName, confName, val);
    EXPECT_EQ(vmConfigRange.defaultValue, val);

    val = 5; // 5 在取值范围里
    uint32_t expect = 5;
    VmConfiguration::GetInstance().GetConfigWithCheckRange(getFuncPtr, vmConfigRange, fileName, confName, val);
    EXPECT_EQ(expect, val);
}

TEST_F(TestVmConfiguration, CheckRangeULong)
{
    VmConfigRange<uint64_t> vmConfigRange = {
        .defaultValue = 30,
        .valueRange = { 20, 300 }
    };
    std::string fileName = "conf file";
    std::string confName = "testConf";
    uint64_t val;

    VmConfiguration::GetConfigFunc<uint64_t> getFuncPtr = [](const std::string &file, const std::string &conf,
        uint64_t &_) -> uint32_t { return VM_ERROR; };
    VmConfiguration::GetInstance().GetConfigWithCheckRange(getFuncPtr, vmConfigRange, fileName, confName, val);
    EXPECT_EQ(vmConfigRange.defaultValue, val);

    val = 10; // 10 小于取值范围
    getFuncPtr = [](const std::string &file, const std::string &conf, uint64_t &_) -> uint32_t { return VM_OK; };
    VmConfiguration::GetInstance().GetConfigWithCheckRange(getFuncPtr, vmConfigRange, fileName, confName, val);
    EXPECT_EQ(vmConfigRange.defaultValue, val);

    val = 301; // 301 大于取值范围
    VmConfiguration::GetInstance().GetConfigWithCheckRange(getFuncPtr, vmConfigRange, fileName, confName, val);
    EXPECT_EQ(vmConfigRange.defaultValue, val);

    val = 200; // 200 在取值范围里
    uint64_t expect = 200;
    VmConfiguration::GetInstance().GetConfigWithCheckRange(getFuncPtr, vmConfigRange, fileName, confName, val);
    EXPECT_EQ(expect, val);
}

TEST_F(TestVmConfiguration, CheckRangeFloat)
{
    VmConfigRange<float_t> vmConfigRange = {
        .defaultValue = 2.0,
        .valueRange = { 2.0, 5.0 }
    };
    std::string fileName = "conf file";
    std::string confName = "testConf";
    float_t val;

    VmConfiguration::GetConfigFunc<float_t> getFuncPtr = [](const std::string &file, const std::string &conf,
        float_t &_) -> uint32_t { return VM_ERROR; };
    VmConfiguration::GetInstance().GetConfigWithCheckRange(getFuncPtr, vmConfigRange, fileName, confName, val);
    EXPECT_EQ(vmConfigRange.defaultValue, val);

    val = 1.5; // 1.5 小于取值范围
    getFuncPtr = [](const std::string &file, const std::string &conf, float_t &_) -> uint32_t { return VM_OK; };
    VmConfiguration::GetInstance().GetConfigWithCheckRange(getFuncPtr, vmConfigRange, fileName, confName, val);
    EXPECT_EQ(vmConfigRange.defaultValue, val);

    val = 5.5; // 5.5 大于取值范围
    VmConfiguration::GetInstance().GetConfigWithCheckRange(getFuncPtr, vmConfigRange, fileName, confName, val);
    EXPECT_EQ(vmConfigRange.defaultValue, val);

    val = 3.0; // 3.0 在取值范围里
    float_t expect = 3.0;
    VmConfiguration::GetInstance().GetConfigWithCheckRange(getFuncPtr, vmConfigRange, fileName, confName, val);
    EXPECT_EQ(expect, val);
}

TEST_F(TestVmConfiguration, CheckEnumULong)
{
    VmConfigEnum<uint64_t> vmConfigEnum = {
        .defaultValue = 2,
        .valueEnum = { 2, 3, 4, 5, 7 }
    };
    std::string fileName = "conf file";
    std::string confName = "testConf";
    uint64_t val;

    VmConfiguration::GetConfigFunc<uint64_t> getFuncPtr = [](const std::string &file, const std::string &conf,
        uint64_t &_) -> uint32_t { return VM_ERROR; };
    VmConfiguration::GetInstance().GetConfigWithCheckEnum(getFuncPtr, vmConfigEnum, fileName, confName, val);
    EXPECT_EQ(vmConfigEnum.defaultValue, val);

    val = 1; // 1 小于取值范围
    getFuncPtr = [](const std::string &file, const std::string &conf, uint64_t &_) -> uint32_t { return VM_OK; };
    VmConfiguration::GetInstance().GetConfigWithCheckEnum(getFuncPtr, vmConfigEnum, fileName, confName, val);
    EXPECT_EQ(vmConfigEnum.defaultValue, val);

    val = 8; // 8 大于取值范围
    VmConfiguration::GetInstance().GetConfigWithCheckEnum(getFuncPtr, vmConfigEnum, fileName, confName, val);
    EXPECT_EQ(vmConfigEnum.defaultValue, val);

    val = 3; // 3 在取值范围里
    uint64_t expect = 3;
    VmConfiguration::GetInstance().GetConfigWithCheckEnum(getFuncPtr, vmConfigEnum, fileName, confName, val);
    EXPECT_EQ(expect, val);
}

TEST_F(TestVmConfiguration, LoadConfig)
{
    MOCKER(&UbseGetUInt).stubs().will(returnValue(VM_ERROR));
    MOCKER(&UbseGetFloat).stubs().will(returnValue(VM_ERROR));
    MOCKER(&UbseGetULong).stubs().will(returnValue(VM_ERROR));

    auto& vmConfig = VmConfiguration::GetInstance();
    auto ret = vmConfig.LoadConfig();
    EXPECT_EQ(ret, VM_OK);
    EXPECT_EQ(EXPORT_INTERVAL.defaultValue, vmConfig.GetExportInterval());
    EXPECT_EQ(MAX_PER_TOTAL_MEMBORROW_SIZE.defaultValue * MB_TO_BYTES, vmConfig.GetMaxPerTotalMemBorrowBytes());
    EXPECT_EQ(MAX_MEM_PERBORROW_MB.defaultValue * MB_TO_BYTES, vmConfig.GetMaxMemPerBorrowBytes());
    EXPECT_EQ(MIN_MEM_PERBORROW_MB.defaultValue * MB_TO_BYTES, vmConfig.GetMinMemPerBorrowBytes());

    MOCKER(&UbseGetUInt).reset();
    MOCKER(&UbseGetFloat).reset();
    MOCKER(&UbseGetULong).reset();
}

TEST_F(TestVmConfiguration, CheckWaterConfRange)
{
    float_t borrowWater = 92;  // 92 符合取值范围
    float_t migrateWater = 85; // 85 符合取值范围
    float_t returnWater = 80;  // 80 符合取值范围
    auto& vmConfig = VmConfiguration::GetInstance();
    EXPECT_EQ(vmConfig.CheckWaterConfRange(borrowWater, migrateWater, returnWater), VM_OK);
    // borrowWater exceeds range
    borrowWater = 69.99; // 69.99 小于取值范围
    EXPECT_EQ(vmConfig.CheckWaterConfRange(borrowWater, migrateWater, returnWater), VM_ERROR);
    borrowWater = 95.01; // 95.01大于取值范围
    EXPECT_EQ(vmConfig.CheckWaterConfRange(borrowWater, migrateWater, returnWater), VM_ERROR);
    // migrateWater exceeds range
    borrowWater = 92;     // 92 符合取值范围
    migrateWater = 64.99; // 64.99 小于取值范围
    EXPECT_EQ(vmConfig.CheckWaterConfRange(borrowWater, migrateWater, returnWater), VM_ERROR);
    migrateWater = 90.01; // 90.01 大于取值范围
    EXPECT_EQ(vmConfig.CheckWaterConfRange(borrowWater, migrateWater, returnWater), VM_ERROR);
    // returnWater exceeds range
    migrateWater = 85;   // 85 符合取值范围
    returnWater = 59.99; // 59.99 小于取值范围
    EXPECT_EQ(vmConfig.CheckWaterConfRange(borrowWater, migrateWater, returnWater), VM_ERROR);
    returnWater = 80.01; // 80.01 大于取值范围
    EXPECT_EQ(vmConfig.CheckWaterConfRange(borrowWater, migrateWater, returnWater), VM_ERROR);
    // 88 84 80 不满足三条水线差值至少为5的要求
    borrowWater = 88;  // 88 84 80 不满足三条水线差值至少为5的要求
    migrateWater = 84; // 88 84 80 不满足三条水线差值至少为5的要求
    returnWater = 80;  // 88 84 80 不满足三条水线差值至少为5的要求
    EXPECT_EQ(vmConfig.CheckWaterConfRange(borrowWater, migrateWater, returnWater), VM_ERROR);
}

uint64_t g_oneKb = 1024;

float default_MaxMemBorrow = 0.25;
uint64_t default_MaxPerTotalMemBorrowBytes = 16384;
TEST_F(TestVmConfiguration, InitMaxBorrow_Test)
{
    GTEST_SKIP();
    auto& vmConfig = VmConfiguration::GetInstance();
    vmConfig.InitMaxBorrow();
    EXPECT_EQ(vmConfig.GetMaxMemBorrow(), default_MaxMemBorrow);
    EXPECT_EQ(vmConfig.GetMaxPerTotalMemBorrowBytes(), default_MaxPerTotalMemBorrowBytes * MB_TO_BYTES);
}

uint64_t g_twoKb = 2048;
TEST_F(TestVmConfiguration, CheckConfigValidity_Test)
{
    auto& vmConfig = VmConfiguration::GetInstance();
    // 正常情况
    vmConfig.minMemPerBorrowSize = g_oneKb;
    vmConfig.maxMemPerBorrowSize = g_twoKb;
    vmConfig.CheckConfigValidity();
    EXPECT_EQ(vmConfig.GetMinMemPerBorrowBytes(), g_oneKb * MB_TO_BYTES);
    EXPECT_EQ(vmConfig.GetMaxMemPerBorrowBytes(), g_twoKb * MB_TO_BYTES);
    // minVMMemory大于maxVMMemory
    vmConfig.minMemPerBorrowSize = g_twoKb;
    vmConfig.maxMemPerBorrowSize = g_oneKb;
    vmConfig.CheckConfigValidity();
    EXPECT_EQ(vmConfig.GetMinMemPerBorrowBytes(), MIN_MEM_PERBORROW_MB.defaultValue * MB_TO_BYTES);
    EXPECT_EQ(vmConfig.GetMaxMemPerBorrowBytes(), MAX_MEM_PERBORROW_MB.defaultValue * MB_TO_BYTES);
}

TEST_F(TestVmConfiguration, LoadWatermarkConf_Test)
{
    auto& vmConfig = VmConfiguration::GetInstance();
    EXPECT_EQ(vmConfig.LoadWatermarkConf(), VM_OK);
}

TEST_F(TestVmConfiguration, GetFunction_Test)
{
    auto& vmConfig = VmConfiguration::GetInstance();
    // GetEscapeAlgorithmDir
    EXPECT_EQ(vmConfig.GetEscapeAlgorithmDir(), DEFAULT_ESCAPE_ALGORITHM_DIR);
    // GetBorrowWatermark
    EXPECT_EQ(vmConfig.GetBorrowWatermark(), DEFAULT_BORROW_WATER_MARK);
}
} // namespace ubse::vm::ut