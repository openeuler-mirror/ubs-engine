/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
* ubs-engine is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
* See the Mulan PSL v2 for more details.
*/
#include "test_libvirt_handler.h"

#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>
#include <ubse_api_server.h>
#include <ubse_com.h>
#include <ubse_ras.h>
#include "vm_configuration.h"
#include "fast_recovery.h"
#include "libvirt_handler.h"
#include "ham_migrate.h"
#include "migrate_strategy.h"

using namespace vm;
using namespace ubse::ras;
using namespace ubse::com;
namespace ubse::vm::ut {

TestLibvirtHandler::TestLibvirtHandler() = default;

void TestLibvirtHandler::SetUp() {
    Test::SetUp();
    VmConfiguration::GetInstance().LoadConfig();
    using FuncType = unsigned int(*)(int, std::string,
                                      std::function<unsigned int(int, std::string)>, ubse::ras::AlarmHandlerPriority);
    MOCKER((FuncType)RegisterAlarmFaultHandler).stubs().will(returnValue(VM_OK));
    MOCKER(com::UbseRegRpcService).stubs().will(returnValue(VM_OK));
    MOCKER(VirtMigrateStrategy::Register).stubs().will(returnValue(VM_OK));
}

void TestLibvirtHandler::TearDown() {
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestLibvirtHandler, HamMigrateNorth_InvalidJson) {
    UbseIpcMessage req{};
    std::string body = "hello world";
    req.buffer = reinterpret_cast<uint8_t*>(const_cast<char*>(body.data()));
    req.length = body.size();

    UbseRequestContext context;
    uint32_t ret = LibvirtHandler::HamMigrateNorth(req, context);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestLibvirtHandler, HamMigrateNorth_MissingAction) {
    UbseIpcMessage req{};
    std::string body = R"({"scene":"ham_migrate"})";
    req.buffer = reinterpret_cast<uint8_t*>(const_cast<char*>(body.data()));
    req.length = body.size();

    UbseRequestContext context;
    uint32_t ret = LibvirtHandler::HamMigrateNorth(req, context);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestLibvirtHandler, HamMigrateNorth_DispatchHamBorrow) {
    UbseIpcMessage req{};
    std::string body = R"({"action":"borrow","srcHostname":"Node0","srcPid":123})";
    req.buffer = reinterpret_cast<uint8_t*>(const_cast<char*>(body.data()));
    req.length = body.size();

    MOCKER(HamMigrate::HandleHamMigrateBorrow).stubs().will(returnValue(VM_OK));

    UbseRequestContext context;
    uint32_t ret = LibvirtHandler::HamMigrateNorth(req, context);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestLibvirtHandler, HamMigrateNorth_DispatchHamClear) {
    UbseIpcMessage req{};
    std::string body = R"({"action":"clear","type":1,"srcHostname":"Node0"})";
    req.buffer = reinterpret_cast<uint8_t*>(const_cast<char*>(body.data()));
    req.length = body.size();

    MOCKER(HamMigrate::HandleHamMigrateClear).stubs().will(returnValue(VM_OK));

    UbseRequestContext context;
    uint32_t ret = LibvirtHandler::HamMigrateNorth(req, context);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestLibvirtHandler, HamMigrateNorth_DispatchFastRecoveryBorrow) {
    UbseIpcMessage req{};
    std::string body = R"({"action":"borrow","scene":"fastRecovery","uuid":"123"})";
    req.buffer = reinterpret_cast<uint8_t*>(const_cast<char*>(body.data()));
    req.length = body.size();

    MOCKER(FastRecovery::HandleFastRecoveryBorrow).stubs().will(returnValue(VM_OK));

    UbseRequestContext context;
    uint32_t ret = LibvirtHandler::HamMigrateNorth(req, context);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestLibvirtHandler, HamMigrateNorth_DispatchFastRecoveryClear) {
    UbseIpcMessage req{};
    std::string body = R"({"action":"clear","scene":"fastRecovery","uuid":"123"})";
    req.buffer = reinterpret_cast<uint8_t*>(const_cast<char*>(body.data()));
    req.length = body.size();

    MOCKER(FastRecovery::HandleFastRecoveryClear).stubs().will(returnValue(VM_OK));

    UbseRequestContext context;
    uint32_t ret = LibvirtHandler::HamMigrateNorth(req, context);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestLibvirtHandler, HamMigrateNorth_UnknownAction) {
    UbseIpcMessage req{};
    std::string body = R"({"action":"jump","scene":"mars"})";
    req.buffer = reinterpret_cast<uint8_t*>(const_cast<char*>(body.data()));
    req.length = body.size();

    UbseRequestContext context;
    uint32_t ret = LibvirtHandler::HamMigrateNorth(req, context);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestLibvirtHandler, ConvertToVaList_Success) {
    rapidjson::Document doc;
    doc.Parse(R"([{"start":4096,"length":4096},{"start":8192,"length":2048}])");

    std::vector<VirtualAddress> valist;
    VmResult ret = LibvirtHandler::ConvertToVaList(doc, valist);

    EXPECT_EQ(ret, VM_OK);
    EXPECT_EQ(valist.size(), 2);
    EXPECT_EQ(valist[0].start, 4096);
    EXPECT_EQ(valist[1].length, 2048);
}

TEST_F(TestLibvirtHandler, ConvertToVaList_NotArray) {
    rapidjson::Document doc;
    doc.Parse(R"({"start":4096})");

    std::vector<VirtualAddress> valist;
    VmResult ret = LibvirtHandler::ConvertToVaList(doc, valist);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestLibvirtHandler, ProcessResponse_Success) {
    RespInfo respInfo;
    respInfo.code = 200;
    respInfo.message = R"({"status":"ok"})";

    UbseIpcMessage resp{};
    uint64_t requestId = 100;

    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    VmResult ret = LibvirtHandler::ProcessResponse(respInfo, resp, requestId);

    EXPECT_EQ(ret, VM_OK);
    ASSERT_EQ(resp.buffer, nullptr);
}

TEST_F(TestLibvirtHandler, Start_RegisterIpcFailed) {
    MOCKER(RegisterIpcHandler).stubs().will(returnValue(VM_ERROR));

    VmResult ret = LibvirtHandler::Start();
    EXPECT_EQ(ret, VM_ERROR);
}


} // namespace ubse::vm::ut