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
    UbseHttpModule::isTcpServer = false;
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

TEST_F(TestUbseHttpModule, Initialize_UdsMode)
{
    std::shared_ptr<UbseConfModule> conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(conf));
    MOCKER_CPP(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(UBSE_OK, testRHM.Initialize());
    EXPECT_FALSE(UbseHttpModule::isTcpServer);
}

TEST_F(TestUbseHttpModule, Initialize_PortInvalid)
{
    std::shared_ptr<UbseConfModule> conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(conf));
    uint32_t invalidPort = 99999;
    MOCKER_CPP(&UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(invalidPort))
        .will(returnValue(UBSE_OK));
    MOCKER(&cert::UbseSslValidator::ValidateAll).stubs().will(returnValue(true));
    EXPECT_EQ(UBSE_OK, testRHM.Initialize());
    EXPECT_TRUE(UbseHttpModule::isTcpServer);
    EXPECT_EQ(UbseHttpModule::port, DEFAULT_UBM_SERVER_PORT);
}

TEST_F(TestUbseHttpModule, Initialize_CertFail)
{
    std::shared_ptr<UbseConfModule> conf = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(conf));
    uint32_t port = 8082;
    MOCKER_CPP(&UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(mockcpp::any(), mockcpp::any(), outBound(port))
        .will(returnValue(UBSE_OK));
    MOCKER(&cert::UbseSslValidator::ValidateAll).stubs().will(returnValue(false));
    EXPECT_EQ(UBSE_ERROR, testRHM.Initialize());
}

TEST_F(TestUbseHttpModule, MakeError_AllMappings)
{
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::Success)), UBSE_OK);
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::Unknown)), UBSE_HTTP_ERROR_UNKNOWN);
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::Connection)), UBSE_HTTP_ERROR_CONNECTION);
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::BindIPAddress)), UBSE_HTTP_ERROR_BIND_IP_ADDRESS);
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::Read)), UBSE_HTTP_ERROR_READ);
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::Write)), UBSE_HTTP_ERROR_WRITE);
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::ExceedRedirectCount)),
              UBSE_HTTP_ERROR_EXCEED_REDIRECT_COUNT);
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::Canceled)), UBSE_HTTP_ERROR_CANCELED);
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::SSLConnection)), UBSE_HTTP_ERROR_SSL_CONNECTION);
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::SSLLoadingCerts)),
              UBSE_HTTP_ERROR_SSL_LOADING_CERTS);
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::SSLServerVerification)),
              UBSE_HTTP_ERROR_SSL_SERVER_VERIFICATION);
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::UnsupportedMultipartBoundaryChars)),
              UBSE_HTTP_ERROR_UNSUPPORTED_MULTIPART_BOUNDARY_CHARS);
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::Compression)), UBSE_HTTP_ERROR_COMPRESSION);
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::ConnectionTimeout)),
              UBSE_HTTP_ERROR_CONNECTION_TIMEOUT);
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::ProxyConnection)),
              UBSE_HTTP_ERROR_PROXY_CONNECTION);
}

TEST_F(TestUbseHttpModule, MakeError_UnknownCode)
{
    EXPECT_EQ(UbseHttpModule::MakeError(static_cast<uint32_t>(Error::Canceled) + 1000), UBSE_HTTP_ERROR_FAILURE);
    EXPECT_EQ(UbseHttpModule::MakeError(0xFFFFFFFF), UBSE_HTTP_ERROR_FAILURE);
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
static bool ThrowOnStart()
{
    throw std::runtime_error("mocked exception");
}

TEST_F(TestUbseHttpModule, Start_Exception)
{
    MOCKER(&UbseHttpServer::Start).stubs().will(invoke(ThrowOnStart));
    EXPECT_EQ(UBSE_ERROR, testRHM.Start());
}

static void ThrowOnStop()
{
    throw std::runtime_error("mocked exception");
}

TEST_F(TestUbseHttpModule, Stop_Exception)
{
    MOCKER(&UbseHttpServer::Stop).stubs().will(invoke(ThrowOnStop));
    EXPECT_NO_THROW(testRHM.Stop());
}
static std::string ThrowOnMethodToString(UbseHttpMethod method)
{
    (void)method;
    throw std::runtime_error("mocked exception");
}

TEST_F(TestUbseHttpModule, RegHttpService_Exception)
{
    MOCKER(UbseHttpMethodToString).stubs().will(invoke(ThrowOnMethodToString));
    EXPECT_EQ(UBSE_ERROR, testRHM.RegHttpService(UbseHttpMethod::UBSE_HTTP_METHOD_GET, "/test", TestHandler));
}
} // namespace ubse::ut::http