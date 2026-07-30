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

#include "test_ubse_mem_controller_fault_handle.h"

#include <mockcpp/mockcpp.hpp>
#include "ubse_api_server_module.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_event.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_fault_handle.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_mem_single_import_message.h"
#include "ubse_ras.h"
#include "ubse_serial_util.h"
#include "ubse_thread_pool_module.h"
#include "ubse_timer.h"
#include "ubse_mem_controller_fault_handle.cpp"
#include "ubse_mem_share_store.h"
#include "ubse_mem_global_ledger_summary_store.h"
#include "ubse_mem_controller_helper.h"
#include "ubse_node_static_info_mgr.h"

namespace ubse::mem_controller::ut {
using namespace ubse::ras;
using namespace ubse::mem::controller;
using namespace ubse::config;
using namespace ubse::context;
using namespace ubse::election;
using namespace ubse::serial;
using namespace ubse::event;
using namespace ubse::mem::def;
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::task_executor;
using namespace ubse::nodeMgr;

// Mock IsHierarchicalMode: true=双层选主, false=非双层
// GetRootIpList 返回 vector<string> 可直接 mock；GetAllNodes 返回 vector<UbseNodeStaticInfo>
// 因 UbseNodeStaticInfo 无 operator==，mockcpp 无法 mock，改用 SetNodes 设置真实数据
static void MockHierarchicalMode(bool hierarchical)
{
    if (hierarchical) {
        std::vector<std::string> emptyRoots;
        MOCKER_CPP(ubse::nodeMgr::GetRootIpList).stubs().will(returnValue(emptyRoots));
        std::vector<UbseNodeStaticInfo> nodes;
        UbseNodeStaticInfo n1; n1.nodeId = "ut_hier_node1";
        UbseNodeStaticInfo n2; n2.nodeId = "ut_hier_node2";
        nodes.push_back(n1);
        nodes.push_back(n2);
        UbseNodeStaticInfoMgr::GetInstance().SetNodes(nodes);
    } else {
        std::vector<std::string> roots{"10.0.0.1"};
        MOCKER_CPP(ubse::nodeMgr::GetRootIpList).stubs().will(returnValue(roots));
    }
}

void TestUbseMemControllerFaultHandle::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerFaultHandle::TearDown()
{
    UbseMemFaultManager::executorPtr = nullptr;
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    g_pendingFaultEvents.clear();
    g_portDownRecords.clear();
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerFaultHandle, InitMemFaultManager_CreateTaskExecutorFailed)
{
    MOCKER(&UbseMemFaultManager::CreateTaskExecutor).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseMemFaultManager::InitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, InitMemFaultManager_RegisterAlarmFailed)
{
    MOCKER(&UbseMemFaultManager::CreateTaskExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseRegRpcService).stubs().will(returnValue(UBSE_OK));
    MOCKER(RegisterAlarmFaultHandler, uint32_t(ALARM_FAULT_TYPE, std::string, AlarmFaultHandler, AlarmHandlerPriority))
        .stubs()
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseMemFaultManager::InitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, InitMemFaultManager_SubEventFailed)
{
    MOCKER(&UbseMemFaultManager::CreateTaskExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseRegRpcService).stubs().will(returnValue(UBSE_OK));
    MOCKER(RegisterAlarmFaultHandler, uint32_t(ALARM_FAULT_TYPE, std::string, AlarmFaultHandler, AlarmHandlerPriority))
        .stubs()
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseSubEvent).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseMemFaultManager::InitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, InitMemFaultManager_Success)
{
    MOCKER(&UbseMemFaultManager::CreateTaskExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseRegRpcService).stubs().will(returnValue(UBSE_OK));
    MOCKER(RegisterAlarmFaultHandler, uint32_t(ALARM_FAULT_TYPE, std::string, AlarmFaultHandler, AlarmHandlerPriority))
        .stubs()
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseSubEvent).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, UbseMemFaultManager::InitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, InitMemFaultManager_TimerRegisterFailed)
{
    MOCKER(&UbseMemFaultManager::CreateTaskExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseRegRpcService).stubs().will(returnValue(UBSE_OK));
    MOCKER(RegisterAlarmFaultHandler, uint32_t(ALARM_FAULT_TYPE, std::string, AlarmFaultHandler, AlarmHandlerPriority))
        .stubs()
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseSubEvent).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::timer::UbseTimerHandlerRegister).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseMemFaultManager::InitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, InitMemFaultManager_PortUpDownSubEventFailed)
{
    MOCKER(&UbseMemFaultManager::CreateTaskExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseRegRpcService).stubs().will(returnValue(UBSE_OK));
    MOCKER(RegisterAlarmFaultHandler, uint32_t(ALARM_FAULT_TYPE, std::string, AlarmFaultHandler, AlarmHandlerPriority))
        .stubs()
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseSubEvent).stubs().will(returnValue(UBSE_OK)).then(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseMemFaultManager::InitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, DeInitMemFaultManager_UnRegisterFailed)
{
    MOCKER_CPP(ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(UnRegisterAlarmFaultHandler).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseMemFaultManager::DeInitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, DeInitMemFaultManager_RemoveTaskExecutorFailed)
{
    MOCKER_CPP(ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(UnRegisterAlarmFaultHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::event::UbseUnSubEvent).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseMemFaultManager::RemoveTaskExecutor).stubs().will(returnValue(UBSE_ERROR_NULLPTR));
    EXPECT_EQ(UBSE_ERROR_NULLPTR, UbseMemFaultManager::DeInitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, DeInitMemFaultManager_Success)
{
    MOCKER_CPP(ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(UnRegisterAlarmFaultHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::event::UbseUnSubEvent).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseMemFaultManager::RemoveTaskExecutor).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, UbseMemFaultManager::DeInitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, PanicRebootFaultEventHandler_EmptyMessage)
{
    std::string eventId = "UbsePanicAndRebootFaultLocalEvent";
    std::string eventMessage;
    EXPECT_EQ(UbseMemFaultManager::PanicRebootFaultEventHandler(eventId, eventMessage), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemControllerFaultHandle, PanicRebootFaultEventHandler_InvalidFormat)
{
    std::string eventId = "UbsePanicAndRebootFaultLocalEvent";
    std::string eventMessage = "invalid_no_underscore";
    EXPECT_NE(UbseMemFaultManager::PanicRebootFaultEventHandler(eventId, eventMessage), UBSE_OK);

    std::string eventMessage2 = "nodeId_invalid_type";
    EXPECT_NE(UbseMemFaultManager::PanicRebootFaultEventHandler(eventId, eventMessage2), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, PanicRebootFaultEventHandler_InvalidFaultType)
{
    std::string eventId = "UbsePanicAndRebootFaultLocalEvent";
    std::string eventMessage = "1_not_a_number";
    EXPECT_NE(UbseMemFaultManager::PanicRebootFaultEventHandler(eventId, eventMessage), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, MemReportWhenExportNodeOnFault_EmptyFaultId)
{
    std::string faultId;
    EXPECT_EQ(UbseMemFaultManager::MemReportWhenExportNodeOnFault(ALARM_MEM_FAULT, faultId), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemControllerFaultHandle, MemReportWhenExportNodeOnFault_NullExecutor)
{
    UbseMemFaultManager::executorPtr = nullptr;
    std::string faultId = "1";
    EXPECT_EQ(UbseMemFaultManager::MemReportWhenExportNodeOnFault(ALARM_MEM_FAULT, faultId), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemControllerFaultHandle, ParseFaultInfo_InvalidJson)
{
    uint64_t memId = 0;
    UbMemFaultType type;
    EXPECT_NE(ParseFaultInfo("not json", memId, type), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, ParseFaultInfo_MissingMemId)
{
    uint64_t memId = 0;
    UbMemFaultType type;
    EXPECT_NE(ParseFaultInfo(R"({"raw_ubus_mem_err_type": 0})", memId, type), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, ParseFaultInfo_MissingFaultType)
{
    uint64_t memId = 0;
    UbMemFaultType type;
    EXPECT_NE(ParseFaultInfo(R"({"memid": 1})", memId, type), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, ParseFaultInfo_ValidJson)
{
    uint64_t memId = 0;
    UbMemFaultType type;
    EXPECT_EQ(ParseFaultInfo(R"({"memid": 100, "raw_ubus_mem_err_type": 0})", memId, type), UBSE_OK);
    EXPECT_EQ(memId, 100);
    EXPECT_EQ(type, UB_MEM_ATOMIC_DATA_ERR);
}

TEST_F(TestUbseMemControllerFaultHandle, FindNameByMemIdInImportObj_GetNodeIdFailed)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    uint64_t memId = 1;
    std::string memName;
    std::string memType;
    UbseUdsInfo udsInfo;
    uint64_t handleId = 0;
    EXPECT_NE(FindNameByMemIdInImportObj(memId, memName, memType, udsInfo, handleId), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, FindNameByMemIdInImportObj_FoundInShareImport)
{
    UbseRoleInfo roleInfo("1", "master");
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));

    UbseMemShareBorrowImportObj shareImportObj{};
    shareImportObj.req.name = "shareTest";
    shareImportObj.importNodeId = "1";
    shareImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    shareImportObj.status.importResults.emplace_back(UbseMemImportResult{.memId = 100});
    UbseMemDebtNumaInfo exportNmaInfo{.nodeId = "2", .socketId = 0, .numaId = 0, .size = 128};
    shareImportObj.algoResult.exportNumaInfos.emplace_back(exportNmaInfo);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("1", "shareTest",
                                                                                           shareImportObj);

    uint64_t memId = 100;
    std::string memName;
    std::string memType;
    UbseUdsInfo udsInfo;
    uint64_t handleId = 0;
    EXPECT_EQ(FindNameByMemIdInImportObj(memId, memName, memType, udsInfo, handleId), UBSE_OK);
    EXPECT_EQ(memName, "shareTest");
    EXPECT_EQ(memType, "share");
    EXPECT_EQ(handleId, 100);
}

TEST_F(TestUbseMemControllerFaultHandle, FindNameByMemIdInImportObj_FoundInFdImport)
{
    UbseRoleInfo roleInfo("1", "master");
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));

    UbseMemFdBorrowImportObj fdImportObj{};
    fdImportObj.req.name = "fdTest";
    fdImportObj.req.importNodeId = "1";
    fdImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    fdImportObj.status.importResults.emplace_back(UbseMemImportResult{.memId = 200});
    UbseMemDebtNumaInfo exportNmaInfo{.nodeId = "2", .socketId = 0, .numaId = 0, .size = 128};
    fdImportObj.algoResult.exportNumaInfos.emplace_back(exportNmaInfo);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("1", "fdTest", fdImportObj);

    uint64_t memId = 200;
    std::string memName;
    std::string memType;
    UbseUdsInfo udsInfo;
    uint64_t handleId = 0;
    EXPECT_EQ(FindNameByMemIdInImportObj(memId, memName, memType, udsInfo, handleId), UBSE_OK);
    EXPECT_EQ(memName, "fdTest");
    EXPECT_EQ(memType, "fd");
    EXPECT_EQ(handleId, 200);
}

TEST_F(TestUbseMemControllerFaultHandle, FindNameByMemIdInImportObj_NotFound)
{
    UbseRoleInfo roleInfo("1", "master");
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));

    uint64_t memId = 999;
    std::string memName;
    std::string memType;
    UbseUdsInfo udsInfo;
    uint64_t handleId = 0;
    EXPECT_NE(FindNameByMemIdInImportObj(memId, memName, memType, udsInfo, handleId), UBSE_OK);
    EXPECT_EQ(memType, "unknown");
}

TEST_F(TestUbseMemControllerFaultHandle, SendMemFaultMessageByType_NullExecutor)
{
    UbseMemFaultManager::executorPtr = nullptr;
    UbseUdsInfo udsInfo{};
    EXPECT_EQ(UbseMemFaultManager::SendMemFaultMessageByType("share", 1, "test", udsInfo, UB_MEM_ATOMIC_DATA_ERR),
              UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemControllerFaultHandle, MemFaultHandler_ParseFaultInfoFailed)
{
    EXPECT_NE(UbseMemFaultManager::MemFaultHandler(ALARM_MEM_FAULT, "invalid"), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, MemFaultHandler_FindNameFailed)
{
    UbseRoleInfo roleInfo("1", "master");
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));
    EXPECT_NE(UbseMemFaultManager::MemFaultHandler(ALARM_MEM_FAULT, R"({"memid": 999, "raw_ubus_mem_err_type": 0})"),
              UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, BmcFaultHandler_NotMaster)
{
    UbseRoleInfo curNodeInfo("1", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFaultManager::BmcFaultHandler(ALARM_REBOOT_EVENT, "faultNode1"), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, BmcFaultHandler_GetNodeInfoFailed)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UbseMemFaultManager::BmcFaultHandler(ALARM_REBOOT_EVENT, "faultNode1"), UBSE_ERROR);
}

TEST_F(TestUbseMemControllerFaultHandle, BmcFaultHandler_SendFailed)
{
    UbseRoleInfo curNodeInfo("master", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::vector<UbseRoleInfo> roleInfos;
    roleInfos.emplace_back("agent1", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().with(outBound(roleInfos)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(UbseMemFaultManager::BmcFaultHandler(ALARM_REBOOT_EVENT, "faultNode1"), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, BmcFaultHandler_DuplicateEvent)
{
    UbseRoleInfo curNodeInfo("master", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::vector<UbseRoleInfo> roleInfos;
    roleInfos.emplace_back("agent1", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().with(outBound(roleInfos)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(UbseMemFaultManager::BmcFaultHandler(ALARM_REBOOT_EVENT, "faultNode1"), UBSE_OK);
    EXPECT_EQ(UbseMemFaultManager::BmcFaultHandler(ALARM_REBOOT_EVENT, "faultNode1"), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, BmcFaultHandler_NoNodesAvailable)
{
    UbseRoleInfo curNodeInfo("master", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::vector<UbseRoleInfo> roleInfos;
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().with(outBound(roleInfos)).will(returnValue(UBSE_OK));

    EXPECT_EQ(UbseMemFaultManager::BmcFaultHandler(ALARM_REBOOT_EVENT, "faultNode1"), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, BmcFaultHandler_GetAllNodeInfosFailed)
{
    UbseRoleInfo curNodeInfo("master", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(UbseMemFaultManager::BmcFaultHandler(ALARM_REBOOT_EVENT, "faultNode1"), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, FaultDeliverTimerHandler_NullExecutor)
{
    UbseMemFaultManager::executorPtr = nullptr;
    EXPECT_EQ(UbseMemFaultManager::FaultDeliverTimerHandler(), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, FaultDeliverTimerHandler_ProcessesPendingEvents)
{
    UbseMemFaultManager::executorPtr = nullptr;

    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));

    std::vector<UbseRoleInfo> roleInfos;
    roleInfos.emplace_back("agent1", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().with(outBound(roleInfos)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));

    PushFaultEvent("faultNode1", static_cast<uint32_t>(ALARM_REBOOT_EVENT));

    ProcessFaultEvents();

    EXPECT_TRUE(g_pendingFaultEvents.empty());
}

TEST_F(TestUbseMemControllerFaultHandle, FaultAgentsNotifyHandler_DeserializeFailed)
{
    UbseByteBuffer req{.data = nullptr, .len = 0, .freeFunc = nullptr};
    UbseByteBuffer resp{.data = nullptr, .len = 0, .freeFunc = nullptr};

    UbseMemFaultManager::FaultAgentsNotifyHandler(req, resp);
}

TEST_F(TestUbseMemControllerFaultHandle, FaultAgentsNotifyHandler_Success)
{
    std::string faultNodeId = "faultNode1";
    uint32_t faultTypeValue = static_cast<uint32_t>(ALARM_REBOOT_EVENT);

    UbseSerialization output;
    output << faultNodeId << faultTypeValue;
    ASSERT_TRUE(output.Check());

    UbseByteBuffer req{.data = output.GetBuffer(), .len = output.GetLength(), .freeFunc = nullptr};
    UbseByteBuffer resp{.data = nullptr, .len = 0, .freeFunc = nullptr};

    MOCKER(&UbseMemFaultManager::MemReportWhenExportNodeOnFault).stubs().will(returnValue(UBSE_OK));

    UbseMemFaultManager::FaultAgentsNotifyHandler(req, resp);
}

TEST_F(TestUbseMemControllerFaultHandle, SingleImportDebtNotifyHandler_DeserializeFailed)
{
    UbseByteBuffer req{.data = nullptr, .len = 0, .freeFunc = nullptr};
    UbseByteBuffer resp{.data = nullptr, .len = 0, .freeFunc = nullptr};

    UbseMemFaultManager::SingleImportDebtNotifyHandler(req, resp);
}

TEST_F(TestUbseMemControllerFaultHandle, SingleImportDebtNotifyHandler_Success)
{
    ubse::mem::controller::message::UbseMemSingleImportMessage msg;

    ubse::mem::def::ShareHandleInfo shareInfo{};
    shareInfo.name = "shareTest";
    shareInfo.memIds.insert(100);
    shareInfo.udsInfo.uid = 1000;
    shareInfo.udsInfo.gid = 1000;
    shareInfo.udsInfo.pid = 1;

    ubse::mem::def::NumaHandleInfo numaInfo{};
    numaInfo.name = "numaTest";
    numaInfo.numaIds.insert(1);
    numaInfo.udsInfo.uid = 1000;
    numaInfo.udsInfo.gid = 1000;
    numaInfo.udsInfo.pid = 1;

    ubse::mem::def::FdHandleInfo fdInfo{};
    fdInfo.name = "fdTest";
    fdInfo.memIds.insert(300);
    fdInfo.udsInfo.uid = 1000;
    fdInfo.udsInfo.gid = 1000;
    fdInfo.udsInfo.pid = 1;

    ubse::mem::def::ShareHandleInfoVec shareVec;
    ubse::mem::def::NumaHandleInfoVec numaVec;
    ubse::mem::def::FdHandleInfoVec fdVec;

    shareVec.push_back(shareInfo);
    numaVec.push_back(numaInfo);
    fdVec.push_back(fdInfo);

    msg.SetShareHandleInfoVec(shareVec);
    msg.SetNumaHandleInfoVec(numaVec);
    msg.SetFdHandleInfoVec(fdVec);
    msg.Serialize();

    UbseByteBuffer req{.data = msg.SerializedData(), .len = msg.SerializedDataSize(), .freeFunc = [](uint8_t*) -> void {
                       }};
    UbseByteBuffer resp{.data = nullptr, .len = 0, .freeFunc = nullptr};

    using ExecuteFuncType = bool (UbseTaskExecutor::*)(const std::function<void()>&);
    MOCKER(static_cast<ExecuteFuncType>(&UbseTaskExecutor::Execute)).stubs().will(returnValue(true));
    auto mockExecutor = UbseTaskExecutor::Create("test", 1, 1000);
    UbseMemFaultManager::executorPtr = mockExecutor;

    UbseMemFaultManager::SingleImportDebtNotifyHandler(req, resp);
}

TEST_F(TestUbseMemControllerFaultHandle, SingleImportDebtNotifyHandler_NullExecutor)
{
    UbseMemFaultManager::executorPtr = nullptr;

    ubse::mem::controller::message::UbseMemSingleImportMessage msg;

    ubse::mem::def::ShareHandleInfo shareInfo{};
    shareInfo.name = "shareTest";
    shareInfo.memIds.insert(100);
    shareInfo.udsInfo.uid = 1000;
    shareInfo.udsInfo.gid = 1000;
    shareInfo.udsInfo.pid = 1;

    ubse::mem::def::ShareHandleInfoVec shareVec;
    shareVec.push_back(shareInfo);

    msg.SetShareHandleInfoVec(shareVec);
    msg.Serialize();

    UbseByteBuffer req{.data = msg.SerializedData(), .len = msg.SerializedDataSize(), .freeFunc = [](uint8_t*) -> void {
                       }};
    UbseByteBuffer resp{.data = nullptr, .len = 0, .freeFunc = nullptr};

    UbseMemFaultManager::SingleImportDebtNotifyHandler(req, resp);
}

TEST_F(TestUbseMemControllerFaultHandle, ReportSingleImportDebt_EmptyHandles)
{
    ubse::mem::def::ShareHandleInfoVec shareVec;
    ubse::mem::def::NumaHandleInfoVec numaVec;
    ubse::mem::def::FdHandleInfoVec fdVec;

    EXPECT_EQ(UbseMemFaultManager::ReportSingleImportDebt("targetNode1", shareVec, numaVec, fdVec), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, ReportSingleImportDebt_WithHandlesSuccess)
{
    ubse::mem::def::ShareHandleInfoVec shareVec;
    ubse::mem::def::NumaHandleInfoVec numaVec;
    ubse::mem::def::FdHandleInfoVec fdVec;

    ubse::mem::def::ShareHandleInfo shareInfo{};
    shareInfo.name = "shareTest";
    shareInfo.memIds.insert(100);
    shareInfo.udsInfo.uid = 1000;
    shareInfo.udsInfo.gid = 1000;
    shareInfo.udsInfo.pid = 1;
    shareVec.push_back(shareInfo);

    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(UbseMemFaultManager::ReportSingleImportDebt("targetNode1", shareVec, numaVec, fdVec), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, ReportSingleImportDebt_SendFailed)
{
    ubse::mem::def::ShareHandleInfoVec shareVec;
    ubse::mem::def::NumaHandleInfoVec numaVec;
    ubse::mem::def::FdHandleInfoVec fdVec;

    ubse::mem::def::ShareHandleInfo shareInfo{};
    shareInfo.name = "shareTest";
    shareInfo.memIds.insert(100);
    shareInfo.udsInfo.uid = 1000;
    shareInfo.udsInfo.gid = 1000;
    shareInfo.udsInfo.pid = 1;
    shareVec.push_back(shareInfo);

    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_NE(UbseMemFaultManager::ReportSingleImportDebt("targetNode1", shareVec, numaVec, fdVec), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, SubmitMemReportTaskWhenNodeStopsMemoryService_GetNodeIdFailed)
{
    MOCKER_CPP(UbseGetCurrentNodeId).stubs().will(returnValue(UBSE_ERROR));

    SubmitMemReportTaskWhenNodeStopsMemoryService("faultId1");
}

TEST_F(TestUbseMemControllerFaultHandle, SubmitMemReportTaskWhenNodeStopsMemoryService_QueryShareFailed)
{
    MOCKER_CPP(UbseGetCurrentNodeId).stubs().will(returnValue(UBSE_OK));

    MOCKER_CPP(debt::UbseQueryShareImportHandleByExportNodeId).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(debt::UbseQueryNumaImportHandleByExportNodeId).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(debt::UbseQueryFdImportHandleByExportNodeId).stubs().will(returnValue(UBSE_OK));

    SubmitMemReportTaskWhenNodeStopsMemoryService("faultId1");
}

TEST_F(TestUbseMemControllerFaultHandle, SubmitMemReportTaskWhenNodeStopsMemoryService_QueryNumaFailed)
{
    MOCKER_CPP(UbseGetCurrentNodeId).stubs().will(returnValue(UBSE_OK));

    MOCKER_CPP(debt::UbseQueryShareImportHandleByExportNodeId).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(debt::UbseQueryNumaImportHandleByExportNodeId).stubs().will(returnValue(UBSE_ERROR));
    MOCKER_CPP(debt::UbseQueryFdImportHandleByExportNodeId).stubs().will(returnValue(UBSE_OK));

    SubmitMemReportTaskWhenNodeStopsMemoryService("faultId1");
}

TEST_F(TestUbseMemControllerFaultHandle, SubmitMemReportTaskWhenNodeStopsMemoryService_QueryFdFailed)
{
    MOCKER_CPP(UbseGetCurrentNodeId).stubs().will(returnValue(UBSE_OK));

    MOCKER_CPP(debt::UbseQueryShareImportHandleByExportNodeId).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(debt::UbseQueryNumaImportHandleByExportNodeId).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(debt::UbseQueryFdImportHandleByExportNodeId).stubs().will(returnValue(UBSE_ERROR));

    SubmitMemReportTaskWhenNodeStopsMemoryService("faultId1");
}

TEST_F(TestUbseMemControllerFaultHandle, SubmitMemReportTaskWhenNodeStopsMemoryService_AllSuccess)
{
    MOCKER_CPP(UbseGetCurrentNodeId).stubs().will(returnValue(UBSE_OK));

    MOCKER_CPP(debt::UbseQueryShareImportHandleByExportNodeId).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(debt::UbseQueryNumaImportHandleByExportNodeId).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(debt::UbseQueryFdImportHandleByExportNodeId).stubs().will(returnValue(UBSE_OK));

    SubmitMemReportTaskWhenNodeStopsMemoryService("faultId1");
}

TEST_F(TestUbseMemControllerFaultHandle, SendSingleFaultMemBlockMessage_ApiServerNullptr)
{
    std::shared_ptr<UbseApiServerModule> nullPtr;
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(nullPtr));

    UbseMemFault fault{.memName = "testMem", .handleId = 100, .type = UbseIpcMemFaultType::UB_MEM_ATOMIC_DATA_ERR};
    UbseUdsInfo udsInfo{.uid = 1000, .gid = 1000, .pid = 1};

    auto result =
        SendSingleFaultMemBlockMessage<UBSE_LONGLINK_FAULT_SHM, UBSE_LONG_LINK_REGISTER>("share", fault, udsInfo);
    EXPECT_NE(result, UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, SendSingleFaultMemBlockMessage_SerializeFailed)
{
    auto apiServerPtr = std::make_shared<UbseApiServerModule>();
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerPtr));
    MOCKER(SerializeMemFault).stubs().will(returnValue(UBSE_ERROR_SERIALIZE_FAILED));

    UbseMemFault fault{.memName = "testMem", .handleId = 100, .type = UbseIpcMemFaultType::UB_MEM_ATOMIC_DATA_ERR};
    UbseUdsInfo udsInfo{.uid = 1000, .gid = 1000, .pid = 1};

    auto result =
        SendSingleFaultMemBlockMessage<UBSE_LONGLINK_FAULT_SHM, UBSE_LONG_LINK_REGISTER>("share", fault, udsInfo);
    EXPECT_NE(result, UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, MemFaultHandler_SuccessShareType)
{
    UbseRoleInfo roleInfo("1", "master");
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));

    UbseMemShareBorrowImportObj shareImportObj{};
    shareImportObj.req.name = "shareTest";
    shareImportObj.importNodeId = "1";
    shareImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    shareImportObj.status.importResults.emplace_back(UbseMemImportResult{.memId = 100});
    shareImportObj.req.udsInfo.uid = 1000;
    shareImportObj.req.udsInfo.gid = 1000;
    shareImportObj.req.udsInfo.pid = 1;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("1", "shareTest",
                                                                                           shareImportObj);

    using ExecuteFuncType = bool (UbseTaskExecutor::*)(const std::function<void()>&);
    MOCKER(static_cast<ExecuteFuncType>(&UbseTaskExecutor::Execute)).stubs().will(returnValue(true));
    auto mockExecutor = UbseTaskExecutor::Create("test", 1, 1000);
    UbseMemFaultManager::executorPtr = mockExecutor;

    EXPECT_EQ(UbseMemFaultManager::MemFaultHandler(ALARM_MEM_FAULT, R"({"memid": 100, "raw_ubus_mem_err_type": 0})"),
              UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, MemFaultHandler_SuccessFdType)
{
    UbseRoleInfo roleInfo("1", "master");
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));

    UbseMemFdBorrowImportObj fdImportObj{};
    fdImportObj.req.name = "fdTest";
    fdImportObj.req.importNodeId = "1";
    fdImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    fdImportObj.status.importResults.emplace_back(UbseMemImportResult{.memId = 200});
    fdImportObj.req.udsInfo.uid = 1000;
    fdImportObj.req.udsInfo.gid = 1000;
    fdImportObj.req.udsInfo.pid = 1;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemFdBorrowImportObj>().PutResource("1", "fdTest", fdImportObj);

    using ExecuteFuncType = bool (UbseTaskExecutor::*)(const std::function<void()>&);
    MOCKER(static_cast<ExecuteFuncType>(&UbseTaskExecutor::Execute)).stubs().will(returnValue(true));
    auto executor = UbseTaskExecutor::Create("test", 1, 1000);
    UbseMemFaultManager::executorPtr = executor;

    EXPECT_EQ(UbseMemFaultManager::MemFaultHandler(ALARM_MEM_FAULT, R"({"memid": 200, "raw_ubus_mem_err_type": 0})"),
              UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, MemFaultHandler_SuccessNumaType)
{
    UbseRoleInfo roleInfo("1", "master");
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));

    UbseMemNumaBorrowImportObj numaImportObj{};
    numaImportObj.req.name = "numaTest";
    numaImportObj.req.importNodeId = "1";
    numaImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemImportResult numaResult{};
    numaResult.memId = 300;
    numaResult.numaId = 1;
    numaImportObj.status.importResults.emplace_back(numaResult);
    numaImportObj.req.udsInfo.uid = 1000;
    numaImportObj.req.udsInfo.gid = 1000;
    numaImportObj.req.udsInfo.pid = 1;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource("1", "numaTest",
                                                                                          numaImportObj);

    using ExecuteFuncType = bool (UbseTaskExecutor::*)(const std::function<void()>&);
    MOCKER(static_cast<ExecuteFuncType>(&UbseTaskExecutor::Execute)).stubs().will(returnValue(true));
    auto executor = UbseTaskExecutor::Create("test", 1, 1000);
    UbseMemFaultManager::executorPtr = executor;

    EXPECT_EQ(UbseMemFaultManager::MemFaultHandler(ALARM_MEM_FAULT, R"({"memid": 300, "raw_ubus_mem_err_type": 0})"),
              UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, FindNameByMemIdInImportObj_FoundInNumaImport)
{
    UbseRoleInfo roleInfo("1", "master");
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));

    UbseMemNumaBorrowImportObj numaImportObj{};
    numaImportObj.req.name = "numaTest";
    numaImportObj.req.importNodeId = "1";
    numaImportObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemImportResult numaResult{};
    numaResult.memId = 300;
    numaResult.numaId = 5;
    numaImportObj.status.importResults.emplace_back(numaResult);
    numaImportObj.req.udsInfo.uid = 1000;
    numaImportObj.req.udsInfo.gid = 1000;
    numaImportObj.req.udsInfo.pid = 1;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource("1", "numaTest",
                                                                                          numaImportObj);

    uint64_t memId = 300;
    std::string memName;
    std::string memType;
    UbseUdsInfo udsInfo;
    uint64_t handleId = 0;
    EXPECT_EQ(FindNameByMemIdInImportObj(memId, memName, memType, udsInfo, handleId), UBSE_OK);
    EXPECT_EQ(memName, "numaTest");
    EXPECT_EQ(memType, "numa");
    EXPECT_EQ(handleId, 5);
}

TEST_F(TestUbseMemControllerFaultHandle, ProcessFaultEvents_EmptyQueue)
{
    UbseRoleInfo curNodeInfo("master", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::vector<UbseRoleInfo> roleInfos;
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().with(outBound(roleInfos)).will(returnValue(UBSE_OK));

    ProcessFaultEvents();
}

TEST_F(TestUbseMemControllerFaultHandle, ProcessFaultEvents_TimeoutRemovesEvent)
{
    UbseRoleInfo curNodeInfo("master", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::vector<UbseRoleInfo> roleInfos;
    roleInfos.emplace_back("agent1", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().with(outBound(roleInfos)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));

    PushFaultEvent("faultNode1", static_cast<uint32_t>(ALARM_REBOOT_EVENT));

    for (auto& [key, event] : g_pendingFaultEvents) {
        event.retryCount = FAULT_DELIVER_MAX_RETRY_COUNT + 1;
    }

    ProcessFaultEvents();

    EXPECT_TRUE(g_pendingFaultEvents.empty());
}

TEST_F(TestUbseMemControllerFaultHandle, SendFaultToNode_SerializeFailed)
{
    EXPECT_NE(SendFaultToNode("targetNode", "faultNode1", 100), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, SendFaultToNode_Success)
{
    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(SendFaultToNode("targetNode", "faultNode1", 100), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, CreateTaskExecutor_GetModuleFailed)
{
    std::shared_ptr<UbseTaskExecutorModule> nullPtr;
    MOCKER(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(nullPtr));

    EXPECT_EQ(UbseMemFaultManager::CreateTaskExecutor("test"), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemControllerFaultHandle, CreateTaskExecutor_Success)
{
    auto taskExecModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskExecModule));

    auto ret = taskExecModule->Create("test", 1, 1000);
    if (ret == UBSE_OK) {
        EXPECT_EQ(UbseMemFaultManager::CreateTaskExecutor("test"), UBSE_OK);
        EXPECT_NE(UbseMemFaultManager::executorPtr, nullptr);
        taskExecModule->Remove("test");
    }
}

TEST_F(TestUbseMemControllerFaultHandle, RemoveTaskExecutor_GetModuleFailed)
{
    std::shared_ptr<UbseTaskExecutorModule> nullPtr;
    MOCKER(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(nullPtr));

    EXPECT_EQ(UbseMemFaultManager::RemoveTaskExecutor("test"), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemControllerFaultHandle, RemoveTaskExecutor_Success)
{
    auto taskExecModule = std::make_shared<UbseTaskExecutorModule>();
    MOCKER(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskExecModule));

    auto ret = taskExecModule->Create("test", 1, 1000);
    if (ret == UBSE_OK) {
        UbseMemFaultManager::executorPtr = taskExecModule->Get("test");
        EXPECT_EQ(UbseMemFaultManager::RemoveTaskExecutor("test"), UBSE_OK);
    }
}

TEST_F(TestUbseMemControllerFaultHandle, DeInitMemFaultManager_UnSubEventFailed)
{
    MOCKER_CPP(ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(UnRegisterAlarmFaultHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::event::UbseUnSubEvent).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseMemFaultManager::DeInitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, DeInitMemFaultManager_PortUpDownUnSubEventFailed)
{
    MOCKER_CPP(ubse::timer::UbseTimerHandlerUnregister).stubs();
    MOCKER(UnRegisterAlarmFaultHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::event::UbseUnSubEvent).stubs().will(returnValue(UBSE_OK)).then(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseMemFaultManager::DeInitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, InitMemFaultManager_RegRpcServiceFailed)
{
    MOCKER(&UbseMemFaultManager::CreateTaskExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseRegRpcService).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseMemFaultManager::InitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, ParsePortDownUpEventMsg_ValidDown)
{
    PortEventInfo info;
    EXPECT_EQ(ParsePortDownUpEventMsg("DOWN;1:2:3:0", info), UBSE_OK);
    EXPECT_EQ(info.status, "DOWN");
    EXPECT_EQ(info.slotId, "1");
    EXPECT_EQ(info.chipId, "2");
    EXPECT_EQ(info.portId, "3");
}

TEST_F(TestUbseMemControllerFaultHandle, ParsePortDownUpEventMsg_ValidUp)
{
    PortEventInfo info;
    EXPECT_EQ(ParsePortDownUpEventMsg("UP;10:20:30:0", info), UBSE_OK);
    EXPECT_EQ(info.status, "UP");
    EXPECT_EQ(info.slotId, "10");
    EXPECT_EQ(info.chipId, "20");
    EXPECT_EQ(info.portId, "30");
}

TEST_F(TestUbseMemControllerFaultHandle, ParsePortDownUpEventMsg_NoSemicolon)
{
    PortEventInfo info;
    EXPECT_EQ(ParsePortDownUpEventMsg("DOWN1:2:3:0", info), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemControllerFaultHandle, ParsePortDownUpEventMsg_InvalidStatus)
{
    PortEventInfo info;
    EXPECT_EQ(ParsePortDownUpEventMsg("INVALID;1:2:3:0", info), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemControllerFaultHandle, ParsePortDownUpEventMsg_MissingColonSlotChip)
{
    PortEventInfo info;
    EXPECT_EQ(ParsePortDownUpEventMsg("DOWN;1", info), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemControllerFaultHandle, ParsePortDownUpEventMsg_MissingColonChipPort)
{
    PortEventInfo info;
    EXPECT_EQ(ParsePortDownUpEventMsg("DOWN;1:2", info), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemControllerFaultHandle, ParsePortDownUpEventMsg_MissingColonPortSuffix)
{
    PortEventInfo info;
    EXPECT_EQ(ParsePortDownUpEventMsg("DOWN;1:2:3", info), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemControllerFaultHandle, ParsePortDownUpEventMsg_EmptyFields)
{
    PortEventInfo info;
    EXPECT_EQ(ParsePortDownUpEventMsg("DOWN;:2:3:0", info), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemControllerFaultHandle, PortDownUpEventHandle_EmptyMessage)
{
    std::string eventId;
    std::string eventMsg;
    EXPECT_EQ(UbseMemFaultManager::PortDownUpEventHandle(eventId, eventMsg), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemControllerFaultHandle, PortDownUpEventHandle_InvalidFormat)
{
    std::string eventId;
    std::string eventMsg = "INVALID";
    EXPECT_EQ(UbseMemFaultManager::PortDownUpEventHandle(eventId, eventMsg), UBSE_ERROR_INVAL);
}

TEST_F(TestUbseMemControllerFaultHandle, PortDownUpEventHandle_PortDownSuccess)
{
    std::string eventId;
    std::string eventMsg = "DOWN;1:2:5:0";

    using ExecuteFuncType = bool (UbseTaskExecutor::*)(const std::function<void()>&);
    MOCKER(static_cast<ExecuteFuncType>(&UbseTaskExecutor::Execute)).stubs().will(returnValue(true));
    auto mockExecutor = UbseTaskExecutor::Create("test", 1, 1000);
    UbseMemFaultManager::executorPtr = mockExecutor;

    EXPECT_EQ(UbseMemFaultManager::PortDownUpEventHandle(eventId, eventMsg), UBSE_OK);
    // TryErasePortDown 返回 true 说明端口确实已 Down
    PortEventInfo info{.status = "DOWN", .slotId = "1", .chipId = "2", .portId = "5"};
    EXPECT_TRUE(TryErasePortDown(info));
}

TEST_F(TestUbseMemControllerFaultHandle, PortDownUpEventHandle_PortDownAlreadyDown)
{
    std::string eventId;
    std::string eventMsg = "DOWN;1:2:5:0";

    PortEventInfo info{.status = "DOWN", .slotId = "1", .chipId = "2", .portId = "5"};
    TryAddPortDown(info);

    EXPECT_EQ(UbseMemFaultManager::PortDownUpEventHandle(eventId, eventMsg), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, PortDownUpEventHandle_PortUpSuccess)
{
    PortEventInfo info{.status = "DOWN", .slotId = "1", .chipId = "2", .portId = "5"};
    TryAddPortDown(info);

    using ExecuteFuncType = bool (UbseTaskExecutor::*)(const std::function<void()>&);
    MOCKER(static_cast<ExecuteFuncType>(&UbseTaskExecutor::Execute)).stubs().will(returnValue(true));
    auto mockExecutor = UbseTaskExecutor::Create("test", 1, 1000);
    UbseMemFaultManager::executorPtr = mockExecutor;

    std::string eventId;
    std::string eventMsg = "UP;1:2:5:0";
    EXPECT_EQ(UbseMemFaultManager::PortDownUpEventHandle(eventId, eventMsg), UBSE_OK);
    // TryErasePortDown 返回 false 说明端口已不在 Down 记录中
    EXPECT_FALSE(TryErasePortDown(info));
}

TEST_F(TestUbseMemControllerFaultHandle, PortDownUpEventHandle_PortUpNotInDownRecord)
{
    std::string eventId;
    std::string eventMsg = "UP;1:2:5:0";
    EXPECT_EQ(UbseMemFaultManager::PortDownUpEventHandle(eventId, eventMsg), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, OnePortDownHandle_NullExecutor)
{
    UbseMemFaultManager::executorPtr = nullptr;
    PortEventInfo info{.status = "DOWN", .slotId = "1", .chipId = "2", .portId = "5"};
    EXPECT_EQ(UbseMemFaultManager::OnePortDownHandle(info), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemControllerFaultHandle, OnePortDownHandle_Success)
{
    using ExecuteFuncType = bool (UbseTaskExecutor::*)(const std::function<void()>&);
    MOCKER(static_cast<ExecuteFuncType>(&UbseTaskExecutor::Execute)).stubs().will(returnValue(true));
    auto mockExecutor = UbseTaskExecutor::Create("test", 1, 1000);
    UbseMemFaultManager::executorPtr = mockExecutor;

    PortEventInfo info{.status = "DOWN", .slotId = "1", .chipId = "2", .portId = "5"};
    EXPECT_EQ(UbseMemFaultManager::OnePortDownHandle(info), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, OnePortUpHandle_NullExecutor)
{
    UbseMemFaultManager::executorPtr = nullptr;
    PortEventInfo info{.status = "UP", .slotId = "1", .chipId = "2", .portId = "5"};
    EXPECT_EQ(UbseMemFaultManager::OnePortUpHandle(info), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseMemControllerFaultHandle, OnePortUpHandle_Success)
{
    using ExecuteFuncType = bool (UbseTaskExecutor::*)(const std::function<void()>&);
    MOCKER(static_cast<ExecuteFuncType>(&UbseTaskExecutor::Execute)).stubs().will(returnValue(true));
    auto mockExecutor = UbseTaskExecutor::Create("test", 1, 1000);
    UbseMemFaultManager::executorPtr = mockExecutor;

    PortEventInfo info{.status = "UP", .slotId = "1", .chipId = "2", .portId = "5"};
    EXPECT_EQ(UbseMemFaultManager::OnePortUpHandle(info), UBSE_OK);
}
TEST_F(TestUbseMemControllerFaultHandle, DeleteShareImportDebtInfoByNodeId_DeletesTargetOnly)
{
    UbseMemShareBorrowImportObj obj1;
    obj1.req.name = "shm1";
    obj1.importNodeId = "3";
    obj1.status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("3", "shm1", obj1);

    UbseMemShareBorrowImportObj obj2;
    obj2.req.name = "shm2";
    obj2.importNodeId = "3";
    obj2.status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("3", "shm2", obj2);

    UbseMemShareBorrowImportObj otherObj;
    otherObj.req.name = "other_shm";
    otherObj.importNodeId = "2";
    otherObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("2", "other_shm", otherObj);

    MOCKER_CPP(&UbseCheckWithoutGlobalMasterNodeId).stubs().will(returnValue(true));

    DeleteShareImportDebtInfoByNodeId("3");

    CascadeMasterStore store;
    UbseMemShareBorrowImportObj out;
    EXPECT_EQ(UBSE_ERR_NOT_EXIST, store.LoadImport("3", "shm1", out));
    EXPECT_EQ(UBSE_ERR_NOT_EXIST, store.LoadImport("3", "shm2", out));
    EXPECT_EQ(UBSE_OK, store.LoadImport("2", "other_shm", out));
}

TEST_F(TestUbseMemControllerFaultHandle, DeleteShareImportDebtInfoByNodeId_NothingToDelete_NoCrash)
{
    MOCKER_CPP(&UbseCheckWithoutGlobalMasterNodeId).stubs().will(returnValue(true));

    DeleteShareImportDebtInfoByNodeId("99");
}

TEST_F(TestUbseMemControllerFaultHandle, DeleteShareImportDebtInfoByNodeId_MixedNodes_DeletesTargetOnly)
{
    UbseMemShareBorrowImportObj targetObj;
    targetObj.req.name = "shm_mixed";
    targetObj.importNodeId = "5";
    targetObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("5", "shm_mixed", targetObj);

    UbseMemShareBorrowImportObj otherObj;
    otherObj.req.name = "other_shm";
    otherObj.importNodeId = "6";
    otherObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().PutResource("6", "other_shm", otherObj);

    MOCKER_CPP(&UbseCheckWithoutGlobalMasterNodeId).stubs().will(returnValue(true));

    DeleteShareImportDebtInfoByNodeId("5");

    CascadeMasterStore store;
    UbseMemShareBorrowImportObj out;
    EXPECT_EQ(UBSE_ERR_NOT_EXIST, store.LoadImport("5", "shm_mixed", out));
    EXPECT_EQ(UBSE_OK, store.LoadImport("6", "other_shm", out));
}

// === 新增 UT：覆盖三种场景(Clos单层/Clos双层/非Clos) × ②③ 故障 ===

// 辅助函数：Clos双层全局主拓扑（currentNode为全局主，包含两个组主）
static UbseResult MockTopoInfoClosDoubleLayerGlobalMaster(UbseElectionModule*, HaTopologyInfo& topoInfo)
{
    topoInfo.currentNode.nodeId = "globalMaster";
    topoInfo.currentNode.globalRole = GlobalRoleType::GLOBAL_MASTER;
    topoInfo.currentGroup.groupId = "1";
    topoInfo.currentGroup.groupMasterId = "groupMaster1";
    GroupTopology group2;
    group2.groupId = "2";
    group2.groupMasterId = "groupMaster2";
    topoInfo.groups.push_back(group2);
    return UBSE_OK;
}

// --- Clos单层 ---

// Clos单层 ②BMC节点宕机：本地主处理并广播全部节点
TEST_F(TestUbseMemControllerFaultHandle, BmcFaultHandler_ClosSingleLayer_MasterBroadcastsToAllNodes)
{
    MockHierarchicalMode(false);
    MOCKER(&UbseSmbios::IsClosType).stubs().will(returnValue(true));

    UbseRoleInfo curNodeInfo("master", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::vector<UbseRoleInfo> roleInfos;
    roleInfos.emplace_back("agent1", ELECTION_ROLE_AGENT);
    roleInfos.emplace_back("agent2", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().with(outBound(roleInfos)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(UbseMemFaultManager::BmcFaultHandler(ALARM_REBOOT_EVENT, "faultNode1"), UBSE_OK);
}

// Clos单层 ③Panic/Reboot：本地主只转发不清理本节点债务（由 agent 通过 FaultAgentsNotifyHandler 清理）
TEST_F(TestUbseMemControllerFaultHandle, PanicRebootFaultEventHandler_ClosSingleLayer_ForwardsOnlyNoLocalCleanup)
{
    MockHierarchicalMode(false);
    MOCKER(&UbseSmbios::IsClosType).stubs().will(returnValue(true));

    // ShouldHandlePanicLocal=false：主节点不调用本地清理
    MOCKER(&UbseMemFaultManager::MemReportWhenExportNodeOnFault).expects(never());

    UbseRoleInfo curNodeInfo("master", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::vector<UbseRoleInfo> roleInfos;
    roleInfos.emplace_back("agent1", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().with(outBound(roleInfos)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));

    std::string eventId = "UbsePanicAndRebootFaultLocalEvent";
    std::string eventMessage = "faultNode1_1007";
    EXPECT_EQ(UbseMemFaultManager::PanicRebootFaultEventHandler(eventId, eventMessage), UBSE_OK);
}

// --- Clos双层 ---

// Clos双层 ②BMC节点宕机：非全局主节点跳过处理
TEST_F(TestUbseMemControllerFaultHandle, BmcFaultHandler_ClosDoubleLayer_NotGlobalMaster_Skips)
{
    MockHierarchicalMode(true);

    std::shared_ptr<UbseElectionModule> nullModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule));

    UbseRoleInfo curNodeInfo("node1", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    EXPECT_EQ(UbseMemFaultManager::BmcFaultHandler(ALARM_REBOOT_EVENT, "faultNode1"), UBSE_OK);
    EXPECT_TRUE(g_pendingFaultEvents.empty());
}

// Clos双层 ②BMC节点宕机：全局主处理并广播各组主
TEST_F(TestUbseMemControllerFaultHandle, BmcFaultHandler_ClosDoubleLayer_GlobalMaster_BroadcastsToGroupMasters)
{
    MockHierarchicalMode(true);

    auto electionModule = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(electionModule));
    MOCKER(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .stubs()
        .will(invoke(MockTopoInfoClosDoubleLayerGlobalMaster));

    UbseRoleInfo curNodeInfo("globalMaster", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(UbseMemFaultManager::BmcFaultHandler(ALARM_REBOOT_EVENT, "faultNode1"), UBSE_OK);
}

// Clos双层 ③Panic/Reboot：非全局主节点跳过处理
TEST_F(TestUbseMemControllerFaultHandle, PanicRebootFaultEventHandler_ClosDoubleLayer_NotGlobalMaster_Skips)
{
    MockHierarchicalMode(true);

    std::shared_ptr<UbseElectionModule> nullModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule));

    std::string eventId = "UbsePanicAndRebootFaultLocalEvent";
    std::string eventMessage = "faultNode1_1007";
    EXPECT_EQ(UbseMemFaultManager::PanicRebootFaultEventHandler(eventId, eventMessage), UBSE_OK);
    EXPECT_TRUE(g_pendingFaultEvents.empty());
}

// Clos双层 ③Panic/Reboot：全局主只转发给各组主，不清理本节点债务（由组长/agent 通过 FaultAgentsNotifyHandler 清理）
TEST_F(TestUbseMemControllerFaultHandle, PanicRebootFaultEventHandler_ClosDoubleLayer_GlobalMaster_ForwardsOnlyNoLocalCleanup)
{
    MockHierarchicalMode(true);

    auto electionModule = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(electionModule));
    MOCKER(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .stubs()
        .will(invoke(MockTopoInfoClosDoubleLayerGlobalMaster));

    // ShouldHandlePanicLocal=false：全局主不调用本地清理
    MOCKER(&UbseMemFaultManager::MemReportWhenExportNodeOnFault).expects(never());

    UbseRoleInfo curNodeInfo("globalMaster", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));

    std::string eventId = "UbsePanicAndRebootFaultLocalEvent";
    std::string eventMessage = "faultNode1_1007";
    EXPECT_EQ(UbseMemFaultManager::PanicRebootFaultEventHandler(eventId, eventMessage), UBSE_OK);
}

// --- 非Clos ---

// 非Clos ②BMC节点宕机：本地主处理并广播全部节点
TEST_F(TestUbseMemControllerFaultHandle, BmcFaultHandler_NonClos_MasterBroadcastsToAllNodes)
{
    MockHierarchicalMode(false);
    MOCKER(&UbseSmbios::IsClosType).stubs().will(returnValue(false));

    UbseRoleInfo curNodeInfo("master", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::vector<UbseRoleInfo> roleInfos;
    roleInfos.emplace_back("agent1", ELECTION_ROLE_AGENT);
    roleInfos.emplace_back("agent2", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().with(outBound(roleInfos)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(UbseMemFaultManager::BmcFaultHandler(ALARM_REBOOT_EVENT, "faultNode1"), UBSE_OK);
}

// 非Clos ③Panic/Reboot：本地主先自扫本地，不广播
TEST_F(TestUbseMemControllerFaultHandle, PanicRebootFaultEventHandler_NonClos_ProcessesNoBroadcast)
{
    MockHierarchicalMode(false);
    MOCKER(&UbseSmbios::IsClosType).stubs().will(returnValue(false));

    MOCKER(&UbseMemFaultManager::MemReportWhenExportNodeOnFault).stubs().will(returnValue(UBSE_OK));

    std::string eventId = "UbsePanicAndRebootFaultLocalEvent";
    std::string eventMessage = "faultNode1_1007";
    EXPECT_EQ(UbseMemFaultManager::PanicRebootFaultEventHandler(eventId, eventMessage), UBSE_OK);
    // 验证非Clos场景下③Panic/Reboot不触发广播
    EXPECT_TRUE(g_pendingFaultEvents.empty());
}

// 非Clos ③Panic/Reboot：agent 节点也需清理本节点导入债务（bug 修复回归）
// 重构前：agent 节点因 ShouldHandle=false 直接 return，漏清理本节点债务
// 重构后：ShouldHandlePanicLocal 恒真，agent 节点也会调用 MemReportWhenExportNodeOnFault
TEST_F(TestUbseMemControllerFaultHandle, PanicRebootFaultEventHandler_NonClos_AgentAlsoProcessesLocalCleanup)
{
    MockHierarchicalMode(false);
    MOCKER(&UbseSmbios::IsClosType).stubs().will(returnValue(false));

    UbseRoleInfo curNodeInfo("agent1", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    // 期望：agent 节点也调用本节点清理
    MOCKER(&UbseMemFaultManager::MemReportWhenExportNodeOnFault)
        .expects(once())
        .will(returnValue(UBSE_OK));

    std::string eventId = "UbsePanicAndRebootFaultLocalEvent";
    std::string eventMessage = "faultNode1_1007";
    EXPECT_EQ(UbseMemFaultManager::PanicRebootFaultEventHandler(eventId, eventMessage), UBSE_OK);
    // 不触发广播
    EXPECT_TRUE(g_pendingFaultEvents.empty());
}

// 非Clos ③Panic/Reboot：本节点清理失败时返回错误，不进入转发
TEST_F(TestUbseMemControllerFaultHandle, PanicRebootFaultEventHandler_NonClos_LocalCleanupFailed)
{
    MockHierarchicalMode(false);
    MOCKER(&UbseSmbios::IsClosType).stubs().will(returnValue(false));

    MOCKER(&UbseMemFaultManager::MemReportWhenExportNodeOnFault).stubs().will(returnValue(UBSE_ERROR));

    std::string eventId = "UbsePanicAndRebootFaultLocalEvent";
    std::string eventMessage = "faultNode1_1007";
    EXPECT_EQ(UbseMemFaultManager::PanicRebootFaultEventHandler(eventId, eventMessage), UBSE_ERROR);
    EXPECT_TRUE(g_pendingFaultEvents.empty());
}

// === TDD: Strategy 单元测试 ===

// GetStrategy 工厂路由验证
TEST_F(TestUbseMemControllerFaultHandle, GetStrategy_ClosDoubleLayer)
{
    MockHierarchicalMode(true);
    EXPECT_EQ(GetStrategy().Name(), "CLOS_DOUBLE_LAYER");
}

TEST_F(TestUbseMemControllerFaultHandle, GetStrategy_ClosSingleLayer)
{
    MockHierarchicalMode(false);
    MOCKER(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    EXPECT_EQ(GetStrategy().Name(), "CLOS_SINGLE_LAYER");
}

TEST_F(TestUbseMemControllerFaultHandle, GetStrategy_NonClos)
{
    MockHierarchicalMode(false);
    MOCKER(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    EXPECT_EQ(GetStrategy().Name(), "NON_CLOS");
}

// ClosDoubleLayerStrategy: ShouldHandleBmc/ShouldProcessFaultQueue/ShouldForwardPanic 依赖全局主，ShouldHandlePanicLocal 恒假
TEST_F(TestUbseMemControllerFaultHandle, ClosDoubleLayerStrategy_ShouldHandle_GlobalMaster)
{
    MockHierarchicalMode(true);
    auto electionModule = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(electionModule));
    MOCKER(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .stubs()
        .will(invoke(MockTopoInfoClosDoubleLayerGlobalMaster));
    auto& s = GetStrategy();
    EXPECT_TRUE(s.ShouldHandleBmc());
    EXPECT_TRUE(s.ShouldProcessFaultQueue());
    EXPECT_FALSE(s.ShouldHandlePanicLocal());
    EXPECT_TRUE(s.ShouldForwardPanic());
}

TEST_F(TestUbseMemControllerFaultHandle, ClosDoubleLayerStrategy_ShouldHandle_NotGlobalMaster)
{
    MockHierarchicalMode(true);
    std::shared_ptr<UbseElectionModule> nullModule;
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(nullModule));
    auto& s = GetStrategy();
    EXPECT_FALSE(s.ShouldHandleBmc());
    EXPECT_FALSE(s.ShouldProcessFaultQueue());
    EXPECT_FALSE(s.ShouldHandlePanicLocal());
    EXPECT_FALSE(s.ShouldForwardPanic());
}

// ClosDoubleLayerStrategy: 策略属性
TEST_F(TestUbseMemControllerFaultHandle, ClosDoubleLayerStrategy_Policy)
{
    MockHierarchicalMode(true);
    auto& s = GetStrategy();
    EXPECT_TRUE(s.ShouldForwardToGroupNodes());
}

// ClosSingleLayerStrategy: ShouldHandleBmc/ShouldProcessFaultQueue/ShouldForwardPanic 依赖本地主，ShouldHandlePanicLocal 恒假
TEST_F(TestUbseMemControllerFaultHandle, ClosSingleLayerStrategy_ShouldHandle_LocalMaster)
{
    MockHierarchicalMode(false);
    MOCKER(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    UbseRoleInfo curNodeInfo("master", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));
    auto& s = GetStrategy();
    EXPECT_TRUE(s.ShouldHandleBmc());
    EXPECT_TRUE(s.ShouldProcessFaultQueue());
    EXPECT_FALSE(s.ShouldHandlePanicLocal());
    EXPECT_TRUE(s.ShouldForwardPanic());
}

TEST_F(TestUbseMemControllerFaultHandle, ClosSingleLayerStrategy_Policy)
{
    MockHierarchicalMode(false);
    MOCKER(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    auto& s = GetStrategy();
    EXPECT_FALSE(s.ShouldForwardToGroupNodes());
}

// NonClosStrategy: ShouldHandleBmc/ShouldProcessFaultQueue 依赖本地主，ShouldHandlePanicLocal 恒真，ShouldForwardPanic 恒假
TEST_F(TestUbseMemControllerFaultHandle, NonClosStrategy_ShouldHandle_LocalMaster)
{
    MockHierarchicalMode(false);
    MOCKER(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    UbseRoleInfo curNodeInfo("master", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));
    auto& s = GetStrategy();
    EXPECT_TRUE(s.ShouldHandleBmc());
    EXPECT_TRUE(s.ShouldProcessFaultQueue());
    EXPECT_TRUE(s.ShouldHandlePanicLocal());
    EXPECT_FALSE(s.ShouldForwardPanic());
}

// NonClosStrategy: 非 master 节点 ShouldHandlePanicLocal 仍为 true（所有节点都清理本节点债务）
TEST_F(TestUbseMemControllerFaultHandle, NonClosStrategy_ShouldHandlePanicLocal_AlwaysTrueForAgent)
{
    MockHierarchicalMode(false);
    MOCKER(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    UbseRoleInfo curNodeInfo("agent1", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));
    auto& s = GetStrategy();
    EXPECT_FALSE(s.ShouldHandleBmc());
    EXPECT_FALSE(s.ShouldProcessFaultQueue());
    EXPECT_TRUE(s.ShouldHandlePanicLocal());
    EXPECT_FALSE(s.ShouldForwardPanic());
}

TEST_F(TestUbseMemControllerFaultHandle, NonClosStrategy_Policy)
{
    MockHierarchicalMode(false);
    MOCKER(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    auto& s = GetStrategy();
    EXPECT_FALSE(s.ShouldForwardToGroupNodes());
}

// GetTargetNodeIds: ClosDoubleLayer 走 GetAllManagingGroupMasterIds
TEST_F(TestUbseMemControllerFaultHandle, ClosDoubleLayerStrategy_GetTargetNodeIds)
{
    MockHierarchicalMode(true);
    auto electionModule = std::make_shared<UbseElectionModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseElectionModule>).stubs().will(returnValue(electionModule));
    MOCKER(&UbseElectionModule::GetCurNodeGlobalTopoInfo)
        .stubs()
        .will(invoke(MockTopoInfoClosDoubleLayerGlobalMaster));
    std::set<std::string> ids;
    EXPECT_TRUE(GetStrategy().GetTargetNodeIds(ids));
    EXPECT_EQ(ids.size(), 2u); // groupMaster1 + groupMaster2
}

// GetTargetNodeIds: NonClos 走 GetAllNodeIdsFromRoleInfos
TEST_F(TestUbseMemControllerFaultHandle, NonClosStrategy_GetTargetNodeIds)
{
    MockHierarchicalMode(false);
    MOCKER(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    std::vector<UbseRoleInfo> roleInfos;
    roleInfos.emplace_back("node1", ELECTION_ROLE_MASTER);
    roleInfos.emplace_back("node2", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().with(outBound(roleInfos)).will(returnValue(UBSE_OK));
    std::set<std::string> ids;
    EXPECT_TRUE(GetStrategy().GetTargetNodeIds(ids));
    EXPECT_EQ(ids.size(), 2u);
}

} // namespace ubse::mem_controller::ut
