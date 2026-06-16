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
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_module.h"
#include "ubse_urma_def.h"
#include "ubse_timer.h"
#include "ubse_event.h"
#include "ubse_urma_controller_rpc.h"
#include "ubse_common_def.h"
#include "ubse_com_module.h"
#include "ubse_thread_pool_module.h"
#include "ubse_node_controller.h"
#include "ubse_com_base.h"

namespace ubse::urmaController {
extern std::atomic<uint32_t> g_asyncHandlerCnt;
extern std::set<std::string> g_RegTimerNames;
extern UbseResult DoTaskWithTimerCallback(const std::string &timerName, UbseUrmaRetryTaskHandler task);
extern UbseResult RpcReg();
extern void DisconnectAllNormalLink();
}

namespace ubse::urmaController {
class UbseUrmaControllerApi {
public:
    static UbseResult Register();
};
}

namespace ubse::urmaControllerModule::ut {
using namespace ubse::urmaController;
using namespace ubse::urma;
using namespace ubse::election;
using namespace ubse::nodeController;
using namespace ubse::context;
using namespace ubse::com;
using namespace ubse::task_executor;
using namespace ubse::event;
using namespace ubse::timer;

TEST_F(TestUbseUrmaControllerModule, AsyncHandlerGuard_DefaultConstructor)
{
    uint32_t before = g_asyncHandlerCnt.load();
    {
        AsyncHandlerGuard guard;
        EXPECT_EQ(g_asyncHandlerCnt.load(), before + 1);
    }
    EXPECT_EQ(g_asyncHandlerCnt.load(), before);
}

TEST_F(TestUbseUrmaControllerModule, AsyncHandlerGuard_ParameterizedConstructor)
{
    std::atomic<uint32_t> customCnt{0};
    EXPECT_EQ(customCnt.load(), 0);
    {
        AsyncHandlerGuard guard(customCnt);
        EXPECT_EQ(customCnt.load(), 1);
    }
    EXPECT_EQ(customCnt.load(), 0);
}

TEST_F(TestUbseUrmaControllerModule, Initialize_TaskExecutorNull)
{
    UbseUrmaControllerModule module;
    auto ret = module.Initialize();
    EXPECT_EQ(ret, UBSE_ERROR_NULLPTR);
}

TEST_F(TestUbseUrmaControllerModule, Start_ReturnsOk)
{
    UbseUrmaControllerModule module;
    MOCKER_CPP(RpcReg).stubs().will(returnValue(UBSE_OK));
    auto ret = module.Start();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, DoTaskWithTimerCallback_GlobalStop)
{
    ubse::context::g_globalStop = true;
    MOCKER_CPP(UbseTimerHandlerUnregister).stubs();
    auto task = []() -> UbseResult { return UBSE_OK; };
    auto ret = DoTaskWithTimerCallback("test_timer", task);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, DoTaskWithTimerCallback_TaskOk)
{
    ubse::context::g_globalStop = false;
    MOCKER_CPP(UbseTimerHandlerUnregister).stubs();
    auto task = []() -> UbseResult { return UBSE_OK; };
    auto ret = DoTaskWithTimerCallback("test_timer", task);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, DoTaskWithTimerCallback_TaskFails)
{
    ubse::context::g_globalStop = false;
    auto task = []() -> UbseResult { return UBSE_ERROR; };
    auto ret = DoTaskWithTimerCallback("test_timer", task);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerModule, HandleTaskWithRetry_GlobalStop)
{
    ubse::context::g_globalStop = true;
    auto task = []() -> UbseResult { return UBSE_OK; };
    auto ret = HandleTaskWithRetry("executor", "task_name", 1000, task);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, HandleTaskWithRetry_TaskOk)
{
    ubse::context::g_globalStop = false;
    auto task = []() -> UbseResult { return UBSE_OK; };
    auto ret = HandleTaskWithRetry("executor", "task_name", 1000, task);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, HandleTaskWithRetry_TaskFails)
{
    ubse::context::g_globalStop = false;
    g_RegTimerNames.clear();
    auto task = []() -> UbseResult { return UBSE_ERROR; };
    MOCKER_CPP(UbseTimerHandlerRegister).stubs().will(returnValue(UBSE_OK));
    auto ret = HandleTaskWithRetry("executor", "task_name", 1000, task);
    g_RegTimerNames.clear();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, DoTaskWithTimerCallback_TaskFailsAndGlobalStop)
{
    ubse::context::g_globalStop = false;
    MOCKER_CPP(UbseTimerHandlerUnregister).stubs();
    auto task = []() -> UbseResult { ubse::context::g_globalStop = true; return UBSE_ERROR; };
    auto ret = DoTaskWithTimerCallback("test_timer", task);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerModule, HandleTaskWithRetry_TaskFailsAndSetsGlobalStop)
{
    ubse::context::g_globalStop = false;
    g_RegTimerNames.clear();
    auto task = []() -> UbseResult { ubse::context::g_globalStop = true; return UBSE_ERROR; };
    auto ret = HandleTaskWithRetry("executor", "task_name", 1000, task);
    g_RegTimerNames.clear();
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

    UbseResult (UbseComModule::*regBrocast)(UbseComBaseMessageHandlerPtr &) =
        &UbseComModule::RegRpcService<UbseUrmaBrocastReqSimpo, UbseUrmaBrocastRspSimpo>;
    MOCKER_CPP(regBrocast).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RpcReg();
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, RpcReg_RegQueryFail)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));

    UbseResult (UbseComModule::*regBrocast)(UbseComBaseMessageHandlerPtr &) =
        &UbseComModule::RegRpcService<UbseUrmaBrocastReqSimpo, UbseUrmaBrocastRspSimpo>;
    MOCKER_CPP(regBrocast).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regQuery)(UbseComBaseMessageHandlerPtr &) =
        &UbseComModule::RegRpcService<UbseUrmaQueryReqSimpo, UbseUrmaQueryRspSimpo>;
    MOCKER_CPP(regQuery).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RpcReg();
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, RpcReg_RegDevQueryFail)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));

    UbseResult (UbseComModule::*regBrocast)(UbseComBaseMessageHandlerPtr &) =
        &UbseComModule::RegRpcService<UbseUrmaBrocastReqSimpo, UbseUrmaBrocastRspSimpo>;
    MOCKER_CPP(regBrocast).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regQuery)(UbseComBaseMessageHandlerPtr &) =
        &UbseComModule::RegRpcService<UbseUrmaQueryReqSimpo, UbseUrmaQueryRspSimpo>;
    MOCKER_CPP(regQuery).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regDevQuery)(UbseComBaseMessageHandlerPtr &) =
        &UbseComModule::RegRpcService<UrmaDevQueryReqSimpo, UrmaDevQueryRspSimpo>;
    MOCKER_CPP(regDevQuery).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RpcReg();
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, RpcReg_RegReportFail)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));

    UbseResult (UbseComModule::*regBrocast)(UbseComBaseMessageHandlerPtr &) =
        &UbseComModule::RegRpcService<UbseUrmaBrocastReqSimpo, UbseUrmaBrocastRspSimpo>;
    MOCKER_CPP(regBrocast).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regQuery)(UbseComBaseMessageHandlerPtr &) =
        &UbseComModule::RegRpcService<UbseUrmaQueryReqSimpo, UbseUrmaQueryRspSimpo>;
    MOCKER_CPP(regQuery).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regDevQuery)(UbseComBaseMessageHandlerPtr &) =
        &UbseComModule::RegRpcService<UrmaDevQueryReqSimpo, UrmaDevQueryRspSimpo>;
    MOCKER_CPP(regDevQuery).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regReport)(UbseComBaseMessageHandlerPtr &) =
        &UbseComModule::RegRpcService<UbseUrmaReportUrmaNodeInfoReqSimpo, UbseUrmaReportUrmaNodeInfoRspSimpo>;
    MOCKER_CPP(regReport).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RpcReg();
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, RpcReg_RegActivateFail)
{
    auto comModule = std::make_shared<UbseComModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseComModule>).stubs().will(returnValue(comModule));

    UbseResult (UbseComModule::*regBrocast)(UbseComBaseMessageHandlerPtr &) =
        &UbseComModule::RegRpcService<UbseUrmaBrocastReqSimpo, UbseUrmaBrocastRspSimpo>;
    MOCKER_CPP(regBrocast).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regQuery)(UbseComBaseMessageHandlerPtr &) =
        &UbseComModule::RegRpcService<UbseUrmaQueryReqSimpo, UbseUrmaQueryRspSimpo>;
    MOCKER_CPP(regQuery).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regDevQuery)(UbseComBaseMessageHandlerPtr &) =
        &UbseComModule::RegRpcService<UrmaDevQueryReqSimpo, UrmaDevQueryRspSimpo>;
    MOCKER_CPP(regDevQuery).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regReport)(UbseComBaseMessageHandlerPtr &) =
        &UbseComModule::RegRpcService<UbseUrmaReportUrmaNodeInfoReqSimpo, UbseUrmaReportUrmaNodeInfoRspSimpo>;
    MOCKER_CPP(regReport).stubs().will(returnValue(UBSE_OK));

    UbseResult (UbseComModule::*regActivate)(UbseComBaseMessageHandlerPtr &) =
        &UbseComModule::RegRpcService<UbseUrmaActivateUrmaInfoReqSimpo, UbseUrmaActivateUrmaInfoRspSimpo>;
    MOCKER_CPP(regActivate).stubs().will(returnValue(UBSE_ERROR));

    auto ret = RpcReg();
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, Initialize_CreateFail)
{
    UbseUrmaControllerModule module;
    auto taskExec = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskExec));
    MOCKER_CPP(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_ERROR));
    auto ret = module.Initialize();
    EXPECT_EQ(ret, UBSE_ERROR_CONF_INVALID);
}

TEST_F(TestUbseUrmaControllerModule, Initialize_RegisterFail)
{
    UbseUrmaControllerModule module;
    auto taskExec = std::make_shared<UbseTaskExecutorModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseTaskExecutorModule>).stubs().will(returnValue(taskExec));
    MOCKER_CPP(&UbseTaskExecutorModule::Create).stubs().will(returnValue(UBSE_OK));
    auto ret = module.Initialize();
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerModule, Start_RpcRegFail)
{
    UbseUrmaControllerModule module;
    auto ret = module.Start();
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

TEST_F(TestUbseUrmaControllerModule, Stop_UnSubEventNodeJoinFail)
{
    g_asyncHandlerCnt = 0;
    g_RegTimerNames.clear();
    MOCKER_CPP(UbseTimerHandlerUnregister).stubs();
    MOCKER_CPP(UbseUnSubEvent).stubs().will(returnValue(UBSE_ERROR));
    UbseUrmaControllerModule module;
    EXPECT_NO_THROW(module.Stop());
}

TEST_F(TestUbseUrmaControllerModule, Stop_UnSubEventNodeTopoLinkFail)
{
    g_asyncHandlerCnt = 0;
    g_RegTimerNames.clear();
    MOCKER_CPP(UbseTimerHandlerUnregister).stubs();
    MOCKER_CPP(UbseUnSubEvent).stubs().will(returnValue(UBSE_OK)).then(returnValue(UBSE_ERROR));
    UbseUrmaControllerModule module;
    EXPECT_NO_THROW(module.Stop());
}
} // namespace ubse::urmaControllerModule::ut
