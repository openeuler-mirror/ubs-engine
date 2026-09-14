/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_http_server_rate_limit.h"

#include <httplib.h>
#include <chrono>
#include <string>
#include <thread>
#include "ubse_http_server.h"

namespace ubse::ut::http {
using namespace ubse::http;
using namespace httplib;

namespace {

constexpr const char* TEST_CLIENT_IP = "192.168.1.100";
constexpr const char* TEST_CLIENT_IP_OTHER = "192.168.1.200";

// 构造限流测试配置（rateLimitRps 可定制）
UbseHttpServer::Config MakeRateLimitConfig()
{
    UbseHttpServer::Config config;
    config.name = "RateLimitTestServer";
    config.useUds = false;
    config.useSsl = true;
    config.listenAddr = "127.0.0.1";
    config.port = 0;
    config.rateLimitRps = 0;
    return config;
}

Request MakeRequest(const std::string& method, const std::string& path, const std::string& body = "")
{
    Request req;
    req.method = method;
    req.path = path;
    req.body = body;
    return req;
}

} // namespace

void TestUbseHttpServerRateLimit::SetUp()
{
    Test::SetUp();
}

void TestUbseHttpServerRateLimit::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

// ============================ 限流测试 ============================

/*
 * 用例描述：测试低于限流阈值时请求正常通过
 * 测试步骤：
 * 1.创建一个rateLimitRps=3的HttpServer
 * 2.从同一IP连续发送3次请求
 * 预期结果：所有请求均正常返回非429状态码
 */
TEST_F(TestUbseHttpServerRateLimit, RateLimit_UnderLimit)
{
    UbseHttpServer::Config config = MakeRateLimitConfig();
    config.rateLimitRps = 3;
    UbseHttpServer server(config);

    for (int i = 0; i < 3; ++i) {
        Request req = MakeRequest("GET", "/test");
        req.remote_addr = TEST_CLIENT_IP;
        Response res;
        server.HandleRequest(req, res);
        EXPECT_NE(res.status, 429) << "Request " << i << " should not be rate limited";
    }
}

/*
 * 用例描述：测试超过限流阈值时返回429
 * 测试步骤：
 * 1.创建一个rateLimitRps=3的HttpServer
 * 2.从同一IP连续发送4次请求
 * 预期结果：第4次请求返回429
 */
TEST_F(TestUbseHttpServerRateLimit, RateLimit_ExceedLimit)
{
    UbseHttpServer::Config config = MakeRateLimitConfig();
    config.rateLimitRps = 3;
    UbseHttpServer server(config);

    for (int i = 0; i < 4; ++i) {
        Request req = MakeRequest("GET", "/test");
        req.remote_addr = TEST_CLIENT_IP;
        Response res;
        server.HandleRequest(req, res);
        if (i < 3) {
            EXPECT_NE(res.status, 429) << "Request " << i << " should not be rate limited";
        } else {
            EXPECT_EQ(res.status, 429) << "Request " << i << " should be rate limited";
        }
    }
}

/*
 * 用例描述：测试限流关闭时不受限制
 * 测试步骤：
 * 1.创建一个rateLimitRps=0（不限流）的HttpServer
 * 2.从同一IP连续发送10次请求
 * 预期结果：所有请求均正常返回
 */
TEST_F(TestUbseHttpServerRateLimit, RateLimit_Disabled)
{
    UbseHttpServer::Config config = MakeRateLimitConfig();
    config.rateLimitRps = 0;
    UbseHttpServer server(config);

    for (int i = 0; i < 10; ++i) {
        Request req = MakeRequest("GET", "/test");
        req.remote_addr = TEST_CLIENT_IP;
        Response res;
        server.HandleRequest(req, res);
        EXPECT_NE(res.status, 429) << "Request " << i << " should not be rate limited when disabled";
    }
}

/*
 * 用例描述：测试不同IP的限流窗口独立
 * 测试步骤：
 * 1.创建一个rateLimitRps=2的HttpServer
 * 2.从IP_A发送3次请求（第3次应被限流）
 * 3.从IP_B发送请求（应正常通过）
 * 预期结果：IP_A的第3次被限流，IP_B正常
 */
TEST_F(TestUbseHttpServerRateLimit, RateLimit_DifferentIps)
{
    UbseHttpServer::Config config = MakeRateLimitConfig();
    config.rateLimitRps = 2;
    UbseHttpServer server(config);

    // IP_A 的前2次正常
    for (int i = 0; i < 2; ++i) {
        Request req = MakeRequest("GET", "/test");
        req.remote_addr = TEST_CLIENT_IP;
        Response res;
        server.HandleRequest(req, res);
        EXPECT_NE(res.status, 429);
    }

    // IP_A 的第3次应被限流
    {
        Request req = MakeRequest("GET", "/test");
        req.remote_addr = TEST_CLIENT_IP;
        Response res;
        server.HandleRequest(req, res);
        EXPECT_EQ(res.status, 429);
    }

    // IP_B 应正常（不同IP的计数器独立）
    {
        Request req = MakeRequest("GET", "/test");
        req.remote_addr = TEST_CLIENT_IP_OTHER;
        Response res;
        server.HandleRequest(req, res);
        EXPECT_NE(res.status, 429);
    }
}

/*
 * 用例描述：测试限流窗口过期后重置
 * 测试步骤：
 * 1.创建一个rateLimitRps=2的HttpServer
 * 2.从同一IP连续发送3次请求（第3次被限流）
 * 3.等待1秒后再次发送请求
 * 预期结果：等待后请求恢复正常
 */
TEST_F(TestUbseHttpServerRateLimit, RateLimit_WindowReset)
{
    UbseHttpServer::Config config = MakeRateLimitConfig();
    config.rateLimitRps = 2;
    UbseHttpServer server(config);

    // 填满窗口
    for (int i = 0; i < 2; ++i) {
        Request req = MakeRequest("GET", "/test");
        req.remote_addr = TEST_CLIENT_IP;
        Response res;
        server.HandleRequest(req, res);
        EXPECT_NE(res.status, 429);
    }

    // 等待超过1秒，窗口过期
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // 窗口重置后应正常
    {
        Request req = MakeRequest("GET", "/test");
        req.remote_addr = TEST_CLIENT_IP;
        Response res;
        server.HandleRequest(req, res);
        EXPECT_NE(res.status, 429);
    }
}

/*
 * 用例描述：测试 429 响应包含 Retry-After 头
 * 测试步骤：
 * 1.创建一个rateLimitRps=1的HttpServer
 * 2.从同一IP连续发送2次请求
 * 预期结果：第2次返回429且包含 Retry-After 头
 */
TEST_F(TestUbseHttpServerRateLimit, RateLimit_HasRetryAfterHeader)
{
    UbseHttpServer::Config config = MakeRateLimitConfig();
    config.rateLimitRps = 1;
    UbseHttpServer server(config);

    Request req = MakeRequest("GET", "/test");
    req.remote_addr = TEST_CLIENT_IP;

    Response res1;
    server.HandleRequest(req, res1);
    EXPECT_NE(res1.status, 429);

    Response res2;
    server.HandleRequest(req, res2);
    EXPECT_EQ(res2.status, 429);
    auto it = res2.headers.find("Retry-After");
    EXPECT_NE(it, res2.headers.end());
    if (it != res2.headers.end()) {
        EXPECT_EQ(it->second, "1");
    }
}

// ============================ 配置默认值测试 ============================

/*
 * 用例描述：验证 UbseHttpServer::Config 默认值
 * 备注：校验限流与并发连接数默认关闭
 */
TEST_F(TestUbseHttpServerRateLimit, DefaultConfig_Values)
{
    UbseHttpServer::Config config;
    EXPECT_EQ(config.rateLimitRps, 0u);
    EXPECT_EQ(config.maxQueuedRequests, 0u);
}

/*
 * 用例描述：验证 maxQueuedRequests 配置可传递至 Config 且默认不限制
 * 备注：mqr>0 的实际拒绝行为依赖 cpp-httplib（v0.40.0 enqueue 失败即 shutdown(SHUT_RDWR)+close(fd) 发送 FIN），
 *       由 IT 层过载用例覆盖；此处覆盖字段传递与默认值。
 */
TEST_F(TestUbseHttpServerRateLimit, MaxQueuedRequests_DefaultAndConfigured)
{
    UbseHttpServer::Config config;
    EXPECT_EQ(config.maxQueuedRequests, 0u);  // 默认不限制，行为与历史版本一致

    config.maxQueuedRequests = 100000;  // 上边界
    EXPECT_EQ(config.maxQueuedRequests, 100000u);
}

} // namespace ubse::ut::http
