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

void TestUbseMemControllerFaultHandle::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerFaultHandle::TearDown()
{
    UbseMemFaultManager::executorPtr = nullptr;
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    g_pendingBmcFaultEvents.clear();
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

TEST_F(TestUbseMemControllerFaultHandle, PanicRebootFaultEventHandler_ValidMessage)
{
    std::string eventId = "UbsePanicAndRebootFaultLocalEvent";
    std::string eventMessage = "2_1013";
    MOCKER(&UbseMemFaultManager::MemReportWhenExportNodeOnFault).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFaultManager::PanicRebootFaultEventHandler(eventId, eventMessage), UBSE_OK);
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

TEST_F(TestUbseMemControllerFaultHandle, BmcFaultHandler_SuccessWithMultipleNodes)
{
    UbseRoleInfo curNodeInfo("master", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::vector<UbseRoleInfo> roleInfos;
    roleInfos.emplace_back("agent1", ELECTION_ROLE_AGENT);
    roleInfos.emplace_back("agent2", ELECTION_ROLE_AGENT);
    roleInfos.emplace_back("agent3", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().with(outBound(roleInfos)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(UbseMemFaultManager::BmcFaultHandler(ALARM_REBOOT_EVENT, "faultNode1"), UBSE_OK);
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

TEST_F(TestUbseMemControllerFaultHandle, BmcFaultTimerHandler_NullExecutor)
{
    UbseMemFaultManager::executorPtr = nullptr;
    EXPECT_EQ(UbseMemFaultManager::BmcFaultTimerHandler(), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, BmcFaultTimerHandler_ProcessesPendingEvents)
{
    UbseMemFaultManager::executorPtr = nullptr;

    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));

    std::vector<UbseRoleInfo> roleInfos;
    roleInfos.emplace_back("agent1", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().with(outBound(roleInfos)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));

    PushBmcFaultEvent("faultNode1", static_cast<uint32_t>(ALARM_REBOOT_EVENT));

    ProcessBmcFaultEvents();

    EXPECT_TRUE(g_pendingBmcFaultEvents.empty());
}

TEST_F(TestUbseMemControllerFaultHandle, BmcFaultTimerHandler_NullExecutorLogsError)
{
    UbseMemFaultManager::executorPtr = nullptr;

    EXPECT_EQ(UbseMemFaultManager::BmcFaultTimerHandler(), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, BmcFaultAgentsHandler_DeserializeFailed)
{
    UbseByteBuffer req{.data = nullptr, .len = 0, .freeFunc = nullptr};
    UbseByteBuffer resp{.data = nullptr, .len = 0, .freeFunc = nullptr};

    UbseMemFaultManager::BmcFaultAgentsHandler(req, resp);
}

TEST_F(TestUbseMemControllerFaultHandle, BmcFaultAgentsHandler_Success)
{
    std::string faultNodeId = "faultNode1";
    uint32_t faultTypeValue = static_cast<uint32_t>(ALARM_REBOOT_EVENT);

    UbseSerialization output;
    output << faultNodeId << faultTypeValue;
    ASSERT_TRUE(output.Check());

    UbseByteBuffer req{.data = output.GetBuffer(), .len = output.GetLength(), .freeFunc = nullptr};
    UbseByteBuffer resp{.data = nullptr, .len = 0, .freeFunc = nullptr};

    MOCKER(&UbseMemFaultManager::MemReportWhenExportNodeOnFault).stubs().will(returnValue(UBSE_OK));

    UbseMemFaultManager::BmcFaultAgentsHandler(req, resp);
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

    UbseByteBuffer req{
        .data = msg.SerializedData(), .len = msg.SerializedDataSize(), .freeFunc = [](uint8_t*) -> void {}};
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

TEST_F(TestUbseMemControllerFaultHandle, ProcessBmcFaultEvents_EmptyQueue)
{
    UbseRoleInfo curNodeInfo("master", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::vector<UbseRoleInfo> roleInfos;
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().with(outBound(roleInfos)).will(returnValue(UBSE_OK));

    ProcessBmcFaultEvents();
}

TEST_F(TestUbseMemControllerFaultHandle, ProcessBmcFaultEvents_TimeoutRemovesEvent)
{
    UbseRoleInfo curNodeInfo("master", ELECTION_ROLE_MASTER);
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));

    std::vector<UbseRoleInfo> roleInfos;
    roleInfos.emplace_back("agent1", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().with(outBound(roleInfos)).will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));

    PushBmcFaultEvent("faultNode1", static_cast<uint32_t>(ALARM_REBOOT_EVENT));

    for (auto& [key, event] : g_pendingBmcFaultEvents) {
        event.retryCount = BMC_FAULT_MAX_RETRY_COUNT + 1;
    }

    ProcessBmcFaultEvents();

    EXPECT_TRUE(g_pendingBmcFaultEvents.empty());
}

TEST_F(TestUbseMemControllerFaultHandle, SendBmcFaultToNode_SerializeFailed)
{
    EXPECT_NE(SendBmcFaultToNode("targetNode", "faultNode1", 100), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, SendBmcFaultToNode_Success)
{
    MOCKER_CPP(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));

    EXPECT_EQ(SendBmcFaultToNode("targetNode", "faultNode1", 100), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, GetAllNodeIds_Failed)
{
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().will(returnValue(UBSE_ERROR));

    std::set<std::string> allNodeIds;
    EXPECT_FALSE(GetAllNodeIds(allNodeIds));
    EXPECT_TRUE(allNodeIds.empty());
}

TEST_F(TestUbseMemControllerFaultHandle, GetAllNodeIds_Success)
{
    std::vector<UbseRoleInfo> roleInfos;
    roleInfos.emplace_back("node1", ELECTION_ROLE_MASTER);
    roleInfos.emplace_back("node2", ELECTION_ROLE_AGENT);
    MOCKER_CPP(UbseGetAllNodeInfos).stubs().with(outBound(roleInfos)).will(returnValue(UBSE_OK));

    std::set<std::string> allNodeIds;
    EXPECT_TRUE(GetAllNodeIds(allNodeIds));
    EXPECT_EQ(allNodeIds.size(), 2);
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

TEST_F(TestUbseMemControllerFaultHandle, InitMemFaultManager_RegRpcServiceFailed)
{
    MOCKER(&UbseMemFaultManager::CreateTaskExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseRegRpcService).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseMemFaultManager::InitMemFaultManager());
}
} // namespace ubse::mem_controller::ut
