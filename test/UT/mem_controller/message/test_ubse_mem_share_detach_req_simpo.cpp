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

#include "test_ubse_mem_share_detach_req_simpo.h"

#include "mockcpp/mockcpp.hpp"

#include "ubse_error.h"
#include "ubse_serial_util.h"
#include "message/ubse_mem_controller_serial.h"
#include "message/ubse_mem_share_detach_req_simpo.h"

namespace ubse::mem::controller::message::ut {
using namespace ubse::message;
using namespace ubse::utils;
using namespace ubse::mem::serial;

void TestUbseMemShareDetachReqSimpo::SetUp()
{
    GTEST_SKIP();
    Test::SetUp();
    obj = new UbseMemShareDetachReqSimpo();
}

void TestUbseMemShareDetachReqSimpo::TearDown()
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
TEST_F(TestUbseMemShareDetachReqSimpo, Serialize)
{
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == obj->Serialize());
    EXPECT_TRUE(UBSE_OK == obj->Serialize());
}

/*
 * 用例描述：测试Deserialize
 * 测试步骤：
 * 1.判断初始状态（mInputRawData为空)时的函数行为
 * 2.给mInputRawData赋值，模拟并判断 UbseMemShareDetachReqDeserialize 在失败情况下Deserialize的返回值
 * 3.判断Check函数返回false和true时Deserialize的行为
 * 预期结果：
 * 1.初始状态函数返回UBSE_ERROR
 * 2. UbseMemShareDetachReqDeserialize 失败时函数返回UBSE_ERROR
 * 3.Check函数返回false时，Deserialize返回UBSE_ERROR
 * 4.一切功能正常时，返回UBSE_OK
 */
TEST_F(TestUbseMemShareDetachReqSimpo, Deserialize)
{
    EXPECT_TRUE(UBSE_ERROR == obj->Deserialize());

    uint32_t size = 4;
    auto buffer = new (std::nothrow) uint8_t[size];
    EXPECT_NE(nullptr, buffer);
    obj->SetInputRawDataFromShared(std::move(static_cast<std::shared_ptr<uint8_t[]>>(buffer)), size);
    MOCKER_CPP(&UbseMemShareDetachReqDeserialization).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == obj->Deserialize());

    MOCKER_CPP(&UbseDeSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == obj->Deserialize());
    EXPECT_TRUE(UBSE_OK == obj->Deserialize());
}
} // namespace ubse::mem::controller::message::ut