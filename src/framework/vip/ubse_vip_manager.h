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

#ifndef UBSE_VIP_MANAGER_H
#define UBSE_VIP_MANAGER_H

#include <cstdint>
#include <string>
#include <mutex>
#include <memory>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_http_server.h"

namespace ubse::vip {
using namespace ubse::common::def;
using namespace ubse::http;

struct UbseVipConfig {
    bool enable{false};
    std::string listenIp;          // CIDR format from config: 192.168.100.200/24
    uint32_t listenPort{10002};    // HTTP server listening port
    uint32_t arpCount{5};
    uint32_t arpInterval{200};

    // Parsed from listenIp
    std::string address;           // e.g. 192.168.100.200
    uint32_t prefix{24};           // e.g. 24
    std::string interface;         // resolved from kIfaceFilePath

    // 容器模式:enable=true 且缺 listenIp,配置经 UDS 由 helper 注入
    bool containerMode{false};
};

// Fixed path for interface file written by K8s helper container
constexpr const char *kIfaceFilePath = "/var/run/ubse/ubse_iface";

class UbseVipManager {
public:
    static UbseVipManager &GetInstance()
    {
        static UbseVipManager instance;
        return instance;
    }

    UbseVipManager(const UbseVipManager &) = delete;
    UbseVipManager &operator=(const UbseVipManager &) = delete;

    UbseResult Init(const UbseVipConfig &config);
    void Deinit();

    UbseResult BindVip();
    UbseResult UnbindVip();

    bool IsVipBound() const;

    // 容器模式:helper 经 UDS 注入 VIP 配置(addr/port/prefix/iface),驱动延迟绑定/热更新
    UbseResult InjectConfig(uint32_t addr, uint16_t port, uint8_t prefix, const std::string &iface);

    // 按值返回并在锁内拷贝,避免与 InjectConfig 的加锁写入(含 std::string move)构成数据竞态
    UbseVipConfig GetConfig() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

    // Register an HTTP route on the VIP HTTP server. Routes are stored and applied when the server starts.
    // If the server is already running, the route is also registered directly.
    void RegisterRoute(const std::string &path, UbseHttpMethod method, UbseHttpHandlerFunc handler);

private:
    UbseVipManager() = default;
    ~UbseVipManager() = default;

    UbseResult ResolveInterface();
    UbseResult ParseListenIp();

    UbseResult BindVipL2();
    UbseResult UnbindVipL2();

    UbseResult StartHttpServer();
    UbseResult StopHttpServer();

    UbseResult ForceCleanup();
    UbseResult AddIpAddress(const std::string &iface);
    UbseResult DelIpAddress(const std::string &iface);
    UbseResult SendGratuitousArp();

    // 无锁核心:ForceCleanup + BindVipL2 + StartHttpServer + vipBound_=true,调用方须持锁
    UbseResult BindVipLocked();

    UbseVipConfig config_;
    bool vipBound_{false};
    bool configured_{false};   // 容器模式:是否已收到合法注入
    bool active_{false};       // 是否处于 master 状态
    mutable std::mutex mutex_;
    std::unique_ptr<UbseHttpServer> httpServer_;
    uint32_t deferBindCount_{0};   // 容器模式 master 延迟绑定计数(注入未到达),用于超时告警

    struct VipRoute {
        std::string path;
        UbseHttpMethod method;
        UbseHttpHandlerFunc handler;
    };
    std::vector<VipRoute> pendingRoutes_;
};

} // namespace ubse::vip

#endif // UBSE_VIP_MANAGER_H