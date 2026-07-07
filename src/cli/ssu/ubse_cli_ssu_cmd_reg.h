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

#ifndef UBSE_CLI_SSU_CMD_REG_H
#define UBSE_CLI_SSU_CMD_REG_H

#include "ubse_cli_reg.h"
#include "ubse_cli_reg_builder.h"

namespace ubse::cli::reg {
using namespace ubse::cli::framework;

// SSU 模块命令注册类：负责 display ssu / create ssu 两条命令的注册与处理函数实现。
// 各处理函数为静态，由框架在命令分发时按注册的函数指针回调。
class UbseCliRegSsuModule : public UbseCliRegModule {
public:
    void UbseCliSignUp() override;

private:
    // 命令元信息构建：定义命令字、操作对象、选项（含短/长选项与帮助描述）及处理函数。
    static UbseCliCommandInfo UbseCliDisplaySsu();
    static UbseCliCommandInfo UbseCliCreateSsu();

    // 命令处理函数：从框架接收已解析的 params（长选项名 → 值），完成校验/IPC/回显。
    static std::shared_ptr<UbseCliResultEcho> UbseCliDisplaySsuFunc(
        [[maybe_unused]] const std::map<std::string, std::string> &params);
    static std::shared_ptr<UbseCliResultEcho> UbseCliCreateSsuFunc(
        [[maybe_unused]] const std::map<std::string, std::string> &params);

    // display ssu 的两个子处理，由 UbseCliDisplaySsuFunc 按 -t 分流调用。
    static std::shared_ptr<UbseCliResultEcho> DisplayAllocSummary();
    static std::shared_ptr<UbseCliResultEcho> DisplayAllocDetail(const std::map<std::string, std::string> &params);
};
} // namespace ubse::cli::reg

#endif // UBSE_CLI_SSU_CMD_REG_H
