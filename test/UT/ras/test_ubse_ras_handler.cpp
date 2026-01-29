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

#include "ubse_error.h"
#include "test_ubse_ras_handler.h"

namespace ubse::ras::ut {
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
    auto &instance1 = UbseRasHandler::GetInstance();
    auto &instance2 = UbseRasHandler::GetInstance();
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
void *xalarmHandle = static_cast<void *>(dummyPtr.get());
void *nullVoid = static_cast<char *>(nullptr);
TEST_F(TestUbseRasHandler, ReportAckToSysSentryWhenDlopenFail)
{
    MOCKER_CPP(dlopen).stubs().will(returnValue(nullVoid));
    auto res = ReportAckToSysSentry(g_alarmFaultType, g_message);
    ASSERT_EQ(res, RAS_ERROR_DLOPEN_XALARMD);
}

TEST_F(TestUbseRasHandler, ReportAckToSysSentryWhenDlsymFail)
{
    MOCKER_CPP(dlopen).stubs().will(returnValue(xalarmHandle));
    MOCKER_CPP(dlsym).stubs().will(returnValue(nullVoid));
    MOCKER_CPP(dlclose).stubs().will(returnValue(0));
    auto res = ReportAckToSysSentry(g_alarmFaultType, g_message);
    ASSERT_EQ(res, RAS_ERROR_DLSYM_XALARMD);
}

TEST_F(TestUbseRasHandler, ReportAckToSysSentryReportWhenReportFuncFail)
{
    XalarmReportEventFunc func = [](unsigned short, char *) -> int {
        return -1;
    };
    MOCKER_CPP(dlopen).stubs().will(returnValue(xalarmHandle));
    MOCKER_CPP(dlsym).stubs().will(returnValue((void *)func));
    MOCKER_CPP(dlclose).stubs().will(returnValue(0));
    auto res = ReportAckToSysSentry(g_alarmFaultType, g_message);
    ASSERT_EQ(res, RAS_ERROR_REPORT_TO_XALARMD);
}

TEST_F(TestUbseRasHandler, ReportAckToSysSentryReportWhenReportFuncSuccess)
{
    XalarmReportEventFunc func = [](unsigned short, char *) -> int {
        return 0;
    };
    MOCKER_CPP(dlopen).stubs().will(returnValue(xalarmHandle));
    MOCKER_CPP(dlsym).stubs().will(returnValue((void *)func));
    MOCKER_CPP(dlclose).stubs().will(returnValue(0));
    auto res = ReportAckToSysSentry(g_alarmFaultType, g_message);
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, SwitchRoleWhenMasterFaultWhenGetMasterInfoFail)
{
    auto &handler = UbseRasHandler::GetInstance();
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(IsMemInitFinished).stubs().will(returnValue(false));
    MOCKER_CPP(ubse::election::UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    auto res = handler.HandlePanicAndRebootFault(ALARM_REBOOT_EVENT, "");
    ASSERT_EQ(res, RAS_IS_NOT_MASTER_OR_MEM_IS_NOT_INIT);
}

TEST_F(TestUbseRasHandler, SwitchRoleWhenMasterFaultWhenGetCurrentInfoFail)
{
    auto &handler = UbseRasHandler::GetInstance();
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(IsMemInitFinished).stubs().will(returnValue(false));
    UbseRoleInfo masterInfo{};
    MOCKER_CPP(ubse::election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    auto res = handler.HandlePanicAndRebootFault(ALARM_REBOOT_EVENT, "");
    ASSERT_EQ(res, UBSE_ERROR_BADF);
}

TEST_F(TestUbseRasHandler, SwitchRoleWhenMasterFaultWhenGetElectionModuleFail)
{
    auto &handler = UbseRasHandler::GetInstance();
    UbseRoleInfo masterInfo("1", ELECTION_ROLE_MASTER, 1);
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_STANDBY, 1);
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(IsMemInitFinished).stubs().will(returnValue(false));
    MOCKER_CPP(ubse::election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    std::shared_ptr<UbseElectionModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule));
    auto res = handler.HandlePanicAndRebootFault(ALARM_REBOOT_EVENT, "");
    ASSERT_EQ(res, RAS_IS_NOT_MASTER_OR_MEM_IS_NOT_INIT);
}

TEST_F(TestUbseRasHandler, SwitchRoleWhenMasterFaultSuccess)
{
    auto &handler = UbseRasHandler::GetInstance();
    UbseRoleInfo masterInfo("1", ELECTION_ROLE_MASTER, 1);
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_STANDBY, 1);
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(IsMemInitFinished).stubs().will(returnValue(false));
    MOCKER_CPP(ubse::election::UbseGetMasterInfo).stubs().with(outBound(masterInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseElectionModule::SwitchMasterFromStandby).stubs();
    auto res = handler.HandlePanicAndRebootFault(ALARM_REBOOT_EVENT, "");
    ASSERT_EQ(res, RAS_IS_NOT_MASTER_OR_MEM_IS_NOT_INIT);
}

TEST_F(TestUbseRasHandler, StartRasHandlerWhenComModuleIsNull)
{
    auto &handler = UbseRasHandler::GetInstance();
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule));
    auto res = handler.StartRasHandler();
    ASSERT_EQ(res, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasHandler, StartRasHandlerWhenRegRpcServiceFail)
{
    auto &handler = UbseRasHandler::GetInstance();
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    UbseResult (UbseComModule::*func)(UbseComBaseMessageHandlerPtr &) =
        &ubse::com::UbseComModule::RegRpcService<UbseRasMessage, UbseRasMessage>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR));
    auto res = handler.StartRasHandler();
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, StartRasHandlerSuccess)
{
    auto &handler = UbseRasHandler::GetInstance();
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    UbseResult (UbseComModule::*func)(UbseComBaseMessageHandlerPtr &) =
        &ubse::com::UbseComModule::RegRpcService<UbseRasMessage, UbseRasMessage>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    auto res = handler.StartRasHandler();
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, ExecuteFaultHandlerWhenNoHandlerRegister)
{
    auto &handler = UbseRasHandler::GetInstance();
    ALARM_FAULT_TYPE type = ALARM_OOM_EVENT;
    std::string faultInfo = "1";
    handler.faultHandlerMap.clear();
    auto res = handler.ExecuteFaultHandler(type, faultInfo);
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, ExecuteFaultHandlerSuccess)
{
    auto &handler = UbseRasHandler::GetInstance();
    ALARM_FAULT_TYPE type = ALARM_OOM_EVENT;
    std::string faultInfo = "1";
    handler.faultHandlerMap.clear();
    handler.faultHandlerMap[type][AlarmHandlerPriority::HIGH].emplace_back("test1", (AlarmFaultHandler) nullptr);
    AlarmFaultHandler func = [](ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo) -> uint32_t {
        return UBSE_OK;
    };
    handler.faultHandlerMap[type][AlarmHandlerPriority::HIGH].emplace_back("test2", func);
    auto res = handler.ExecuteFaultHandler(type, faultInfo);
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
    ASSERT_EQ(res, RAS_PANIC_REBOOT_MSG_INVALID);
}

TEST_F(TestUbseRasHandler, HandleCnaAndEidMsgWhenNodeIdIsEmpty)
{
    std::string faultInfo = "1";
    std::string faultNodeId = "1";
    std::string emptyStr = "";
    MOCKER_CPP(ubse::utils::SplitSysSentryMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(QueryNodeIdByEid).stubs().will(returnValue(emptyStr));
    auto res = HandleCnaAndEidMsg(faultInfo, faultNodeId);
    ASSERT_EQ(res, RAS_ERROR_QUERY_NODE_BY_EID);
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
    ubse::mti::DevName dev1("1-1");
    ubse::mti::DevName dev2("1-");
    ubse::mti::UbseLcneSocketInfo info1{.primaryEid = "192.168.1.1"};
    ubse::mti::UbseLcneSocketInfo info2{.primaryEid = "192.168.1.2"};
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
    ubse::mti::DevName dev1("1-1");
    ubse::mti::DevName dev2("1-2");
    ubse::mti::UbseLcneSocketInfo info1{.primaryEid = "192.168.1.1"};
    ubse::mti::UbseLcneSocketInfo info2{.primaryEid = "192.168.1.2"};
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
    ubse::mti::DevName dev1("1-1");
    ubse::mti::DevName dev2("1-2");
    ubse::mti::UbseLcneSocketInfo info1{.primaryEid = "192.168.1.1"};
    ubse::mti::UbseLcneSocketInfo info2{.primaryEid = "192.168.1.2"};
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
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_REBOOT_EVENT, .pucParas = "HandleBMCFaultGetCurrentNodeInfoFail"};
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_ERROR_BADF);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultWhenGetMasterInfoFail)
{
    auto &handle = UbseRasHandler::GetInstance();
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_STANDBY, 1);
    alarm_msg msg{.usAlarmId = ALARM_REBOOT_EVENT, .pucParas = "HandleBMCFaultGetMasterInfoFail"};
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_ERROR_BADF);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultWhenGetComModuleFail)
{
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_REBOOT_EVENT, .pucParas = "HandleBMCFaultGetComModuleFail"};
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
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_REBOOT_EVENT, .pucParas = "HandleBMCFaultSuccess"};
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
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_REBOOT_EVENT, .pucParas = "HandleBMCFaultMsgDuplication"};
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
    ASSERT_EQ(res2, RAS_ERROR_MSG_DUPLICATION);
}
*/

TEST_F(TestUbseRasHandler, HandleOomFaultWhenMsgIsNull)
{
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg *nullMsg = nullptr;
    auto res = handle.HandleOomFault(nullMsg);
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}
/*
TEST_F(TestUbseRasHandler, HandleOomFaultWhenMsgVecSizeError)
{
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = TOPOLOGY_FAULT_TYPE::OOM, .pucParas = "HandleOomFaultMsgVecSizeError"};
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseRasHandler, HandleOomFaultSuccess)
{
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = TOPOLOGY_FAULT_TYPE::OOM, .pucParas = "HandleOomFaultSuccess_success"};
    MOCKER_CPP(&UbseRasHandler::ExecuteFaultHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(ReportAckToSysSentry).stubs().will(returnValue(UBSE_OK));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, HandleOomFaultWhenMsgDuplication)
{
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = TOPOLOGY_FAULT_TYPE::OOM, .pucParas = "HandleOomFaultMsgDuplication_1"};
    MOCKER_CPP(&UbseRasHandler::ExecuteFaultHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(ReportAckToSysSentry).stubs().will(returnValue(UBSE_OK));
    auto res1 = handle.NodeFaultHandle(&msg);
    auto res2 = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res1, UBSE_OK);
    ASSERT_EQ(res2, RAS_ERROR_MSG_DUPLICATION);
}

TEST_F(TestUbseRasHandler, HandlePanicAndRebootFaultWhenHandleCnaFail)
{
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = TOPOLOGY_FAULT_TYPE::PANIC, .pucParas = "HandlePanicAndRebootFaultHandleCnaFail"};
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_ERROR));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, HandlePanicAndRebootFaultWhenGetCurrentNodeInfoFail)
{
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = TOPOLOGY_FAULT_TYPE::PANIC,
                  .pucParas = "HandlePanicAndRebootFaultGetCurrentNodeInfoFail"};
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(SwitchRoleWhenMasterFault).stubs();
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_ERROR_BADF);
}

TEST_F(TestUbseRasHandler, HandlePanicAndRebootFaultWhenNodeIsNotMaster)
{
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = TOPOLOGY_FAULT_TYPE::PANIC, .pucParas = "HandlePanicAndRebootFaultIsNotMaster"};
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(SwitchRoleWhenMasterFault).stubs();
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_STANDBY, 1);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, RAS_IS_NOT_MASTER_OR_MEM_IS_NOT_INIT);
}

TEST_F(TestUbseRasHandler, HandlePanicAndRebootFaultWhenMsgVecSizeError)
{
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = TOPOLOGY_FAULT_TYPE::PANIC, .pucParas = "HandlePanicAndRebootFaultMsgVecSizeError"};
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(SwitchRoleWhenMasterFault).stubs();
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_MASTER, 1);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(IsMemInitFinished).stubs().will(returnValue(true));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseRasHandler, HandlePanicAndRebootFaultSuccess)
{
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = TOPOLOGY_FAULT_TYPE::PANIC, .pucParas = "HandlePanicAndRebootFaultSuccess_1"};
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(SwitchRoleWhenMasterFault).stubs();
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_MASTER, 1);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(IsMemInitFinished).stubs().will(returnValue(true));
    handle.nodeStateHandler = [](const std::string &faultInfo) {
    };
    MOCKER_CPP(&UbseRasHandler::ExecuteFaultHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(ReportAckToSysSentry).stubs().will(returnValue(UBSE_OK));
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, HandlePanicAndRebootFaultWhenMsgDuplication)
{
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = TOPOLOGY_FAULT_TYPE::PANIC, .pucParas = "HandlePanicAndRebootFaultMsgDuplication_1"};
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(SwitchRoleWhenMasterFault).stubs();
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_MASTER, 1);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(IsMemInitFinished).stubs().will(returnValue(true));
    handle.nodeStateHandler = [](const std::string &faultInfo) {
    };
    MOCKER_CPP(&UbseRasHandler::ExecuteFaultHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(ReportAckToSysSentry).stubs().will(returnValue(UBSE_OK));
    auto res1 = handle.NodeFaultHandle(&msg);
    auto res2 = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res1, UBSE_OK);
    ASSERT_EQ(res2, RAS_ERROR_MSG_DUPLICATION);
}

TEST_F(TestUbseRasHandler, HandleRebootFaultSuccess)
{
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = TOPOLOGY_FAULT_TYPE::KERNEL_REBOOT, .pucParas = "HandleRebootFault_1"};
    MOCKER_CPP(HandleCnaAndEidMsg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(SwitchRoleWhenMasterFault).stubs();
    UbseRoleInfo currentInfo("2", ELECTION_ROLE_MASTER, 1);
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo).stubs().with(outBound(currentInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(IsMemInitFinished).stubs().will(returnValue(true));
    handle.nodeStateHandler = [](const std::string &faultInfo) {
    };
    MOCKER_CPP(&UbseRasHandler::ExecuteFaultHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(ReportAckToSysSentry).stubs().will(returnValue(UBSE_OK));
    auto res1 = handle.NodeFaultHandle(&msg);
    auto res2 = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res1, UBSE_OK);
    ASSERT_EQ(res2, RAS_ERROR_MSG_DUPLICATION);
}
*/
TEST_F(TestUbseRasHandler, NodeFaultHandleWhenMsgIsNull)
{
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg *msg = nullptr;
    auto res = handle.NodeFaultHandle(msg);
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseRasHandler, NodeFaultHandleWhenDefaultCase)
{
    auto &handle = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = 0};
    auto res = handle.NodeFaultHandle(&msg);
    ASSERT_EQ(res, UBSE_ERROR);
}
}