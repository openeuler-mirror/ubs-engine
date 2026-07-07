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

#include <gtest/gtest.h>

namespace ubse::ut::cli {
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
