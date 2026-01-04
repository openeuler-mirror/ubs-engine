/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_ubse_urma_controller_rpc.h"
#include "ubse_urma_controller_rpc.h"
#include "ubse_com_base.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_urma_controller.h"

namespace ubse::urmaController::ut {
using namespace ubse::urmaController;
using namespace ubse::com;
using namespace ubse::context;
using namespace ubse::election;

TEST_F(TestUbseUrmaQosRpc, UbseUrmaQosRpcGetModuleCode)
{
    UbseUrmaQosMessageHandler ubseUrmaQosMessageHandler{};
    EXPECT_EQ(ubseUrmaQosMessageHandler.GetModuleCode(), static_cast<uint16_t>(UbseModuleCode::UBSE_URMA));
}

TEST_F(TestUbseUrmaQosRpc, UbseUrmaQosRpcGetOpCode)
{
    UbseUrmaQosMessageHandler ubseUrmaQosMessageHandler{};
    EXPECT_EQ(ubseUrmaQosMessageHandler.GetOpCode(), static_cast<uint16_t>(UbseUrmaRpcOpCode::URMA_RPC_QOS_QUERY));
}

uint32_t UbseGetNodeInfo_stub_0(UbseRoleInfo &roleInfo)
{
    roleInfo.nodeId = "0";
}

uint32_t UbseGetNodeInfo_stub_1(UbseRoleInfo &roleInfo)
{
    roleInfo.nodeId = "1";
}

TEST_F(TestUbseUrmaQosRpc, UbseUrmaQosRpcHandle_localNode)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseUrmaQosReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseUrmaQosRspSimpo();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};
    UbseUrmaQosMessageHandler ubseUrmaQosMessageHandler{};

    MOCKER_CPP(&UrmaController::UbseUrmaBandWidthGet).stubs().will(returnValue(UBSE_OK));
    const auto func = &UbseComModule::RpcSend<UbseUrmaQosReqPtr, UbseUrmaQosRspPtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().will(invoke(UbseGetNodeInfo_stub_0));
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(invoke(UbseGetNodeInfo_stub_1));
    EXPECT_EQ(ubseUrmaQosMessageHandler.Handle(req, rsp, ctx), UBSE_OK);
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().will(invoke(UbseGetNodeInfo_stub_1));
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(invoke(UbseGetNodeInfo_stub_0));    
    GlobalMockObject::verify();
}

TEST_F(TestUbseUrmaQosRpc, UbseUrmaQosRpcHandle_localNode_fail)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseUrmaQosReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseUrmaQosRspSimpo();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};
    UbseUrmaQosMessageHandler ubseUrmaQosMessageHandler{};

    MOCKER_CPP(&UrmaController::UbseUrmaBandWidthGet).stubs().will(returnValue(UBSE_ERROR));
    const auto func = &UbseComModule::RpcSend<UbseUrmaQosReqPtr, UbseUrmaQosRspPtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));

    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().will(invoke(UbseGetNodeInfo_stub_0));
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(invoke(UbseGetNodeInfo_stub_1));
    EXPECT_EQ(ubseUrmaQosMessageHandler.Handle(req, rsp, ctx), UBSE_ERROR_SRCH);
    GlobalMockObject::verify();
}

TEST_F(TestUbseUrmaQosRpc, UbseUrmaQosRpcHandle_MasterNode)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseUrmaQosReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseUrmaQosRspSimpo();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};
    UbseUrmaQosMessageHandler ubseUrmaQosMessageHandler{};
    std::shared_ptr<UbseComModule> comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    const auto func = &UbseComModule::RpcSend<UbseUrmaQosReqPtr, UbseUrmaQosRspPtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().will(invoke(UbseGetNodeInfo_stub_1));
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(invoke(UbseGetNodeInfo_stub_1));
    EXPECT_EQ(ubseUrmaQosMessageHandler.Handle(req, rsp, ctx), UBSE_OK);
    GlobalMockObject::verify();
}

TEST_F(TestUbseUrmaQosRpc, UbseUrmaQosRpcHandle_MasterNode_fail)
{
    const UbseBaseMessagePtr req = new (std::nothrow) UbseUrmaQosReqSimpo();
    const UbseBaseMessagePtr rsp = new (std::nothrow) UbseUrmaQosRspSimpo();
    EXPECT_NE(req, nullptr);
    EXPECT_NE(rsp, nullptr);
    UbseComBaseMessageHandlerCtxPtr ctx{};
    UbseUrmaQosMessageHandler ubseUrmaQosMessageHandler{};
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule));
    MOCKER_CPP(&UbseGetCurrentNodeInfo).stubs().will(invoke(UbseGetNodeInfo_stub_1));
    MOCKER_CPP(&UbseGetMasterInfo).stubs().will(invoke(UbseGetNodeInfo_stub_1));
    EXPECT_EQ(ubseUrmaQosMessageHandler.Handle(req, rsp, ctx), UBSE_ERROR_NULLPTR);
    GlobalMockObject::verify();
}

}