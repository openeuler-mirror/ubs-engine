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

#include "ubse_election.h"
#include "event_handler.h"
#include "event_listener.h"
#include "rack_ucache_plugin.h"
#include "ucache_agent.h"
#include "ucache_config.h"
#include "ucache_error.h"
#include "ucache_master.h"

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

using namespace std;
using namespace ucache;
using namespace ubse::election;

class RackUcachePluginTest : public ::testing::Test {
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

namespace ucache {
uint32_t HandleChangeToMaster(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId);
uint32_t HandleChangeToStandby(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId);
uint32_t HandleChangeToAgent(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId);
uint32_t HandleStandbyToMaster(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId);
uint32_t HandleSwitchOver(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId);
uint32_t RegEvent(UbseElectionEventType type, const std::string& handlerName, ElectinHandler HandlerFunction);
uint32_t DeregEvent(UbseElectionEventType type, const std::string& handlerName, ElectinHandler HandlerFunction);
void RegRackElectionEvent();
void DeregRackElectionEvent();
void TryInitUCacheMaster();
} // namespace ucache

TEST_F(RackUcachePluginTest, UbseElectionHandlerTest)
{
    UbseElectionEventType type = UbseElectionEventType::CHANGE_TO_MASTER;
    UBSE_ID_TYPE nodeId = "Node1";
    MOCKER(ucache::master::Init).stubs().will(returnValue(UCACHE_OK));
    uint32_t ret = HandleChangeToMaster(type, nodeId);
    EXPECT_EQ(ret, UCACHE_OK);
    GlobalMockObject::verify();

    MOCKER(ucache::master::Init).stubs().will(returnValue(UCACHE_ERR));
    ret = HandleChangeToMaster(type, nodeId);
    EXPECT_EQ(ret, UCACHE_ERR);
    GlobalMockObject::verify();

    MOCKER(ucache::master::Init).stubs().will(returnValue(UCACHE_OK));
    ret = HandleStandbyToMaster(type, nodeId);
    EXPECT_EQ(ret, UCACHE_OK);

    ret = HandleSwitchOver(type, nodeId);
    EXPECT_EQ(ret, UCACHE_OK);
}

TEST_F(RackUcachePluginTest, HandleNewMasterOnlineTest)
{
    UbseElectionEventType type = UbseElectionEventType::CHANGE_TO_STANDBY;
    UBSE_ID_TYPE nodeId = "Node1";

    uint32_t ret = HandleChangeToStandby(type, nodeId);
    EXPECT_EQ(ret, UCACHE_OK);

    ret = HandleChangeToAgent(type, nodeId);
    EXPECT_EQ(ret, UCACHE_OK);
}

std::atomic<bool> g_masterInitThreadStopFlag{false};

TEST_F(RackUcachePluginTest, TryInitUCacheMasterSuccessTest)
{
    MOCKER(RegRackElectionEvent).stubs();
    MOCKER(ucache::master::Init).stubs().will(returnValue(UCACHE_OK));
    TryInitUCacheMaster();
    MOCKER(UbseGetMasterNodeId).stubs().will(returnValue(UCACHE_ERR)).then(returnValue(UCACHE_OK));
    TryInitUCacheMaster();
    MOCKER(UbseGetCurrentNodeId).stubs().will(returnValue(UCACHE_ERR)).then(returnValue(UCACHE_OK));
    TryInitUCacheMaster();
    GlobalMockObject::verify();
}

TEST_F(RackUcachePluginTest, TryInitUCacheMasterSuccessTest_2)
{
    MOCKER(RegRackElectionEvent).stubs();
    MOCKER(ucache::master::Init).stubs().will(returnValue(UCACHE_ERR)).then(returnValue(UCACHE_OK));
    TryInitUCacheMaster();
    GlobalMockObject::verify();
}

TEST_F(RackUcachePluginTest, RegRackElectionEventTest)
{
    MOCKER(RegEvent).stubs().will(returnValue(UCACHE_OK));
    RegRackElectionEvent();
    GlobalMockObject::verify();

    MOCKER(RegEvent).stubs().will(returnValue(UCACHE_ERR));
    RegRackElectionEvent();
    GlobalMockObject::verify();

    MOCKER(RegEvent).stubs().will(returnValue(UCACHE_OK)).then(returnValue(UCACHE_ERR));
    RegRackElectionEvent();
    GlobalMockObject::verify();
}

TEST_F(RackUcachePluginTest, DeregRackElectionEventTest)
{
    MOCKER(DeregEvent).stubs().will(returnValue(UCACHE_OK));
    DeregRackElectionEvent();
    GlobalMockObject::verify();

    MOCKER(DeregEvent).stubs().will(returnValue(UCACHE_ERR));
    DeregRackElectionEvent();
    GlobalMockObject::verify();

    MOCKER(DeregEvent).stubs().will(returnValue(UCACHE_OK)).then(returnValue(UCACHE_ERR));
    DeregRackElectionEvent();
    GlobalMockObject::verify();
}

TEST_F(RackUcachePluginTest, UbsePluginInitOKTest)
{
    const uint16_t modCode = 888;
    MOCKER(ucache::master::Init).stubs().will(returnValue(UCACHE_OK));
    MOCKER(ucache::agent::Init).stubs().will(returnValue(UCACHE_OK));
    uint32_t ret = UbsePluginInit(modCode);
    EXPECT_EQ(ret, UCACHE_OK);
    UbsePluginDeInit();
}

TEST_F(RackUcachePluginTest, UbsePluginInitFailedTest)
{
    const uint16_t modCode = 888;

    MOCKER(ucache::agent::Init).stubs().will(returnValue(UCACHE_ERR));
    uint32_t ret = UbsePluginInit(modCode);
    EXPECT_EQ(ret, UCACHE_ERR);
    UbsePluginDeInit();

    MOCKER(ucache::fault_handler::EventListener::Init).stubs().will(returnValue(UCACHE_ERR));
    ret = UbsePluginInit(modCode);
    EXPECT_EQ(ret, UCACHE_ERR);
    UbsePluginDeInit();

    MOCKER_CPP(&ucache::UcacheConfig::Initialize, uint32_t(*)(void*, uint16_t)).stubs().will(returnValue(UCACHE_ERR));
    ret = UbsePluginInit(modCode);
    EXPECT_EQ(ret, UCACHE_ERR);
    UbsePluginDeInit();
}
