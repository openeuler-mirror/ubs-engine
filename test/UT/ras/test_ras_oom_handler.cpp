// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#include "test_ras_oom_handler.h"
#include "securec.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_ras_oom_handler.cpp"
#include "ubse_ras_oom_handler.h"

namespace ubse::ras::ut {
using namespace ubse::ras;
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::election;

void TestUbseRasOomHandler::SetUp()
{
    Test::SetUp();
}
void TestUbseRasOomHandler::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestUbseRasOomHandler, TestOomHandler)
{
    int type = 1;
    std::string faultInfo = "1650_{nr_nid:1,nid:[0,-1,-1,-1,-1,-1,-1,-1],sync:1,timeout:30000,reason:2,"
                            "timesec:1741057335,timeusec:469389}";
    MOCKER(ForwardOomEventToManager).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(0, OomHandler(type, faultInfo));
    MOCKER(ForwardOomEventToManager).reset();
}

TEST_F(TestUbseRasOomHandler, TestForwardOomEventToManager)
{
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    auto func1 = &UbseComModule::RpcSend<UbseRasOomMessagePtr, UbseRasOomMessagePtr>;
    MOCKER(func1).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, ForwardOomEventToManager(0, TWO_MB_HUGE_PAGE));
    MOCKER(UbseGetCurrentNodeInfo).reset();
    MOCKER(UbseGetMasterInfo).reset();
    MOCKER(&UbseContext::GetModule<UbseComModule>).reset();
    MOCKER(func1).reset();
}

TEST_F(TestUbseRasOomHandler, TestOomHandlerSmallPageOom)
{
    int type = 1;
    std::string faultInfo = "1650_{nr_nid:1,nid:[0,-1,-1,-1,-1,-1,-1,-1],sync:1,timeout:30000,reason:0,"
                            "timesec:1741057335,timeusec:469389}";
    MOCKER(ForwardOomEventToManager).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(0, OomHandler(type, faultInfo));
    MOCKER(ForwardOomEventToManager).reset();
}

TEST_F(TestUbseRasOomHandler, TestGetFreeMemInfo)
{
    GTEST_SKIP();
    uint64_t memFree;
    int16_t numaId = 1;
    EXPECT_EQ(0, GetFreeMemInfo(numaId, memFree));
}

TEST_F(TestUbseRasOomHandler, TestInitOomWaitTime)
{
    uint64_t memFree;
    int16_t numaId = 1;
    EXPECT_NO_THROW(InitOomWaitTime());
}
} // namespace ubse::ras::ut