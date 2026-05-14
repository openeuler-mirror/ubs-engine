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

#include "test_ubse_lcne_businstance.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "src/framework/http/ubse_http_module.h"
namespace ubse::ut::lcne {
using namespace ubse::lcne;
using namespace ubse::context;

void TestUbseLcneBusInstance ::SetUp()
{
    Test::SetUp();
}

void TestUbseLcneBusInstance ::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneBusInstance, ParseQueryBusinstanceResponse)
{
    std::string responseXml = R"(<vbussw-inventory xmlns="urn:huawei:yang:huawei-vbussw-inventory">
     <logic-entity-mappings>
       <logic-entity-mapping>
         <host-bus-instance-eid>0x10401</host-bus-instance-eid>
         <physical-entity-mappings>
           <physical-entity-mapping>
             <slot-id>1</slot-id>
             <chip-id>1</chip-id>
             <die-id>1</die-id>
           </physical-entity-mapping>
           <physical-entity-mapping>
             <slot-id>1</slot-id>
             <chip-id>2</chip-id>
             <die-id>1</die-id>
           </physical-entity-mapping>
         </physical-entity-mappings>
       </logic-entity-mapping>
     </logic-entity-mappings>
   </vbussw-inventory>
   )";
    UbseLcneBusInstanceInfo busInstanceInfo;
    EXPECT_EQ(UbseLcneBusInstance::GetInstance().ParseQueryBusinstanceResponse(responseXml, busInstanceInfo), UBSE_OK);
    EXPECT_EQ(busInstanceInfo.hostBusinstanceEid, "0x10401");
    EXPECT_EQ(busInstanceInfo.localNodeId, "1");
}

TEST_F(TestUbseLcneBusInstance, ParseQueryBusinstanceResponse_ParseFailed)
{
    std::string responseXml{};
    UbseLcneBusInstanceInfo busInstanceInfo;
    EXPECT_EQ(UbseLcneBusInstance::GetInstance().ParseQueryBusinstanceResponse(responseXml, busInstanceInfo),
              UBSE_ERROR);
}

TEST_F(TestUbseLcneBusInstance, ParseQueryBusinstanceResponse_ParseFailed2)
{
    std::string responseXml = R"(<vbussw-inventory xmlns="urn:huawei:yang:huawei-vbussw-inventory">
   </vbussw-inventory>)";
    UbseLcneBusInstanceInfo busInstanceInfo;
    EXPECT_EQ(UbseLcneBusInstance::GetInstance().ParseQueryBusinstanceResponse(responseXml, busInstanceInfo),
              UBSE_ERROR);
}

TEST_F(TestUbseLcneBusInstance, ParseQueryBusinstanceResponse_ParseFailed3)
{
    std::string responseXml = R"(<vbussw-inventory xmlns="urn:huawei:yang:huawei-vbussw-inventory">
   <logic-entity-mappings>
   </logic-entity-mappings>
   </vbussw-inventory>)";
    UbseLcneBusInstanceInfo busInstanceInfo;
    EXPECT_EQ(UbseLcneBusInstance::GetInstance().ParseQueryBusinstanceResponse(responseXml, busInstanceInfo),
              UBSE_ERROR);
}

TEST_F(TestUbseLcneBusInstance, ParseQueryBusinstanceResponse_ParseFailed4)
{
    std::string responseXml = R"(<vbussw-inventory xmlns="urn:huawei:yang:huawei-vbussw-inventory">
   <logic-entity-mappings>
     <logic-entity-mapping>
       <host-bus-instance-eid>0x10401</host-bus-instance-eid>
     </logic-entity-mapping>
   </logic-entity-mappings>
   </vbussw-inventory>)";
    UbseLcneBusInstanceInfo busInstanceInfo;
    EXPECT_EQ(UbseLcneBusInstance::GetInstance().ParseQueryBusinstanceResponse(responseXml, busInstanceInfo),
              UBSE_ERROR);
}

TEST_F(TestUbseLcneBusInstance, ParseQueryBusinstanceResponse_ParseFailed5)
{
    std::string responseXml = R"(<vbussw-inventory xmlns="urn:huawei:yang:huawei-vbussw-inventory">
   <logic-entity-mappings>
     <logic-entity-mapping>
       <host-bus-instance-eid>0x10401</host-bus-instance-eid>
       <physical-entity-mappings>
       </physical-entity-mappings>
     </logic-entity-mapping>
   </logic-entity-mappings>
   </vbussw-inventory>)";
    UbseLcneBusInstanceInfo busInstanceInfo;
    EXPECT_EQ(UbseLcneBusInstance::GetInstance().ParseQueryBusinstanceResponse(responseXml, busInstanceInfo),
              UBSE_ERROR);
}

TEST_F(TestUbseLcneBusInstance, QueryBusinstance_Success)
{
    UbseLcneBusInstanceInfo busInstanceInfo;
    UbseLcneBusInstanceInfo info;
    info.localNodeId = "1";
    info.hostBusinstanceEid = "0x10401";

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneBusInstance::ParseQueryBusinstanceResponse;
    MOCKER_CPP(func2).stubs().with(rsp.body, outBound(info)).will(returnValue(UBSE_OK));

    EXPECT_EQ(UbseLcneBusInstance::GetInstance().QueryBusinstance(busInstanceInfo), UBSE_OK);
    EXPECT_EQ(busInstanceInfo.hostBusinstanceEid, "0x10401");
    EXPECT_EQ(busInstanceInfo.localNodeId, "1");
}

TEST_F(TestUbseLcneBusInstance, QueryBusinstance_SlotIdInvalid)
{
    UbseLcneBusInstanceInfo busInstanceInfo;
    UbseLcneBusInstanceInfo info;
    info.localNodeId = "abc";
    info.hostBusinstanceEid = "0x10401";

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneBusInstance::ParseQueryBusinstanceResponse;
    MOCKER_CPP(func2).stubs().with(rsp.body, outBound(info)).will(returnValue(UBSE_OK));

    EXPECT_EQ(UbseLcneBusInstance::GetInstance().QueryBusinstance(busInstanceInfo), UBSE_ERROR);
}

TEST_F(TestUbseLcneBusInstance, QueryBusinstance_HttpSendFailed)
{
    UbseLcneBusInstanceInfo ubseLcneBusInstanceInfo;

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_ERROR));

    UbseLcneBusInstanceInfo busInstanceInfo;
    EXPECT_EQ(UbseLcneBusInstance::GetInstance().QueryBusinstance(busInstanceInfo), UBSE_ERROR);
}

TEST_F(TestUbseLcneBusInstance, QueryBusinstance_RspStatusFailed)
{
    UbseLcneBusInstanceInfo busInstanceInfo;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = 404;

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    EXPECT_EQ(UbseLcneBusInstance::GetInstance().QueryBusinstance(busInstanceInfo), UBSE_ERROR);
}

TEST_F(TestUbseLcneBusInstance, QueryBusinstance_RspEmpty)
{
    UbseLcneBusInstanceInfo busInstanceInfo;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    EXPECT_EQ(UbseLcneBusInstance::GetInstance().QueryBusinstance(busInstanceInfo), UBSE_ERROR);
}

TEST_F(TestUbseLcneBusInstance, QueryBusinstance_ParseFailed)
{
    UbseLcneBusInstanceInfo busInstanceInfo;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneBusInstance::ParseQueryBusinstanceResponse;
    MOCKER_CPP(func2).stubs().will(returnValue(UBSE_ERROR));

    EXPECT_EQ(UbseLcneBusInstance::GetInstance().QueryBusinstance(busInstanceInfo), UBSE_ERROR);
}
} // namespace ubse::ut::lcne