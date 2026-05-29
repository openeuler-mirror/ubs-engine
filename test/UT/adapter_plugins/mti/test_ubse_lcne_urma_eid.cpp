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

#include "test_ubse_lcne_urma_eid.h"

#include <mockcpp/mokc.h>

#include "ubse_context.h"
#include "ubse_error.h"
#include "adapter_plugins/mti/ubse_mti_def.h"
#include "lcne/ubse_lcne_urma_eid.h"
#include "src/framework/http/ubse_http_module.h"
namespace ubse::lcne {
using namespace ubse::context;
using namespace adapter_plugins::mti;
using namespace ubse::http;
void TestUbseLcneUrmaEid::SetUp()
{
    Test::SetUp();
    auto module = std::make_shared<UbseHttpModule>();
    MOCKER(&UbseContext::GetModule<UbseHttpModule>).stubs().will(returnValue(module));
}

void TestUbseLcneUrmaEid::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneUrmaEid, ParseXmlData)
{
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap{};
    std::string responseXml = R"(<vbussw-service xmlns="urn:huawei:yang:huawei-vbussw-service">
     <static-urma-eids>
       <static-urma-eid>
         <slot-id>1</slot-id>
         <ubpu-id>0</ubpu-id>
         <iou-id>1</iou-id>
         <entity-id>2</entity-id>
         <fe-id>1</fe-id>
         <urma-eid-infos>
           <urma-eid-info>
             <urma-eid>1234:5678:8765:4321:1234:5678:8765:4321</urma-eid>
             <port-group-id>1</port-group-id>
           </urma-eid-info>
           <urma-eid-info>
             <urma-eid>1234:5678:8765:4321:1234:5678:8765:4325</urma-eid>
             <physical-port>2</physical-port>
           </urma-eid-info>
         </urma-eid-infos>
       </static-urma-eid>
       <static-urma-eid>
         <slot-id>1</slot-id>
         <ubpu-id>0</ubpu-id>
         <iou-id>1</iou-id>
         <entity-id>2</entity-id>
         <fe-id>2</fe-id>
         <urma-eid-infos>
           <urma-eid-info>
             <urma-eid>1234:5678:8765:4321:1234:5678:8765:4300</urma-eid>
             <port-group-id>1</port-group-id>
           </urma-eid-info>
           <urma-eid-info>
             <urma-eid>1234:5678:8765:4321:1234:5678:8765:4301</urma-eid>
             <physical-port>2</physical-port>
           </urma-eid-info>
         </urma-eid-infos>
       </static-urma-eid>
       <static-urma-eid>
         <slot-id>2</slot-id>
         <ubpu-id>1</ubpu-id>
         <iou-id>1</iou-id>
         <entity-id>2</entity-id>
         <fe-id>2</fe-id>
         <urma-eid-infos>
           <urma-eid-info>
             <urma-eid>1234:5678:8765:4321:1234:5678:8765:4322</urma-eid>
             <physical-port>1</physical-port>
           </urma-eid-info>
           <urma-eid-info>
             <urma-eid>1234:5678:8765:4321:1234:5678:8765:4323</urma-eid>
             <port-group-id>2</port-group-id>
           </urma-eid-info>
         </urma-eid-infos>
       </static-urma-eid>
       <static-urma-eid>
         <slot-id>3</slot-id>
         <ubpu-id>1</ubpu-id>
         <iou-id>1</iou-id>
         <entity-id>2</entity-id>
         <fe-id>3</fe-id>
         <urma-eid-infos>
           <urma-eid-info>
             <urma-eid>1234:5678:8765:4321:1234:5678:8765:4324</urma-eid>
             <physical-port>1</physical-port>
           </urma-eid-info>
           <urma-eid-info>
             <urma-eid>1234:5678:8765:4321:1234:5678:8765:4326</urma-eid>
             <port-group-id>2</port-group-id>
           </urma-eid-info>
         </urma-eid-infos>
       </static-urma-eid>
     </static-urma-eids>
   </vbussw-service>)";

    EXPECT_EQ(UbseLcneUrmaEid::GetInstance().ParseGetUrmaEidResponse(responseXml, socketInfoMap), UBSE_OK);
    UbseDevName devName1("1", "0");
    EXPECT_EQ(socketInfoMap[devName1].primaryEid, "1234:5678:8765:4321:1234:5678:8765:4321");
    EXPECT_EQ(socketInfoMap[devName1].portEidList["2"], "1234:5678:8765:4321:1234:5678:8765:4325");
    UbseDevName devName2("2", "1");
    EXPECT_EQ(socketInfoMap[devName2].primaryEid, "1234:5678:8765:4321:1234:5678:8765:4323");
    EXPECT_EQ(socketInfoMap[devName2].portEidList["1"], "1234:5678:8765:4321:1234:5678:8765:4322");
    UbseDevName devName3("3", "1");
    EXPECT_EQ(socketInfoMap[devName3].primaryEid, "1234:5678:8765:4321:1234:5678:8765:4326");
    EXPECT_EQ(socketInfoMap[devName3].portEidList["1"], "1234:5678:8765:4321:1234:5678:8765:4324");
}

TEST_F(TestUbseLcneUrmaEid, ParseXmlData_ParseFailed)
{
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap{};
    std::string responseXml{};

    UbseResult ret = UbseLcneUrmaEid::GetInstance().ParseGetUrmaEidResponse(responseXml, socketInfoMap);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneUrmaEid, ParseXmlData_ParseFailed1)
{
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap{};
    std::string responseXml = R"(<vbussw-service xmlns="urn:huawei:yang:huawei-vbussw-service">
   </vbussw-service>)";

    UbseResult ret = UbseLcneUrmaEid::GetInstance().ParseGetUrmaEidResponse(responseXml, socketInfoMap);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneUrmaEid, ParseXmlData_ParseFailed2)
{
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap{};
    std::string responseXml = R"(<vbussw-service xmlns="urn:huawei:yang:huawei-vbussw-service">
     <static-urma-eids>
       <static-urma-eid>
         <slot-id>1</slot-id>
         <ubpu-id>0</ubpu-id>
         <iou-id>1</iou-id>
         <entity-id>2</entity-id>
         <fe-id>1</fe-id>
       </static-urma-eid>
     </static-urma-eids>
   </vbussw-service>)";

    UbseResult ret = UbseLcneUrmaEid::GetInstance().ParseGetUrmaEidResponse(responseXml, socketInfoMap);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseLcneUrmaEid, GetUrmaEid_Success)
{
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap;
    std::map<UbseDevName, UbseUrmaEidInfo> infomap;
    UbseDevName devName1("1", "0");
    UbseUrmaEidInfo info;
    info.primaryEid = "1234:5678:8765:4321:1234:5678:8765:4321";
    std::string urmaEid = "1234:5678:8765:4321:1234:5678:8765:4325";
    info.portEidList.emplace("2", urmaEid);
    infomap.emplace(devName1, info);
    UbseDevName devName2("2", "1");
    UbseUrmaEidInfo info1;
    info1.primaryEid = "1234:5678:8765:4321:1234:5678:8765:4323";
    std::string urmaEid1 = "1234:5678:8765:4321:1234:5678:8765:4322";
    info1.portEidList.emplace("1", urmaEid1);
    infomap.emplace(devName2, info1);
    UbseDevName devName3("3", "1");
    UbseUrmaEidInfo info2;
    info2.primaryEid = "1234:5678:8765:4321:1234:5678:8765:4326";
    std::string urmaEid2 = "1234:5678:8765:4321:1234:5678:8765:4324";
    info2.portEidList.emplace("1", urmaEid2);
    infomap.emplace(devName3, info2);
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneUrmaEid::ParseGetUrmaEidResponse;
    MOCKER_CPP(func2).stubs().with(rsp.body, outBound(infomap)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneUrmaEid::GetInstance().GetUrmaEid(socketInfoMap);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(socketInfoMap[devName1].primaryEid, "1234:5678:8765:4321:1234:5678:8765:4321");
    EXPECT_EQ(socketInfoMap[devName1].portEidList["2"], "1234:5678:8765:4321:1234:5678:8765:4325");
    EXPECT_EQ(socketInfoMap[devName2].primaryEid, "1234:5678:8765:4321:1234:5678:8765:4323");
    EXPECT_EQ(socketInfoMap[devName2].portEidList["1"], "1234:5678:8765:4321:1234:5678:8765:4322");
    EXPECT_EQ(socketInfoMap[devName3].primaryEid, "1234:5678:8765:4321:1234:5678:8765:4326");
    EXPECT_EQ(socketInfoMap[devName3].portEidList["1"], "1234:5678:8765:4321:1234:5678:8765:4324");
}

TEST_F(TestUbseLcneUrmaEid, GetUrmaEid_HttpSendFailed)
{
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap;
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().will(returnValue(UBSE_ERROR));

    UbseResult ret = UbseLcneUrmaEid::GetInstance().GetUrmaEid(socketInfoMap);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneUrmaEid, GetUrmaEid_RspStatusFailed)
{
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = 404;

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneUrmaEid::GetInstance().GetUrmaEid(socketInfoMap);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneUrmaEid, GetUrmaEid_RspEmpty)
{
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseResult ret = UbseLcneUrmaEid::GetInstance().GetUrmaEid(socketInfoMap);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneUrmaEid, GetUrmaEid_ParseFailed)
{
    std::map<UbseDevName, UbseUrmaEidInfo> socketInfoMap;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";

    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneUrmaEid::ParseGetUrmaEidResponse;
    MOCKER_CPP(func2).stubs().will(returnValue(UBSE_ERROR));

    UbseResult ret = UbseLcneUrmaEid::GetInstance().GetUrmaEid(socketInfoMap);
    EXPECT_EQ(UBSE_ERROR, ret);
}
} // namespace ubse::lcne