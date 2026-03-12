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

#include "test_ubs_virt_agent_case_conf_helper.h"
#include <gmock/gmock-function-mocker.h>
#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>

#include "case_conf_msg.h"
#include "ubs_virt_agent_case_conf_helper.h"
#include "vm_serial_util.h"

using namespace vm;
namespace ubse::vm::ut {

TestLibvirtAgentCaseConfHelper::TestLibvirtAgentCaseConfHelper() = default;

void TestLibvirtAgentCaseConfHelper::SetUp()
{
    Test::SetUp();
}

void TestLibvirtAgentCaseConfHelper::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentCaseConfHelper, ubse_case_conf_info_unpack_fail)
{
    uint8_t buffer[10] = "123";
    case_conf_info_t case_conf_info{};
    auto ret = ubse_case_conf_info_unpack(buffer, sizeof(buffer), &case_conf_info);
    EXPECT_EQ(ret, VA_ERROR_DESERIALIZE_FAILED);
}

TEST_F(TestLibvirtAgentCaseConfHelper, ubse_case_conf_set_unpack_fail)
{
    uint8_t buffer[10] = "123";
    case_conf_set_info_t case_conf_set_info{};
    auto ret = ubse_case_conf_set_unpack(buffer, sizeof(buffer), &case_conf_set_info);
    EXPECT_EQ(ret, VA_ERROR_DESERIALIZE_FAILED);
}

} // namespace ubse::vm::ut