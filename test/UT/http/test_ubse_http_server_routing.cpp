/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "test_ubse_http_server_routing.h"

#include <httplib.h>
#include "ubse_error.h"
#include "ubse_http_common.h"
#include "ubse_http_server.h"

namespace ubse::ut::http {
using namespace ubse::http;
using namespace ubse::common::def;
using namespace httplib;

namespace {
// 构造一个最小可用的HttpServer配置，仅用于路由测试（不调用Start，无需SSL/网络）
UbseHttpServer::Config MakeConfig()
{
    UbseHttpServer::Config config;
    config.name = "RoutingTestServer";
    config.useUds = false;
    config.useSsl = false;
    config.listenAddr = "127.0.0.1";
    config.port = 0;
    config.udsPath = "";
    return config;
}

// 构造一个标记handler，命中时设置响应体为marker，状态200
UbseHttpHandlerFunc MakeMarkerHandler(const std::string &marker)
{
    return [marker](const UbseHttpRequest & /*req*/, UbseHttpResponse &resp) -> uint32_t {
        resp.status = 200;
        resp.body = marker;
        return UBSE_OK;
    };
}

// 构造httplib请求
Request MakeRequest(const std::string &method, const std::string &path, const std::string &body = "")
{
    Request req;
    req.method = method;
    req.path = path;
    req.body = body;
    return req;
}
} // namespace

void TestUbseHttpServerRouting::SetUp()
{
    Test::SetUp();
}

void TestUbseHttpServerRouting::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：测试路由精确匹配命中
 * 测试步骤：
 * 1.注册路由 GET /foo -> handlerA
 * 2.发送请求 GET /foo
 * 预期结果：handlerA被命中，响应体为"A"
 */
TEST_F(TestUbseHttpServerRouting, ExactMatch_Hit)
{
    UbseHttpServer server(MakeConfig());
    server.RegisterRoute("/foo", "GET", MakeMarkerHandler("A"));

    Request req = MakeRequest("GET", "/foo");
    Response res;
    server.HandleRequest(req, res);
    EXPECT_EQ(res.status, 200);
    EXPECT_EQ(res.body, "A");
}

/*
 * 用例描述：测试无匹配路由返回404
 * 测试步骤：
 * 1.注册路由 GET /foo
 * 2.发送请求 GET /bar（未注册）
 * 预期结果：返回404，响应体为"Not Found"
 */
TEST_F(TestUbseHttpServerRouting, NoMatch_Returns404)
{
    UbseHttpServer server(MakeConfig());
    server.RegisterRoute("/foo", "GET", MakeMarkerHandler("A"));

    Request req = MakeRequest("GET", "/bar");
    Response res;
    server.HandleRequest(req, res);
    EXPECT_EQ(res.status, NotFound_404);
    EXPECT_EQ(res.body, "Not Found");
}

/*
 * 用例描述：测试前缀匹配兜底（动态路径参数路由）
 * 测试步骤：
 * 1.注册前缀路由 DELETE /ubse/v1/ssu/spaces/ -> handlerP
 * 2.发送请求 DELETE /ubse/v1/ssu/spaces/myname
 * 预期结果：handlerP被命中，响应体为"P"
 */
TEST_F(TestUbseHttpServerRouting, PrefixMatch_DynamicPath_Hit)
{
    UbseHttpServer server(MakeConfig());
    server.RegisterRoute("/ubse/v1/ssu/spaces/", "DELETE", MakeMarkerHandler("P"));

    Request req = MakeRequest("DELETE", "/ubse/v1/ssu/spaces/myname");
    Response res;
    server.HandleRequest(req, res);
    EXPECT_EQ(res.status, 200);
    EXPECT_EQ(res.body, "P");
}

/*
 * 用例描述：测试前缀匹配要求请求路径严格长于注册前缀
 * 测试步骤：
 * 1.注册前缀路由 DELETE /ubse/v1/ssu/spaces/
 * 2.发送请求 DELETE /ubse/v1/ssu/spaces/（路径与前缀等长）
 * 预期结果：精确匹配命中前缀路由本身（routeKey完全相等），handler被命中
 * 备注：routeKey.size() > key.size() 不成立时不走前缀分支，但此时精确匹配自身命中
 */
TEST_F(TestUbseHttpServerRouting, PrefixMatch_PathEqualsPrefix_ExactHit)
{
    UbseHttpServer server(MakeConfig());
    server.RegisterRoute("/ubse/v1/ssu/spaces/", "DELETE", MakeMarkerHandler("P"));

    // 路径与前缀完全相等，routeKey完全匹配，走精确命中分支
    Request req = MakeRequest("DELETE", "/ubse/v1/ssu/spaces/");
    Response res;
    server.HandleRequest(req, res);
    EXPECT_EQ(res.status, 200);
    EXPECT_EQ(res.body, "P");
}

/*
 * 用例描述：测试多个前缀路由时，最长前缀优先匹配
 * 测试步骤：
 * 1.注册前缀路由 /a/ -> handlerA，前缀路由 /a/b/ -> handlerB
 * 2.发送请求 GET /a/b/c
 * 预期结果：handlerB被命中（最长前缀），响应体为"B"
 */
TEST_F(TestUbseHttpServerRouting, PrefixMatch_LongestWins)
{
    UbseHttpServer server(MakeConfig());
    server.RegisterRoute("/a/", "GET", MakeMarkerHandler("A"));
    server.RegisterRoute("/a/b/", "GET", MakeMarkerHandler("B"));

    Request req = MakeRequest("GET", "/a/b/c");
    Response res;
    server.HandleRequest(req, res);
    EXPECT_EQ(res.status, 200);
    EXPECT_EQ(res.body, "B");
}

/*
 * 用例描述：测试非前缀路由（不以'/'结尾）不参与前缀兜底匹配
 * 测试步骤：
 * 1.注册普通路由 /api（不以'/'结尾）
 * 2.发送请求 GET /api/child
 * 预期结果：返回404（/api 不作为前缀参与兜底）
 */
TEST_F(TestUbseHttpServerRouting, NonPrefixRoute_DoesNotParticipateInPrefixMatch)
{
    UbseHttpServer server(MakeConfig());
    server.RegisterRoute("/api", "GET", MakeMarkerHandler("API"));

    Request req = MakeRequest("GET", "/api/child");
    Response res;
    server.HandleRequest(req, res);
    EXPECT_EQ(res.status, NotFound_404);
    EXPECT_EQ(res.body, "Not Found");
}

/*
 * 用例描述：测试重复注册同一路由不会覆盖已注册handler
 * 测试步骤：
 * 1.注册路由 GET /dup -> handlerFirst
 * 2.再次注册路由 GET /dup -> handlerSecond
 * 3.发送请求 GET /dup
 * 预期结果：仍命中handlerFirst，响应体为"first"
 */
TEST_F(TestUbseHttpServerRouting, DuplicateRegistration_KeepsFirstHandler)
{
    UbseHttpServer server(MakeConfig());
    server.RegisterRoute("/dup", "GET", MakeMarkerHandler("first"));
    server.RegisterRoute("/dup", "GET", MakeMarkerHandler("second"));

    Request req = MakeRequest("GET", "/dup");
    Response res;
    server.HandleRequest(req, res);
    EXPECT_EQ(res.status, 200);
    EXPECT_EQ(res.body, "first");
}

/*
 * 用例描述：测试空路由表时任意请求返回404
 * 测试步骤：
 * 1.不注册任何路由
 * 2.发送请求 GET /anything
 * 预期结果：返回404
 */
TEST_F(TestUbseHttpServerRouting, EmptyRoutes_Returns404)
{
    UbseHttpServer server(MakeConfig());

    Request req = MakeRequest("GET", "/anything");
    Response res;
    server.HandleRequest(req, res);
    EXPECT_EQ(res.status, NotFound_404);
    EXPECT_EQ(res.body, "Not Found");
}

/*
 * 用例描述：测试前缀匹配与精确匹配共存时，精确匹配优先
 * 测试步骤：
 * 1.注册前缀路由 /ubse/v1/ssu/spaces/ -> handlerPrefix
 * 2.注册精确路由 /ubse/v1/ssu/spaces -> handlerExact
 * 3.发送请求 POST /ubse/v1/ssu/spaces（精确命中）
 * 4.发送请求 DELETE /ubse/v1/ssu/spaces/name（前缀命中）
 * 预期结果：
 * 3.handlerExact命中，响应体"exact"
 * 4.handlerPrefix命中，响应体"prefix"
 */
TEST_F(TestUbseHttpServerRouting, ExactAndPrefix_Coexist)
{
    UbseHttpServer server(MakeConfig());
    server.RegisterRoute("/ubse/v1/ssu/spaces", "POST", MakeMarkerHandler("exact"));
    server.RegisterRoute("/ubse/v1/ssu/spaces/", "DELETE", MakeMarkerHandler("prefix"));

    Request reqExact = MakeRequest("POST", "/ubse/v1/ssu/spaces");
    Response resExact;
    server.HandleRequest(reqExact, resExact);
    EXPECT_EQ(resExact.status, 200);
    EXPECT_EQ(resExact.body, "exact");

    Request reqPrefix = MakeRequest("DELETE", "/ubse/v1/ssu/spaces/name");
    Response resPrefix;
    server.HandleRequest(reqPrefix, resPrefix);
    EXPECT_EQ(resPrefix.status, 200);
    EXPECT_EQ(resPrefix.body, "prefix");
}

/*
 * 用例描述：测试不同HTTP方法的路由相互隔离
 * 测试步骤：
 * 1.注册 GET /item -> handlerGet
 * 2.注册 DELETE /item -> handlerDel
 * 3.发送 GET /item 和 DELETE /item
 * 预期结果：分别命中对应handler
 */
TEST_F(TestUbseHttpServerRouting, MethodIsolation)
{
    UbseHttpServer server(MakeConfig());
    server.RegisterRoute("/item", "GET", MakeMarkerHandler("get"));
    server.RegisterRoute("/item", "DELETE", MakeMarkerHandler("del"));

    Request reqGet = MakeRequest("GET", "/item");
    Response resGet;
    server.HandleRequest(reqGet, resGet);
    EXPECT_EQ(resGet.body, "get");

    Request reqDel = MakeRequest("DELETE", "/item");
    Response resDel;
    server.HandleRequest(reqDel, resDel);
    EXPECT_EQ(resDel.body, "del");
}
} // namespace ubse::ut::http
