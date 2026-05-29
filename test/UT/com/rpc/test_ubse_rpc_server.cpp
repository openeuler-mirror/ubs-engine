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

#include "test_ubse_rpc_server.h"

#include "ubse_conf_module.h"
#include "ubse_context.h"

using namespace ubse::com;
using namespace ubse::ut::com;
using namespace ubse::context;
using namespace ubse::config;

void TestUbseRpcServer::SetUp()
{
    Test::SetUp();
}

void TestUbseRpcServer::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

/*
 * 用例描述：
 * rpc服务端启动
 * 测试步骤：
 * 1.启动失败
 * 2.成功
 * 预期结果：
 * 1.UBSE_ERROR
 * 2.成功
 */
namespace ubse::ut::com {
uint64_t port = 8080;

TEST_F(TestUbseRpcServer, Start)
{
    UbseRpcServer server{"127.0.0.1", 8080, "name", "id"};

    std::shared_ptr<UbseConfModule> nullModule = nullptr;
    std::shared_ptr<UbseConfModule> module = std::make_shared<UbseConfModule>();
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(nullModule)).then(returnValue(module));
    MOCKER_CPP(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(UbseCommunication::CreateUbseComEngine).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR_CONF_INVALID, server.Start());
}

void StopServer(const std::string& name) {}

/*
 * 用例描述：
 * rpc服务端停止
 * 测试步骤：
 * 1.成功
 * 预期结果：
 * 1.成功
 */
TEST_F(TestUbseRpcServer, Stop)
{
    UbseRpcServer server{"127.0.0.1", 8080, "name", "id"};
    MOCKER(UbseCommunication::DeleteUbseComEngine).stubs().will(invoke(StopServer));
    EXPECT_NO_THROW(server.Stop());
}

void MockDoReconnectServers(UbseRpcServer* rpcServer,
                            std::map<std::string, std::pair<std::string, uint16_t>> serverListNotConnect)
{
}

TEST_F(TestUbseRpcServer, IsReconnect)
{
    UbseRpcServer server{"127.0.0.1", port, "name", "id", {{"server", {"127.0.0.1", 7000}}}};
    EXPECT_EQ(true, server.IsReConnect(""));
}

TEST_F(TestUbseRpcServer, TestStart)
{
    GTEST_SKIP();
    UbseRpcServer server{"127.0.0.1", 8080, "name", "id"};

    std::shared_ptr<UbseConfModule> module = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseConfModule::GetConf<bool>).stubs().will(returnValue(UBSE_OK));
    MOCKER(UbseCommunication::CreateUbseComEngine).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, server.Start());
}

TEST_F(TestUbseRpcServer, TestStartFailed)
{
    UbseRpcServer server{"127.0.0.1", 8080, "name", "id"};

    std::shared_ptr<UbseConfModule> module = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(module));
    MOCKER_CPP(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseConfModule::GetConf<bool>).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(UbseCommunication::CreateUbseComEngine).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, server.Start());
}

TEST_F(TestUbseRpcServer, TestStartGetModuleFailed)
{
    UbseRpcServer server{"127.0.0.1", 8080, "name", "id"};

    std::shared_ptr<UbseConfModule> nullptrModule = nullptr;
    std::shared_ptr<UbseConfModule> module = std::make_shared<UbseConfModule>();
    MOCKER_CPP(&UbseContext::GetModule<UbseConfModule>)
        .stubs()
        .will(returnValue(module))
        .then(returnValue(nullptrModule));
    MOCKER_CPP(&UbseConfModule::GetConf<uint32_t>).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseConfModule::GetConf<bool>).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(UbseCommunication::CreateUbseComEngine).stubs().will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_OK, server.Start());
}

TEST_F(TestUbseRpcServer, TestConnect)
{
    UbseRpcServer server{"127.0.0.1", 8080, "name", "id"};
    EXPECT_EQ(UBSE_OK, server.Connect());
}
} // namespace ubse::ut::com