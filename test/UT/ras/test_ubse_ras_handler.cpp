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

#include "test_ubse_ras_handler.h"
#include <ubse_event.h>
#include "ubse_com_module.h"
#include "ubse_error.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller_module.h"
#include "ubse_ras_handler.cpp"

namespace ubse::ras::ut {
using namespace ubse::com;

void TestUbseRasHandler::SetUp()
{
    Test::SetUp();
}

void TestUbseRasHandler::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseRasHandler, GetInstanceSame)
{
    auto& instance1 = UbseRasHandler::GetInstance();
    auto& instance2 = UbseRasHandler::GetInstance();
    ASSERT_EQ(&instance1, &instance2);
}

const ALARM_FAULT_TYPE g_alarmFaultType = ALARM_OOM_EVENT;
const std::string g_message = "test_oom";
TEST_F(TestUbseRasHandler, ReportAckToSysSentryWhenStrncpyFail)
{
    MOCKER_CPP(strncpy_s).stubs().will(returnValue(EINVAL));
    auto res = ReportAckToSysSentry(g_alarmFaultType, g_message);
    ASSERT_EQ(res, UBSE_ERROR_IO);
}

std::shared_ptr<int> dummyPtr = std::make_shared<int>(1234);
void* xalarmHandle = static_cast<void*>(dummyPtr.get());
void* nullVoid = static_cast<char*>(nullptr);
TEST_F(TestUbseRasHandler, ReportAckToSysSentryWhenDlopenFail)
{
    MOCKER_CPP(dlopen).stubs().will(returnValue(nullVoid));
    auto res = ReportAckToSysSentry(g_alarmFaultType, g_message);
    ASSERT_EQ(res, UBSE_RAS_ERROR_DLOPEN_XALARMD);
}

TEST_F(TestUbseRasHandler, ReportAckToSysSentryWhenDlsymFail)
{
    MOCKER_CPP(dlopen).stubs().will(returnValue(xalarmHandle));
    MOCKER_CPP(dlsym).stubs().will(returnValue(nullVoid));
    MOCKER_CPP(dlclose).stubs().will(returnValue(0));
    auto res = ReportAckToSysSentry(g_alarmFaultType, g_message);
    ASSERT_EQ(res, UBSE_RAS_ERROR_DLSYM_XALARMD);
}

TEST_F(TestUbseRasHandler, ReportAckToSysSentryReportWhenReportFuncFail)
{
    GTEST_SKIP();
    // XalarmReportEventFunc func = [](unsigned short, char *) -> int {
    //     return -1;
    // };
    MOCKER_CPP(dlopen).stubs().will(returnValue(xalarmHandle));
    // MOCKER_CPP(dlsym).stubs().will(returnValue((void *)func));
    MOCKER_CPP(dlclose).stubs().will(returnValue(0));
    auto res = ReportAckToSysSentry(g_alarmFaultType, g_message);
    ASSERT_EQ(res, UBSE_RAS_ERROR_REPORT_TO_XALARMD);
}

TEST_F(TestUbseRasHandler, ReportAckToSysSentryReportWhenReportFuncSuccess)
{
    GTEST_SKIP();
    // XalarmReportEventFunc func = [](unsigned short, char *) -> int {
    //     return 0;
    // };
    MOCKER_CPP(dlopen).stubs().will(returnValue(xalarmHandle));
    // MOCKER_CPP(dlsym).stubs().will(returnValue((void *)func));
    MOCKER_CPP(dlclose).stubs().will(returnValue(0));
    auto res = ReportAckToSysSentry(g_alarmFaultType, g_message);
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, SwitchRoleWhenMasterFaultWhenGetMasterInfoFail)
{
    auto& handler = UbseRasHandler::GetInstance();
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(IsMemInitFinished).stubs().will(returnValue(false));
    MOCKER_CPP(ubse::election::UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    auto res = handler.HandlePanicAndRebootFault(ALARM_REBOOT_EVENT, "");
    ASSERT_EQ(res, UBSE_RAS_IS_NOT_MASTER_OR_MEM_IS_NOT_INIT);
}

TEST_F(TestUbseRasHandler, SwitchRoleWhenMasterFaultWhenGetCurrentInfoFail)
{
    auto& handler = UbseRasHandler::GetInstance();
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(IsMemInitFinished).stubs().will(returnValue(false));
    UbseRoleInfo masterInfo{};
    MOCKER_CPP(ubse::election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    auto res = handler.HandlePanicAndRebootFault(ALARM_REBOOT_EVENT, "");
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, SwitchRoleWhenMasterFaultWhenGetElectionModuleFail)
{
    auto& handler = UbseRasHandler::GetInstance();
    UbseRoleInfo masterInfo("1", ELECTION_ROLE_MASTER, 1);
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_STANDBY, 1);
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(IsMemInitFinished).stubs().will(returnValue(false));
    MOCKER_CPP(ubse::election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    std::shared_ptr<UbseElectionModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule));
    auto res = handler.HandlePanicAndRebootFault(ALARM_REBOOT_EVENT, "");
    ASSERT_EQ(res, UBSE_RAS_IS_NOT_MASTER_OR_MEM_IS_NOT_INIT);
}

TEST_F(TestUbseRasHandler, SwitchRoleWhenMasterFaultSuccess)
{
    auto& handler = UbseRasHandler::GetInstance();
    UbseRoleInfo masterInfo("1", ELECTION_ROLE_MASTER, 1);
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_STANDBY, 1);
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(IsMemInitFinished).stubs().will(returnValue(false));
    MOCKER_CPP(ubse::election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseElectionModule::SwitchMasterFromStandby).stubs();
    auto res = handler.HandlePanicAndRebootFault(ALARM_REBOOT_EVENT, "");
    ASSERT_EQ(res, UBSE_RAS_IS_NOT_MASTER_OR_MEM_IS_NOT_INIT);
}

TEST_F(TestUbseRasHandler, StartRasHandlerWhenComModuleIsNull)
{
    auto& handler = UbseRasHandler::GetInstance();
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule));
    auto res = handler.StartRasHandler();
    ASSERT_EQ(res, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasHandler, StartRasHandlerWhenRegRpcServiceFail)
{
    auto& handler = UbseRasHandler::GetInstance();
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    UbseResult (UbseComModule::*func)(UbseComBaseMessageHandlerPtr&) =
        &ubse::com::UbseComModule::RegRpcService<UbseRasMessage, UbseRasMessage>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
    auto res = handler.StartRasHandler();
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, StartRasHandlerSuccess)
{
    auto& handler = UbseRasHandler::GetInstance();
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    UbseResult (UbseComModule::*func)(UbseComBaseMessageHandlerPtr&) =
        &ubse::com::UbseComModule::RegRpcService<UbseRasMessage, UbseRasMessage>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(event::UbseSubEvent).stubs().will(returnValue(UBSE_OK));
    auto res = handler.StartRasHandler();
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, ExecuteFaultHandlerWhenNoHandlerRegister)
{
    auto& handler = UbseRasHandler::GetInstance();
    ALARM_FAULT_TYPE type = ALARM_OOM_EVENT;
    std::string faultInfo = "1";
    handler.faultHandlerMap.clear();
    auto res = handler.ExecuteFaultHandler(type, faultInfo, "1");
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, ExecuteFaultHandlerSuccess)
{
    auto& handler = UbseRasHandler::GetInstance();
    ALARM_FAULT_TYPE type = ALARM_OOM_EVENT;
    std::string faultInfo = "1";
    handler.faultHandlerMap.clear();
    handler.faultHandlerMap[type][AlarmHandlerPriority::HIGH].emplace_back("test1", (AlarmFaultHandler) nullptr);
    AlarmFaultHandler func = [](ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo) -> uint32_t {
        return UBSE_OK;
    };
    handler.faultHandlerMap[type][AlarmHandlerPriority::HIGH].emplace_back("test2", func);
    std::string msgId = "1";
    auto res = handler.ExecuteFaultHandler(type, faultInfo, msgId);
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, IsMemInitFinishedWhenNodeIdIsEmpty)
{
    UbseNodeInfo info;
    info.nodeId = "";
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(info));
    auto res = IsMemInitFinished();
    ASSERT_EQ(res, false);
}

TEST_F(TestUbseRasHandler, IsMemInitFinishedWhenNodeNotWorking)
{
    UbseNodeInfo info;
    info.nodeId = "1";
    info.clusterState = UbseNodeClusterState::UBSE_NODE_INIT;
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(info));
    auto res = IsMemInitFinished();
    ASSERT_EQ(res, false);
}

TEST_F(TestUbseRasHandler, IsMemInitFinishedSuccess)
{
    UbseNodeInfo info;
    info.nodeId = "1";
    info.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(info));
    auto res = IsMemInitFinished();
    ASSERT_EQ(res, true);
}

TEST_F(TestUbseRasHandler, HandleCnaAndEidMsgWhenSplitFail)
{
    std::string faultInfo = "1";
    std::string faultNodeId = "1";
    MOCKER_CPP(ubse::utils::SplitSysSentryMsg).stubs().will(returnValue(UBSE_ERROR));
    auto res = HandleCnaAndEidMsg(faultInfo, faultNodeId);
    ASSERT_EQ(res, UBSE_RAS_PANIC_REBOOT_MSG_INVALID);
}

TEST_F(TestUbseRasHandler, HandleCnaAndEidMsgWhenNodeIdIsEmpty)
{
    std::string faultInfo = "1";
    std::string faultNodeId = "1";
    std::string emptyStr = "";
    MOCKER_CPP(ubse::utils::SplitSysSentryMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(QueryNodeIdByEid).stubs().will(returnValue(emptyStr));
    auto res = HandleCnaAndEidMsg(faultInfo, faultNodeId);
    ASSERT_EQ(res, UBSE_RAS_ERROR_QUERY_NODE_BY_EID);
}

TEST_F(TestUbseRasHandler, HandleCnaAndEidMsgSuccess)
{
    std::string faultInfo = "1";
    std::string faultNodeId = "1";
    MOCKER_CPP(ubse::utils::SplitSysSentryMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(QueryNodeIdByEid).stubs().will(returnValue(faultNodeId));
    auto res = HandleCnaAndEidMsg(faultInfo, faultNodeId);
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, QueryNodeIdByEidWhenLcneModuleIsNull)
{
    std::string emptyStr = "";
    std::shared_ptr<ubse::mti::UbseLcneModule> nullLcne;
    MOCKER_CPP(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(nullLcne));
    auto res = QueryNodeIdByEid(emptyStr);
    ASSERT_EQ(res, emptyStr);
}

TEST_F(TestUbseRasHandler, QueryNodeIdByEidWhenDevVecSizeError)
{
    std::string emptyStr = "";
    auto lcneModule = std::make_shared<ubse::mti::UbseLcneModule>();
    adapter_plugins::mti::UbseMtiIouInfo dev1{"1", "1", ""};
    adapter_plugins::mti::UbseMtiIouInfo dev2{"1", "", ""};
    adapter_plugins::mti::UbseMtiEidGroup info1{.primaryEid = "192.168.1.1"};
    adapter_plugins::mti::UbseMtiEidGroup info2{.primaryEid = "192.168.1.2"};
    lcneModule->allSocketComEid.clear();
    lcneModule->allSocketComEid[dev1] = info1;
    lcneModule->allSocketComEid[dev2] = info2;
    MOCKER_CPP(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));
    auto res = QueryNodeIdByEid("1");
    ASSERT_EQ(res, emptyStr);
}

TEST_F(TestUbseRasHandler, QueryNodeIdByEidWhenNodeIdIsNotExist)
{
    std::string emptyStr = "";
    auto lcneModule = std::make_shared<ubse::mti::UbseLcneModule>();
    adapter_plugins::mti::UbseMtiIouInfo dev1{"1", "1", ""};
    adapter_plugins::mti::UbseMtiIouInfo dev2{"1", "2", ""};
    adapter_plugins::mti::UbseMtiEidGroup info1{.primaryEid = "192.168.1.1"};
    adapter_plugins::mti::UbseMtiEidGroup info2{.primaryEid = "192.168.1.2"};
    lcneModule->allSocketComEid.clear();
    lcneModule->allSocketComEid[dev1] = info1;
    lcneModule->allSocketComEid[dev2] = info2;
    MOCKER_CPP(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));
    auto res = QueryNodeIdByEid("2");
    ASSERT_EQ(res, emptyStr);
}

TEST_F(TestUbseRasHandler, QueryNodeIdByEidSuccess)
{
    std::string emptyStr = "";
    auto lcneModule = std::make_shared<ubse::mti::UbseLcneModule>();
    adapter_plugins::mti::UbseMtiIouInfo dev1{"1", "1", ""};
    adapter_plugins::mti::UbseMtiIouInfo dev2{"1", "2", ""};
    adapter_plugins::mti::UbseMtiEidGroup info1{.primaryEid = "192.168.1.1"};
    adapter_plugins::mti::UbseMtiEidGroup info2{.primaryEid = "192.168.1.2"};
    lcneModule->allSocketComEid.clear();
    lcneModule->allSocketComEid[dev1] = info1;
    lcneModule->allSocketComEid[dev2] = info2;
    MOCKER_CPP(&UbseContext::GetModule<ubse::mti::UbseLcneModule>).stubs().will(returnValue(lcneModule));
    auto res = QueryNodeIdByEid("192.168.1.1");
    ASSERT_EQ(res, "1");
}
/*
TEST_F(TestUbseRasHandler, HandleBMCFaultWhenGetCurrentNodeInfoFail)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_REBOOT_EVENT, .pucParas = "123456"};
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultWhenGetMasterInfoFail)
{
    auto& handle = UbseRasHandler::GetInstance();
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_STANDBY, 1);
    alarm_msg msg{.usAlarmId = ALARM_REBOOT_EVENT, .pucParas = "123456"};
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultWhenGetComModuleFail)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_REBOOT_EVENT, .pucParas = "123456"};
    UbseRoleInfo masterInfo("1", ELECTION_ROLE_MASTER, 1);
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_STANDBY, 1);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<UbseComModule>(nullptr)));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultSuccess)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_REBOOT_EVENT, .pucParas = "123456"};
    UbseRoleInfo masterInfo("1", ELECTION_ROLE_MASTER, 1);
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_STANDBY, 1);
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseRasMessagePtr, UbseRasMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(ReportAckToSysSentry).stubs().will(returnValue(UBSE_OK));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultWhenMsgDuplication)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_REBOOT_EVENT, .pucParas = "123456"};
    UbseRoleInfo masterInfo("1", ELECTION_ROLE_MASTER, 1);
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_STANDBY, 1);
    std::shared_ptr<UbseComModule> ubseComModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(ubseComModule));
    auto func = &UbseComModule::RpcSend<UbseRasMessagePtr, UbseRasMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(ReportAckToSysSentry).stubs().will(returnValue(UBSE_OK));
    auto res1 = handle.NodeFaultHandle(&msg);
    auto res2 = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res1, UBSE_OK);
    ASSERT_EQ(res2, UBSE_OK);
}
*/

TEST_F(TestUbseRasHandler, HandleOomFaultWhenMsgIsNull)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg* nullMsg = nullptr;
    auto res = handle.HandleOomFault(nullMsg);
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}
/*
TEST_F(TestUbseRasHandler, HandleOomFaultWhenMsgVecSizeError)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_OOM_EVENT, .pucParas = "HandleOomFaultMsgVecSizeError"};
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasHandler, HandleOomFaultSuccess)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_OOM_EVENT,
                  .pucParas = "1652_{nr_nid:1,nid:[0,-1,-1,-1,-1,-1,-1,-1],sync:1,timeout:30000,reason:2}"};
    MOCKER_CPP(ReportAckToSysSentry).stubs().will(returnValue(UBSE_OK));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, HandleOomFaultWhenMsgDuplication)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_OOM_EVENT,
                  .pucParas = "1653_{nr_nid:1,nid:[0,-1,-1,-1,-1,-1,-1,-1],sync:1,timeout:30000,reason:2}"};

    MOCKER_CPP(ReportAckToSysSentry).stubs().will(returnValue(UBSE_OK));
    auto res1 = handle.NodeFaultHandle(&msg);
    handle.AddProcessedMsgId("1653");
    auto res2 = handle.NodeFaultHandle(&msg);
    handle.ClearAllMsgId();
    ASSERT_EQ(res1, UBSE_OK);
    ASSERT_EQ(res2, UBSE_RAS_ERROR_MSG_DUPLICATION);
}

TEST_F(TestUbseRasHandler, HandlePanicAndRebootFaultWhenHandleCnaFail)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_PANIC_EVENT, .pucParas = "1_{cna:1,eid:0000:0000:0000:1000:0010:0000:DF00:0460}"};
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_ERROR));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, HandlePanicAndRebootFaultWhenGetCurrentNodeInfoFail)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_PANIC_EVENT, .pucParas = "1_{cna:1,eid:0000:0000:0000:1000:0010:0000:DF00:0460}"};
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(SwitchRoleWhenMasterFault).stubs();
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, HandlePanicAndRebootFaultWhenNodeIsNotMaster)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_PANIC_EVENT, .pucParas = "1_{cna:1,eid:0000:0000:0000:1000:0010:0000:DF00:0460}"};
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(SwitchRoleWhenMasterFault).stubs();
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_STANDBY, 1);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_RAS_IS_NOT_MASTER_OR_MEM_IS_NOT_INIT);
}

TEST_F(TestUbseRasHandler, HandlePanicAndRebootFaultSuccess)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_PANIC_EVENT, .pucParas = "1_{cna:1,eid:0000:0000:0000:1000:0010:0000:DF00:0460}"};
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(SwitchRoleWhenMasterFault).stubs();
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_MASTER, 1);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(IsMemInitFinished).stubs().will(returnValue(true));
    handle.nodeStateHandler = [](const std::string& faultInfo) {
    };
    MOCKER_CPP(ReportAckToSysSentry).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseNodeControllerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseNodeControllerModule>()));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, HandlePanicAndRebootFaultWhenMsgDuplication)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_PANIC_EVENT,
                  .pucParas = "123_{cna:1,eid:0000:0000:0000:1000:0010:0000:DF00:0460}"};
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(SwitchRoleWhenMasterFault).stubs();
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_MASTER, 1);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(IsMemInitFinished).stubs().will(returnValue(true));
    handle.nodeStateHandler = [](const std::string& faultInfo) {
    };
    MOCKER_CPP(ReportAckToSysSentry).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseNodeControllerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseNodeControllerModule>()));
    MOCKER_CPP(&UbseContext::GetModule<UbseNodeControllerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseNodeControllerModule>()));
    auto res1 = handle.NodeFaultHandle(&msg);
    auto res2 = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res1, UBSE_OK);
    ASSERT_EQ(res2, UBSE_OK);
}

TEST_F(TestUbseRasHandler, HandleRebootFaultSuccess)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_KERNEL_REBOOT_EVENT,
                  .pucParas = "12334_{cna:1,eid:0000:0000:0000:1000:0010:0000:DF00:0460}"};
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(SwitchRoleWhenMasterFault).stubs();
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_MASTER, 1);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(IsMemInitFinished).stubs().will(returnValue(true));
    handle.nodeStateHandler = [](const std::string& faultInfo) {
    };
    MOCKER_CPP(ReportAckToSysSentry).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseNodeControllerModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseNodeControllerModule>()));
    auto res1 = handle.NodeFaultHandle(&msg);
    auto res2 = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res1, UBSE_OK);
    ASSERT_EQ(res2, UBSE_OK);
}
*/
TEST_F(TestUbseRasHandler, NodeFaultHandleWhenMsgIsNull)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg* msg = nullptr;
    auto res = handle.NodeFaultHandle(msg);
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseRasHandler, NodeFaultHandleWhenDefaultCase)
{
    auto& handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = 0};
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, TestLogMemDebtInfo)
{
    auto& handle = UbseRasHandler::GetInstance();
    EXPECT_NO_THROW(LogMemDebtInfoWithNode(1003, "1"));
}

TEST_F(TestUbseRasHandler, TestLogMemDebtInfo2)
{
    auto& handle = UbseRasHandler::GetInstance();
    MOCKER(IsNodeInStaticList).stubs().will(returnValue(true));
    EXPECT_NO_THROW(LogMemDebtInfoWithNode(1003, "1"));
}

TEST_F(TestUbseRasHandler, TestLogMemDebtInfo3)
{
    ubse::adapter_plugins::mmi::NodeMemDebtInfoMap memDebtInfoMap;
    ubse::adapter_plugins::mmi::NodeMemDebtInfo debtInfo;
    debtInfo.fdExportObjMap = {{"1", {}}};
    debtInfo.fdImportObjMap = {{"2", {}}};
    debtInfo.numaExportObjMap = {{"1", {}}};
    debtInfo.numaImportObjMap = {{"2", {}}};
    debtInfo.addrExportObjMap = {{"1", {}}};
    debtInfo.addrImportObjMap = {{"2", {}}};
    memDebtInfoMap["1"] = debtInfo;
    EXPECT_NO_THROW(ProcessDebtInfo(memDebtInfoMap, "1", {}));
}

TEST_F(TestUbseRasHandler, TestProcessExportObj)
{
    ubse::adapter_plugins::mmi::UbseMemBorrowExportBaseObj numaExporetObj;
    numaExporetObj.status.state = ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_EXPORT_SUCCESS;
    std::unordered_map<std::string, DebtInfo> numaMemoryDebtInfoMap;
    MOCKER(IsNodeInStaticList).stubs().will(returnValue(true));
    EXPECT_NO_THROW(ProcessExportObj("Fd", "1", numaExporetObj, "1", numaMemoryDebtInfoMap));
}

TEST_F(TestUbseRasHandler, TestProcessExportObj2)
{
    ubse::adapter_plugins::mmi::UbseMemBorrowExportBaseObj numaExporetObj;
    numaExporetObj.status.state = ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_EXPORT_SUCCESS;
    std::unordered_map<std::string, DebtInfo> numaMemoryDebtInfoMap;
    std::string borrowNodeId = "1";
    std::string lentNodeId = "2";
    MOCKER(GetBorrowNodeId).stubs().will(returnValue(borrowNodeId));
    MOCKER(GetLentNodeId).stubs().will(returnValue(lentNodeId));
    EXPECT_NO_THROW(ProcessExportObj("Fd", "1", numaExporetObj, "1", numaMemoryDebtInfoMap));
}

TEST_F(TestUbseRasHandler, TestProcessImportObj)
{
    ubse::adapter_plugins::mmi::UbseMemBorrowImportBaseObj numaExporetObj;
    numaExporetObj.status.state = ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_EXPORT_SUCCESS;
    std::unordered_map<std::string, DebtInfo> numaMemoryDebtInfoMap;
    std::string borrowNodeId = "1";
    std::string lentNodeId = "2";
    MOCKER(GetBorrowNodeId).stubs().will(returnValue(borrowNodeId));
    MOCKER(GetLentNodeId).stubs().will(returnValue(lentNodeId));
    EXPECT_NO_THROW(ProcessImportObj("Fd", "1", numaExporetObj, "1", numaMemoryDebtInfoMap));
}

TEST_F(TestUbseRasHandler, SendSwitchRoleToStandbyWhenRoleIsNotMaster)
{
    UbseRoleInfo info;
    info.nodeRole = ELECTION_ROLE_STANDBY;
    auto ret = SendSwitchRoleToStandby(info, "SendSwitchRoleToStandbyWhenRoleIsNotMaster");
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseRasHandler, SendSwitchRoleToStandbyWhenGetStandbyInfoFailed)
{
    UbseRoleInfo info;
    info.nodeRole = ELECTION_ROLE_MASTER;
    g_MSG_ID_MAP[ALARM_REBOOT_EVENT].erase("SendSwitchRoleToStandbyWhenGetStandbyInfoFailed");
    MOCKER_CPP(UbseGetStandbyInfo).stubs().will(returnValue(UBSE_ERROR));
    auto ret = SendSwitchRoleToStandby(info, "SendSwitchRoleToStandbyWhenGetStandbyInfoFailed");
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, SendSwitchRoleToStandbyWhenGetModuleFailed)
{
    UbseRoleInfo info;
    info.nodeRole = ELECTION_ROLE_MASTER;
    g_MSG_ID_MAP[ALARM_REBOOT_EVENT].erase("SendSwitchRoleToStandbyWhenGetModuleFailed");
    MOCKER_CPP(UbseGetStandbyInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<UbseComModule>(nullptr)));
    auto ret = SendSwitchRoleToStandby(info, "SendSwitchRoleToStandbyWhenGetModuleFailed");
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseRasHandler, SendSwitchRoleToStandbyWhenRpcSendFailed)
{
    UbseRoleInfo info;
    info.nodeRole = ELECTION_ROLE_MASTER;
    g_MSG_ID_MAP[ALARM_REBOOT_EVENT].erase("SendSwitchRoleToStandbyWhenRpcSendFailed");
    MOCKER_CPP(UbseGetStandbyInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseElectionModule>()));
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func = &UbseComModule::RpcSend<UbseRasMessagePtr, UbseRasMessagePtr>;
    UbseRasMessagePtr response = new (std::nothrow) UbseRasMessage();
    response->SetResult(UBSE_ERROR);
    MOCKER_CPP(func).stubs().with(_, _, outBound(response), _).will(returnValue(UBSE_ERROR));
    auto ret = SendSwitchRoleToStandby(info, "SendSwitchRoleToStandbyWhenRpcSendFailed");
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, SendSwitchRoleToStandbyWhenSuccess)
{
    UbseRoleInfo info;
    info.nodeRole = ELECTION_ROLE_MASTER;
    g_MSG_ID_MAP[ALARM_REBOOT_EVENT].erase("SendSwitchRoleToStandbyWhenSuccess");
    MOCKER_CPP(UbseGetStandbyInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseElectionModule>()));
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(std::make_shared<UbseComModule>()));
    const auto func = &UbseComModule::RpcSend<UbseRasMessagePtr, UbseRasMessagePtr>;
    UbseRasMessagePtr response = new (std::nothrow) UbseRasMessage();
    response->SetResult(UBSE_OK);
    MOCKER_CPP(func).stubs().with(_, _, outBound(response), _).will(returnValue(UBSE_OK));
    auto ret = SendSwitchRoleToStandby(info, "SendSwitchRoleToStandbyWhenSuccess");
    ASSERT_EQ(ret, UBSE_RAS_ERROR_SWITCH_ROLE);
}

UbseResult MockOnlyOneNodeUbseGetAllNodes(UbseElectionModule*, Node& master, Node& standby, std::vector<Node>& agents)
{
    master = {"1"};
    standby = {};
    agents = {};
    return UBSE_OK;
}

UbseResult MockOnlyOneNodeGetCurrentNode(UbseElectionModule*, Node& currentNode)
{
    currentNode = {"1"};
    return UBSE_OK;
}

UbseResult MockEmptyIdGetCurrentNode(UbseElectionModule*, Node& currentNode)
{
    currentNode = {""};
    return UBSE_OK;
}

UbseResult MockNode2GetCurrentNode(UbseElectionModule*, Node& currentNode)
{
    currentNode = {"2"};
    return UBSE_OK;
}

UbseResult MockMasterAndStandbyUbseGetAllNodes(UbseElectionModule*, Node& master, Node& standby,
                                               std::vector<Node>& agents)
{
    master = {"1"};
    standby = {"2"};
    agents = {};
    return UBSE_OK;
}

UbseResult MockMasterAndAgentUbseGetAllNodes(UbseElectionModule*, Node& master, Node& standby,
                                             std::vector<Node>& agents)
{
    master = {"1"};
    standby = {};
    agents = {{"3"}};
    return UBSE_OK;
}

TEST_F(TestUbseRasHandler, IsOnlyOneNodeInClusterWhenElectionModuleNull)
{
    std::shared_ptr<UbseElectionModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule));
    ASSERT_FALSE(IsOnlyOneNodeInCluster());
}

TEST_F(TestUbseRasHandler, IsOnlyOneNodeInClusterWhenGetAllNodesFail)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(returnValue(UBSE_ERROR));
    ASSERT_FALSE(IsOnlyOneNodeInCluster());
}

TEST_F(TestUbseRasHandler, IsOnlyOneNodeInClusterWhenGetCurrentNodeFail)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(invoke(MockOnlyOneNodeUbseGetAllNodes));
    MOCKER(&UbseElectionModule::GetCurrentNode).stubs().will(returnValue(UBSE_ERROR));
    ASSERT_FALSE(IsOnlyOneNodeInCluster());
}

TEST_F(TestUbseRasHandler, IsOnlyOneNodeInClusterWhenCurrentNodeIdEmpty)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(invoke(MockOnlyOneNodeUbseGetAllNodes));
    MOCKER(&UbseElectionModule::GetCurrentNode).stubs().will(invoke(MockEmptyIdGetCurrentNode));
    ASSERT_FALSE(IsOnlyOneNodeInCluster());
}

TEST_F(TestUbseRasHandler, IsOnlyOneNodeInClusterWhenCurrentNodeIsNotMaster)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(invoke(MockOnlyOneNodeUbseGetAllNodes));
    MOCKER(&UbseElectionModule::GetCurrentNode).stubs().will(invoke(MockNode2GetCurrentNode));
    ASSERT_FALSE(IsOnlyOneNodeInCluster());
}

TEST_F(TestUbseRasHandler, IsOnlyOneNodeInClusterWhenStandbyExists)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(invoke(MockMasterAndStandbyUbseGetAllNodes));
    MOCKER(&UbseElectionModule::GetCurrentNode).stubs().will(invoke(MockOnlyOneNodeGetCurrentNode));
    ASSERT_FALSE(IsOnlyOneNodeInCluster());
}

TEST_F(TestUbseRasHandler, IsOnlyOneNodeInClusterWhenAgentsExist)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(invoke(MockMasterAndAgentUbseGetAllNodes));
    MOCKER(&UbseElectionModule::GetCurrentNode).stubs().will(invoke(MockOnlyOneNodeGetCurrentNode));
    ASSERT_FALSE(IsOnlyOneNodeInCluster());
}

TEST_F(TestUbseRasHandler, IsOnlyOneNodeInClusterSuccess)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(invoke(MockOnlyOneNodeUbseGetAllNodes));
    MOCKER(&UbseElectionModule::GetCurrentNode).stubs().will(invoke(MockOnlyOneNodeGetCurrentNode));
    ASSERT_TRUE(IsOnlyOneNodeInCluster());
}
} // namespace ubse::ras::ut