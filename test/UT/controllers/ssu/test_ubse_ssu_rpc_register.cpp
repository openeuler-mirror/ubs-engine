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

#include "test_ubse_ssu_rpc_register.h"

#include "ubse_com.h"
#include "ubse_com_op_code.h"
#include "ubse_error.h"
#include "ubse_ssu_rpc_processor.h"

namespace ubse::ssu::controller::ut {

using namespace ubse::com;

void TestUbseSsuRpcRegister::SetUp()
{
    Test::SetUp();
}

void TestUbseSsuRpcRegister::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// 所有Register*方法都成功时，RegHandler应返回UBSE_OK
TEST_F(TestUbseSsuRpcRegister, RegHandler_Success)
{
    MOCKER(&UbseRpcEndpointFactory::Build).stubs().will(returnValue(std::make_shared<UbseRpcEndpoint>()));
    auto ret = UbseSsuRpcProcessor::RegHandler();
    EXPECT_EQ(ret, UBSE_OK);
}

// 第一个注册失败（Alloc req）导致整体失败
TEST_F(TestUbseSsuRpcRegister, RegHandler_AllocReqFail)
{
    MOCKER(&UbseRpcEndpointFactory::Build).stubs().will(returnValue(std::shared_ptr<UbseRpcEndpoint>(nullptr)));
    auto ret = UbseSsuRpcProcessor::RegHandler();
    EXPECT_EQ(ret, UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED);
}

// RegisterAllocHandlers: 第一个Build成功、第二个失败 → 返回UBSE_ERROR
TEST_F(TestUbseSsuRpcRegister, RegisterAllocHandlers_RespFail)
{
    MOCKER(&UbseRpcEndpointFactory::Build)
        .stubs()
        .will(returnValue(std::make_shared<UbseRpcEndpoint>()))
        .then(returnValue(std::shared_ptr<UbseRpcEndpoint>(nullptr)));
    auto ret = UbseSsuRpcProcessor::RegisterAllocHandlers();
    EXPECT_EQ(ret, UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED);
}

// RegisterAllocHandlers: 全部成功 → 返回UBSE_OK
TEST_F(TestUbseSsuRpcRegister, RegisterAllocHandlers_Success)
{
    MOCKER(&UbseRpcEndpointFactory::Build).stubs().will(returnValue(std::make_shared<UbseRpcEndpoint>()));
    auto ret = UbseSsuRpcProcessor::RegisterAllocHandlers();
    EXPECT_EQ(ret, UBSE_OK);
}

// RegisterStatusHandler: Build失败 → UBSE_ERROR
TEST_F(TestUbseSsuRpcRegister, RegisterStatusHandler_Fail)
{
    MOCKER(&UbseRpcEndpointFactory::Build).stubs().will(returnValue(std::shared_ptr<UbseRpcEndpoint>(nullptr)));
    auto ret = UbseSsuRpcProcessor::RegisterStatusHandler();
    EXPECT_EQ(ret, UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED);
}

// RegisterStatusHandler: 成功 → UBSE_OK
TEST_F(TestUbseSsuRpcRegister, RegisterStatusHandler_Success)
{
    MOCKER(&UbseRpcEndpointFactory::Build).stubs().will(returnValue(std::make_shared<UbseRpcEndpoint>()));
    auto ret = UbseSsuRpcProcessor::RegisterStatusHandler();
    EXPECT_EQ(ret, UBSE_OK);
}

// RegisterFreeHandlers: 第一个Build成功、第二个失败 → UBSE_ERROR
TEST_F(TestUbseSsuRpcRegister, RegisterFreeHandlers_RespFail)
{
    MOCKER(&UbseRpcEndpointFactory::Build)
        .stubs()
        .will(returnValue(std::make_shared<UbseRpcEndpoint>()))
        .then(returnValue(std::shared_ptr<UbseRpcEndpoint>(nullptr)));
    auto ret = UbseSsuRpcProcessor::RegisterFreeHandlers();
    EXPECT_EQ(ret, UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED);
}

// RegisterFreeHandlers: 全部成功 → UBSE_OK
TEST_F(TestUbseSsuRpcRegister, RegisterFreeHandlers_Success)
{
    MOCKER(&UbseRpcEndpointFactory::Build).stubs().will(returnValue(std::make_shared<UbseRpcEndpoint>()));
    auto ret = UbseSsuRpcProcessor::RegisterFreeHandlers();
    EXPECT_EQ(ret, UBSE_OK);
}

// RegisterAttachDetachVerifyHandlers: Build失败 → UBSE_ERROR
TEST_F(TestUbseSsuRpcRegister, RegisterAttachDetachVerifyHandlers_Fail)
{
    MOCKER(&UbseRpcEndpointFactory::Build).stubs().will(returnValue(std::shared_ptr<UbseRpcEndpoint>(nullptr)));
    auto ret = UbseSsuRpcProcessor::RegisterAttachDetachVerifyHandlers();
    EXPECT_EQ(ret, UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED);
}

// RegisterAttachDetachVerifyHandlers: 成功 → UBSE_OK
TEST_F(TestUbseSsuRpcRegister, RegisterAttachDetachVerifyHandlers_Success)
{
    MOCKER(&UbseRpcEndpointFactory::Build).stubs().will(returnValue(std::make_shared<UbseRpcEndpoint>()));
    auto ret = UbseSsuRpcProcessor::RegisterAttachDetachVerifyHandlers();
    EXPECT_EQ(ret, UBSE_OK);
}

// RegisterAddPermHandlers: 第一个Build成功、第二个失败 → UBSE_ERROR
TEST_F(TestUbseSsuRpcRegister, RegisterAddPermHandlers_RespFail)
{
    MOCKER(&UbseRpcEndpointFactory::Build)
        .stubs()
        .will(returnValue(std::make_shared<UbseRpcEndpoint>()))
        .then(returnValue(std::shared_ptr<UbseRpcEndpoint>(nullptr)));
    auto ret = UbseSsuRpcProcessor::RegisterAddPermHandlers();
    EXPECT_EQ(ret, UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED);
}

// RegisterAddPermHandlers: 全部成功 → UBSE_OK
TEST_F(TestUbseSsuRpcRegister, RegisterAddPermHandlers_Success)
{
    MOCKER(&UbseRpcEndpointFactory::Build).stubs().will(returnValue(std::make_shared<UbseRpcEndpoint>()));
    auto ret = UbseSsuRpcProcessor::RegisterAddPermHandlers();
    EXPECT_EQ(ret, UBSE_OK);
}

// RegisterRemovePermHandlers: 第一个Build成功、第二个失败 → UBSE_ERROR
TEST_F(TestUbseSsuRpcRegister, RegisterRemovePermHandlers_RespFail)
{
    MOCKER(&UbseRpcEndpointFactory::Build)
        .stubs()
        .will(returnValue(std::make_shared<UbseRpcEndpoint>()))
        .then(returnValue(std::shared_ptr<UbseRpcEndpoint>(nullptr)));
    auto ret = UbseSsuRpcProcessor::RegisterRemovePermHandlers();
    EXPECT_EQ(ret, UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED);
}

// RegisterRemovePermHandlers: 全部成功 → UBSE_OK
TEST_F(TestUbseSsuRpcRegister, RegisterRemovePermHandlers_Success)
{
    MOCKER(&UbseRpcEndpointFactory::Build).stubs().will(returnValue(std::make_shared<UbseRpcEndpoint>()));
    auto ret = UbseSsuRpcProcessor::RegisterRemovePermHandlers();
    EXPECT_EQ(ret, UBSE_OK);
}

// RegisterQueryHandlers: 第1个Build成功、第2个失败 → UBSE_ERROR（GetNsStats成功，ListAllocInfo失败）
TEST_F(TestUbseSsuRpcRegister, RegisterQueryHandlers_ListAllocInfoFail)
{
    MOCKER(&UbseRpcEndpointFactory::Build)
        .stubs()
        .will(returnValue(std::make_shared<UbseRpcEndpoint>()))
        .then(returnValue(std::shared_ptr<UbseRpcEndpoint>(nullptr)));
    auto ret = UbseSsuRpcProcessor::RegisterQueryHandlers();
    EXPECT_EQ(ret, UBSE_SSU_ERROR_ENDPOINT_REGISTER_FAILED);
}

// RegisterQueryHandlers: 全部成功 → UBSE_OK
TEST_F(TestUbseSsuRpcRegister, RegisterQueryHandlers_Success)
{
    MOCKER(&UbseRpcEndpointFactory::Build).stubs().will(returnValue(std::make_shared<UbseRpcEndpoint>()));
    auto ret = UbseSsuRpcProcessor::RegisterQueryHandlers();
    EXPECT_EQ(ret, UBSE_OK);
}

} // namespace ubse::ssu::controller::ut
