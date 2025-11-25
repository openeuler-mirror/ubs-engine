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

#include "ubse_http_common.h"
#include "ubse_http_error.h"
#include "ubse_http_tcp_server.h"
#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_pointer_process.h"
#include "ubse_topology_interface.h"

using namespace ubse::http;
using namespace ubse::context;
namespace ubse::ut::http {
constexpr const uint32_t TCP_SERVER_PORT = 8088;

TestUbseHttpModule::TestUbseHttpModule() = default;

void TestUbseHttpModule::SetUp()
{
    tFunc = [](const UbseHttpRequest &req, UbseHttpResponse &resp) {
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
    MOCKER(&UbseHttpTcpServer::Start).stubs().will(returnValue(true));
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
    MOCKER(&UbseHttpTcpServer::Start).stubs().will(returnValue(false));
    auto ret = testRHM.Start();
    EXPECT_EQ(ret, UBSE_ERROR);
    testRHM.Stop();
}

static uint32_t TestTcpHandler(const UbseHttpRequest &req, UbseHttpResponse &resp)
{
    resp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    return UBSE_OK;
}

TEST_F(TestUbseHttpModule, RegHttpTcpService)
{
    UbseHttpMethod method = UbseHttpMethod::UBSE_HTTP_METHOD_GET;
    const std::string url = "ubse/test/url";
    EXPECT_NO_THROW(testRHM.RegHttpTcpService(method, url, TestTcpHandler));
}

bool TestSend(httplib::Request &req, httplib::Response &res, httplib::Error &error)
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
    EXPECT_NO_THROW(testRHM.RegHttpTcpService(method, url, TestTcpHandler));
    auto ret = testRHM.HttpSend("127.0.0.1", TCP_SERVER_PORT, req, rsp);
    EXPECT_NE(ret, UBSE_OK);
}
}