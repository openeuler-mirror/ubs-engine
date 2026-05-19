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

#include "test_mem_fragmentation_sdk_server.h"

#include <ubse_api_server.h>
#include <ubse_election.h>
#include <ubse_security.h>
#include <ubse_com.h>
#include <mockcpp/mockcpp.hpp>
#include <vector>

#include "gmock/gmock-actions.h"
#include "hugepage_handler.h"
#include "mem_fragmentation_msg.h"
#include "mem_fragmentation_sdk_server.h"
#include "mem_handler.h"
#include "mem_task_manager.h"
#include "mempooling_def.h"
#include "mempooling_module.h"
#include "msg_utils.h"
#include "status_manager.h"
#include "vm_error.h"

using namespace api::server;
using namespace ubse::election;
using namespace ubse::security;
using namespace ubse::com;
using namespace vm;
using namespace vm::mempooling;
namespace ubse::ut::vm {

void TestMemFragmentationSdkServer::SetUp()
{
    Test::SetUp();
}

void TestMemFragmentationSdkServer::TearDown()
{
    Test::TearDown();
}

pid_t globalDefaultPid = 1234;
uint16_t globalDefaultMemSize = 2048;
uint64_t globalDefaultWaitingTime = 12000;
time_t globalDefaultTimestamp = 17370099141;
uint64_t globalDefaultHugePage = 41000;
uint64_t globalDefaultMemTotal = 98455804;
uint64_t globalDefaultPageSize = 2048;
uint64_t globalMemBorrowedSize = 1073741824;
uint64_t globalMemBorrowedSize_MB = 1048576;

TEST_F(TestMemFragmentationSdkServer, Register_Failed)
{
    MOCKER(RegisterIpcHandler).stubs().will(returnValue(VM_ERROR));
    MOCKER(UbseRegRpcService).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::Register(), VM_ERROR);
    MOCKER(RegisterIpcHandler).reset();
}

TEST_F(TestMemFragmentationSdkServer, Register_Success)
{
    MOCKER(RegisterIpcHandler).stubs().will(returnValue(VM_OK));
    MOCKER(UbseRegRpcService).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMemFragSdk::Register(), VM_OK);
    MOCKER(RegisterIpcHandler).reset();
}

TEST_F(TestMemFragmentationSdkServer, GetNodeInfo_ShouldReturnError_WhenMempoolingNotInitialized)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    EXPECT_EQ(VirtMemFragSdk::GetNodeInfo(req, context), VM_ERROR);
}

TEST_F(TestMemFragmentationSdkServer, GetNodeInfo_ShouldReturnError_WhenGetUBSRMRSGetNumaInfoListOnNodeFailed)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    UBSRMRSGetNumaInfoListOnNodeFunc func = nullptr;
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode).stubs().will(returnValue(func));
    EXPECT_EQ(VirtMemFragSdk::GetNodeInfo(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode).reset();
}

UBSRMRSGetNumaInfoListOnNodeFunc MockUBSRMRSGetNumaInfoListOnNodeError()
{
    return [](std::vector<NumaInfo>&) {
        return VM_ERROR;
    };
}

UBSRMRSGetNumaInfoListOnNodeFunc MockUBSRMRSGetNumaInfoListOnNodeIsEmpty()
{
    return [](std::vector<NumaInfo>& vmInfoList) {
        return VM_OK;
    };
}

UBSRMRSGetNumaInfoListOnNodeFunc MockUBSRMRSGetNumaInfoListOnNodeOK()
{
    return [](std::vector<NumaInfo>& numaInfos) {
        NumaInfo numaInfo;
        numaInfo.metaData.nodeId = "1";
        numaInfo.metaData.numaId = 0;
        numaInfo.metaData.isLocal = true;
        numaInfo.metaData.memTotal = globalDefaultMemTotal;
        numaInfo.metaData.memFree = globalDefaultMemTotal;

        NumaPageData pageData;
        pageData.pageSize = NO_2048;
        pageData.hugePageTotal = globalDefaultHugePage;
        pageData.hugePageFree = globalDefaultHugePage;
        numaInfo.metaData.numaPageInfo[NO_2048] = pageData;

        numaInfo.metaData.socketId = 0;
        numaInfos.push_back(numaInfo);
        return VM_OK;
    };
}

TEST_F(TestMemFragmentationSdkServer, GetNodeInfo_ShouldReturnError_WhenUBSRMRSGetNumaInfoListOnNodeError)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode).stubs().will(invoke(MockUBSRMRSGetNumaInfoListOnNodeError));
    EXPECT_EQ(VirtMemFragSdk::GetNodeInfo(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode).reset();
}

/**
 * @tc.name  : GetNodeInfo_ShouldReturnOK_WhenUBSRMRSGetNumaInfoListOnNodeReturnEmpty
 * @tc.number: GetNodeInfo_004
 * @tc.desc  : 当UBSRMRSGetNumaInfoListOnNode函数返回为空时，函数返回成功
 */
TEST_F(TestMemFragmentationSdkServer, GetNodeInfo_ShouldReturnError_WhenUBSRMRSGetNumaInfoListOnNodeReturnEmpty)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode)
        .stubs()
        .will(invoke(MockUBSRMRSGetNumaInfoListOnNodeIsEmpty));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMemFragSdk::GetNodeInfo(req, context), VM_OK);
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : GetNodeInfo_ShouldReturnError_WhenMemcpyFailed
 * @tc.number: GetNodeInfo_005
 * @tc.desc  : 当memcpy_s函数失败时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, GetNodeInfo_ShouldReturnError_WhenMemcpyFailed)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode).stubs().will(invoke(MockUBSRMRSGetNumaInfoListOnNodeOK));
    MOCKER(memcpy_s).stubs().will(returnValue(1));
    EXPECT_EQ(VirtMemFragSdk::GetNodeInfo(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode).reset();
    MOCKER(memcpy_s).reset();
}

/**
 * @tc.name  : GetNodeInfo_ShouldReturnError_WhenSendResponseFailed
 * @tc.number: GetNodeInfo_006
 * @tc.desc  : 当SendResponse函数失败时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, GetNodeInfo_ShouldReturnError_WhenSendResponseFailed)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode).stubs().will(invoke(MockUBSRMRSGetNumaInfoListOnNodeOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_ERROR_DESERIALIZE_ERROR));
    EXPECT_EQ(VirtMemFragSdk::GetNodeInfo(req, context), VM_ERROR_DESERIALIZE_ERROR);
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : GetNodeInfo_ShouldReturnOk_WhenEverythingIsOk
 * @tc.number: GetNodeInfo_007
 * @tc.desc  : 当一切函数正常时，函数返回成功
 */
TEST_F(TestMemFragmentationSdkServer, GetNodeInfo_ShouldReturnOk_WhenEverythingIsOk)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode).stubs().will(invoke(MockUBSRMRSGetNumaInfoListOnNodeOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMemFragSdk::GetNodeInfo(req, context), VM_OK);
    MOCKER(MempoolingModule::UBSRMRSGetNumaInfoListOnNode).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : GetVmInfo_ShouldReturnError_WhenMempoolingNotInitialized
 * @tc.number: GetVmInfo_001
 * @tc.desc  : 当mempooling未初始化时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, GetVmInfo_ShouldReturnError_WhenMempoolingNotInitialized)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    EXPECT_EQ(VirtMemFragSdk::GetVmInfo(req, context), VM_ERROR);
}

/**
 * @tc.name  : GetVmInfo_ShouldReturnError_WhenGetUBSRMRSGetVmInfoListOnNodeFailed
 * @tc.number: GetVmInfo_002
 * @tc.desc  : 当获取UBSRMRSGetVmInfoListOnNode函数失败时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, GetVmInfo_ShouldReturnError_WhenGetUBSRMRSGetVmInfoListOnNodeFailed)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    UBSRMRSGetVmInfoListOnNodeFunc func = nullptr;
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode).stubs().will(returnValue(func));
    EXPECT_EQ(VirtMemFragSdk::GetVmInfo(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode).reset();
}

UBSRMRSGetVmInfoListOnNodeFunc MockUBSRMRSGetVmInfoListOnNodeError()
{
    return [](std::vector<mempooling::VmDomainInfo>&) {
        return VM_ERROR;
    };
}

UBSRMRSGetVmInfoListOnNodeFunc MockUBSRMRSGetVmInfoListOnNodeIsEmpty()
{
    return [](std::vector<mempooling::VmDomainInfo>& vmInfoList) {
        return VM_OK;
    };
}

UBSRMRSGetVmInfoListOnNodeFunc MockUBSRMRSGetVmInfoListOnNodeOK()
{
    return [](std::vector<mempooling::VmDomainInfo>& vmDomainInfos) {
        mempooling::VmDomainInfo vmDomainInfo;
        vmDomainInfo.timestamp = globalDefaultTimestamp;
        vmDomainInfo.metaData.hostName = "controller";
        vmDomainInfo.metaData.nodeId = "1";
        vmDomainInfo.metaData.pid = globalDefaultPid;
        vmDomainInfo.metaData.uuid = "94a20ea4-fd5a-11ee-947c-0007c6034b7c";
        vmDomainInfo.metaData.name = "test_2u2g_1";
        vmDomainInfo.metaData.state = "RUNNING";
        vmDomainInfo.metaData.vmCreateTime = globalDefaultTimestamp;
        vmDomainInfo.metaData.maxMem = globalDefaultMemTotal;
        vmDomainInfos.push_back(vmDomainInfo);
        return VM_OK;
    };
}

/**
 * @tc.name  : GetVmInfo_ShouldReturnError_WhenUBSRMRSGetVmInfoListOnNodeFailed
 * @tc.number: GetVmInfo_003
 * @tc.desc  : 当UBSRMRSGetVmInfoListOnNode函数失败时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, GetVmInfo_ShouldReturnError_WhenUBSRMRSGetVmInfoListOnNodeFailed)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode).stubs().will(invoke(MockUBSRMRSGetVmInfoListOnNodeError));
    EXPECT_EQ(VirtMemFragSdk::GetVmInfo(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode).reset();
}

/**
 * @tc.name  : GetVmInfo_ShouldReturnOK_WhenUBSRMRSGetVmInfoListOnNodeReturnEmpty
 * @tc.number: GetVmInfo_004
 * @tc.desc  : 当UBSRMRSGetVmInfoListOnNode函数返回为空时，函数返回OK
 */
TEST_F(TestMemFragmentationSdkServer, GetVmInfo_ShouldReturnError_WhenUBSRMRSGetVmInfoListOnNodeReturnEmpty)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode).stubs().will(invoke(MockUBSRMRSGetVmInfoListOnNodeIsEmpty));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMemFragSdk::GetVmInfo(req, context), VM_OK);
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : GetVmInfo_ShouldReturnOK_WhenMemcpyFailed
 * @tc.number: GetVmInfo_005
 * @tc.desc  : 当memcpy_s失败时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, GetVmInfo_ShouldReturnOK_WhenMemcpyFailed)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode).stubs().will(invoke(MockUBSRMRSGetVmInfoListOnNodeOK));
    MOCKER(memcpy_s).stubs().will(returnValue(1));
    EXPECT_EQ(VirtMemFragSdk::GetVmInfo(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode).reset();
    MOCKER(memcpy_s).reset();
}

/**
 * @tc.name  : GetVmInfo_ShouldReturnError_WhenSendResponseFailed
 * @tc.number: GetVmInfo_006
 * @tc.desc  : 当SendResponse失败时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, GetVmInfo_ShouldReturnError_WhenSendResponseFailed)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode).stubs().will(invoke(MockUBSRMRSGetVmInfoListOnNodeOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_ERROR_DESERIALIZE_ERROR));
    EXPECT_EQ(VirtMemFragSdk::GetVmInfo(req, context), VM_ERROR_DESERIALIZE_ERROR);
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : GetVmInfo_ShouldReturnOK_WhenEverythingIsOk
 * @tc.number: GetVmInfo_007
 * @tc.desc  : 当一切功能正常时，函数返回成功
 */
TEST_F(TestMemFragmentationSdkServer, GetVmInfo_ShouldReturnOK_WhenEverythingIsOk)
{
    UbseIpcMessage req;
    UbseRequestContext context;
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode).stubs().will(invoke(MockUBSRMRSGetVmInfoListOnNodeOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMemFragSdk::GetVmInfo(req, context), VM_OK);
    MOCKER(MempoolingModule::UBSRMRSGetVmInfoListOnNode).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : ValidateRequest_ShouldReturnFalse_WhenBufferIsNull
 * @tc.number: ValidateRequest_001
 * @tc.desc  : 当传入空指针时，返回false
 */
TEST_F(TestMemFragmentationSdkServer, ValidateRequest_ShouldReturnFalse_WhenBufferIsNull)
{
    UbseIpcMessage req;
    req.buffer = nullptr;
    req.length = 1;
    EXPECT_FALSE(VirtMemFragSdk::ValidateRequest(req));
}

/**
 * @tc.name  : ValidateRequest_ShouldReturnFalse_WhenLengthIsZero
 * @tc.number: ValidateRequest_002
 * @tc.desc  : 当传入长度为0时，返回false
 */
TEST_F(TestMemFragmentationSdkServer, ValidateRequest_ShouldReturnFalse_WhenLengthIsZero)
{
    UbseIpcMessage req;
    req.buffer = new uint8_t[1];
    req.length = 0;
    EXPECT_FALSE(VirtMemFragSdk::ValidateRequest(req));
    delete[] req.buffer;
}

/**
 * @tc.name  : ValidateRequest_ShouldReturnTrue_WhenBufferIsValid
 * @tc.number: ValidateRequest_003
 * @tc.desc  : 当传入合法时，返回true
 */
TEST_F(TestMemFragmentationSdkServer, ValidateRequest_ShouldReturnTrue_WhenBufferIsValid)
{
    UbseIpcMessage req;
    req.buffer = new uint8_t[1];
    req.length = 1;
    EXPECT_TRUE(VirtMemFragSdk::ValidateRequest(req));
    delete[] req.buffer;
}

/**
 * @tc.name  : UpdateAntiAffinityConfig_ShouldReturnError_WhenUpdateAntiNodeIsNull
 * @tc.number: UpdateAntiAffinityConfig_001
 * @tc.desc  : 获取mempooling接口为空时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, UpdateAntiAffinityConfig_ShouldReturnError_WhenUpdateAntiNodeIsNull)
{
    std::map<std::string, std::vector<std::string>> nodeAntiAffinityMap;
    UBSRMRSUpdateAntiNodeFunc func = nullptr;
    MOCKER(MempoolingModule::UBSRMRSUpdateAntiNode).stubs().will(returnValue(func));
    EXPECT_EQ(VirtMemFragSdk::UpdateAntiAffinityConfig(nodeAntiAffinityMap), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSUpdateAntiNode).reset();
}

UBSRMRSUpdateAntiNodeFunc MockUBSRMRSUpdateAntiNodeError()
{
    return [](const std::map<std::string, std::vector<std::string>>&) {
        return VM_ERROR;
    };
}

UBSRMRSUpdateAntiNodeFunc MockUBSRMRSUpdateAntiNodeOK()
{
    return [](const std::map<std::string, std::vector<std::string>>&) {
        return VM_OK;
    };
}

/**
 * @tc.name  : UpdateAntiAffinityConfig_ShouldReturnError_WhenUpdateFails
 * @tc.number: UpdateAntiAffinityConfig_002
 * @tc.desc  : 当UBSRMRSUpdateAntiNode函数失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, UpdateAntiAffinityConfig_ShouldReturnError_WhenUpdateFails)
{
    std::map<std::string, std::vector<std::string>> nodeAntiAffinityMap;
    MOCKER(MempoolingModule::UBSRMRSUpdateAntiNode).stubs().will(invoke(MockUBSRMRSUpdateAntiNodeError));
    EXPECT_EQ(VirtMemFragSdk::UpdateAntiAffinityConfig(nodeAntiAffinityMap), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSUpdateAntiNode).reset();
}

/**
 * @tc.name  : UpdateAntiAffinityConfig_ShouldReturnOk_WhenUpdateSucceeds
 * @tc.number: UpdateAntiAffinityConfig_003
 * @tc.desc  : 当UBSRMRSUpdateAntiNode函数成功时，返回OK
 */
TEST_F(TestMemFragmentationSdkServer, UpdateAntiAffinityConfig_ShouldReturnOk_WhenUpdateSucceeds)
{
    std::map<std::string, std::vector<std::string>> nodeAntiAffinityMap;
    MOCKER(MempoolingModule::UBSRMRSUpdateAntiNode).stubs().will(invoke(MockUBSRMRSUpdateAntiNodeOK));
    EXPECT_EQ(VirtMemFragSdk::UpdateAntiAffinityConfig(nodeAntiAffinityMap), VM_OK);
    MOCKER(MempoolingModule::UBSRMRSUpdateAntiNode).reset();
}

/**
 * @tc.name  : ParseKey_ShouldReturnEmpty_WhenBufferIsInsufficient
 * @tc.number: ParseKey_001
 * @tc.desc  : 当buffer_size不足时，返回为空
 */
TEST_F(TestMemFragmentationSdkServer, ParseKey_ShouldReturnEmpty_WhenBufferIsInsufficient)
{
    const uint8_t buffer[] = {0, 0, 0, 0};
    const uint8_t* ptr = buffer;
    auto result = VirtMemFragSdk::ParseKey(buffer, sizeof(buffer) - 1, ptr);
    EXPECT_TRUE(result.first.empty());
}

/**
 * @tc.name  : ParseKey_ShouldReturnKey_WhenBufferIsValid
 * @tc.number: ParseKey_002
 * @tc.desc  : 当参数合法时，返回key值
 */
TEST_F(TestMemFragmentationSdkServer, ParseKey_ShouldReturnKey_WhenBufferIsValid)
{
    const uint8_t buffer[] = {0x05, 0x00, 0x00, 0x00, 't', 'e', 's', 't', 0x00};
    const uint8_t* ptr = buffer;
    auto result = VirtMemFragSdk::ParseKey(buffer, sizeof(buffer), ptr);
    EXPECT_EQ(result.first, "test");
}

/**
 * @tc.name  : ParseValues_ShouldReturnEmpty_WhenBufferIsInsufficient
 * @tc.number: ParseValues_001
 * @tc.desc  : 当buffer_size不足时，返回为空
 */
TEST_F(TestMemFragmentationSdkServer, ParseValues_ShouldReturnEmpty_WhenBufferIsInsufficient)
{
    const uint8_t buffer[] = {0, 0, 0, 0};
    const uint8_t* ptr = buffer;
    auto result = VirtMemFragSdk::ParseValues(buffer, sizeof(buffer) - 1, ptr);
    EXPECT_TRUE(result.empty());
}

/**
 * @tc.name  : ParseValues_ShouldReturnValues_WhenBufferIsValid
 * @tc.number: ParseValues_002
 * @tc.desc  : 当参数合法时，返回value值
 */
TEST_F(TestMemFragmentationSdkServer, ParseValues_ShouldReturnValues_WhenBufferIsValid)
{
    const uint8_t buffer[] = {0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 't', 'e', 's', 't', 0x00};
    const uint8_t* ptr = buffer;
    auto result = VirtMemFragSdk::ParseValues(buffer, sizeof(buffer), ptr);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "test");
}

/**
 * @tc.name  : ParseEntry_ShouldReturnEmpty_WhenKeyIsInvalid
 * @tc.number: ParseEntry_001
 * @tc.desc  : 当buffer_size不足时，返回为空
 */
TEST_F(TestMemFragmentationSdkServer, ParseEntry_ShouldReturnEmpty_WhenKeyIsInvalid)
{
    const uint8_t buffer[] = {0, 0, 0, 0};
    const uint8_t* ptr = buffer;
    auto result = VirtMemFragSdk::ParseEntry(buffer, sizeof(buffer) - 1, ptr);
    EXPECT_TRUE(result.first.empty());
    EXPECT_TRUE(result.second.empty());
}

/**
 * @tc.name  : ParseEntry_ShouldReturnEntry_WhenBufferIsValid
 * @tc.number: ParseEntry_002
 * @tc.desc  : 当参数合法时，返回key-value
 */
TEST_F(TestMemFragmentationSdkServer, ParseEntry_ShouldReturnEntry_WhenBufferIsValid)
{
    const uint8_t buffer[] = {0x05, 0x00, 0x00, 0x00, 't',  'e', 's', 't', 0x00, 0x01, 0x00, 0x00,
                              0x00, 0x06, 0x00, 0x00, 0x00, 'v', 'a', 'l', 'u',  'e',  0x00};
    const uint8_t* ptr = buffer;
    auto result = VirtMemFragSdk::ParseEntry(buffer, sizeof(buffer), ptr);
    EXPECT_EQ(result.first, "test");
    ASSERT_EQ(result.second.size(), 1);
    EXPECT_EQ(result.second[0], "value");
}

/**
 * @tc.name  : DeserializeNodeAntiDictionary_ShouldReturnError_WhenBufferIsNull
 * @tc.number: DeserializeNodeAntiDictionary_001
 * @tc.desc  : 当buffer为空时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, DeserializeNodeAntiDictionary_ShouldReturnError_WhenBufferIsNull)
{
    std::map<std::string, std::vector<std::string>> node_dict_map;
    EXPECT_EQ(VirtMemFragSdk::DeserializeNodeAntiDictionary(nullptr, 0, node_dict_map), VM_ERROR);
}

/**
 * @tc.name  : DeserializeNodeAntiDictionary_ShouldReturnError_WhenBufferIsInvalid
 * @tc.number: DeserializeNodeAntiDictionary_002
 * @tc.desc  : 当buffer_size不足时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, DeserializeNodeAntiDictionary_ShouldReturnError_WhenBufferIsInvalid)
{
    const uint8_t buffer[] = {0, 0, 0, 0};
    std::map<std::string, std::vector<std::string>> node_dict_map;
    MOCKER(UbseGetAllNodeInfos).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMemFragSdk::DeserializeNodeAntiDictionary(buffer, sizeof(buffer) - 1, node_dict_map), VM_ERROR);
    MOCKER(UbseGetAllNodeInfos).reset();
}

uint32_t MockUbseGetAllNodeInfos(std::vector<UbseRoleInfo>& roleInfos)
{
    UbseRoleInfo info{};
    info.nodeId = "node";
    roleInfos.push_back(info);
    return VM_OK;
}

/**
 * @tc.name  : DeserializeNodeAntiDictionary_ShouldReturnOk_WhenBufferIsValid
 * @tc.number: DeserializeNodeAntiDictionary_003
 * @tc.desc  : 当参数合法时，返回ok
 */
TEST_F(TestMemFragmentationSdkServer, DeserializeNodeAntiDictionary_ShouldReturnOk_WhenBufferIsValid)
{
    const uint8_t buffer[] = {0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 'n', 'o', 'd', 'e', 0x00, 0x01,
                              0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 'v',  'a', 'l', 'u', 'e', 0x00};
    std::map<std::string, std::vector<std::string>> node_dict_map;
    MOCKER(UbseGetAllNodeInfos).stubs().will(invoke(MockUbseGetAllNodeInfos));
    EXPECT_EQ(VirtMemFragSdk::DeserializeNodeAntiDictionary(buffer, sizeof(buffer), node_dict_map), VM_OK);
    ASSERT_EQ(node_dict_map.size(), 1);
    EXPECT_EQ(node_dict_map["node"][0], "value");
    MOCKER(UbseGetAllNodeInfos).reset();
}

/**
 * @tc.name  : NodeAntiAffinity_ShouldReturnError_WhenRequestIsInvalid
 * @tc.number: NodeAntiAffinity_001
 * @tc.desc  : 当入参为空时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, NodeAntiAffinity_ShouldReturnError_WhenRequestIsInvalid)
{
    UbseIpcMessage req;
    req.buffer = nullptr;
    req.length = 0;
    UbseRequestContext context;
    context.requestId = 1;
    EXPECT_EQ(VirtMemFragSdk::NodeAntiAffinity(req, context), VM_ERROR);
}

/**
 * @tc.name  : NodeAntiAffinity_ShouldReturnError_WhenDeserializationFails
 * @tc.number: NodeAntiAffinity_002
 * @tc.desc  : 当反序列化失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, NodeAntiAffinity_ShouldReturnError_WhenDeserializationFails)
{
    UbseIpcMessage req;
    req.buffer = new uint8_t[1];
    req.length = 1;
    UbseRequestContext context;
    context.requestId = 1;
    MOCKER(VirtMemFragSdk::DeserializeNodeAntiDictionary).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::NodeAntiAffinity(req, context), VM_ERROR);
    delete[] req.buffer;
    MOCKER(VirtMemFragSdk::DeserializeNodeAntiDictionary).reset();
}

/**
 * @tc.name  : NodeAntiAffinity_ShouldReturnError_WhenUpdateFails
 * @tc.number: NodeAntiAffinity_003
 * @tc.desc  : 当UpdateAntiAffinityConfig失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, NodeAntiAffinity_ShouldReturnError_WhenUpdateFails)
{
    UbseIpcMessage req;
    req.buffer = new uint8_t[1];
    req.length = 1;
    UbseRequestContext context;
    context.requestId = 1;
    MOCKER(VirtMemFragSdk::DeserializeNodeAntiDictionary).stubs().will(returnValue(VM_OK));
    MOCKER(VirtMemFragSdk::UpdateAntiAffinityConfig).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::NodeAntiAffinity(req, context), VM_ERROR);
    delete[] req.buffer;
    MOCKER(VirtMemFragSdk::DeserializeNodeAntiDictionary).reset();
    MOCKER(VirtMemFragSdk::UpdateAntiAffinityConfig).reset();
}

/**
 * @tc.name  : NodeAntiAffinity_ShouldReturnOk_WhenAllOperationsSucceed
 * @tc.number: NodeAntiAffinity_004
 * @tc.desc  : 当功能正常时，返回成功
 */
TEST_F(TestMemFragmentationSdkServer, NodeAntiAffinity_ShouldReturnOk_WhenAllOperationsSucceed)
{
    UbseIpcMessage req;
    req.buffer = new uint8_t[1];
    req.length = 1;
    UbseRequestContext context;
    context.requestId = 1;
    MOCKER(VirtMemFragSdk::DeserializeNodeAntiDictionary).stubs().will(returnValue(VM_OK));
    MOCKER(VirtMemFragSdk::UpdateAntiAffinityConfig).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMemFragSdk::NodeAntiAffinity(req, context), VM_OK);
    delete[] req.buffer;
    MOCKER(VirtMemFragSdk::DeserializeNodeAntiDictionary).reset();
    MOCKER(VirtMemFragSdk::UpdateAntiAffinityConfig).reset();
}

/**
 * @tc.name  : PackBorrowStrategyRsp_ShouldReturnOK_WhenParamIsOk
 * @tc.number: PackBorrowStrategyRsp
 * @tc.desc  : 当功能正常时，返回成功
 */
TEST_F(TestMemFragmentationSdkServer, PackBorrowStrategyRsp_ShouldReturnOK_WhenParamIsOk)
{
    MemBorrowStrategyResult borrowStrategyResult{};
    UbseIpcMessage buffer;
    borrowStrategyResult.srcParam.srcNid = "1";
    EXPECT_EQ(VirtMemFragSdk::PackBorrowStrategyRsp(borrowStrategyResult, buffer), VM_OK);
}

/**
 * @tc.name  : MemBorrowStrategy_ShouldReturnError_WhenReqIsNull
 * @tc.number: MemBorrowStrategy_001
 * @tc.desc  : 当入参为空时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemBorrowStrategy_ShouldReturnError_WhenReqIsNull)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(VirtMemFragSdk::MemBorrowStrategy(req, context), VM_ERROR);
}

/**
 * @tc.name  : MemBorrowStrategy_ShouldReturnError_WhenReqIsInvalid
 * @tc.number: MemBorrowStrategy_002
 * @tc.desc  : 当入参不合法时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemBorrowStrategy_ShouldReturnError_WhenReqIsInvalid)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    req.buffer = new uint8_t[1];
    req.length = 1;
    EXPECT_EQ(VirtMemFragSdk::MemBorrowStrategy(req, context), VM_ERROR_DESERIALIZE_ERROR);
}

/**
 * @tc.name  : MemBorrowStrategy_ShouldReturnError_WhenGetUBSRMRSMemBorrowStrategyFailed
 * @tc.number: MemBorrowStrategy_003
 * @tc.desc  : 当获取UBSRMRSMemBorrowStrategy失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemBorrowStrategy_ShouldReturnError_WhenGetUBSRMRSMemBorrowStrategyFailed)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    src_memory_borrow_param srcParam{.src_nid = "1", .src_socket_id = 1, .src_numa_id = 1, .borrow_size = 1};
    MemFragmentationMemBorrowStrategyInputMsg inputMsg{srcParam};
    auto ret = inputMsg.Serialize();
    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();
    ASSERT_EQ(ret, VM_OK);
    UBSRMRSMemBorrowStrategyFunc func = nullptr;
    MOCKER(MempoolingModule::UBSRMRSMemBorrowStrategy).stubs().will(returnValue(func));
    EXPECT_EQ(VirtMemFragSdk::MemBorrowStrategy(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowStrategy).reset();
}

UBSRMRSMemBorrowStrategyFunc MockUBSRMRSMemBorrowStrategyError()
{
    return [](const SrcMemoryBorrowParam&, const uint64_t&, MemBorrowStrategyResult&) {
        return VM_ERROR;
    };
}

UBSRMRSMemBorrowStrategyFunc MockUBSRMRSMemBorrowStrategyOK()
{
    return [](const SrcMemoryBorrowParam&, const uint64_t&, MemBorrowStrategyResult&) {
        return VM_OK;
    };
}

/**
 * @tc.name  : MemBorrowStrategy_ShouldReturnError_WhenUBSRMRSMemBorrowStrategyError
 * @tc.number: MemBorrowStrategy_004
 * @tc.desc  : 当UBSRMRSMemBorrowStrategy函数失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemBorrowStrategy_ShouldReturnError_WhenUBSRMRSMemBorrowStrategyError)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    src_memory_borrow_param srcParam{.src_nid = "1", .src_socket_id = 1, .src_numa_id = 1, .borrow_size = 1};
    MemFragmentationMemBorrowStrategyInputMsg inputMsg{srcParam};
    auto ret = inputMsg.Serialize();
    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();
    ASSERT_EQ(ret, VM_OK);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowStrategy).stubs().will(invoke(MockUBSRMRSMemBorrowStrategyError));
    EXPECT_EQ(VirtMemFragSdk::MemBorrowStrategy(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowStrategy).reset();
}

/**
 * @tc.name  : MemBorrowStrategy_ShouldReturnError_WhenPackBorrowStrategyRspError
 * @tc.number: MemBorrowStrategy_005
 * @tc.desc  : 当PackBorrowStrategyRsp失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemBorrowStrategy_ShouldReturnError_WhenPackBorrowStrategyRspError)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    src_memory_borrow_param srcParam{.src_nid = "1", .src_socket_id = 1, .src_numa_id = 1, .borrow_size = 1};
    MemFragmentationMemBorrowStrategyInputMsg inputMsg{srcParam};
    auto ret = inputMsg.Serialize();
    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();
    ASSERT_EQ(ret, VM_OK);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowStrategy).stubs().will(invoke(MockUBSRMRSMemBorrowStrategyOK));
    MOCKER(VirtMemFragSdk::PackBorrowStrategyRsp).stubs().will(returnValue(VM_ERROR_SERIALIZE_ERROR));
    EXPECT_EQ(VirtMemFragSdk::MemBorrowStrategy(req, context), VM_ERROR_SERIALIZE_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowStrategy).reset();
    MOCKER(VirtMemFragSdk::PackBorrowStrategyRsp).reset();
}

/**
 * @tc.name  : MemBorrowStrategy_ShouldReturnError_WhenSendResponseError
 * @tc.number: MemBorrowStrategy_006
 * @tc.desc  : 当SendResponse失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemBorrowStrategy_ShouldReturnError_WhenSendResponseError)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    src_memory_borrow_param srcParam{.src_nid = "1", .src_socket_id = 1, .src_numa_id = 1, .borrow_size = 1};
    MemFragmentationMemBorrowStrategyInputMsg inputMsg{srcParam};
    auto ret = inputMsg.Serialize();
    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();
    ASSERT_EQ(ret, VM_OK);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowStrategy).stubs().will(invoke(MockUBSRMRSMemBorrowStrategyOK));
    MOCKER(VirtMemFragSdk::PackBorrowStrategyRsp).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_ERROR_NOMEM));
    EXPECT_EQ(VirtMemFragSdk::MemBorrowStrategy(req, context), VM_ERROR_NOMEM);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowStrategy).reset();
    MOCKER(VirtMemFragSdk::PackBorrowStrategyRsp).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : MemBorrowStrategy_ShouldReturnOK_WhenEverythingIsOk
 * @tc.number: MemBorrowStrategy_007
 * @tc.desc  : 当一切功能正常时，返回OK
 */
TEST_F(TestMemFragmentationSdkServer, MemBorrowStrategy_ShouldReturnOK_WhenEverythingIsOk)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    src_memory_borrow_param srcParam{.src_nid = "1", .src_socket_id = 1, .src_numa_id = 1, .borrow_size = 1};
    MemFragmentationMemBorrowStrategyInputMsg inputMsg{srcParam};
    auto ret = inputMsg.Serialize();
    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();
    ASSERT_EQ(ret, VM_OK);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowStrategy).stubs().will(invoke(MockUBSRMRSMemBorrowStrategyOK));
    MOCKER(VirtMemFragSdk::PackBorrowStrategyRsp).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMemFragSdk::MemBorrowStrategy(req, context), VM_OK);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowStrategy).reset();
    MOCKER(VirtMemFragSdk::PackBorrowStrategyRsp).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : SetSrcNodeHugePage_ShouldReturnError_WhenResultIsEmpty
 * @tc.number: SetSrcNodeHugePage_001
 * @tc.desc  : 当入参为空时，返回失败
 */
TEST_F(TestMemFragmentationSdkServer, SetSrcNodeHugePage_ShouldReturnError_WhenResultIsEmpty)
{
    MemBorrowExecuteResult result{};
    EXPECT_EQ(VirtMemFragSdk::SetSrcNodeHugePage(result), VM_ERROR);
}

/**
 * @tc.name  : SetSrcNodeHugePage_ShouldReturnError_WhenGetBorrowedSizeMapFailed
 * @tc.number: SetSrcNodeHugePage_002
 * @tc.desc  : 当GetBorrowedSizeMap失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, SetSrcNodeHugePage_ShouldReturnError_WhenGetBorrowedSizeMapFailed)
{
    MemBorrowExecuteResult result{};
    result.presentNumaIds.push_back(1);
    MOCKER(MemHandler::GetBorrowedSizeMap).stubs().will(returnValue(VM_ERROR_NOMEM));
    EXPECT_EQ(VirtMemFragSdk::SetSrcNodeHugePage(result), VM_ERROR_NOMEM);
    MOCKER(MemHandler::GetBorrowedSizeMap).reset();
}

VmResult MockGetBorrowedSizeMap(const std::vector<uint16_t>& remoteNumaIds,
                                std::map<uint16_t, uint64_t>& numaBorrowedSizeMap)
{
    uint16_t numaId = 0;
    uint64_t borrowedSize = (::vm::NO_1024) * (::vm::NO_1024);
    numaBorrowedSizeMap.emplace(std::make_pair(numaId, borrowedSize));
    return VM_OK;
}

/**
 * @tc.name  : SetSrcNodeHugePage_ShouldReturnError_WhenSetHugePagesFailed
 * @tc.number: SetSrcNodeHugePage_003
 * @tc.desc  : 当SetHugePages失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, SetSrcNodeHugePage_ShouldReturnError_WhenSetHugePagesFailed)
{
    MemBorrowExecuteResult result{};
    result.presentNumaIds.push_back(1);
    MOCKER(ChangeOverrideCapability).stubs().will(returnValue(VM_OK));
    MOCKER(MemHandler::GetBorrowedSizeMap).stubs().will(invoke(MockGetBorrowedSizeMap));
    MOCKER(HugePageHandler::SetHugePages).stubs().will(returnValue(VM_ERROR_NOMEM));
    EXPECT_EQ(VirtMemFragSdk::SetSrcNodeHugePage(result), VM_ERROR_NOMEM);
    MOCKER(MemHandler::GetBorrowedSizeMap).reset();
    MOCKER(HugePageHandler::SetHugePages).reset();
}

/**
 * @tc.name  : SetSrcNodeHugePage_ShouldReturnOK_WhenEverythingIsOk
 * @tc.number: SetSrcNodeHugePage_004
 * @tc.desc  : 当功能正常时，返回OK
 */
TEST_F(TestMemFragmentationSdkServer, SetSrcNodeHugePage_ShouldReturnOK_WhenEverythingIsOk)
{
    MemBorrowExecuteResult result{};
    result.presentNumaIds.push_back(1);
    MOCKER(ChangeOverrideCapability).stubs().will(returnValue(VM_OK));
    MOCKER(MemHandler::GetBorrowedSizeMap).stubs().will(invoke(MockGetBorrowedSizeMap));
    MOCKER(HugePageHandler::SetHugePages).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMemFragSdk::SetSrcNodeHugePage(result), VM_OK);
    MOCKER(MemHandler::GetBorrowedSizeMap).reset();
    MOCKER(HugePageHandler::SetHugePages).reset();
}

/**
 * @tc.name  : MemBorrowExecute_ShouldReturnError_WhenReqIsNull
 * @tc.number: MemBorrowExecute_001
 * @tc.desc  : 当入参为空时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemBorrowExecute_ShouldReturnError_WhenReqIsNull)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(VirtMemFragSdk::MemBorrowExecute(req, context), VM_ERROR);
}

borrow_strategy_c globalBorrowStrategy{
    .src_host_id = "1",
    .src_socket_id = 1,
    .src_numa_id = 1,
    .borrow_size = 0,
    .dest_numa_infos = {{.host_id = "2", .socket_id = 1, .numa_nums = 1, .numa_ids = {1}, .mem_sizes = {0}}},
    .dest_numa_infos_size = 1};

/**
 * @tc.name  : MemBorrowExecute_ShouldReturnError_WhenGetUBSRMRSMemBorrowExecuteFailed
 * @tc.number: MemBorrowExecute_002
 * @tc.desc  : 当获取UBSRMRSMemBorrowExecute接口失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemBorrowExecute_ShouldReturnError_WhenGetUBSRMRSMemBorrowExecuteFailed)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    MemFragmentationMemBorrowStrategyOutputMsg msg(ubse::ut::vm::globalBorrowStrategy);
    ASSERT_EQ(msg.Serialize(), VM_OK);
    req.buffer = msg.SerializedData();
    req.length = msg.SerializedDataSize();
    UBSRMRSMemBorrowExecuteFunc func = nullptr;
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).stubs().will(returnValue(func));
    EXPECT_EQ(VirtMemFragSdk::MemBorrowExecute(req, context), VM_ERROR_DESERIALIZE_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).reset();
}

UBSRMRSMemBorrowExecuteFunc MockUBSRMRSMemBorrowExecuteError()
{
    return [](const SrcMemoryBorrowParam&, const std::vector<DestMemoryBorrowParam>&, MemBorrowExecuteResult&) {
        return VM_ERROR;
    };
}

UBSRMRSMemBorrowExecuteFunc MockUBSRMRSMemBorrowExecuteOK()
{
    return [](const SrcMemoryBorrowParam&, const std::vector<DestMemoryBorrowParam>&, MemBorrowExecuteResult&) {
        return VM_OK;
    };
}

/**
 * @tc.name  : MemBorrowExecute_ShouldReturnError_WhenUBSRMRSMemBorrowExecuteFailed
 * @tc.number: MemBorrowExecute_003
 * @tc.desc  : 当UBSRMRSMemBorrowExecute失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemBorrowExecute_ShouldReturnError_WhenUBSRMRSMemBorrowExecuteFailed)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    MemFragmentationMemBorrowStrategyOutputMsg msg(ubse::ut::vm::globalBorrowStrategy);
    ASSERT_EQ(msg.Serialize(), VM_OK);
    req.buffer = msg.SerializedData();
    req.length = msg.SerializedDataSize();
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).stubs().will(invoke(MockUBSRMRSMemBorrowExecuteError));
    EXPECT_EQ(VirtMemFragSdk::MemBorrowExecute(req, context), VM_ERROR_DESERIALIZE_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).reset();
}

/**
 * @tc.name  : MemBorrowExecute_ShouldReturnError_WhenSetSrcNodeHugePageFailed
 * @tc.number: MemBorrowExecute_004
 * @tc.desc  : 当设置大页失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemBorrowExecute_ShouldReturnError_WhenSetSrcNodeHugePageFailed)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    MemFragmentationMemBorrowStrategyOutputMsg msg(ubse::ut::vm::globalBorrowStrategy);
    ASSERT_EQ(msg.Serialize(), VM_OK);
    req.buffer = msg.SerializedData();
    req.length = msg.SerializedDataSize();
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).stubs().will(invoke(MockUBSRMRSMemBorrowExecuteOK));
    MOCKER(VirtMemFragSdk::SetSrcNodeHugePage).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::MemBorrowExecute(req, context), VM_ERROR_DESERIALIZE_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).reset();
    MOCKER(VirtMemFragSdk::SetSrcNodeHugePage).reset();
}

/**
 * @tc.name  : MemBorrowExecute_ShouldReturnError_WhenPackBorrowExecuteRspFailed
 * @tc.number: MemBorrowExecute_005
 * @tc.desc  : 当打包失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemBorrowExecute_ShouldReturnError_WhenPackBorrowExecuteRspFailed)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    MemFragmentationMemBorrowStrategyOutputMsg msg(ubse::ut::vm::globalBorrowStrategy);
    ASSERT_EQ(msg.Serialize(), VM_OK);
    req.buffer = msg.SerializedData();
    req.length = msg.SerializedDataSize();
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).stubs().will(invoke(MockUBSRMRSMemBorrowExecuteOK));
    MOCKER(VirtMemFragSdk::SetSrcNodeHugePage).stubs().will(returnValue(VM_OK));
    MOCKER(VirtMemFragSdk::PackBorrowExecuteRsp).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::MemBorrowExecute(req, context), VM_ERROR_DESERIALIZE_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).reset();
    MOCKER(VirtMemFragSdk::SetSrcNodeHugePage).reset();
    MOCKER(VirtMemFragSdk::PackBorrowExecuteRsp).reset();
}

/**
 * @tc.name  : MemBorrowExecute_ShouldReturnError_WhenSendResponseFailed
 * @tc.number: MemBorrowExecute_006
 * @tc.desc  : 当发送回信时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemBorrowExecute_ShouldReturnError_WhenSendResponseFailed)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    MemFragmentationMemBorrowStrategyOutputMsg msg(ubse::ut::vm::globalBorrowStrategy);
    ASSERT_EQ(msg.Serialize(), VM_OK);
    req.buffer = msg.SerializedData();
    req.length = msg.SerializedDataSize();
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).stubs().will(invoke(MockUBSRMRSMemBorrowExecuteOK));
    MOCKER(VirtMemFragSdk::SetSrcNodeHugePage).stubs().will(returnValue(VM_OK));
    MOCKER(VirtMemFragSdk::PackBorrowExecuteRsp).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_ERROR_BEGIN_USER));
    EXPECT_EQ(VirtMemFragSdk::MemBorrowExecute(req, context), VM_ERROR_DESERIALIZE_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).reset();
    MOCKER(VirtMemFragSdk::SetSrcNodeHugePage).reset();
    MOCKER(VirtMemFragSdk::PackBorrowExecuteRsp).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : MemBorrowExecute_ShouldReturnOK_WhenEverythingIsOk
 * @tc.number: MemBorrowExecute_007
 * @tc.desc  : 当功能正常时，返回OK
 */
TEST_F(TestMemFragmentationSdkServer, MemBorrowExecute_ShouldReturnOK_WhenEverythingError)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    MemFragmentationMemBorrowStrategyOutputMsg msg(ubse::ut::vm::globalBorrowStrategy);
    ASSERT_EQ(msg.Serialize(), VM_OK);
    req.buffer = msg.SerializedData();
    req.length = msg.SerializedDataSize();
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).stubs().will(invoke(MockUBSRMRSMemBorrowExecuteOK));
    MOCKER(VirtMemFragSdk::SetSrcNodeHugePage).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMemFragSdk::MemBorrowExecute(req, context), VM_ERROR_DESERIALIZE_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).reset();
    MOCKER(VirtMemFragSdk::SetSrcNodeHugePage).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : MemMigrateStrategy_ShouldReturnError_WhenReqIsNull
 * @tc.number: MemMigrateStrategy_001
 * @tc.desc  : 当入参为空，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemMigrateStrategy_ShouldReturnError_WhenReqIsNull)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(VirtMemFragSdk::MemMigrateStrategy(req, context), VM_ERROR);
}

MemMigrateStrategySrcParam globalMemMigrateStrategySrcParam{
    .borrowSize = 1,
    .borrowInNode = "node1",
    .vmInfoList = {{.pid = 0, .ratio = 1}, {.pid = 1, .ratio = 1}},
    .vmInfoListSize = 2};

/**
 * @tc.name  : MemMigrateStrategy_ShouldReturnError_WhenGetUBSRMRSMigrateStrategyFailed
 * @tc.number: MemMigrateStrategy_002
 * @tc.desc  : 当获取UBSRMRSMemMigrateStrategy时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemMigrateStrategy_ShouldReturnError_WhenGetUBSRMRSMigrateStrategyFailed)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    MemFragmentationMemMigrateStrategyInputMsg inputMsg{};
    ASSERT_EQ(inputMsg.SetInputMsg(globalMemMigrateStrategySrcParam), VM_OK);
    ASSERT_EQ(inputMsg.Serialize(), VM_OK);
    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();
    UBSRMRSMigrateStrategyFunc func = nullptr;
    MOCKER(MempoolingModule::UBSRMRSMigrateStrategy).stubs().will(returnValue(func));
    EXPECT_EQ(VirtMemFragSdk::MemMigrateStrategy(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMigrateStrategy).reset();
}

UBSRMRSMigrateStrategyFunc MockUBSRMRSMigrateStrategyError()
{
    return [](const std::string&, const std::vector<VMPresetParam>&, const uint64_t&, MigrateStrategyResult&) {
        return VM_ERROR;
    };
}

UBSRMRSMigrateStrategyFunc MockUBSRMRSMigrateStrategyOK()
{
    return [](const std::string&, const std::vector<VMPresetParam>&, const uint64_t&, MigrateStrategyResult&) {
        return VM_OK;
    };
}

/**
 * @tc.name  : MemMigrateStrategy_ShouldReturnError_WhenUBSRMRSMigrateStrategyFailed
 * @tc.number: MemMigrateStrategy_003
 * @tc.desc  : 当迁移接口执行失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemMigrateStrategy_ShouldReturnError_WhenUBSRMRSMigrateStrategyFailed)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    MemFragmentationMemMigrateStrategyInputMsg inputMsg{};
    ASSERT_EQ(inputMsg.SetInputMsg(globalMemMigrateStrategySrcParam), VM_OK);
    ASSERT_EQ(inputMsg.Serialize(), VM_OK);
    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();
    MOCKER(MempoolingModule::UBSRMRSMigrateStrategy).stubs().will(invoke(MockUBSRMRSMigrateStrategyError));
    EXPECT_EQ(VirtMemFragSdk::MemMigrateStrategy(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMigrateStrategy).reset();
}

/**
 * @tc.name  : MemMigrateStrategy_ShouldReturnError_WhenPackMigrateStrategyRspFailed
 * @tc.number: MemMigrateStrategy_004
 * @tc.desc  : 当打包失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemMigrateStrategy_ShouldReturnError_WhenPackMigrateStrategyRspFailed)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    MemFragmentationMemMigrateStrategyInputMsg inputMsg{};
    ASSERT_EQ(inputMsg.SetInputMsg(globalMemMigrateStrategySrcParam), VM_OK);
    ASSERT_EQ(inputMsg.Serialize(), VM_OK);
    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();
    MOCKER(MempoolingModule::UBSRMRSMigrateStrategy).stubs().will(invoke(MockUBSRMRSMigrateStrategyOK));
    MOCKER(VirtMemFragSdk::PackMigrateStrategyRsp).stubs().will(returnValue(VM_ERROR_SERIALIZE_ERROR));
    EXPECT_EQ(VirtMemFragSdk::MemMigrateStrategy(req, context), VM_ERROR_SERIALIZE_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMigrateStrategy).reset();
    MOCKER(VirtMemFragSdk::PackMigrateStrategyRsp).reset();
}

/**
 * @tc.name  : MemMigrateStrategy_ShouldReturnError_WhenSendResponseFailed
 * @tc.number: MemMigrateStrategy_005
 * @tc.desc  : 当发送回信失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemMigrateStrategy_ShouldReturnError_WhenSendResponseFailed)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    MemFragmentationMemMigrateStrategyInputMsg inputMsg{};
    ASSERT_EQ(inputMsg.SetInputMsg(globalMemMigrateStrategySrcParam), VM_OK);
    ASSERT_EQ(inputMsg.Serialize(), VM_OK);
    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();
    MOCKER(MempoolingModule::UBSRMRSMigrateStrategy).stubs().will(invoke(MockUBSRMRSMigrateStrategyOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_ERROR_BEGIN_USER));
    EXPECT_EQ(VirtMemFragSdk::MemMigrateStrategy(req, context), VM_ERROR_BEGIN_USER);
    MOCKER(MempoolingModule::UBSRMRSMigrateStrategy).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : MemMigrateStrategy_ShouldReturnOK_WhenEverythingIsOk
 * @tc.number: MemMigrateStrategy_006
 * @tc.desc  : 当功能正常时，返回OK
 */
TEST_F(TestMemFragmentationSdkServer, MemMigrateStrategy_ShouldReturnOK_WhenEverythingIsOk)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    MemFragmentationMemMigrateStrategyInputMsg inputMsg{};
    ASSERT_EQ(inputMsg.SetInputMsg(globalMemMigrateStrategySrcParam), VM_OK);
    ASSERT_EQ(inputMsg.Serialize(), VM_OK);
    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();
    MOCKER(MempoolingModule::UBSRMRSMigrateStrategy).stubs().will(invoke(MockUBSRMRSMigrateStrategyOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMemFragSdk::MemMigrateStrategy(req, context), VM_OK);
    MOCKER(MempoolingModule::UBSRMRSMigrateStrategy).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : MemMigrateExecute_ShouldReturnError_WhenReqIsNull
 * @tc.number: MemMigrateExecute_001
 * @tc.desc  : 当入参为空时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemMigrateExecute_ShouldReturnError_WhenReqIsNull)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(VirtMemFragSdk::MemMigrateExecute(req, context), VM_ERROR);
}

MemMigrateExecuteSrcParam globalMemMigrateExecuteSrcParam{
    .borrowInNode = "node1",
    .borrowIds = {"id1", "id2"},
    .vmInfoList = {{.destNumaId = 0, .memSize = 1, .pid = 0}, {.destNumaId = 0, .memSize = 1, .pid = 1}},
    .vmInfoListSize = 2,
    .waitingTime = ::vm::NO_1000};

/**
 * @tc.name  : MemMigrateExecute_ShouldReturnError_WhenGetUBSRMRSMigrateExecuteFailed
 * @tc.number: MemMigrateExecute_002
 * @tc.desc  : 当获取接口失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemMigrateExecute_ShouldReturnError_WhenGetUBSRMRSMigrateExecuteFailed)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    MemFragmentationMemMigrateExecuteInputMsg inputMsg{};
    ASSERT_EQ(inputMsg.SetInputMsg(globalMemMigrateExecuteSrcParam), VM_OK);
    ASSERT_EQ(inputMsg.Serialize(), VM_OK);
    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();
    UBSRMRSMigrateExecuteFunc func = nullptr;
    MOCKER(MempoolingModule::UBSRMRSMigrateExecute).stubs().will(returnValue(func));
    EXPECT_EQ(VirtMemFragSdk::MemMigrateExecute(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMigrateExecute).reset();
}

UBSRMRSMigrateExecuteFunc MockUBSRMRSMigrateExecuteError()
{
    return [](const std::string&, const std::vector<VMMigrateOutParam>&, const std::uint64_t&,
              const std::vector<std::string>&) {
        return VM_ERROR;
    };
}

UBSRMRSMigrateExecuteFunc MockUBSRMRSMigrateExecuteOK()
{
    return [](const std::string&, const std::vector<VMMigrateOutParam>&, const std::uint64_t&,
              const std::vector<std::string>&) {
        return VM_OK;
    };
}

/**
 * @tc.name  : MemMigrateExecute_ShouldReturnError_WhenUBSRMRSMigrateExecuteFailed
 * @tc.number: MemMigrateExecute_003
 * @tc.desc  : 当接口执行失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemMigrateExecute_ShouldReturnError_WhenUBSRMRSMigrateExecuteFailed)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    MemFragmentationMemMigrateExecuteInputMsg inputMsg{};
    ASSERT_EQ(inputMsg.SetInputMsg(globalMemMigrateExecuteSrcParam), VM_OK);
    ASSERT_EQ(inputMsg.Serialize(), VM_OK);
    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();
    MOCKER(MempoolingModule::UBSRMRSMigrateExecute).stubs().will(invoke(MockUBSRMRSMigrateExecuteError));
    EXPECT_EQ(VirtMemFragSdk::MemMigrateExecute(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMigrateExecute).reset();
}

/**
 * @tc.name  : MemMigrateExecute_ShouldReturnError_WhenSendResponseFailed
 * @tc.number: MemMigrateExecute_004
 * @tc.desc  : 当发送信息失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemMigrateExecute_ShouldReturnError_WhenSendResponseFailed)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    MemFragmentationMemMigrateExecuteInputMsg inputMsg{};
    ASSERT_EQ(inputMsg.SetInputMsg(globalMemMigrateExecuteSrcParam), VM_OK);
    ASSERT_EQ(inputMsg.Serialize(), VM_OK);
    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();
    MOCKER(MempoolingModule::UBSRMRSMigrateExecute).stubs().will(invoke(MockUBSRMRSMigrateExecuteOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_ERROR_BEGIN_USER));
    EXPECT_EQ(VirtMemFragSdk::MemMigrateExecute(req, context), VM_ERROR_BEGIN_USER);
    MOCKER(MempoolingModule::UBSRMRSMigrateExecute).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : MemMigrateExecute_ShouldReturnOK_WhenEverythingIsOK
 * @tc.number: MemMigrateExecute_005
 * @tc.desc  : 当一切正常时，返回OK
 */
TEST_F(TestMemFragmentationSdkServer, MemMigrateExecute_ShouldReturnOK_WhenEverythingIsOK)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    MemFragmentationMemMigrateExecuteInputMsg inputMsg{};
    ASSERT_EQ(inputMsg.SetInputMsg(globalMemMigrateExecuteSrcParam), VM_OK);
    ASSERT_EQ(inputMsg.Serialize(), VM_OK);
    req.buffer = inputMsg.SerializedData();
    req.length = inputMsg.SerializedDataSize();
    MOCKER(MempoolingModule::UBSRMRSMigrateExecute).stubs().will(invoke(MockUBSRMRSMigrateExecuteOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMemFragSdk::MemMigrateExecute(req, context), VM_OK);
    MOCKER(MempoolingModule::UBSRMRSMigrateExecute).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : MemReturn_ShouldReturnError_WhenReqIsNull
 * @tc.number: MemReturn_001
 * @tc.desc  : 当入参为空时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemReturn_ShouldReturnError_WhenReqIsNull)
{
    UbseIpcMessage req{};
    UbseRequestContext context{};
    EXPECT_EQ(VirtMemFragSdk::MemReturn(req, context), VM_ERROR);
}

/**
 * @tc.name  : MemReturn_ShouldReturnError_WhenNodeIdIsEmpty
 * @tc.number: MemReturn_002
 * @tc.desc  : 当NodeId为空时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemReturn_ShouldReturnError_WhenNodeIdIsEmpty)
{
    std::string nodeId = "";
    UbseIpcMessage req{};
    UbseRequestContext context{};
    std::vector<uint8_t> buffer(nodeId.begin(), nodeId.end());
    req.buffer = buffer.data();
    req.length = buffer.size();
    EXPECT_EQ(VirtMemFragSdk::MemReturn(req, context), VM_ERROR);
}

UBSRMRSMemFreeFunc MockUBSRMRSMemFreeError()
{
    return [](const std::string&) {
        return VM_ERROR;
    };
}

UBSRMRSMemFreeFunc MockUBSRMRSMemFreeOK()
{
    return [](const std::string&) {
        return VM_OK;
    };
}

/**
 * @tc.name  : MemReturn_ShouldReturnError_WhenSendResponseError
 * @tc.number: MemReturn_005
 * @tc.desc  : 当发送信息失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemReturn_ShouldReturnError_WhenSendResponseError)
{
    std::string nodeId = "1";
    UbseIpcMessage req{};
    UbseRequestContext context{};
    std::vector<uint8_t> buffer(nodeId.begin(), nodeId.end());
    req.buffer = buffer.data();
    req.length = buffer.size();
    MOCKER(MempoolingModule::UBSRMRSMemFree).stubs().will(invoke(MockUBSRMRSMemFreeOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::MemReturn(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemFree).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : MemReturn_ShouldReturnOK_WhenEverythingIsOk
 * @tc.number: MemReturn_006
 * @tc.desc  : 当一切功能正常时，返回OK
 */
TEST_F(TestMemFragmentationSdkServer, MemReturn_ShouldReturnOK_WhenEverythingOK)
{
    std::string nodeId = "1";
    UbseIpcMessage req{};
    UbseRequestContext context{};
    std::vector<uint8_t> buffer(nodeId.begin(), nodeId.end());
    req.buffer = buffer.data();
    req.length = buffer.size();
    MOCKER(MempoolingModule::UBSRMRSMemFree).stubs().will(invoke(MockUBSRMRSMemFreeOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMemFragSdk::MemReturn(req, context), VM_OK);
    MOCKER(MempoolingModule::UBSRMRSMemFree).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : MemRollback_ShouldReturnError_WhenGetUBSRMRSMemBorrowRollbackFailed
 * @tc.number: MemRollback_001
 * @tc.desc  : 当入参为空时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemRollback_ShouldReturnError_WhenGetUBSRMRSMemBorrowRollbackFailed)
{
    std::vector<std::string> borrowIdList{"0", "1"};
    RollbackParams rollbackParams{};
    rollbackParams.node_id = "1";
    rollbackParams.borrow_id_list = borrowIdList;
    rollbackParams.borrow_id_size = ::vm::NO_2;
    MemRollbackMsg rollbackMsg(rollbackParams);
    ASSERT_EQ(rollbackMsg.Serialize(), VM_OK);
    UbseIpcMessage req{rollbackMsg.SerializedData(), rollbackMsg.SerializedDataSize()};
    UbseRequestContext context{};

    UBSRMRSMemBorrowRollbackFunc func = nullptr;
    MOCKER(MempoolingModule::UBSRMRSMemBorrowRollback).stubs().will(returnValue(func));
    EXPECT_EQ(VirtMemFragSdk::MemRollback(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowRollback).reset();
}

UBSRMRSMemBorrowRollbackFunc MockUBSRMRSMemBorrowRollbackError()
{
    return [](const std::string&, const std::vector<std::string>&) {
        return VM_ERROR;
    };
}

UBSRMRSMemBorrowRollbackFunc MockUBSRMRSMemBorrowRollbackOK()
{
    return [](const std::string&, const std::vector<std::string>&) {
        return VM_OK;
    };
}

/**
 * @tc.name  : MemRollback_ShouldReturnError_WhenUBSRMRSMemBorrowRollbackFailed
 * @tc.number: MemRollback_002
 * @tc.desc  : 当执行接口失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemRollback_ShouldReturnError_WhenUBSRMRSMemBorrowRollbackFailed)
{
    std::vector<std::string> borrowIdList{"0", "1"};
    RollbackParams rollbackParams{};
    rollbackParams.node_id = "1";
    rollbackParams.borrow_id_list = borrowIdList;
    rollbackParams.borrow_id_size = ::vm::NO_2;
    MemRollbackMsg rollbackMsg(rollbackParams);
    ASSERT_EQ(rollbackMsg.Serialize(), VM_OK);
    UbseIpcMessage req{rollbackMsg.SerializedData(), rollbackMsg.SerializedDataSize()};
    UbseRequestContext context{};

    MOCKER(MempoolingModule::UBSRMRSMemBorrowRollback).stubs().will(invoke(MockUBSRMRSMemBorrowRollbackError));
    EXPECT_EQ(VirtMemFragSdk::MemRollback(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowRollback).reset();
}

/**
 * @tc.name  : MemRollback_ShouldReturnError_WhenSendResponseFailed
 * @tc.number: MemRollback_003
 * @tc.desc  : 当发送信息失败时，返回error
 */
TEST_F(TestMemFragmentationSdkServer, MemRollback_ShouldReturnError_WhenSendResponseFailed)
{
    std::vector<std::string> borrowIdList{"0", "1"};
    RollbackParams rollbackParams{};
    rollbackParams.node_id = "1";
    rollbackParams.borrow_id_list = borrowIdList;
    rollbackParams.borrow_id_size = ::vm::NO_2;
    MemRollbackMsg rollbackMsg(rollbackParams);
    ASSERT_EQ(rollbackMsg.Serialize(), VM_OK);
    UbseIpcMessage req{rollbackMsg.SerializedData(), rollbackMsg.SerializedDataSize()};
    UbseRequestContext context{};

    MOCKER(MempoolingModule::UBSRMRSMemBorrowRollback).stubs().will(invoke(MockUBSRMRSMemBorrowRollbackOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::MemRollback(req, context), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowRollback).reset();
    MOCKER(SendResponse).reset();
}

/**
 * @tc.name  : MemRollback_ShouldReturnOK_WhenEverythingIsOk
 * @tc.number: MemRollback_004
 * @tc.desc  : 当一切功能正常时，返回OK
 */
TEST_F(TestMemFragmentationSdkServer, MemRollback_ShouldReturnOK_WhenEverythingIsOk)
{
    std::vector<std::string> borrowIdList{"0", "1"};
    RollbackParams rollbackParams{};
    rollbackParams.node_id = "1";
    rollbackParams.borrow_id_list = borrowIdList;
    rollbackParams.borrow_id_size = ::vm::NO_2;
    MemRollbackMsg rollbackMsg(rollbackParams);
    ASSERT_EQ(rollbackMsg.Serialize(), VM_OK);
    UbseIpcMessage req{rollbackMsg.SerializedData(), rollbackMsg.SerializedDataSize()};
    UbseRequestContext context{};

    MOCKER(MempoolingModule::UBSRMRSMemBorrowRollback).stubs().will(invoke(MockUBSRMRSMemBorrowRollbackOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMemFragSdk::MemRollback(req, context), VM_OK);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowRollback).reset();
    MOCKER(SendResponse).reset();
}

TEST_F(TestMemFragmentationSdkServer, StartMemBorrowAsync_Success)
{
    borrow_setting_c borrowSetting{};
    strcpy_s(borrowSetting.borrow_strategy.src_host_id, VIRT_MEM_MAX_NODE_ID_LENGTH, "test_host");
    borrowSetting.borrow_strategy.src_socket_id = 0;
    borrowSetting.borrow_strategy.src_numa_id = 0;
    borrowSetting.borrow_strategy.borrow_size = 1024 * 1024; // 1MB

    borrowSetting.borrow_strategy.dest_numa_infos_size = 1;
    strcpy_s(borrowSetting.borrow_strategy.dest_numa_infos[0].host_id, VIRT_MEM_MAX_NODE_ID_LENGTH, "dest_host");
    borrowSetting.borrow_strategy.dest_numa_infos[0].socket_id = 0;
    borrowSetting.borrow_strategy.dest_numa_infos[0].numa_nums = 1;
    borrowSetting.borrow_strategy.dest_numa_infos[0].numa_ids[0] = 10;
    borrowSetting.borrow_strategy.dest_numa_infos[0].mem_sizes[0] = 1024 * 1024;

    borrowSetting.isAsync = true;

    MemBorrowSettingMsg msg(borrowSetting);
    ASSERT_EQ(msg.Serialize(), VM_OK);

    UbseIpcMessage req{msg.SerializedData(), msg.SerializedDataSize()};
    UbseRequestContext context{};

    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).stubs().will(invoke(MockUBSRMRSMemBorrowExecuteOK));
    MOCKER(VirtMemFragSdk::SetSrcNodeHugePage).stubs().will(returnValue(VM_OK));
    uint32_t ret = VirtMemFragSdk::MemBorrowExecute(req, context);
    EXPECT_EQ(ret, VM_OK);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).reset();
    MOCKER(VirtMemFragSdk::SetSrcNodeHugePage).reset();
    MOCKER(SendResponse).reset();
    GlobalMockObject::verify();
}

TEST_F(TestMemFragmentationSdkServer, StartMemBorrowsync_Success)
{
    borrow_setting_c borrowSetting{};
    strcpy_s(borrowSetting.borrow_strategy.src_host_id, VIRT_MEM_MAX_NODE_ID_LENGTH, "test_host");
    borrowSetting.borrow_strategy.src_socket_id = 0;
    borrowSetting.borrow_strategy.src_numa_id = 0;
    borrowSetting.borrow_strategy.borrow_size = 1024 * 1024; // 1MB

    borrowSetting.borrow_strategy.dest_numa_infos_size = 1;
    strcpy_s(borrowSetting.borrow_strategy.dest_numa_infos[0].host_id, VIRT_MEM_MAX_NODE_ID_LENGTH, "dest_host");
    borrowSetting.borrow_strategy.dest_numa_infos[0].socket_id = 0;
    borrowSetting.borrow_strategy.dest_numa_infos[0].numa_nums = 1;
    borrowSetting.borrow_strategy.dest_numa_infos[0].numa_ids[0] = 10;
    borrowSetting.borrow_strategy.dest_numa_infos[0].mem_sizes[0] = 1024 * 1024;

    borrowSetting.isAsync = false;

    MemBorrowSettingMsg msg(borrowSetting);
    ASSERT_EQ(msg.Serialize(), VM_OK);

    UbseIpcMessage req{msg.SerializedData(), msg.SerializedDataSize()};
    UbseRequestContext context{};

    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).stubs().will(invoke(MockUBSRMRSMemBorrowExecuteOK));
    MOCKER(VirtMemFragSdk::SetSrcNodeHugePage).stubs().will(returnValue(VM_OK));
    uint32_t ret = VirtMemFragSdk::MemBorrowExecute(req, context);
    EXPECT_EQ(ret, VM_OK);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).reset();
    MOCKER(VirtMemFragSdk::SetSrcNodeHugePage).reset();
    MOCKER(SendResponse).reset();
    GlobalMockObject::verify();
}

TEST_F(TestMemFragmentationSdkServer, TestVirtMemFragSdk_PackBorrowExecuteRsp)
{
    MemBorrowExecuteResult result;
    result.borrowIds = {"borrow1", "borrow2"};
    result.presentNumaIds = {0, 1, 2};
    UbseIpcMessage buffer;

    auto ret = VirtMemFragSdk::PackBorrowExecuteRsp(result, buffer);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestMemFragmentationSdkServer, MemTaskQuery_ShouldReturnOK_WhenTaskExists)
{
    // 准备测试数据
    std::string taskId = "test_task_123";
    async_task_info_c taskInfoC{};
    taskInfoC.task_id[0] = '\0';
    strcpy_s(taskInfoC.task_id, MEM_TASK_ID_MAX, taskId.c_str());
    taskInfoC.status = ASYNC_TASK_SUCCESS;
    taskInfoC.resultCode = 0;
    taskInfoC.memBorrowResult.borrow_ids_size = 2;
    taskInfoC.memBorrowResult.present_numa_ids_size = 1;
    strcpy_s(taskInfoC.memBorrowResult.borrow_ids_ptr[0], MAX_BORROW_ID_LENGTH, "id1");
    strcpy_s(taskInfoC.memBorrowResult.borrow_ids_ptr[1], MAX_BORROW_ID_LENGTH, "id2");
    taskInfoC.memBorrowResult.present_numa_ids_ptr[0] = 100;
    strcpy_s(taskInfoC.memBorrowResult.task_id, MEM_TASK_ID_MAX, "test_task_123");

    // 模拟序列化
    MemTaskResultQueryMsg msg(taskInfoC);
    ASSERT_EQ(msg.Serialize(), VM_OK);

    UbseIpcMessage req{const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(taskId.c_str())),
                       static_cast<uint32_t>(taskId.size())};
    UbseRequestContext context{};
    context.requestId = 12345;

    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));

    EXPECT_EQ(VirtMemFragSdk::MemTaskQuery(req, context), VM_OK);

    GlobalMockObject::verify();
    MOCKER(SendResponse).reset();
}

TEST_F(TestMemFragmentationSdkServer, MemTaskQuery_ConvertTaskResult_failed)
{
    std::string taskId = "test_task_123";
    async_task_info_c taskInfoC{};
    taskInfoC.task_id[0] = '\0';
    strcpy_s(taskInfoC.task_id, MEM_TASK_ID_MAX, taskId.c_str());
    taskInfoC.status = ASYNC_TASK_SUCCESS;
    taskInfoC.resultCode = 0;
    taskInfoC.memBorrowResult.borrow_ids_size = 2;
    taskInfoC.memBorrowResult.present_numa_ids_size = 1;
    strcpy_s(taskInfoC.memBorrowResult.borrow_ids_ptr[0], MAX_BORROW_ID_LENGTH, "id1");
    strcpy_s(taskInfoC.memBorrowResult.borrow_ids_ptr[1], MAX_BORROW_ID_LENGTH, "id2");
    taskInfoC.memBorrowResult.present_numa_ids_ptr[0] = 100;
    strcpy_s(taskInfoC.memBorrowResult.task_id, MEM_TASK_ID_MAX, "test_task_123");

    MemTaskResultQueryMsg msg(taskInfoC);
    ASSERT_EQ(msg.Serialize(), VM_OK);

    UbseIpcMessage req{const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(taskId.c_str())),
                       static_cast<uint32_t>(taskId.size())};
    UbseRequestContext context{};
    context.requestId = 12345;

    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));

    EXPECT_EQ(VirtMemFragSdk::MemTaskQuery(req, context), VM_OK);

    GlobalMockObject::verify();
    MOCKER(SendResponse).reset();
}

TEST_F(TestMemFragmentationSdkServer, MemTaskQuery_ConvertTaskResult_succeed)
{
    std::string taskId = ThreadTaskManager::GetInstance().AddTask("memborrow");
    async_task_info_c taskInfoC{};
    taskInfoC.task_id[0] = '\0';
    strcpy_s(taskInfoC.task_id, MEM_TASK_ID_MAX, taskId.c_str());
    taskInfoC.status = ASYNC_TASK_SUCCESS;
    taskInfoC.resultCode = 0;
    taskInfoC.memBorrowResult.borrow_ids_size = 2;
    taskInfoC.memBorrowResult.present_numa_ids_size = 1;
    strcpy_s(taskInfoC.memBorrowResult.borrow_ids_ptr[0], MAX_BORROW_ID_LENGTH, "id1");
    strcpy_s(taskInfoC.memBorrowResult.borrow_ids_ptr[1], MAX_BORROW_ID_LENGTH, "id2");
    taskInfoC.memBorrowResult.present_numa_ids_ptr[0] = 100;
    strcpy_s(taskInfoC.memBorrowResult.task_id, MEM_TASK_ID_MAX, "test_task_123");

    ThreadTaskManager::GetInstance().SetMemBorrowResult(taskId, taskInfoC.memBorrowResult);

    UbseIpcMessage req{const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(taskId.c_str())),
                       static_cast<uint32_t>(taskId.size())};
    UbseRequestContext context{};
    context.requestId = 12345;

    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));

    EXPECT_EQ(VirtMemFragSdk::MemTaskQuery(req, context), VM_OK);
    ThreadTaskManager::GetInstance().ClearAllTasks();
    GlobalMockObject::verify();
    MOCKER(SendResponse).reset();
}

TEST_F(TestMemFragmentationSdkServer, MemReturn_Async_WhenEverythingIsOk)
{
    bool isAsync = true;
    UbseIpcMessage req{reinterpret_cast<uint8_t*>(&isAsync), sizeof(bool)};
    UbseRequestContext context{};

    MOCKER(MempoolingModule::UBSRMRSMemFree).stubs().will(invoke(MockUBSRMRSMemFreeOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));

    uint32_t ret = VirtMemFragSdk::MemReturn(req, context);
    EXPECT_EQ(ret, VM_OK);

    GlobalMockObject::verify();
    MOCKER(MempoolingModule::UBSRMRSMemFree).reset();
    MOCKER(SendResponse).reset();
}

TEST_F(TestMemFragmentationSdkServer, MemReturn_Sync_WhenEverythingIsOk)
{
    bool isAsync = false;
    UbseIpcMessage req{reinterpret_cast<uint8_t*>(&isAsync), sizeof(bool)};
    UbseRequestContext context{};

    MOCKER(MempoolingModule::UBSRMRSMemFree).stubs().will(invoke(MockUBSRMRSMemFreeOK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));

    uint32_t ret = VirtMemFragSdk::MemReturn(req, context);
    EXPECT_EQ(ret, VM_OK);

    GlobalMockObject::verify();
    MOCKER(MempoolingModule::UBSRMRSMemFree).reset();
    MOCKER(SendResponse).reset();
}

TEST_F(TestMemFragmentationSdkServer, MemReturn_Sync_UBSRMRSMemFreeFailed)
{
    bool isAsync = false;
    UbseIpcMessage req{reinterpret_cast<uint8_t*>(&isAsync), sizeof(bool)};
    UbseRequestContext context{};

    MOCKER(MempoolingModule::UBSRMRSMemFree).stubs().will(invoke(MockUBSRMRSMemFreeError));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));

    uint32_t ret = VirtMemFragSdk::MemReturn(req, context);
    EXPECT_EQ(ret, VM_OK);

    GlobalMockObject::verify();
    MOCKER(MempoolingModule::UBSRMRSMemFree).reset();
    MOCKER(SendResponse).reset();
}

TEST_F(TestMemFragmentationSdkServer, BorrowParamDeserializeTest)
{
    UbseIpcMessage req{};
    mem_fragmentation::BorrowParam borrowParam{};
    bool isAsync{};
    // error fork 1
    EXPECT_EQ(VirtMemFragSdk::BorrowParamDeserialize(req, borrowParam, isAsync), VM_ERROR);

    req.buffer = reinterpret_cast<uint8_t *>(true);
    req.length = sizeof(bool);
    // error fork 2
    EXPECT_EQ(VirtMemFragSdk::BorrowParamDeserialize(req, borrowParam, isAsync), VM_ERROR_DESERIALIZE_ERROR);

    // error fork 3
    mem_fragmentation::BorrowParam borrowParamOri{
        .nodeId = "",
        .numaMetaInfos = std::vector<mem_fragmentation::NumaMetaInfo>{},
        .borrowSize = 0,
    };
    mem_fragmentation::MemFragmentationMemBorrowParamMsg reqMsg(borrowParamOri, true);
    if (const auto ret = reqMsg.Serialize(); ret != VM_OK) {
        return;
    }
    req.buffer = reqMsg.SerializedData();
    req.length = reqMsg.SerializedDataSize();
    EXPECT_EQ(VirtMemFragSdk::BorrowParamDeserialize(req, borrowParam, isAsync), VM_ERROR);

    // success fork
    borrowParamOri.nodeId = "1";
    borrowParamOri.numaMetaInfos = std::vector{
        mem_fragmentation::NumaMetaInfo{
            .socketId = 0,
            .numaId = 0,
        },
        mem_fragmentation::NumaMetaInfo{
            .socketId = 0,
            .numaId = 1,
        },
    };
    borrowParamOri.borrowSize = 1024;
    mem_fragmentation::MemFragmentationMemBorrowParamMsg reqMsg2(borrowParamOri, true);
    if (const auto ret = reqMsg2.Serialize(); ret != VM_OK) {
        return;
    }
    req.buffer = reqMsg2.SerializedData();
    req.length = reqMsg2.SerializedDataSize();
    EXPECT_EQ(VirtMemFragSdk::BorrowParamDeserialize(req, borrowParam, isAsync), VM_OK);
}

UBSRMRSBatchBorrowStrategyFunc MockMemBorrowStrategyByRMRS()
{
    return [](const BatchSrcMemoryBorrowParam &, const uint64_t &, std::vector<MemBorrowStrategyResult> &,
              BorrowStrategy) -> uint32_t {
        return VM_OK;
    };
}

UBSRMRSBatchBorrowStrategyFunc MockMemBorrowStrategyByRMRSError()
{
    return [](const BatchSrcMemoryBorrowParam &, const uint64_t &, std::vector<MemBorrowStrategyResult> &,
              BorrowStrategy) -> uint32_t {
        return VM_ERROR;
    };
}

UBSRMRSBatchBorrowStrategyFunc MockMemBorrowStrategyByRMRSException()
{
    return [](const BatchSrcMemoryBorrowParam &, const uint64_t &, std::vector<MemBorrowStrategyResult> &,
              BorrowStrategy) -> uint32_t {
        throw std::exception();
    };
}

TEST_F(TestMemFragmentationSdkServer, MemBorrowStrategyByRMRSTest)
{
    const mem_fragmentation::BorrowParam borrowParam{
        .nodeId = "1",
        .numaMetaInfos =
            std::vector{
                mem_fragmentation::NumaMetaInfo{
                    .socketId = 0,
                    .numaId = 0,
                },
                mem_fragmentation::NumaMetaInfo{
                    .socketId = 0,
                    .numaId = 1,
                },
            },
        .borrowSize = 1024,
    };
    std::vector<MemBorrowStrategyResult> borrowStrategyRst{};

    // error fork 1
    MOCKER(MempoolingModule::UBSRMRSBatchBorrowStrategy)
        .stubs()
        .will(returnValue(static_cast<UBSRMRSBatchBorrowStrategyFunc>(nullptr)));
    EXPECT_EQ(VirtMemFragSdk::MemBorrowStrategyByRMRS(borrowParam, borrowStrategyRst), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSBatchBorrowStrategy).reset();

    // error fork 2
    MOCKER(MempoolingModule::UBSRMRSBatchBorrowStrategy).stubs().will(invoke(MockMemBorrowStrategyByRMRSError));
    EXPECT_EQ(VirtMemFragSdk::MemBorrowStrategyByRMRS(borrowParam, borrowStrategyRst), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSBatchBorrowStrategy).reset();

    // error fork 3
    MOCKER(MempoolingModule::UBSRMRSBatchBorrowStrategy).stubs().will(invoke(MockMemBorrowStrategyByRMRSException));
    EXPECT_EQ(VirtMemFragSdk::MemBorrowStrategyByRMRS(borrowParam, borrowStrategyRst), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSBatchBorrowStrategy).reset();

    // success fork
    MOCKER(MempoolingModule::UBSRMRSBatchBorrowStrategy).stubs().will(invoke(MockMemBorrowStrategyByRMRS));
    EXPECT_EQ(VirtMemFragSdk::MemBorrowStrategyByRMRS(borrowParam, borrowStrategyRst), VM_OK);
    MOCKER(MempoolingModule::UBSRMRSBatchBorrowStrategy).reset();
}

/**
 * @tc.name  : RunBorrowExec_ShouldReturnError_WhenUBSRMRSMemBorrowExecuteIsNull
 * @tc.number: RunBorrowExec_001
 * @tc.desc  : 当获取UBSRMRSMemBorrowExecute函数指针为空时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, RunBorrowExec_ShouldReturnError_WhenUBSRMRSMemBorrowExecuteIsNull)
{
    const std::string taskId = "test_task_123";
    MemBorrowStrategyResult memBorrowStrategyRst{};
    mem_borrow_result_c memBorrowRstC{};

    UBSRMRSMemBorrowExecuteFunc func = nullptr;
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).stubs().will(returnValue(func));

    EXPECT_EQ(VirtMemFragSdk::RunBorrowExec(taskId, memBorrowStrategyRst, memBorrowRstC), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).reset();
}

/**
 * @tc.name  : RunBorrowExec_ShouldReturnError_WhenUBSRMRSMemBorrowExecuteFailed
 * @tc.number: RunBorrowExec_002
 * @tc.desc  : 当UBSRMRSMemBorrowExecute执行失败时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, RunBorrowExec_ShouldReturnError_WhenUBSRMRSMemBorrowExecuteFailed)
{
    const std::string taskId = "test_task_123";
    MemBorrowStrategyResult memBorrowStrategyRst{};
    mem_borrow_result_c memBorrowRstC{};

    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).stubs().will(invoke(MockUBSRMRSMemBorrowExecuteError));

    EXPECT_EQ(VirtMemFragSdk::RunBorrowExec(taskId, memBorrowStrategyRst, memBorrowRstC), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).reset();
}

/**
 * @tc.name  : RunBorrowExec_ShouldReturnError_WhenConvertToLegacyResultFailed
 * @tc.number: RunBorrowExec_003
 * @tc.desc  : 当ConvertToLegacyResult转换失败时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, RunBorrowExec_ShouldReturnError_WhenConvertToLegacyResultFailed)
{
    const std::string taskId = "test_task_123";
    MemBorrowStrategyResult memBorrowStrategyRst{};
    mem_borrow_result_c memBorrowRstC{};

    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).stubs().will(invoke(MockUBSRMRSMemBorrowExecuteOK));
    MOCKER(VirtMemFragSdk::ConvertToLegacyResult).stubs().will(returnValue(VM_ERROR));

    EXPECT_EQ(VirtMemFragSdk::RunBorrowExec(taskId, memBorrowStrategyRst, memBorrowRstC), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).reset();
    MOCKER(VirtMemFragSdk::ConvertToLegacyResult).reset();
}

/**
 * @tc.name  : RunBorrowExec_ShouldReturnError_WhenSetMemBorrowResultFailed
 * @tc.number: RunBorrowExec_004
 * @tc.desc  : 当ThreadTaskManager::SetMemBorrowResult失败时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, RunBorrowExec_ShouldReturnError_WhenSetMemBorrowResultFailed)
{
    const std::string taskId = "test_task_123";
    MemBorrowStrategyResult memBorrowStrategyRst{};
    mem_borrow_result_c memBorrowRstC{};

    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).stubs().will(invoke(MockUBSRMRSMemBorrowExecuteOK));
    MOCKER(VirtMemFragSdk::ConvertToLegacyResult).stubs().will(returnValue(VM_OK));
    MOCKER_CPP(&ThreadTaskManager::SetMemBorrowResult,
               uint32_t (ThreadTaskManager::*)(const std::string &, const mem_borrow_result_c &))
        .stubs()
        .will(returnValue(VM_ERROR));

    EXPECT_EQ(VirtMemFragSdk::RunBorrowExec(taskId, memBorrowStrategyRst, memBorrowRstC), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).reset();
    MOCKER(VirtMemFragSdk::ConvertToLegacyResult).reset();
    MOCKER_CPP(&ThreadTaskManager::SetMemBorrowResult,
               uint32_t (ThreadTaskManager::*)(const std::string &, const mem_borrow_result_c &))
        .reset();
}

/**
 * @tc.name  : RunBorrowExec_ShouldReturnOk_WhenEverythingIsOk
 * @tc.number: RunBorrowExec_005
 * @tc.desc  : 当一切正常时，函数返回成功
 */
TEST_F(TestMemFragmentationSdkServer, RunBorrowExec_ShouldReturnOk_WhenEverythingIsOk)
{
    const std::string taskId = "test_task_123";
    MemBorrowStrategyResult memBorrowStrategyRst{};
    mem_borrow_result_c memBorrowRstC{};

    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).stubs().will(invoke(MockUBSRMRSMemBorrowExecuteOK));
    MOCKER(VirtMemFragSdk::ConvertToLegacyResult).stubs().will(returnValue(VM_OK));
    MOCKER_CPP(&ThreadTaskManager::SetMemBorrowResult,
               uint32_t (ThreadTaskManager::*)(const std::string &, const mem_borrow_result_c &))
        .stubs()
        .will(returnValue(VM_OK));
    MOCKER_CPP(&ThreadTaskManager::UpdateTaskStatus,
               void (ThreadTaskManager::*)(const std::string &, AsyncTaskStatus, uint32_t, const std::string &))
        .stubs();

    EXPECT_EQ(VirtMemFragSdk::RunBorrowExec(taskId, memBorrowStrategyRst, memBorrowRstC), VM_OK);
    MOCKER(MempoolingModule::UBSRMRSMemBorrowExecute).reset();
    MOCKER(VirtMemFragSdk::ConvertToLegacyResult).reset();
    MOCKER_CPP(&ThreadTaskManager::SetMemBorrowResult,
               uint32_t (ThreadTaskManager::*)(const std::string &, const mem_borrow_result_c &))
        .reset();
    MOCKER_CPP(&ThreadTaskManager::UpdateTaskStatus,
               void (ThreadTaskManager::*)(const std::string &, AsyncTaskStatus, uint32_t, const std::string &))
        .reset();
}

/**
 * @tc.name  : SyncMemBorrowExec_ShouldReturnError_WhenTaskCreationFailed
 * @tc.number: SyncMemBorrowExec_001
 * @tc.desc  : 当任务创建失败时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, SyncMemBorrowExec_ShouldReturnError_WhenTaskCreationFailed)
{
    std::vector<MemBorrowStrategyResult> borrowStrategyRsts{{}};
    std::vector<mem_borrow_result_c> memBorrowRstCs{};

    MOCKER_CPP(&ThreadTaskManager::AddTask, std::string (ThreadTaskManager::*)(const std::string &))
        .stubs()
        .will(returnValue(std::string()));

    EXPECT_EQ(VirtMemFragSdk::SyncMemBorrowExec(borrowStrategyRsts, memBorrowRstCs), VM_ERROR);
    MOCKER_CPP(&ThreadTaskManager::AddTask, std::string (ThreadTaskManager::*)(const std::string &)).reset();
}

/**
 * @tc.name  : SyncMemBorrowExec_ShouldReturnError_WhenRunBorrowExecFailed
 * @tc.number: SyncMemBorrowExec_002
 * @tc.desc  : 当RunBorrowExec执行失败时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, SyncMemBorrowExec_ShouldReturnError_WhenRunBorrowExecFailed)
{
    std::vector<MemBorrowStrategyResult> borrowStrategyRsts{{}};
    std::vector<mem_borrow_result_c> memBorrowRstCs{};

    MOCKER_CPP(&ThreadTaskManager::AddTask, std::string (ThreadTaskManager::*)(const std::string &))
        .stubs()
        .will(returnValue(std::string("task_123")));
    MOCKER(VirtMemFragSdk::RunBorrowExec).stubs().will(returnValue(VM_ERROR));
    MOCKER_CPP(&ThreadTaskManager::UpdateTaskStatus,
               void (ThreadTaskManager::*)(const std::string &, AsyncTaskStatus, uint32_t, const std::string &))
        .stubs();

    EXPECT_EQ(VirtMemFragSdk::SyncMemBorrowExec(borrowStrategyRsts, memBorrowRstCs), VM_OK);
    MOCKER_CPP(&ThreadTaskManager::AddTask, std::string (ThreadTaskManager::*)(const std::string &)).reset();
    MOCKER(VirtMemFragSdk::RunBorrowExec).reset();
    MOCKER_CPP(&ThreadTaskManager::UpdateTaskStatus,
               void (ThreadTaskManager::*)(const std::string &, AsyncTaskStatus, uint32_t, const std::string &))
        .reset();
}

/**
 * @tc.name  : SyncMemBorrowExec_ShouldReturnOk_WhenEverythingIsOk
 * @tc.number: SyncMemBorrowExec_003
 * @tc.desc  : 当一切正常时，函数返回成功
 */
TEST_F(TestMemFragmentationSdkServer, SyncMemBorrowExec_ShouldReturnOk_WhenEverythingIsOk)
{
    std::vector<MemBorrowStrategyResult> borrowStrategyRsts{{}};
    std::vector<mem_borrow_result_c> memBorrowRstCs{};

    MOCKER_CPP(&ThreadTaskManager::AddTask, std::string (ThreadTaskManager::*)(const std::string &))
        .stubs()
        .will(returnValue(std::string("task_123")));
    MOCKER(VirtMemFragSdk::RunBorrowExec).stubs().will(returnValue(VM_OK));
    MOCKER_CPP(&ThreadTaskManager::UpdateTaskStatus,
               void (ThreadTaskManager::*)(const std::string &, AsyncTaskStatus, uint32_t, const std::string &))
        .stubs();

    EXPECT_EQ(VirtMemFragSdk::SyncMemBorrowExec(borrowStrategyRsts, memBorrowRstCs), VM_OK);
    MOCKER_CPP(&ThreadTaskManager::AddTask, std::string (ThreadTaskManager::*)(const std::string &)).reset();
    MOCKER(VirtMemFragSdk::RunBorrowExec).reset();
    MOCKER_CPP(&ThreadTaskManager::UpdateTaskStatus,
               void (ThreadTaskManager::*)(const std::string &, AsyncTaskStatus, uint32_t, const std::string &))
        .reset();
}

/**
 * @tc.name  : AsyncMemBorrowExec_ShouldReturnError_WhenTaskCreationFailed
 * @tc.number: AsyncMemBorrowExec_001
 * @tc.desc  : 当任务创建失败时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, AsyncMemBorrowExec_ShouldReturnError_WhenTaskCreationFailed)
{
    std::vector<MemBorrowStrategyResult> borrowStrategyRsts{{}};
    std::vector<mem_borrow_result_c> memBorrowRstCs{};

    MOCKER_CPP(&ThreadTaskManager::AddTask, std::string (ThreadTaskManager::*)(const std::string &))
        .stubs()
        .will(returnValue(std::string()));

    EXPECT_EQ(VirtMemFragSdk::AsyncMemBorrowExec(borrowStrategyRsts, memBorrowRstCs), VM_ERROR);
    MOCKER_CPP(&ThreadTaskManager::AddTask, std::string (ThreadTaskManager::*)(const std::string &)).reset();
}

/**
 * @tc.name  : AsyncMemBorrowExec_ShouldReturnError_WhenStringToCFailed
 * @tc.number: AsyncMemBorrowExec_002
 * @tc.desc  : 当StringToC转换失败时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, AsyncMemBorrowExec_ShouldReturnError_WhenStringToCFailed)
{
    std::vector<MemBorrowStrategyResult> borrowStrategyRsts{{}};
    std::vector<mem_borrow_result_c> memBorrowRstCs{};

    MOCKER_CPP(&ThreadTaskManager::AddTask, std::string (ThreadTaskManager::*)(const std::string &))
        .stubs()
        .will(returnValue(std::string("task_123")));
    MOCKER(StringToC).stubs().will(returnValue(VM_ERROR));
    MOCKER_CPP(&ThreadTaskManager::UpdateTaskStatus,
               void (ThreadTaskManager::*)(const std::string &, AsyncTaskStatus, uint32_t, const std::string &))
        .stubs();

    EXPECT_EQ(VirtMemFragSdk::AsyncMemBorrowExec(borrowStrategyRsts, memBorrowRstCs), VM_ERROR);
    MOCKER_CPP(&ThreadTaskManager::AddTask, std::string (ThreadTaskManager::*)(const std::string &)).reset();
    MOCKER(StringToC).reset();
    MOCKER_CPP(&ThreadTaskManager::UpdateTaskStatus,
               void (ThreadTaskManager::*)(const std::string &, AsyncTaskStatus, uint32_t, const std::string &))
        .reset();
}

/**
 * @tc.name  : AsyncMemBorrowExec_ShouldReturnError_WhenThreadCreationFailed
 * @tc.number: AsyncMemBorrowExec_003
 * @tc.desc  : 当线程创建失败时，函数返回失败
 */
TEST_F(TestMemFragmentationSdkServer, AsyncMemBorrowExec_ShouldReturnError_WhenThreadCreationFailed)
{
    std::vector<MemBorrowStrategyResult> borrowStrategyRsts{{}};
    std::vector<mem_borrow_result_c> memBorrowRstCs{};

    MOCKER_CPP(&ThreadTaskManager::AddTask, std::string (ThreadTaskManager::*)(const std::string &))
        .stubs()
        .will(returnValue(std::string("task_123")));
    MOCKER(StringToC).stubs().will(returnValue(VM_OK));
    MOCKER_CPP(&ThreadTaskManager::UpdateTaskStatus,
               void (ThreadTaskManager::*)(const std::string &, AsyncTaskStatus, uint32_t, const std::string &))
        .stubs();

    // 这里模拟线程创建失败的情况，实际线程创建失败会在运行时抛出异常
    EXPECT_EQ(VirtMemFragSdk::AsyncMemBorrowExec(borrowStrategyRsts, memBorrowRstCs), VM_OK);
    MOCKER_CPP(&ThreadTaskManager::AddTask, std::string (ThreadTaskManager::*)(const std::string &)).reset();
    MOCKER(StringToC).reset();
    MOCKER_CPP(&ThreadTaskManager::UpdateTaskStatus,
               void (ThreadTaskManager::*)(const std::string &, AsyncTaskStatus, uint32_t, const std::string &))
        .reset();
}

/**
 * @tc.name  : AsyncMemBorrowExec_ShouldReturnOk_WhenEverythingIsOk
 * @tc.number: AsyncMemBorrowExec_004
 * @tc.desc  : 当一切正常时，函数返回成功
 */
TEST_F(TestMemFragmentationSdkServer, AsyncMemBorrowExec_ShouldReturnOk_WhenEverythingIsOk)
{
    std::vector<MemBorrowStrategyResult> borrowStrategyRsts{{}};
    std::vector<mem_borrow_result_c> memBorrowRstCs{};

    MOCKER_CPP(&ThreadTaskManager::AddTask, std::string (ThreadTaskManager::*)(const std::string &))
        .stubs()
        .will(returnValue(std::string("task_123")));
    MOCKER(StringToC).stubs().will(returnValue(VM_OK));
    MOCKER_CPP(&ThreadTaskManager::UpdateTaskStatus,
               void (ThreadTaskManager::*)(const std::string &, AsyncTaskStatus, uint32_t, const std::string &))
        .stubs();

    EXPECT_EQ(VirtMemFragSdk::AsyncMemBorrowExec(borrowStrategyRsts, memBorrowRstCs), VM_OK);
    MOCKER_CPP(&ThreadTaskManager::AddTask, std::string (ThreadTaskManager::*)(const std::string &)).reset();
    MOCKER(StringToC).reset();
    MOCKER_CPP(&ThreadTaskManager::UpdateTaskStatus,
               void (ThreadTaskManager::*)(const std::string &, AsyncTaskStatus, uint32_t, const std::string &))
        .reset();
}

TEST_F(TestMemFragmentationSdkServer, BorrowResultSerializeTest)
{
    const std::vector memBorrowRstCs{
        mem_borrow_result_c{
            .borrow_ids_ptr = {"borrowId1"},
            .present_numa_ids_ptr = {1},
            .borrow_ids_size = 1,
            .present_numa_ids_size = 1,
            .task_id = "task1",
        },
    };
    UbseIpcMessage resp{};

    // success fork
    EXPECT_EQ(VirtMemFragSdk::BorrowResultSerialize(memBorrowRstCs, resp), VM_OK);
}

TEST_F(TestMemFragmentationSdkServer, MemBorrowTest)
{
    const mem_fragmentation::BorrowParam borrowParam{
        .nodeId = "1",
        .numaMetaInfos =
            std::vector{
                mem_fragmentation::NumaMetaInfo{
                    .socketId = 0,
                    .numaId = 0,
                },
                mem_fragmentation::NumaMetaInfo{
                    .socketId = 0,
                    .numaId = 1,
                },
            },
        .borrowSize = 1024,
    };
    mem_fragmentation::MemFragmentationMemBorrowParamMsg reqMsg(borrowParam, true);
    if (const auto ret = reqMsg.Serialize(); ret != VM_OK) {
        return;
    }
    UbseIpcMessage req{
        .buffer = reqMsg.SerializedData(),
        .length = reqMsg.SerializedDataSize(),
    };
    UbseRequestContext context{};
    // error fork 1
    MOCKER(VirtMemFragSdk::BorrowParamDeserialize).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::MemBorrow(req, context), VM_ERROR);
    MOCKER(VirtMemFragSdk::BorrowParamDeserialize).reset();

    // error fork 2
    MOCKER(VirtMemFragSdk::MemBorrowStrategyByRMRS).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::MemBorrow(req, context), VM_ERROR);
    MOCKER(VirtMemFragSdk::MemBorrowStrategyByRMRS).reset();

    MOCKER(VirtMemFragSdk::MemBorrowStrategyByRMRS).stubs().will(returnValue(VM_OK));
    // error fork 3
    MOCKER(VirtMemFragSdk::AsyncMemBorrowExec).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::MemBorrow(req, context), VM_ERROR);
    MOCKER(VirtMemFragSdk::AsyncMemBorrowExec).reset();

    MOCKER(VirtMemFragSdk::AsyncMemBorrowExec).stubs().will(returnValue(VM_OK));
    // error fork 4
    MOCKER(VirtMemFragSdk::BorrowResultSerialize).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::MemBorrow(req, context), VM_ERROR);
    MOCKER(VirtMemFragSdk::BorrowResultSerialize).reset();

    MOCKER(VirtMemFragSdk::BorrowResultSerialize).stubs().will(returnValue(VM_OK));
    // error fork 5
    MOCKER(SendResponse).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::MemBorrow(req, context), VM_ERROR);
    MOCKER(SendResponse).reset();

    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    // success fork 1
    EXPECT_EQ(VirtMemFragSdk::MemBorrow(req, context), VM_OK);

    // success fork 2
    mem_fragmentation::MemFragmentationMemBorrowParamMsg reqMsg2(borrowParam, false);
    if (const auto ret = reqMsg.Serialize(); ret != VM_OK) {
        return;
    }
    req.buffer = reqMsg.SerializedData();
    req.length = reqMsg.SerializedDataSize();
    MOCKER(VirtMemFragSdk::SyncMemBorrowExec).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::MemBorrow(req, context), VM_OK);
    MOCKER(VirtMemFragSdk::SyncMemBorrowExec).reset();
    MOCKER(SendResponse).reset();
    MOCKER(VirtMemFragSdk::BorrowResultSerialize).reset();
    MOCKER(VirtMemFragSdk::AsyncMemBorrowExec).reset();
    MOCKER(VirtMemFragSdk::AsyncMemBorrowExec).reset();
}

TEST_F(TestMemFragmentationSdkServer, PageSwapEnableParamSerializeTest)
{
    UbseIpcMessage req{};
    pid_t pid{};
    std::vector<mem_fragmentation::PageSwapPair> pageSwapPairs{};
    // error fork 1
    EXPECT_EQ(VirtMemFragSdk::PageSwapEnableParamSerialize(req, pid, pageSwapPairs), VM_ERROR);

    req.buffer = reinterpret_cast<uint8_t *>(true);
    req.length = sizeof(bool);
    // error fork 2
    EXPECT_EQ(VirtMemFragSdk::PageSwapEnableParamSerialize(req, pid, pageSwapPairs), VM_ERROR_DESERIALIZE_ERROR);

    // success fork
    const std::vector pageSwapPairsOri{
        mem_fragmentation::PageSwapPair{
            .localNumaQuotas = std::vector{mem_fragmentation::NumaQuota{
                .numaId = 1,
                .quota = 1024,
            }},
            .remoteNumaQuotas = std::vector{mem_fragmentation::NumaQuota{
                .numaId = 5,
                .quota = 1024,
            }},
        },
    };
    mem_fragmentation::MemFragmentationPageSwapEnableMsg reqMsg(123, pageSwapPairsOri);
    if (const auto ret = reqMsg.Serialize(); ret != VM_OK) {
        return;
    }
    req.buffer = reqMsg.SerializedData();
    req.length = reqMsg.SerializedDataSize();
    EXPECT_EQ(VirtMemFragSdk::PageSwapEnableParamSerialize(req, pid, pageSwapPairs), VM_OK);
}

UBSRMRSSmapEnableProcessMigrateGroupedFunc MockUBSRMRSSmapEnableProcessMigrateGrouped()
{
    return [](pid_t, const std::vector<mempooling::PageSwapPair> &) -> uint32_t {
        return VM_OK;
    };
}

UBSRMRSSmapEnableProcessMigrateGroupedFunc MockUBSRMRSSmapEnableProcessMigrateGroupedError()
{
    return [](pid_t, const std::vector<mempooling::PageSwapPair> &) -> uint32_t {
        return VM_ERROR;
    };
}

UBSRMRSSmapEnableProcessMigrateGroupedFunc MockUBSRMRSSmapEnableProcessMigrateGroupedException()
{
    return [](pid_t, const std::vector<PageSwapPair> &) -> uint32_t {
        throw std::exception();
    };
}

TEST_F(TestMemFragmentationSdkServer, PageSwapEnableByRMRSTest)
{
    const pid_t pid = 123;
    const std::vector pageSwapPairs{
        mem_fragmentation::PageSwapPair{
            .localNumaQuotas = std::vector{mem_fragmentation::NumaQuota{
                .numaId = 1,
                .quota = 1024,
            }},
            .remoteNumaQuotas = std::vector{mem_fragmentation::NumaQuota{
                .numaId = 5,
                .quota = 1024,
            }},
        },
    };
    // error fork 1
    MOCKER(MempoolingModule::UBSRMRSSmapEnableProcessMigrateGrouped)
        .stubs()
        .will(returnValue(static_cast<UBSRMRSSmapEnableProcessMigrateGroupedFunc>(nullptr)));
    EXPECT_EQ(VirtMemFragSdk::PageSwapEnableByRMRS(pid, pageSwapPairs), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSSmapEnableProcessMigrateGrouped).reset();

    // error fork 2
    MOCKER(MempoolingModule::UBSRMRSSmapEnableProcessMigrateGrouped)
        .stubs()
        .will(invoke(MockUBSRMRSSmapEnableProcessMigrateGroupedError));
    MOCKER(MempoolingModule::UBSRMRSSmapEnableProcessMigrateGrouped)
        .stubs()
        .will(invoke(MockUBSRMRSSmapEnableProcessMigrateGroupedError));
    EXPECT_EQ(VirtMemFragSdk::PageSwapEnableByRMRS(pid, pageSwapPairs), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSSmapEnableProcessMigrateGrouped).reset();

    // error fork 3
    MOCKER(MempoolingModule::UBSRMRSSmapEnableProcessMigrateGrouped)
        .stubs()
        .will(invoke(MockUBSRMRSSmapEnableProcessMigrateGroupedException));
    EXPECT_EQ(VirtMemFragSdk::PageSwapEnableByRMRS(pid, pageSwapPairs), VM_ERROR);
    MOCKER(MempoolingModule::UBSRMRSSmapEnableProcessMigrateGrouped).reset();

    // success fork
    MOCKER(MempoolingModule::UBSRMRSSmapEnableProcessMigrateGrouped)
        .stubs()
        .will(invoke(MockUBSRMRSSmapEnableProcessMigrateGrouped));
    EXPECT_EQ(VirtMemFragSdk::PageSwapEnableByRMRS(pid, pageSwapPairs), VM_OK);
    MOCKER(MempoolingModule::UBSRMRSSmapEnableProcessMigrateGrouped).reset();
}

TEST_F(TestMemFragmentationSdkServer, PageSwapEnableTest)
{
    const std::vector pageSwapPairs{
        mem_fragmentation::PageSwapPair{
            .localNumaQuotas = std::vector{mem_fragmentation::NumaQuota{
                .numaId = 1,
                .quota = 1024,
            }},
            .remoteNumaQuotas = std::vector{mem_fragmentation::NumaQuota{
                .numaId = 5,
                .quota = 1024,
            }},
        },
    };
    mem_fragmentation::MemFragmentationPageSwapEnableMsg reqMsg(123, pageSwapPairs);
    if (const auto ret = reqMsg.Serialize(); ret != VM_OK) {
        return;
    }
    const UbseIpcMessage req{
        .buffer = reqMsg.SerializedData(),
        .length = reqMsg.SerializedDataSize(),
    };
    UbseRequestContext context{};
    // error fork 1
    MOCKER(VirtMemFragSdk::PageSwapEnableParamSerialize).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::PageSwapEnable(req, context), VM_ERROR);
    MOCKER(VirtMemFragSdk::PageSwapEnableParamSerialize).reset();

    // error fork 2
    MOCKER(VirtMemFragSdk::PageSwapEnableByRMRS).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::PageSwapEnable(req, context), VM_ERROR);
    MOCKER(VirtMemFragSdk::PageSwapEnableByRMRS).reset();

    // error fork 3
    MOCKER(VirtMemFragSdk::PageSwapEnableByRMRS).stubs().will(returnValue(VM_OK));
    MOCKER(SendResponse).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(VirtMemFragSdk::PageSwapEnable(req, context), VM_ERROR);
    MOCKER(VirtMemFragSdk::PageSwapEnableByRMRS).reset();
    MOCKER(SendResponse).reset();

    // success fork
    MOCKER(MempoolingModule::UBSRMRSSmapEnableProcessMigrateGrouped)
        .stubs()
        .will(invoke(MockUBSRMRSSmapEnableProcessMigrateGrouped));
    MOCKER(SendResponse).stubs().will(returnValue(VM_OK));
    EXPECT_EQ(VirtMemFragSdk::PageSwapEnable(req, context), VM_OK);
    MOCKER(MempoolingModule::UBSRMRSSmapEnableProcessMigrateGrouped).reset();
    MOCKER(SendResponse).reset();
}
} // namespace ubse::ut::vm
