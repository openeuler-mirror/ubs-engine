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
#include "test_fast_recovery.h"
#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>
#include <ubse_api_server.h>
#include <ubse_com.h>
#include <ubse_logger.h>
#include <ubse_mem_controller.h>
#include <ubse_node.h>
#include "vm_configuration.h"
#include "resource_query.h"
#include "fast_recovery.h"
#include "libvirt_handler.h"

using namespace vm;
using namespace ubse::mem::controller;
using namespace ubse::nodeController;
using namespace ubse::com;
namespace ubse::vm::ut {
TestFastRecovery::TestFastRecovery() = default;

void TestFastRecovery::SetUp() {
    Test::SetUp();
    VmConfiguration::GetInstance().LoadConfig();
    MOCKER(com::UbseRegRpcService).stubs().will(returnValue(VM_OK));
}

void TestFastRecovery::TearDown() {
    Test::TearDown();
    GlobalMockObject::verify();
}

VmResult MockGetVmDomainInfoForFR(HostVmDomainInfo &hostVmDomainInfo) {
    VmDomainInfo info;
    info.pid = 123;
    info.uuid = "test-uuid-recovery";
    info.numaMemInfo.emplace(0, mempooling::VmDomainNumaInfo{0, 0, 0, 0, true});
    hostVmDomainInfo.vmDomainInfos.push_back(info);
    return VM_OK;
}

TEST_F(TestFastRecovery, HandleFastRecoveryBorrow_Success) {
    rapidjson::Document msgJson;
    msgJson.Parse(R"({
        "uuid":"test-uuid-recovery",
        "dstHostname":"Node1",
        "valist":[{"start":4096,"length":4096}]
    })");

    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfoForFR));
    MOCKER(UbseNodeGetNodeIdByHostname).stubs().will(returnValue(VM_OK));
    MOCKER(LibvirtHandler::ConvertToVaList).stubs().will(returnValue(VM_OK));
    MOCKER(UbseMemAddrCreate).stubs().will(returnValue(VM_OK));
    MOCKER(LibvirtHandler::ProcessResponse).stubs().will(returnValue(VM_OK));

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;

    VmResult ret = FastRecovery::HandleFastRecoveryBorrow(msgJson, respInfo, resp, context);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestFastRecovery, HandleFastRecoveryBorrow_ParseFailed) {
    rapidjson::Document msgJson;
    msgJson.Parse(R"({"uuid":123})"); // uuid should be string

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;

    VmResult ret = FastRecovery::HandleFastRecoveryBorrow(msgJson, respInfo, resp, context);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestFastRecovery, HandleFastRecoveryBorrow_BorrowFailed) {
    rapidjson::Document msgJson;
    msgJson.Parse(R"({"uuid":"test-uuid","dstHostname":"Node1","valist":[]})");

    MOCKER(LibvirtHandler::ConvertToVaList).stubs().will(returnValue(VM_OK));
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().will(invoke(MockGetVmDomainInfoForFR));
    MOCKER(UbseNodeGetNodeIdByHostname).stubs().will(returnValue(VM_OK));
    MOCKER(UbseMemAddrCreate).stubs().will(returnValue(VM_ERROR));

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;

    VmResult ret = FastRecovery::HandleFastRecoveryBorrow(msgJson, respInfo, resp, context);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestFastRecovery, HandleFastRecoveryClear_Success) {
    rapidjson::Document msgJson;
    msgJson.Parse(R"({"uuid":"test-uuid-clear"})");

    MOCKER(UbseMemAddrDelete).stubs().will(returnValue(VM_OK));
    MOCKER(LibvirtHandler::ProcessResponse).stubs().will(returnValue(VM_OK));

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;

    VmResult ret = FastRecovery::HandleFastRecoveryClear(msgJson, respInfo, resp, context);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestFastRecovery, HandleFastRecoveryClear_RetrySuccess) {
    rapidjson::Document msgJson;
    msgJson.Parse(R"({"uuid":"test-uuid-retry"})");

    MOCKER(UbseMemAddrDelete).stubs()
        .will(returnValue(VM_ERROR))
        .then(returnValue(VM_OK));

    MOCKER(LibvirtHandler::ProcessResponse).stubs().will(returnValue(VM_OK));

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;

    VmResult ret = FastRecovery::HandleFastRecoveryClear(msgJson, respInfo, resp, context);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestFastRecovery, HandleFastRecoveryClear_FailedAfterRetry) {
    rapidjson::Document msgJson;
    msgJson.Parse(R"({"uuid":"test-uuid-fail"})");

    MOCKER(UbseMemAddrDelete).stubs().will(returnValue(VM_ERROR));

    RespInfo respInfo;
    UbseIpcMessage resp{};
    UbseRequestContext context;

    VmResult ret = FastRecovery::HandleFastRecoveryClear(msgJson, respInfo, resp, context);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestFastRecovery, ConvertToBorrowFastRecovery_MissingField) {
    rapidjson::Document msgJson;
    msgJson.Parse(R"({"uuid":"test-uuid"})"); // 缺少dstHostname

    BorrowInfo borrowInfo;
    VmResult ret = FastRecovery::ConvertToBorrowFastRecovery(msgJson, borrowInfo);
    EXPECT_EQ(ret, VM_ERROR);
}

} // namespace ubse::vm::ut