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

#include "test_ubse_context.h"

#include "ubse_error.h"
#include "ubse_module.h"

namespace ubse::ut::context {
using namespace ubse::context;
using namespace ubse::module;
using testing::_;
using testing::Return;

const int ARGC = 3; // 参数个数
const std::string ARG_NAME = "f";
const std::string ARG_VAL = "conf";
const std::string DASH_ARG_NAME = "-" + ARG_NAME;
char *g_argv[] = {(char *)"", (char *)DASH_ARG_NAME.c_str(), (char *)ARG_VAL.c_str()};

void RegisterMockModule(std::shared_ptr<MockUbseModule> mockModule)
{
    UbseModuleEntry entry;
    entry.creator = [mockModule]() {
        return mockModule;
    };
    entry.category = UbseModuleCategory::CORE;
    UbseModuleRegistry::GetInstance().registry_[MockUbseModule::kModuleName] = entry;
}

TEST_F(TestUbseContext, EMPTYMODULES)
{
    registry.registry_.clear();
    EXPECT_EQ(context.Run(ARGC, g_argv, ProcessMode::MANAGER), UBSE_OK);
}

TEST_F(TestUbseContext, CyclicDependencyModule)
{
    registry.registry_.clear();
    registry.RegisterCoreModule<MockUbseModule, MockUbseModuleD>();
    registry.RegisterCoreModule<MockUbseModuleD, MockUbseModule>();
    EXPECT_EQ(context.Run(ARGC, g_argv, ProcessMode::MANAGER), UBSE_ERROR_MODULE_LOAD_FAILED);
}
TEST_F(TestUbseContext, ModuleNotRegisteredError)
{
    registry.registry_.clear();
    registry.RegisterCoreModule<MockUbseModule, MockUbseModuleD>();
    EXPECT_EQ(context.Run(ARGC, g_argv, ProcessMode::MANAGER), UBSE_ERROR_MODULE_LOAD_FAILED);
}

TEST_F(TestUbseContext, RegisterArgError)
{
    auto mockModule = std::make_shared<MockUbseModule>();
    EXPECT_CALL(*mockModule, RegArgs()).WillOnce([]() { throw std::runtime_error(""); });
    RegisterMockModule(mockModule);
    EXPECT_EQ(context.Run(ARGC, g_argv, ProcessMode::MANAGER), UBSE_ERROR_PARSE_ARGS_FAILED);
}
TEST_F(TestUbseContext, ParserArgsError)
{
    char empty[3] = "";
    char *errArgv[] = {empty, const_cast<char *>(ARG_NAME.c_str()), const_cast<char *>(ARG_VAL.c_str())};
    auto module = std::make_shared<MockUbseModule>();
    RegisterMockModule(module);
    EXPECT_CALL(*module, RegArgs()).WillRepeatedly(Return());
    EXPECT_EQ(context.Run(ARGC, errArgv, ProcessMode::MANAGER), UBSE_ERROR_PARSE_ARGS_FAILED);
    char tmp[2] = "-";
    errArgv[1] = tmp;
    EXPECT_EQ(context.Run(ARGC, errArgv, ProcessMode::MANAGER), UBSE_ERROR_PARSE_ARGS_FAILED);
}
TEST_F(TestUbseContext, InitModuleError)
{
    auto mockModule = std::make_shared<MockUbseModule>();
    EXPECT_CALL(*mockModule, Initialize()).WillOnce(Return(UBSE_ERROR));
    RegisterMockModule(mockModule);
    EXPECT_EQ(context.Run(ARGC, g_argv, ProcessMode::MANAGER), UBSE_ERROR);
}
TEST_F(TestUbseContext, StartModuleError)
{
    auto mockModule = std::make_shared<MockUbseModule>();
    EXPECT_CALL(*mockModule, Initialize()).WillRepeatedly(Return(UBSE_OK));
    EXPECT_CALL(*mockModule, Start()).WillOnce(Return(UBSE_ERROR));
    RegisterMockModule(mockModule);
    EXPECT_EQ(context.Run(ARGC, g_argv, ProcessMode::MANAGER), UBSE_ERROR);
}
TEST_F(TestUbseContext, RunAllSuccess)
{
    auto mockModule = std::make_shared<MockUbseModule>();
    EXPECT_CALL(*mockModule, Start()).WillRepeatedly(Return(UBSE_OK));
    RegisterMockModule(mockModule);
    EXPECT_EQ(context.Run(ARGC, g_argv, ProcessMode::MANAGER), UBSE_OK);
}
TEST_F(TestUbseContext, StopModuleError)
{
    context.Run(ARGC, g_argv, ProcessMode::MANAGER);
    auto module = context.GetModule<MockUbseModule>().get();
    EXPECT_CALL(*module, Stop()).WillOnce([]() { throw std::runtime_error("stop error"); });
    EXPECT_NO_THROW(context.Stop());
}
TEST_F(TestUbseContext, DestroyModuleError)
{
    context.Run(ARGC, g_argv, ProcessMode::MANAGER);
    auto module = context.GetModule<MockUbseModule>().get();
    EXPECT_CALL(*module, Stop()).WillRepeatedly(Return());
    EXPECT_CALL(*module, UnInitialize()).WillOnce([]() { throw std::runtime_error("destroy error"); });
    EXPECT_NO_THROW(context.Stop());
}
TEST_F(TestUbseContext, DestroyModuleSuccess)
{
    context.Run(ARGC, g_argv);
    auto module = context.GetModule<MockUbseModule>().get();
    EXPECT_CALL(*module, Stop()).WillRepeatedly(Return());
    EXPECT_NO_THROW(context.Stop());
}

TEST_F(TestUbseContext, GetModuleFailed)
{
    context.Run(ARGC, g_argv, ProcessMode::MANAGER);
    EXPECT_EQ(context.GetModule<MockUbseModuleD>(), nullptr);
}
TEST_F(TestUbseContext, GetModuleSuccess)
{
    context.Run(ARGC, g_argv, ProcessMode::MANAGER);
    EXPECT_EQ(context.GetModule<MockUbseModule>(), context.moduleMap_[MockUbseModule::kModuleName]);
}
TEST_F(TestUbseContext, GetArgStrFailed)
{
    std::string noArgName = "noArgs"; // 不存在的参数
    std::string noArgValue;
    context.Run(ARGC, g_argv, ProcessMode::MANAGER); // 加载模型
    EXPECT_EQ(context.GetArgStr(noArgName, noArgValue), UBSE_ERROR_PARSE_ARGS_FAILED);
}
TEST_F(TestUbseContext, GetArgStrSuccess)
{
    context.Run(ARGC, g_argv, ProcessMode::MANAGER);
    std::string getArgValue;
    EXPECT_EQ(context.GetArgStr(ARG_NAME, getArgValue), UBSE_OK);
    EXPECT_STREQ(getArgValue.c_str(), ARG_VAL.c_str());
}
TEST_F(TestUbseContext, GetProcessDefaultMode)
{
    EXPECT_EQ(context.GetProcessMode(), ProcessMode::MANAGER);
}
TEST_F(TestUbseContext, GetProcessMode)
{
    context.Run(ARGC, g_argv, ProcessMode::MANAGER);
    EXPECT_EQ(context.GetProcessMode(), ProcessMode::MANAGER);
}
void TestUbseContext::SetUp()
{
    g_globalStop.store(false);
    registry.registry_.clear();
    registry.RegisterCoreModule<MockUbseModule>();

    Test::SetUp();
}

void TestUbseContext::TearDown()
{
    context.Stop();
    Test::TearDown();
}
} // namespace ubse::ut::context