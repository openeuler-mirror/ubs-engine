/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "test_ubse_mem_operation_resp_simpo.h"

#include "mockcpp/mockcpp.hpp"

#include "message/ubse_mem_controller_conversion.h"
#include "message/ubse_mem_controller_serial.h"
#include "message/ubse_mem_operation_resp_simpo.h"
#include "ubse_conf_error.h"
#include "ubse_serial_util.h"

namespace ubse::mem::controller::message::ut {
using namespace ubse::message;
using namespace ubse::utils;
using namespace ubse::mem::serial;

void TestUbseMemOperationRespSimpo::SetUp()
{
    Test::SetUp();
    obj = new UbseMemOperationRespSimpo();
}

void TestUbseMemOperationRespSimpo::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：测试Serialize方法
 * 测试步骤：
 * 1.模拟Check函数返回值，先false再true
 * 预期结果：
 * Check返回false时，方法返回UBSE_ERROR;反之返回UBSE_OK;
 */
TEST_F(TestUbseMemOperationRespSimpo, Serialize)
{
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == obj->Serialize());
    EXPECT_TRUE(UBSE_OK == obj->Serialize());
}

/*
 * 用例描述：测试Deserialize
 * 测试步骤：
 * 1.判断初始状态（mInputRawData为空)时的函数行为
 * 2.给mInputRawData赋值，模拟并判断 UbseMemOperationRespDeserialize 在失败情况下Deserialize的返回值
 * 3.判断Check函数返回false和true时Deserialize的行为
 * 预期结果：
 * 1.初始状态函数返回UBSE_ERROR
 * 2. UbseMemOperationRespDeserialize 失败时函数返回UBSE_ERROR
 * 3.Check函数返回false时，Deserialize返回UBSE_ERROR
 * 4.一切功能正常时，返回UBSE_OK
 */
TEST_F(TestUbseMemOperationRespSimpo, Deserialize)
{
    EXPECT_TRUE(UBSE_ERROR == obj->Deserialize());

    uint32_t size = 4;
    auto buffer = new (std::nothrow) uint8_t[size];
    EXPECT_NE(nullptr, buffer);
    obj->SetInputRawDataFromShared(std::move(static_cast<std::shared_ptr<uint8_t[]>>(buffer)), size);
    MOCKER_CPP(&UbseMemOperationRespDeserialize).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == obj->Deserialize());

    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == obj->Deserialize());
    EXPECT_TRUE(UBSE_OK == obj->Deserialize());
}
} // namespace ubse::mem::controller::message::ut