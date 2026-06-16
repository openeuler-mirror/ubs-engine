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

#include "test_ubse_urma_controller_module.h"
#include "ubse_com_base.h"
#include "ubse_com_module.h"
#include "ubse_common_def.h"
#include "ubse_conf.h"
#include "ubse_event.h"
#include "ubse_node_controller.h"
#include "ubse_thread_pool_module.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_module.h"
#include "ubse_urma_controller_qos.h"
#include "ubse_urma_controller_rpc.h"
#include "ubse_urma_controller_util.h"
#include "ubse_urma_def.h"

namespace ubse::urmaController {
extern UbseResult RpcReg();
extern void DisconnectAllNormalLink();
} // namespace ubse::urmaController

namespace ubse::urmaController {
class UbseUrmaControllerApi {
public:
    static UbseResult Register();
};
} // namespace ubse::urmaController

namespace ubse::urmaControllerModule::ut {
using namespace ubse::urmaController;
using namespace ubse::urma;
using namespace ubse::election;
using namespace ubse::nodeController;
using namespace ubse::context;
using namespace ubse::com;
using namespace ubse::task_executor;
using namespace ubse::event;

TEST_F(TestUbseUrmaControllerModule, Initialize_TaskExecutorNull)
{
    UbseUrmaControllerModule module;
    auto ret = module.Initialize();
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerModule, Start_ReturnsOk)
{
    UbseUrmaControllerModule module;
    auto ret = module.Start();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, RpcReg_ComModuleNull)
{
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule));
    auto ret = RpcReg();
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerModule, RpcReg_RegBrocastFail)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));

    UbseResult (UbseComModule::*regBrocast)(UbseComBaseMessageHandlerPtr&) =
        &UbseComModule::RegRpcService<UbseUrmaBrocastReqSimpo, UbseUrmaBrocastRspSimpo>;
    MOCKER_CPP(regBrocast).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RpcReg();
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, RpcReg_RegQueryFail)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));

    UbseResult (UbseComModule::*regBrocast)(UbseComBaseMessageHandlerPtr&) =
        &UbseComModule::RegRpcService<UbseUrmaBrocastReqSimpo, UbseUrmaBrocastRspSimpo>;
    MOCKER_CPP(regBrocast).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regQuery)(UbseComBaseMessageHandlerPtr&) =
        &UbseComModule::RegRpcService<UbseUrmaQueryReqSimpo, UbseUrmaQueryRspSimpo>;
    MOCKER_CPP(regQuery).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RpcReg();
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, RpcReg_RegDevQueryFail)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));

    UbseResult (UbseComModule::*regBrocast)(UbseComBaseMessageHandlerPtr&) =
        &UbseComModule::RegRpcService<UbseUrmaBrocastReqSimpo, UbseUrmaBrocastRspSimpo>;
    MOCKER_CPP(regBrocast).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regQuery)(UbseComBaseMessageHandlerPtr&) =
        &UbseComModule::RegRpcService<UbseUrmaQueryReqSimpo, UbseUrmaQueryRspSimpo>;
    MOCKER_CPP(regQuery).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regDevQuery)(UbseComBaseMessageHandlerPtr&) =
        &UbseComModule::RegRpcService<UrmaDevQueryReqSimpo, UrmaDevQueryRspSimpo>;
    MOCKER_CPP(regDevQuery).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RpcReg();
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, RpcReg_RegReportFail)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));

    UbseResult (UbseComModule::*regBrocast)(UbseComBaseMessageHandlerPtr&) =
        &UbseComModule::RegRpcService<UbseUrmaBrocastReqSimpo, UbseUrmaBrocastRspSimpo>;
    MOCKER_CPP(regBrocast).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regQuery)(UbseComBaseMessageHandlerPtr&) =
        &UbseComModule::RegRpcService<UbseUrmaQueryReqSimpo, UbseUrmaQueryRspSimpo>;
    MOCKER_CPP(regQuery).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regDevQuery)(UbseComBaseMessageHandlerPtr&) =
        &UbseComModule::RegRpcService<UrmaDevQueryReqSimpo, UrmaDevQueryRspSimpo>;
    MOCKER_CPP(regDevQuery).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regReport)(UbseComBaseMessageHandlerPtr&) =
        &UbseComModule::RegRpcService<UbseUrmaReportUrmaNodeInfoReqSimpo, UbseUrmaReportUrmaNodeInfoRspSimpo>;
    MOCKER_CPP(regReport).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RpcReg();
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, RpcReg_RegActivateFail)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));

    UbseResult (UbseComModule::*regBrocast)(UbseComBaseMessageHandlerPtr&) =
        &UbseComModule::RegRpcService<UbseUrmaBrocastReqSimpo, UbseUrmaBrocastRspSimpo>;
    MOCKER_CPP(regBrocast).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regQuery)(UbseComBaseMessageHandlerPtr&) =
        &UbseComModule::RegRpcService<UbseUrmaQueryReqSimpo, UbseUrmaQueryRspSimpo>;
    MOCKER_CPP(regQuery).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regDevQuery)(UbseComBaseMessageHandlerPtr&) =
        &UbseComModule::RegRpcService<UrmaDevQueryReqSimpo, UrmaDevQueryRspSimpo>;
    MOCKER_CPP(regDevQuery).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regReport)(UbseComBaseMessageHandlerPtr&) =
        &UbseComModule::RegRpcService<UbseUrmaReportUrmaNodeInfoReqSimpo, UbseUrmaReportUrmaNodeInfoRspSimpo>;
    MOCKER_CPP(regReport).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RpcReg();
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, Initialize_CreateFail)
{
    UbseUrmaControllerModule module;
    MOCKER_CPP(UbseUrmaControllerApi::Register).stubs().will(returnValue(UBSE_OK));
    auto taskExec = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskExec));
    MOCKER_CPP(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_ERROR));
    auto ret = module.Initialize();
    EXPECT_EQ(ret, UBSE_ERROR_CONF_INVALID);
}

TEST_F(TestUbseUrmaControllerModule, Initialize_RegisterFail)
{
    UbseUrmaControllerModule module;
    MOCKER_CPP(UbseUrmaControllerApi::Register).stubs().will(returnValue(UBSE_ERROR));
    auto taskExec = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskExec));
    MOCKER_CPP(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    auto ret = module.Initialize();
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, Initialize_RpcRegFail)
{
    UbseUrmaControllerModule module;
    auto taskExec = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskExec));
    MOCKER_CPP(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseUrmaControllerApi::Register).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule));
    auto ret = module.Initialize();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerModule, Initialize_SubEventFail)
{
    UbseUrmaControllerModule module;
    auto taskExec = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskExec));
    MOCKER_CPP(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseUrmaControllerApi::Register).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(RpcReg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseSubEvent).stubs().will(returnValue(UBSE_ERROR));
    auto ret = module.Initialize();
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, Initialize_Success)
{
    UbseUrmaControllerModule module;
    auto taskExec = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskExec));
    MOCKER_CPP(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseUrmaControllerApi::Register).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(RpcReg).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseSubEvent).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseUrmaControllerQos<EtsQosConfig>::UbseUrmaQosInit).stubs().will(returnValue(UBSE_OK));
    auto ret = module.Initialize();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, DisconnectAllNormalLink_ComModuleNull)
{
    std::shared_ptr<UbseComModule> nullModule = nullptr;
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(nullModule));
    EXPECT_NO_THROW(DisconnectAllNormalLink());
}

TEST_F(TestUbseUrmaControllerModule, DisconnectAllNormalLink_Success)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));
    std::unordered_map<std::string, UbseNodeInfo> nodes;
    UbseNodeInfo node;
    node.nodeId = "test_node";
    nodes["test_node"] = node;
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodes));
    MOCKER_CPP(&UbseComModule::RemoveChannel).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(DisconnectAllNormalLink());
}

} // namespace ubse::urmaControllerModule::ut
