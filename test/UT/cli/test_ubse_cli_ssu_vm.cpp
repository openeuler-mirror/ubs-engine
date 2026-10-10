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

// 注:本测试不验证 ubse_invoke_call 的 IPC 路径,仅验证 vm_mode 来源(环境变量覆盖项/ubse.conf)下的命令注册子集

#include <gtest/gtest.h>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "test_ubse_cli_ssu_cmd_reg.h"

namespace ubse::ut::cli {

using ubse::cli::reg::UbseCliRegSsuModule;
using ubse::cli::reg::IsSsuVmModeConf;
using ubse::cli::reg::ParseSsuVmModeConf;
using ubse::cli::framework::UbseCliCommandInfo;

// ============================================================================
// 测试夹具
// ============================================================================

class TestUbseCliSsuVm : public testing::Test {
public:
    void SetUp() override
    {
        // 默认清除环境变量与临时配置文件,用例按需通过 SetVmModeEnv/WriteConfFile 切换
        UnsetVmModeEnv();
        RemoveConfFile();
    }

    void TearDown() override
    {
        // 确保环境变量与临时文件被清除,避免污染其他测试
        UnsetVmModeEnv();
        RemoveConfFile();
    }

protected:
    // 写入临时 ubse.conf 供 IsSsuVmModeConf 读取(模拟真实部署的同一配置源)
    static void WriteConfFile(const std::string &path, const std::string &content)
    {
        std::ofstream file(path, std::ios::trunc);
        ASSERT_TRUE(file.is_open());
        file << content;
        confPath_ = path;
    }

    static void RemoveConfFile()
    {
        if (!confPath_.empty()) {
            std::remove(confPath_.c_str());
            confPath_.clear();
        }
    }

    // 在 cmd_ 中查找指定 command 字符串(返回 true 表示命令已注册)
    static bool HasCommand(const std::vector<UbseCliCommandInfo> &cmds, const std::string &command)
    {
        for (const auto &cmd : cmds) {
            if (cmd.command == command) {
                return true;
            }
        }
        return false;
    }

    inline static std::string confPath_;
};

// ============================================================================
// A. VM 模式命令注册
// ============================================================================

/*
 * 用例描述:VM 模式下 display ssu 命令被注册
 * 测试步骤:
 * 1、SetVmModeEnv(true)
 * 2、UbseCliRegSsuModule 实例化并调用 UbseCliSignUp
 * 预期结果:
 * 1、cmd_ 包含 display 命令
 */
TEST_F(TestUbseCliSsuVm, RegCmd_VmMode_IncludesDisplay)
{
    SetVmModeEnv(true);
    UbseCliRegSsuModule module;
    module.UbseCliSignUp();
    EXPECT_TRUE(HasCommand(module.cmd_, "display"));
}

/*
 * 用例描述:VM 模式下 attach ssu 命令被注册
 * 测试步骤:
 * 1、SetVmModeEnv(true)
 * 2、UbseCliRegSsuModule 实例化并调用 UbseCliSignUp
 * 预期结果:
 * 1、cmd_ 包含 attach 命令
 */
TEST_F(TestUbseCliSsuVm, RegCmd_VmMode_IncludesAttach)
{
    SetVmModeEnv(true);
    UbseCliRegSsuModule module;
    module.UbseCliSignUp();
    EXPECT_TRUE(HasCommand(module.cmd_, "attach"));
}

/*
 * 用例描述:VM 模式下 detach ssu 命令被注册
 * 测试步骤:
 * 1、SetVmModeEnv(true)
 * 2、UbseCliRegSsuModule 实例化并调用 UbseCliSignUp
 * 预期结果:
 * 1、cmd_ 包含 detach 命令
 */
TEST_F(TestUbseCliSsuVm, RegCmd_VmMode_IncludesDetach)
{
    SetVmModeEnv(true);
    UbseCliRegSsuModule module;
    module.UbseCliSignUp();
    EXPECT_TRUE(HasCommand(module.cmd_, "detach"));
}

/*
 * 用例描述:VM 模式下 create ssu 命令不注册(盘由 openstack 分配)
 * 测试步骤:
 * 1、SetVmModeEnv(true)
 * 2、UbseCliRegSsuModule 实例化并调用 UbseCliSignUp
 * 预期结果:
 * 1、cmd_ 不包含 create 命令
 */
TEST_F(TestUbseCliSsuVm, RegCmd_VmMode_ExcludesCreate)
{
    SetVmModeEnv(true);
    UbseCliRegSsuModule module;
    module.UbseCliSignUp();
    EXPECT_FALSE(HasCommand(module.cmd_, "create"));
}

/*
 * 用例描述:VM 模式下 delete ssu 命令不注册
 * 测试步骤:
 * 1、SetVmModeEnv(true)
 * 2、UbseCliRegSsuModule 实例化并调用 UbseCliSignUp
 * 预期结果:
 * 1、cmd_ 不包含 delete 命令
 */
TEST_F(TestUbseCliSsuVm, RegCmd_VmMode_ExcludesDelete)
{
    SetVmModeEnv(true);
    UbseCliRegSsuModule module;
    module.UbseCliSignUp();
    EXPECT_FALSE(HasCommand(module.cmd_, "delete"));
}

/*
 * 用例描述:VM 模式下注册的命令总数为 3(display/attach/detach)
 * 测试步骤:
 * 1、SetVmModeEnv(true)
 * 2、UbseCliRegSsuModule 实例化并调用 UbseCliSignUp
 * 预期结果:
 * 1、cmd_.size() == 3
 */
TEST_F(TestUbseCliSsuVm, RegCmd_VmMode_CommandCountIs3)
{
    SetVmModeEnv(true);
    UbseCliRegSsuModule module;
    module.UbseCliSignUp();
    EXPECT_EQ(module.cmd_.size(), 3u);
}

// ============================================================================
// B. host 模式向后兼容
// ============================================================================

/*
 * 用例描述:host 模式下 create ssu 命令被注册(向后兼容)
 * 测试步骤:
 * 1、SetVmModeEnv(false)
 * 2、UbseCliRegSsuModule 实例化并调用 UbseCliSignUp
 * 预期结果:
 * 1、cmd_ 包含 create 命令
 */
TEST_F(TestUbseCliSsuVm, RegCmd_HostMode_IncludesCreate)
{
    SetVmModeEnv(false);
    UbseCliRegSsuModule module;
    module.UbseCliSignUp();
    EXPECT_TRUE(HasCommand(module.cmd_, "create"));
}

/*
 * 用例描述:host 模式下 delete ssu 命令被注册(向后兼容)
 * 测试步骤:
 * 1、SetVmModeEnv(false)
 * 2、UbseCliRegSsuModule 实例化并调用 UbseCliSignUp
 * 预期结果:
 * 1、cmd_ 包含 delete 命令
 */
TEST_F(TestUbseCliSsuVm, RegCmd_HostMode_IncludesDelete)
{
    SetVmModeEnv(false);
    UbseCliRegSsuModule module;
    module.UbseCliSignUp();
    EXPECT_TRUE(HasCommand(module.cmd_, "delete"));
}

/*
 * 用例描述:host 模式下注册的命令总数为 5(display/create/delete/attach/detach)
 * 测试步骤:
 * 1、SetVmModeEnv(false)
 * 2、UbseCliRegSsuModule 实例化并调用 UbseCliSignUp
 * 预期结果:
 * 1、cmd_.size() == 5(向后兼容)
 */
TEST_F(TestUbseCliSsuVm, RegCmd_HostMode_CommandCountIs5)
{
    SetVmModeEnv(false);
    UbseCliRegSsuModule module;
    module.UbseCliSignUp();
    EXPECT_EQ(module.cmd_.size(), 5u);
}

// ============================================================================
// C. vm_mode 配置源统一(与守护进程同读 ubse.conf)
// ============================================================================

// 临时 conf 路径(不与部署路径 /etc/ubse/ubse.conf 冲突)
constexpr const char *TEST_CONF_PATH = "/tmp/ubse_test_ssu_vm_mode.conf";

// 合法 ubse.conf 片段:含其他节、注释与 [ubse.ssu] vm_mode=true(与仓库 conf/ubse.conf 结构一致)
const std::string VM_MODE_TRUE_CONF = R"([ubse.ssu]
# vm_mode comment line
; another comment
vm_mode=true
[other]
key=value
)";

/*
 * 用例描述:ubse.conf 中 [ubse.ssu] vm_mode=true 时 CLI 注册 VM 命令子集(与环境变量无关)
 * 测试步骤:
 * 1、不设置环境变量,写入临时 conf(vm_mode=true)
 * 2、IsSsuVmModeConf 读取该文件
 * 预期结果:
 * 1、返回 true,即与守护进程同源判定 VM 模式
 */
TEST_F(TestUbseCliSsuVm, VmModeConf_True_ReturnsVmMode)
{
    WriteConfFile(TEST_CONF_PATH, VM_MODE_TRUE_CONF);
    EXPECT_TRUE(IsSsuVmModeConf(TEST_CONF_PATH));
}

/*
 * 用例描述:ubse.conf 中 vm_mode=false 时 CLI 按 host 模式处理
 * 测试步骤:
 * 1、不设置环境变量,写入临时 conf(vm_mode=false)
 * 预期结果:
 * 1、IsSsuVmModeConf 返回 false
 */
TEST_F(TestUbseCliSsuVm, VmModeConf_False_ReturnsHostMode)
{
    WriteConfFile(TEST_CONF_PATH, "[ubse.ssu]\nvm_mode=false\n");
    EXPECT_FALSE(IsSsuVmModeConf(TEST_CONF_PATH));
}

/*
 * 用例描述:配置文件不存在时回落 host 模式
 * 测试步骤:
 * 1、不设置环境变量,不写任何 conf
 * 预期结果:
 * 1、IsSsuVmModeConf 对不存在的路径返回 false(与守护进程 ReadVmModeConfig 失败回落一致)
 */
TEST_F(TestUbseCliSsuVm, VmModeConf_MissingFile_FallsBackToHost)
{
    EXPECT_FALSE(IsSsuVmModeConf("/tmp/ubse_no_such_conf_file.conf"));
}

/*
 * 用例描述:环境变量覆盖项优先于配置文件
 * 测试步骤:
 * 1、写入 vm_mode=false 的 conf,同时设置 UBSE_SSU_VM_MODE=1
 * 预期结果:
 * 1、IsSsuVmModeConf 为 false 但 IsVmMode 注册子集生效:注册 exclude create/delete
 */
TEST_F(TestUbseCliSsuVm, VmModeEnv_OverridesConf)
{
    WriteConfFile(TEST_CONF_PATH, "[ubse.ssu]\nvm_mode=false\n");
    SetVmModeEnv(true);
    UbseCliRegSsuModule module;
    module.UbseCliSignUp();
    EXPECT_FALSE(IsSsuVmModeConf(TEST_CONF_PATH));
    EXPECT_TRUE(HasCommand(module.cmd_, "attach"));
    EXPECT_FALSE(HasCommand(module.cmd_, "create"));
}

// ============================================================================
// D. ParseSsuVmModeConf 解析规则(与守护进程 UbseConfigManager/GetBoolConf 语义对齐)
// ============================================================================

/*
 * 用例描述:vm_mode 取值语义与守护进程 GetBoolConf 一致,仅字面量 "true" 生效
 * 测试步骤:
 * 1、分别以 "true"/"True"/"1"/空值/缺失键 解析
 * 预期结果:
 * 1、仅 "true" 返回 true,其余(含大小写不符与非法值)返回 false
 */
TEST_F(TestUbseCliSsuVm, ParseSsuVmModeConf_ValueSemantics_MatchesDaemon)
{
    EXPECT_TRUE(ParseSsuVmModeConf("[ubse.ssu]\nvm_mode=true\n"));
    EXPECT_FALSE(ParseSsuVmModeConf("[ubse.ssu]\nvm_mode=True\n"));
    EXPECT_FALSE(ParseSsuVmModeConf("[ubse.ssu]\nvm_mode=1\n"));
    EXPECT_FALSE(ParseSsuVmModeConf("[ubse.ssu]\nvm_mode=\n"));
    EXPECT_FALSE(ParseSsuVmModeConf("[ubse.ssu]\nother_key=true\n"));
    EXPECT_FALSE(ParseSsuVmModeConf(""));
}

/*
 * 用例描述:解析器正确处理注释、空白、其他节与同键覆盖(与守护进程解析规则对齐)
 * 测试步骤:
 * 1、含 ';''#' 注释行、键值空白、vm_mode 出现在其他节、同键后值覆盖先值
 * 预期结果:
 * 1、其他节的 vm_mode 不生效;注释/空白行不影响;同键以最后一次出现为准
 */
TEST_F(TestUbseCliSsuVm, ParseSsuVmModeConf_ParserRules_MatchDaemon)
{
    // 注释与空白
    EXPECT_TRUE(ParseSsuVmModeConf("; comment\n# comment\n\n  [ubse.ssu]  \n  vm_mode  =  true  \n"));
    // 其他节的 vm_mode 不生效
    EXPECT_FALSE(ParseSsuVmModeConf("[other]\nvm_mode=true\n"));
    // 后值覆盖先值(守护进程 AddConfig 同键覆盖)
    EXPECT_FALSE(ParseSsuVmModeConf("[ubse.ssu]\nvm_mode=true\nvm_mode=false\n"));
    EXPECT_TRUE(ParseSsuVmModeConf("[ubse.ssu]\nvm_mode=false\nvm_mode=true\n"));
    // 非法行(key=value 缺失等号)不影响其余解析
    EXPECT_TRUE(ParseSsuVmModeConf("[ubse.ssu]\nbroken_line\nvm_mode=true\n"));
}

} // namespace ubse::ut::cli
