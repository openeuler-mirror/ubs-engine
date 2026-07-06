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
#include "httplib.h"
#include "src/include/cert/ubse_cert_def.h"
#include "ubse_common_def.h"
#include "ubse_http_common.h"
#include "ubse_secure_buffer.h"
namespace ubse::http {
using namespace httplib;
using namespace ubse::common::def;

class UbseHttpServer {
public:
    struct Config {
        std::string name;
        std::string listenAddr;
        uint32_t port{0};
        bool useUds{false};
        std::string udsPath;
        bool useSsl{false};
        cert::UbseCertPaths certPaths;  // 证书相关文件路径，由调用方初始化时填写
    };

    UbseHttpServer(const Config &config);
    ~UbseHttpServer();

    bool Start();

    void Stop();

    void HandleRequest(const httplib::Request &req, httplib::Response &res);

    void RegisterRoute(const std::string &path, const std::string &method, UbseHttpHandlerFunc handler);

    const Config &GetConfig() const { return config_; }

    void GetTcpServerPort(uint32_t &port) { port = config_.port; }

private:
    Config config_;
    std::mutex routesMutex_;
    std::atomic<bool> running_;
    std::unique_ptr<httplib::Server> server_;
    std::thread serverThread_;
    std::unordered_map<std::string, UbseHttpHandlerFunc> routes_;
    utils::SecureBuffer password;

    void TcpRun();

    void UdsRun();

    std::unique_ptr<httplib::SSLServer> CreateSslServer();

    UbseResult ValidateHttpRequest(const httplib::Request &req, UbseHttpRequest &request);

    void BuildResponse(httplib::Response &res, const UbseHttpResponse &response);

    std::string GenerateQueryString(const std::multimap<std::string, std::string> &queryParams);

    static std::string GetParentDirectory(const std::string &path);
};

} // namespace ubse::http

#endif // UBSE_HTTP_SERVER_H