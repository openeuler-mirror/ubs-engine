/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <dlfcn.h>
#include <gtest/gtest.h>
#include "mockcpp/mokc.h"
#define private public
#include "agent_task_processor.h"
#include "turbo_runtime_manager.h"
#include "turbo_ucache_interface.h"
#include "ucache_config.h"
#include "ucache_error.h"
#include "ucache_serialize.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

void *dlopen(const char *d_file, int d_mode);
int dlclose(void *d_handle);
void *dlsym(void *__restrict d_handle, const char *__restrict d_name);

using namespace ubse::com;
using namespace turbo::ucache;
using namespace ucache;
using namespace ucache::agent;
using namespace ubse::com;
using namespace ubse::log;
using namespace ucache;
using namespace ucache::serialize;

class AgentTaskProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {}

    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

namespace ucache::agent {
uint32_t FillErrorResp(const std::string &requestType, const UbseByteBuffer &req, UbseByteBuffer &resp);
void FreeMemory(uint8_t *data);
} // namespace ucache::agent

TEST_F(AgentTaskProcessorTest, InitAgentTaskProcessor_TESTOK)
{
    MOCKER(&UbseRegRpcService).stubs().will(returnValue(UCACHE_OK));
    uint32_t ret = InitAgentTaskProcessor();
    EXPECT_EQ(ret, UCACHE_OK);
}

TEST_F(AgentTaskProcessorTest, InitAgentTaskProcessor_TESTERR)
{
    MOCKER(&UbseRegRpcService).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = InitAgentTaskProcessor();
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(AgentTaskProcessorTest, ProcessTask_TEST_Invalid_Buffer)
{
    UbseByteBuffer req{};
    UbseByteBuffer resp{};
    ProcessTask(req, resp);
    TaskResponse tRespA{};
    {
        UCacheInStream in(resp.data, resp.len);
        in >> tRespA;
    }
    EXPECT_EQ(tRespA.resCode, INVALID_BUFFER);

    req.len = 1;
    ProcessTask(req, resp);
    TaskResponse tRespB{};
    {
        UCacheInStream in(resp.data, resp.len);
        in >> tRespB;
    }
    EXPECT_EQ(tRespB.resCode, INVALID_BUFFER);
}

TEST_F(AgentTaskProcessorTest, ProcessTask_TEST_Invalid_TaskType)
{
    TaskRequest tReq{};
    tReq.type = static_cast<TaskType>(10);
    UCacheOutStream out{};
    out << tReq;
    UbseByteBuffer req = {.data = out.GetBufferPointer(), .len = out.GetSize(), .freeFunc = nullptr};
    UbseByteBuffer resp{};
    ProcessTask(req, resp);
    TaskResponse tResp{};
    {
        UCacheInStream in(resp.data, resp.len);
        in >> tResp;
    }
    EXPECT_EQ(tResp.resCode, INVALID_TASK_TYPE);
    delete[] req.data;
}

uint32_t MockUCacheExecuteTaskReturnOK(const TaskRequest &tReq, TaskResponse &tResp)
{
    tResp.resCode = UCACHE_OK;
    return UCACHE_OK;
}

uint32_t MockUCacheExecuteTaskReturnERR(const TaskRequest &tReq, TaskResponse &tResp)
{
    return TURBO_EXECUTE_ERROR;
}

TEST_F(AgentTaskProcessorTest, ProcessTask_TEST_IPCFailed)
{
    TurboRuntimeManager::ucacheExecuteTask = &MockUCacheExecuteTaskReturnERR;
    TaskRequest tReq{};
    tReq.type = TaskType::MIGRATION_STRATEGY;
    UCacheOutStream out{};
    out << tReq;
    UbseByteBuffer req = {.data = out.GetBufferPointer(), .len = out.GetSize(), .freeFunc = nullptr};
    UbseByteBuffer resp{};
    ProcessTask(req, resp);
    TaskResponse tResp{};
    {
        UCacheInStream in(resp.data, resp.len);
        in >> tResp;
    }
    EXPECT_EQ(tResp.resCode, TURBO_EXECUTE_ERROR);
    delete[] req.data;
}

TEST_F(AgentTaskProcessorTest, ProcessTask_TEST_OK)
{
    TurboRuntimeManager::ucacheExecuteTask = &MockUCacheExecuteTaskReturnOK;
    TaskRequest tReq{};
    tReq.type = TaskType::MIGRATION_STRATEGY;
    UCacheOutStream out{};
    out << tReq;
    UbseByteBuffer req = {.data = out.GetBufferPointer(), .len = out.GetSize(), .freeFunc = nullptr};
    UbseByteBuffer resp{};
    ProcessTask(req, resp);
    TaskResponse tResp{};
    {
        UCacheInStream in(resp.data, resp.len);
        in >> tResp;
    }
    EXPECT_EQ(tResp.resCode, UCACHE_OK);
    delete[] req.data;
}

TEST_F(AgentTaskProcessorTest, FreeMemory_TEST)
{
    uint8_t* data = new uint8_t[100];
    data[0] = 123;
    ASSERT_NO_THROW(FreeMemory(data));
}