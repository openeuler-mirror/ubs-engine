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

#ifndef UBSE_SSU_CONTROLLER_MODULE_H
#define UBSE_SSU_CONTROLLER_MODULE_H

#include <memory>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_module.h"
#include "ubse_ssu_service.h"
#include "ubse_ssu_vm_cache.h"
#include "src/framework/vscok/ubse_vsock_server.h"
#include "ubse_thread_pool.h"

namespace ubse::ssu::controller {

using namespace ubse::common::def;
using ubse::plugin::service::ssu::UbseSsuService;
using ubse::vsock::UbseVsockServer;

// SSU 模块配置段与 vm_mode 配置项名称
constexpr const char *UBSE_SSU_CONF_SECTION = "ubse.ssu";
constexpr const char *UBSE_SSU_CONF_VM_MODE_KEY = "vm_mode";
constexpr const char *UBSE_SSU_CONF_VSOCK_PORT_KEY = "vsockServerPort";
constexpr const char *UBSE_SSU_CONF_ENABLE_TLS_KEY = "enable_tls";
// VM 模式缓存文件固定路径,不支持配置(证书目录固定为 UbseVsockConfig 默认值,同样不支持配置)
constexpr const char *UBSE_SSU_DEFAULT_CACHE_FILE = "/var/lib/ubse/ssu_vm_cache.json";

// vsock 模块码:与 IPC ubse_ipc_module_code_t::UBSE_SSU=0x0007 对齐
// (vsock server RegisterHandler 接受 uint16_t,无需引入 C enum 头文件)
constexpr uint16_t UBSE_VSOCK_SSU_MODULE_CODE = 0x0007;

// 插件依赖在模块 Initialize 之前解析，因此须在插件注册阶段按 vm_mode 选择。
// 配置读取失败时按默认 host 模式返回完整依赖，保持现有启动行为。
std::vector<ubse::module::UbseOptionModule> GetSsuModuleDependencies();

class UbseSsuControllerModule : public ubse::module::UbseModule {
public:
    static constexpr const char *kModuleName = "UbseSsuControllerModule";
    std::string Name() const override { return kModuleName; }

    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

private:
    // 复用 host 模式现有成员,VM 模式分支不修改其语义
    std::shared_ptr<UbseSsuService> ssuService_;

    // VM 模式新增成员(host 模式下保持为 nullptr,不初始化)
    // 仅按照 UbseVsockServer / UbseSsuVmCache 公开接口调用,不修改二者实现
    std::unique_ptr<UbseVsockServer> vsockServer_;
    std::unique_ptr<UbseSsuVmCache> vmCache_;
    // VM 模式异步落盘线程池(单条推送触发,异步 FlushToDisk 不阻塞 ACK)
    ubse::task_executor::UbseTaskExecutorPtr vmFlushExecutor_{};
    bool vmFlushExecutorCreated_{false};
    bool vmMode_{false};

    // VM 模式初始化分支:读取 vm_mode 配置,创建 vmCache_ 与 vsockServer_
    UbseResult InitializeVmMode();

    // VM 模式启动分支:注册 IPC 子集、启动 vsockServer_
    UbseResult StartVmMode();

    // VM 模式停止分支:停止 vsockServer_、最终落盘 vmCache_
    void StopVmMode();

    // 释放 VM 模式初始化状态；可用于正常反初始化及 InitializeVmMode 失败回滚
    void ResetVmModeState();

    // 读取 vm_mode 配置(单次读取,后续只读)
    UbseResult ReadVmModeConfig(bool &vmMode);

    // 构造 vsock 配置(含 enableTls/certDir/port)
    ubse::vsock::UbseVsockConfig BuildVsockConfig();

    // 注册 VM 侧 vsock 推送 handler(分配信息/连接信息/全量同步)
    UbseResult RegisterVsockHandlers();

    // vsock 推送处理函数(由 vsockServer_ 在线程池中回调)
    uint32_t HandlePushAllocInfo(const ubse::vsock::UbseIpcMessage &request,
                                 const ubse::vsock::UbseRequestContext &context);
    uint32_t HandlePushConnectInfo(const ubse::vsock::UbseIpcMessage &request,
                                   const ubse::vsock::UbseRequestContext &context);
    uint32_t HandleFullSync(const ubse::vsock::UbseIpcMessage &request,
                            const ubse::vsock::UbseRequestContext &context);
};

} // namespace ubse::ssu::controller

#endif // UBSE_SSU_CONTROLLER_MODULE_H
