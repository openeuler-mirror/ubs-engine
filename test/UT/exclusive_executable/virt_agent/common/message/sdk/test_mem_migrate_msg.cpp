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

#include "test_mem_migrate_msg.h"
#include <gmock/gmock-function-mocker.h>
#include <mockcpp/mokc.h>

#include "mem_migrate_msg.h"

using namespace vm;
using vm::MemMigrateMsg;
namespace ubse::vm::ut {

TestMemMigrateMsg::TestMemMigrateMsg() = default;

void TestMemMigrateMsg::SetUp()
{
    Test::SetUp();
}

void TestMemMigrateMsg::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestMemMigrateMsg, TestMemMigrateMsg_Serialize_Deserialize)
{
    MemMigrateInputParams inputParams{"opt", "uuid"};
    MemMigrateMsg msg{inputParams};
    auto ret = msg.Serialize();
    EXPECT_EQ(ret, VM_OK);
    MemMigrateMsg dmsg{msg.SerializedData(), msg.SerializedDataSize()};
    ret = dmsg.Deserialize();
    EXPECT_EQ(ret, VM_OK);
    auto param = dmsg.GetInputParams();
    EXPECT_EQ(param.opt, "opt");
    EXPECT_EQ(param.uuid, "uuid");
}
} // namespace ubse::vm::ut