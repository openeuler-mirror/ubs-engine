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

#include "test_ubse_topo_cna.h"

#include <mockcpp/mokc.h>

#include "lcne/ubse_topo_cna.h"
#include "src/framework/http/ubse_http_module.h"
#include "ubse_context.h"
#include "ubse_error.h"

namespace ubse::lcne {
using namespace ubse::context;

constexpr int ZERO = 0;
constexpr int ONE = 1;

void TestUbseTopoCna::SetUp()
{
    Test::SetUp();
}

void TestUbseTopoCna::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseTopoCna, ParseXmlData)
{
    std::vector<LcneNodeCnaInfo> lcneNodeCnaInfos;
    std::string responseXml = R"(<data xmlns="urn:huawei:yang:huawei-lingqu-topology">
  <addresses>
    <address>
      <slot>1</slot>
      <ubpu>1</ubpu>
      <iou>1</iou>
      <bus-primary-cna>0x0401</bus-primary-cna>
      <node-cna>0x000401</node-cna>
      <node-ip>-</node-ip>
      <physical-ports>
        <physical-port>
          <physical-port-id>0</physical-port-id>
          <interface-name>400GUB1/1/1</interface-name>
          <bus-port-cna>0x0403</bus-port-cna>
          <port-cna>0x000403</port-cna>
          <port-ip>-</port-ip>
        </physical-port>
        <physical-port>
          <physical-port-id>1</physical-port-id>
          <interface-name>400GUB1/1/2</interface-name>
          <bus-port-cna>0x0404</bus-port-cna>
          <port-cna>0x000404</port-cna>
          <port-ip>-</port-ip>
        </physical-port>
      </physical-ports>
    </address>
    <address>
      <slot>1</slot>
      <ubpu>2</ubpu>
      <iou>1</iou>
      <bus-primary-cna>0x0402</bus-primary-cna>
      <node-cna>0x000402</node-cna>
      <node-ip>-</node-ip>
      <physical-ports>
        <physical-port>
          <physical-port-id>0</physical-port-id>
          <interface-name>400GUB1/2/1</interface-name>
          <bus-port-cna>0x040c</bus-port-cna>
          <port-cna>0x00040c</port-cna>
          <port-ip>-</port-ip>
        </physical-port>
        <physical-port>
          <physical-port-id>1</physical-port-id>
          <interface-name>400GUB1/2/2</interface-name>
          <bus-port-cna>0x040d</bus-port-cna>
          <port-cna>0x00040d</port-cna>
          <port-ip>-</port-ip>
        </physical-port>
      </physical-ports>
    </address>
  </addresses>
</data>)";

    EXPECT_EQ(UbseTopoCna::GetInstance().ParseTopoCnaRsp(responseXml, lcneNodeCnaInfos), UBSE_OK);
    EXPECT_EQ(lcneNodeCnaInfos[ZERO].slotId, "1");
    EXPECT_EQ(lcneNodeCnaInfos[ZERO].chipId, "1");
    EXPECT_EQ(lcneNodeCnaInfos[ZERO].cardId, "1");
    EXPECT_EQ(lcneNodeCnaInfos[ZERO].busNodeCna, "0x0401");
    EXPECT_EQ(lcneNodeCnaInfos[ZERO].ports.size(), 2);
    EXPECT_EQ(lcneNodeCnaInfos[ZERO].ports[ZERO].portId, "0");
    EXPECT_EQ(lcneNodeCnaInfos[ZERO].ports[ZERO].portCna, "0x0403");
    EXPECT_EQ(lcneNodeCnaInfos[ONE].slotId, "1");
    EXPECT_EQ(lcneNodeCnaInfos[ONE].chipId, "2");
    EXPECT_EQ(lcneNodeCnaInfos[ONE].cardId, "1");
    EXPECT_EQ(lcneNodeCnaInfos[ONE].busNodeCna, "0x0402");
    EXPECT_EQ(lcneNodeCnaInfos[ONE].ports.size(), 2);
    EXPECT_EQ(lcneNodeCnaInfos[ONE].ports[ONE].portId, "1");
    EXPECT_EQ(lcneNodeCnaInfos[ONE].ports[ONE].portCna, "0x040d");
}

TEST_F(TestUbseTopoCna, ParseXmlData_ParseFailed)
{
    std::vector<LcneNodeCnaInfo> lcneNodeCnaInfos;
    std::string responseXml{};

    UbseResult ret = UbseTopoCna::GetInstance().ParseTopoCnaRsp(responseXml, lcneNodeCnaInfos);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseTopoCna, ParseXmlData_ParseFailed1)
{
    std::vector<LcneNodeCnaInfo> lcneNodeCnaInfos;
    std::string responseXml = R"(<data> </data>)";

    UbseResult ret = UbseTopoCna::GetInstance().ParseTopoCnaRsp(responseXml, lcneNodeCnaInfos);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseTopoCna, ParseXmlData_ParseFailed2)
{
    std::vector<LcneNodeCnaInfo> lcneNodeCnaInfos;
    std::string responseXml = R"(<data>
<addresses>
    <address>
      <slot>1</slot>
      <ubpu>1</ubpu>
      <iou>1</iou>
      <bus-primary-cna>0x0401</bus-primary-cna>
      <node-cna>0x000401</node-cna>
      <node-ip>-</node-ip>
    </address>
  </addresses>
</data>)";

    UbseResult ret = UbseTopoCna::GetInstance().ParseTopoCnaRsp(responseXml, lcneNodeCnaInfos);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseTopoCna, QueryTopoCna_Success)
{
    std::vector<LcneNodeCnaInfo> lcneNodeCnaInfos;
    std::vector<LcneNodeCnaInfo> infos;
    LcneNodeCnaInfo info;
    info.slotId = "1";
    info.chipId = "1";
    info.cardId = "1";
    info.busNodeCna = "0x0401";
    std::vector<LcnePortCnaInfo> ports;
    LcnePortCnaInfo port1;
    port1.portId = "0";
    port1.portCna = "0x0403";
    ports.push_back(port1);
    port1.portId = "1";
    port1.portCna = "0x0404";
    ports.push_back(port1);
    info.ports = ports;
    infos.push_back(info);
    LcneNodeCnaInfo info1;
    info1.slotId = "1";
    info1.chipId = "2";
    info1.cardId = "1";
    info1.busNodeCna = "0x0402";
    std::vector<LcnePortCnaInfo> ports1;
    LcnePortCnaInfo port2;
    port2.portId = "0";
    port2.portCna = "0x040c";
    ports1.push_back(port2);
    port2.portId = "1";
    port2.portCna = "0x040d";
    ports1.push_back(port2);
    info1.ports = ports1;
    infos.push_back(info1);

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    const auto func2 = &UbseTopoCna::ParseTopoCnaRsp;
    MOCKER_CPP(func2).stubs().with(rsp.body, outBound(infos)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseTopoCna::GetInstance().QueryTopoCna(lcneNodeCnaInfos);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(lcneNodeCnaInfos[ZERO].slotId, "1");
    EXPECT_EQ(lcneNodeCnaInfos[ZERO].chipId, "1");
    EXPECT_EQ(lcneNodeCnaInfos[ZERO].cardId, "1");
    EXPECT_EQ(lcneNodeCnaInfos[ZERO].busNodeCna, "0x0401");
    EXPECT_EQ(lcneNodeCnaInfos[ZERO].ports.size(), 2);
    EXPECT_EQ(lcneNodeCnaInfos[ZERO].ports[ZERO].portId, "0");
    EXPECT_EQ(lcneNodeCnaInfos[ZERO].ports[ZERO].portCna, "0x0403");
    EXPECT_EQ(lcneNodeCnaInfos[ONE].slotId, "1");
    EXPECT_EQ(lcneNodeCnaInfos[ONE].chipId, "2");
    EXPECT_EQ(lcneNodeCnaInfos[ONE].cardId, "1");
    EXPECT_EQ(lcneNodeCnaInfos[ONE].busNodeCna, "0x0402");
    EXPECT_EQ(lcneNodeCnaInfos[ONE].ports.size(), 2);
    EXPECT_EQ(lcneNodeCnaInfos[ONE].ports[ONE].portId, "1");
    EXPECT_EQ(lcneNodeCnaInfos[ONE].ports[ONE].portCna, "0x040d");
}

TEST_F(TestUbseTopoCna, QueryTopoCna_HttpSendFailed)
{
    std::vector<LcneNodeCnaInfo> lcneNodeCnaInfos;
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_ERROR));

    UbseResult ret = UbseTopoCna::GetInstance().QueryTopoCna(lcneNodeCnaInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseTopoCna, QueryTopoCna_RspStatusFailed)
{
    std::vector<LcneNodeCnaInfo> lcneNodeCnaInfos;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = 404;

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseTopoCna::GetInstance().QueryTopoCna(lcneNodeCnaInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseTopoCna, QueryTopoCna_RspEmpty)
{
    std::vector<LcneNodeCnaInfo> lcneNodeCnaInfos;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseTopoCna::GetInstance().QueryTopoCna(lcneNodeCnaInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseTopoCna, QueryTopoCna_ParseFailed)
{
    std::vector<LcneNodeCnaInfo> lcneNodeCnaInfos;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    const auto func2 = &UbseTopoCna::ParseTopoCnaRsp;
    MOCKER_CPP(func2).stubs().will(returnValue(UBSE_ERROR));

    UbseResult ret = UbseTopoCna::GetInstance().QueryTopoCna(lcneNodeCnaInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseTopoCna, QueryTopoCna_CnaInvalid)
{
    std::vector<LcneNodeCnaInfo> lcneNodeCnaInfos;
    std::vector<LcneNodeCnaInfo> infos;
    LcneNodeCnaInfo info;
    info.slotId = "1";
    info.chipId = "1";
    info.cardId = "1";
    info.busNodeCna = "0x0401";
    std::vector<LcnePortCnaInfo> ports;
    LcnePortCnaInfo port1;
    port1.portId = "0";
    port1.portCna = "invalidcna";
    ports.push_back(port1);
    info.ports = ports;
    infos.push_back(info);

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    const auto func2 = &UbseTopoCna::ParseTopoCnaRsp;
    MOCKER_CPP(func2).stubs().with(rsp.body, outBound(infos)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseTopoCna::GetInstance().QueryTopoCna(lcneNodeCnaInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
}
}  // namespace ubse::lcne