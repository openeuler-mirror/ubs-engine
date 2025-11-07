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

#include "test_sys_sentry_module.h"
namespace syssentry::ut {
auto module = std::make_shared<SysSentryModule>();

void TestSysSentryModule::SetUp()
{
    Test::SetUp();
}

void TestSysSentryModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestSysSentryModule, Initialize)
{
    ASSERT_EQ(UBSE_OK, module->Initialize());
}

TEST_F(TestSysSentryModule, StartWhenRasObserverError)
{
    void (*InitSysSentryBaseCommandStub)(SysSentryModule *) = [](SysSentryModule *This) {
    };
    void (*InitPanicHandleStub)(SysSentryModule *) = [](SysSentryModule *This) {
    };
    MOCKER_CPP(&SysSentryModule::InitSysSentryBaseCommand).stubs().will(invoke(InitSysSentryBaseCommandStub));
    MOCKER_CPP(&SysSentryModule::InitPanicHandle).stubs().will(invoke(InitPanicHandleStub));
    MOCKER_CPP(&UbseRasObserver::Start).stubs().will(returnValue(UBSE_ERROR));
    ASSERT_EQ(UBSE_ERROR, module->Start());
}

TEST_F(TestSysSentryModule, StartWhenRasObserverSuccess)
{
    void (*InitSysSentryBaseCommandStub)(SysSentryModule *) = [](SysSentryModule *This) {
    };
    void (*InitPanicHandleStub)(SysSentryModule *) = [](SysSentryModule *This) {
    };
    MOCKER_CPP(&SysSentryModule::InitSysSentryBaseCommand).stubs().will(invoke(InitSysSentryBaseCommandStub));
    MOCKER_CPP(&SysSentryModule::InitPanicHandle).stubs().will(invoke(InitPanicHandleStub));
    MOCKER_CPP(&UbseRasObserver::Start).stubs().will(returnValue(UBSE_OK));
    ASSERT_EQ(UBSE_OK, module->Start());
}

TEST_F(TestSysSentryModule, Stop)
{
    MOCKER_CPP(&UbseRasObserver::Stop).stubs();
    module->Stop();
    ASSERT_EQ(UbseRasObserver::GetInstance().stopThread, true);
}

TEST_F(TestSysSentryModule, GetEidsWhenLcneModuleIsNull)
{
    std::string clientEid;
    std::string serverEids;
    std::shared_ptr<ubse::mti::UbseLcneModule> nullModule = nullptr;
    MOCKER_CPP(&ubse::context::UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(nullModule));
    auto res = GetEids(clientEid, serverEids);
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}

TEST_F(TestSysSentryModule, GetEidsFailWhenGetLocalNodeInfoFail)
{
    std::string clientEid;
    std::string serverEids;
    auto lcneModule = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER_CPP(&ubse::context::UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));
    MOCKER_CPP(ubse::mti::UbseGetLocalNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    auto res = GetEids(clientEid, serverEids);
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestSysSentryModule, GetEidsFailWhenDevVecSizeError)
{
    std::string clientEid;
    std::string serverEids;
    auto lcneModule = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER_CPP(&ubse::context::UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));
    ubse::mti::MtiNodeInfo localNodeInfo{.nodeId = "1", .eid = "192.168.1.1"};
    MOCKER_CPP(ubse::mti::UbseGetLocalNodeInfo).stubs().with(outBound(localNodeInfo)).will(returnValue(UBSE_OK));
    ubse::mti::DevName dev1("1-");
    ubse::mti::UbseLcneSocketInfo info1{.primaryEid = "192.168.1.1"};
    lcneModule->allSocketComEid.clear();
    lcneModule->allSocketComEid[dev1] = info1;
    auto res = GetEids(clientEid, serverEids);
    ASSERT_EQ(res, UBSE_ERROR_INVAL);
}

TEST_F(TestSysSentryModule, GetEidsSuccess)
{
    std::string clientEid;
    std::string serverEids;
    auto lcneModule = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER_CPP(&ubse::context::UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));
    ubse::mti::MtiNodeInfo localNodeInfo{.nodeId = "1", .eid = "192.168.1.1"};
    MOCKER_CPP(ubse::mti::UbseGetLocalNodeInfo).stubs().with(outBound(localNodeInfo)).will(returnValue(UBSE_OK));
    ubse::mti::DevName dev1("1-1");
    ubse::mti::DevName dev2("1-2");
    ubse::mti::DevName dev3("2-1");
    ubse::mti::DevName dev4("2-2");
    ubse::mti::UbseLcneSocketInfo info1{.primaryEid = "192.168.1.1"};
    ubse::mti::UbseLcneSocketInfo info2{.primaryEid = "192.168.1.2"};
    ubse::mti::UbseLcneSocketInfo info3{.primaryEid = "192.168.1.3"};
    ubse::mti::UbseLcneSocketInfo info4{.primaryEid = "192.168.1.4"};
    lcneModule->allSocketComEid.clear();
    lcneModule->allSocketComEid[dev1] = info1;
    lcneModule->allSocketComEid[dev2] = info2;
    lcneModule->allSocketComEid[dev3] = info3;
    lcneModule->allSocketComEid[dev4] = info4;
    MOCKER_CPP(&ubse::utils::UbseOsUtil::Exec).stubs().will(returnObjectList(UBSE_ERROR, UBSE_OK, UBSE_OK, UBSE_OK));
    auto res = GetEids(clientEid, serverEids);
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestSysSentryModule, SplitString)
{
    std::string str = "1-2-3";
    std::vector<std::string> expctedRes{"1", "2", "3"};
    auto res = SplitString(str, '-');
    ASSERT_EQ(res, expctedRes);
}

TEST_F(TestSysSentryModule, InitPanicHandle)
{
    MOCKER_CPP(GetEids).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseRasObserver::Start).stubs().will(returnValue(UBSE_OK));
    auto res = module->Start();
    ASSERT_EQ(res, UBSE_OK);
}

}