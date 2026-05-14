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

#include "suite_test_pt_init.h"

#include "main_test_pt.h"
#include "service.h"

namespace ubse::pt {
static constexpr uint32_t TIME_OUT = 3e8;
static constexpr uint32_t UI_TIME_100 = 100;
static constexpr uint32_t UI_TIME_200 = 200;

ProcessMmap* TestPTInit::pMmap = nullptr;

TestPTInit::TestPTInit() {}

int32_t PTestPutStorage(ProcessMmap*);

void TestPTInit::SetUpTestSuite()
{
    std::cout << "TestITInit SetUpTestSuite" << std::endl;
    pMmap = MemoryMapping();

    // 资源初始化
    PTestResourceInit();
    // 注册操作函数
    PTestCmdAndFuncRegister(CMD_SERVER_START, PTestCmdServerStart, ProcessMode::MANAGER);
    PTestCmdAndFuncRegister(CMD_CLI_START, PTestCmdCliStart, ProcessMode::CLI);
    PTestCmdAndFuncRegister(CMD_SERVER_STOP, PTestCmdServerStop, ProcessMode::MANAGER);
    PTestCmdAndFuncRegister(CMD_CLI_STOP, PTestCmdCliStop, ProcessMode::CLI);

    PTestCmdAndFuncRegister(CMD_MEM_COM, PTestMemCom, ProcessMode::MANAGER);
}

void TestPTInit::SetUp()
{
    int32_t iStatus;

    // 设置子进程
    ASSERT_EQ(UBSE_OK, PTestSetProcess(pMmap));

    // 启动服务端
    pMmap->stServerInfo.uiCmd = CMD_SERVER_START;
    PTestGetServerResult(pMmap, TIME_OUT, &iStatus);
    ASSERT_EQ(UBSE_OK, iStatus);
    int32_t iStatusM;

    Test::SetUp();
}

void TestPTInit::TearDown()
{
    // 停止服务端
    int32_t iStatus;
    pMmap->stServerInfo.uiCmd = CMD_SERVER_STOP;
    PTestGetServerResult(pMmap, TIME_OUT, &iStatus);
    EXPECT_EQ(UBSE_OK, iStatus);
    PTestQuitProcess(pMmap);
    Test::TearDown();
}
void TestPTInit::TearDownTestSuite()
{
    // 释放资源
    PTestClearResource(pMmap);
}

TEST_F(TestPTInit, PT_TEST_MEM_COM)
{
    int32_t iStatusR;
    pMmap->stServerInfo.uiCmd = CMD_MEM_COM;
    PTestGetServerResult(pMmap, TIME_OUT, &iStatusR);
    EXPECT_EQ(UBSE_OK, iStatusR);
}
} // namespace ubse::pt