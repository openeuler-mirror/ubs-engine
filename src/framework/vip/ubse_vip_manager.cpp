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

#include <fstream>
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

    if (!config_.enable) {
        UBSE_LOG_INFO << "[VIP] VIP management is disabled";
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

    if (!UbseNetUtil::ValidIpv4Addr(config_.address)) {
        UBSE_LOG_ERROR << "[VIP] Invalid VIP address: " << config_.address;
        return UBSE_ERROR;
    }

    if (config_.prefix == 0 || config_.prefix > 32) {
        UBSE_LOG_ERROR << "[VIP] Invalid prefix: " << config_.prefix;
        return UBSE_ERROR;
    }

    ret = ResolveInterface();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[VIP] ResolveInterface failed";
        return ret;
    }

    // VIP HTTP 服务走 TCP，需要校验证书是否就绪
    UbseSslValidator validator(MakeVipCertPaths());
    if (!validator.ValidateAll()) {
        UBSE_LOG_ERROR << "[VIP] Cert validation failed, please check cert files";
        return UBSE_ERROR;
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
        UnbindVipL2();
        vipBound_ = false;
    }
    UBSE_LOG_INFO << "[VIP] Deinit completed";
}

UbseResult UbseVipManager::BindVip()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!config_.enable) {
        UBSE_LOG_DEBUG << "[VIP] VIP management is disabled, skip bind";
        return UBSE_OK;
    }

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
    UBSE_LOG_INFO << "[VIP] VIP bound successfully";
    return UBSE_OK;
}

UbseResult UbseVipManager::UnbindVip()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!config_.enable) {
        UBSE_LOG_DEBUG << "[VIP] VIP management is disabled, skip unbind";
        return UBSE_OK;
    }

    if (!vipBound_) {
        UBSE_LOG_WARN << "[VIP] VIP not bound, skip unbind";
        return UBSE_OK;
    }

    StopHttpServer();

    UBSE_LOG_INFO << "[VIP] Unbinding VIP " << config_.address << "/" << config_.prefix;

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
    pendingRoutes_.push_back({path, method, handler});
    UBSE_LOG_INFO << "[VIP] Route registered: " << path;
}

UbseResult UbseVipManager::ResolveInterface()
{
    std::ifstream ifs(kIfaceFilePath);
    if (!ifs.is_open()) {
        UBSE_LOG_ERROR << "[VIP] Cannot open iface file: " << kIfaceFilePath;
        return UBSE_ERROR;
    }

    std::string iface;
    std::getline(ifs, iface);

    auto trimPos = iface.find_last_not_of(" \t\r\n");
    if (trimPos != std::string::npos) {
        iface = iface.substr(0, trimPos + 1);
    }

    if (iface.empty()) {
        UBSE_LOG_ERROR << "[VIP] Iface file is empty: " << kIfaceFilePath;
        return UBSE_ERROR;
    }

    // 校验接口名仅包含合法字符，防止命令注入
    std::regex ifacePattern("^[a-zA-Z0-9._-]+$");
    if (!std::regex_match(iface, ifacePattern)) {
        UBSE_LOG_ERROR << "[VIP] Invalid interface name: " << iface;
        return UBSE_ERROR;
    }

    config_.interface = iface;
    UBSE_LOG_INFO << "[VIP] Interface resolved from " << kIfaceFilePath << ": " << config_.interface;
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