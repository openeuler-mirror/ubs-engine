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
#include "ubse_http_server.h"

#include <arpa/inet.h>
#include <cerrno>
#include <grp.h>
#include <securec.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "adapter_plugins/mti/ubse_topology_interface.h"
#include "httplib.h"
#include "src/framework/misc/ubse_secure_buffer.h"
#include "src/include/cert/ubse_cert_def.h"
#include "src/include/cert/ubse_cert_validator.h"
#include "ubse_common_def.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_file_util.h"
#include "ubse_http_common.h"
#include "ubse_logger.h"
#include "ubse_net_util.h"
#include "ubse_security_module.h"

namespace ubse::http {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::common::def;
using namespace ubse::log;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::utils;
using namespace ubse::security;

constexpr uint32_t START_TIMEOUT = 3000;
constexpr uint32_t SLEEP_TIME = 100;
constexpr mode_t FOLDER_PERMISSION = 0755;

bool SetSocketFilePermission(const std::string &udsAddress)
{
    uint32_t retryTime = 0;
    while (access(udsAddress.c_str(), F_OK) == -1 && retryTime++ < START_TIMEOUT) {
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
        retryTime += SLEEP_TIME;
    }

    struct group *grp = getgrnam(UBM_GROUP.c_str());
    if (!grp) {
        UBSE_LOG_ERROR << "Group " << UBM_GROUP << " not found.";
        return false;
    }
    gid_t target_gid = grp->gr_gid;

    // 设置文件组
    if (chown(udsAddress.c_str(), -1, target_gid) != 0) { // -1 表示不修改所有者
        UBSE_LOG_ERROR << "chown failed: " << strerror(errno);
        return false;
    }

    // 设置文件权限为 0660
    constexpr mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP; // 0660
    if (chmod(udsAddress.c_str(), mode) != 0) {
        UBSE_LOG_ERROR << "chmod socket fail: " << strerror(errno);
        return false;
    }

    UBSE_LOG_DEBUG << "Set socket file permission and group success.";
    return true;
}

UbseHttpServer::UbseHttpServer(const Config &config)
    : config_(config), running_(false)
{
}

UbseHttpServer::~UbseHttpServer()
{
    Stop();
}

bool UbseHttpServer::Start()
{
    try {
        if (config_.useUds) {
            serverThread_ = std::thread(&UbseHttpServer::UdsRun, this);
        } else {
            serverThread_ = std::thread(&UbseHttpServer::TcpRun, this);
        }
    } catch (const std::system_error &) {
        UBSE_LOG_ERROR << "Failed to create thread for " << config_.name;
        return false;
    }
    uint32_t retryTime = 0;
    while (retryTime <= START_TIMEOUT) {
        if (server_ && server_->is_running()) {
            return !config_.useUds || SetSocketFilePermission(config_.udsPath);
        }
        retryTime += SLEEP_TIME;
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
    }
    return server_ && server_->is_running();
}

void UbseHttpServer::Stop()
{
    if (server_) {
        server_->stop();
    }
    if (serverThread_.joinable()) {
        serverThread_.join();
    }

    if (config_.useUds && !config_.udsPath.empty()) {
        if (unlink(config_.udsPath.c_str()) != 0 && errno != ENOENT) {
            UBSE_LOG_ERROR << "Failed to delete UDS socket file: " << strerror(errno);
        }
    }
}

static void ProcessRequestHeadersAndParams(const httplib::Request &req, UbseHttpRequest &request)
{
    for (const auto &pair : req.headers) {
        if (request.headers.find(pair.first) == request.headers.end()) {
            request.headers.emplace(pair.first, pair.second);
        }
    }

    for (const auto &pair : req.params) {
        if (request.params.find(pair.first) == request.params.end()) {
            request.params.emplace(pair.first, pair.second);
        }
    }
}

// Extract peer certificate info from the mTLS handshake and fill request.peerCert.
// present is set to true only when a client certificate was actually provided.
static void FillPeerCertInfo(const httplib::Request &req, UbseHttpRequest &request)
{
#ifdef CPPHTTPLIB_SSL_ENABLED
    if (req.ssl == nullptr) {
        return;  // non-HTTPS request, leave peerCert default (present=false)
    }
    // tls::const_session_t is `const void*` pointing to the OpenSSL SSL object
    SSL *ssl = const_cast<SSL *>(static_cast<const SSL *>(req.ssl));
    X509 *cert = SSL_get1_peer_certificate(ssl);
    if (cert == nullptr) {
        return;  // client did not present a certificate
    }
    X509_NAME *name = X509_get_subject_name(cert);
    char buf[256] = {0};
    if (X509_NAME_get_text_by_NID(name, NID_commonName, buf, sizeof(buf)) > 0) {
        request.peerCert.cn = buf;
    }
    memset_s(buf, sizeof(buf), 0, sizeof(buf));
    if (X509_NAME_get_text_by_NID(name, NID_organizationalUnitName, buf, sizeof(buf)) > 0) {
        request.peerCert.ou = buf;
    }
    X509_free(cert);
    request.peerCert.present = true;
#else
    (void)req;
    (void)request;
#endif
}

std::string UbseHttpServer::GenerateQueryString(const std::multimap<std::string, std::string> &queryParams)
{
    std::string queryString;
    bool first = true;
    for (const auto &param : queryParams) {
        if (!first) {
            queryString.append("&");
        }
        first = false;
        queryString.append(param.first);
        queryString.append("=");
        queryString.append(param.second);
    }
    return queryString;
}

UbseResult UbseHttpServer::ValidateHttpRequest(const httplib::Request &req, UbseHttpRequest &request)
{
    if (!req.params.empty()) {
        std::string queryStr = GenerateQueryString(req.params);
        if (queryStr.size() > httpMaxQuerySize) {
            UBSE_LOG_ERROR << "HttpMsg queryParams is oversize";
            return UBSE_HTTP_ERROR_MSG_OVERSIZE;
        }
    }
    if (!req.body.empty() && req.body.size() > httpMaxBodySize) {
        UBSE_LOG_ERROR << "HttpRequest BodyLen is oversize";
        return UBSE_HTTP_ERROR_MSG_OVERSIZE;
    }
    request.method = req.method;
    if (StringToUbseHttpMethod(request.method) == UbseHttpMethod::UBSE_HTTP_METHOD_INVALID) {
        UBSE_LOG_ERROR << "The request method is not support";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseHttpServer::HandleRequest(const httplib::Request &req, httplib::Response &res)
{
    UBSE_LOG_INFO << "[" << config_.name << "] Receive request, uri=" << req.path << ", method=" << req.method;
    UbseHttpRequest request{};
    if (ValidateHttpRequest(req, request) != UBSE_OK) {
        res.status = BadRequest_400;
        res.set_content("The request is invalid.", "text/plain");
        return;
    }
    request.path = req.path;
    ProcessRequestHeadersAndParams(req, request);
    FillPeerCertInfo(req, request);
    UbseHttpResponse response{};
    std::string routeKey = req.method + req.path;
    // 与 RegisterRoute 使用同一把锁，避免并发遍历与插入导致迭代器失效或数据损坏。
    // 仅保护路由查找阶段，找到 handler 后释放锁再执行，避免长耗时handler序列化所有请求。
    UbseHttpHandlerFunc handler;
    {
        std::lock_guard<std::mutex> lock(routesMutex_);
        auto it = routes_.find(routeKey);
        if (it == routes_.end()) {
            // 前缀匹配兜底：处理动态路径参数路由（如 DELETE /ubse/v1/ssu/spaces/{name}）
            // 以'/'结尾的注册路由作为前缀路由，请求路径在前缀之后有附加内容时匹配命中
            std::string bestMatchKey;
            for (const auto &route : routes_) {
                const std::string &key = route.first;
                if (key.empty() || key.back() != '/') {
                    continue;
                }
                if (routeKey.size() > key.size() && routeKey.compare(0, key.size(), key) == 0) {
                    if (key.size() > bestMatchKey.size()) {
                        bestMatchKey = key;
                    }
                }
            }
            if (!bestMatchKey.empty()) {
                it = routes_.find(bestMatchKey);
            }
        }
        if (it == routes_.end()) {
            UBSE_LOG_ERROR << "[" << config_.name << "] url=" << req.path << " has not been registered.";
            res.status = NotFound_404;
            res.set_content("Not Found", "text/plain");
            return;
        }
        handler = it->second;
    }
    handler(request, response);
    BuildResponse(res, response);
}

void UbseHttpServer::BuildResponse(httplib::Response &res, const UbseHttpResponse &response)
{
    res.status = response.status;

    for (const auto &header : response.headers) {
        res.set_header(header.first, header.second);
    }

    res.body = response.body;
}

void UbseHttpServer::RegisterRoute(const std::string &path, const std::string &method, UbseHttpHandlerFunc handler)
{
    std::string routeKey = method + path;
    std::lock_guard<std::mutex> lock(routesMutex_);
    auto it = routes_.find(routeKey);
    if (it != routes_.end()) {
        UBSE_LOG_WARN << "[" << config_.name << "] url=" << path << " has already been registered.";
        return;
    }
    routes_[routeKey] = handler;
    UBSE_LOG_INFO << "[" << config_.name << "] register method= " << method << ", url= " << path;
}

std::unique_ptr<httplib::SSLServer> UbseHttpServer::CreateSslServer()
{
    cert::UbseSslValidator validator(config_.certPaths);
    password = validator.LoadPassword();
    if (password.size() == 0) {
        UBSE_LOG_ERROR << "[" << config_.name << "] Private key password not found.";
        return nullptr;
    }
    auto sslServer = std::make_unique<httplib::SSLServer>(config_.certPaths.serverCertFile.c_str(),
                                                          config_.certPaths.serverKeyFile.c_str(),
                                                          config_.certPaths.trustCertFile.c_str(), nullptr,
                                                          password.c_str());
    if (!sslServer || !sslServer->is_valid()) {
        UBSE_LOG_ERROR << "[" << config_.name << "] Failed to initialize SSL server.";
        return nullptr;
    }
    // 配置客户端证书验证（mTLS）
    SSL_CTX* ctx = static_cast<SSL_CTX*>(sslServer->tls_context());
    if (SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION) != 1) {
        // 设置失败，可能是 OpenSSL 版本过低或不支持 TLS 1.3
        UBSE_LOG_ERROR << "[" << config_.name << "] Failed to set min protocol version: TLS1_3_VERSION";
        return nullptr;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
    // 配置证书吊销列表（CRL）验证
    if (!validator.ConfigureCrlValidation(ctx)) {
        UBSE_LOG_ERROR << "[" << config_.name << "] Failed to configure CRL validation for server";
        return nullptr;
    }
    sslServer->new_task_queue = []() -> httplib::ThreadPool* {
        return new httplib::ThreadPool(NO_1, NO_8);
    };
    return sslServer;
}

void UbseHttpServer::TcpRun()
{
    try {
        auto sslServer = CreateSslServer();
        if (!sslServer) {
            throw std::runtime_error("Failed to create SSL server.");
        }

        sslServer->Get("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        sslServer->Post("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        sslServer->Put("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        sslServer->Delete("/.*",
                          [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        sslServer->Patch("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });

        UBSE_LOG_INFO << "[" << config_.name << "] TCP server start listening on addr=" << config_.listenAddr
                      << ", port=" << config_.port;

        server_ = std::move(sslServer);
        server_->listen(config_.listenAddr, config_.port);
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "[" << config_.name << "] Error starting server=" << e.what();
    }
}

void UbseHttpServer::UdsRun()
{
    try {
        server_ = std::make_unique<Server>();
        server_->new_task_queue = []() -> httplib::ThreadPool* {
            return new httplib::ThreadPool(NO_1, NO_8);
        };
        if (!server_) {
            throw std::runtime_error("Failed to create server.");
        }
        server_->Get("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server_->Post("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server_->Put("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server_->Delete("/.*",
                        [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server_->Patch("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });

        std::string udsAddress = config_.udsPath.empty() ? UBSE_UBM_UDS_ADDRESS : config_.udsPath;

        const auto udsParentPath = GetParentDirectory(udsAddress);
        std::vector<__u32> caps = {CAP_DAC_OVERRIDE};
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
        if (UbseFileUtil::CreateAndChmodDirectory(udsParentPath, FOLDER_PERMISSION) != UBSE_OK) {
            UBSE_LOG_ERROR << "[" << config_.name << "] Uds file directory= " << udsParentPath << " is unavailable.";
            UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
            return;
        }
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);

        auto res = unlink(udsAddress.c_str());
        if (res != 0 && errno != ENOENT) {
            UBSE_LOG_ERROR << "[" << config_.name << "] unlink uds file fail, errno= " << errno;
            return;
        }

        bool ret = server_->set_address_family(AF_UNIX).listen(udsAddress, 80);
        if (!ret) {
            UBSE_LOG_ERROR << "[" << config_.name << "] start server fail";
            return;
        }
        // listen接口为阻塞接口，当服务停止，将打印此日志
        UBSE_LOG_INFO << "[" << config_.name << "] uds server stopped";
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "[" << config_.name << "] ERROR starting server= " << e.what();
    }
}

std::string UbseHttpServer::GetParentDirectory(const std::string &path)
{
    if (path.empty()) {
        return "";
    }
    size_t pos = path.find_last_of('/');
    return (pos == std::string::npos) ? "" : path.substr(0, pos);
}
} // namespace ubse::http