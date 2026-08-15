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
#include <chrono>
#include <vector>
#include "ubse_com_module.h"
#include "ubse_error.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller_module.h"
#include "adapter_plugins/mti/ubse_smbios.h"
#include "src/controllers/mem/mem_controller/ubse_mem_service_impl.h"
#include "src/framework/node_mgr/ubse_node_static_info_mgr.h"
#include "ubse_ras_handler.cpp"

namespace ubse::ras::ut {
using namespace ubse::com;

using ManagingGroupInfoRpcSend = UbseResult (UbseComModule::*)(const SendParam&, UbseRasMessagePtr&, UbseRasMessagePtr&,
                                                               const bool);
const auto g_managingGroupInfoRpcSend =
    static_cast<ManagingGroupInfoRpcSend>(&UbseComModule::RpcSend<UbseRasMessagePtr, UbseRasMessagePtr>);

std::shared_ptr<UbseComModule> g_managingGroupInfoComModule;
std::vector<std::string> g_managingGroupInfoTargets;
std::string g_managingGroupInfoResponseData;
UbseResult g_managingGroupInfoBusinessRet = UBSE_OK;
UbseResult g_managingGroupInfoRpcRet = UBSE_OK;

UbseResult CaptureManagingGroupInfoSend(UbseComModule*, const SendParam& param, UbseRasMessagePtr& request,
                                        UbseRasMessagePtr& response, const bool)
{
    EXPECT_NE(request.Get(), nullptr);
    EXPECT_EQ(param.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::RAS));
    EXPECT_EQ(param.GetOpCode(), static_cast<uint16_t>(UbseRasOpCode::UBSE_RAS_QUERY_MANAGING_GROUP_INFO));
    g_managingGroupInfoTargets.push_back(param.GetRemoteId());
    response = new (std::nothrow) UbseRasMessage(g_managingGroupInfoResponseData);
    if (response == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    response->SetResult(g_managingGroupInfoBusinessRet);
    return g_managingGroupInfoRpcRet;
}

void ResetManagingGroupInfoTestData()
{
    g_managingGroupInfoTargets.clear();
    g_managingGroupInfoResponseData.clear();
    g_managingGroupInfoBusinessRet = UBSE_OK;
    g_managingGroupInfoRpcRet = UBSE_OK;
}

void ExpectManagingGroupInfoSends(uint32_t sendCount, const std::string& data, UbseResult businessRet = UBSE_OK,
                                  UbseResult rpcRet = UBSE_OK)
{
    g_managingGroupInfoResponseData = data;
    g_managingGroupInfoBusinessRet = businessRet;
    g_managingGroupInfoRpcRet = rpcRet;
    g_managingGroupInfoComModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>)
        .expects(exactly(sendCount))
        .will(returnValue(g_managingGroupInfoComModule));
    MOCKER(g_managingGroupInfoRpcSend).expects(exactly(sendCount)).will(invoke(CaptureManagingGroupInfoSend));
}

void MockMemService()
{
    std::shared_ptr<UbseMemService> memService = std::make_shared<UbseMemServiceImpl>();
    MOCKER_CPP(GetMemService).stubs().will(returnValue(memService));
}
void TestUbseRasHandler::SetUp()
{
    ClearAllBmcManagingGroupMissingStates();
    ResetManagingGroupInfoTestData();
    MockMemService();
    Test::SetUp();
}

void TestUbseRasHandler::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

void MockOneDimensionalTopology()
{
    MOCKER_CPP(&ubse::adapter_plugins::smbios::UbseSmbios::IsClosType).stubs().will(returnValue(false));
}

void MockHierarchicalElectionTopology()
{
    static const std::vector<std::string> rootIps;
    MOCKER_CPP(&ubse::adapter_plugins::smbios::UbseSmbios::IsClosType).stubs().will(returnValue(true));
    MOCKER_CPP(ubse::nodeMgr::GetRootIpList).stubs().will(returnValue(rootIps));
}

void MockSpecifiedRootNodeTopology()
{
    static const std::vector<std::string> rootIps{"10.0.0.1"};
    MOCKER_CPP(&ubse::adapter_plugins::smbios::UbseSmbios::IsClosType).stubs().will(returnValue(true));
    MOCKER_CPP(ubse::nodeMgr::GetRootIpList).stubs().will(returnValue(rootIps));
}

std::shared_ptr<UbseElectionModule> MockElectionModuleAvailable()
{
    auto electionModule = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(electionModule));
    return electionModule;
}

void FillCascadeGroupTopology(HaTopologyInfo& topology, const NodeRoleInfo& currentNode)
{
    topology = {};
    topology.currentNode = currentNode;
    topology.currentGroup = {"cascade-group", false, "cascade-master", "", {"cascade-master", ""}};
    topology.groups.push_back(
        {"another-cascade-group", false, "another-cascade-master", "", {"another-cascade-master", ""}});
}

UbseResult MockCascadeGroupMasterWithManagingGroup(UbseElectionModule*, HaTopologyInfo& topology)
{
    FillCascadeGroupTopology(topology, {"cascade-master", RoleType::MASTER, GlobalRoleType::GLOBAL_INITIALIZER});
    topology.groups.push_back({"managing-group", true, "managing-master", "", {"managing-master", ""}});
    return UBSE_OK;
}

UbseResult MockCascadeGroupMasterWithoutManagingGroup(UbseElectionModule*, HaTopologyInfo& topology)
{
    FillCascadeGroupTopology(topology, {"cascade-master", RoleType::MASTER, GlobalRoleType::GLOBAL_INITIALIZER});
    topology.groups.push_back({"", true, "", "", {}});
    return UBSE_OK;
}

UbseResult MockCascadeGroupAgent(UbseElectionModule*, HaTopologyInfo& topology)
{
    FillCascadeGroupTopology(topology, {"cascade-agent", RoleType::AGENT, GlobalRoleType::GLOBAL_NONE});
    topology.currentGroup.groupNodes.push_back("cascade-agent");
    return UBSE_OK;
}

// 构造当前节点为全局主、且没有组内备的管理组拓扑。
UbseResult MockManagingGroupGlobalMasterWithoutLocalStandby(UbseElectionModule*, HaTopologyInfo& topology)
{
    topology.currentNode = {"global-master", RoleType::MASTER, GlobalRoleType::GLOBAL_MASTER};
    topology.currentGroup = {"managing-group", true, "global-master", "", {"global-master", ""}};
    return UBSE_OK;
}

UbseResult MockManagingGroupGlobalMasterWithLocalStandby(UbseElectionModule* module, HaTopologyInfo& topology)
{
    MockManagingGroupGlobalMasterWithoutLocalStandby(module, topology);
    topology.currentGroup.groupStandbyId = "local-standby";
    topology.currentGroup.groupNodes[1] = "local-standby";
    return UBSE_OK;
}

UbseResult MockManagingGroupAgent(UbseElectionModule*, HaTopologyInfo& topology)
{
    topology.currentNode = {"managing-agent", RoleType::AGENT, GlobalRoleType::GLOBAL_NONE};
    topology.currentGroup = {"managing-group", true, "managing-master", "", {"managing-master", "", "managing-agent"}};
    return UBSE_OK;
}

void SetManagingGroupMissingDuration(const std::string& nodeId, std::chrono::seconds missingDuration)
{
    auto state = g_bmcManagingGroupMissingStates.find(nodeId);
    ASSERT_NE(state, g_bmcManagingGroupMissingStates.end());
    state->second.firstMissingAt = std::chrono::steady_clock::now() - missingDuration;
}

void ExpireManagingGroupMissingState(const std::string& nodeId)
{
    SetManagingGroupMissingDuration(nodeId, std::chrono::seconds(31));
}

void PrepareExpiredManagingGroupMissingState(const std::string& nodeId, const std::string& msgId)
{
    g_bmcManagingGroupMissingStates[nodeId] = {msgId, std::chrono::steady_clock::now() - std::chrono::seconds(31), 1};
}

TEST_F(TestUbseRasHandler, GetInstanceSame)
{
    auto& instance1 = UbseRasHandler::GetInstance();
    auto& instance2 = UbseRasHandler::GetInstance();
    ASSERT_EQ(&instance1, &instance2);
}

TEST_F(TestUbseRasHandler, IsDigitStringRejectsEmptyString)
{
    EXPECT_FALSE(IsDigitString(""));
}

TEST_F(TestUbseRasHandler, ManagingGroupInfoQueryEmptyMasterDoesNotSend)
{
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).expects(never());

    EXPECT_EQ(QueryManagingGroupInfo(""), ManagingGroupInfoState::UNKNOWN);
}

TEST_F(TestUbseRasHandler, ManagingGroupInfoQueryReturnsUnknownWhenComModuleIsUnavailable)
{
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>)
        .expects(once())
        .will(returnValue(std::shared_ptr<UbseComModule>(nullptr)));

    EXPECT_EQ(QueryManagingGroupInfo("master-a"), ManagingGroupInfoState::UNKNOWN);
}

TEST_F(TestUbseRasHandler, ManagingGroupInfoQueryReturnsUnknownWhenRpcSendFails)
{
    ExpectManagingGroupInfoSends(1, "available", UBSE_OK, UBSE_ERROR);

    EXPECT_EQ(QueryManagingGroupInfo("master-a"), ManagingGroupInfoState::UNKNOWN);
    EXPECT_EQ(g_managingGroupInfoTargets, (std::vector<std::string>{"master-a"}));
}

TEST_F(TestUbseRasHandler, ManagingGroupInfoQueryReturnsUnknownWhenRemoteQueryFails)
{
    ExpectManagingGroupInfoSends(1, "available", UBSE_ERROR);

    EXPECT_EQ(QueryManagingGroupInfo("master-a"), ManagingGroupInfoState::UNKNOWN);
}

TEST_F(TestUbseRasHandler, ManagingGroupInfoQueryReturnsUnknownForInvalidData)
{
    ExpectManagingGroupInfoSends(1, "invalid");

    EXPECT_EQ(QueryManagingGroupInfo("master-a"), ManagingGroupInfoState::UNKNOWN);
}

TEST_F(TestUbseRasHandler, ManagingGroupInfoQueryReturnsAvailableSynchronously)
{
    ExpectManagingGroupInfoSends(1, "available");

    EXPECT_EQ(QueryManagingGroupInfo("master-a"), ManagingGroupInfoState::AVAILABLE);
    EXPECT_EQ(g_managingGroupInfoTargets, (std::vector<std::string>{"master-a"}));
}

TEST_F(TestUbseRasHandler, ManagingGroupInfoQueryReturnsMissingSynchronously)
{
    ExpectManagingGroupInfoSends(1, "missing");

    EXPECT_EQ(QueryManagingGroupInfo("master-a"), ManagingGroupInfoState::MISSING);
}

TEST_F(TestUbseRasHandler, ManagingGroupInfoQueryDoesNotCacheSuccessfulResult)
{
    ExpectManagingGroupInfoSends(2, "available");

    EXPECT_EQ(QueryManagingGroupInfo("master-a"), ManagingGroupInfoState::AVAILABLE);
    EXPECT_EQ(QueryManagingGroupInfo("master-a"), ManagingGroupInfoState::AVAILABLE);
    EXPECT_EQ(g_managingGroupInfoTargets, (std::vector<std::string>{"master-a", "master-a"}));
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

TEST_F(TestUbseRasHandler, SwitchRoleWhenMasterFaultCurrentMasterInvokesSwitchAgent)
{
    const std::string faultNodeId = "1";
    UbseRoleInfo currentInfo("1", ELECTION_ROLE_MASTER, 1);
    Node localMaster;
    localMaster.id = "1";
    Node localStandby;
    localStandby.id = "2";
    auto electionModule = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).expects(once()).will(returnValue(electionModule));
    MOCKER_CPP(&UbseElectionModule::GetLocalMasterNode)
        .expects(once())
        .with(outBound(localMaster))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseElectionModule::GetLocalStandbyNode)
        .expects(once())
        .with(outBound(localStandby))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetCurrentNodeInfo)
        .expects(once())
        .with(outBound(currentInfo))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseElectionModule::SwitchAgentFromMaster).expects(once());

    SwitchRoleWhenMasterFault(faultNodeId);
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
    MOCKER_CPP(func).expects(exactly(3)).will(returnValue(UBSE_OK));
    UbseResult (UbseComModule::*panicFunc)(UbseComBaseMessageHandlerPtr&) =
        &ubse::com::UbseComModule::RegRpcService<UbseRasPanicRebootMessage, UbseRasPanicRebootMessage>;
    MOCKER_CPP(panicFunc).stubs().will(returnValue(UBSE_OK));
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
    MockOneDimensionalTopology();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseElectionModule>()));
    MOCKER_CPP(UbseGetStandbyInfo).stubs().will(returnValue(UBSE_ERROR));
    auto ret = SendSwitchRoleToStandby(info, "SendSwitchRoleToStandbyWhenGetStandbyInfoFailed");
    ASSERT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, SendSwitchRoleToStandbyWhenGetModuleFailed)
{
    UbseRoleInfo info;
    info.nodeRole = ELECTION_ROLE_MASTER;
    MockOneDimensionalTopology();
    MOCKER_CPP(UbseGetStandbyInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseElectionModule>()));
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
    MockOneDimensionalTopology();
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
    MockOneDimensionalTopology();
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

TEST_F(TestUbseRasHandler, SendSwitchRoleToStandbyNotifiesOnlyLocalStandbyForHierarchicalLocalMaster)
{
    UbseRoleInfo current("local-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    UbseRoleInfo globalMaster("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    Node localStandby{"local-standby"};
    auto electionModule = std::make_shared<UbseElectionModule>();
    auto comModule = std::make_shared<UbseComModule>();
    MockHierarchicalElectionTopology();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(electionModule));
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    MOCKER_CPP(&UbseElectionModule::GetLocalStandbyNode)
        .expects(once())
        .with(outBound(localStandby))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetMasterInfo).expects(once()).with(outBound(globalMaster)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseElectionModule::SwitchAgentFromMaster).expects(once());
    MOCKER(SendSwitchRoleMessage).expects(once()).with(_, std::string("local-standby")).will(returnValue(UBSE_OK));

    ASSERT_EQ(SendSwitchRoleToStandby(current, "1"), UBSE_RAS_ERROR_SWITCH_ROLE);
}

TEST_F(TestUbseRasHandler, SendSwitchRoleToStandbyNotifiesOnlyLocalStandbyForGlobalMasterWithoutGlobalStandby)
{
    UbseRoleInfo current("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    UbseRoleInfo globalMaster("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    Node localStandby{"local-standby"};
    auto electionModule = std::make_shared<UbseElectionModule>();
    auto comModule = std::make_shared<UbseComModule>();
    MockHierarchicalElectionTopology();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(electionModule));
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    MOCKER_CPP(&UbseElectionModule::GetLocalStandbyNode)
        .expects(once())
        .with(outBound(localStandby))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetMasterInfo).expects(once()).with(outBound(globalMaster)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetStandbyInfo).expects(once()).will(returnValue(UBSE_ERROR));
    MOCKER_CPP(&UbseElectionModule::SwitchAgentFromMaster).expects(once());
    MOCKER(SendSwitchRoleMessage).expects(once()).with(_, std::string("local-standby")).will(returnValue(UBSE_OK));

    ASSERT_EQ(SendSwitchRoleToStandby(current, "1"), UBSE_RAS_ERROR_SWITCH_ROLE);
}

TEST_F(TestUbseRasHandler, SendSwitchRoleToStandbyNotifiesLocalAndGlobalStandbyForHierarchicalGlobalMaster)
{
    UbseRoleInfo current("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    UbseRoleInfo globalMaster("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    UbseRoleInfo globalStandby("global-standby", ELECTION_ROLE_STANDBY, ELECTION_NODE_ONLINE);
    Node localStandby{"local-standby"};
    auto electionModule = std::make_shared<UbseElectionModule>();
    auto comModule = std::make_shared<UbseComModule>();
    MockHierarchicalElectionTopology();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(electionModule));
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    MOCKER_CPP(&UbseElectionModule::GetLocalStandbyNode)
        .expects(once())
        .with(outBound(localStandby))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetMasterInfo).expects(once()).with(outBound(globalMaster)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetStandbyInfo).expects(once()).with(outBound(globalStandby)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseElectionModule::SwitchAgentFromMaster).expects(once());
    MOCKER(SendSwitchRoleMessage).expects(once()).with(_, std::string("local-standby")).will(returnValue(UBSE_OK));
    MOCKER(SendSwitchRoleMessage).expects(once()).with(_, std::string("global-standby")).will(returnValue(UBSE_OK));
    ASSERT_EQ(SendSwitchRoleToStandby(current, "1"), UBSE_RAS_ERROR_SWITCH_ROLE);
}

TEST_F(TestUbseRasHandler, SendSwitchRoleToStandbyNotifiesGlobalStandbyWithoutLocalStandby)
{
    UbseRoleInfo current("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    UbseRoleInfo globalMaster("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    UbseRoleInfo globalStandby("global-standby", ELECTION_ROLE_STANDBY, ELECTION_NODE_ONLINE);
    auto electionModule = std::make_shared<UbseElectionModule>();
    auto comModule = std::make_shared<UbseComModule>();
    MockHierarchicalElectionTopology();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(electionModule));
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    MOCKER_CPP(&UbseElectionModule::GetLocalStandbyNode).expects(once()).will(returnValue(UBSE_ERROR));
    MOCKER_CPP(UbseGetMasterInfo).expects(once()).with(outBound(globalMaster)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetStandbyInfo).expects(once()).with(outBound(globalStandby)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseElectionModule::SwitchAgentFromMaster).expects(once());
    MOCKER(SendSwitchRoleMessage).expects(once()).with(_, std::string("global-standby")).will(returnValue(UBSE_OK));

    ASSERT_EQ(SendSwitchRoleToStandby(current, "1"), UBSE_RAS_ERROR_SWITCH_ROLE);
}

TEST_F(TestUbseRasHandler, SendSwitchRoleToStandbyDoesNotDemoteWhenNoStandbyExists)
{
    UbseRoleInfo current("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    UbseRoleInfo globalMaster("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    auto electionModule = std::make_shared<UbseElectionModule>();
    MockHierarchicalElectionTopology();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(electionModule));
    MOCKER_CPP(&UbseElectionModule::GetLocalStandbyNode).expects(once()).will(returnValue(UBSE_ERROR));
    MOCKER_CPP(UbseGetMasterInfo).expects(once()).with(outBound(globalMaster)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetStandbyInfo).expects(once()).will(returnValue(UBSE_ERROR));
    MOCKER_CPP(&UbseElectionModule::SwitchAgentFromMaster).expects(never());
    MOCKER(SendSwitchRoleMessage).expects(never());

    ASSERT_EQ(SendSwitchRoleToStandby(current, "1"), UBSE_OK);
}

TEST_F(TestUbseRasHandler, SendSwitchRoleToStandbyIgnoresGlobalSendFailure)
{
    UbseRoleInfo current("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    UbseRoleInfo globalMaster("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    UbseRoleInfo globalStandby("global-standby", ELECTION_ROLE_STANDBY, ELECTION_NODE_ONLINE);
    Node localStandby{"local-standby"};
    auto electionModule = std::make_shared<UbseElectionModule>();
    auto comModule = std::make_shared<UbseComModule>();
    MockHierarchicalElectionTopology();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(electionModule));
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    MOCKER_CPP(&UbseElectionModule::GetLocalStandbyNode)
        .expects(once())
        .with(outBound(localStandby))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetMasterInfo).expects(once()).with(outBound(globalMaster)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetStandbyInfo).expects(once()).with(outBound(globalStandby)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseElectionModule::SwitchAgentFromMaster).expects(once());
    MOCKER(SendSwitchRoleMessage).expects(once()).with(_, std::string("local-standby")).will(returnValue(UBSE_OK));
    MOCKER(SendSwitchRoleMessage).expects(once()).with(_, std::string("global-standby")).will(returnValue(UBSE_ERROR));
    ASSERT_EQ(SendSwitchRoleToStandby(current, "1"), UBSE_RAS_ERROR_SWITCH_ROLE);
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
/*
TEST_F(TestUbseRasHandler, IsOnlyOneNodeInClusterWhenGetAllNodesFail)
{
    auto module = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(module));
    MOCKER(&UbseElectionModule::UbseGetAllNodes).stubs().will(returnValue(UBSE_ERROR));
    ASSERT_FALSE(IsOnlyOneNodeInCluster());
}
*/
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
// ==================== IsHandlerDone 测试 ====================

// IsHandlerDone: msg 不在 map 中，应返回 false
TEST_F(TestUbseRasHandler, IsHandlerDoneWhenMsgNotFound)
{
    ASSERT_FALSE(IsHandlerDone("isdone_nonexist", "handler1"));
}

// IsHandlerDone: handler 不在 msg 下，应返回 false
TEST_F(TestUbseRasHandler, IsHandlerDoneWhenHandlerNotFound)
{
    SetHandlerResult(ALARM_OOM_EVENT, "isdone_msg1", "handler1", UBSE_OK);
    ASSERT_FALSE(IsHandlerDone("isdone_msg1", "handler2")); // handler2 不存在
}

// IsHandlerDone: handler 结果为 UBSE_OK，应返回 true
TEST_F(TestUbseRasHandler, IsHandlerDoneWhenHandlerOk)
{
    SetHandlerResult(ALARM_OOM_EVENT, "isdone_ok", "handler1", UBSE_OK);
    ASSERT_TRUE(IsHandlerDone("isdone_ok", "handler1"));
}

// IsHandlerDone: handler 结果为非 UBSE_OK，应返回 false
TEST_F(TestUbseRasHandler, IsHandlerDoneWhenHandlerFailed)
{
    SetHandlerResult(ALARM_OOM_EVENT, "isdone_fail", "handler1", UBSE_ERROR);
    ASSERT_FALSE(IsHandlerDone("isdone_fail", "handler1"));
}

// ==================== SetHandlerResult 测试 ====================

// SetHandlerResult: 设置新 handler 结果，可通过 GetResultFromHandlersByMsg 获取
TEST_F(TestUbseRasHandler, SetHandlerResultNewEntry)
{
    SetHandlerResult(ALARM_OOM_EVENT, "set_result_new", "handler1", UBSE_OK);
    ASSERT_EQ(GetResultFromHandlersByMsg("set_result_new"), UBSE_OK);
}

// SetHandlerResult: 覆盖已有 handler 结果
TEST_F(TestUbseRasHandler, SetHandlerResultOverwrite)
{
    SetHandlerResult(ALARM_OOM_EVENT, "set_result_overwrite", "handler1", UBSE_ERROR);
    ASSERT_EQ(GetResultFromHandlersByMsg("set_result_overwrite"), UBSE_ERROR);
    SetHandlerResult(ALARM_OOM_EVENT, "set_result_overwrite", "handler1", UBSE_OK); // 覆盖为 OK
    ASSERT_EQ(GetResultFromHandlersByMsg("set_result_overwrite"), UBSE_OK);
}

// ==================== GetResultFromHandlersByMsg 测试 ====================

// GetResultFromHandlersByMsg: msg 不在 map 中，应返回 UBSE_OK
TEST_F(TestUbseRasHandler, GetResultFromHandlersByMsgWhenMsgNotFound)
{
    ASSERT_EQ(GetResultFromHandlersByMsg("get_result_nonexist"), UBSE_OK);
}

// GetResultFromHandlersByMsg: 所有 handler 都返回 OK，应返回 UBSE_OK
TEST_F(TestUbseRasHandler, GetResultFromHandlersByMsgAllOk)
{
    SetHandlerResult(ALARM_OOM_EVENT, "get_result_all_ok", "handler1", UBSE_OK);
    SetHandlerResult(ALARM_OOM_EVENT, "get_result_all_ok", "handler2", UBSE_OK);
    ASSERT_EQ(GetResultFromHandlersByMsg("get_result_all_ok"), UBSE_OK);
}

// GetResultFromHandlersByMsg: 存在一个失败的 handler，应返回其错误码
TEST_F(TestUbseRasHandler, GetResultFromHandlersByMsgHasError)
{
    SetHandlerResult(ALARM_OOM_EVENT, "get_result_has_err", "handler1", UBSE_OK);
    SetHandlerResult(ALARM_OOM_EVENT, "get_result_has_err", "handler2", UBSE_ERROR_INVAL);
    ASSERT_EQ(GetResultFromHandlersByMsg("get_result_has_err"), UBSE_ERROR_INVAL);
}

// GetResultFromHandlersByMsg: 多个 handler 失败，返回第一个非 OK 的错误码
TEST_F(TestUbseRasHandler, GetResultFromHandlersByMsgFirstError)
{
    SetHandlerResult(ALARM_OOM_EVENT, "get_result_first_err", "handler1", UBSE_ERROR_NULLPTR);
    SetHandlerResult(ALARM_OOM_EVENT, "get_result_first_err", "handler2", UBSE_ERROR_INVAL);
    // 由于 unordered_map 迭代顺序不确定，检查结果是非 OK 即可
    auto result = GetResultFromHandlersByMsg("get_result_first_err");
    ASSERT_NE(result, UBSE_OK);
}

// ==================== ClearAllHandlerResults 测试 ====================

// ClearAllHandlerResults: 清除空 map，无异常
TEST_F(TestUbseRasHandler, ClearAllHandlerResultsEmpty)
{
    EXPECT_NO_THROW(ClearAllHandlerResults());
}

// ClearAllHandlerResults: 清除非空 map，清除后查询应返回 UBSE_OK
TEST_F(TestUbseRasHandler, ClearAllHandlerResultsNonEmpty)
{
    SetHandlerResult(ALARM_OOM_EVENT, "clear_nonempty_1", "handler1", UBSE_ERROR);
    SetHandlerResult(ALARM_OOM_EVENT, "clear_nonempty_2", "handler1", UBSE_ERROR);
    ClearAllHandlerResults();
    ASSERT_EQ(GetResultFromHandlersByMsg("clear_nonempty_1"), UBSE_OK);
    ASSERT_EQ(GetResultFromHandlersByMsg("clear_nonempty_2"), UBSE_OK);
    ASSERT_FALSE(IsHandlerDone("clear_nonempty_1", "handler1"));
}

// ==================== ExecuteFaultHandler 集成测试 ====================

// ExecuteFaultHandler: handler 已完成（IsHandlerDone 返回 true），跳过重复执行
TEST_F(TestUbseRasHandler, ExecuteFaultHandlerSkipDoneHandler)
{
    auto& handler = UbseRasHandler::GetInstance();
    ALARM_FAULT_TYPE type = ALARM_OOM_EVENT;
    std::string faultInfo = "1";
    handler.faultHandlerMap.clear();
    int callCount = 0;
    AlarmFaultHandler func = [&callCount](ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo) -> uint32_t {
        callCount++;
        return UBSE_OK;
    };
    handler.faultHandlerMap[type][AlarmHandlerPriority::HIGH].emplace_back("test_handler", func);
    std::string msgId = "integration_msg";

    // 预置 handler 为已完成状态
    SetHandlerResult(ALARM_OOM_EVENT, msgId, "test_handler", UBSE_OK);

    auto res = handler.ExecuteFaultHandler(type, faultInfo, msgId);
    ASSERT_EQ(res, UBSE_OK);
    ASSERT_EQ(callCount, 0); // 已完成的 handler 不应被再次调用
}

// ExecuteFaultHandler: handler 未完成，应正常执行并记录结果
TEST_F(TestUbseRasHandler, ExecuteFaultHandlerWithResultCaching)
{
    auto& handler = UbseRasHandler::GetInstance();
    ALARM_FAULT_TYPE type = ALARM_OOM_EVENT;
    std::string faultInfo = "1";
    handler.faultHandlerMap.clear();
    AlarmFaultHandler func = [](ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo) -> uint32_t {
        return UBSE_ERROR_INVAL;
    };
    handler.faultHandlerMap[type][AlarmHandlerPriority::HIGH].emplace_back("test_handler", func);
    std::string msgId = "integration_msg2";

    auto res = handler.ExecuteFaultHandler(type, faultInfo, msgId);
    // handler 返回了 UBSE_ERROR_INVAL，ExecuteFaultHandler 应传播此错误
    ASSERT_EQ(res, UBSE_ERROR_INVAL);
    // handler 返回错误，所以 IsHandlerDone 应返回 false（只有 UBSE_OK 才算 done）
    ASSERT_FALSE(IsHandlerDone(msgId, "test_handler"));
}

// ==================== CallOneNodeHandleRetry 测试 ====================

TEST_F(TestUbseRasHandler, CallOneNodeHandleRetrySuccessImmediate)
{
    NodeHandler handlerFunc = [](const std::string& nodeId) -> UbseResult {
        return UBSE_OK;
    };
    auto ret = CallOneNodeHandleRetry(handlerFunc, "1");
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseRasHandler, CallOneNodeHandleRetryAllFail)
{
    int callCount = 0;
    NodeHandler handlerFunc = [&callCount](const std::string& nodeId) -> UbseResult {
        callCount++;
        return UBSE_ERROR;
    };
    MOCKER(sleep).stubs().will(returnValue(0u));
    auto ret = CallOneNodeHandleRetry(handlerFunc, "1");
    ASSERT_EQ(ret, UBSE_ERROR);
    ASSERT_GT(callCount, 1);
}

TEST_F(TestUbseRasHandler, CallOneNodeHandleRetrySuccessAfterRetries)
{
    int callCount = 0;
    NodeHandler handlerFunc = [&callCount](const std::string& nodeId) -> UbseResult {
        callCount++;
        if (callCount < 3) {
            return UBSE_ERROR;
        }
        return UBSE_OK;
    };
    MOCKER(sleep).stubs().will(returnValue(0u));
    auto ret = CallOneNodeHandleRetry(handlerFunc, "1");
    ASSERT_EQ(ret, UBSE_OK);
    ASSERT_EQ(callCount, 3);
}

// ==================== ReportBMCFaultToMaster 测试 ====================

TEST_F(TestUbseRasHandler, ReportBMCFaultToMasterWhenFaultNodeIsMaster)
{
    auto res = ReportBMCFaultToMaster("info", "same_node", "same_node");
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, ReportBMCFaultToMasterWhenComModuleNull)
{
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<UbseComModule>(nullptr)));
    auto res = ReportBMCFaultToMaster("info", "fault_node", "master_node");
    ASSERT_EQ(res, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseRasHandler, ReportBMCFaultToMasterWhenRpcSendFail)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    const auto func = &UbseComModule::RpcSend<UbseRasMessagePtr, UbseRasMessagePtr>;
    UbseRasMessagePtr response = new (std::nothrow) UbseRasMessage();
    response->SetResult(UBSE_OK);
    MOCKER_CPP(func).stubs().with(_, _, outBound(response), _).will(returnValue(UBSE_ERROR));
    auto res = ReportBMCFaultToMaster("info", "fault_node", "master_node");
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, ReportBMCFaultToMasterWhenResponseNotOk)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    const auto func = &UbseComModule::RpcSend<UbseRasMessagePtr, UbseRasMessagePtr>;
    UbseRasMessagePtr response = new (std::nothrow) UbseRasMessage();
    response->SetResult(UBSE_ERROR);
    MOCKER_CPP(func).stubs().with(_, _, outBound(response), _).will(returnValue(UBSE_OK));
    auto res = ReportBMCFaultToMaster("info", "fault_node", "master_node");
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, ReportBMCFaultToMasterSuccess)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    const auto func = &UbseComModule::RpcSend<UbseRasMessagePtr, UbseRasMessagePtr>;
    UbseRasMessagePtr response = new (std::nothrow) UbseRasMessage();
    response->SetResult(UBSE_OK);
    MOCKER_CPP(func).stubs().with(_, _, outBound(response), _).will(returnValue(UBSE_OK));
    auto res = ReportBMCFaultToMaster("info", "fault_node", "master_node");
    ASSERT_EQ(res, UBSE_OK);
}

// ==================== HandleBMCFault 测试 ====================

TEST_F(TestUbseRasHandler, ShouldSkipBmcFaultHandlingForSpecifiedRootMasterWithoutStandby)
{
    UbseRoleInfo current("master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    MockSpecifiedRootNodeTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(UbseGetStandbyInfo).expects(once()).will(returnValue(UBSE_ERROR));

    ASSERT_EQ(EvaluateBmcFaultAction(current, "1").action, BmcFaultAction::ACKNOWLEDGE);
}

TEST_F(TestUbseRasHandler, ShouldSkipBmcFaultHandlingForSpecifiedRootNonRootWithoutMasterAndStandby)
{
    UbseRoleInfo current("agent", ELECTION_ROLE_AGENT, ELECTION_NODE_ONLINE);
    auto& nodeInfoMgr = ubse::nodeMgr::UbseNodeStaticInfoMgr::GetInstance();
    const auto originalNode = nodeInfoMgr.GetCurrentNode();
    UbseNodeStaticInfo currentNode;
    currentNode.addr = "10.0.0.2";
    nodeInfoMgr.SetCurrentNode(currentNode);
    MockSpecifiedRootNodeTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(UbseGetMasterInfo).expects(once()).will(returnValue(UBSE_ERROR));
    MOCKER_CPP(UbseGetStandbyInfo).expects(once()).will(returnValue(UBSE_ERROR));

    const auto decision = EvaluateBmcFaultAction(current, "1");
    nodeInfoMgr.SetCurrentNode(originalNode);
    ASSERT_EQ(decision.action, BmcFaultAction::ACKNOWLEDGE);
}

TEST_F(TestUbseRasHandler, CascadeGroupMasterContinuesWhenTopologyContainsManagingGroupAndGlobalMaster)
{
    UbseRoleInfo current("cascade-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    UbseRoleInfo globalMaster("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .expects(once())
        .will(invoke(MockCascadeGroupMasterWithManagingGroup));
    MOCKER_CPP(UbseGetMasterInfo).expects(once()).with(outBound(globalMaster)).will(returnValue(UBSE_OK));

    ASSERT_EQ(EvaluateBmcFaultAction(current, "1").action, BmcFaultAction::CONTINUE_HANDLING);
}

TEST_F(TestUbseRasHandler, CascadeGroupMasterTreatsEmptyManagingGroupPlaceholderAsMissing)
{
    UbseRoleInfo current("cascade-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .expects(once())
        .will(invoke(MockCascadeGroupMasterWithoutManagingGroup));

    ASSERT_EQ(EvaluateBmcFaultAction(current, "1").action, BmcFaultAction::RETRY_LATER);
    ASSERT_EQ(g_bmcManagingGroupMissingStates.count(current.nodeId), 1U);
}

TEST_F(TestUbseRasHandler, CascadeGroupAgentContinuesImmediatelyWhenSynchronousQueryReturnsAvailable)
{
    UbseRoleInfo current("cascade-agent", ELECTION_ROLE_AGENT, ELECTION_NODE_ONLINE);
    UbseRoleInfo globalMaster("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    PrepareExpiredManagingGroupMissingState(current.nodeId, "1");
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    ExpectManagingGroupInfoSends(1, "available");
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo).expects(once()).will(invoke(MockCascadeGroupAgent));
    MOCKER_CPP(UbseGetMasterInfo).expects(once()).with(outBound(globalMaster)).will(returnValue(UBSE_OK));

    EXPECT_EQ(EvaluateBmcFaultAction(current, "1").action, BmcFaultAction::CONTINUE_HANDLING);
    EXPECT_EQ(g_managingGroupInfoTargets, (std::vector<std::string>{"cascade-master"}));
    EXPECT_EQ(g_bmcManagingGroupMissingStates.count(current.nodeId), 0U);
}

TEST_F(TestUbseRasHandler, CascadeGroupAgentStartsMissingWindowFromSynchronousQuery)
{
    UbseRoleInfo current("cascade-agent", ELECTION_ROLE_AGENT, ELECTION_NODE_ONLINE);
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    ExpectManagingGroupInfoSends(2, "missing");
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo).expects(exactly(2)).will(invoke(MockCascadeGroupAgent));

    EXPECT_EQ(EvaluateBmcFaultAction(current, "1").action, BmcFaultAction::RETRY_LATER);
    ASSERT_EQ(g_bmcManagingGroupMissingStates.count(current.nodeId), 1U);
    ExpireManagingGroupMissingState(current.nodeId);
    const auto decision = EvaluateBmcFaultAction(current, "1");
    EXPECT_EQ(decision.action, BmcFaultAction::ACKNOWLEDGE);
    EXPECT_TRUE(decision.managingGroupMissingAck);
    EXPECT_EQ(g_managingGroupInfoTargets, (std::vector<std::string>{"cascade-master", "cascade-master"}));
}

TEST_F(TestUbseRasHandler, CascadeGroupAgentRetriesWhenSynchronousQueryFails)
{
    UbseRoleInfo current("cascade-agent", ELECTION_ROLE_AGENT, ELECTION_NODE_ONLINE);
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    ExpectManagingGroupInfoSends(1, "available", UBSE_OK, UBSE_ERROR);
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo).expects(once()).will(invoke(MockCascadeGroupAgent));

    EXPECT_EQ(EvaluateBmcFaultAction(current, "1").action, BmcFaultAction::RETRY_LATER);
    EXPECT_EQ(g_bmcManagingGroupMissingStates.count(current.nodeId), 0U);
}

TEST_F(TestUbseRasHandler, CascadeGroupMasterAcknowledgesAfterManagingGroupIsMissingForTwentySeconds)
{
    UbseRoleInfo current("cascade-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .expects(exactly(2))
        .will(invoke(MockCascadeGroupMasterWithoutManagingGroup));

    EXPECT_EQ(EvaluateBmcFaultAction(current, "1").action, BmcFaultAction::RETRY_LATER);
    ExpireManagingGroupMissingState(current.nodeId);
    const auto decision = EvaluateBmcFaultAction(current, "1");
    EXPECT_EQ(decision.action, BmcFaultAction::ACKNOWLEDGE);
    EXPECT_TRUE(decision.managingGroupMissingAck);
}

TEST_F(TestUbseRasHandler, CascadeGroupMasterRetriesWhenManagingGroupExistsWithoutGlobalMaster)
{
    UbseRoleInfo current("cascade-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .expects(once())
        .will(invoke(MockCascadeGroupMasterWithManagingGroup));
    MOCKER_CPP(UbseGetMasterInfo).expects(once()).will(returnValue(UBSE_ERROR));

    ASSERT_EQ(EvaluateBmcFaultAction(current, "1").action, BmcFaultAction::RETRY_LATER);
}

TEST_F(TestUbseRasHandler, ManagingGroupGlobalMasterAcknowledgesWithoutLocalOrGlobalStandby)
{
    UbseRoleInfo current("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .expects(once())
        .will(invoke(MockManagingGroupGlobalMasterWithoutLocalStandby));
    MOCKER_CPP(UbseGetStandbyInfo).expects(once()).will(returnValue(UBSE_ERROR));

    ASSERT_EQ(EvaluateBmcFaultAction(current, "1").action, BmcFaultAction::ACKNOWLEDGE);
}

TEST_F(TestUbseRasHandler, ManagingGroupGlobalMasterContinuesWhenLocalStandbyExists)
{
    UbseRoleInfo current("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .expects(once())
        .will(invoke(MockManagingGroupGlobalMasterWithLocalStandby));
    MOCKER_CPP(UbseGetStandbyInfo).expects(never());

    ASSERT_EQ(EvaluateBmcFaultAction(current, "1").action, BmcFaultAction::CONTINUE_HANDLING);
}

TEST_F(TestUbseRasHandler, ManagingGroupGlobalMasterContinuesWhenGlobalStandbyExists)
{
    UbseRoleInfo current("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    UbseRoleInfo globalStandby("global-standby", ELECTION_ROLE_STANDBY, ELECTION_NODE_ONLINE);
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .expects(once())
        .will(invoke(MockManagingGroupGlobalMasterWithoutLocalStandby));
    MOCKER_CPP(UbseGetStandbyInfo).expects(once()).with(outBound(globalStandby)).will(returnValue(UBSE_OK));

    ASSERT_EQ(EvaluateBmcFaultAction(current, "1").action, BmcFaultAction::CONTINUE_HANDLING);
}

TEST_F(TestUbseRasHandler, ManagingGroupAgentContinuesNormalBmcHandling)
{
    UbseRoleInfo current("managing-agent", ELECTION_ROLE_AGENT, ELECTION_NODE_ONLINE);
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo).expects(once()).will(invoke(MockManagingGroupAgent));
    MOCKER_CPP(UbseGetStandbyInfo).expects(never());

    ASSERT_EQ(EvaluateBmcFaultAction(current, "1").action, BmcFaultAction::CONTINUE_HANDLING);
}

TEST_F(TestUbseRasHandler, BmcFaultAcknowledgesWhenElectionModuleIsUnavailable)
{
    UbseRoleInfo current("cascade-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>)
        .expects(once())
        .will(returnValue(std::shared_ptr<UbseElectionModule>(nullptr)));

    ASSERT_EQ(EvaluateBmcFaultAction(current, "1").action, BmcFaultAction::ACKNOWLEDGE);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultAcknowledgesWhenElectionModuleIsUnavailable)
{
    auto& handler = UbseRasHandler::GetInstance();
    MockHierarchicalElectionTopology();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>)
        .expects(once())
        .will(returnValue(std::shared_ptr<UbseElectionModule>(nullptr)));
    MOCKER_CPP(UbseGetCurrentNodeInfo).expects(never());
    MOCKER(ReportAckToSysSentry).expects(once()).will(returnValue(UBSE_OK));

    ASSERT_EQ(handler.HandleBMCFault("12345"), UBSE_OK);
}

TEST_F(TestUbseRasHandler, CascadeGroupMasterAcknowledgesAfterTopologyQueryFailsForTwentySeconds)
{
    UbseRoleInfo current("cascade-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo).expects(exactly(2)).will(returnValue(UBSE_ERROR));

    ASSERT_EQ(EvaluateBmcFaultAction(current, "1").action, BmcFaultAction::RETRY_LATER);
    ExpireManagingGroupMissingState(current.nodeId);
    const auto decision = EvaluateBmcFaultAction(current, "1");
    ASSERT_EQ(decision.action, BmcFaultAction::ACKNOWLEDGE);
    ASSERT_TRUE(decision.managingGroupMissingAck);
}

TEST_F(TestUbseRasHandler, SpecifiedRootPathKeepsManagingGroupMissingState)
{
    UbseRoleInfo current("root-node", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    PrepareExpiredManagingGroupMissingState(current.nodeId, "1");
    MockSpecifiedRootNodeTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(UbseGetStandbyInfo).expects(once()).will(returnValue(UBSE_ERROR));

    ASSERT_EQ(EvaluateBmcFaultAction(current, "1").action, BmcFaultAction::ACKNOWLEDGE);
    ASSERT_EQ(g_bmcManagingGroupMissingStates.count(current.nodeId), 1U);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultSpecifiedRootPathKeepsManagingGroupMissingState)
{
    auto& handler = UbseRasHandler::GetInstance();
    UbseRoleInfo current("root-node", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    PrepareExpiredManagingGroupMissingState(current.nodeId, "12345");
    MockSpecifiedRootNodeTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(UbseGetCurrentNodeInfo).expects(once()).with(outBound(current)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetStandbyInfo).expects(once()).will(returnValue(UBSE_ERROR));
    MOCKER(ReportAckToSysSentry).expects(once()).will(returnValue(UBSE_OK));

    ASSERT_EQ(handler.HandleBMCFault("12345"), UBSE_OK);
    ASSERT_EQ(g_bmcManagingGroupMissingStates.count(current.nodeId), 1U);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultWhenMsgIdInvalid)
{
    auto& handler = UbseRasHandler::GetInstance();
    auto res = handler.HandleBMCFault("not_a_number");
    ASSERT_EQ(res, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultWhenOnlyOneNodeInCluster)
{
    auto& handler = UbseRasHandler::GetInstance();
    MockOneDimensionalTopology();
    MOCKER(IsOnlyOneNodeInCluster).stubs().will(returnValue(true));
    MOCKER(ReportAckToSysSentry).stubs().will(returnValue(UBSE_OK));
    auto res = handler.HandleBMCFault("12345");
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultCascadeGroupAgentContinuesAfterSynchronousAvailableQuery)
{
    auto& handler = UbseRasHandler::GetInstance();
    UbseRoleInfo current("cascade-agent", ELECTION_ROLE_AGENT, ELECTION_NODE_ONLINE);
    UbseRoleInfo globalMaster("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    ExpectManagingGroupInfoSends(1, "available");
    MOCKER_CPP(UbseGetCurrentNodeInfo).expects(once()).with(outBound(current)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo).expects(once()).will(invoke(MockCascadeGroupAgent));
    MOCKER_CPP(UbseGetMasterInfo).expects(exactly(2)).with(outBound(globalMaster)).will(returnValue(UBSE_OK));
    MOCKER(SendSwitchRoleToStandby).expects(once()).will(returnValue(UBSE_OK));
    MOCKER(ReportBMCFaultToMaster).expects(once()).will(returnValue(UBSE_OK));
    MOCKER(ReportAckToSysSentry).expects(once()).will(returnValue(UBSE_OK));

    EXPECT_EQ(handler.HandleBMCFault("12345"), UBSE_OK);
    EXPECT_EQ(g_managingGroupInfoTargets, (std::vector<std::string>{"cascade-master"}));
}

TEST_F(TestUbseRasHandler, HandleBMCFaultCascadeGroupAgentRetriesWhenSynchronousQueryFails)
{
    auto& handler = UbseRasHandler::GetInstance();
    UbseRoleInfo current("cascade-agent", ELECTION_ROLE_AGENT, ELECTION_NODE_ONLINE);
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    ExpectManagingGroupInfoSends(1, "available", UBSE_OK, UBSE_ERROR);
    MOCKER_CPP(UbseGetCurrentNodeInfo).expects(once()).with(outBound(current)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo).expects(once()).will(invoke(MockCascadeGroupAgent));
    MOCKER(SendSwitchRoleToStandby).expects(never());
    MOCKER(ReportAckToSysSentry).expects(never());

    EXPECT_EQ(handler.HandleBMCFault("12345"), UBSE_ERROR);
    EXPECT_EQ(g_bmcManagingGroupMissingStates.count(current.nodeId), 0U);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultCascadeGroupAgentAcknowledgesAfterSynchronousMissingQueries)
{
    auto& handler = UbseRasHandler::GetInstance();
    UbseRoleInfo current("cascade-agent", ELECTION_ROLE_AGENT, ELECTION_NODE_ONLINE);
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    ExpectManagingGroupInfoSends(2, "missing");
    MOCKER_CPP(UbseGetCurrentNodeInfo).expects(exactly(2)).with(outBound(current)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo).expects(exactly(2)).will(invoke(MockCascadeGroupAgent));
    MOCKER(SendSwitchRoleToStandby).expects(never());
    MOCKER(ReportAckToSysSentry)
        .expects(once())
        .with(ALARM_REBOOT_ACK_EVENT, std::string("12345_0"))
        .will(returnValue(UBSE_OK));

    EXPECT_EQ(handler.HandleBMCFault("12345"), UBSE_ERROR);
    ExpireManagingGroupMissingState(current.nodeId);
    EXPECT_EQ(handler.HandleBMCFault("12345"), UBSE_OK);
    EXPECT_EQ(g_bmcManagingGroupMissingStates.count(current.nodeId), 0U);
    EXPECT_EQ(g_managingGroupInfoTargets, (std::vector<std::string>{"cascade-master", "cascade-master"}));
}

TEST_F(TestUbseRasHandler, HandleBMCFaultRetriesWithoutRoleSwitchWhenManagingGroupExistsWithoutGlobalMaster)
{
    auto& handler = UbseRasHandler::GetInstance();
    UbseRoleInfo current("cascade-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(UbseGetCurrentNodeInfo).expects(once()).with(outBound(current)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .expects(once())
        .will(invoke(MockCascadeGroupMasterWithManagingGroup));
    MOCKER_CPP(UbseGetMasterInfo).expects(once()).will(returnValue(UBSE_ERROR));
    MOCKER(SendSwitchRoleToStandby).expects(never());
    MOCKER(ReportAckToSysSentry).expects(never());

    ASSERT_EQ(handler.HandleBMCFault("12345"), UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultAcknowledgesAfterManagingGroupMissingTimeout)
{
    auto& handler = UbseRasHandler::GetInstance();
    UbseRoleInfo current("cascade-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    PrepareExpiredManagingGroupMissingState(current.nodeId, "12345");
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(UbseGetCurrentNodeInfo).expects(once()).with(outBound(current)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .expects(once())
        .will(invoke(MockCascadeGroupMasterWithoutManagingGroup));
    MOCKER(SendSwitchRoleToStandby).expects(never());
    MOCKER(ReportAckToSysSentry)
        .expects(once())
        .with(ALARM_REBOOT_ACK_EVENT, std::string("12345_0"))
        .will(returnValue(UBSE_OK));

    ASSERT_EQ(handler.HandleBMCFault("12345"), UBSE_OK);
    ASSERT_EQ(g_bmcManagingGroupMissingStates.count(current.nodeId), 0U);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultRetriesManagingGroupMissingAckAfterAcknowledgementFails)
{
    auto& handler = UbseRasHandler::GetInstance();
    UbseRoleInfo current("cascade-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    PrepareExpiredManagingGroupMissingState(current.nodeId, "12345");
    MockHierarchicalElectionTopology();
    MockElectionModuleAvailable();
    MOCKER_CPP(UbseGetCurrentNodeInfo).expects(exactly(2)).with(outBound(current)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .expects(exactly(2))
        .will(invoke(MockCascadeGroupMasterWithoutManagingGroup));
    MOCKER(SendSwitchRoleToStandby).expects(never());
    MOCKER(ReportAckToSysSentry).expects(exactly(2)).will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));

    ASSERT_EQ(handler.HandleBMCFault("12345"), UBSE_ERROR);
    ASSERT_NE(g_bmcManagingGroupMissingStates.find(current.nodeId), g_bmcManagingGroupMissingStates.end());
    ASSERT_EQ(handler.HandleBMCFault("12345"), UBSE_OK);
    ASSERT_EQ(g_bmcManagingGroupMissingStates.count(current.nodeId), 0U);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultReportsToGlobalMasterWhenLocalMasterHasNoLocalStandby)
{
    auto& handler = UbseRasHandler::GetInstance();
    UbseRoleInfo current("cascade-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    UbseRoleInfo globalMaster("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    auto electionModule = std::make_shared<UbseElectionModule>();
    MockHierarchicalElectionTopology();
    MOCKER_CPP(UbseGetCurrentNodeInfo).expects(once()).with(outBound(current)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetMasterInfo).expects(exactly(3)).with(outBound(globalMaster)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetStandbyInfo).expects(never());
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).expects(exactly(3)).will(returnValue(electionModule));
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .expects(once())
        .will(invoke(MockCascadeGroupMasterWithManagingGroup));
    MOCKER_CPP(&UbseElectionModule::GetLocalStandbyNode).expects(once()).will(returnValue(UBSE_ERROR));
    MOCKER_CPP(&UbseElectionModule::SwitchAgentFromMaster).expects(never());
    MOCKER(SendSwitchRoleMessage).expects(never());
    MOCKER(ReportBMCFaultToMaster)
        .expects(once())
        .with(std::string("12345"), std::string("cascade-master"), std::string("global-master"))
        .will(returnValue(UBSE_OK));
    MOCKER(ReportAckToSysSentry).expects(once()).will(returnValue(UBSE_OK));

    ASSERT_EQ(handler.HandleBMCFault("12345"), UBSE_OK);
}

TEST_F(TestUbseRasHandler, HandleBMCFaultAcknowledgesHierarchicalGlobalMasterWithoutAnyStandby)
{
    auto& handler = UbseRasHandler::GetInstance();
    UbseRoleInfo current("global-master", ELECTION_ROLE_MASTER, ELECTION_NODE_ONLINE);
    auto electionModule = std::make_shared<UbseElectionModule>();
    MockHierarchicalElectionTopology();
    MOCKER_CPP(UbseGetCurrentNodeInfo).expects(once()).with(outBound(current)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetStandbyInfo).expects(once()).will(returnValue(UBSE_ERROR));
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).expects(exactly(2)).will(returnValue(electionModule));
    MOCKER_CPP(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .expects(once())
        .will(invoke(MockManagingGroupGlobalMasterWithoutLocalStandby));
    MOCKER_CPP(&UbseElectionModule::GetLocalStandbyNode).expects(never());
    MOCKER(SendSwitchRoleToStandby).expects(never());
    MOCKER(ReportAckToSysSentry).expects(once()).will(returnValue(UBSE_OK));

    ASSERT_EQ(handler.HandleBMCFault("12345"), UBSE_OK);
}

TEST_F(TestUbseRasHandler, HandleBMCFault2GetCurrentNodeInfoFail)
{
    auto& handler = UbseRasHandler::GetInstance();
    MockOneDimensionalTopology();
    MOCKER(IsOnlyOneNodeInCluster).stubs().will(returnValue(false));
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    auto res = handler.HandleBMCFault("12345");
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, HandleBMCFault3SendSwitchRoleFail)
{
    auto& handler = UbseRasHandler::GetInstance();
    MockOneDimensionalTopology();
    MOCKER(IsOnlyOneNodeInCluster).stubs().will(returnValue(false));
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(SendSwitchRoleToStandby).stubs().will(returnValue(UBSE_ERROR));
    auto res = handler.HandleBMCFault("12345");
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, HandleBMCFault4GetMasterInfoFail)
{
    auto& handler = UbseRasHandler::GetInstance();
    MockOneDimensionalTopology();
    MOCKER(IsOnlyOneNodeInCluster).stubs().will(returnValue(false));
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(SendSwitchRoleToStandby).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_ERROR));
    auto res = handler.HandleBMCFault("12345");
    ASSERT_EQ(res, UBSE_ERROR);
}

TEST_F(TestUbseRasHandler, HandleBMCFault5ReportBMCFaultFails)
{
    auto& handler = UbseRasHandler::GetInstance();
    MockOneDimensionalTopology();
    MOCKER(IsOnlyOneNodeInCluster).stubs().will(returnValue(false));
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(SendSwitchRoleToStandby).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(ReportBMCFaultToMaster).stubs().will(returnValue(UBSE_ERROR));
    auto res = handler.HandleBMCFault("12345");
    ASSERT_EQ(res, UBSE_ERROR);
}

// ==================== HandleMemoryFault 测试 ====================

TEST_F(TestUbseRasHandler, HandleMemoryFaultWhenNoHandlerRegistered)
{
    auto& handler = UbseRasHandler::GetInstance();
    handler.faultHandlerMap.clear();
    auto res = handler.HandleMemoryFault(ALARM_OOM_EVENT, "test_info");
    // ExecuteFaultHandler returns UBSE_OK when no handler registered, then HandleMemoryFault also returns UBSE_OK
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, HandleMemoryFaultWhenHandlerReturnsError)
{
    auto& handler = UbseRasHandler::GetInstance();
    handler.faultHandlerMap.clear();
    ALARM_FAULT_TYPE type = ALARM_OOM_EVENT;
    AlarmFaultHandler func = [](ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo) -> uint32_t {
        return UBSE_ERROR;
    };
    handler.faultHandlerMap[type][AlarmHandlerPriority::HIGH].emplace_back("test_handler", func);
    auto res = handler.HandleMemoryFault(type, "test_info");
    ASSERT_EQ(res, UBSE_ERROR);
}

// ==================== ClearFaultHandlerResult 测试 ====================

TEST_F(TestUbseRasHandler, ClearFaultHandlerResultClearsHandlers)
{
    SetHandlerResult(ALARM_OOM_EVENT, "clear_fault_msg", "handler1", UBSE_ERROR);
    ASSERT_FALSE(IsHandlerDone("clear_fault_msg", "handler1"));
    ClearFaultHandlerResult("clear_fault_msg");
    ASSERT_EQ(GetResultFromHandlersByMsg("clear_fault_msg"), UBSE_OK);
    ASSERT_FALSE(IsHandlerDone("clear_fault_msg", "handler1"));
}

// ==================== ExecuteFaultHandler (无 msgId 重载) 测试 ====================

TEST_F(TestUbseRasHandler, ExecuteFaultHandlerWithoutMsgIdNoHandlerRegistered)
{
    auto& handler = UbseRasHandler::GetInstance();
    handler.faultHandlerMap.clear();
    ALARM_FAULT_TYPE type = ALARM_OOM_EVENT;
    std::string faultInfo = "test";
    auto res = handler.ExecuteFaultHandler(type, faultInfo);
    ASSERT_EQ(res, UBSE_OK);
}

TEST_F(TestUbseRasHandler, ExecuteFaultHandlerWithoutMsgIdWithHandler)
{
    auto& handler = UbseRasHandler::GetInstance();
    handler.faultHandlerMap.clear();
    ALARM_FAULT_TYPE type = ALARM_OOM_EVENT;
    std::string faultInfo = "test";
    AlarmFaultHandler func = [](ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo) -> uint32_t {
        return UBSE_OK;
    };
    handler.faultHandlerMap[type][AlarmHandlerPriority::HIGH].emplace_back("test_handler", func);
    auto res = handler.ExecuteFaultHandler(type, faultInfo);
    ASSERT_EQ(res, UBSE_OK);
}

// ==================== CallNodeHandle 测试 ====================

TEST_F(TestUbseRasHandler, CallNodeHandleWhenTypeNotRegistered)
{
    auto& handler = UbseRasHandler::GetInstance();
    auto saveHandler = handler.nodeHandlerMap;
    handler.nodeHandlerMap.clear();
    auto ret = handler.CallNodeHandle(NodeHandlerType::PRE_FAULT_STATE_HANDLER_TYPE, "1");
    handler.nodeHandlerMap = saveHandler;
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasHandler, CallNodeHandleWhenNodeIdEmpty)
{
    auto& handler = UbseRasHandler::GetInstance();
    NodeHandler func = [](const std::string& nodeId) -> uint32_t {
        return UBSE_OK;
    };
    auto saveHandler = handler.nodeHandlerMap;
    handler.nodeHandlerMap.clear();
    handler.nodeHandlerMap[NodeHandlerType::PRE_FAULT_STATE_HANDLER_TYPE].emplace_back(func);
    auto ret = handler.CallNodeHandle(NodeHandlerType::PRE_FAULT_STATE_HANDLER_TYPE, "");
    handler.nodeHandlerMap = saveHandler;
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasHandler, CallNodeHandleWhenHandlerIsNull)
{
    auto& handler = UbseRasHandler::GetInstance();
    auto saveHandler = handler.nodeHandlerMap;
    handler.nodeHandlerMap.clear();
    handler.nodeHandlerMap[NodeHandlerType::PRE_FAULT_STATE_HANDLER_TYPE].emplace_back(nullptr);
    auto ret = handler.CallNodeHandle(NodeHandlerType::PRE_FAULT_STATE_HANDLER_TYPE, "1");
    handler.nodeHandlerMap = saveHandler;
    ASSERT_EQ(ret, UBSE_ERROR);
}

// ==================== RegisterNodeHandler 空 handler 测试 ====================

TEST_F(TestUbseRasHandler, RegisterNodeHandlerWithNullHandler)
{
    auto& handler = UbseRasHandler::GetInstance();
    NodeHandler nullFunc = nullptr;
    auto saveHandler = handler.nodeHandlerMap;
    handler.nodeHandlerMap.clear();
    auto ret = handler.RegisterNodeHandler(NodeHandlerType::PRE_FAULT_STATE_HANDLER_TYPE, nullFunc);
    handler.nodeHandlerMap = saveHandler;
    ASSERT_EQ(ret, UBSE_ERROR_NULLPTR);
}

// ==================== HandleOomFault 额外路径测试 ====================

TEST_F(TestUbseRasHandler, HandleOomFaultWhenMsgFormatInvalid)
{
    auto& handler = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_OOM_EVENT, .pucParas = "invalid_format"};
    auto res = handler.HandleOomFault(&msg);
    ASSERT_EQ(res, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasHandler, HandleOomFaultWhenTaskModuleNotAvailable)
{
    auto& handler = UbseRasHandler::GetInstance();
    alarm_msg msg{.usAlarmId = ALARM_OOM_EVENT,
                  .pucParas = "1650_{nr_nid:1,nid:[0,-1,-1,-1,-1,-1,-1,-1],"
                              "sync:1,timeout:30000,reason:2}"};
    // Task module is not set up in test environment, so HandleOomFault will fail with UBSE_ERROR
    auto res = handler.HandleOomFault(&msg);
    ASSERT_EQ(res, UBSE_ERROR);
}

// ==================== ProcessExportObj 额外测试 ====================

TEST_F(TestUbseRasHandler, ProcessExportObjWithNonSuccessState)
{
    ubse::adapter_plugins::mmi::UbseMemBorrowExportBaseObj numaExporetObj;
    numaExporetObj.status.state = ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_EXPORT_RUNNING;
    std::unordered_map<std::string, DebtInfo> numaMemoryDebtInfoMap;
    EXPECT_NO_THROW(ProcessExportObj("Fd", "1", numaExporetObj, "1", numaMemoryDebtInfoMap));
}

TEST_F(TestUbseRasHandler, ProcessExportObjWithBorrowNodeIdSameAsCurrent)
{
    ubse::adapter_plugins::mmi::UbseMemBorrowExportBaseObj numaExporetObj;
    numaExporetObj.status.state = ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_EXPORT_SUCCESS;
    std::unordered_map<std::string, DebtInfo> numaMemoryDebtInfoMap;
    std::string borrowNodeId = "1"; // Same as current node id
    std::string lentNodeId = "2";
    MOCKER(GetBorrowNodeId).stubs().will(returnValue(borrowNodeId));
    MOCKER(GetLentNodeId).stubs().will(returnValue(lentNodeId));
    EXPECT_NO_THROW(ProcessExportObj("Fd", "1", numaExporetObj, "1", numaMemoryDebtInfoMap));
}

// ==================== ProcessImportObj 额外测试 ====================

TEST_F(TestUbseRasHandler, ProcessImportObjWithNonSuccessState)
{
    ubse::adapter_plugins::mmi::UbseMemBorrowImportBaseObj numaImportObj;
    numaImportObj.status.state = ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_IMPORT_RUNNING;
    std::unordered_map<std::string, DebtInfo> numaMemoryDebtInfoMap;
    EXPECT_NO_THROW(ProcessImportObj("Fd", "1", numaImportObj, "1", numaMemoryDebtInfoMap));
}

TEST_F(TestUbseRasHandler, ProcessImportObjWithLentNodeIdSameAsCurrent)
{
    ubse::adapter_plugins::mmi::UbseMemBorrowImportBaseObj numaImportObj;
    numaImportObj.status.state = ubse::adapter_plugins::mmi::UbseMemState::UBSE_MEM_IMPORT_SUCCESS;
    std::unordered_map<std::string, DebtInfo> numaMemoryDebtInfoMap;
    std::string borrowNodeId = "2";
    std::string lentNodeId = "1"; // Same as current node id
    MOCKER(GetBorrowNodeId).stubs().will(returnValue(borrowNodeId));
    MOCKER(GetLentNodeId).stubs().will(returnValue(lentNodeId));
    MOCKER(IsNodeInStaticList).stubs().will(returnValue(true));
    EXPECT_NO_THROW(ProcessImportObj("Fd", "1", numaImportObj, "1", numaMemoryDebtInfoMap));
}

// ==================== ProcessDebtInfo 空map测试 ====================

TEST_F(TestUbseRasHandler, ProcessDebtInfoWithEmptyMap)
{
    ubse::adapter_plugins::mmi::NodeMemDebtInfoMap memDebtInfoMap;
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> allNodeInfos;
    EXPECT_NO_THROW(ProcessDebtInfo(memDebtInfoMap, "1", allNodeInfos));
}

// ==================== LogMemDebtInfoWithNode 额外路径测试 ====================

TEST_F(TestUbseRasHandler, LogMemDebtInfoWithNodeWhenInStaticList)
{
    MOCKER(IsNodeInStaticList).stubs().will(returnValue(true));
    EXPECT_NO_THROW(LogMemDebtInfoWithNode(ALARM_OOM_EVENT, "1"));
}

TEST_F(TestUbseRasHandler, LogMemDebtInfoWithNodeWhenNotInStaticList)
{
    MOCKER(IsNodeInStaticList).stubs().will(returnValue(false));
    EXPECT_NO_THROW(LogMemDebtInfoWithNode(ALARM_OOM_EVENT, "1"));
}

// ==================== IsNodeInStaticList 测试 ====================

TEST_F(TestUbseRasHandler, IsNodeInStaticListEmptyList)
{
    std::vector<ubse::nodeMgr::UbseNodeStaticInfo> staticNodeInfos;
    auto result = IsNodeInStaticList("1", staticNodeInfos);
    ASSERT_FALSE(result);
}

// ==================== ParseOomEventInfo 测试 ====================

TEST_F(TestUbseRasHandler, ParseOomEventInfoValid)
{
    OomEventInfo eventInfo;
    auto ret =
        ParseOomEventInfo("1650_{nr_nid:1,nid:[0,-1,-1,-1,-1,-1,-1,-1],sync:1,timeout:30000,reason:2}", eventInfo);
    ASSERT_EQ(ret, UBSE_OK);
    ASSERT_EQ(eventInfo.msgId, "1650");
    ASSERT_EQ(eventInfo.sync, 1);
    ASSERT_EQ(eventInfo.reason, 2);
    ASSERT_EQ(eventInfo.nrNid, 1);
    // SplitNids in ubse_ras_handler.cpp filters negative nids
    ASSERT_EQ(eventInfo.nids.size(), 1u);
    ASSERT_EQ(eventInfo.nids[0], 0);
}

TEST_F(TestUbseRasHandler, ParseOomEventInfoValidAllNonNegative)
{
    OomEventInfo eventInfo;
    auto ret = ParseOomEventInfo("100_{nr_nid:3,nid:[0,1,2],sync:0,timeout:10000,reason:0}", eventInfo);
    ASSERT_EQ(ret, UBSE_OK);
    ASSERT_EQ(eventInfo.msgId, "100");
    ASSERT_EQ(eventInfo.sync, 0);
    ASSERT_EQ(eventInfo.reason, 0);
    ASSERT_EQ(eventInfo.nrNid, 3);
    ASSERT_EQ(eventInfo.nids.size(), 3u);
    ASSERT_EQ(eventInfo.nids[0], 0);
    ASSERT_EQ(eventInfo.nids[1], 1);
    ASSERT_EQ(eventInfo.nids[2], 2);
}

TEST_F(TestUbseRasHandler, ParseOomEventInfoInvalidFormat)
{
    OomEventInfo eventInfo;
    auto ret = ParseOomEventInfo("invalid_format", eventInfo);
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasHandler, ParseOomEventInfoInvalidMsgId)
{
    OomEventInfo eventInfo;
    auto ret = ParseOomEventInfo("abc_{nr_nid:1,nid:[0],sync:1,timeout:30000,reason:2}", eventInfo);
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseRasHandler, ParseOomEventInfoMissingBrace)
{
    OomEventInfo eventInfo;
    auto ret = ParseOomEventInfo("1650_{nr_nid:1,nid:[0],sync:1,timeout:30000,reason:2", eventInfo);
    ASSERT_EQ(ret, UBSE_ERROR_INVAL);
}

// ==================== BuildOomStrFromEventInfo 测试 ====================

TEST_F(TestUbseRasHandler, BuildOomStrFromEventInfoSingleNid)
{
    OomEventInfo eventInfo;
    eventInfo.msgId = "1650";
    eventInfo.sync = 1;
    eventInfo.reason = 2;
    eventInfo.nrNid = 1;
    eventInfo.nids = {0};
    auto result = BuildOomStrFromEventInfo(eventInfo);
    ASSERT_EQ(result, "1_0_2");
}

TEST_F(TestUbseRasHandler, BuildOomStrFromEventInfoMultipleNids)
{
    OomEventInfo eventInfo;
    eventInfo.msgId = "1650";
    eventInfo.sync = 1;
    eventInfo.reason = 2;
    eventInfo.nrNid = 3;
    eventInfo.nids = {0, 1, 2};
    auto result = BuildOomStrFromEventInfo(eventInfo);
    ASSERT_EQ(result, "3_0_1_2_2");
}

TEST_F(TestUbseRasHandler, BuildOomStrFromEventInfoEmptyNids)
{
    OomEventInfo eventInfo;
    eventInfo.msgId = "1650";
    eventInfo.sync = 1;
    eventInfo.reason = 0;
    eventInfo.nrNid = 0;
    eventInfo.nids = {};
    auto result = BuildOomStrFromEventInfo(eventInfo);
    ASSERT_EQ(result, "0_0");
}

// ==================== SplitNids (ubse_ras_handler.cpp 版本: 过滤负数) 测试 ====================

TEST_F(TestUbseRasHandler, SplitNidsFiltersNegative)
{
    auto result = SplitNids("0,-1,-1,-1");
    ASSERT_EQ(result.size(), 1u);
    ASSERT_EQ(result[0], 0);
}

TEST_F(TestUbseRasHandler, SplitNidsAllNegative)
{
    auto result = SplitNids("-1,-2,-3");
    ASSERT_TRUE(result.empty());
}

TEST_F(TestUbseRasHandler, SplitNidsAllNonNegative)
{
    auto result = SplitNids("0,1,2,3");
    ASSERT_EQ(result.size(), 4u);
}

TEST_F(TestUbseRasHandler, SplitNidsEmptyString)
{
    auto result = SplitNids("");
    ASSERT_TRUE(result.empty());
}

// ==================== ClearExpiredHandlerResult 测试 ====================

TEST_F(TestUbseRasHandler, ClearExpiredHandlerResultRemovesExpiredOomEntry)
{
    ClearAllHandlerResults();
    SetHandlerResult(ALARM_OOM_EVENT, "expired_msg", "handler1", UBSE_OK);
    // Set timestamp to 0 to make it long-expired
    {
        std::lock_guard<std::mutex> lock(g_handlerResultMutex);
        g_handlerResultMap["expired_msg"]["handler1"].timestamp = 0;
    }
    ClearExpiredHandlerResult();
    ASSERT_FALSE(IsHandlerDone("expired_msg", "handler1"));
}

TEST_F(TestUbseRasHandler, ClearExpiredHandlerResultKeepsRecentEntry)
{
    ClearAllHandlerResults();
    SetHandlerResult(ALARM_OOM_EVENT, "recent_msg", "handler1", UBSE_OK);
    // timestamp is current time from SetHandlerResult, should NOT be expired
    ClearExpiredHandlerResult();
    ASSERT_TRUE(IsHandlerDone("recent_msg", "handler1"));
}

TEST_F(TestUbseRasHandler, ClearExpiredHandlerResultKeepsNonOomEntry)
{
    ClearAllHandlerResults();
    SetHandlerResult(ALARM_REBOOT_EVENT, "non_oom_msg", "handler1", UBSE_OK);
    // Even with old timestamp, non-OOM entries should survive
    {
        std::lock_guard<std::mutex> lock(g_handlerResultMutex);
        g_handlerResultMap["non_oom_msg"]["handler1"].timestamp = 0;
    }
    ClearExpiredHandlerResult();
    ASSERT_TRUE(IsHandlerDone("non_oom_msg", "handler1"));
}

TEST_F(TestUbseRasHandler, ClearExpiredHandlerResultMixedEntries)
{
    ClearAllHandlerResults();
    // Expired OOM entry
    SetHandlerResult(ALARM_OOM_EVENT, "mixed_msg", "oom_handler", UBSE_OK);
    {
        std::lock_guard<std::mutex> lock(g_handlerResultMutex);
        g_handlerResultMap["mixed_msg"]["oom_handler"].timestamp = 0;
    }
    // Non-expired OOM entry in same msg
    SetHandlerResult(ALARM_OOM_EVENT, "mixed_msg", "recent_handler", UBSE_OK);
    ClearExpiredHandlerResult();
    ASSERT_FALSE(IsHandlerDone("mixed_msg", "oom_handler"));
    ASSERT_TRUE(IsHandlerDone("mixed_msg", "recent_handler"));
}

TEST_F(TestUbseRasHandler, ClearExpiredHandlerResultEmptyMap)
{
    ClearAllHandlerResults();
    EXPECT_NO_THROW(ClearExpiredHandlerResult());
}

// ==================== IsPendingFaultExisted / AddPendingFaultId / DelPendingFaultId 测试 ====================

TEST_F(TestUbseRasHandler, AddPendingFaultIdNew)
{
    auto& handler = UbseRasHandler::GetInstance();
    ASSERT_FALSE(handler.IsPendingFaultExisted("pending_new"));
    ASSERT_TRUE(handler.AddPendingFaultId("pending_new"));
    ASSERT_TRUE(handler.IsPendingFaultExisted("pending_new"));
    handler.DelPendingFaultId("pending_new");
}

TEST_F(TestUbseRasHandler, AddPendingFaultIdDuplicate)
{
    auto& handler = UbseRasHandler::GetInstance();
    ASSERT_TRUE(handler.AddPendingFaultId("pending_dup"));
    ASSERT_FALSE(handler.AddPendingFaultId("pending_dup"));
    handler.DelPendingFaultId("pending_dup");
}

TEST_F(TestUbseRasHandler, IsPendingFaultExistedNotFound)
{
    auto& handler = UbseRasHandler::GetInstance();
    ASSERT_FALSE(handler.IsPendingFaultExisted("nonexistent_pending_fault"));
}

TEST_F(TestUbseRasHandler, DelPendingFaultIdRemovesEntry)
{
    auto& handler = UbseRasHandler::GetInstance();
    ASSERT_TRUE(handler.AddPendingFaultId("pending_del"));
    ASSERT_TRUE(handler.IsPendingFaultExisted("pending_del"));
    handler.DelPendingFaultId("pending_del");
    ASSERT_FALSE(handler.IsPendingFaultExisted("pending_del"));
}

TEST_F(TestUbseRasHandler, DelPendingFaultIdNonexistent)
{
    auto& handler = UbseRasHandler::GetInstance();
    ASSERT_FALSE(handler.IsPendingFaultExisted("pending_nonexist"));
    EXPECT_NO_THROW(handler.DelPendingFaultId("pending_nonexist"));
}

TEST_F(TestUbseRasHandler, AddPendingFaultIdMultipleDifferent)
{
    auto& handler = UbseRasHandler::GetInstance();
    ASSERT_TRUE(handler.AddPendingFaultId("pending_multi_1"));
    ASSERT_TRUE(handler.AddPendingFaultId("pending_multi_2"));
    ASSERT_TRUE(handler.IsPendingFaultExisted("pending_multi_1"));
    ASSERT_TRUE(handler.IsPendingFaultExisted("pending_multi_2"));
    handler.DelPendingFaultId("pending_multi_1");
    ASSERT_FALSE(handler.IsPendingFaultExisted("pending_multi_1"));
    ASSERT_TRUE(handler.IsPendingFaultExisted("pending_multi_2"));
    handler.DelPendingFaultId("pending_multi_2");
}
} // namespace ubse::ras::ut
