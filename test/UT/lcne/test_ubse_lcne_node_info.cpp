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

#include "test_ubse_lcne_node_info.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_http_module.h"
#include "lcne/ubse_lcne_node_info.h"
#include "ubse_xml.h"
#include "ubse_context.h"
#include "ubse_error.h"

namespace ubse::ut::lcne {
using namespace ubse::lcne;
using namespace ubse::context;

void TestUbseLcneNodeInfo ::SetUp()
{
    Test::SetUp();
}

void TestUbseLcneNodeInfo ::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneNodeInfo, ParseIODieInfoQueryAllResponseSuccess)
{
    std::string responseXml = R"(<vbussw-service xmlns="urn:huawei:yang:huawei-vbussw-service">
  <iou-infos>
    <iou-info>
      <slot-id>1</slot-id>
      <ubpu-id>1</ubpu-id>
      <die-id>1</die-id>
      <bus-controller-eid>0x00000</bus-controller-eid>
      <guid>01-0101-0-1-0101-0101-010101-0101010101</guid>
      <upi>0x0003</upi>
      <primary-cna>0x0085a7</primary-cna>
      <ubpu-type>CPU</ubpu-type>
      <die-status>normal</die-status>
    </iou-info>
  </iou-infos>
</vbussw-service>)";
    UbseLcneIODieInfoMap ubseLcneIODieInfoMap;
    UbseResult ret =
        UbseLcneNodeInfo::GetGetInstance().ParseIODieInfoQueryAllResponse(responseXml, ubseLcneIODieInfoMap);
    DevName devName1("1", "1");
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ("01-0101-0-1-0101-0101-010101-0101010101", ubseLcneIODieInfoMap[devName1].guid);
    EXPECT_EQ("0x0085a7", ubseLcneIODieInfoMap[devName1].primaryCna);
    EXPECT_EQ(ubse::mti::DevType::CPU, ubseLcneIODieInfoMap[devName1].chipType);
}

TEST_F(TestUbseLcneNodeInfo, ParseIODieInfoQueryAllResponseFailed)
{
    std::string responseXml{};
    UbseLcneIODieInfoMap ubseLcneIODieInfoMap;
    UbseResult ret =
        UbseLcneNodeInfo::GetGetInstance().ParseIODieInfoQueryAllResponse(responseXml, ubseLcneIODieInfoMap);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneNodeInfo, ParseIODieInfoQueryAllResponse_ParseDieInfosFailed)
{
    std::string responseXml = R"(<vbussw-service xmlns="urn:huawei:yang:huawei-vbussw-service">
</vbussw-service>)";
    UbseLcneIODieInfoMap ubseLcneIODieInfoMap;
    UbseResult ret =
        UbseLcneNodeInfo::GetGetInstance().ParseIODieInfoQueryAllResponse(responseXml, ubseLcneIODieInfoMap);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneNodeInfo, QueryAllLcneIODieInfoSuccess)
{
    UbseLcneIODieInfoMap ubseLcneIODieInfoMap;
    UbseLcneIODieInfoMap ubmap;
    DevName devName1("1", "1");
    UbseLcneIODieInfo info{};
    info.guid = "01-0101-0-1-0101-0101-010101-0101010101";
    info.primaryCna = "0x0085a7";
    info.chipType = ubse::mti::DevType::CPU;
    ubmap.emplace(devName1, info);
    std::string host = "127.0.0.1";
    int port = 34256;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneNodeInfo::ParseIODieInfoQueryAllResponse;
    MOCKER_CPP(func2).stubs().with(rsp.body, outBound(ubmap)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneNodeInfo::GetGetInstance().QueryAllLcneIODieInfo(ubseLcneIODieInfoMap);

    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ("01-0101-0-1-0101-0101-010101-0101010101", ubseLcneIODieInfoMap[devName1].guid);
    EXPECT_EQ("0x0085a7", ubseLcneIODieInfoMap[devName1].primaryCna);
    EXPECT_EQ(ubse::mti::DevType::CPU, ubseLcneIODieInfoMap[devName1].chipType);
}

TEST_F(TestUbseLcneNodeInfo, QueryAllLcneIODieInfo_HttpSendFailed)
{
    UbseLcneIODieInfoMap ubseLcneIODieInfoMap;

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_ERROR));
    UbseResult ret = UbseLcneNodeInfo::GetGetInstance().QueryAllLcneIODieInfo(ubseLcneIODieInfoMap);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneNodeInfo, QueryAllLcneIODieInfo_RspStatusFailed)
{
    UbseLcneIODieInfoMap ubseLcneIODieInfoMap;
    std::string host = "127.0.0.1";
    int port = 34256;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = 404;

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    UbseResult ret = UbseLcneNodeInfo::GetGetInstance().QueryAllLcneIODieInfo(ubseLcneIODieInfoMap);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneNodeInfo, QueryAllLcneIODieInfo_RspEmpty)
{
    UbseLcneIODieInfoMap ubseLcneIODieInfoMap;
    std::string host = "127.0.0.1";
    int port = 34256;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    UbseResult ret = UbseLcneNodeInfo::GetGetInstance().QueryAllLcneIODieInfo(ubseLcneIODieInfoMap);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneNodeInfo, QueryAllLcneIODieInfo_ParseFailed)
{
    UbseLcneIODieInfoMap ubseLcneIODieInfoMap;
    std::string host = "127.0.0.1";
    int port = 34256;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    const auto func2 = &UbseLcneNodeInfo::ParseIODieInfoQueryAllResponse;
    MOCKER_CPP(func2).stubs().will(returnValue(UBSE_ERROR));
    UbseResult ret = UbseLcneNodeInfo::GetGetInstance().QueryAllLcneIODieInfo(ubseLcneIODieInfoMap);
    EXPECT_EQ(UBSE_ERROR, ret);
}
}  // namespace ubse::ut::lcne