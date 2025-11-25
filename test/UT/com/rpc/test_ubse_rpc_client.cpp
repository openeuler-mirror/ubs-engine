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

#include "test_ubse_rpc_client.h"
#include "rpc/ubse_rpc_client.h"
using namespace ubse::com;
using namespace ubse::ut::com;

void TestUbseRpcClient::SetUp()
{
    Test::SetUp();
}

void TestUbseRpcClient::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

namespace ubse::ut::com {
void MockDoReconnectServers(UbseRpcClient *rpcClient,
                            std::map<std::string, std::pair<std::string, uint16_t>> serverListNotConnect)
{
}
/*
 * 用例描述：
 * rpc客户端启动
 * 测试步骤：
 * 1.启动失败
 * 2.成功
 * 预期结果：
 * 1.UBSE_ERROR
 * 2.成功
 */
TEST_F(TestUbseRpcClient, Start)
{
    UbseRpcClient client({{"server", {"127.0.0.1", 7000}}}, "name", "id");
    MOCKER(UbseCommunication::CreateUbseComEngine).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR, client.Start());
    EXPECT_EQ(UBSE_OK, client.Start());
}

void StopClient(const std::string &name) {}

/*
 * 用例描述：
 * rpc客户端停止
 * 测试步骤：
 * 1.成功
 * 预期结果：
 * 1.成功
 */
TEST_F(TestUbseRpcClient, Stop)
{
    UbseRpcClient client({{"server", {"127.0.0.1", 7000}}}, "name", "id");
    MOCKER(UbseCommunication::DeleteUbseComEngine).stubs().will(invoke(StopClient));
    EXPECT_NO_THROW(client.Stop());
}

/*
 * 用例描述：
 * rpc客户端连接服务端
 * 测试步骤：
 * 1.成功
 * 预期结果：
 * 1.成功
 */
TEST_F(TestUbseRpcClient, Connect)
{
    UbseRpcClient client({{"server", {"127.0.0.1", 7000}}}, "name", "id");
    MOCKER(UbseCommunication::UbseComRpcConnect).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseRpcClient::DoReconnectServers).stubs().will(invoke(MockDoReconnectServers));
    EXPECT_EQ(UBSE_OK, client.Connect());
    sleep(1);
}

/*
 * 用例描述：
 * rpc客户端连接服务端失败
 * 测试步骤：
 * 1.实例化UbseRpcClient
 * 2.Mock函数ConnectWithRetry
 * 3.连接客户端
 * 预期结果：
 * 1.连接失败
 */
TEST_F(TestUbseRpcClient, ConnectFailed)
{
    UbseRpcClient client({{"server", {"127.0.0.1", 7000}}}, "name", "id");
    MOCKER(UbseCommunication::UbseComRpcConnect).stubs().will(returnValue(UBSE_ERROR));
    MOCKER(&UbseRpcClient::DoReconnectServers).stubs().will(invoke(MockDoReconnectServers));
    EXPECT_EQ(UBSE_ERROR, client.Connect());
    sleep(1);
}

TEST_F(TestUbseRpcClient, TestDoReconnect)
{
    UbseRpcClient client({{"server", {"127.0.0.1", 7000}}}, "name", "id");
    std::map<std::string, std::pair<std::string, uint16_t>> serverListNotConnect = {{"server", {"127.0.0.1", 7000}}};
    MOCKER(UbseCommunication::UbseComRpcConnect).stubs().will(returnValue(UBSE_OK));

    EXPECT_NO_THROW(client.DoReconnectServers(serverListNotConnect));
}
} // namespace ubse::ut::com