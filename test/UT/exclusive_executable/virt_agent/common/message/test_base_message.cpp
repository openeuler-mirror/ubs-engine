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

#include "test_base_message.h"

#include "mockcpp/mockcpp.hpp"
#include "vm_error.h"

using namespace vm;
namespace ubse::ut::vm {

// 设置测试环境
void TestBaseMessage::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestBaseMessage::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

VmResult TestBaseMessageClass::Serialize()
{
    return 0;
}

VmResult TestBaseMessageClass::Deserialize()
{
    return 0;
}

TEST_F(TestBaseMessage, SetInputRawDataTest)
{
    TestBaseMessageClass baseMessage;
    EXPECT_EQ(baseMessage.SetInputRawData(nullptr, 0, false), VM_ERROR_SERIALIZE_DESERIALIZE_COMMON_ERROR);
    std::string str = "test";
    auto data = reinterpret_cast<uint8_t *>(str.data());
    auto len = str.length();
    EXPECT_EQ(baseMessage.SetInputRawData(data, 0, false), VM_ERROR_SERIALIZE_DESERIALIZE_COMMON_ERROR);
    MOCKER(memcpy_s).stubs().will(returnValue(1));
    EXPECT_EQ(baseMessage.SetInputRawData(data, len, true), VM_ERROR_SERIALIZE_DESERIALIZE_COMMON_ERROR);
    MOCKER(memcpy_s).reset();
    EXPECT_EQ(baseMessage.SetInputRawData(data, len, true), VM_OK);
    EXPECT_EQ(baseMessage.SetInputRawData(data, len, false), VM_OK);
}
} // namespace ubse::ut::vm