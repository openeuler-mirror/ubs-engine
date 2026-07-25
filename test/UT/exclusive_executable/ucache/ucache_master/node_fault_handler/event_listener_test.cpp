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

#include "gtest/gtest.h"
#include "mockcpp/mokc.h"

#define private public

#include "event_listener.h"

#include "ubse_logger.h"
#include "ubse_ras.h"
#include "event_handler.h"
#include "ucache_config.h"
#include "ucache_error.h"

using namespace std;
using namespace ucache;
using namespace ucache::fault_handler;
using namespace ubse::log;
using namespace ubse::ras;

class EventListenerTest : public ::testing::Test {
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

TEST_F(EventListenerTest, InitTest1)
{
    EventListener obj;
    using RegFn = uint32_t (*)(AlarmHandler);
    MOCKER_CPP(RegisterAlarmFaultHandler, RegFn).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = obj.Init();
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(EventListenerTest, InitTest2)
{
    EventListener obj;
    using RegFn = uint32_t (*)(AlarmHandler);
    MOCKER_CPP(RegisterAlarmFaultHandler, RegFn).stubs().will(returnValue(UCACHE_OK)).then(returnValue(UCACHE_ERR));
    uint32_t ret = obj.Init();
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(EventListenerTest, InitTest3)
{
    EventListener obj;
    using RegFn = uint32_t (*)(AlarmHandler);
    MOCKER_CPP(RegisterAlarmFaultHandler, RegFn)
        .stubs()
        .will(returnValue(UCACHE_OK))
        .then(returnValue(UCACHE_OK))
        .then(returnValue(UCACHE_ERR));
    uint32_t ret = obj.Init();
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(EventListenerTest, InitTest4)
{
    EventListener obj;
    using RegFn = uint32_t (*)(AlarmHandler);
    MOCKER_CPP(RegisterAlarmFaultHandler, RegFn)
        .stubs()
        .will(returnValue(UCACHE_OK))
        .then(returnValue(UCACHE_OK))
        .then(returnValue(UCACHE_OK))
        .then(returnValue(UCACHE_ERR));
    uint32_t ret = obj.Init();
    EXPECT_EQ(ret, UCACHE_ERR);
}

TEST_F(EventListenerTest, InitTest5)
{
    EventListener obj;
    using RegFn = uint32_t (*)(AlarmHandler);
    MOCKER_CPP(RegisterAlarmFaultHandler, RegFn).stubs().will(returnValue(UCACHE_OK));
    uint32_t ret = obj.Init();
    EXPECT_EQ(ret, UCACHE_OK);
}
