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

#ifndef TEST_UBSE_CLI_SSU_CMD_REG_H
#define TEST_UBSE_CLI_SSU_CMD_REG_H

#include <cstdlib>

#include <gtest/gtest.h>

#include "ubse_cli_ssu_cmd_reg.h"

namespace ubse::ut::cli {
// 设置 UBSE_SSU_VM_MODE 环境变量(常量名引用生产代码 ubse_cli_ssu_cmd_reg.h,避免字符串漂移):
// enabled=true 写入 "1" 强制 VM 模式,false 写入 "0" 强制 host 模式。显式设值保证用例确定性,
// 不依赖测试机 /etc/ubse/ubse.conf 的实际内容;用例 TearDown 统一 unsetenv 避免污染其他用例。
// 仅可在 void 返回的测试上下文/夹具方法中调用(失败时经 ASSERT_EQ 直接返回)。
inline void SetVmModeEnv(bool enabled)
{
    ASSERT_EQ(::setenv(ubse::cli::reg::UBSE_SSU_VM_MODE_ENV, enabled ? "1" : "0", 1), 0);
}

// 清除 UBSE_SSU_VM_MODE 环境变量:与 SetVmModeEnv 配对使用,恢复"未设置 → 读配置文件"的默认判定路径。
inline void UnsetVmModeEnv() { ::unsetenv(ubse::cli::reg::UBSE_SSU_VM_MODE_ENV); }

// SSU 命令注册用例夹具：SetUp 清空 mock 捕获，TearDown 复位 ubse_invoke_call 桩并校验 mock 期望。
// 用例通过静态处理函数（UbseCliDisplaySsuFunc/UbseCliCreateSsuFunc）直接驱动，绕过框架命令解析。
class TestUbseCliSsuCmdReg : public testing::Test {
public:
    TestUbseCliSsuCmdReg() = default;

    void SetUp() override;
    void TearDown() override;
};
} // namespace ubse::ut::cli

#endif // TEST_UBSE_CLI_SSU_CMD_REG_H
