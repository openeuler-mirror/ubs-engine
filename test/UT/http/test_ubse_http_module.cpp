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

#include "test_ubse_http_module.h"

#include "ubse_cert_validator.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_http_common.h"
#include "ubse_http_server.h"
#include "ubse_pointer_process.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"

using namespace ubse::http;
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::common::def;
using namespace httplib;
namespace ubse::ut::http {
constexpr const uint32_t TCP_SERVER_PORT = 8088;

TestUbseHttpModule::TestUbseHttpModule() = default;

void TestUbseHttpModule::SetUp()
{
    tFunc = [](const UbseHttpRequest& req, UbseHttpResponse& resp) {
        return UBSE_OK;
    };
    Test::SetUp();
}

void TestUbseHttpModule::TearDown()
{
    tFunc = nullptr;
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * 测试UbseHttpModule类的UbseHttpModuleStart方法和Stop方法
 * 测试步骤：
 * 1.启动成功
 * 预期结果：
 * 1.返回UBSE_OK
 */
TEST_F(TestUbseHttpModule, ShouldReturnUBSE_OK_When_Strat_Return_True)
{
    std::shared_ptr<UbseConfModule> conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(conf));
    MOCKER(&UbseHttpServer::Start).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_OK, testRHM.Initialize());
    auto ret = testRHM.Start();
    EXPECT_EQ(ret, UBSE_OK);
    testRHM.Stop();
    testRHM.UnInitialize();
}

/*
 * 用例描述：
 * 测试UbseHttpModule类的UbseHttpModuleStart方法和Stop方法
 * 测试步骤：
 * 1.启动失败
 * 预期结果：
 * 1.返回UBSE_ERROR
 */
TEST_F(TestUbseHttpModule, ShouldReturnUBSE_ERROR_When_Strat_Return_False)
{
    MOCKER(&UbseHttpServer::Start).stubs().will(returnValue(false));
    auto ret = testRHM.Start();
    EXPECT_EQ(ret, UBSE_ERROR);
    testRHM.Stop();
}

static uint32_t TestHandler(const UbseHttpRequest& req, UbseHttpResponse& resp)
{
    resp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    return UBSE_OK;
}

static uint32_t TestJsonHandler(const UbseHttpRequest& req, UbseHttpResponse& resp)
{
    resp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    return UBSE_OK;
}

TEST_F(TestUbseHttpModule, RegHttpService)
{
    UbseHttpMethod method = UbseHttpMethod::UBSE_HTTP_METHOD_GET;
    const std::string url = "ubse/test/url";
    EXPECT_NO_THROW(testRHM.RegHttpService(method, url, TestHandler));
}

TEST_F(TestUbseHttpModule, Initialize_TcpDefault)
{
    EXPECT_EQ(UBSE_ERROR_MODULE_LOAD_FAILED, testRHM.Initialize());
    std::shared_ptr<UbseConfModule> conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(conf));
    MOCKER_CPP(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseHttpServer::Start).stubs().will(returnValue(true));
    MOCKER(&cert::UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_OK, testRHM.Initialize());
    auto ret = testRHM.Start();
    EXPECT_EQ(ret, UBSE_OK);
    testRHM.Stop();
    testRHM.UnInitialize();
}

TEST_F(TestUbseHttpModule, Initialize_Conf)
{
    std::shared_ptr<UbseConfModule> conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(conf));
    uint32_t port = 8082;
    MOCKER_CPP(&UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(port))
        .will(returnValue(UBSE_OK));
    MOCKER(&UbseHttpServer::Start).stubs().will(returnValue(true));
    MOCKER(&cert::UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_OK, testRHM.Initialize());
    auto ret = testRHM.Start();
    EXPECT_EQ(ret, UBSE_OK);
    testRHM.Stop();
    testRHM.UnInitialize();
}

bool TestSend(httplib::Request& req, httplib::Response& res, httplib::Error& error)
{
    res.status = OK_200;
    res.body = "response";
    error = httplib::Error::Success;
    return true;
}

TEST_F(TestUbseHttpModule, HttpSend)
{
    UbseHttpRequest req{};
    UbseHttpResponse rsp{};
    req.method = "GET";
    req.path = "ubse/test/httpSend/url";
    UbseHttpMethod method = UbseHttpMethod::UBSE_HTTP_METHOD_GET;
    const std::string url = "ubse/test/httpSend/url";
    EXPECT_NO_THROW(testRHM.RegHttpService(method, url, TestHandler));
    auto ret = testRHM.HttpSend(req, rsp);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseHttpModule, HttpSend_Tcp)
{
    UbseHttpRequest req{};
    UbseHttpResponse rsp{};
    req.method = "GET";
    req.path = "ubse/test/httpSend/url";
    testRHM.isTcpServer = true;
    auto ret = testRHM.HttpSend(req, rsp);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseHttpModule, UbseHttpPostJsonRequest)
{
    UbseHttpRequest req{};
    std::string rsp;
    req.path = "ubse/test/httpSend/url1";
    req.body = "";
    testRHM.isTcpServer = false;
    auto ret = testRHM.UbseHttpPostJsonRequest(req.path, req.body, rsp);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseHttpModule, UbseHttpPostJsonRequest_Tcp)
{
    UbseHttpRequest req{};
    std::string rsp;
    req.path = "ubse/test/httpSend/url1";
    req.body = "";
    testRHM.isTcpServer = true;
    auto ret = testRHM.UbseHttpPostJsonRequest(req.path, req.body, rsp);
    EXPECT_NE(ret, UBSE_OK);
}
} // namespace ubse::ut::http