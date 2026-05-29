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

#include "test_vm_migrate.h"
#include "ubse_api_server.h"
#include "mem_migrate_msg.h"
#include "mockcpp/mockcpp.hpp"
#include "resource_collect.h"
#include "resource_query.h"
#include "vm_http_util.h"
#include "vm_json_util.h"
#include "vm_migrate.h"

using namespace vm;
using namespace api::server;
namespace ubse::ut::vm {

// 设置测试环境
void TestVmMigrate::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestVmMigrate::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestVmMigrate, ToUpdateVmStatusTest)
{
    VmMigrate vmMigrate;
    NumaMemInfoMap numaMemInfoMap{};
    VmMigrateStatus vmMigrateStatus;
    // UpdateVMStatus失败
    MOCKER(&ResourceCollect::UpdateVMStatus).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(vmMigrate.ToUpdateVmStatus(numaMemInfoMap, "uuid", 1, vmMigrateStatus), VM_ERROR);
    MOCKER(&ResourceCollect::UpdateVMStatus).reset();
    // 成功
    MOCKER(&ResourceCollect::UpdateVMStatus).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(vmMigrate.ToUpdateVmStatus(numaMemInfoMap, "uuid", 1, vmMigrateStatus), VM_OK);
    GlobalMockObject::verify();
}

VmResult MockGetHostVmDomainInfos(std::vector<HostVmDomainInfo>& hostVmDomainInfos)
{
    HostVmDomainInfo hostVmDomainInfo;
    VmDomainInfo vmDomainInfo = {.uuid = "uuid", .nodeId = "Node0", .pid = 1};

    hostVmDomainInfo.vmDomainInfos.push_back(vmDomainInfo);
    hostVmDomainInfos.push_back(hostVmDomainInfo);
    return VM_OK;
}

TEST_F(TestVmMigrate, FindNodeIdPidTest)
{
    VmMigrate vmMigrate;
    NumaMemInfoMap numaMemInfoMap{};
    std::string nodeId;
    pid_t pid;
    EXPECT_EQ(vmMigrate.FindNodeIdPid("uuid", nodeId, pid, numaMemInfoMap), VM_ERROR);
    HostVmDomainInfo info{
        "0", "name", "metric", {{"uuid", "name", "s", 0, 100, 0, 100, "0", "n", 111, 10, {{0, {0, 0, 0, 0, 0}}}}}};
    MOCKER(ResourceQuery::GetVmDomainInfosFromGlobal).stubs().with(outBound(info)).will(returnValue(VM_OK));
    EXPECT_EQ(vmMigrate.FindNodeIdPid("uuid", nodeId, pid, numaMemInfoMap), VM_OK);
}

TEST_F(TestVmMigrate, ToJsonTest)
{
    PageFlowResultParam pageFlowResultParam = {.ret = VM_OK, .msg = "msg"};
    // 转化失败
    MOCKER(VMJsonUtil::VMConvertMap2JsonStr).stubs().will(returnValue(false));
    EXPECT_EQ(pageFlowResultParam.ToJson(), "");
    MOCKER(VMJsonUtil::VMConvertMap2JsonStr).reset();
    // 成功
    EXPECT_EQ(pageFlowResultParam.ToJson(), "{\n    \"msg\": \"msg\",\n    \"ret\": 0\n}");
    GlobalMockObject::verify();
}

TEST_F(TestVmMigrate, UpdatePageFlowAndStatus_DeserializeFail)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    req.length = 10;
    req.buffer = new uint8_t[10];
    uint32_t result = VmMigrate::UpdatePageFlowAndStatus(req, context);
    EXPECT_NE(result, VM_OK);
    GlobalMockObject::verify();
    delete[] req.buffer;
}

TEST_F(TestVmMigrate, UpdatePageFlowAndStatus_PidNotFound)
{
    MemMigrateInputParams param{"opt", "uuid"};
    MemMigrateMsg msg{param};
    msg.Serialize();
    UbseIpcMessage req{msg.SerializedData(), msg.SerializedDataSize()};
    UbseRequestContext context;
    uint32_t result = VmMigrate::UpdatePageFlowAndStatus(req, context);
    EXPECT_NE(result, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestVmMigrate, UpdatePageFlowAndStatus)
{
    MemMigrateInputParams param{"opt", "uuid"};
    MemMigrateMsg msg{param};
    msg.Serialize();
    UbseIpcMessage req{msg.SerializedData(), msg.SerializedDataSize()};
    UbseRequestContext context;
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    uint32_t result = VmMigrate::UpdatePageFlowAndStatus(req, context);
    EXPECT_EQ(result, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestVmMigrate, BuildIpcResponse)
{
    std::string message{"test"};
    UbseIpcMessage resp{};
    auto ret = VmMigrate::BuildIpcResponse(message, resp);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestVmMigrate, UpdateVmStatusByMigrateStatus_MigratingSuccess)
{
    std::string opt{NONE_AND_MIGRATING};
    NumaMemInfoMap numaMemInfomap{};
    std::string uuid{"uuid"};
    UbseIpcMessage response{};
    MOCKER(VmMigrate::ToUpdateVmStatus).stubs().will(returnValue(VM_OK));
    auto ret = VmMigrate::UpdateVmStatusByMigrateStatus(opt, numaMemInfomap, uuid, 0, response);
    EXPECT_EQ(ret, 0);
    GlobalMockObject::verify();
}

TEST_F(TestVmMigrate, UpdateVmStatusByMigrateStatus_MigratableFail)
{
    std::string opt{NONE_AND_MIGRATE_SUCCESS};
    NumaMemInfoMap numaMemInfomap{};
    std::string uuid{"uuid"};
    UbseIpcMessage response{};
    MOCKER(VmMigrate::ToUpdateVmStatus).stubs().will(returnValue(VM_ERROR));
    auto ret = VmMigrate::UpdateVmStatusByMigrateStatus(opt, numaMemInfomap, uuid, 0, response);
    EXPECT_NE(ret, 0);
    GlobalMockObject::verify();
}

TEST_F(TestVmMigrate, Register)
{
    MOCKER(RegisterIpcHandler).stubs().will(returnValue(VM_OK));
    auto ret = VmMigrate::Register();
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestVmMigrate, RegisterFail)
{
    MOCKER(RegisterIpcHandler).stubs().will(returnValue(VM_ERROR));
    auto ret = VmMigrate::Register();
    EXPECT_EQ(ret, VM_ERROR);
    GlobalMockObject::verify();
}

TEST_F(TestVmMigrate, ProcessRequest_none_migrating)
{
    MemMigrateInputParams param{"none_migrating", "uuid"};
    UbseIpcMessage response;

    std::string nodeId{"0"};
    pid_t pid{111};
    NumaMemInfoMap numaMemInfoMap{{0, {}}};
    MOCKER(VmMigrate::FindNodeIdPid)
        .stubs()
        .with(_, outBound(nodeId), outBound(pid), outBound(numaMemInfoMap))
        .will(returnValue(VM_OK));
    MOCKER(VmMigrate::UpdateVmStatusByMigrateStatus).stubs().will(returnValue(VM_OK));

    auto ret = VmMigrate::ProcessRequest(param, response);
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestVmMigrate, ProcessRequest_Migrateable)
{
    MemMigrateInputParams param{"true", "uuid"};
    UbseIpcMessage response;

    std::string nodeId{"0"};
    pid_t pid{111};
    NumaMemInfoMap numaMemInfoMap{{0, {}}};
    MOCKER(VmMigrate::FindNodeIdPid)
        .stubs()
        .with(_, outBound(nodeId), outBound(pid), outBound(numaMemInfoMap))
        .will(returnValue(VM_OK));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(VmMigrate::ToUpdateVmStatus).stubs().will(returnValue(VM_OK));
    auto ret = VmMigrate::ProcessRequest(param, response);
    EXPECT_EQ(ret, VM_OK);
    param.opt = "false";
    ret = VmMigrate::ProcessRequest(param, response);
    EXPECT_EQ(ret, VM_OK);
    GlobalMockObject::verify();
}

TEST_F(TestVmMigrate, ProcessRequest_EnableProcessMigrateFail)
{
    MemMigrateInputParams param{"false", "uuid"};
    UbseIpcMessage response;

    std::string nodeId{"0"};
    pid_t pid{111};
    NumaMemInfoMap numaMemInfoMap{{0, {}}};
    MOCKER(VmMigrate::FindNodeIdPid)
        .stubs()
        .with(_, outBound(nodeId), outBound(pid), outBound(numaMemInfoMap))
        .will(returnValue(VM_OK));
    MOCKER(HttpUtil::EnableProcessMigrate).stubs().will(returnValue(VM_ERROR));
    auto ret = VmMigrate::ProcessRequest(param, response);
    EXPECT_EQ(ret, VM_ERROR);
    GlobalMockObject::verify();
}
} // namespace ubse::ut::vm