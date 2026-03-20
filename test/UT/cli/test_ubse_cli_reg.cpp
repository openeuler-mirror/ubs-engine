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

#include "test_ubse_cli_reg.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_cli_reg.h"
#include "ubse_cli_reg_builder.h"

namespace ubse::ut::cli {
using namespace ubse::cli::reg;
using namespace ubse::cli::framework;

void TestUbseCliReg::SetUp() {}

void TestUbseCliReg::TearDown() {}

std::shared_ptr<UbseCliResultEcho> UbseCliTestFun([[maybe_unused]] const std::map<std::string, std::string> &params)
{
    return std::make_shared<UbseCliStringEcho>("Hello Ub!");
}

TEST_F(TestUbseCliReg, RegNormal)
{
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back({ "t1", "test1", "this is option test1" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    UbseCliRegBuilder builder;
    cmd_reg_info.push_back({ "command1", "type1", params_infos, UbseCliTestFun });
    cmd_reg_info.push_back({ "command2", "type2", {}, UbseCliTestFun });
    cmd_reg_info.push_back({ "command3", "type3", {}, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    auto flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse(
        { "command1", "type1", "-t1", "test" });
    EXPECT_TRUE(flag);
    flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ "command2", "type2" });
    EXPECT_TRUE(flag);
    flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ "command3", "type3" });
    EXPECT_TRUE(flag);
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, RegInfoNull)
{
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    ASSERT_NO_THROW(UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info));
}

TEST_F(TestUbseCliReg, RegInfoMissing)
{
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back({ "t1", "test1", "this is option test1" });
    std::vector<UbseCliOptionsInfo> params_infos_without_cmd;
    params_infos_without_cmd.push_back({ "", "test1", "this is option test1" });
    std::vector<UbseCliOptionsInfo> params_infos_without_type;
    params_infos_without_type.push_back({ "t1", "", "this is option test1" });
    std::vector<UbseCliOptionsInfo> params_infos_without_desc;
    params_infos_without_desc.push_back({ "t1", "test1", "" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ "command1", "type1", {}, UbseCliTestFun });
    cmd_reg_info.push_back({ "command2", "type2", params_infos, UbseCliTestFun });
    cmd_reg_info.push_back({ "command3", "type3", params_infos_without_cmd, UbseCliTestFun });
    cmd_reg_info.push_back({ "command4", "type4", params_infos_without_type, UbseCliTestFun });
    cmd_reg_info.push_back({ "command5", "type5", params_infos_without_desc, UbseCliTestFun });
    cmd_reg_info.push_back({ "", "type6", params_infos, UbseCliTestFun });
    cmd_reg_info.push_back({ "command7", "", params_infos, UbseCliTestFun });
    cmd_reg_info.push_back({ "command8", "type8", params_infos, nullptr });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    auto flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ "command1", "type1" });
    EXPECT_TRUE(flag);
    flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ "command2", "type2" });
    EXPECT_TRUE(flag);
    flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ "command3", "type3" });
    EXPECT_FALSE(flag);
    flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ "command4", "type4" });
    EXPECT_FALSE(flag);
    flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ "command5", "type5" });
    EXPECT_FALSE(flag);
    flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ "command6", "type6" });
    EXPECT_FALSE(flag);
    flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ "command7", "type7" });
    EXPECT_FALSE(flag);
    flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ "command8", "type8" });
    EXPECT_FALSE(flag);
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, RegInfoCmdTooLong)
{
    std::string command = "command1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    std::string command_normal = "command1";
    std::string type = "type1";
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back({ "t1", "test1", "this is option test1" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ command, type, params_infos, UbseCliTestFun });
    cmd_reg_info.push_back({ command_normal, type, params_infos, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    auto flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ command, type });
    EXPECT_FALSE(flag);
    flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ command_normal, type });
    EXPECT_TRUE(flag);
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, RegInfoTypeTooLong)
{
    std::string command = "command1";
    std::string type = "type1xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    std::string type_normal = "type1";
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back({ "t1", "test1", "this is option test1" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ command, type, params_infos, UbseCliTestFun });
    cmd_reg_info.push_back({ command, type_normal, params_infos, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    auto flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ command, type });
    EXPECT_FALSE(flag);
    flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ command, type_normal });
    EXPECT_TRUE(flag);
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, RegInfoOptTooLong)
{
    std::string command = "command1";
    std::string type = "type1";
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back(
        { "t1", "longnamexxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", "this is option test1" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ command, type, params_infos, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    auto flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ command, type });
    EXPECT_FALSE(flag);
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, RegInfoOptNumExceeds)
{
    std::string command = "command1";
    std::string type = "type1";
    std::vector<UbseCliOptionsInfo> params_infos;
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    params_infos.reserve(16);
    for (int i = 0; i < 16; ++i) {
        params_infos.push_back({ "t1" + std::to_string(i), "test1" + std::to_string(i), "this is option test1" });
    }
    cmd_reg_info.push_back({ command, type, params_infos, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    auto flag = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse({ command, type });
    EXPECT_FALSE(flag);
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, RegCmdInfoNumExceeds)
{
    std::string command = "command1";
    std::string type = "type1";
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.reserve(32);
    for (int i = 0; i < 32; ++i) {
        cmd_reg_info.push_back({ "command1" + std::to_string(i), "type1", {}, UbseCliTestFun });
    }
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().fullCommandInfo_.size(), 0);
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, OneCommandHelpInfo)
{
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back({ "t1", "test1", "this is option test1" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ "command1", "type1", params_infos, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().fullCommandInfo_.size(), 1);
    EXPECT_TRUE(UbseCliModuleRegistry::GetInstance().UbseCliHelpInfoParse({ "command1", "type1", "-h" }));
    EXPECT_TRUE(UbseCliModuleRegistry::GetInstance().UbseCliHelpInfoParse({ "command1", "type1", "--help" }));
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, AllCommandsHelpInfo)
{
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back({ "t1", "test1", "this is option test1" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ "command1", "type1", params_infos, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    std::vector<std::string> args{};
    EXPECT_TRUE(UbseCliModuleRegistry::GetInstance().UbseCliHelpInfoParse(args));
    args = { "--help" };
    EXPECT_TRUE(UbseCliModuleRegistry::GetInstance().UbseCliHelpInfoParse(args));
    args = { "display", "--help" };
    EXPECT_TRUE(UbseCliModuleRegistry::GetInstance().UbseCliHelpInfoParse(args));
    args = { "display", "info", "--help" };
    EXPECT_TRUE(UbseCliModuleRegistry::GetInstance().UbseCliHelpInfoParse(args));
    args = { "command1", "type1", "--help" };
    EXPECT_TRUE(UbseCliModuleRegistry::GetInstance().UbseCliHelpInfoParse(args));
    args = { "command1", "type1", "another", "--help" };
    EXPECT_TRUE(UbseCliModuleRegistry::GetInstance().UbseCliHelpInfoParse(args));
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, GetMappingCmd)
{
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back({ "t1", "test1", "this is option test1" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ "command1", "type1", params_infos, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    std::vector<std::string> args = { "command1", "type1" };
    EXPECT_TRUE(UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse(args));
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().UbseCliGetMatchCommand().command, "command1");
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().UbseCliGetMatchCommand().type, "type1");
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, CmdNotExit)
{
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back({ "t1", "test1", "this is option test1" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ "command1", "type1", params_infos, UbseCliTestFun });
    cmd_reg_info.push_back({ "command2", "type2", {}, UbseCliTestFun });
    cmd_reg_info.push_back({ "command3", "type3", {}, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    std::vector<std::string> args{ "command4", "type1" };
    EXPECT_FALSE(UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse(args));
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, GetInputOptValMap)
{
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back({ "t1", "test1", "this is option test1" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ "command1", "type1", params_infos, UbseCliTestFun });
    cmd_reg_info.push_back({ "command2", "type2", {}, UbseCliTestFun });
    cmd_reg_info.push_back({ "command3", "type3", {}, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    std::vector<std::string> args{ "command1", "type1", "--test1", "aaa" };
    EXPECT_TRUE(UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse(args));
    auto info = UbseCliModuleRegistry::GetInstance().UbseCliGetMatchCommand();
    EXPECT_EQ(info.command, "command1");
    EXPECT_EQ(info.type, "type1");
    auto infoMap = UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliGetInputOptionMap();
    EXPECT_EQ(infoMap.find("test1")->second, "aaa");
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, InputOptMapValNotExit)
{
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back({ "t1", "test1", "this is option test1" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ "command1", "type1", params_infos, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    std::vector<std::string> args{ "command4", "type1", "-t1", "aaa" };
    EXPECT_FALSE(UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse(args));
    EXPECT_EQ(0, UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliGetInputOptionMap().size());
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, InputInvalidOptVal)
{
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back({ "t1", "test1", "this is option test1" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ "command1", "type1", params_infos, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    std::vector<std::string> args{ "command1", "type1", "t2", "aaa" };
    EXPECT_FALSE(UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse(args));
    EXPECT_EQ(0, UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliGetInputOptionMap().size());
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, InputDupOptVal)
{
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back({ "t1", "test1", "this is option test1" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ "command1", "type1", params_infos, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    std::vector<std::string> args{ "command1", "type1", "-t1", "aaa", "--test1", "aaa" };
    EXPECT_FALSE(UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse(args));
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, InputOptNoVal)
{
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back({ "t1", "test1", "this is option test1" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ "command1", "type1", params_infos, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    std::vector<std::string> args{ "command1", "type1", "-t1" };
    EXPECT_FALSE(UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse(args));
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, InputUnkonwOpt)
{
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back({ "t1", "test1", "this is option test1" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ "command1", "type1", params_infos, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    std::vector<std::string> args{ "ubse-cli", "command1", "type1", "-t2", "aaa" };
    EXPECT_FALSE(UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse(args));
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, InputOptionMapNoOpt)
{
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ "command1", "type1", {}, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    std::vector<std::string> args{ "command1", "type1", "-t1", "aaa" };
    EXPECT_FALSE(UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse(args));
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, InputOptionMapNoOptSuccess)
{
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ "command1", "type1", {}, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    std::vector<std::string> args{ "command1", "type1" };
    EXPECT_TRUE(UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse(args));
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

TEST_F(TestUbseCliReg, InputTooLongValue)
{
    std::vector<UbseCliOptionsInfo> params_infos;
    params_infos.push_back({ "t1", "test1", "this is option test1" });
    std::vector<UbseCliCommandInfo> cmd_reg_info;
    cmd_reg_info.push_back({ "command1", "type1", params_infos, UbseCliTestFun });
    UbseCliModuleRegistry::GetInstance().UbseCliRegister(cmd_reg_info);
    std::vector<std::string> args{ "command1", "type1", "-t1", std::string(1025, 'a') };
    EXPECT_FALSE(UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliArgsParse(args));
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UbseCliModuleRegistry::GetInstance().UbseCliGetParseTool().UbseCliReset();
}

// 测试多线程安全性
TEST_F(TestUbseCliReg, ThreadSafety)
{
    UbseCliWaitIndicator indicator("Test Message");
    std::thread t1([&indicator]() { indicator.Stop(); });
    std::thread t2([&indicator]() { indicator.Stop(); });
    EXPECT_TRUE(t1.joinable());
    EXPECT_TRUE(t2.joinable());
    EXPECT_NO_THROW(t1.join());
    EXPECT_NO_THROW(t2.join());
}
} // namespace ubse::ut::cli
