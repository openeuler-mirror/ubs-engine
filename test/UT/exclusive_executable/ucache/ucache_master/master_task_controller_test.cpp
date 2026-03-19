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

#include <vector>
#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#define private public
#include "ubse_com.h"
#include "ubse_logger.h"
#include "ucache_config.h"
#include "ucache_error.h"
#include "ucache_serialize.h"
#include "master_task_controller.h"

using namespace ubse::log;
using namespace ubse::com;
using namespace ucache::serialize;
using namespace std;
using namespace ucache;
using namespace turbo::ucache;

namespace ucache::master {
    void OnTaskResult(void *ctx, const UbseByteBuffer &respData, uint32_t resCode);
}

class master_task_controllerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        cout << "[Phase SetUp Begin]" << endl;
        cout << "[Phase SetUp End]" << endl;
    }
    void TearDown() override
    {
        cout << "[Phase TearDown Begin]" << endl;
        GlobalMockObject::verify();
        cout << "[Phase TearDown End]" << endl;
    }
};

TEST_F(master_task_controllerTest, OnTaskResult_Invalid_01)
{
    UbseByteBuffer resp{};
    TaskResponse taskRes{};
    taskRes.resCode = 0;
    UCacheOutStream out;
    out << taskRes;
    resp.data = NULL;
    resp.len = 1;

    ucache::master::OnTaskResult(&taskRes, resp, taskRes.resCode);
}

TEST_F(master_task_controllerTest, OnTaskResult_Invalid_02)
{
    UbseByteBuffer resp{};
    TaskResponse taskRes{};
    taskRes.resCode = 0;
    UCacheOutStream out;
    out << taskRes;
    resp.data = out.GetBufferPointer();
    resp.len = 0;

    ucache::master::OnTaskResult(&taskRes, resp, taskRes.resCode);
}

TEST_F(master_task_controllerTest, OnTaskResult_Invalid_03)
{
    UbseByteBuffer resp{};
    TaskResponse taskRes{};
    taskRes.resCode = 1;
    UCacheOutStream out;
    out << taskRes;
    resp.data = out.GetBufferPointer();
    resp.len = out.GetSize();

    ucache::master::OnTaskResult(&taskRes, resp, taskRes.resCode);
}

TEST_F(master_task_controllerTest, OnTaskResult_Invalid_04)
{
    UbseByteBuffer resp{};
    TaskResponse taskRes{};
    taskRes.resCode = 1;
    UCacheOutStream out;
    out << taskRes;
    resp.data = (uint8_t*)malloc(sizeof(uint8_t));
    resp.len = 1;

    ucache::master::OnTaskResult(&taskRes, resp, taskRes.resCode);
}

TEST_F(master_task_controllerTest, OnTaskResult_Valid)
{
    UbseByteBuffer resp{};
    TaskResponse taskRes{};
    taskRes.resCode = 0;
    UCacheOutStream out;
    out << taskRes;
    resp.data = out.GetBufferPointer();
    resp.len = out.GetSize();

    ucache::master::OnTaskResult(&taskRes, resp, taskRes.resCode);
}

TEST_F(master_task_controllerTest, DispatchTask_Invalid)
{
    TaskRequest tReq;
    tReq.type = TaskType::INVALID;
    TaskResponse tResp;
    std::string destNode;

    uint32_t ret = ucache::master::DispatchTask(tReq, tResp, destNode);
    EXPECT_EQ(ret, INVALID_TASK_TYPE);
}

TEST_F(master_task_controllerTest, DispatchTask_Valid_01)
{
    TaskRequest tReq;
    tReq.type = TaskType::COLLECT_RESOURCE;
    TaskResponse tResp;
    std::string destNode;

    MOCKER(UbseRpcSend)
        .stubs()
        .will(returnValue(0));

    uint32_t ret = ucache::master::DispatchTask(tReq, tResp, destNode);
    EXPECT_EQ(ret, UCACHE_OK);
}

TEST_F(master_task_controllerTest, DispatchTask_Valid_02)
{
    TaskRequest tReq;
    tReq.type = TaskType::MIGRATION_STRATEGY;
    TaskResponse tResp;
    std::string destNode;

    MOCKER(UbseRpcSend)
        .stubs()
        .will(returnValue(0));

    uint32_t ret = ucache::master::DispatchTask(tReq, tResp, destNode);
    EXPECT_EQ(ret, UCACHE_OK);
}