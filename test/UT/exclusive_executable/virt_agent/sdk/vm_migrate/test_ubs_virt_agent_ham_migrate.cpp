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

#include "ubse_ipc_client.h"
#include "ubs_virt_agent_ham_migrate.h"
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
    virt_agent_ret_t ret = RackStartIpcClientWithTimeout(500);
    EXPECT_EQ(ret, VA_SUCCESS);
}

TEST_F(TestUbsVirtAgentHamMigrate, AllocateRequestBufferFail)
{
    HamComByteBuffer request = {nullptr, 0};
    HamComByteBuffer response = {nullptr, 0};
    auto ret = RackSyncSendForHam(&request, &response);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);
    ret = RackSyncSendForHam(nullptr, &response);
    EXPECT_EQ(ret, VA_ERROR_INVALID_PARAM);
}

TEST_F(TestUbsVirtAgentHamMigrate, CallExternalApiWithTimeoutFail)
{
    HamComByteBuffer request = {new uint8_t[10], 10};
    HamComByteBuffer response = {nullptr, 0};
    int ret = RackSyncSendForHam(&request, &response);
    EXPECT_NE(ret, VA_SUCCESS);
    delete[] request.data;
}

TEST_F(TestUbsVirtAgentHamMigrate, ProcessResponseFail)
{
    HamComByteBuffer request = {new uint8_t[10], 10};
    HamComByteBuffer response = {nullptr, 0};
    MOCKER(ubse_invoke_call).stubs().will(returnValue(static_cast<uint32_t>(0)));
    int ret = RackSyncSendForHam(&request, &response);
    EXPECT_NE(ret, VA_SUCCESS);
    delete[] request.data;
}

TEST_F(TestUbsVirtAgentHamMigrate, TestNullRequest)
{
    HamComByteBuffer request = {nullptr, 0};
    HamComCallbackDef callback = {nullptr, nullptr};
    char* data = "request";
    request.data = reinterpret_cast<uint8_t*>(data);
    request.len = strlen(data);
    int ret = RackAsyncSendForHam(&request, &callback);
    EXPECT_EQ(ret, 0);
}

} // namespace ubse::vm::ut