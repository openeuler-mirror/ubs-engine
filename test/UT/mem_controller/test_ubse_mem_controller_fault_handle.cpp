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
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_event.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_fault_handle.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_ras.h"
#include "ubse_serial_util.h"
#include "ubse_mem_controller_fault_handle.cpp"

namespace ubse::mem_controller::ut {
using namespace ubse::ras;
using namespace ubse::mem::controller;
using namespace ubse::config;
using namespace ubse::context;
using namespace ubse::election;
using namespace ubse::serial;
using namespace ubse::event;
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;

void TestUbseMemControllerFaultHandle::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerFaultHandle::TearDown()
{
    UbseMemFaultManager::executorPtr = nullptr;
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
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
    MOCKER(RegisterAlarmFaultHandler, uint32_t(ALARM_FAULT_TYPE, std::string, AlarmFaultHandler, AlarmHandlerPriority))
        .stubs()
        .will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseMemFaultManager::InitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, InitMemFaultManager_SubEventFailed)
{
    MOCKER(&UbseMemFaultManager::CreateTaskExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(RegisterAlarmFaultHandler, uint32_t(ALARM_FAULT_TYPE, std::string, AlarmFaultHandler, AlarmHandlerPriority))
        .stubs()
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseSubEvent).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseMemFaultManager::InitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, InitMemFaultManager_Success)
{
    MOCKER(&UbseMemFaultManager::CreateTaskExecutor).stubs().will(returnValue(UBSE_OK));
    MOCKER(RegisterAlarmFaultHandler, uint32_t(ALARM_FAULT_TYPE, std::string, AlarmFaultHandler, AlarmHandlerPriority))
        .stubs()
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseSubEvent).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, UbseMemFaultManager::InitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, DeInitMemFaultManager_UnRegisterFailed)
{
    MOCKER(UnRegisterAlarmFaultHandler).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_ERROR, UbseMemFaultManager::DeInitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, DeInitMemFaultManager_RemoveTaskExecutorFailed)
{
    MOCKER(UnRegisterAlarmFaultHandler).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseMemFaultManager::RemoveTaskExecutor).stubs().will(returnValue(UBSE_ERROR_NULLPTR));
    EXPECT_EQ(UBSE_ERROR_NULLPTR, UbseMemFaultManager::DeInitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, DeInitMemFaultManager_Success)
{
    MOCKER(UnRegisterAlarmFaultHandler).stubs().will(returnValue(UBSE_OK));
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
    UbMemFaultType type = UB_MEM_ATOMIC_DATA_ERR;
    std::string memName;
    std::string memType;
    UbseUdsInfo udsInfo;
    EXPECT_NE(FindNameByMemIdInImportObj(memId, type, memName, memType, udsInfo), UBSE_OK);
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
    UbMemFaultType type = UB_MEM_ATOMIC_DATA_ERR;
    std::string memName;
    std::string memType;
    UbseUdsInfo udsInfo;
    EXPECT_EQ(FindNameByMemIdInImportObj(memId, type, memName, memType, udsInfo), UBSE_OK);
    EXPECT_EQ(memName, "shareTest");
    EXPECT_EQ(memType, "share");
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
    UbMemFaultType type = UB_MEM_ATOMIC_DATA_ERR;
    std::string memName;
    std::string memType;
    UbseUdsInfo udsInfo;
    EXPECT_EQ(FindNameByMemIdInImportObj(memId, type, memName, memType, udsInfo), UBSE_OK);
    EXPECT_EQ(memName, "fdTest");
    EXPECT_EQ(memType, "fd");
}

TEST_F(TestUbseMemControllerFaultHandle, FindNameByMemIdInImportObj_NotFound)
{
    UbseRoleInfo roleInfo("1", "master");
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(roleInfo)).will(returnValue(UBSE_OK));

    uint64_t memId = 999;
    UbMemFaultType type = UB_MEM_ATOMIC_DATA_ERR;
    std::string memName;
    std::string memType;
    UbseUdsInfo udsInfo;
    EXPECT_NE(FindNameByMemIdInImportObj(memId, type, memName, memType, udsInfo), UBSE_OK);
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
} // namespace ubse::mem_controller::ut
