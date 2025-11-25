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

#include "test_ubse_http_tcp_server.h"
#include <httplib.h>
#include <securec.h>
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_http_common.h"
#include "ubse_http_error.h"
#include "ubse_http_tcp_server.h"
#include "ubse_pointer_process.h"
#include "ubse_topology_interface.h"

namespace ubse::ut::http {
using namespace ubse::http;
using namespace ubse::context;
using namespace ubse::config;

constexpr const uint32_t DEFAULT_TCP_SERVER_PORT = 8082;
constexpr const uint32_t TCP_SERVER_PORT = 8088;
constexpr const uint32_t INVALID_TCP_SERVER_PORT = 65536;

void TestUbseHttpTcpServer::SetUp()
{
    Test::SetUp();
}

void TestUbseHttpTcpServer::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：如下
 * 测试启停tcp server
 * 测试步骤：如下
 * 1.启动tcp server
 * 2.停止tcp server
 * 预期结果：如下
 * 1.启动后IsTcpServerRunning为true
 * 2.停止后IsTcpServerRunning为false
 */
TEST_F(TestUbseHttpTcpServer, StartAndStopTcpServer)
{
    std::shared_ptr<UbseConfModule> module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    ubse::mti::MtiNodeInfo ubseNodeInfo{"Node1", "127.0.0.1"};
    MOCKER(ubse::mti::UbseGetLocalNodeInfo).stubs().with(outBound(ubseNodeInfo)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UbseHttpTcpServer::GetInstance().Start(), true);
    EXPECT_NO_THROW(UbseHttpTcpServer::GetInstance().Stop());
    GlobalMockObject::verify();
}

/*
 * 用例描述：如下
 * 测试ValidateHttpRequest入参
 * 测试步骤：如下
 * 1.设置req的params长度为httpMaxQuerySize + 1
 * 预期结果：如下
 * 1.校验返回UBSE_ERROR_HTTP_MSG_OVERSIZE
 */
TEST_F(TestUbseHttpTcpServer, ValidateHttpRequestFailedCauseInvalidParamsSize)
{
    UbseHttpRequest request{};
    httplib::Request req{};

    std::string testString(httpMaxQuerySize + 1, 'a'); // 设置测试字符串长度为param最大值加1
    req.params.insert(std::make_pair("param1", testString));
    EXPECT_EQ(UbseHttpTcpServer::GetInstance().ValidateHttpRequest(req, request), UBSE_ERROR_HTTP_MSG_OVERSIZE);
}

/*
 * 用例描述：如下
 * 测试ValidateHttpRequest入参
 * 测试步骤：如下
 * 1.设置req的请求体长度为httpMaxBodySize + 1
 * 预期结果：如下
 * 1.校验返回UBSE_ERROR_HTTP_MSG_OVERSIZE
 */
TEST_F(TestUbseHttpTcpServer, ValidateHttpRequestFailedCauseInvalidBodySize)
{
    UbseHttpRequest request{};
    httplib::Request req{};

    std::string testBodyString(httpMaxBodySize + 1, 'a'); // 设置测试字符串长度为body最大值加1
    req.params.insert(std::make_pair("param1", "value1"));
    req.body = testBodyString;
    EXPECT_EQ(UbseHttpTcpServer::GetInstance().ValidateHttpRequest(req, request), UBSE_ERROR_HTTP_MSG_OVERSIZE);
}

/*
 * 用例描述：如下
 * 测试ValidateHttpRequest入参
 * 测试步骤：如下
 * 1.设置req的请求体长度为和请求体长度都正常
 * 2.设置req的请求方法为非法
 * 3.再设置req的请求方法正常
 * 预期结果：如下
 * 1.校验返回UBSE_ERROR
 * 2.校验返回UBSE_OK
 */
TEST_F(TestUbseHttpTcpServer, ValidateHttpRequest)
{
    UbseHttpRequest request{};
    httplib::Request req{};

    req.params.insert(std::make_pair("param1", "value1"));
    req.body = "testRequestBody";
    req.method = "INVALIDTYPE";
    EXPECT_EQ(UbseHttpTcpServer::GetInstance().ValidateHttpRequest(req, request), UBSE_ERROR);

    req.method = "GET";
    EXPECT_EQ(UbseHttpTcpServer::GetInstance().ValidateHttpRequest(req, request), UBSE_OK);
}

/*
 * 用例描述：如下
 * 测试GetTcpServerPort
 * 测试步骤：如下
 * 1.mock UbseContext::GetModule<UbseConfModule>返回空对象
 * 预期结果：如下
 * 1.GetTcpServerPort不抛异常
 * 2.port值为DEFAULT_TCP_SERVER_PORT
 */
TEST_F(TestUbseHttpTcpServer, GetTcpServerPortFailedCauseGetConfMoudleFailed)
{
    uint32_t port = 0;
    std::shared_ptr<UbseConfModule> nullModule = nullptr;
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(nullModule));
    EXPECT_NO_THROW(UbseHttpTcpServer::GetInstance().GetTcpServerPort(port));
    EXPECT_EQ(port, DEFAULT_TCP_SERVER_PORT);
}

static UbseResult GetConfMocker(UbseConfModule *This, std::string &section, std::string &configKey, uint32_t &configVal)
{
    configVal = INVALID_TCP_SERVER_PORT;
    return UBSE_OK;
}

/*
 * 用例描述：如下
 * 测试GetTcpServerPort
 * 测试步骤：如下
 * 1.mock UbseContext::GetModule<UbseConfModule>返回非空对象
 * 2.mock UbseConfModule::GetConf<std::string>返回UBSE_ERROR
 * 3.再设置ackConfModule::GetConf<std::string>返回的端口为MAX_PORT加1
 * 预期结果：如下
 * 1.GetTcpServerPort不抛异常
 * 2.port值为DEFAULT_TCP_SERVER_PORT
 * 3.GetTcpServerPort不抛异常，port值也被重置为DEFAULT_TCP_SERVER_PORT
 */
TEST_F(TestUbseHttpTcpServer, GetTcpServerPortFailedCauseGetConfFailed)
{
    uint32_t port = 0;
    std::shared_ptr<UbseConfModule> module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    MOCKER(&UbseConfModule::GetConf<std::string>).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NO_THROW(UbseHttpTcpServer::GetInstance().GetTcpServerPort(port));
    EXPECT_EQ(port, DEFAULT_TCP_SERVER_PORT);

    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(invoke(GetConfMocker));
    EXPECT_NO_THROW(UbseHttpTcpServer::GetInstance().GetTcpServerPort(port));
    EXPECT_EQ(port, DEFAULT_TCP_SERVER_PORT);
}

static UbseResult GetConfMockerWithValidPort(UbseConfModule *This, std::string &section, std::string &configKey,
                                             uint32_t &configVal)
{
    configVal = TCP_SERVER_PORT;
    return UBSE_OK;
}

/*
 * 用例描述：如下
 * 测试GetTcpServerPort
 * 测试步骤：如下
 * 1.mock UbseContext::GetModule<UbseConfModule>返回非空对象
 * 2.设置ackConfModule::GetConf<std::string>返回的端口为8088
 * 预期结果：如下
 * 1.GetTcpServerPort不抛异常
 * 2.port值为8088
 */
TEST_F(TestUbseHttpTcpServer, GetTcpServerPortSucceed)
{
    uint32_t port = 0;
    std::shared_ptr<UbseConfModule> module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    MOCKER(&UbseConfModule::GetConf<uint32_t>).stubs().will(invoke(GetConfMockerWithValidPort));
    EXPECT_NO_THROW(UbseHttpTcpServer::GetInstance().GetTcpServerPort(port));
    EXPECT_EQ(port, TCP_SERVER_PORT);
}

static uint32_t TestHandlerForTcpReg(const UbseHttpRequest &req, UbseHttpResponse &resp)
{
    resp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    return UBSE_OK;
}

TEST_F(TestUbseHttpTcpServer, HandleRequest400)
{
    httplib::Request req{};
    httplib::Response resp{};

    req.params.insert(std::make_pair("param1", "value1"));
    req.body = "testRequestBody";
    req.method = "GET";
    req.path = "";

    MOCKER(&UbseHttpTcpServer::ValidateHttpRequest).stubs().will(returnValue(UBSE_ERROR));
    UbseHttpTcpServer::GetInstance().HandleRequest(req, resp);
    EXPECT_EQ(resp.status, BadRequest_400);

    GlobalMockObject::verify();
}

TEST_F(TestUbseHttpTcpServer, HandleRequest404)
{
    httplib::Request req{};
    httplib::Response resp{};

    req.params.insert(std::make_pair("param1", "value1"));
    req.body = "testRequestBody";
    req.method = "GET";
    req.path = "";

    MOCKER(&UbseHttpTcpServer::ValidateHttpRequest).stubs().will(returnValue(UBSE_OK));
    UbseHttpTcpServer::GetInstance().HandleRequest(req, resp);
    EXPECT_EQ(resp.status, NotFound_404);

    GlobalMockObject::verify();
}

TEST_F(TestUbseHttpTcpServer, HandleRequest200)
{
    httplib::Request req{};
    httplib::Response resp{};

    req.params.insert(std::make_pair("param1", "value1"));
    req.body = "testRequestBody";
    req.method = "GET";
    req.path = "";

    MOCKER(&UbseHttpTcpServer::ValidateHttpRequest).stubs().will(returnValue(UBSE_OK));

    UbseHttpTcpServer::GetInstance().RegisterRoute(req.path, req.method, TestHandlerForTcpReg);
    UbseHttpTcpServer::GetInstance().HandleRequest(req, resp);
    EXPECT_EQ(resp.status, OK_200);
    GlobalMockObject::verify();
}
} // namespace ubse::ut::http