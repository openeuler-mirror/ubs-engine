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
#include "adapter_plugins/mti/ubse_mti_def.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "src/adapter_plugins/mti/ubse_mti_interface_default.h"
#include "ubse_timer.h"

namespace syssentry::ut {
using namespace ubse::adapter_plugins::mti;
auto module = std::make_shared<SysSentryModule>();

void TestSysSentryModule::SetUp()
{
    Test::SetUp();
    MOCKER(ubse::timer::UbseTimerHandlerUnregister).stubs().will(ignoreReturnValue());
    UbseRasObserver::GetInstance().worker = std::make_unique<std::thread>();
}

void TestSysSentryModule::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestSysSentryModule, Initialize)
{
    std::shared_ptr<UbseTaskExecutorModule> taskModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskModule));
    MOCKER_CPP(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    ASSERT_EQ(UBSE_ERROR_MODULE_LOAD_FAILED, module->Initialize());
}

TEST_F(TestSysSentryModule, StartWhenRasObserverError)
{
    void (*SetSysSentryFaultReporterStub)(SysSentryModule*) = [](SysSentryModule* This) {
    };
    MOCKER_CPP(&SetSysSentryFaultReporter).stubs().will(invoke(SetSysSentryFaultReporterStub));
    MOCKER_CPP(&UbseRasObserver::Start).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(&UbseRasObserver::UbseConfigSysSentryWithRetry).stubs().will(returnValue(UBSE_OK));
    ASSERT_EQ(UBSE_ERROR, module->Start());
}

TEST_F(TestSysSentryModule, StartWhenRasObserverSuccess)
{
    void (*SetSysSentryFaultReporterStub)(SysSentryModule*) = [](SysSentryModule* This) {
    };
    MOCKER_CPP(&SetSysSentryFaultReporter).stubs().will(invoke(SetSysSentryFaultReporterStub));
    MOCKER_CPP(&UbseRasObserver::Start).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseRasObserver::UbseConfigSysSentryWithRetry).stubs().will(returnValue(UBSE_OK));
    ASSERT_EQ(UBSE_OK, module->Start());
}

TEST_F(TestSysSentryModule, GetEidsWhenLcneModuleIsNull)
{
    std::string clientEid;
    std::string serverEids;
    std::shared_ptr<ubse::mti::UbseLcneModule> nullModule = nullptr;
    MOCKER_CPP(&ubse::context::UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(nullModule));
    auto res = GetEids(clientEid, serverEids);
    ASSERT_EQ(res, UBSE_ERROR_MODULE_LOAD_FAILED);
}

TEST_F(TestSysSentryModule, GetEidsFailWhenGetLocalNodeInfoFail)
{
    std::string clientEid;
    std::string serverEids;
    auto lcneModule = std::make_shared<ubse::mti::UbseLcneModule>();
    UbseMtiInterface& mtiInterface = UbseMtiInterface::GetInstance();
    MOCKER_CPP(&ubse::context::UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));
    MOCKER_CPP_VIRTUAL(mtiInterface, &ubse::adapter_plugins::mti::UbseMtiInterface::GetLocalNodeInfo)
        .stubs()
        .will(returnValue(UBSE_ERROR));
    MOCKER_CPP(ubse::mti::UbseGetLocalNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    auto res = GetEids(clientEid, serverEids);
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestSysSentryModule, GetEidsFailWhenDevVecSizeError)
{
    std::string clientEid;
    std::string serverEids;
    auto lcneModule = std::make_shared<ubse::mti::UbseLcneModule>();
    UbseMtiInterface& mtiInterface = UbseMtiInterface::GetInstance();
    MOCKER_CPP(&ubse::context::UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));
    UbseMtiNodeInfo localNodeInfo{.nodeId = "1", .eid = "192.168.1.1"};
    MOCKER_CPP_VIRTUAL(mtiInterface, &ubse::adapter_plugins::mti::UbseMtiInterface::GetLocalNodeInfo)
        .stubs()
        .with(outBound(localNodeInfo))
        .will(returnValue(UBSE_OK));
    UbseDevName dev1("1-");
    UbseMtiEidGroup info1{.primaryEid = "192.168.1.1"};
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
    UbseDevName dev1("1-1");
    UbseDevName dev2("1-2");
    UbseDevName dev3("2-1");
    UbseDevName dev4("2-2");
    UbseMtiEidGroup info1{.primaryEid = "192.168.1.1"};
    UbseMtiEidGroup info2{.primaryEid = "192.168.1.2"};
    UbseMtiEidGroup info3{.primaryEid = "192.168.1.3"};
    UbseMtiEidGroup info4{.primaryEid = "192.168.1.4"};
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

TEST_F(TestSysSentryModule, SetSysSentryFaultReporter)
{
    MOCKER_CPP(GetEids).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseRasObserver::Start).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseRasObserver::UbseConfigSysSentryWithRetry).stubs().will(returnValue(UBSE_OK));
    auto res = module->Start();
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestSysSentryModule, GetCurNodeCna)
{
    // 获取cpu topo失败
    UbseMtiInterfaceDefault mtiDefault;
    MOCKER_CPP_VIRTUAL(&mtiDefault, &UbseMtiInterfaceDefault::GetClusterCpuTopo).stubs().will(returnValue(UBSE_ERROR));
    std::vector<std::string> busNodeCnas;
    auto ret = GetCurNodeCna(busNodeCnas);
    EXPECT_EQ(ret, UBSE_ERROR);
    // 获取本地节点信息失败
    UbseDevName devName1_1("1-1");
    UbseDevName devName1_2("1-2");
    UbseDevName devName2_1("2-1");
    UbseDevName devName2_2("2-2");
    UbseMtiCpuTopoInfo cpuTopoInfo1_1{.nodeId = 1, .chipId = 1, .busNodeCna = 0x123, .portInfos = {}};
    UbseMtiCpuTopoInfo cpuTopoInfo1_2{.nodeId = 1, .chipId = 2, .busNodeCna = 0x456, .portInfos = {}};
    UbseMtiCpuTopoInfo cpuTopoInfo2_1{.nodeId = 2, .chipId = 1, .busNodeCna = 0x789, .portInfos = {}};
    UbseMtiCpuTopoInfo cpuTopoInfo2_2{.nodeId = 2, .chipId = 2, .busNodeCna = 0xabc, .portInfos = {}};
    UbseMtiCpuTopoInfoMap topo;
    topo[devName1_1] = cpuTopoInfo1_1;
    topo[devName1_2] = cpuTopoInfo1_2;
    topo[devName2_1] = cpuTopoInfo2_1;
    topo[devName2_2] = cpuTopoInfo2_2;
    GlobalMockObject::verify();
    MOCKER_CPP_VIRTUAL(&mtiDefault, &UbseMtiInterfaceDefault::GetClusterCpuTopo)
        .stubs()
        .with(outBound(topo))
        .will(returnValue(UBSE_OK));
    UbseMtiNodeInfo localNodeInfo{.nodeId = "1", .eid = ""};
    MOCKER_CPP_VIRTUAL(&mtiDefault, &UbseMtiInterfaceDefault::GetLocalNodeInfo)
        .stubs()
        .with(outBound(localNodeInfo))
        .will(returnValue(UBSE_ERROR));
    ret = GetCurNodeCna(busNodeCnas);
    EXPECT_EQ(ret, UBSE_ERROR);
    // 成功获取cna
    GlobalMockObject::verify();
    MOCKER_CPP_VIRTUAL(&mtiDefault, &UbseMtiInterfaceDefault::GetClusterCpuTopo)
        .stubs()
        .with(outBound(topo))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(&mtiDefault, &UbseMtiInterfaceDefault::GetLocalNodeInfo)
        .stubs()
        .with(outBound(localNodeInfo))
        .will(returnValue(UBSE_OK));
    ret = GetCurNodeCna(busNodeCnas);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(busNodeCnas.size(), 2);
}
}  // namespace syssentry::ut