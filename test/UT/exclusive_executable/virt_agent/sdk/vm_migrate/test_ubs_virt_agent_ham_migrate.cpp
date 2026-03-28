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

#include "test_ubs_virt_agent_ham_migrate.h"
#include <gmock/gmock-function-mocker.h>
#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>

#include "ubs_virt_agent_ham_migrate.h"
#include "ubse_ipc_client.h"
#include "vm_serial_util.h"

using namespace vm;
namespace ubse::vm::ut {

TestUbsVirtAgentHamMigrate::TestUbsVirtAgentHamMigrate() = default;

void TestUbsVirtAgentHamMigrate::SetUp()
{
    Test::SetUp();
}

void TestUbsVirtAgentHamMigrate::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbsVirtAgentHamMigrate, ubs_virt_agent_make_migrate_decision_fail)
{
    auto ret = ubs_virt_agent_make_migrate_decision(1, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);
    char uuid[] = "uuid";
    char destHostName[] = "destHostName";
    uint32_t migrateStrategy = 0;
    MOCKER(ubse_invoke_call).stubs().will(returnValue(uint32_t(1)));
    ret = ubs_virt_agent_make_migrate_decision(1, uuid, destHostName, 0, &migrateStrategy);
    EXPECT_EQ(ret, VA_ERROR_BASE);
    MOCKER(ubse_invoke_call).reset();
    MOCKER(ubse_invoke_call).stubs().will(returnValue(uint32_t(0)));
    ret = ubs_virt_agent_make_migrate_decision(1, uuid, destHostName, 0, &migrateStrategy);
    EXPECT_NE(ret, VA_SUCCESS);
}

TEST_F(TestUbsVirtAgentHamMigrate, SetTimeout_Normal)
{
    virt_agent_ret_t ret = ubs_virt_agent_set_timeout(500, 1);
    EXPECT_EQ(ret, VA_SUCCESS);
}

TEST_F(TestUbsVirtAgentHamMigrate, AllocateRequestBufferFail)
{
    VirtAgentByteBuffer request = {nullptr, 0};
    VirtAgentByteBuffer response = {nullptr, 0};
    auto ret = ubs_sync_send_msg(&request, &response, 1);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);
    ret = ubs_sync_send_msg(nullptr, &response, 1);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);
}

TEST_F(TestUbsVirtAgentHamMigrate, CallExternalApiWithTimeoutFail)
{
    VirtAgentByteBuffer request = {new uint8_t[10], 10};
    VirtAgentByteBuffer response = {nullptr, 0};
    int ret = ubs_sync_send_msg(&request, &response, 1);
    EXPECT_NE(ret, VA_SUCCESS);
    delete[] request.data;
}

TEST_F(TestUbsVirtAgentHamMigrate, ProcessResponseFail)
{
    VirtAgentByteBuffer request = {new uint8_t[10], 10};
    VirtAgentByteBuffer response = {nullptr, 0};
    MOCKER(ubse_invoke_call).stubs().will(returnValue(static_cast<uint32_t>(0)));
    int ret = ubs_sync_send_msg(&request, &response, 1);
    EXPECT_NE(ret, VA_SUCCESS);
    delete[] request.data;
}

TEST_F(TestUbsVirtAgentHamMigrate, TestNullRequest)
{
    VirtAgentByteBuffer request = {nullptr, 0};
    VirtAgentCallbackDef callback = {nullptr, nullptr};
    char *data = "request";
    request.data = reinterpret_cast<uint8_t *>(data);
    request.len = strlen(data);
    int ret = ubs_async_send_msg(&request, &callback, 1);
    EXPECT_EQ(ret, 0);
}

} // namespace ubse::vm::ut