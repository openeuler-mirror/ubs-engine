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

#include "test_ubse_lcne_sub_topo_change_info.h"

#include <mockcpp/mokc.h>

#include "ubse_context.h"
#include "ubse_error.h"
#include "lcne/ubse_lcne_sub_topo_change_info.h"
#include "src/framework/http/ubse_http_module.h"

namespace ubse::lcne {
using namespace ubse::context;
using namespace ubse::http;

void TestUbseLcneLinkInfo::SetUp()
{
    Test::SetUp();
}

void TestUbseLcneLinkInfo::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneLinkInfo, SubLcneLinkInfo_Success)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneLinkInfo::ParseMonitorData;
    MOCKER_CPP(func2).stubs().will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneLinkInfo::GetInstance().SubLcneLinkInfo();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneLinkInfo, SubLcneLinkInfo_HttpSendFailed)
{
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_ERROR));

    UbseResult ret = UbseLcneLinkInfo::GetInstance().SubLcneLinkInfo();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneLinkInfo, SubLcneLinkInfo_RspStatusFailed)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = 404;

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneLinkInfo::GetInstance().SubLcneLinkInfo();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneLinkInfo, SubLcneLinkInfo_RspEmpty)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneLinkInfo::GetInstance().SubLcneLinkInfo();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneLinkInfo, SubLcneLinkInfo_ParseFailed)
{
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneLinkInfo::ParseMonitorData;
    MOCKER_CPP(func2).stubs().will(returnValue(UBSE_ERROR));

    UbseResult ret = UbseLcneLinkInfo::GetInstance().SubLcneLinkInfo();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneLinkInfo, ParseMonitorData_Success)
{
    std::string resBody = R"(<data>
    <rpc_reply>
    <result>
    ok
    </result>
    </rpc_reply>
    </data>)";
    UbseResult ret = UbseLcneLinkInfo::GetInstance().ParseMonitorData(resBody);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseLcneLinkInfo, ParseMonitorData_RspEmpty)
{
    std::string resBody{};
    UbseResult ret = UbseLcneLinkInfo::GetInstance().ParseMonitorData(resBody);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneLinkInfo, ParseMonitorData_RspInvalid)
{
    std::string resBody = R"(<data>
    </data>)";
    UbseResult ret = UbseLcneLinkInfo::GetInstance().ParseMonitorData(resBody);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneLinkInfo, ParseMonitorData_RspInvalidResult)
{
    std::string resBody = R"(<data>
    <rpc_reply>
    </rpc_reply>
    </data>)";
    UbseResult ret = UbseLcneLinkInfo::GetInstance().ParseMonitorData(resBody);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneLinkInfo, ParseLinkUpDownReq_LinkUpSuccess)
{
    std::string reqBody = R"(<notification>
    <eventTime>2025-01-01T00:00:00Z</eventTime>
    <link-up xmlns="urn:ietf:params:xml:ns:netconf:notification:1.0">
    <index>0</index>
    <admin-status>UP</admin-status>
    <oper-status>UP</oper-status>
    <name>400GUB1/1/1</name>
    <main-if-name>400GUB1/1/1</main-if-name>
    </link-up>
    </notification>)";
    std::string linkUpDown;
    std::string interfaceName;
    UbseResult ret = UbseLcneLinkInfo::GetInstance().ParseLinkUpDownReq(reqBody, linkUpDown, interfaceName);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(linkUpDown, "link-up");
    EXPECT_EQ(interfaceName, "400GUB1/1/1");
}

TEST_F(TestUbseLcneLinkInfo, ParseLinkUpDownReq_LinkDownSuccess)
{
    std::string reqBody = R"(<notification>
    <eventTime>2025-01-01T00:00:00Z</eventTime>
    <link-down xmlns="urn:ietf:params:xml:ns:netconf:notification:1.0">
    <index>0</index>
    <admin-status>DOWN</admin-status>
    <oper-status>DOWN</oper-status>
    <name>400GUB1/1/1</name>
    <main-if-name>400GUB1/1/1</main-if-name>
    </link-down>
    </notification>)";
    std::string linkUpDown;
    std::string interfaceName;
    UbseResult ret = UbseLcneLinkInfo::GetInstance().ParseLinkUpDownReq(reqBody, linkUpDown, interfaceName);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(linkUpDown, "link-down");
    EXPECT_EQ(interfaceName, "400GUB1/1/1");
}

TEST_F(TestUbseLcneLinkInfo, ParseLinkUpDownReq_RspEmpty)
{
    std::string reqBody{};
    std::string linkUpDown;
    std::string interfaceName;
    UbseResult ret = UbseLcneLinkInfo::GetInstance().ParseLinkUpDownReq(reqBody, linkUpDown, interfaceName);
    EXPECT_EQ(ret, UBSE_ERROR);
    EXPECT_EQ(linkUpDown, "");
    EXPECT_EQ(interfaceName, "");
}

TEST_F(TestUbseLcneLinkInfo, ParseLinkUpDownReq_RspInvalid)
{
    std::string reqBody = R"(<notification>
    <eventTime>2025-01-01T00:00:00Z</eventTime>
    <netconf xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
    <UserName>admin</UserName>
    <session-id>123456</session-id>
    </netconf>
    </notification>)";
    std::string linkUpDown;
    std::string interfaceName;
    UbseResult ret = UbseLcneLinkInfo::GetInstance().ParseLinkUpDownReq(reqBody, linkUpDown, interfaceName);
    EXPECT_EQ(ret, UBSE_ERROR);
    EXPECT_EQ(linkUpDown, "");
    EXPECT_EQ(interfaceName, "");
}
} // namespace ubse::lcne