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

#include "test_case_conf.h"
#include <thread>
#include "mempooling_module.h"
#include "mockcpp/mockcpp.hpp"
#include "ubse_com.h"
#include "ubse_storage.h"
#include "securec.h"
#include "router.h"
#include "rack_vm_plugin.h"
#include "vm_json_util.h"
#include "case_conf.h"
#include "vm_configuration.h"
#include "vm_string_util.h"

using namespace vm;
using namespace vm::mempooling;
using namespace ubse::com;
using namespace ubse::storage;
namespace ubse::ut::vm {

// 设置测试环境
void TestCaseConf::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestCaseConf::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

VmResult MockQueryCaseAndOverCommitmentRatio(CaseAndOvercommitmentRatio &caseConf)
{
    caseConf.curCase = OVER_COMMITMENT_CASE;
    caseConf.overCommitmentRatio = MAX_OVER_COMMITMENT_RATIO;
    return VM_OK;
}

TEST_F(TestCaseConf, InitTest)
{
    // 查询结果为空，数据库中无数据
    MOCKER(CaseConf::QueryCaseAndOverCommitmentRatio).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(CaseConf::GetInstance().Init(), VM_OK);
    MOCKER(CaseConf::QueryCaseAndOverCommitmentRatio).reset();
    // memcpy_s失败
    MOCKER(CaseConf::QueryCaseAndOverCommitmentRatio).stubs().will(invoke(MockQueryCaseAndOverCommitmentRatio));
    MOCKER(memcpy_s).stubs().will(returnValue(1));
    EXPECT_EQ(CaseConf::GetInstance().Init(), VM_ERROR);
    MOCKER(memcpy_s).reset();
    // 成功
    EXPECT_EQ(CaseConf::GetInstance().Init(), VM_OK);
}

TEST_F(TestCaseConf, QueryCaseAndOverCommitmentRatioTest)
{
    CaseAndOvercommitmentRatio caseConf{};
    // 查询失败
    MOCKER(UbseStorageQueryData).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(CaseConf::GetInstance().QueryCaseAndOverCommitmentRatio(caseConf), VM_ERROR);
    MOCKER(UbseStorageQueryData).reset();
    // 成功
    MOCKER(UbseStorageQueryData).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(CaseConf::GetInstance().QueryCaseAndOverCommitmentRatio(caseConf), VM_OK);
}

TEST_F(TestCaseConf, UbseStorageDealDataTest)
{
    std::string str = "caseType:overCommitment;overCommitment:1.25";
    auto data = reinterpret_cast<uint8_t *>(str.data());
    const UbseByteBuffer buff = {.data = data, .len = str.length()};
    CaseAndOvercommitmentRatio caseAndOvercommitmentRatio{};
    // ctx为null
    CaseConf::GetInstance().UbseStorageDealData("", "", buff, nullptr);
    // 成功
    CaseConf::GetInstance().UbseStorageDealData("", "", buff, &caseAndOvercommitmentRatio);
}

void MockUbsePluginDeInit()
{
    return;
}

VmResult MockQueryCaseAndOverCommitmentRatio2(CaseAndOvercommitmentRatio &caseConf)
{
    caseConf.curCase = MEM_FRAGMENTATION_CASE;
    caseConf.overCommitmentRatio = MEM_FRAGMENTATION_RATIO;
    return VM_OK;
}

uint32_t UBSRMRSSetRunModeSuccess(const int &runMode)
{
    return VM_OK;
}

uint32_t UBSRMRSSetRunModeFail(const int &runMode)
{
    return VM_ERROR;
}

UBSRMRSSetRunModeFunc MockUBSRMRSSetRunModeSuccess()
{
    return UBSRMRSSetRunModeSuccess;
}

UBSRMRSSetRunModeFunc MockUBSRMRSSetRunModeFail()
{
    return UBSRMRSSetRunModeFail;
}

TEST_F(TestCaseConf, RunQueryCaseConfTestRunSetRunModeWithNullptr)
{
    // 前置条件
    MOCKER(MempoolingModule::UBSRMRSSetRunMode).stubs().will(returnValue(UBSRMRSSetRunModeFunc(nullptr)));
    auto ret = CaseConf::SetMemPoolingRunMode(OVER_COMMITMENT_CASE);
    EXPECT_EQ(ret, false);
}

TEST_F(TestCaseConf, RunQueryCaseConfTestRunSetRunMode)
{
    // 前置条件
    MOCKER(MempoolingModule::UBSRMRSSetRunMode).stubs().will(invoke(MockUBSRMRSSetRunModeSuccess));
    auto ret = CaseConf::SetMemPoolingRunMode(OVER_COMMITMENT_CASE);
    EXPECT_EQ(ret, true);
    ret = CaseConf::SetMemPoolingRunMode(MEM_FRAGMENTATION_CASE);
    EXPECT_EQ(ret, true);
    MOCKER(MempoolingModule::UBSRMRSSetRunMode).reset();

    MOCKER(MempoolingModule::UBSRMRSSetRunMode)
        .stubs()
        .will(invoke(MockUBSRMRSSetRunModeFail));
    ret = CaseConf::SetMemPoolingRunMode(OVER_COMMITMENT_CASE);
    EXPECT_EQ(ret, false);
    MOCKER(MempoolingModule::UBSRMRSSetRunMode).reset();
    GlobalMockObject::verify();
}

TEST_F(TestCaseConf, CaseConfResultParamToJsonTest)
{
    CaseConfResultParam caseResult = {.ret=1, .msg="success", .data="success"};
    MOCKER(VMJsonUtil::VMConvertMap2JsonStr).stubs().will(returnValue(false));
    EXPECT_EQ(caseResult.ToJson(), "");
    MOCKER(VMJsonUtil::VMConvertMap2JsonStr).reset();
    MOCKER(VMJsonUtil::VMConvertMap2JsonStr).stubs().will(returnValue(true));
    EXPECT_EQ(caseResult.ToJson(), "");
    GlobalMockObject::verify();
}

TEST_F(TestCaseConf, CaseConfParamFromJsonTest)
{
    CaseConfParam caseParam{};
    std::string overCommitmentStr = R"({"caseType": "overCommitment", "overCommitment": 1.25})";
    CaseConfResultParam caseResult = {.ret = 1, .msg = "success", .data = "success"};
    MOCKER(VMJsonUtil::VMConvertJsonStr2Map).stubs().will(returnValue(false));
    EXPECT_EQ(caseParam.FromJson(overCommitmentStr), false);
    MOCKER(VMJsonUtil::VMConvertJsonStr2Map).reset();
    MOCKER(VmStringUtil::SafeStof).stubs().will(throws(std::invalid_argument("Invalid argument: ff")));
    EXPECT_EQ(caseParam.FromJson(overCommitmentStr), false);
    MOCKER(VmStringUtil::SafeStof).reset();
    EXPECT_EQ(caseParam.FromJson(overCommitmentStr), true);
    GlobalMockObject::verify();
}

TEST_F(TestCaseConf, CaseAndOvercommitmentRatioDeserialTest)
{
    std::string caseConf = "aaa";
    CaseAndOvercommitmentRatio caseAndOvercommitmentRatio{};
    auto ret = CaseConf::GetInstance().CaseAndOvercommitmentRatioDeserial(caseConf, caseAndOvercommitmentRatio);
    EXPECT_EQ(ret, VM_ERROR);
    caseConf = "caseType:overCommitment;overCommitment:1.250000";
    ret = CaseConf::GetInstance().CaseAndOvercommitmentRatioDeserial(caseConf, caseAndOvercommitmentRatio);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestCaseConf, CaseConfInit_EmptyCaseConf)
{
    auto ret = CaseConf::GetInstance().Init();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestCaseConf, CaseConfInit)
{
    CaseAndOvercommitmentRatio caseConf{"memFragmentation", 1.25, 0};
    MOCKER(CaseConf::QueryCaseAndOverCommitmentRatio).stubs().with(outBound(caseConf)).will(returnValue(VM_OK));
    auto ret = CaseConf::GetInstance().Init();
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

UBSRMRSSetRunModeFunc MockerRunMode()
{
    return [](const int &) {
        return VM_OK;
    };
}

UBSRMRSSetWaterMarkFunc MockerSetWaterMarkFunc()
{
    return [](const WaterMark &) {
        return VM_OK;
    };
}

UBSRMRSSetWaterMarkFunc MockerSetWaterMarkFail()
{
    return [](const WaterMark &) {
        return VM_ERROR;
    };
}

TEST(CaseConfTest, SetMemPoolingWaterMarkWithNullptr) {
    MOCKER(MempoolingModule::UBSRMRSSetWaterMark).stubs().will(returnValue(UBSRMRSSetWaterMarkFunc(nullptr)));
    auto ret = CaseConf::SetMemPoolingWaterMark();
    EXPECT_EQ(ret, false);
    GlobalMockObject::verify();
}

TEST(CaseConfTest, SetMemPoolingWaterMark) {
    MOCKER(MempoolingModule::UBSRMRSSetWaterMark).stubs().will(invoke(MockerSetWaterMarkFunc));
    MOCKER(&VmConfiguration::GetBorrowWatermark).stubs().will(returnValue(std::string("80")));
    MOCKER(&VmConfiguration::GetLowWatermark).stubs().will(returnValue(std::string("20")));
    auto ret = CaseConf::SetMemPoolingWaterMark();
    EXPECT_EQ(ret, true);
    MOCKER(MempoolingModule::UBSRMRSSetWaterMark).reset();
    MOCKER(MempoolingModule::UBSRMRSSetWaterMark).stubs().will(invoke(MockerSetWaterMarkFail));
    ret = CaseConf::SetMemPoolingWaterMark();
    EXPECT_EQ(ret, false);

    MOCKER(&VmConfiguration::GetBorrowWatermark).reset();
    MOCKER(&VmConfiguration::GetBorrowWatermark).stubs().will(returnValue(std::string("abc")));
    ret = CaseConf::SetMemPoolingWaterMark();
    EXPECT_EQ(ret, false);
    GlobalMockObject::verify();
}

TEST(CaseConfTest, SetCaseConfToDB) {
    CaseConfParam param{.caseType="overCommitment", .overCommitmentRatio=1.25};
    MOCKER(UbseStoragePutData).stubs().will(returnValue(0));
    auto ret = SetCaseConfToDB(param);
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}
}