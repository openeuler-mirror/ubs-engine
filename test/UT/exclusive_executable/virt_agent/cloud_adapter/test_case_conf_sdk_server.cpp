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

#include "test_case_conf_sdk_server.h"

#include "mockcpp/mockcpp.hpp"
#include "ubse_api_server.h"

#include "case_conf_sdk_server.h"

using namespace vm;
namespace ubse::ut::vm {

// 设置测试环境
void TestCaseConfSdkServer::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestCaseConfSdkServer::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestCaseConfSdkServer, RegisterFail)
{
    auto ret = VirtCaseConfSdk::Register();
    EXPECT_NE(ret, VM_OK);
}

TEST_F(TestCaseConfSdkServer, Register)
{
    MOCKER(RegisterIpcHandler).stubs().will(returnValue(VM_OK));
    auto ret = VirtCaseConfSdk::Register();
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestCaseConfSdkServer, GetCaseConfHandler_Success)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    MOCKER(VirtCaseConfSdk::GetCaseConf).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    VmResult result = VirtCaseConfSdk::GetCaseConfHandler(req, context);
    EXPECT_EQ(result, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestCaseConfSdkServer, GetCaseConfHandler_PackFail)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    MOCKER(VirtCaseConfSdk::GetCaseConf).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    MOCKER(VirtCaseConfSdk::PackGetCaseConfRsp).stubs().will(returnValue(VM_ERROR));
    VmResult result = VirtCaseConfSdk::GetCaseConfHandler(req, context);
    EXPECT_NE(result, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestCaseConfSdkServer, GetCaseConfHandler_SendResponseFail)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    MOCKER(VirtCaseConfSdk::GetCaseConf).stubs().will(returnValue(VM_OK));
    MOCKER(VirtCaseConfSdk::PackGetCaseConfRsp).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_ERROR));
    VmResult result = VirtCaseConfSdk::GetCaseConfHandler(req, context);
    EXPECT_EQ(result, VM_ERROR);
    GlobalMockObject::verify();
}

TEST_F(TestCaseConfSdkServer, GetCaseConfHandler_GetCaseConfFail)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    MOCKER(VirtCaseConfSdk::GetCaseConf).stubs().will(returnValue(VM_ERROR));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    VmResult result = VirtCaseConfSdk::GetCaseConfHandler(req, context);
    EXPECT_NE(result, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestCaseConfSdkServer, GetCaseConf_Fail)
{
    CaseConfInfo caseConfInfo;
    VmResult result = VirtCaseConfSdk::GetCaseConf(caseConfInfo);
    EXPECT_NE(result, VM_OK);
}

TEST_F(TestCaseConfSdkServer, GetCaseConf_EmptyCaseConf)
{
    CaseConfInfo caseConfInfo;
    MOCKER(CaseConf::QueryCaseAndOverCommitmentRatio).stubs().will(returnValue(VM_OK));
    VmResult result = VirtCaseConfSdk::GetCaseConf(caseConfInfo);
    EXPECT_NE(result, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestCaseConfSdkServer, GetCaseConf_Success)
{
    CaseConfInfo caseConfInfo;
    CaseAndOvercommitmentRatio expectedCaseConf;
    expectedCaseConf.curCase = "normal";
    expectedCaseConf.overCommitmentRatio = 1.5f;
    expectedCaseConf.index = 0;
    MOCKER(CaseConf::QueryCaseAndOverCommitmentRatio).stubs().with(outBound(expectedCaseConf)).will(returnValue(VM_OK));
    VmResult result = VirtCaseConfSdk::GetCaseConf(caseConfInfo);
    EXPECT_EQ(result, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestCaseConfSdkServer, SetCaseConfHandler)
{
    std::string reqJson = R"({
        "caseType": "overCommitment",
        "overCommitment": 1.25
    })";
    // 创建请求消息
    UbseIpcMessage req;
    req.buffer = new uint8_t[reqJson.size()];
    memcpy_s(req.buffer, reqJson.size(), reqJson.c_str(), reqJson.size());
    req.length = reqJson.size();
    UbseRequestContext context;
    MOCKER(VirtCaseConfSdk::SetCaseConf).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    uint32_t ret = VirtCaseConfSdk::SetCaseConfHandler(req, context);
    EXPECT_EQ(ret, VM_OK);
    delete[] req.buffer;
}

TEST_F(TestCaseConfSdkServer, SetCaseConfHandler_PackFail)
{
    std::string reqJson = R"({
        "caseType": "overCommitment",
        "overCommitment": 1.25
    })";
    // 创建请求消息
    UbseIpcMessage req;
    req.buffer = new uint8_t[reqJson.size()];
    memcpy_s(req.buffer, reqJson.size(), reqJson.c_str(), reqJson.size());
    req.length = reqJson.size();
    UbseRequestContext context;
    MOCKER(VirtCaseConfSdk::SetCaseConf).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    MOCKER(VirtCaseConfSdk::PackSetCaseConfRsp).stubs().will(returnValue(VM_ERROR));
    uint32_t ret = VirtCaseConfSdk::SetCaseConfHandler(req, context);
    EXPECT_EQ(ret, VM_ERROR);
    delete[] req.buffer;
}

TEST_F(TestCaseConfSdkServer, SetCaseConfHandler_SetCaseConfFail)
{
    std::string reqJson = R"({
        "caseType": "overCommitment",
        "overCommitment": 1.25
    })";
    // 创建请求消息
    UbseIpcMessage req;
    req.buffer = new uint8_t[reqJson.size()];
    memcpy_s(req.buffer, reqJson.size(), reqJson.c_str(), reqJson.size());
    req.length = reqJson.size();
    UbseRequestContext context;
    uint32_t ret = VirtCaseConfSdk::SetCaseConfHandler(req, context);
    EXPECT_EQ(ret, VM_ERROR);
    delete[] req.buffer;
}

TEST_F(TestCaseConfSdkServer, SetCaseConfHandler_NullBuffer)
{
    UbseIpcMessage req;
    req.buffer = nullptr;
    UbseRequestContext context;
    uint32_t ret = VirtCaseConfSdk::SetCaseConfHandler(req, context);
    EXPECT_EQ(ret, VM_ERROR_NULLPTR);
}

TEST_F(TestCaseConfSdkServer, SetCaseConf_Success)
{
    // 构造合法的JSON请求数据
    std::string reqJson = R"({
        "caseType": "overCommitment",
        "overCommitment": 1.25
    })";

    CaseConfParam caseParam;
    CaseConfResultParam caseResult;
    CaseAndOvercommitmentRatio caseConf{"overCommitment", 1.25f, 0};
    MOCKER(CaseConf::QueryCaseAndOverCommitmentRatio).stubs().with(outBound(caseConf)).will(returnValue(VM_OK));
    // 调用被测函数
    uint32_t ret = VirtCaseConfSdk::SetCaseConf(reqJson, caseParam, caseResult);
    // 验证返回值
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestCaseConfSdkServer, SetCaseConf_CaseNotExist)
{
    // 构造合法的JSON请求数据
    std::string reqJson = R"({
        "caseType": "overCommitment",
        "overCommitment": 1.25
    })";

    CaseConfParam caseParam;
    CaseConfResultParam caseResult;
    CaseAndOvercommitmentRatio caseConf{"overCommitment", 1.25f, 0};
    MOCKER(CaseConf::QueryCaseAndOverCommitmentRatio).stubs().will(returnValue(VM_OK));
    MOCKER(SetCaseConfToDB).stubs().will(returnValue(VM_OK));
    // 调用被测函数
    uint32_t ret = VirtCaseConfSdk::SetCaseConf(reqJson, caseParam, caseResult);
    // 验证返回值
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestCaseConfSdkServer, SetCaseConf_SetCaseConfFail)
{
    // 构造合法的JSON请求数据
    std::string reqJson = R"({
        "caseType": "overCommitment",
        "overCommitment": 1.25
    })";

    CaseConfParam caseParam;
    CaseConfResultParam caseResult;
    CaseAndOvercommitmentRatio caseConf{"overCommitment", 1.25f, 0};
    MOCKER(CaseConf::QueryCaseAndOverCommitmentRatio).stubs().will(returnValue(VM_OK));
    MOCKER(SetCaseConfToDB).stubs().will(returnValue(VM_ERROR));
    // 调用被测函数
    uint32_t ret = VirtCaseConfSdk::SetCaseConf(reqJson, caseParam, caseResult);
    // 验证返回值
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestCaseConfSdkServer, SetCaseConfCheckReq_EmptyReq)
{
    CaseConfParam caseParam;
    CaseConfResultParam caseResult;
    auto ret = VirtCaseConfSdk::SetCaseConfCheckReq("", caseParam, caseResult);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestCaseConfSdkServer, SetCaseConfCheckReq_JsonParseFail)
{
    CaseConfParam caseParam;
    CaseConfResultParam caseResult;
    auto ret = VirtCaseConfSdk::SetCaseConfCheckReq("reqBody", caseParam, caseResult);
    EXPECT_EQ(ret, VM_ERROR);
}

} // namespace ubse::ut::vm