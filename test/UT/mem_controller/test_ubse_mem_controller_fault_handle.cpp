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
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_fault_handle.cpp"
#include "ubse_mem_controller_fault_handle.h"
#include "ubse_ras.h"
#include "ubse_serial_util.h"

namespace ubse::mem_controller::ut {
using namespace ubse::ras;
using namespace ubse::mem::controller;
using namespace ubse::config;
using namespace ubse::context;
using namespace ubse::election;
using namespace ubse::mem::controller;
using namespace ubse::serial;

void TestUbseMemControllerFaultHandle::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerFaultHandle::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerFaultHandle, InitMemFaultManager)
{
    MOCKER(RegisterAlarmFaultHandler, uint32_t(ALARM_FAULT_TYPE alarmFaultEvent, std::string name,
                                               AlarmFaultHandler handler, AlarmHandlerPriority priority))
        .stubs()
        .will(returnValue(UBSE_ERROR))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR, UbseMemFaultManager::InitMemFaultManager());
    MOCKER(&UbseMemFaultManager::CreateTaskExecutor).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR, UbseMemFaultManager::InitMemFaultManager());
    MOCKER(UbseRegRpcService).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NE(UBSE_ERROR, UbseMemFaultManager::InitMemFaultManager());
}

TEST_F(TestUbseMemControllerFaultHandle, MemFaultReportTask)
{
    g_reportTaskCtrlMap["test"].ready = true;

    UbseMemFaultMsg msg;
    msg.faultNode = "1";
    msg.nodeNum = 2;
    msg.notifiedNode = "2";
    UbseMemFaultInfo info(1, "test", "fault");
    msg.info = info;

    MOCKER(&UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));

    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(std::make_shared<UbseConfModule>()));

    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(std::make_shared<UbseConfModule>()));
    MOCKER(UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseRpcAsyncSend).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(UbseMemFaultManager::MemFaultReportTask(msg));
}

TEST_F(TestUbseMemControllerFaultHandle, GetMemNameById)
{
    uint64_t memId = 1;
    std::string memName = "test1";
    std::string nodeId = "node1";
    MOCKER(&GetCurrentNodeId).stubs().with(outBound(nodeId)).will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_NE(UbseMemFaultManager::GetMemNameById(memId, memName), UBSE_OK);

    EXPECT_EQ(UbseMemFaultManager::GetMemNameById(memId, memName), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, StartMemFaultReportTask)
{
    UbseMemFaultMsg msg;
    MOCKER_CPP(&UbseMemFaultManager::MemFaultReportTask);
    EXPECT_NO_THROW(UbseMemFaultManager::StartMemFaultReportTask(msg));
}

TEST_F(TestUbseMemControllerFaultHandle, DeInitMemFaultManager)
{
    MOCKER(UnRegisterAlarmFaultHandler).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFaultManager::DeInitMemFaultManager(), UBSE_ERROR);
    EXPECT_EQ(UbseMemFaultManager::DeInitMemFaultManager(), UBSE_ERROR_NULLPTR);
    MOCKER(UbseMemFaultManager::RemoveTaskExecutor).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseMemFaultManager::DeInitMemFaultManager(), UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, MemFaultHandler)
{
    ALARM_FAULT_TYPE alarmFaultEvent;
    std::string faultInfo = "test";
    auto ret = UbseMemFaultManager::MemFaultHandler(alarmFaultEvent, faultInfo);
    EXPECT_NE(ret, UBSE_OK);
    faultInfo = R"({"memid" : 1, "raw_ubus_mem_err_type": 0})";
    MOCKER(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_OK));
    ret = UbseMemFaultManager::MemFaultHandler(alarmFaultEvent, faultInfo);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerFaultHandle, MemFaultReportHandler)
{
    UbseByteBuffer req;
    UbseByteBuffer resp;
    EXPECT_NO_THROW(UbseMemFaultManager::MemFaultReportHandler(req, resp));
    UbseRoleInfo curNodeInfo{};
    curNodeInfo.nodeRole = ELECTION_ROLE_MASTER;
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));
    UbseSerialization serialization;
    UbseMemFaultMsg msg;
    UbseMemFaultInfo info(0, "test", R"({"memid" : 1, "raw_ubus_mem_err_type": 0})");
    msg.info = info;
    msg.nodeNum = 2;
    msg.faultNode = 1;
    msg.notifiedNode = 1;
    serialization << msg.info.memId_ << msg.nodeNum << msg.info.memName_ << msg.faultNode << msg.notifiedNode
                  << msg.info.faultInfo_;
    req.data = serialization.GetBuffer();
    req.len = serialization.GetLength();
    EXPECT_NO_THROW(UbseMemFaultManager::MemFaultReportHandler(req, resp));
}

TEST_F(TestUbseMemControllerFaultHandle, MemFaultNotifyReplyHandler)
{
    UbseByteBuffer req;
    UbseByteBuffer resp;
    EXPECT_NO_THROW(UbseMemFaultManager::MemFaultNotifyReplyHandler(req, resp));
    UbseRoleInfo curNodeInfo{};
    curNodeInfo.nodeRole = ELECTION_ROLE_MASTER;
    MOCKER(UbseGetCurrentNodeInfo).stubs().with(outBound(curNodeInfo)).will(returnValue(UBSE_OK));
    UbseSerialization serialization;
    UbseMemFaultMsg msg;
    UbseMemFaultInfo info(0, "test", R"({"memid" : 1, "raw_ubus_mem_err_type": 0})");
    msg.info = info;
    msg.nodeNum = 2;
    msg.faultNode = 1;
    msg.notifiedNode = 1;
    serialization << msg.info.memId_ << msg.nodeNum << msg.info.memName_ << msg.faultNode << msg.notifiedNode
                  << msg.info.faultInfo_;
    req.data = serialization.GetBuffer();
    req.len = serialization.GetLength();
    EXPECT_NO_THROW(UbseMemFaultManager::MemFaultNotifyReplyHandler(req, resp));
}

TEST_F(TestUbseMemControllerFaultHandle, MemFaultNotifyHandler)
{
    GTEST_SKIP();
    UbseByteBuffer req;
    UbseByteBuffer resp;
    UbseSerialization serialization;
    UbseMemFaultMsg msg;
    UbseMemFaultInfo info(0, "test", R"({"memid" : 1, "raw_ubus_mem_err_type": 0})");
    msg.info = info;
    msg.nodeNum = 2;
    msg.faultNode = 1;
    msg.notifiedNode = 1;
    serialization << msg.info.memId_ << msg.nodeNum << msg.info.memName_ << msg.faultNode << msg.notifiedNode
                  << msg.info.faultInfo_;
    req.data = serialization.GetBuffer();
    req.len = serialization.GetLength();
    EXPECT_NO_THROW(UbseMemFaultManager::MemFaultNotifyHandler(req, resp));

    std::shared_ptr<UbseApiServerModule> apiServerPtr = std::make_shared<UbseApiServerModule>();
    MOCKER(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerPtr));
    EXPECT_NO_THROW(UbseMemFaultManager::MemFaultNotifyHandler(req, resp));
}
} // namespace ubse::mem_controller::ut
