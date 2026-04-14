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

#include "test_ubs_virt_agent_case_conf.h"
#include <gmock/gmock-function-mocker.h>
#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>

#include "ubs_virt_agent_case_conf.h"
#include "ubs_virt_agent_case_conf_helper.h"
#include "ubse_ipc_client.h"
#include "vm_serial_util.h"

using namespace vm;
namespace ubse::vm::ut {

TestLibvirtAgentCaseConf::TestLibvirtAgentCaseConf() = default;

void TestLibvirtAgentCaseConf::SetUp()
{
    Test::SetUp();
}

void TestLibvirtAgentCaseConf::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentCaseConf, ubs_virt_agent_case_conf_get_with_nullptr)
{
    auto ret = ubs_virt_agent_case_conf_get(nullptr);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);
}

TEST_F(TestLibvirtAgentCaseConf, ubs_virt_agent_case_conf_get)
{
    case_conf_info_t case_conf_info{"cur_case", "over_commitment_ratio", "migrate_waterLine", 0, "host_id"};
    MOCKER(ubse_invoke_call).stubs().will(returnValue(uint32_t(0)));
    MOCKER(ubse_case_conf_info_unpack).stubs().will(returnValue(0));
    auto ret = ubs_virt_agent_case_conf_get(&case_conf_info);
    EXPECT_EQ(ret, VA_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentCaseConf, ubs_virt_agent_case_conf_get_fail)
{
    case_conf_info_t case_conf_info{"cur_case", "over_commitment_ratio", "migrate_waterLine", 0, "host_id"};
    auto ret = ubs_virt_agent_case_conf_get(&case_conf_info);
    EXPECT_EQ(ret, VA_ERROR_BASE);
}

TEST_F(TestLibvirtAgentCaseConf, ubs_virt_agent_case_conf_get_unpack_fail)
{
    case_conf_info_t case_conf_info{"cur_case", "over_commitment_ratio", "migrate_waterLine", 0, "host_id"};
    MOCKER(ubse_invoke_call).stubs().will(returnValue(uint32_t(0)));
    MOCKER(ubse_case_conf_info_unpack).stubs().will(returnValue(1));
    auto ret = ubs_virt_agent_case_conf_get(&case_conf_info);
    EXPECT_EQ(ret, VA_ERROR_DESERIALIZE_FAILED);
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentCaseConf, ubs_virt_agent_case_conf_set_with_nullptr)
{
    auto ret = ubs_virt_agent_case_conf_set(nullptr, nullptr);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);
}

TEST_F(TestLibvirtAgentCaseConf, ubs_virt_agent_case_conf_set)
{
    char param[] = "param";
    case_conf_set_info_t case_conf_set_info{0};
    MOCKER(memcpy_s).stubs().will(returnValue(0));
    MOCKER(ubse_invoke_call).stubs().will(returnValue(uint32_t(0)));
    MOCKER(ubse_case_conf_set_unpack).stubs().will(returnValue(0));
    auto ret = ubs_virt_agent_case_conf_set(param, &case_conf_set_info);
    EXPECT_EQ(ret, VA_SUCCESS);
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtAgentCaseConf, ubs_virt_agent_case_conf_set_fail)
{
    char param[] = "param";
    case_conf_set_info_t case_conf_set_info{0};
    MOCKER(ubse_invoke_call).stubs().will(returnValue(1));
    auto ret = ubs_virt_agent_case_conf_set(param, &case_conf_set_info);
    EXPECT_EQ(ret, VA_ERROR_BASE);
}

TEST_F(TestLibvirtAgentCaseConf, ubs_virt_agent_case_conf_set_unpack_fail)
{
    char param[] = "param";
    case_conf_set_info_t case_conf_set_info{0};
    MOCKER(ubse_invoke_call).stubs().will(returnValue(uint32_t(0)));
    MOCKER(ubse_case_conf_set_unpack).stubs().will(returnValue(1));
    auto ret = ubs_virt_agent_case_conf_set(param, &case_conf_set_info);
    EXPECT_EQ(ret, VA_ERROR_DESERIALIZE_FAILED);
    GlobalMockObject::verify();
}
} // namespace ubse::vm::ut