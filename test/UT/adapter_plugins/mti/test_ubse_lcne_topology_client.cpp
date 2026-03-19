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

#include "test_ubse_lcne_topology_client.h"
#include "lcne/ubse_lcne_topology_client.h"

namespace ubse::ut::lcne {
using namespace ubse::lcne;
constexpr int ZERO = 0;
constexpr int ONE = 1;
void TestUbseLcneTopologyClient::SetUp()
{
    Test::SetUp();
}

void TestUbseLcneTopologyClient::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneTopologyClient, ParseXmlData)
{
    GTEST_SKIP();
    std::string resBody = R"(
    <data>
      <nodes>
        <node>
          <slot>1</slot>
          <ubpu>1</ubpu>
          <iou>1</iou>
          <ubpu-type>CPU</ubpu-type>
          <physical-ports>
            <physical-port>
              <physical-port-id>1</physical-port-id>
              <interface-name>400GPU1/1/1</interface-name>
              <physical-port-role>ubse-in</physical-port-role>
              <physical-port-status>up</physical-port-status>
              <remote-slot>1</remote-slot>
              <remote-ubpu>1</remote-ubpu>
              <remote-iou>1</remote-iou>
              <remote-physical-port-id>1</remote-physical-port-id>
            </physical-port>
            <physical-port>
              <physical-port-id>2</physical-port-id>
              <interface-name>400GPU2/2/2</interface-name>
              <physical-port-role>ubse-in</physical-port-role>
              <physical-port-status>up</physical-port-status>
              <remote-slot>2</remote-slot>
              <remote-ubpu>2</remote-ubpu>
              <remote-die-id>2</remote-die-id>
              <remote-physical-port-id>2</remote-physical-port-id>
            </physical-port>
          </physical-ports>
        </node>
      </nodes>
    </data>
    )";
    std::vector<LcneNodeInfo> lcneNodes;
    UbseLcneTopologyClient::GetInstance().ParseData(resBody, lcneNodes);
    EXPECT_EQ(lcneNodes.size(), 1);  // node数量为1
    EXPECT_EQ(lcneNodes[ZERO].ports.size(), 2);
    EXPECT_EQ(lcneNodes[ZERO].slotId, "1");
    EXPECT_EQ(lcneNodes[ZERO].cardId, "1");
    EXPECT_EQ(lcneNodes[ZERO].type, "CPU");
    EXPECT_EQ(lcneNodes[ZERO].ports[ZERO].portId, "1");
    EXPECT_EQ(lcneNodes[ZERO].ports[ZERO].remotePortId, "1");
    EXPECT_EQ(lcneNodes[ZERO].ports[ONE].portId, "2");
    EXPECT_EQ(lcneNodes[ZERO].ports[ONE].remotePortId, "2");
}

TEST_F(TestUbseLcneTopologyClient, ParseXmlData_ParseFailed)
{
    std::vector<LcneNodeInfo> lcneNodes;
    std::string responseXml{};

    UbseResult ret = UbseLcneTopologyClient::GetInstance().ParseData(responseXml, lcneNodes);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneTopologyClient, ParseXmlData_ParseNodesFailed)
{
    std::vector<LcneNodeInfo> lcneNodes;
    std::string responseXml = R"(<data> </data>)";

    UbseResult ret = UbseLcneTopologyClient::GetInstance().ParseData(responseXml, lcneNodes);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneTopologyClient, ParseXmlData_ParsePortsFailed)
{
    std::vector<LcneNodeInfo> lcneNodes;
    std::string responseXml = R"(
    <data>
      <nodes>
        <node>
          <slot>1</slot>
          <ubpu>1</ubpu>
          <iou>1</iou>
          <ubpu-type>CPU</ubpu-type>
        </node>
      </nodes>
    </data>
    )";

    UbseResult ret = UbseLcneTopologyClient::GetInstance().ParseData(responseXml, lcneNodes);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneTopologyClient, GetTopology_Success)
{
    std::vector<LcneNodeInfo> lcneNodes;
    std::vector<LcneNodeInfo> nodes;
    LcneNodeInfo node;
    node.slotId = "1";
    node.cardId = "1";
    node.type = "CPU";
    LcnePortInfo port0;
    port0.portId = "1";
    port0.remotePortId = "1";
    node.ports.push_back(port0);
    LcnePortInfo port1;
    port1.portId = "2";
    port1.remotePortId = "2";
    node.ports.push_back(port1);
    nodes.push_back(node);

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    const auto func2 = &UbseLcneTopologyClient::ParseData;
    MOCKER_CPP(func2).stubs().with(rsp.body, outBound(nodes)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneTopologyClient::GetInstance().GetTopology(lcneNodes);
    EXPECT_EQ(lcneNodes.size(), 1);  // node数量为1
    EXPECT_EQ(lcneNodes[ZERO].ports.size(), 2);
    EXPECT_EQ(lcneNodes[ZERO].slotId, "1");
    EXPECT_EQ(lcneNodes[ZERO].cardId, "1");
    EXPECT_EQ(lcneNodes[ZERO].type, "CPU");
    EXPECT_EQ(lcneNodes[ZERO].ports[ZERO].portId, "1");
    EXPECT_EQ(lcneNodes[ZERO].ports[ZERO].remotePortId, "1");
    EXPECT_EQ(lcneNodes[ZERO].ports[ONE].portId, "2");
    EXPECT_EQ(lcneNodes[ZERO].ports[ONE].remotePortId, "2");
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneTopologyClient, GetTopology_HttpSendFailed)
{
    std::vector<LcneNodeInfo> lcneNodes;
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_ERROR));

    UbseResult ret = UbseLcneTopologyClient::GetInstance().GetTopology(lcneNodes);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneTopologyClient, GetTopology_RspStatusFailed)
{
    std::vector<LcneNodeInfo> lcneNodes;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = 404;

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneTopologyClient::GetInstance().GetTopology(lcneNodes);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneTopologyClient, GetTopology_RspEmpty)
{
    std::vector<LcneNodeInfo> lcneNodes;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneTopologyClient::GetInstance().GetTopology(lcneNodes);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneTopologyClient, GetTopology_ParseFailed)
{
    std::vector<LcneNodeInfo> lcneNodes;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    const auto func2 = &UbseLcneTopologyClient::ParseData;
    MOCKER_CPP(func2).stubs().will(returnValue(UBSE_ERROR));

    UbseResult ret = UbseLcneTopologyClient::GetInstance().GetTopology(lcneNodes);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneTopologyClient, GetLcneNodesString)
{
    std::vector<LcneNodeInfo> lcneNodes;
    LcneNodeInfo info;
    lcneNodes.push_back(info);
    std::string ret = UbseLcneTopologyClient::GetInstance().GetLcneNodesString(lcneNodes);
    EXPECT_TRUE(ret.find("[MTI] Topology information printing") != std::string::npos);
}
}  // namespace ubse::ut::lcne
