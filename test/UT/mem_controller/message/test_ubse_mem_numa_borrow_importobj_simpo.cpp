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

#include "test_ubse_mem_numa_borrow_importobj_simpo.h"

#include "mockcpp/mockcpp.hpp"

#include "message/ubse_mem_numa_borrow_importobj_simpo.h"
#include "ubse_conf_error.h"

namespace ubse::mem::controller::message::ut {
using namespace ubse::utils;
using namespace ubse::mem::serial;
using namespace ubse::mem::obj;
using namespace ubse::message;

void TestUbseMemNumaBorrowImportobjSimpo::SetUp()
{
    Test::SetUp();
    obj = new UbseMemNumaBorrowImportobjSimpo();
}

void TestUbseMemNumaBorrowImportobjSimpo::TearDown()
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
TEST_F(TestUbseMemNumaBorrowImportobjSimpo, Serialize)
{
    MOCKER_CPP(&UbseSerialization::Check).stubs().will(returnValue(false)).then(returnValue(true));
    EXPECT_TRUE(UBSE_ERROR == obj->Serialize());
    EXPECT_TRUE(UBSE_OK == obj->Serialize());
}

/*
 * 用例描述：测试Deserialize
 * 测试步骤：
 * 1.判断初始状态（mInputRawData为空)时的函数行为
 * 2.给mInputRawData赋值，模拟并判断 UbseMemNumaBorrowImportObjDeserialization 在失败和成功两种情况下Deserialize的返回值
 * 预期结果：
 * 1.初始状态函数返回UBSE_ERROR
 * 2. UbseMemNumaBorrowImportObjDeserialization 失败时函数返回UBSE_ERROR
 * 3. UbseMemNumaBorrowImportObjDeserialization 成功时函数返回UBSE_OK
 */
TEST_F(TestUbseMemNumaBorrowImportobjSimpo, Deserialize)
{
    EXPECT_TRUE(UBSE_ERROR == obj->Deserialize());

    uint32_t size = 4;
    auto buffer = new (std::nothrow) uint8_t[size];
    EXPECT_NE(nullptr, buffer);
    obj->SetInputRawDataFromShared(std::move(static_cast<std::shared_ptr<uint8_t[]>>(buffer)), size);
    MOCKER_CPP(&UbseMemNumaBorrowImportObjDeserialization)
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_TRUE(UBSE_ERROR == obj->Deserialize());
    EXPECT_TRUE(UBSE_OK == obj->Deserialize());
}
} // namespace ubse::mem::controller::message::ut