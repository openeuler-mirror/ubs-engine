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

#include "ubse_http_module.h"

#include <httplib.h> // for Error, Response, Request, Client
#include <openssl/x509.h>
#include <securec.h> // for memcpy_s, EOK, errno_t
#include <map>       // for map, operator!=, _Rb_tree_con...
#include <mutex>     // for mutex, lock_guard

#include "ubse_cert_def.h"
#include "ubse_cert_validator.h"
#include "ubse_conf_module.h" // for UbseConfModule
#include "ubse_context.h"     // for UbseContext, ProcessMode, BAS...
#include "ubse_error.h"       // for UBSE_OK, UBSE_ERROR, UBSE_ERR...
#include "ubse_http_common.h" // for UbseHttpMethodToString, Chang...
#include "ubse_http_server.h" // for UbseHttpServer
#include "ubse_logger.h"      // for UbseLoggerEntry, FormatRetCode
#include "ubse_net_util.h"
#include "ubse_thread_pool_module.h" // for UbseTaskExecutorModule

namespace ubse::http {
using namespace ubse::task_executor;
using namespace ubse::utils;
using namespace ubse::context;
using namespace ubse::config;

BASE_DYNAMIC_CREATE(UbseHttpModule, UbseTaskExecutorModule);
UBSE_DEFINE_THIS_MODULE("ubse");

const size_t MAX_RESPONSE_BODY_SIZE = 2 * 1024 * 1024;
int UbseHttpModule::port = DEFAULT_UBM_SERVER_PORT;
bool UbseHttpModule::isTcpServer = false;

UbseResult UbseHttpModule::Initialize()
{
    auto module = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "Failed to get config module";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    uint32_t intCid;
    auto ret = module->GetConf<uint32_t>("ubse.ubfm", "ubm.server.cid", intCid);
    if (ret != UBSE_OK) {
        isTcpServer = false;
    } else {
        isTcpServer = true;
        uint32_t portValue = 0;
        ret = module->GetConf<uint32_t>("ubse.ubfm", "ubm.server.port", portValue);
        if (ret != UBSE_OK) {
            UBSE_LOG_WARN << "Get ubm.server.port failed, will use default value: " << DEFAULT_UBM_SERVER_PORT;
            portValue = DEFAULT_UBM_SERVER_PORT;
        }

        if (!UbseNetUtil::IsPortVaLid(portValue)) {
            UBSE_LOG_ERROR << "ubm.server.port=" << portValue
                           << " is out of range[1024, 65535], will use default value: " << DEFAULT_UBM_SERVER_PORT;
            port = DEFAULT_UBM_SERVER_PORT;
        } else {
            // 端口号有效，使用配置文件中的端口号
            port = static_cast<int>(portValue);
        }
        // 开启TCP通信时、需要对相关的证书进行校验
        if (!cert::UbseSslValidator::ValidateAll()) {
            UBSE_LOG_ERROR << "Init https server failed, please check cert file.";
            return UBSE_ERROR;
        }
    }
    UBSE_LOG_INFO << "http is using " << (isTcpServer ? "tcp" : "uds") << " server.";
    return UBSE_OK;
}

void UbseHttpModule::UnInitialize() {}

UbseResult UbseHttpModule::Start()
{
    UBSE_LOG_INFO << "Start HTTP server. HTTP type is " << (isTcpServer ? "TCP" : "UDS");
    try {
        if (UbseHttpServer::GetInstance().Start(isTcpServer)) {
            return UBSE_OK;
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Failed to start server, error=" << e.what();
    }
    UBSE_LOG_ERROR << "Failed to start server";
    return UBSE_ERROR;
}

void UbseHttpModule::Stop()
{
    try {
        UbseHttpServer::GetInstance().Stop();
        UBSE_LOG_INFO << "http stop end";
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Failed to stop server, error=" << e.what();
    }
}

UbseResult UbseHttpModule::RegHttpService(UbseHttpMethod method, const std::string &url, UbseHttpHandlerFunc func)
{
    try {
        UbseHttpServer::GetInstance().RegisterRoute(url, UbseHttpMethodToString(method), std::move(func));
    } catch (const std::exception &) {
        UBSE_LOG_ERROR << "Failed to RegisterRoute.";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseHttpModule::MakeError(uint32_t code)
{
    // 定义驱动表
    static const std::unordered_map<uint32_t, uint32_t> errorMapping = {
        {static_cast<uint32_t>(Error::Success), UBSE_OK},
        {static_cast<uint32_t>(Error::Unknown), UBSE_HTTP_ERROR_UNKNOWN},
        {static_cast<uint32_t>(Error::Connection), UBSE_HTTP_ERROR_CONNECTION},
        {static_cast<uint32_t>(Error::BindIPAddress), UBSE_HTTP_ERROR_BIND_IP_ADDRESS},
        {static_cast<uint32_t>(Error::Read), UBSE_HTTP_ERROR_READ},
        {static_cast<uint32_t>(Error::Write), UBSE_HTTP_ERROR_WRITE},
        {static_cast<uint32_t>(Error::ExceedRedirectCount), UBSE_HTTP_ERROR_EXCEED_REDIRECT_COUNT},
        {static_cast<uint32_t>(Error::Canceled), UBSE_HTTP_ERROR_CANCELED},
        {static_cast<uint32_t>(Error::SSLConnection), UBSE_HTTP_ERROR_SSL_CONNECTION},
        {static_cast<uint32_t>(Error::SSLLoadingCerts), UBSE_HTTP_ERROR_SSL_LOADING_CERTS},
        {static_cast<uint32_t>(Error::SSLServerVerification), UBSE_HTTP_ERROR_SSL_SERVER_VERIFICATION},
        {static_cast<uint32_t>(Error::UnsupportedMultipartBoundaryChars),
         UBSE_HTTP_ERROR_UNSUPPORTED_MULTIPART_BOUNDARY_CHARS},
        {static_cast<uint32_t>(Error::Compression), UBSE_HTTP_ERROR_COMPRESSION},
        {static_cast<uint32_t>(Error::ConnectionTimeout), UBSE_HTTP_ERROR_CONNECTION_TIMEOUT},
        {static_cast<uint32_t>(Error::ProxyConnection), UBSE_HTTP_ERROR_PROXY_CONNECTION},
    };
    auto it = errorMapping.find(code);
    if (it != errorMapping.end()) {
        return it->second;
    }
    return UBSE_HTTP_ERROR_FAILURE;
}

bool UbseHttpModule::TcpSend(httplib::Request &httpReq, httplib::Response &httpRsp, httplib::Error &error)
{
    // UBFM TCP服务端口为本机，端口为配置文件中读取的端口
    SecureBuffer serverKeyPassword = cert::UbseSslValidator::LoadPasswordFromFile(UbseSSLConfig::PasswordFile);
    if (serverKeyPassword.size() == 0) {
        UBSE_LOG_ERROR << "ServerKeyPassword is empty!";
        return false;
    }
    std::string hostName = "localhost";
    auto module = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "Failed to get config module";
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }
    auto ret = module->GetConf<std::string>("ubse.ubfm", "ubm.server.hostname", hostName);
    if (ret != UBSE_OK || hostName.empty()) {
        UBSE_LOG_WARN << "Get ubm.server.hostname failed or value is empty, will use default value: localhost";
        hostName = "localhost";
    }
    SSLClient cli(hostName, port, UbseSSLConfig::ServerCertFile, UbseSSLConfig::ServerKeyFile,
                  serverKeyPassword.c_str());
    cli.set_ca_cert_path(UbseSSLConfig::TrustCertFile);
    cli.set_connection_timeout(5, 0); // 设置连接超时时间为5s
    cli.set_path_encode(false);
    SSL_CTX *ctx = cli.ssl_context();
    if (ctx && !cert::UbseSslValidator::ConfigureCrlValidation(ctx)) {
        UBSE_LOG_ERROR << "Failed to configure CRL validation for client";
        return false;
    }
    cli.send(httpReq, httpRsp, error);
    return true;
}

void UbseHttpModule::UdsSend(httplib::Request &httpReq, httplib::Response &httpRsp, httplib::Error &error)
{
    // UBFM UDS服务地址为 UBFM_UDS_ADDRESS
    httplib::Client cli(UBM_UDS_ADDRESS);
    cli.set_address_family(AF_UNIX);
    cli.set_path_encode(false);
    cli.set_connection_timeout(5, 0); // 设置连接超时时间为5s
    cli.send(httpReq, httpRsp, error);
}

UbseResult UbseHttpModule::HttpSend(UbseHttpRequest &req, UbseHttpResponse &rsp)
{
    httplib::Error error;
    httplib::Headers headerMap;
    for (auto &header : req.headers) {
        headerMap.emplace(header.first, header.second);
    }
    // 禁用Expect头
    headerMap.emplace("Expect", "");
    httplib::Request httpReq{};
    httpReq.method = req.method;
    httpReq.path = req.path;
    httpReq.params = req.params;
    httpReq.headers = headerMap;
    httpReq.body = req.body;
    httplib::Response httpRsp{};

    // 目前send仅支持向UBFM服务端口发送请求
    if (isTcpServer) {
        if (!TcpSend(httpReq, httpRsp, error)) {
            return UBSE_ERROR;
        }
    } else {
        UdsSend(httpReq, httpRsp, error);
    }

    if (httpRsp.body.size() > MAX_RESPONSE_BODY_SIZE) {
        UBSE_LOG_ERROR << "Response body is too large, and the size is " << httpRsp.body.size() << ", "
                       << log::FormatRetCode(UBSE_HTTP_ERROR_MSG_OVERSIZE);
        return UBSE_HTTP_ERROR_MSG_OVERSIZE;
    }

    rsp.status = httpRsp.status;
    rsp.body = std::move(httpRsp.body);
    for (auto &header : httpRsp.headers) {
        rsp.headers.emplace(header.first, header.second);
    }
    if (error != httplib::Error::Success) {
        if (error == httplib::Error::SSLServerVerification) {
            UBSE_LOG_ERROR << "HTTPS request failed due to SSL server verification error. Please check if the server "
                              "certificate is revoked (CRL)";
        }
        return MakeError(static_cast<uint32_t>(error));
    }
    return UBSE_OK;
}

UbseResult UbseHttpModule::UbseHttpPostJsonRequest(const std::string &path, const std::string &body,
                                                   std::string &jsonRsp)
{
    Request req{};
    req.method = "POST";
    req.path = path;
    req.body = body;
    req.headers.emplace("Accept", "application/json");
    req.headers.emplace("Content-Type", "application/json");
    // 禁用Expect头
    req.headers.emplace("Expect", "");

    Error error;
    Response rsp{};
    if (isTcpServer) {
        if (!TcpSend(req, rsp, error)) {
            return UBSE_ERROR;
        }
    } else {
        UdsSend(req, rsp, error);
    }
    if (error != Error::Success) {
        if (error == Error::SSLServerVerification) {
            UBSE_LOG_ERROR << "HTTPS request failed due to SSL server verification error. Please check if the server "
                              "certificate is revoked (CRL)";
        }
        return MakeError(static_cast<uint32_t>(error));
    }

    if (rsp.status != static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK)) {
        UBSE_LOG_ERROR << "Response failed, unexpected status " << rsp.status << ", "
                       << log::FormatRetCode(UBSE_HTTP_ERROR_STATUS_ERROR);
        return UBSE_HTTP_ERROR_STATUS_ERROR;
    }
    if (rsp.body.size() > MAX_RESPONSE_BODY_SIZE) {
        UBSE_LOG_ERROR << "Response body is too large, and the size is " << rsp.body.size() << ", "
                       << log::FormatRetCode(UBSE_HTTP_ERROR_MSG_OVERSIZE);
        return UBSE_HTTP_ERROR_MSG_OVERSIZE;
    }
    if (rsp.body.empty()) {
        UBSE_LOG_ERROR << "Response body is empty, " << log::FormatRetCode(UBSE_HTTP_ERROR_MSG_EMPTY);
        return UBSE_HTTP_ERROR_MSG_EMPTY;
    }

    jsonRsp = rsp.body;
    return UBSE_OK;
}
} // namespace ubse::http