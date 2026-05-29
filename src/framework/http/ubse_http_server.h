/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_HTTP_SERVER_H
#define UBSE_HTTP_SERVER_H
#include "ubse_common_def.h"
#include "ubse_http_common.h"
#include "ubse_secure_buffer.h"
#include "httplib.h"
namespace ubse::http {
using httplib::Request;
using httplib::Response;
using httplib::Server;
using ubse::common::def::UbseResult;

class UbseHttpServer {
public:
    static UbseHttpServer& GetInstance()
    {
        std::call_once(initInstanceFlag_, &UbseHttpServer::InitInstance);
        if (!instance_) {
            throw std::runtime_error("Failed to initialize UbseHttpServer instance");
        }
        return *instance_;
    }

    bool Start(bool isTcpServer);

    void Stop();

    void HandleRequest(const httplib::Request& req, httplib::Response& res);

    void RegisterRoute(const std::string& path, const std::string& method, UbseHttpHandlerFunc handler);

private:
    UbseHttpServer() : running_(false){};
    std::mutex routesMutex_;
    std::atomic<bool> running_;
    std::unique_ptr<httplib::Server> server_;
    std::thread serverThread_;
    std::unordered_map<std::string, UbseHttpHandlerFunc> routes_;

    static std::unique_ptr<UbseHttpServer> instance_;
    static std::once_flag initInstanceFlag_;
    utils::SecureBuffer password;

    void TcpRun();

    void UdsRun();

    void GetTcpServerPort(uint32_t& port);

    static void InitInstance()
    {
        try {
            instance_.reset(new UbseHttpServer());
        } catch (const std::bad_alloc&) {
            instance_.reset(nullptr);
        }
    }

    static std::string GetParentDirectory(const std::string& path);

    UbseResult ValidateHttpRequest(const httplib::Request& req, UbseHttpRequest& request);

    void BuildResponse(httplib::Response& res, const UbseHttpResponse& response);

    std::string GenerateQueryString(const std::multimap<std::string, std::string>& queryParams);

    std::unique_ptr<httplib::SSLServer> CreateSslServer();
};

} // namespace ubse::http

#endif // UBSE_HTTP_SERVER_H
