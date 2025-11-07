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

#include <httplib.h>                 // for Error, Response, Request, Client
#include <securec.h>                 // for memcpy_s, EOK, errno_t
#include <map>                       // for map, operator!=, _Rb_tree_con...
#include <mutex>                     // for mutex, lock_guard

#include "ubse_def.h"                // for UbseByteBuffer
#include "ubse_http_common.h"        // for UbseHttpMethodToString, Chang...
#include "ubse_http_error.h"         // for UBSE_ERROR_HTTP_GET_KEY_ERROR
#include "ubse_http_start.h"
#include "ubse_http_tcp_server.h"    // for UbseHttpTcpServer
#include "ubse_thread_pool_module.h" // for UbseTaskExecutorModule
#include "ubse_conf_module.h"        // for UbseConfModule
#include "ubse_context.h"            // for UbseContext, ProcessMode, BAS...
#include "ubse_error.h"              // for UBSE_OK, UBSE_ERROR, UBSE_ERR...
#include "ubse_logger.h"             // for UbseLoggerEntry, FormatRetCode
#include "ubse_logger_inner.h"       // for RM_LOG_ERROR, RM_LOG_INFO

namespace ubse::http {
using namespace ubse::task_executor;
using namespace ubse::utils;

BASE_DYNAMIC_CREATE(UbseHttpModule, UbseTaskExecutorModule);
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_HTTP_MID)

const size_t MAX_RESPONSE_BODY_SIZE = 2 * 1024 * 1024;

UbseResult UbseHttpModule::Initialize()
{
    return UBSE_OK;
}

void UbseHttpModule::UnInitialize() {}

UbseResult UbseHttpModule::Start()
{
    if (!StartTcpServer()) {
        UBSE_LOG_ERROR << "http tcp server start failed!";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseHttpModule::Stop()
{
    StopTcpServer();
    UBSE_LOG_INFO << "http stop end";
}


void UbseHttpModule::RegHttpTcpService(UbseHttpMethod method, const std::string &url, UbseHttpHandlerFunc func)
{
    try {
        return UbseHttpTcpServer::GetInstance().RegisterRoute(url, UbseHttpMethodToString(method), func);
    } catch (const std::exception &) {
        UBSE_LOG_ERROR << "Failed to RegisterRoute.";
    }
}

uint32_t UbseHttpModule::HttpSend(const std::string &host, int port, UbseHttpRequest &req, UbseHttpResponse &rsp)
{
    httplib::Client cli(host, port);
    httplib::Error error;

    httplib::Headers headerMap;
    for (auto &header : req.headers) {
        headerMap.emplace(header.first, header.second);
    }
    httplib::Request httpReq{};
    httpReq.method = req.method;
    httpReq.path = req.path;
    httpReq.params = req.params;
    httpReq.headers = headerMap;
    httpReq.body = req.body;
    httplib::Response httpRsp{};
    cli.send(httpReq, httpRsp, error);
    if (httpRsp.body.size() > MAX_RESPONSE_BODY_SIZE) {
        UBSE_LOG_ERROR << "Response body is too large, and the size is " << httpRsp.body.size() << ", " <<
            log::FormatRetCode(UBSE_ERROR_HTTP_MSG_OVERSIZE);
        return UBSE_ERROR_HTTP_MSG_OVERSIZE;
    }

    rsp.status = httpRsp.status;
    rsp.body = std::move(httpRsp.body);
    for (auto &header : httpRsp.headers) {
        rsp.headers.emplace(header.first, header.second);
    }

    if (error != httplib::Error::Success) {
        return MakeError(static_cast<uint32_t>(error));
    }
    return UBSE_OK;
}

UbseResult UbseHttpModule::MakeError(uint32_t code)
{
    // 定义驱动表
    static const std::unordered_map<uint32_t, uint32_t> errorMapping = {
        {static_cast<uint32_t>(Error::Success), UBSE_OK},
        {static_cast<uint32_t>(Error::Unknown), UBSE_ERROR_HTTP_UNKNOWN},
        {static_cast<uint32_t>(Error::Connection), UBSE_ERROR_HTTP_CONNECTION},
        {static_cast<uint32_t>(Error::BindIPAddress), UBSE_ERROR_HTTP_BIND_IP_ADDRESS},
        {static_cast<uint32_t>(Error::Read), UBSE_ERROR_HTTP_READ},
        {static_cast<uint32_t>(Error::Write), UBSE_ERROR_HTTP_WRITE},
        {static_cast<uint32_t>(Error::ExceedRedirectCount), UBSE_ERROR_HTTP_EXCEED_REDIRECT_COUNT},
        {static_cast<uint32_t>(Error::Canceled), UBSE_ERROR_HTTP_CANCELED},
        {static_cast<uint32_t>(Error::SSLConnection), UBSE_ERROR_HTTP_SSL_CONNECTION},
        {static_cast<uint32_t>(Error::SSLLoadingCerts), UBSE_ERROR_HTTP_SSL_LOADING_CERTS},
        {static_cast<uint32_t>(Error::SSLServerVerification), UBSE_ERROR_HTTP_SSL_SERVER_VERIFICATION},
        {static_cast<uint32_t>(Error::UnsupportedMultipartBoundaryChars),
         UBSE_ERROR_HTTP_UNSUPPORTED_MULTIPART_BOUNDARY_CHARS},
        {static_cast<uint32_t>(Error::Compression), UBSE_ERROR_HTTP_COMPRESSION},
        {static_cast<uint32_t>(Error::ConnectionTimeout), UBSE_ERROR_HTTP_CONNECTION_TIMEOUT},
        {static_cast<uint32_t>(Error::ProxyConnection), UBSE_ERROR_HTTP_PROXY_CONNECTION},
    };
    auto it = errorMapping.find(code);
    if (it != errorMapping.end()) {
        return it->second;
    }
    return UBSE_ERROR_HTTP_FAILURE;
}
} // namespace ubse::http