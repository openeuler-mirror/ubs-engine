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

#ifndef UBSE_HTTP_TCP_SERVER_H
#define UBSE_HTTP_TCP_SERVER_H
#include "ubse_http_common.h"
#include "ubse_common_def.h"
#include "httplib.h"

namespace ubse::http {
using namespace httplib;
using namespace ubse::common::def;

class UbseHttpTcpServer {
public:
    static UbseHttpTcpServer &GetInstance()
    {
        std::call_once(initInstanceFlag, &UbseHttpTcpServer::InitInstance);
        if (!instance) {
            throw std::runtime_error("Failed to initialize UbseHttpTcpServer instance");
        }
        return *instance;
    }

    bool Start();

    void Stop();

    void HandleRequest(const httplib::Request &req, httplib::Response &res);

    void RegisterRoute(const std::string &path, const std::string &method, UbseHttpHandlerFunc func);

private:
    UbseHttpTcpServer() : running(false){};
    std::mutex routesMutex;
    std::atomic<bool> running;
    Server server;
    std::thread serverThread;
    std::unordered_map<std::string, UbseHttpHandlerFunc> routes;

    static std::unique_ptr<UbseHttpTcpServer> instance;
    static std::once_flag initInstanceFlag;

    void Run();

    void GetTcpServerPort(uint32_t &port);

    static void InitInstance()
    {
        try {
            instance.reset(new UbseHttpTcpServer());
        } catch (const std::bad_alloc&) {
            instance.reset(nullptr);
        }
    }

    UbseResult ValidateHttpRequest(const httplib::Request &req, UbseHttpRequest &request);

    void BuildResponse(httplib::Response &res, const UbseHttpResponse &response);

    std::string GenerateQueryString(const std::multimap<std::string, std::string> &queryParams);
};

} // namespace ubse::http

#endif // UBSE_HTTP_TCP_SERVER_H
