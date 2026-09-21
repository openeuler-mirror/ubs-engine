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

#include "ubse_vip_manager.h"

#include <regex>
#include <sstream>
#include <unistd.h>

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_net_util.h"
#include "ubse_os_util.h"
#include "ubse_cert_def.h"
#include "ubse_cert_validator.h"

namespace ubse::vip {
using namespace ubse::log;
using namespace ubse::utils;
using namespace ubse::cert;

namespace {
// VIP HTTP 服务使用的证书路径，由外部工具部署写入
constexpr const char *VIP_SERVER_CERT_FILE = "/var/lib/ubse/vip_server_cert/server.pem";
constexpr const char *VIP_TRUST_CERT_FILE = "/var/lib/ubse/vip_server_cert/trust.pem";
constexpr const char *VIP_CRL_FILE = "/var/lib/ubse/vip_server_cert/ca.crl";
constexpr const char *VIP_SERVER_KEY_FILE = "/var/lib/ubse/vip_server_cert/server_key.pem";
constexpr const char *VIP_PASSWORD_FILE = "/var/lib/ubse/vip_server_cert/key_pwd.txt";

// 容器模式 master 持续未收到注入时的延迟绑定次数阈值,超过后升级为 ERROR 告警
constexpr uint32_t kInjectionTimeoutDeferThreshold = 10;

// 网卡名合法字符白名单,InjectConfig 与 ValidateInterface 共用,避免热路径重复编译 NFA
const std::regex kIfacePattern("^[a-zA-Z0-9._-]+$");

UbseCertPaths MakeVipCertPaths()
{
    UbseCertPaths paths;
    paths.serverCertFile = VIP_SERVER_CERT_FILE;
    paths.trustCertFile = VIP_TRUST_CERT_FILE;
    paths.crlFile = VIP_CRL_FILE;
    paths.serverKeyFile = VIP_SERVER_KEY_FILE;
    paths.passwordFile = VIP_PASSWORD_FILE;
    return paths;
}

// 对 shell 命令参数进行转义，用单引号包裹防止 shell 解析元字符
std::string ShellEscape(const std::string &str)
{
    if (str.empty()) {
        return "''";
    }
    std::string result;
    result += '\'';
    for (char c : str) {
        if (c == '\'') {
            result += "'\\''";
        } else {
            result += c;
        }
    }
    result += '\'';
    return result;
}
} // namespace

UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseVipManager::Init(const UbseVipConfig &config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;

    // Init 是完整重新初始化:重置运行期状态,避免重复 Init(如进程内重载/测试)时
    // 残留的 configured_/active_ 等标志引发误绑定或误跳过延迟绑定分支
    configured_ = false;
    active_ = false;
    deferBindCount_ = 0;

    if (!config_.enable) {
        UBSE_LOG_INFO << "[VIP] VIP management is disabled";
        return UBSE_OK;
    }

    if (config_.containerMode) {
        // 容器模式二段式:先启动等待 UDS 注入,延迟的是 listenIp/网卡/证书的预校验;
        // 实际 BindVip 时(StartHttpServer useSsl)仍要求证书文件就绪。
        UBSE_LOG_INFO << "[VIP] container mode init, waiting for UDS injection";
        return UBSE_OK;
    }

    if (config_.listenIp.empty()) {
        UBSE_LOG_ERROR << "[VIP] vip.httpServer.listen.ip is not configured";
        return UBSE_ERROR;
    }

    auto ret = ParseListenIp();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[VIP] ParseListenIp failed";
        return ret;
    }

    if (!UbseNetUtil::ValidIpv4Addr(config_.address) || UbseNetUtil::IsSpecialIP(config_.address)) {
        UBSE_LOG_ERROR << "[VIP] Invalid VIP address: " << config_.address;
        return UBSE_ERROR;
    }

    if (config_.prefix == 0 || config_.prefix > 32) {
        UBSE_LOG_ERROR << "[VIP] Invalid prefix: " << config_.prefix;
        return UBSE_ERROR;
    }

    ret = ValidateInterface();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[VIP] ValidateInterface failed";
        return ret;
    }

    // VIP HTTP 服务走 TCP，需要校验证书是否就绪
    UbseSslValidator validator(MakeVipCertPaths());
    if (!validator.ValidateAll()) {
        UBSE_LOG_ERROR << "[VIP] Cert validation failed, please check cert files";
        return UBSE_ERROR;
    }

    // 启动时主动清理网卡上可能残留的 VIP，覆盖上次进程异常退出（crash/SIGKILL/断电等）
    // 导致 Deinit() 未执行而遗留的 VIP，避免重启后出现主备双节点同时持有 VIP 的脑裂问题。
    auto cleanupRet = ForceCleanup();
    if (cleanupRet != UBSE_OK) {
        UBSE_LOG_WARN << "[VIP] Stale VIP cleanup failed on Init, will retry on BindVip";
    }

    UBSE_LOG_INFO << "[VIP] Init success, vip=" << config_.address << "/" << config_.prefix
                  << ", listenPort=" << config_.listenPort;
    return UBSE_OK;
}

void UbseVipManager::Deinit()
{
    std::lock_guard<std::mutex> lock(mutex_);
    StopHttpServer();

    if (vipBound_) {
        if (UnbindVipL2() != UBSE_OK) {
            UBSE_LOG_WARN << "[VIP] UnbindVipL2 failed in Deinit, retrying with ForceCleanup";
            // 常规 del 失败时再尝试一次 ForceCleanup（先 show 再 del），尽量保证优雅退出时
            // 释放网卡上的 VIP，避免残留给下次启动带来脑裂风险。
            ForceCleanup();
        }
        vipBound_ = false;
    }
    UBSE_LOG_INFO << "[VIP] Deinit completed";
}

UbseResult UbseVipManager::BindVip()
{
    std::lock_guard<std::mutex> lock(mutex_);

    active_ = true;   // 记录 master 角色

    if (!config_.enable) {
        UBSE_LOG_DEBUG << "[VIP] VIP management is disabled, skip bind";
        return UBSE_OK;
    }

    // 容器模式 + 配置未注入 → 延迟绑定,不报错,避免选举 handler 返回错误;
    // 计数并在超过阈值后升级为 ERROR,让"缺 helper/注入永久未到达"的部署在监控上可见。
    if (config_.containerMode && !configured_) {
        ++deferBindCount_;
        UBSE_LOG_WARN << "[VIP] container mode: waiting for injection, defer bind (count=" << deferBindCount_ << ")";
        if (deferBindCount_ >= kInjectionTimeoutDeferThreshold) {
            UBSE_LOG_ERROR << "[VIP] VIP still unbound after " << deferBindCount_
                           << " defer attempts; check ubse-helper DaemonSet and UDS socket";
        }
        return UBSE_OK;
    }

    return BindVipLocked();
}

UbseResult UbseVipManager::BindVipLocked()
{
    if (vipBound_) {
        UBSE_LOG_WARN << "[VIP] VIP already bound, skip";
        return UBSE_OK;
    }

    auto cleanupRet = ForceCleanup();
    if (cleanupRet != UBSE_OK) {
        UBSE_LOG_WARN << "[VIP] ForceCleanup encountered errors, continuing with bind";
    }

    UBSE_LOG_INFO << "[VIP] Binding VIP " << config_.address << "/" << config_.prefix;

    auto ret = BindVipL2();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[VIP] BindVip failed";
        return ret;
    }

    auto httpRet = StartHttpServer();
    if (httpRet != UBSE_OK) {
        UBSE_LOG_ERROR << "[VIP] Start HTTP server failed after VIP bind, rolling back";
        UnbindVipL2();
        return UBSE_ERROR;
    }
    UBSE_LOG_INFO << "[VIP] HTTP server started successfully";

    vipBound_ = true;
    deferBindCount_ = 0;   // 绑定成功后清零,避免主备切换后误报 ERROR
    UBSE_LOG_INFO << "[VIP] VIP bound successfully";
    return UBSE_OK;
}

UbseResult UbseVipManager::UnbindVip()
{
    std::lock_guard<std::mutex> lock(mutex_);

    active_ = false;   // 记录降为 standby/agent

    if (!config_.enable) {
        UBSE_LOG_DEBUG << "[VIP] VIP management is disabled, skip unbind";
        return UBSE_OK;
    }

    // 不依赖内存标志位 vipBound_ 早退：进程重启后 vipBound_ 会重置为 false，但网卡上可能
    // 仍残留上次进程绑定的 VIP。始终执行一次解绑，DelIpAddress 已用 "2>/dev/null" 容错，
    // 删除不存在的 VIP 不会报错。
    StopHttpServer();

    UBSE_LOG_INFO << "[VIP] Unbinding VIP " << config_.address << "/" << config_.prefix
                  << ", vipBound_ was " << (vipBound_ ? "true" : "false");

    auto ret = UnbindVipL2();
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[VIP] UnbindVipL2 failed, HTTP server already stopped, resetting vipBound state";
    }

    vipBound_ = false;
    UBSE_LOG_INFO << "[VIP] VIP unbound successfully";
    return UBSE_OK;
}

bool UbseVipManager::IsVipBound() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return vipBound_;
}

UbseResult UbseVipManager::InjectConfig(uint32_t addr, uint16_t port, uint8_t prefix, const std::string &iface)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!config_.containerMode) {
        UBSE_LOG_ERROR << "[VIP] InjectConfig rejected: not container mode";
        return UBSE_ERR_INVALID_ARG;
    }

    // 字段校验:handler 负责 payload 结构(长度/空指针),本函数负责数值范围与业务合法性
    std::string newAddr = UbseNetUtil::IntToIpV4(addr);
    if (addr == 0 || !UbseNetUtil::ValidIpv4Addr(newAddr) || UbseNetUtil::IsSpecialIP(newAddr)) {
        UBSE_LOG_ERROR << "[VIP] InjectConfig invalid addr: " << newAddr;
        return UBSE_ERR_INVALID_ARG;
    }
    if (port < 1024 || port > 65535) {
        UBSE_LOG_ERROR << "[VIP] InjectConfig invalid port: " << port;
        return UBSE_ERR_INVALID_ARG;
    }
    if (prefix == 0 || prefix > 32) {
        UBSE_LOG_ERROR << "[VIP] InjectConfig invalid prefix: " << static_cast<uint32_t>(prefix);
        return UBSE_ERR_INVALID_ARG;
    }
    if (iface.empty() || iface.size() > 15 || !std::regex_match(iface, kIfacePattern)) {
        UBSE_LOG_ERROR << "[VIP] InjectConfig invalid iface: " << iface;
        return UBSE_ERR_INVALID_ARG;
    }

    configured_ = true;

    // 变更检测
    bool changed = (newAddr != config_.address) || (port != config_.listenPort) ||
                   (prefix != config_.prefix) || (iface != config_.interface);
    if (!changed) {
        // 心跳重推:配置未变但 master 尚未绑定(此前 bind 失败/注入丢失)→ 补 bind
        if (active_ && !vipBound_) {
            UBSE_LOG_INFO << "[VIP] config unchanged, master re-bind (heartbeat recovery)";
            return BindVipLocked();
        }
        UBSE_LOG_INFO << "[VIP] config unchanged, no-op";
        return UBSE_OK;
    }

    // 落地新配置
    config_.address = std::move(newAddr);
    config_.listenPort = port;
    config_.prefix = prefix;
    config_.interface = iface;
    UBSE_LOG_INFO << "[VIP] config injected: " << config_.address << "/" << config_.prefix
                  << ":" << config_.listenPort << " @" << config_.interface;

    // 非 master 仅保存
    if (!active_) {
        UBSE_LOG_INFO << "[VIP] not master, config stored only";
        return UBSE_OK;
    }

    // master 热更新:先解绑旧 VIP 再绑新 VIP
    if (vipBound_) {
        StopHttpServer();
        UnbindVipL2();
        vipBound_ = false;
    }
    return BindVipLocked();
}

UbseResult UbseVipManager::StartHttpServer()
{
    if (httpServer_) {
        UBSE_LOG_WARN << "[VIP] HTTP server already running";
        return UBSE_OK;
    }

    try {
        UbseHttpServer::Config httpConfig;
        httpConfig.name = "VipManager";
        httpConfig.useUds = false;
        httpConfig.useSsl = true;
        httpConfig.listenAddr = config_.address;
        httpConfig.port = config_.listenPort;
        httpConfig.udsPath = "";
        httpConfig.certPaths = MakeVipCertPaths();

        // 端口限流配置：来自 vip.httpServer.rateLimitRps，0=不限流
        httpConfig.rateLimitRps = config_.rateLimitRps;

        // 北向 HTTP 等待队列上限：来自 vip.httpServer.maxQueuedRequests，0=不限制
        httpConfig.maxQueuedRequests = config_.maxQueuedRequests;

        httpServer_ = std::make_unique<UbseHttpServer>(httpConfig);

        // Register all pending routes
        for (const auto &route : pendingRoutes_) {
            httpServer_->RegisterRoute(route.path, UbseHttpMethodToString(route.method), route.handler);
        }

        if (httpServer_->Start()) {
            UBSE_LOG_INFO << "[VIP] HTTP server started on " << config_.address << ":" << config_.listenPort;
            return UBSE_OK;
        } else {
            UBSE_LOG_ERROR << "[VIP] Failed to start HTTP server";
            httpServer_.reset();
            return UBSE_ERROR;
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "[VIP] Exception starting HTTP server: " << e.what();
        httpServer_.reset();
        return UBSE_ERROR;
    }
}

UbseResult UbseVipManager::StopHttpServer()
{
    if (httpServer_) {
        httpServer_->Stop();
        httpServer_.reset();
        UBSE_LOG_INFO << "[VIP] HTTP server stopped";
    }
    return UBSE_OK;
}

UbseResult UbseVipManager::ParseListenIp()
{
    auto pos = config_.listenIp.find('/');
    if (pos == std::string::npos) {
        UBSE_LOG_ERROR << "[VIP] Invalid CIDR format: " << config_.listenIp << ", expected <ip>/<prefix>";
        return UBSE_ERROR;
    }

    config_.address = config_.listenIp.substr(0, pos);
    std::string prefixStr = config_.listenIp.substr(pos + 1);
    if (prefixStr.empty()) {
        UBSE_LOG_ERROR << "[VIP] Empty prefix in CIDR: " << config_.listenIp;
        return UBSE_ERROR;
    }

    try {
        config_.prefix = static_cast<uint32_t>(std::stoul(prefixStr));
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "[VIP] Invalid prefix in CIDR: " << config_.listenIp << ", " << e.what();
        return UBSE_ERROR;
    }

    UBSE_LOG_INFO << "[VIP] Parsed listenIp: address=" << config_.address << ", prefix=" << config_.prefix;
    return UBSE_OK;
}

void UbseVipManager::RegisterRoute(const std::string &path, UbseHttpMethod method, UbseHttpHandlerFunc handler)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // 始终存入 pendingRoutes_，保证主备切换后重建 httpServer_ 时能重新注册全部路由
    pendingRoutes_.push_back({path, method, handler});

    // 若 HTTP server 已在运行（VIP 已 bound），直接注册到运行实例，避免路由延迟到下次绑定才生效
    if (httpServer_) {
        httpServer_->RegisterRoute(path, UbseHttpMethodToString(method), handler);
    }
    UBSE_LOG_INFO << "[VIP] Route registered: " << path << ", httpServerRunning=" << (httpServer_ != nullptr);
}

UbseResult UbseVipManager::ValidateInterface()
{
    if (config_.interface.empty()) {
        UBSE_LOG_ERROR << "[VIP] vip.iface is not configured";
        return UBSE_ERROR;
    }

    // 校验接口名仅包含合法字符，防止命令注入
    if (!std::regex_match(config_.interface, kIfacePattern)) {
        UBSE_LOG_ERROR << "[VIP] Invalid interface name: " << config_.interface;
        return UBSE_ERROR;
    }

    // 与内核IFNAMSIZ-1对齐 超长网卡名启动即失败
    if (config_.interface.size() > 15) {
        UBSE_LOG_ERROR << "[VIP] Interface name exceeds 15 chars: " << config_.interface;
        return UBSE_ERROR;
    }

    UBSE_LOG_INFO << "[VIP] Interface from config: " << config_.interface;
    return UBSE_OK;
}

UbseResult UbseVipManager::ForceCleanup()
{
    std::ostringstream checkCmd;
    checkCmd << "ip addr show " << ShellEscape(config_.interface) << " | grep -w " << ShellEscape(config_.address);
    std::string result;
    if (UbseOsUtil::Exec(checkCmd.str(), result) == UBSE_OK && !result.empty()) {
        UBSE_LOG_WARN << "[VIP] Stale VIP found on " << config_.interface << ", cleaning up";
        auto delRet = DelIpAddress(config_.interface);
        if (delRet != UBSE_OK) {
            UBSE_LOG_ERROR << "[VIP] Failed to clean up stale VIP on " << config_.interface;
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

UbseResult UbseVipManager::BindVipL2()
{
    auto ret = AddIpAddress(config_.interface);
    if (ret != UBSE_OK) {
        return ret;
    }

    ret = SendGratuitousArp();
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[VIP] Gratuitous ARP failed, but VIP is bound";
    }

    return UBSE_OK;
}

UbseResult UbseVipManager::UnbindVipL2()
{
    return DelIpAddress(config_.interface);
}

UbseResult UbseVipManager::AddIpAddress(const std::string &iface)
{
    std::ostringstream cmd;
    cmd << "ip addr add " << ShellEscape(config_.address) << "/" << config_.prefix << " dev " << ShellEscape(iface);

    std::string result;
    auto ret = UbseOsUtil::Exec(cmd.str(), result);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[VIP] Add IP address failed, cmd=" << cmd.str() << ", result=" << result;
        return UBSE_ERROR;
    }

    UBSE_LOG_INFO << "[VIP] Add IP address success, cmd=" << cmd.str();
    return UBSE_OK;
}

UbseResult UbseVipManager::DelIpAddress(const std::string &iface)
{
    std::ostringstream cmd;
    cmd << "ip addr del " << ShellEscape(config_.address) << "/" << config_.prefix << " dev " << ShellEscape(iface)
        << " 2>/dev/null";

    std::string result;
    auto ret = UbseOsUtil::Exec(cmd.str(), result);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "[VIP] Delete IP address failed, cmd=" << cmd.str() << ", result=" << result;
        return UBSE_ERROR;
    }

    UBSE_LOG_INFO << "[VIP] Delete IP address success, cmd=" << cmd.str();
    return UBSE_OK;
}

UbseResult UbseVipManager::SendGratuitousArp()
{
    bool anySuccess = false;
    for (uint32_t i = 0; i < config_.arpCount; ++i) {
        std::ostringstream cmd;
        cmd << "arping -c 1 -A -I " << ShellEscape(config_.interface) << " " << ShellEscape(config_.address);

        std::string result;
        auto ret = UbseOsUtil::Exec(cmd.str(), result);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "[VIP] Send gratuitous ARP failed, attempt " << (i + 1) << "/" << config_.arpCount
                          << ", result=" << result;
        } else {
            anySuccess = true;
            UBSE_LOG_INFO << "[VIP] Send gratuitous ARP success, attempt " << (i + 1) << "/" << config_.arpCount;
        }

        if (i < config_.arpCount - 1 && config_.arpInterval > 0) {
            usleep(config_.arpInterval * 1000);
        }
    }

    if (!anySuccess) {
        UBSE_LOG_ERROR << "[VIP] All gratuitous ARP attempts failed";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult RegVipHttpService(UbseHttpMethod method, const std::string &url, UbseHttpHandlerFunc func)
{
    UbseVipManager::GetInstance().RegisterRoute(url, method, std::move(func));
    return UBSE_OK;
}

} // namespace ubse::vip