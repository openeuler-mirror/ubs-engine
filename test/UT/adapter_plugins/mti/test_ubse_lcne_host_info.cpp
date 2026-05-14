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

#include "test_ubse_lcne_host_info.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_http_module.h"
#include "adapter_plugins/mti/ubse_topology_interface.h"
#include "lcne/ubse_lcne_host_info.h"

namespace ubse::ut::lcne {
using namespace ubse::lcne;
using namespace ubse::context;

void TestUbseLcneHostInfo ::SetUp()
{
    Test::SetUp();
}

void TestUbseLcneHostInfo ::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneHostInfo, ParseHostQueryResponseSuccess)
{
    std::string responseXml = R"(<vbussw-inventory xmlns="urn:huawei:yang:huawei-vbussw-inventory">
  <logic-entities>
    <logic-entity>
      <bus-instance-eid>0x413</bus-instance-eid>
      <guid>12-3456-7-8-abcd-ef01-234512-6789abcd</guid>
      <type>host</type>
      <upi>0x1231</upi>
      <state>online</state>
      <try-times>1</try-times>
    </logic-entity>
  </logic-entities>
</vbussw-inventory>)";
    UbseLcneOSInfo ubseLcneOSInfo;
    UbseResult ret = UbseLcneHostInfo::GetGetInstance().ParseHostQueryResponse(responseXml, ubseLcneOSInfo);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ("0x413", ubseLcneOSInfo.busInstanceEid);
    EXPECT_EQ("12-3456-7-8-abcd-ef01-234512-6789abcd", ubseLcneOSInfo.guid);
    EXPECT_EQ(LogicEntityType::host, ubseLcneOSInfo.logicEntityType);
    EXPECT_EQ("0x1231", ubseLcneOSInfo.upi);
    EXPECT_EQ(LogicEntityStatus::online, ubseLcneOSInfo.logicEntityStatus);
}

TEST_F(TestUbseLcneHostInfo, ParseHostQueryFailed)
{
    std::string responseXml{};
    UbseLcneOSInfo ubseLcneOSInfo;
    UbseResult ret = UbseLcneHostInfo::GetGetInstance().ParseHostQueryResponse(responseXml, ubseLcneOSInfo);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneHostInfo, ParseHostQuery_ParseFailed1)
{
    std::string responseXml = R"(<vbussw-inventory xmlns="urn:huawei:yang:huawei-vbussw-inventory">
    </vbussw-inventory>)";
    UbseLcneOSInfo ubseLcneOSInfo;
    UbseResult ret = UbseLcneHostInfo::GetGetInstance().ParseHostQueryResponse(responseXml, ubseLcneOSInfo);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneHostInfo, ParseHostQuery_ParseFailed2)
{
    std::string responseXml = R"(<vbussw-inventory xmlns="urn:huawei:yang:huawei-vbussw-inventory">
<logic-entities>
  <logic-entity>
    <bus-instance-eid>0x413</bus-instance-eid>
    <guid>12-3456-7-8-abcd-ef01-234512-6789abcd</guid>
    <type>guest</type>
    <upi>0x1231</upi>
    <state>online</state>
    <try-times>1</try-times>
  </logic-entity>
</logic-entities>
</vbussw-inventory>)";
    UbseLcneOSInfo ubseLcneOSInfo;
    UbseResult ret = UbseLcneHostInfo::GetGetInstance().ParseHostQueryResponse(responseXml, ubseLcneOSInfo);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneHostInfo, QueryLcneHostInfo_Success)
{
    UbseLcneOSInfo ubseLcneOSInfo;
    UbseLcneOSInfo info;
    info.busInstanceEid = "0x413";
    info.guid = "12-3456-7-8-abcd-ef01-234512-6789abcd";
    info.logicEntityType = LogicEntityType::host;
    info.upi = "0x1231";
    info.logicEntityStatus = LogicEntityStatus::online;
    LcneServer::isTcpServer = true;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneHostInfo::ParseHostQueryResponse;
    MOCKER_CPP(func2).stubs().with(rsp.body, outBound(info)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneHostInfo::GetGetInstance().QueryLcneHostInfo(ubseLcneOSInfo);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ("0x413", ubseLcneOSInfo.busInstanceEid);
    EXPECT_EQ("12-3456-7-8-abcd-ef01-234512-6789abcd", ubseLcneOSInfo.guid);
    EXPECT_EQ(LogicEntityType::host, ubseLcneOSInfo.logicEntityType);
    EXPECT_EQ("0x1231", ubseLcneOSInfo.upi);
    EXPECT_EQ(LogicEntityStatus::online, ubseLcneOSInfo.logicEntityStatus);
}

TEST_F(TestUbseLcneHostInfo, QueryLcneHostInfo_HttpSendFailed)
{
    UbseLcneOSInfo ubseLcneOSInfo;
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_ERROR));

    UbseResult ret = UbseLcneHostInfo::GetGetInstance().QueryLcneHostInfo(ubseLcneOSInfo);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneHostInfo, QueryLcneHostInfo_RspStatusFailed)
{
    UbseLcneOSInfo ubseLcneOSInfo;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = 404;

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneHostInfo::GetGetInstance().QueryLcneHostInfo(ubseLcneOSInfo);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneHostInfo, QueryLcneHostInfo_RspEmpty)
{
    UbseLcneOSInfo ubseLcneOSInfo;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneHostInfo::GetGetInstance().QueryLcneHostInfo(ubseLcneOSInfo);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneHostInfo, QueryLcneHostInfo_ParseFailed)
{
    UbseLcneOSInfo ubseLcneOSInfo;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneHostInfo::ParseHostQueryResponse;
    MOCKER_CPP(func2).stubs().will(returnValue(UBSE_ERROR));

    UbseResult ret = UbseLcneHostInfo::GetGetInstance().QueryLcneHostInfo(ubseLcneOSInfo);
    EXPECT_EQ(UBSE_ERROR, ret);
}
} // namespace ubse::ut::lcne