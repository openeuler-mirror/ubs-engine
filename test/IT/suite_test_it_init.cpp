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

#include "suite_test_it_init.h"

#include "com_it.h"
#include "dbg_deadloop_it.h"
#include "dbg_exception_it.h"
#include "main_test_it.h"
#include "plugin_it.h"

namespace ubse::it {
using namespace ubse::it::plugin;
using namespace ubse::it::com;
using namespace ubse::it::deadloop;
using namespace ubse::it::exception;

static constexpr uint32_t TIME_OUT = 30;

ProcessMmap *TestITInit::pMmap = nullptr;

TestITInit::TestITInit() {}

void TestITInit::SetUpTestSuite()
{
    std::cout << "TestITInit SetUpTestSuite" << std::endl;
    pMmap = MemoryMapping();

    // 资源初始化
    ITestResourceInit();
    // 注册操作函数
    ITestCmdAndFuncRegister(CMD_SERVER_START, ITestCmdServerStart, ProcessMode::MANAGER);

    // 注册插件操作函数
    ITestCmdAndFuncRegister(CMD_PLUGIN_ADDPLUGIN, ITestCmdPluginAddPlugin, ProcessMode::MANAGER);
    ITestCmdAndFuncRegister(CMD_PLUGIN_ADDADMISSION, ITestCmdPluginAddAdmission, ProcessMode::MANAGER);
    ITestCmdAndFuncRegister(CMD_PLUGIN_DELETEADMISSION, ITestCmdPluginDeleteAdmission, ProcessMode::MANAGER);
    ITestCmdAndFuncRegister(CMD_PLUGIN_RELOAD, ITestCmdPluginReloadPlugin, ProcessMode::MANAGER);

    ITestCmdAndFuncRegister(CMD_CLI_START, ITestCmdCliStart, ProcessMode::CLI);
    ITestCmdAndFuncRegister(CMD_SERVER_STOP, ITestCmdServerStop, ProcessMode::MANAGER);
    ITestCmdAndFuncRegister(CMD_CLI_STOP, ITestCmdCliStop, ProcessMode::CLI);

    ITestCmdAndFuncRegister(CMD_PUT_STORAGE, ITestPutStorage, ProcessMode::MANAGER);
    ITestCmdAndFuncRegister(CMD_GET_STORAGE, ITestGetData, ProcessMode::MANAGER);
    ITestCmdAndFuncRegister(CMD_GET_PREFIX_STORAGE, ITestCmdDBGetDataWithPrefix, ProcessMode::MANAGER);
    ITestCmdAndFuncRegister(CMD_DELETE_PREFIX_STORAGE, ITestCmdDBDeleteData, ProcessMode::MANAGER);

    // 注册通信操作函数
    ITestCmdAndFuncRegister(CMD_COM_INTERSEND, ITestCmdRpcSend, ProcessMode::MANAGER);
    ITestCmdAndFuncRegister(CMD_COM_INTERSENDUSECOM, ITestCmdSendUseUbseCom, ProcessMode::MANAGER);
    ITestCmdAndFuncRegister(CMD_COM_CLI_SEND, ITestCmdItCliSend, ProcessMode::CLI);
    ITestCmdAndFuncRegister(CMD_COM_MASTER_REGIISTER, ITestCmdItManagerSend, ProcessMode::MANAGER);

    // 注册死循环检测功能操作函数
    ITestCmdAndFuncRegister(CMD_DEADLOOP_SWITCH_ON, ITestCmdDeadloopEnable, ProcessMode::MANAGER);
    ITestCmdAndFuncRegister(CMD_DEADLOOP_SWITCH_OFF, ITestCmdDeadloopDisable, ProcessMode::MANAGER);
    // 注册异常检查操作函数
    ITestCmdAndFuncRegister(CMD_EXCEPTION_TEST, ITestCmdExceptionTest, ProcessMode::MANAGER);
}

/*
 * 用例描述：
 * 测试模块注册成功,在manager和cli进程都成功注册并成功加载和卸载
 * 测试步骤：
 * 1.启动cli进程
 * 2.cli处理
 * 3.停止cli进程
 * 预期结果：
 * 运行成功
 */
TEST_F(TestITInit, IT_CLI_PROCESS)
{
    GTEST_SKIP();
    int32_t iStatusA;

    // 启动cli
    pMmap->stCliInfo.uiCmd = CMD_CLI_START;
    ITestGetCliResult(pMmap, TIME_OUT, &iStatusA);
    EXPECT_EQ(UBSE_OK, iStatusA);

    // 停止cli
    pMmap->stCliInfo.uiCmd = CMD_CLI_STOP;
    ITestGetCliResult(pMmap, TIME_OUT, &iStatusA);
    EXPECT_EQ(UBSE_OK, iStatusA);
}

TEST_F(TestITInit, IT_TEST_STORAGE)
{
    int32_t iStatusM;
    pMmap->stServerInfo.uiCmd = CMD_PUT_STORAGE;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);

    pMmap->stServerInfo.uiCmd = CMD_GET_STORAGE;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);

    pMmap->stServerInfo.uiCmd = CMD_GET_PREFIX_STORAGE;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);

    pMmap->stServerInfo.uiCmd = CMD_DELETE_PREFIX_STORAGE;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);
}

TEST_F(TestITInit, IT_PLUGIN_ADDPLUGIN)
{
    GTEST_SKIP();
    int32_t iStatusM;
    pMmap->stServerInfo.uiCmd = CMD_PLUGIN_ADDPLUGIN;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);
}

TEST_F(TestITInit, IT_PLUGIN_ADDADMISSION)
{
    GTEST_SKIP();
    int32_t iStatusM;
    pMmap->stServerInfo.uiCmd = CMD_PLUGIN_ADDADMISSION;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);
}

TEST_F(TestITInit, IT_PLUGIN_DELETEADMISSION)
{
    GTEST_SKIP();
    int32_t iStatusM;
    pMmap->stServerInfo.uiCmd = CMD_PLUGIN_DELETEADMISSION;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);
}

TEST_F(TestITInit, IT_PLUGIN_RELOAD)
{
    int32_t iStatusM;
    pMmap->stServerInfo.uiCmd = CMD_PLUGIN_RELOAD;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);
}

TEST_F(TestITInit, IT_HTTP_SEND)
{
    GTEST_SKIP();
    int32_t iStatusA;
    pMmap->stServerInfo.uiCmd = CMD_HTTP_SEND_WITH_NO_PARAMS;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusA);
    EXPECT_EQ(UBSE_OK, iStatusA);
    sleep(NO_1);

    pMmap->stServerInfo.uiCmd = CMD_HTTP_SEND_WITH_ONLY_PARAMS;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusA);
    EXPECT_EQ(UBSE_OK, iStatusA);
    sleep(NO_1);

    pMmap->stServerInfo.uiCmd = CMD_HTTP_SEND_WITH_ONLY_REQUEST_BODY;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusA);
    EXPECT_EQ(UBSE_OK, iStatusA);
    sleep(NO_1);

    pMmap->stServerInfo.uiCmd = CMD_HTTP_SEND_WITH_ONLY_HEADERS;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusA);
    EXPECT_EQ(UBSE_OK, iStatusA);
    sleep(NO_1);

    pMmap->stServerInfo.uiCmd = CMD_HTTP_SEND_WITH_ALL_PARAMS;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusA);
    EXPECT_EQ(UBSE_OK, iStatusA);
    sleep(NO_1);

    pMmap->stServerInfo.uiCmd = CMD_HTTP_SEND_WITH_ERROR_URL;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusA);
    EXPECT_EQ(UBSE_OK, iStatusA);
    sleep(NO_1);

    pMmap->stServerInfo.uiCmd = CMD_HTTP_SEND_WITH_ERROR_RESPONSE;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusA);
    EXPECT_EQ(UBSE_OK, iStatusA);
}

/*
 * 用例描述：
 * 测试内部通信
 * 测试步骤：
 * 1.启动COM模块
 * 2.注册消息处理函数
 * 3.发送同步消息进行测试
 * 4.发送异步消息进行测试
 * 预期结果：
 * 运行成功，且在handler中接受到的消息正确
 */
TEST_F(TestITInit, IT_COM_INTERSEND)
{
    GTEST_SKIP();
    int32_t iStatusM;
    pMmap->stServerInfo.uiCmd = CMD_COM_INTERSEND;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);
    sleep(NO_1);
    pMmap->stServerInfo.uiCmd = CMD_COM_INTERSENDUSECOM;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);
}

/*
 * 用例描述：
 * 测试跨进程通信
 * 测试步骤：
 * 1.启动cli进程
 * 2.在master上注册消息处理函数
 * 3.检验链路是否正常链接
 * 4.cli向master发送同步信息
 * 5.cli向master发送异步信息
 * 预期结果：
 * 运行成功，且在handler中接受到的消息正确
 */
TEST_F(TestITInit, IT_SEND_IT)
{
    GTEST_SKIP();
    int32_t iStatusM;

    pMmap->stCliInfo.uiCmd = CMD_CLI_START;
    ITestGetCliResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);

    pMmap->stServerInfo.uiCmd = CMD_COM_MASTER_REGIISTER;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);

    // cli往manager发送信息
    pMmap->stCliInfo.uiCmd = CMD_COM_CLI_SEND;
    ITestGetCliResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);

    // 停止cli
    pMmap->stCliInfo.uiCmd = CMD_CLI_STOP;
    ITestGetCliResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);
}

/*
 * 用例描述：
 * 测试死循环检测功能
 * 测试步骤：
 * 1.死循环检测开关打开，检测代码段出现超时
 * 2.死循环检测开关关闭，检测代码段出现超时
 * 预期结果：
 * 运行成功，且输出到日志的信息正确
 */
TEST_F(TestITInit, IT_DEADLOOP)
{
    int32_t iStatusM;

    pMmap->stServerInfo.uiCmd = CMD_DEADLOOP_SWITCH_ON;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);
    sleep(NO_1);

    pMmap->stServerInfo.uiCmd = CMD_DEADLOOP_SWITCH_OFF;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);
}

/*
 * 用例描述：
 * 测试异常接管模块，接管动态库
 * 测试步骤：
 * 1.启动exception模块
 * 2.注册信号处理函数
 * 3.调取动态库中模拟异常函数
 * 预期结果：
 * 运行成功，且有正确栈信息函数名打印
 */
TEST_F(TestITInit, IT_EXCEPTION_TEST)
{
    GTEST_SKIP();
    int32_t iStatusM;
    pMmap->stServerInfo.uiCmd = CMD_EXCEPTION_TEST;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusM);
    EXPECT_EQ(UBSE_OK, iStatusM);
}

void TestITInit::SetUp()
{
    int32_t iStatus;

    // 设置子进程
    ASSERT_EQ(UBSE_OK, ITestSetProcess(pMmap));

    // 启动服务端
    pMmap->stServerInfo.uiCmd = CMD_SERVER_START;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatus);
    ASSERT_EQ(UBSE_OK, iStatus);
    int32_t iStatusM;

    pMmap->stServerInfo.uiCmd = CMD_HTTP_REGISTER_URL;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatusM);
    ASSERT_EQ(UBSE_OK, iStatusM);

    Test::SetUp();
}

void TestITInit::TearDown()
{
    // 停止服务端
    int32_t iStatus;
    pMmap->stServerInfo.uiCmd = CMD_SERVER_STOP;
    ITestGetServerResult(pMmap, TIME_OUT, &iStatus);
    ITestQuitProcess(pMmap);
    Test::TearDown();
}
void TestITInit::TearDownTestSuite()
{
    // 释放资源
    ITestClearResource(pMmap);
}
} // namespace ubse::it