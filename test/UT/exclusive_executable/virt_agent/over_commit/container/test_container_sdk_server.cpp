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

#include "test_container_sdk_server.h"

#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>
#include <securec.h>
#include "container_sdk_server.h"
#include "container_service.h"
#include "mem_container_msg.h"
#include "os_helper.h"
#include "ubse_api_server.h"
#include "ubse_error.h"
#include "vm_error.h"

using namespace vm;
using namespace vm::overcommit;
namespace ubse::vm::ut {
TestContainerSdkService::TestContainerSdkService() = default;

void TestContainerSdkService::SetUp()
{
    Test::SetUp();
}

void TestContainerSdkService::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

VmResult MockExecuteCommandOk()
{
    return VM_OK;
}

VmResult MockExecuteCommandError()
{
    return VM_ERROR;
}

VmResult testGetPidsByContainerIds(const std::unordered_set<std::string> &containerIds,
                                   std::unordered_map<std::string, std::vector<pid_t>> &containerInfos)
{
    containerInfos["container1"] = {1234, 5678, 91011};
    return VM_OK;
}

UBSRMRSPidNumaInfoCollectFunc testGetContainerMemInfoWithStructure()
{
    return [](const SrcMemoryBorrowParam &borrowParam, const std::vector<pid_t> &pids, std::vector<PidInfo> &pidInfos) {
        PidInfo pidInfo;
        pidInfo.pid = 71685;
        pidInfo.localUsedMem = 1024 * 1024 * 50;
        pidInfo.localNumaIds = {0, 1, 2};
        pidInfo.remoteUsedMem = 1024 * 1024 * 30;
        pidInfo.remoteNumaId = 3;
        pidInfos.push_back(pidInfo);
        return VM_OK;
    };
}

UBSRMRSSetWaterMarkFunc testSetWaterMarkFunc()
{
    return [](const WaterMark &) {
        return VM_OK;
    };
}

TEST_F(TestContainerSdkService, testSDKRegister)
{
    VirtContainerSdk virtContainerSdk;
    MOCKER(RegisterIpcHandler).stubs().will(invoke(MockExecuteCommandOk));
    auto ret = virtContainerSdk.Register();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestContainerSdkService, testSDKRegisterFailed)
{
    VirtContainerSdk virtContainerSdk;
    auto ret = virtContainerSdk.Register();
    EXPECT_EQ(ret, UBSE_ERROR_MODULE_LOAD_FAILED);
}

TEST_F(TestContainerSdkService, testGetContainerPidsHandler)
{
    VirtContainerSdk virtContainerSdk;

    UbseIpcMessage req;
    UbseRequestContext context;
    auto ret = virtContainerSdk.GetContainerPidsHandler(req, context);
    EXPECT_EQ(ret, VM_ERROR_NULLPTR);
}

TEST_F(TestContainerSdkService, testGetContainerPidsHandler2)
{
    VirtContainerSdk virtContainerSdk;
    UbseIpcMessage req;
    container_id_list_for_c container_list;
    std::string data = "b29b7dbe53e8362090d66e4324a7b1732947f14b5dcdbb17507659bb358d1ea4";
    const std::size_t maxSize = data.size() + 1;
    strcpy_s(container_list.containerId[0], maxSize, data.c_str());
    container_list.containerIdSize = 1;

    ContainerIdListForCInputMsg inputMsg;
    inputMsg.SetInputInfos(container_list);
    inputMsg.Serialize();

    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();

    UbseRequestContext context;
    MOCKER(SendResponse).stubs().will(invoke(MockExecuteCommandOk));
    MOCKER(OsHelper::GetPidsByContainerIds).stubs().will(invoke(testGetPidsByContainerIds));
    auto ret = virtContainerSdk.GetContainerPidsHandler(req, context);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestContainerSdkService, testGetContainerPidsHandler3)
{
    VirtContainerSdk virtContainerSdk;
    UbseIpcMessage req;
    container_id_list_for_c container_list;
    std::string data = "b29b7dbe53e8362090d66e4324a7b1732947f14b5dcdbb17507659bb358d1ea4";
    const std::size_t maxSize = data.size() + 1;
    strcpy_s(container_list.containerId[0], maxSize, data.c_str());
    container_list.containerIdSize = 1;

    ContainerIdListForCInputMsg inputMsg;
    inputMsg.SetInputInfos(container_list);
    inputMsg.Serialize();

    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();

    UbseRequestContext context;
    MOCKER(SendResponse).stubs().will(invoke(MockExecuteCommandError));
    MOCKER(OsHelper::GetPidsByContainerIds).stubs().will(invoke(testGetPidsByContainerIds));
    auto ret = virtContainerSdk.GetContainerPidsHandler(req, context);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestContainerSdkService, testGetMemInfoForPid)
{
    VirtContainerSdk virtContainerSdk;
    UbseIpcMessage req;
    pid_param_fo_c param = {0};
    snprintf_s(param.srcNid, sizeof(param.srcNid), sizeof(param.srcNid), "2");
    param.pids[0] = 71685;
    param.pid_size = 1;

    MemContainerPidMemInfoInputMsg inputMsg;
    inputMsg.SetInputInfos(param);
    inputMsg.Serialize();

    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();

    UbseRequestContext context;
    MOCKER(SendResponse).stubs().will(invoke(MockExecuteCommandOk));
    MOCKER(MempoolingModule::UBSRMRSPidNumaInfoCollect).stubs().will(invoke(testGetContainerMemInfoWithStructure));
    auto ret = virtContainerSdk.GetMemInfoForPid(req, context);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestContainerSdkService, testGetMemInfoForPid2)
{
    VirtContainerSdk virtContainerSdk;
    UbseIpcMessage req;
    pid_param_fo_c param = {0};
    snprintf_s(param.srcNid, sizeof(param.srcNid), sizeof(param.srcNid), "2");
    param.pids[0] = 71685;
    param.pid_size = 1;

    MemContainerPidMemInfoInputMsg inputMsg;
    inputMsg.SetInputInfos(param);
    inputMsg.Serialize();

    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();

    UbseRequestContext context;
    MOCKER(SendResponse).stubs().will(invoke(MockExecuteCommandError));
    MOCKER(MempoolingModule::UBSRMRSPidNumaInfoCollect).stubs().will(invoke(testGetContainerMemInfoWithStructure));
    auto ret = virtContainerSdk.GetMemInfoForPid(req, context);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestContainerSdkService, testInjectWaterLineHandler)
{
    VirtContainerSdk virtContainerSdk;
    UbseIpcMessage req;
    WaterMark param;
    param.highWaterMark = 123;
    param.lowWaterMark = 456;

    UpdateWaterLineForCInputMsg inputMsg;
    inputMsg.SetInputInfos(param);
    inputMsg.Serialize();

    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();

    UbseRequestContext context;
    MOCKER(SendResponse).stubs().will(invoke(MockExecuteCommandOk));
    MOCKER(MempoolingModule::UBSRMRSSetWaterMark).stubs().will(invoke(testSetWaterMarkFunc));
    auto ret = virtContainerSdk.InjectWaterLineHandler(req, context);
    EXPECT_NE(ret, VM_OK);
    param.highWaterMark = 80;
    param.lowWaterMark = 60;
    inputMsg.SetInputInfos(param);
    inputMsg.Serialize();
    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();
    ret = virtContainerSdk.InjectWaterLineHandler(req, context);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestContainerSdkService, testInjectWaterLineHandler2)
{
    VirtContainerSdk virtContainerSdk;
    UbseIpcMessage req;
    WaterMark param;
    param.highWaterMark = 123;
    param.lowWaterMark = 456;

    UpdateWaterLineForCInputMsg inputMsg;
    inputMsg.SetInputInfos(param);
    inputMsg.Serialize();

    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();

    UbseRequestContext context;
    MOCKER(SendResponse).stubs().will(invoke(MockExecuteCommandError));
    MOCKER(MempoolingModule::UBSRMRSSetWaterMark).stubs().will(invoke(testSetWaterMarkFunc));
    auto ret = virtContainerSdk.InjectWaterLineHandler(req, context);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestContainerSdkService, WaterLineMemBorrow)
{
    MemBorrowRequestC memBorrowRequest{{"0", {{0, 0}}, 1}, {100, 200}, 2, {80, 60}};
    MemContainerWaterLineMemBorrowInputMsg inputMsg{memBorrowRequest};
    inputMsg.Serialize();
    UbseIpcMessage req{inputMsg.SerializedData(), inputMsg.SerializedDataSize()};
    UbseRequestContext context;
    MOCKER(&ContainerService::MemBorrow).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(0));
    auto ret = VirtContainerSdk::WaterLineMemBorrow(req, context);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestContainerSdkService, WaterLineMemBorrowFail)
{
    MemBorrowRequestC memBorrowRequest{{"0", {{0, 0}}, 1}, {100, 200}, 2, {80, 60}};
    MemContainerWaterLineMemBorrowInputMsg inputMsg{memBorrowRequest};
    inputMsg.Serialize();
    UbseIpcMessage req{inputMsg.SerializedData(), inputMsg.SerializedDataSize()};
    UbseRequestContext context;
    MOCKER(&ContainerService::MemBorrow).stubs().will(returnValue(VM_ERROR));
    auto ret = VirtContainerSdk::WaterLineMemBorrow(req, context);
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(&ContainerService::MemBorrow).reset();
    MOCKER(&ContainerService::MemBorrow).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(1));
    ret = VirtContainerSdk::WaterLineMemBorrow(req, context);
    EXPECT_NE(ret, VM_OK);
}

TEST_F(TestContainerSdkService, WaterLineMemMigrate)
{
    MemMigrateRequestC param{{"0", {{0, 0}}, 1}, {"0", "1"}, 2, {{100, 50}}, 1};
    MemContainerWaterLineMemMigrateInputMsg inputMsg{param};
    inputMsg.Serialize();
    UbseIpcMessage req{inputMsg.SerializedData(), inputMsg.SerializedDataSize()};
    UbseRequestContext context;
    MOCKER(&ContainerService::MemMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(0));
    auto ret = VirtContainerSdk::WaterLineMemMigrate(req, context);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestContainerSdkService, WaterLineMemMigrateFail)
{
    MemMigrateRequestC param{{"0", {{0, 0}}, 1}, {"0", "1"}, 2, {{100, 50}}, 1};
    MemContainerWaterLineMemMigrateInputMsg inputMsg{param};
    inputMsg.Serialize();
    UbseIpcMessage req{inputMsg.SerializedData(), inputMsg.SerializedDataSize()};
    UbseRequestContext context;
    MOCKER(&ContainerService::MemMigrate).stubs().will(returnValue(VM_ERROR));
    auto ret = VirtContainerSdk::WaterLineMemMigrate(req, context);
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(&ContainerService::MemMigrate).reset();
    MOCKER(&ContainerService::MemMigrate).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(1));
    ret = VirtContainerSdk::WaterLineMemMigrate(req, context);
    EXPECT_NE(ret, VM_OK);
}

TEST_F(TestContainerSdkService, WaterLineMemReturn)
{
    MemReturnRequestC param{{"0", {{0, 0}}, 1}, {"0", "1"}, 2, {1, 2}, 2};
    MemContainerWaterLineMemReturnInputMsg inputMsg{param};
    inputMsg.Serialize();
    UbseIpcMessage req{inputMsg.SerializedData(), inputMsg.SerializedDataSize()};
    UbseRequestContext context;
    MOCKER(&ContainerService::MemReturn).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(0));
    auto ret = VirtContainerSdk::WaterLineMemReturn(req, context);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestContainerSdkService, WaterLineMemReturnFail)
{
    MemReturnRequestC param{{"0", {{0, 0}}, 1}, {"0", "1"}, 2, {1, 2}, 2};
    MemContainerWaterLineMemReturnInputMsg inputMsg{param};
    inputMsg.Serialize();
    UbseIpcMessage req{inputMsg.SerializedData(), inputMsg.SerializedDataSize()};
    UbseRequestContext context;
    MOCKER(&ContainerService::MemReturn).stubs().will(returnValue(VM_ERROR));
    auto ret = VirtContainerSdk::WaterLineMemReturn(req, context);
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(&ContainerService::MemReturn).reset();
    MOCKER(&ContainerService::MemReturn).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(1));
    ret = VirtContainerSdk::WaterLineMemReturn(req, context);
    EXPECT_NE(ret, VM_OK);
}
} // namespace ubse::vm::ut