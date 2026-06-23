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

#include "test_ubse_npu_controller_dispatcher.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"

using namespace ubse::npu::controller;
using namespace ubse::context;
using namespace ::api::server;
using namespace ubse::common::def;

namespace ubse::npu::controller {
UbseResult QueryLocalUbDevices(const UbseIpcMessage& req, const UbseRequestContext& context);
UbseResult AllocUbDevice(const UbseIpcMessage& req, const UbseRequestContext& context);
UbseResult FreeUbDevice(const UbseIpcMessage& req, const UbseRequestContext& context);
UbseResult QueryTidUbaSize(const UbseIpcMessage& req, const UbseRequestContext& context);
} // namespace ubse::npu::controller

namespace ubse::npu::controller::ut {

void TestUbseNpuControllerDispatcher::SetUp()
{
    Test::SetUp();
}

void TestUbseNpuControllerDispatcher::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNpuControllerDispatcher, QueryLocalUbDevices_ExecuteFailed)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    MOCKER(QueryDeviceExecute).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(QueryLocalUbDevices(req, context), UBSE_ERROR);
}

TEST_F(TestUbseNpuControllerDispatcher, QueryLocalUbDevices_ApiServerModuleNull)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    TransRespMsg respMsg{nullptr, 0};
    MOCKER(QueryDeviceExecute).stubs().with(any(), outBound(respMsg)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<UbseApiServerModule>()));
    EXPECT_EQ(QueryLocalUbDevices(req, context), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNpuControllerDispatcher, QueryLocalUbDevices_SendResponseFailed)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    TransRespMsg respMsg{nullptr, 0};
    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER(QueryDeviceExecute).stubs().with(any(), outBound(respMsg)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NE(QueryLocalUbDevices(req, context), UBSE_OK);
}

TEST_F(TestUbseNpuControllerDispatcher, QueryLocalUbDevices_Success)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    TransRespMsg respMsg{nullptr, 0};
    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER(QueryDeviceExecute).stubs().with(any(), outBound(respMsg)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(QueryLocalUbDevices(req, context), UBSE_OK);
}

TEST_F(TestUbseNpuControllerDispatcher, AllocUbDevice_ExecuteFailed)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    MOCKER(AllocDeviceExecute).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(AllocUbDevice(req, context), UBSE_ERROR);
}

TEST_F(TestUbseNpuControllerDispatcher, AllocUbDevice_ApiServerModuleNull)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    TransRespMsg respMsg{nullptr, 0};
    MOCKER(AllocDeviceExecute).stubs().with(any(), outBound(respMsg)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<UbseApiServerModule>()));
    EXPECT_EQ(AllocUbDevice(req, context), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNpuControllerDispatcher, AllocUbDevice_SendResponseFailed)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    TransRespMsg respMsg{nullptr, 0};
    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER(AllocDeviceExecute).stubs().with(any(), outBound(respMsg)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NE(AllocUbDevice(req, context), UBSE_OK);
}

TEST_F(TestUbseNpuControllerDispatcher, AllocUbDevice_Success)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    TransRespMsg respMsg{nullptr, 0};
    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER(AllocDeviceExecute).stubs().with(any(), outBound(respMsg)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(AllocUbDevice(req, context), UBSE_OK);
}

TEST_F(TestUbseNpuControllerDispatcher, FreeUbDevice_ExecuteFailed)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    MOCKER(FreeDeviceExecute).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(FreeUbDevice(req, context), UBSE_ERROR);
}

TEST_F(TestUbseNpuControllerDispatcher, FreeUbDevice_ApiServerModuleNull)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    TransRespMsg respMsg{nullptr, 0};
    MOCKER(FreeDeviceExecute).stubs().with(any(), outBound(respMsg)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<UbseApiServerModule>()));
    EXPECT_EQ(FreeUbDevice(req, context), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNpuControllerDispatcher, FreeUbDevice_SendResponseFailed)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    TransRespMsg respMsg{nullptr, 0};
    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER(FreeDeviceExecute).stubs().with(any(), outBound(respMsg)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NE(FreeUbDevice(req, context), UBSE_OK);
}

TEST_F(TestUbseNpuControllerDispatcher, FreeUbDevice_Success)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    TransRespMsg respMsg{nullptr, 0};
    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER(FreeDeviceExecute).stubs().with(any(), outBound(respMsg)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(FreeUbDevice(req, context), UBSE_OK);
}

TEST_F(TestUbseNpuControllerDispatcher, QueryTidUbaSize_ExecuteFailed)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    MOCKER(QueryTidUbaSizeExecute).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(QueryTidUbaSize(req, context), UBSE_ERROR);
}

TEST_F(TestUbseNpuControllerDispatcher, QueryTidUbaSize_ApiServerModuleNull)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    TransRespMsg respMsg{nullptr, 0};
    MOCKER(QueryTidUbaSizeExecute).stubs().with(any(), outBound(respMsg)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<UbseApiServerModule>()));
    EXPECT_EQ(QueryTidUbaSize(req, context), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNpuControllerDispatcher, QueryTidUbaSize_SendResponseFailed)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    TransRespMsg respMsg{nullptr, 0};
    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER(QueryTidUbaSizeExecute).stubs().with(any(), outBound(respMsg)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NE(QueryTidUbaSize(req, context), UBSE_OK);
}

TEST_F(TestUbseNpuControllerDispatcher, QueryTidUbaSize_Success)
{
    UbseIpcMessage req{nullptr, 0};
    UbseRequestContext context{};
    TransRespMsg respMsg{nullptr, 0};
    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER(QueryTidUbaSizeExecute).stubs().with(any(), outBound(respMsg)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::SendResponse).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(QueryTidUbaSize(req, context), UBSE_OK);
}

TEST_F(TestUbseNpuControllerDispatcher, RegisterSdkDispatcher_ApiServerModuleNull)
{
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<UbseApiServerModule>()));
    EXPECT_EQ(RegisterSdkDispatcher(), UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseNpuControllerDispatcher, RegisterSdkDispatcher_RegisterFailed)
{
    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::RegisterIpcHandler).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NE(RegisterSdkDispatcher(), UBSE_OK);
}

TEST_F(TestUbseNpuControllerDispatcher, RegisterSdkDispatcher_Success)
{
    auto apiServerModule = std::make_shared<UbseApiServerModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseApiServerModule>).stubs().will(returnValue(apiServerModule));
    MOCKER_CPP(&UbseApiServerModule::RegisterIpcHandler).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(RegisterSdkDispatcher(), UBSE_OK);
}

} // namespace ubse::npu::controller::ut
