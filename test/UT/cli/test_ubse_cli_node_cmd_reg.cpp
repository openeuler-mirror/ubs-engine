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

#include "test_ubse_cli_node_cmd_reg.h"
#include <securec.h>
#include <mockcpp/mockcpp.hpp>
#include "ubse_cli_node_cmd_reg.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_ipc_client.h"
#include "ubse_serial_util.h"
#include "test_mock_invoke.h"

namespace ubse::ut::cli {
using namespace ubse::cli::reg;
using namespace ubse::common::def;
using namespace ubse::serial;
void TestUbseCliNodeCmdReg::SetUp() {}

void TestUbseCliNodeCmdReg::TearDown() {}

TEST_F(TestUbseCliNodeCmdReg, RegisterNodeModule)
{
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
    UBSE_CLI_REGISTER_MODULE("CLI_NODE_MODULE", UbseCliRegNodeModule);
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().creators_.size(), 1);
    UbseCliModuleRegistry::GetInstance().UbseCliCallAllModuleSignUp();
    EXPECT_EQ(UbseCliModuleRegistry::GetInstance().creators_.size(), 0);
    UbseCliModuleRegistry::GetInstance().UbseCliReset();
}

TEST_F(TestUbseCliNodeCmdReg, ClusterInvokeFailed)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_failed));
    EXPECT_NO_THROW(UbseCliRegNodeModule::UbseCliQueryClusterInfoFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, ClusterInvokeErrorSize)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_error_size));
    EXPECT_NO_THROW(UbseCliRegNodeModule::UbseCliQueryClusterInfoFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, ClusterInvokeEmpty)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_ubse_invoke_call_empty));
    EXPECT_NO_THROW(UbseCliRegNodeModule::UbseCliQueryClusterInfoFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, ClusterInvokeSuccess)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_cluster_ubse_invoke_call_normal));
    EXPECT_NO_THROW(UbseCliRegNodeModule::UbseCliQueryClusterInfoFunc({})->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, QueryCpuTopoInvokeSuccess)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_cpu_topo_ubse_invoke_call_normal));
    std::map<std::string, std::string> params = {};
    params["type"] = "cpu";
    EXPECT_NO_THROW(UbseCliRegNodeModule::UbseCliSDKQueryCpuTopoFunc(params)->UbseCliDisplayResult());
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, QueryCpuTopoNoTypeParam)
{
    std::map<std::string, std::string> params = {};
    auto result = UbseCliRegNodeModule::UbseCliSDKQueryCpuTopoFunc(params);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliNodeCmdReg, QueryCpuTopoInvalidType)
{
    std::map<std::string, std::string> params = {};
    params["type"] = "memory";
    auto result = UbseCliRegNodeModule::UbseCliSDKQueryCpuTopoFunc(params);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliNodeCmdReg, ValidateNodeIdEmpty)
{
    uint32_t parsedId = 0;
    EXPECT_FALSE(UbseCliRegNodeModule::ValidateNodeId("", parsedId));
}

TEST_F(TestUbseCliNodeCmdReg, ValidateNodeIdNonNumeric)
{
    uint32_t parsedId = 0;
    EXPECT_FALSE(UbseCliRegNodeModule::ValidateNodeId("abc", parsedId));
}

TEST_F(TestUbseCliNodeCmdReg, ValidateNodeIdOutOfRange)
{
    uint32_t parsedId = 0;
    EXPECT_FALSE(UbseCliRegNodeModule::ValidateNodeId("0", parsedId));
    EXPECT_FALSE(UbseCliRegNodeModule::ValidateNodeId("256", parsedId));
}

TEST_F(TestUbseCliNodeCmdReg, ValidateNodeIdValid)
{
    uint32_t parsedId = 0;
    EXPECT_TRUE(UbseCliRegNodeModule::ValidateNodeId("1", parsedId));
    EXPECT_EQ(parsedId, 1);
    EXPECT_TRUE(UbseCliRegNodeModule::ValidateNodeId("255", parsedId));
    EXPECT_EQ(parsedId, 255);
}

TEST_F(TestUbseCliNodeCmdReg, ExtractNodeIdWithParens)
{
    UbseSerialization ser;
    ser << right_v<uint32_t>(UBSE_ERR_NODE_NOT_ACTIVE);
    std::string nodeName = "hostname(5)";
    std::string role = "master";
    std::string bondingEid = "eid1";
    std::string guid = "guid1";
    ser << nodeName << role << bondingEid << guid;

    UbseDeSerialization deser(ser.GetBuffer(), ser.GetLength());
    auto result = UbseCliRegNodeModule::ProcessNodeInfoResponse(deser);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliNodeCmdReg, ExtractNodeIdNoParens)
{
    UbseSerialization ser;
    ser << right_v<uint32_t>(UBSE_ERR_NODE_NOT_ACTIVE);
    std::string nodeName = "hostname5";
    std::string role = "master";
    std::string bondingEid = "eid1";
    std::string guid = "guid1";
    ser << nodeName << role << bondingEid << guid;

    UbseDeSerialization deser(ser.GetBuffer(), ser.GetLength());
    auto result = UbseCliRegNodeModule::ProcessNodeInfoResponse(deser);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliNodeCmdReg, ProcessNodeInfoResponseSuccess)
{
    UbseSerialization ser;
    ser << right_v<uint32_t>(UBSE_OK);
    std::string node = "node1(1)";
    std::string role = "master";
    std::string bondingEid = "eid1";
    std::string guid = "guid1";
    ser << node << role << bondingEid << guid;

    UbseDeSerialization deser(ser.GetBuffer(), ser.GetLength());
    auto result = UbseCliRegNodeModule::ProcessNodeInfoResponse(deser);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliNodeCmdReg, ProcessNodeInfoResponseDeserFailed)
{
    uint8_t emptyBuf[1] = {0};
    UbseDeSerialization deser(emptyBuf, 0);
    auto result = UbseCliRegNodeModule::ProcessNodeInfoResponse(deser);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliNodeCmdReg, ProcessNodeInfoResponseNodeNotFound)
{
    UbseSerialization ser;
    ser << right_v<uint32_t>(UBSE_ERR_NODE_NOT_FOUND);
    std::string node = "node1";
    std::string role = "master";
    std::string bondingEid = "eid1";
    std::string guid = "guid1";
    ser << node << role << bondingEid << guid;

    UbseDeSerialization deser(ser.GetBuffer(), ser.GetLength());
    auto result = UbseCliRegNodeModule::ProcessNodeInfoResponse(deser);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliNodeCmdReg, ProcessNodeInfoResponseNotResponding)
{
    UbseSerialization ser;
    ser << right_v<uint32_t>(UBSE_ERR_NODE_NOT_RESPONDING);
    std::string node = "node1(3)";
    std::string role = "master";
    std::string bondingEid = "eid1";
    std::string guid = "guid1";
    ser << node << role << bondingEid << guid;

    UbseDeSerialization deser(ser.GetBuffer(), ser.GetLength());
    auto result = UbseCliRegNodeModule::ProcessNodeInfoResponse(deser);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliNodeCmdReg, ProcessNodeInfoResponseUnknownError)
{
    UbseSerialization ser;
    ser << right_v<uint32_t>(9999);
    std::string node = "node1";
    std::string role = "master";
    std::string bondingEid = "eid1";
    std::string guid = "guid1";
    ser << node << role << bondingEid << guid;

    UbseDeSerialization deser(ser.GetBuffer(), ser.GetLength());
    auto result = UbseCliRegNodeModule::ProcessNodeInfoResponse(deser);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliNodeCmdReg, QueryNodeInfoIpcFailed)
{
    MOCKER(&ubse_invoke_call).stubs().will(returnValue(UBSE_ERROR));
    auto result = UbseCliRegNodeModule::QueryNodeInfo("1");
    EXPECT_NE(result, nullptr);
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, QueryNodeInfoNotSupported)
{
    MOCKER(&ubse_invoke_call).stubs().will(returnValue(UBSE_ERR_NOT_SUPPORTED));
    auto result = UbseCliRegNodeModule::QueryNodeInfo("1");
    EXPECT_NE(result, nullptr);
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, QueryNodeInfoEmptyResponse)
{
    MOCKER(&ubse_invoke_call).stubs().will(returnValue(UBSE_OK));
    auto result = UbseCliRegNodeModule::QueryNodeInfo("1");
    EXPECT_NE(result, nullptr);
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, QueryNodeInfoSuccess)
{
    MOCKER(&ubse_invoke_call).stubs().will(invoke(mock_node_info_ubse_invoke_call_success));
    auto result = UbseCliRegNodeModule::QueryNodeInfo("1");
    EXPECT_NE(result, nullptr);
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, UbseCliQueryNodeInfoFuncNoParam)
{
    MOCKER(&ubse_invoke_call).stubs().will(returnValue(UBSE_ERROR));
    std::map<std::string, std::string> params = {};
    auto result = UbseCliRegNodeModule::UbseCliQueryNodeInfoFunc(params);
    EXPECT_NE(result, nullptr);
    MOCKER(&ubse_invoke_call).reset();
}

TEST_F(TestUbseCliNodeCmdReg, UbseCliQueryNodeInfoFuncEmptyParam)
{
    std::map<std::string, std::string> params = {};
    params["node"] = "";
    auto result = UbseCliRegNodeModule::UbseCliQueryNodeInfoFunc(params);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliNodeCmdReg, UbseCliQueryNodeInfoFuncInvalidParam)
{
    std::map<std::string, std::string> params = {};
    params["node"] = "abc";
    auto result = UbseCliRegNodeModule::UbseCliQueryNodeInfoFunc(params);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliNodeCmdReg, BuildCpuTopoTableWithData)
{
    std::vector<CliPhysicalLink> links;
    CliPhysicalLink link1;
    link1.node = "node1";
    link1.socketId = "0";
    link1.portId = "1";
    link1.interfaceName = "eth0";
    link1.peerNode = "node2";
    link1.peerSocketId = "0";
    link1.peerPortId = "2";
    link1.peerInterfaceName = "eth1";
    link1.linkId = "link1";
    links.push_back(link1);

    CliPhysicalLink link2;
    link2.node = "node1";
    link2.socketId = "1";
    link2.portId = "3";
    link2.interfaceName = "eth2";
    link2.peerNode = "node3";
    link2.peerSocketId = "0";
    link2.peerPortId = "4";
    link2.peerInterfaceName = "eth3";
    link2.linkId = "-";
    links.push_back(link2);

    auto result = UbseCliRegNodeModule::BuildCpuTopoTable(links);
    EXPECT_NE(result, nullptr);
}

TEST_F(TestUbseCliNodeCmdReg, BuildNodeInfoTableBasic)
{
    auto result = UbseCliRegNodeModule::BuildNodeInfoTable("node1(1)", "master", "eid1", "guid1");
    EXPECT_NE(result, nullptr);
}
} // namespace ubse::ut::cli
