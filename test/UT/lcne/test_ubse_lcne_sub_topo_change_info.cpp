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

#include "lcne/ubse_lcne_sub_topo_change_info.h"
#include "src/framework/http/ubse_http_module.h"
#include "ubse_context.h"
#include "ubse_error.h"

namespace ubse::lcne {
using namespace ubse::context;

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
    std::string host = "127.0.0.1";
    int port = 8799;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
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
    std::string host = "127.0.0.1";
    int port = 8799;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = 404;

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneLinkInfo::GetInstance().SubLcneLinkInfo();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneLinkInfo, SubLcneLinkInfo_RspEmpty)
{
    std::string host = "127.0.0.1";
    int port = 8799;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneLinkInfo::GetInstance().SubLcneLinkInfo();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneLinkInfo, SubLcneLinkInfo_ParseFailed)
{
    std::string host = "127.0.0.1";
    int port = 8799;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
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
}  // namespace ubse::lcne