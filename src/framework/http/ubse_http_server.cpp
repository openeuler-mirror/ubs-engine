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

#include <grp.h>
#include <securec.h>

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

std::unique_ptr<UbseHttpServer> UbseHttpServer::instance_ = nullptr;
std::once_flag UbseHttpServer::initInstanceFlag_;
constexpr uint32_t START_TIMEOUT = 3000;
constexpr uint32_t SLEEP_TIME = 100;
constexpr mode_t FOLDER_PEUBSEISSION = 0755;

bool SetSocketFilePermission()
{
    // 重置 uds socket 文件权限为 660，防止不同环境权限不一致
    std::string udsAddress = UBSE_UBM_UDS_ADDRESS;
    uint32_t retryTime = 0;
    while (access(udsAddress.c_str(), F_OK) == -1 && retryTime++ < START_TIMEOUT) {
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME)); // 当文件不存在，等待100ms
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

bool UbseHttpServer::Start(bool isTcpServer)
{
    try {
        if (isTcpServer) {
            serverThread_ = std::thread(&UbseHttpServer::TcpRun, this);
        } else {
            serverThread_ = std::thread(&UbseHttpServer::UdsRun, this);
        }
    } catch (const std::system_error &) {
        UBSE_LOG_ERROR << "Failed to create thread.";
        return false;
    }
    uint32_t retryTime = 0;
    while (retryTime <= START_TIMEOUT) {
        if (server_ && server_->is_running()) {
            return isTcpServer || SetSocketFilePermission();
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

    // 删除 UDS socket 文件
    std::string udsAddress = UBSE_UBM_UDS_ADDRESS;
    if (unlink(udsAddress.c_str()) != 0 && errno != ENOENT) {
        UBSE_LOG_ERROR << "Failed to delete UDS socket file: " << strerror(errno);
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

std::string UbseHttpServer::GenerateQueryString(const std::multimap<std::string, std::string> &queryParams)
{
    std::string queryString;
    bool first = true;
    for (const auto &param : queryParams) {
        if (!first) {
            queryString.append("&"); // 添加分隔符
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
    UBSE_LOG_INFO << "Receive request, uri=" << req.path << ", method=" << req.method;
    UbseHttpRequest request{};
    if (ValidateHttpRequest(req, request) != UBSE_OK) {
        res.status = BadRequest_400;
        res.set_content("The request is invalid.", "text/plain");
        return;
    }
    request.path = req.path;
    ProcessRequestHeadersAndParams(req, request);
    UbseHttpResponse response{};
    std::string routeKey = req.method + req.path;
    auto it = routes_.find(routeKey);
    if (it == routes_.end()) {
        UBSE_LOG_ERROR << "url=" << req.path << "has not been registered in tcp server.";
        res.status = NotFound_404;
        res.set_content("Not Found", "text/plain");
        return;
    }
    it->second(request, response);
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
        UBSE_LOG_WARN << "url=" << path << " has already been registered in server.";
        return;
    }
    routes_[routeKey] = handler;
    UBSE_LOG_INFO << "register method= " << method << ", url= " << path << " for http server.";
    return;
}

void UbseHttpServer::GetTcpServerPort(uint32_t &port)
{
    port = DEFAULT_TCP_SERVER_PORT;
    auto module = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "[HTTP] GetTcpServerPort GetModule failed.";
        return;
    }

    UbseResult ret = module->GetConf<uint32_t>(UBSE_UBFM_SECTION, UBSE_HTTP_TCP_SERVER_PORT, port);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[HTTP] GetTcpServerPort get ubse.server.port failed, will use default value: "
                       << DEFAULT_TCP_SERVER_PORT;
    }
    if (!UbseNetUtil::IsPortVaLid(port)) {
        UBSE_LOG_ERROR << "Tcp server port=" << port
                       << " is invalid, will use default value: " << DEFAULT_TCP_SERVER_PORT;
        port = DEFAULT_TCP_SERVER_PORT;
    }
}

std::unique_ptr<httplib::SSLServer> UbseHttpServer::CreateSslServer()
{
    password = cert::UbseSslValidator::LoadPasswordFromFile(UbseSSLConfig::PasswordFile);
    if (password.size() == 0) {
        UBSE_LOG_ERROR << "Private key password not found.";
        return nullptr;
    }
    auto sslServer = std::make_unique<httplib::SSLServer>(UbseSSLConfig::ServerCertFile, UbseSSLConfig::ServerKeyFile,
                                                          UbseSSLConfig::TrustCertFile, nullptr, password.c_str());
    if (!sslServer || !sslServer->is_valid()) {
        UBSE_LOG_ERROR << "Failed to initialize SSL server.";
        return nullptr;
    }
    // 配置客户端证书验证（mTLS）
    SSL_CTX *ctx = sslServer->ssl_context();
    if (SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION) != 1) {
        // 设置失败，可能是 OpenSSL 版本过低或不支持 TLS 1.3
        UBSE_LOG_ERROR << "Failed to set min protocol version: TLS1_3_VERSION";
        return nullptr;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
    // 如果 CRL 文件存在，加载并启用吊销检查
    if (access(UbseSSLConfig::CrlFile, F_OK) == 0) {
        X509_STORE *store = SSL_CTX_get_cert_store(ctx);
        if (store) {
            X509_LOOKUP *lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file());
            if (lookup && X509_load_crl_file(lookup, UbseSSLConfig::CrlFile, X509_FILETYPE_PEM)) {
                X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);
                UBSE_LOG_INFO << "CRL validation enabled.";
            } else {
                UBSE_LOG_ERROR << "Failed to parse CRL file: " << UbseSSLConfig::CrlFile;
                return nullptr;
            }
        }
    } else {
        UBSE_LOG_WARN << "CRL file not found, skipping CRL validation.";
    }

    sslServer->new_task_queue = []() -> httplib::ThreadPool* {
        return new httplib::ThreadPool(NO_4, NO_4);
    };
    return sslServer;
}

void UbseHttpServer::TcpRun()
{
    try {
        server_ = CreateSslServer();
        if (!server_) {
            throw std::runtime_error("Failed to create SSL server.");
        }
        server_->Get("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server_->Post("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server_->Put("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server_->Delete("/.*",
                        [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server_->Patch("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });

        uint32_t port = 0;
        GetTcpServerPort(port);
        UBSE_LOG_INFO << "TCP server start listening on port=" << port;

        // 检查端口是否被占用
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            throw std::runtime_error("Failed to create socket.");
        }

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        // 尝试绑定
        if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            close(sockfd);
            throw std::runtime_error("Port " + std::to_string(port) + " is already in use.");
        }

        // 关闭套接字
        close(sockfd);

        std::string eid = "127.0.0.1";
        server_->listen(eid, port);
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Error starting server=" << e.what();
    }
}

void UbseHttpServer::UdsRun()
{
    try {
        server_ = std::make_unique<Server>();
        if (!server_) {
            throw std::runtime_error("Failed to create SSL server.");
        }
        server_->Get("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server_->Post("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server_->Put("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server_->Delete("/.*",
                        [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server_->Patch("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });

        std::string udsAddress = UBSE_UBM_UDS_ADDRESS;

        const auto udsParentPath = GetParentDirectory(udsAddress);
        std::vector<__u32> caps = {CAP_DAC_OVERRIDE};
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
        if (UbseFileUtil::CreateAndChmodDirectory(udsParentPath, FOLDER_PEUBSEISSION) != UBSE_OK) {
            UBSE_LOG_ERROR << "Uds file directory= " << udsParentPath << " is unavailable.";
            UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
            return;
        }
        UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);

        // 如果 socket 文件已存在，先删掉
        auto res = unlink(udsAddress.c_str());
        if (res != 0 && errno != ENOENT) {
            UBSE_LOG_ERROR << "unlink uds file fail, errno= " << errno;
            return;
        }

        bool ret = server_->set_address_family(AF_UNIX).listen(udsAddress, 80);
        if (!ret) {
            UBSE_LOG_ERROR << "start server fail";
            return;
        }
        // listen接口为阻塞接口，当服务停止，将打印此日志
        UBSE_LOG_INFO << "uds server stopped";
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "ERROR starting server= " << e.what();
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