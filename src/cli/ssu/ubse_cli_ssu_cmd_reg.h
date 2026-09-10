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

#include <string>

#include "ubse_cli_reg.h"
#include "ubse_cli_reg_builder.h"

namespace ubse::cli::reg {
using namespace ubse::cli::framework;

// ============================================================================
// VM 模式检测(供 IsVmMode 与 UT 复用)
// CLI 与守护进程统一读取同一配置源 ubse.conf 的 [ubse.ssu] vm_mode:
// 守护进程经 UbseGetBool 读取(见 ubse_ssu_controller_module.cpp ReadVmModeConfig),
// CLI 为轻量客户端不加载框架配置模块,按同一节/键/取值语义(字面量 "true")直接解析同一文件。
// ============================================================================
// VM 模式环境变量覆盖项:仅字面量 "1" 强制 VM 模式,其他任何值(含 "0"、空串)强制回落 host 模式;
// 未设置时以配置文件为准。声明于头文件供测试用例与生产代码共用同一常量名,避免字符串漂移。
inline constexpr char UBSE_SSU_VM_MODE_ENV[] = "UBSE_SSU_VM_MODE";
// 解析 ubse.conf 全文内容,[ubse.ssu] vm_mode == "true" 返回 true;
// 未找到该键、值非法或内容为空返回 false(与守护进程 ReadVmModeConfig 失败回落 host 模式一致)
bool ParseSsuVmModeConf(const std::string &confContent);
// 读取指定路径的 ubse.conf 并解析 vm_mode;文件不存在或不可读返回 false
bool IsSsuVmModeConf(const std::string &confPath);

// SSU 模块命令注册类：负责 display/create/delete/attach/detach ssu 命令的注册与处理函数实现。
// 各处理函数为静态，由框架在命令分发时按注册的函数指针回调。
class UbseCliRegSsuModule : public UbseCliRegModule {
public:
    void UbseCliSignUp() override;

private:
    // 命令元信息构建：定义命令字、操作对象、选项（含短/长选项与帮助描述）及处理函数。
    static UbseCliCommandInfo UbseCliDisplaySsu();
    static UbseCliCommandInfo UbseCliCreateSsu();
    static UbseCliCommandInfo UbseCliDeleteSsu();
    static UbseCliCommandInfo UbseCliAttachSsu();
    static UbseCliCommandInfo UbseCliDetachSsu();

    // 命令处理函数：从框架接收已解析的 params（长选项名 → 值），完成校验/IPC/回显。
    static std::shared_ptr<UbseCliResultEcho> UbseCliDisplaySsuFunc(
        [[maybe_unused]] const std::map<std::string, std::string> &params);
    static std::shared_ptr<UbseCliResultEcho> UbseCliCreateSsuFunc(
        [[maybe_unused]] const std::map<std::string, std::string> &params);
    static std::shared_ptr<UbseCliResultEcho> UbseCliDeleteSsuFunc(
        [[maybe_unused]] const std::map<std::string, std::string> &params);
    static std::shared_ptr<UbseCliResultEcho> UbseCliAttachSsuFunc(
        [[maybe_unused]] const std::map<std::string, std::string> &params);
    static std::shared_ptr<UbseCliResultEcho> UbseCliDetachSsuFunc(
        [[maybe_unused]] const std::map<std::string, std::string> &params);

    // display ssu 的三个子处理，由 UbseCliDisplaySsuFunc 按 -t 分流调用。
    static std::shared_ptr<UbseCliResultEcho> DisplayAllocSummary();
    static std::shared_ptr<UbseCliResultEcho> DisplayAllocDetail(const std::map<std::string, std::string> &params);
    // VM 模式新增:查询已直通 namespace 的连接信息
    static std::shared_ptr<UbseCliResultEcho> DisplayConnectInfo(const std::map<std::string, std::string> &params);
};
} // namespace ubse::cli::reg

#endif // UBSE_CLI_SSU_CMD_REG_H
