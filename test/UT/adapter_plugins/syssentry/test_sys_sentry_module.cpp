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
    ubse::context::g_globalStop = false;
    UbseRasObserver::GetInstance().configSysSentrySuccess = false;
    UbseRasObserver::GetInstance().isSentryMsgMonitorRunning = false;
    UbseRasObserver::GetInstance().stopThread = false;
    UbseRasObserver::GetInstance().worker = std::make_unique<std::thread>();
}

void TestSysSentryModule::TearDown()
{
    Test::TearDown();
    ubse::context::g_globalStop = false;
    GlobalMockObject::verify();
}

TEST_F(TestSysSentryModule, SplitString)
{
    std::string str = "1-2-3";
    std::vector<std::string> expctedRes{"1", "2", "3"};
    auto res = SplitString(str, '-');
    ASSERT_EQ(res, expctedRes);
}

TEST_F(TestSysSentryModule, Initialize)
{
    std::shared_ptr<UbseTaskExecutorModule> taskModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskModule));
    MOCKER_CPP(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    ASSERT_EQ(UBSE_ERROR_MODULE_LOAD_FAILED, module->Initialize());
}

TEST_F(TestSysSentryModule, UnInitialize_NullExecutor)
{
    std::shared_ptr<UbseTaskExecutorModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(nullModule));
    EXPECT_NO_THROW(module->UnInitialize());
}

TEST_F(TestSysSentryModule, UnInitialize_Success)
{
    auto taskModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskModule));
    EXPECT_NO_THROW(module->UnInitialize());
}

TEST_F(TestSysSentryModule, Stop)
{
    UbseRasObserver::GetInstance().stopThread = false;
    module->Stop();
    EXPECT_TRUE(UbseRasObserver::GetInstance().stopThread);
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
    UbseMtiInterface &mtiInterface = UbseMtiInterface::GetInstance();
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
    UbseMtiInterface &mtiInterface = UbseMtiInterface::GetInstance();
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

TEST_F(TestSysSentryModule, GetEids_LocalNodeInfoEmpty)
{
    std::string clientEid;
    std::string serverEids;
    auto lcneModule = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER_CPP(&ubse::context::UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));
    UbseDevName dev1("1-1");
    UbseUrmaEidInfo info1{.primaryEid = "eid1"};
    lcneModule->allSocketComEid.clear();
    lcneModule->allSocketComEid[dev1] = info1;
    auto res = GetEids(clientEid, serverEids);
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestSysSentryModule, GetEidsSuccess)
{
    std::string clientEid;
    std::string serverEids;
    auto lcneModule = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER_CPP(&ubse::context::UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));
    lcneModule->ubseNodeInfo_.nodeId = "1";
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
    ASSERT_FALSE(clientEid.empty());
    ASSERT_FALSE(serverEids.empty());
}

TEST_F(TestSysSentryModule, GetEids_LocalNodeNotInTopo)
{
    std::string clientEid;
    std::string serverEids;
    auto lcneModule = std::make_shared<ubse::mti::UbseLcneModule>();
    MOCKER_CPP(&ubse::context::UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));
    lcneModule->ubseNodeInfo_.nodeId = "999";
    UbseDevName dev1("1-1");
    UbseUrmaEidInfo info1{.primaryEid = "eid1"};
    lcneModule->allSocketComEid.clear();
    lcneModule->allSocketComEid[dev1] = info1;
    auto res = GetEids(clientEid, serverEids);
    ASSERT_EQ(res, UBSE_OK);
    EXPECT_TRUE(clientEid.empty());
}

TEST_F(TestSysSentryModule, GetCurNodeCna)
{
    UbseMtiInterfaceDefault mtiDefault;
    MOCKER_CPP_VIRTUAL(&mtiDefault, &UbseMtiInterfaceDefault::GetClusterCpuTopo).stubs().will(returnValue(UBSE_ERROR));
    std::vector<std::string> busNodeCnas;
    auto ret = GetCurNodeCna(busNodeCnas);
    EXPECT_EQ(ret, UBSE_ERROR);

    UbseDevName devName1_1("1-1");
    UbseDevName devName1_2("1-2");
    UbseDevName devName2_1("2-1");
    UbseDevName devName2_2("2-2");
    UbseMtiCpuTopoInfo cpuTopoInfo1_1{.slotId = 1, .socketId = 1, .busNodeCna = 0x123, .portInfos = {}};
    UbseMtiCpuTopoInfo cpuTopoInfo1_2{.slotId = 1, .socketId = 2, .busNodeCna = 0x456, .portInfos = {}};
    UbseMtiCpuTopoInfo cpuTopoInfo2_1{.slotId = 2, .socketId = 1, .busNodeCna = 0x789, .portInfos = {}};
    UbseMtiCpuTopoInfo cpuTopoInfo2_2{.slotId = 2, .socketId = 2, .busNodeCna = 0xabc, .portInfos = {}};
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

TEST_F(TestSysSentryModule, GetCurNodeCna_SplitDevNameFail)
{
    auto &mtiInterface = UbseMtiInterface::GetInstance();
    UbseMtiCpuTopoInfoMap topo;
    UbseDevName badDev;
    badDev.devName = "node1";
    topo[badDev] = {.slotId = 1, .busNodeCna = 0x123};
    MOCKER_CPP_VIRTUAL(&mtiInterface, &ubse::adapter_plugins::mti::UbseMtiInterface::GetClusterCpuTopo)
        .stubs()
        .with(outBound(topo))
        .will(returnValue(UBSE_OK));
    UbseMtiNodeInfo localNodeInfo{.nodeId = "1", .eid = ""};
    MOCKER_CPP_VIRTUAL(&mtiInterface, &ubse::adapter_plugins::mti::UbseMtiInterface::GetLocalNodeInfo)
        .stubs()
        .with(outBound(localNodeInfo))
        .will(returnValue(UBSE_OK));
    std::vector<std::string> busNodeCnas;
    auto ret = GetCurNodeCna(busNodeCnas);
    EXPECT_EQ(ret, UBSE_ERROR_AGAIN);
}

TEST_F(TestSysSentryModule, LinkStrings_EmptyResult)
{
    std::string result;
    LinkStrings(result, ";", {"a", "b", "c"});
    EXPECT_EQ(result, "a;b;c");
}

TEST_F(TestSysSentryModule, LinkStrings_SingleItem)
{
    std::string result;
    LinkStrings(result, ",", {"only"});
    EXPECT_EQ(result, "only");
}

TEST_F(TestSysSentryModule, LinkStrings_EmptyList)
{
    std::string result = "existing";
    LinkStrings(result, ";", {});
    EXPECT_EQ(result, "existing");
}

TEST_F(TestSysSentryModule, ShellEscape_Empty)
{
    EXPECT_EQ(ShellEscape(""), "''");
}

TEST_F(TestSysSentryModule, ShellEscape_Normal)
{
    EXPECT_EQ(ShellEscape("hello"), "'hello'");
}

TEST_F(TestSysSentryModule, ShellEscape_WithQuote)
{
    std::string result = ShellEscape("it's");
    EXPECT_EQ(result, "'it'\\''s'");
}

TEST_F(TestSysSentryModule, ShellEscape_SpecialChars)
{
    std::string result = ShellEscape("a;b`c");
    EXPECT_EQ(result, "'a;b`c'");
}

TEST_F(TestSysSentryModule, ProcessEids_DevNameSplitFail)
{
    UbseDevName dev;
    dev.devName = "node1";
    UbseUrmaEidInfo info{.primaryEid = "eid1"};
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap = {{dev, info}};
    std::unordered_map<std::string, std::vector<std::string>> eids;
    std::vector<std::string> eidGroup;

    auto ret = ProcessEids(socketInfoMap, "1", eids, eidGroup);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestSysSentryModule, ProcessEids_SingleNode)
{
    UbseDevName dev1("1-1");
    UbseDevName dev2("1-2");
    UbseUrmaEidInfo info1{.primaryEid = "eid_a"};
    UbseUrmaEidInfo info2{.primaryEid = "eid_b"};
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap = {{dev1, info1}, {dev2, info2}};
    std::unordered_map<std::string, std::vector<std::string>> eids;
    std::vector<std::string> eidGroup;

    auto ret = ProcessEids(socketInfoMap, "1", eids, eidGroup);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(eidGroup.size(), 2);
    EXPECT_EQ(eidGroup[0], "eid_a");
    EXPECT_EQ(eidGroup[1], "eid_b");
}

TEST_F(TestSysSentryModule, ProcessEids_MultiNodeWithMerge)
{
    UbseDevName dev1_1("1-1");
    UbseDevName dev2_1("2-1");
    UbseUrmaEidInfo info1{.primaryEid = "eid_a"};
    UbseUrmaEidInfo info2{.primaryEid = "eid_b"};
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap = {{dev1_1, info1}, {dev2_1, info2}};
    std::unordered_map<std::string, std::vector<std::string>> eids;
    std::vector<std::string> eidGroup;

    auto ret = ProcessEids(socketInfoMap, "1", eids, eidGroup);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(eidGroup.size(), 1);
    EXPECT_NE(eidGroup[0].find("eid_a"), std::string::npos);
    EXPECT_NE(eidGroup[0].find("eid_b"), std::string::npos);
}

TEST_F(TestSysSentryModule, ProcessEids_DuplicateEidSkipped)
{
    UbseDevName dev1_1("1-1");
    UbseDevName dev1_2("1-2");
    UbseDevName dev2_1("2-1");
    UbseUrmaEidInfo info1{.primaryEid = "eid_a"};
    UbseUrmaEidInfo info2{.primaryEid = "eid_b"};
    UbseUrmaEidInfo info3{.primaryEid = "eid_a"};
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap = {{dev1_1, info1}, {dev1_2, info2}, {dev2_1, info3}};
    std::unordered_map<std::string, std::vector<std::string>> eids;
    std::vector<std::string> eidGroup;

    auto ret = ProcessEids(socketInfoMap, "1", eids, eidGroup);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(eidGroup.size(), 2);
    EXPECT_EQ(eidGroup[0], "eid_a");
    EXPECT_EQ(eidGroup[1], "eid_b");
}

TEST_F(TestSysSentryModule, ProcessEids_EmptyEidSkipped)
{
    UbseDevName dev1_1("1-1");
    UbseDevName dev2_1("2-1");
    UbseUrmaEidInfo info1{.primaryEid = "eid_a"};
    UbseUrmaEidInfo info2{.primaryEid = ""};
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap = {{dev1_1, info1}, {dev2_1, info2}};
    std::unordered_map<std::string, std::vector<std::string>> eids;
    std::vector<std::string> eidGroup;

    auto ret = ProcessEids(socketInfoMap, "1", eids, eidGroup);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestSysSentryModule, ProcessEids_NodeCountMismatch)
{
    UbseDevName dev1_1("1-1");
    UbseDevName dev1_2("1-2");
    UbseDevName dev2_1("2-1");
    UbseUrmaEidInfo info1{.primaryEid = "eid1"};
    UbseUrmaEidInfo info2{.primaryEid = "eid2"};
    UbseUrmaEidInfo info3{.primaryEid = "eid3"};
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap = {{dev1_1, info1}, {dev1_2, info2}, {dev2_1, info3}};
    std::unordered_map<std::string, std::vector<std::string>> eids;
    std::vector<std::string> eidGroup;

    auto ret = ProcessEids(socketInfoMap, "1", eids, eidGroup);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestSysSentryModule, ProcessEids_RemoteNodeMoreEids)
{
    UbseDevName dev1_1("1-1");
    UbseDevName dev2_1("2-1");
    UbseDevName dev2_2("2-2");
    UbseUrmaEidInfo info1{.primaryEid = "eid1"};
    UbseUrmaEidInfo info2{.primaryEid = "eid2"};
    UbseUrmaEidInfo info3{.primaryEid = "eid3"};
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap = {{dev1_1, info1}, {dev2_1, info2}, {dev2_2, info3}};
    std::unordered_map<std::string, std::vector<std::string>> eids;
    std::vector<std::string> eidGroup;

    auto ret = ProcessEids(socketInfoMap, "1", eids, eidGroup);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(eidGroup.size(), 2);
    EXPECT_EQ(eidGroup[0], "eid1,eid2");
    EXPECT_EQ(eidGroup[1], "eid3");
}

static UbseResult StubGetEidsOk(std::string& clientEid, std::string& serverEids)
{
    clientEid = "client_eid";
    serverEids = "server_eid";
    return UBSE_OK;
}

static UbseResult StubGetEidsQuote(std::string &clientEid, std::string &serverEids)
{
    clientEid = "client\"eid";
    serverEids = "server_eid";
    return UBSE_OK;
}

TEST_F(TestSysSentryModule, SetSysSentryFaultReporter_GetEidsFails)
{
    MOCKER_CPP(GetEids).stubs().will(returnValue(UBSE_ERROR));
    auto ret = SetSysSentryFaultReporter();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestSysSentryModule, SetSysSentryFaultReporter_EidContainsQuote)
{
    MOCKER_CPP(GetEids).stubs().will(invoke(StubGetEidsQuote));
    auto ret = SetSysSentryFaultReporter();
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestSysSentryModule, SetSysSentryFaultReporter_ExecFails)
{
    MOCKER_CPP(GetEids).stubs().will(invoke(StubGetEidsOk));
    MOCKER_CPP(&ubse::utils::UbseOsUtil::Exec).stubs().will(returnValue(UBSE_ERROR));
    auto ret = SetSysSentryFaultReporter();
    EXPECT_EQ(ret, UBSE_RAS_ERROR_SET_SENTRY_REPORTER);
}

TEST_F(TestSysSentryModule, SetSysSentryFaultReporter_Success)
{
    MOCKER_CPP(GetEids).stubs().will(invoke(StubGetEidsOk));
    MOCKER_CPP(&ubse::utils::UbseOsUtil::Exec).stubs().will(returnValue(UBSE_OK));
    auto ret = SetSysSentryFaultReporter();
    EXPECT_EQ(ret, UBSE_OK);
}

} // namespace syssentry::ut
