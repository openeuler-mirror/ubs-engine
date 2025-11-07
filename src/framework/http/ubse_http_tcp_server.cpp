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

#include "ubse_http_tcp_server.h"
#include <securec.h>
#include "ubse_common_def.h"
#include "ubse_http_common.h"
#include "ubse_http_error.h"
#include "httplib.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_logger_inner.h"
#include "ubse_net_util.h"
#include "ubse_pointer_process.h"
#include "ubse_topology_interface.h"

namespace ubse::http {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_HTTP_MID)
using namespace ubse::common::def;
using namespace ubse::log;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::utils;

std::unique_ptr<UbseHttpTcpServer> UbseHttpTcpServer::instance = nullptr;
std::once_flag UbseHttpTcpServer::initInstanceFlag;
constexpr uint32_t START_TIMEOUT = 3000;
constexpr uint32_t SLEEP_TIME = 100;

bool UbseHttpTcpServer::Start()
{
    try {
        serverThread = std::thread(&UbseHttpTcpServer::Run, this);
    } catch (const std::system_error &) {
        UBSE_LOG_ERROR << "Failed to create thread.";
        return false;
    }
    uint32_t retryTime = 0;
    while (retryTime <= START_TIMEOUT) {
        if (server.is_running()) {
            return true;
        }
        retryTime += SLEEP_TIME;
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME));
    }
    return server.is_running();
}

void UbseHttpTcpServer::Stop()
{
    server.stop();
    if (serverThread.joinable()) {
        serverThread.join();
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

std::string UbseHttpTcpServer::GenerateQueryString(const std::multimap<std::string, std::string> &queryParams)
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

UbseResult UbseHttpTcpServer::ValidateHttpRequest(const httplib::Request &req, UbseHttpRequest &request)
{
    if (!req.params.empty()) {
        std::string queryStr = GenerateQueryString(req.params);
        if (queryStr.size() > httpMaxQuerySize) {
            UBSE_LOG_ERROR << "HttpMsg queryParams is oversize";
            return UBSE_ERROR_HTTP_MSG_OVERSIZE;
        }
    }
    if (!req.body.empty() && req.body.size() > httpMaxBodySize) {
        UBSE_LOG_ERROR << "HttpRequest BodyLen is oversize";
        return UBSE_ERROR_HTTP_MSG_OVERSIZE;
    }
    request.method = req.method;
    if (StringToUbseHttpMethod(request.method) == UbseHttpMethod::UBSE_HTTP_METHOD_INVALID) {
        UBSE_LOG_ERROR << "The request method is not support";
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseHttpTcpServer::HandleRequest(const httplib::Request &req, httplib::Response &res)
{
    UBSE_LOG_INFO << "Receive request, uri: " << req.path << ", method: " << req.method;
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
    auto it = routes.find(routeKey);
    if (it == routes.end()) {
        UBSE_LOG_ERROR << "url: " << req.path << "has not been registered in tcp server.";
        res.status = NotFound_404;
        res.set_content("Not Found", "text/plain");
        return;
    }
    it->second(request, response);
    BuildResponse(res, response);
}

void UbseHttpTcpServer::BuildResponse(httplib::Response &res, const UbseHttpResponse &response)
{
    res.status = response.status;

    for (const auto &header : response.headers) {
        res.set_header(header.first, header.second);
    }

    res.body = response.body;
}

void UbseHttpTcpServer::RegisterRoute(const std::string &path, const std::string &method, UbseHttpHandlerFunc handler)
{
    std::string routeKey = method + path;
    std::lock_guard<std::mutex> lock(routesMutex);
    auto it = routes.find(routeKey);
    if (it != routes.end()) {
        UBSE_LOG_WARN << "url: " << path << " has already been registered in tcp server.";
        return;
    }
    routes[routeKey] = handler;
    UBSE_LOG_INFO << "register url: " << path << " for http tcp server.";
    return;
}

void UbseHttpTcpServer::GetTcpServerPort(uint32_t &port)
{
    port = DEFAULT_TCP_SERVER_PORT;
    auto module = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "[HTTP] GetTcpServerPort GetModule failed.";
        return;
    }

    UbseResult ret = module->GetConf<uint32_t>(UBSE_HTTP_SECTION, UBSE_HTTP_TCP_SERVER_PORT, port);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "[HTTP] GetTcpServerPort get ubse.server.port failed.";
    }
    if (!UbseNetUtil::IsPortVaLid(port)) {
        UBSE_LOG_ERROR << "Tcp server port: " << port << " is invalid";
        port = DEFAULT_TCP_SERVER_PORT;
    }
}

void UbseHttpTcpServer::Run()
{
    try {
        server.new_task_queue = []() -> httplib::ThreadPool * {
            return new httplib::ThreadPool(NO_4, NO_4);
        };

        server.Get("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server.Post("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server.Put("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server.Delete("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });
        server.Patch("/.*", [this](const httplib::Request &req, httplib::Response &res) { HandleRequest(req, res); });

        uint32_t port = 0;
        GetTcpServerPort(port);
        UBSE_LOG_INFO << "TCP server start listening on port: " << port;

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

        server.listen(eid, port);
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "Error starting server: " << e.what();
    }
}
} // namespace ubse::http