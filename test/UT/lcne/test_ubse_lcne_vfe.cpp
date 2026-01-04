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
#include "test_ubse_lcne_vfe.h"
#include <mockcpp/mockcpp.hpp>
#include "lcne/ubse_lcne_qos.h"
#include "src/framework/misc/ubse_pointer_process.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_http_module.h"
#include "ubse_lcne_vfe_eid.h"
#include "ubse_topology_interface.h"
#include "ubse_xml.h"

namespace ubse::ut::lcne {
using namespace ubse::lcne;
using namespace ubse::context;
using namespace ubse::utils;
using namespace ubse::mti;

void TestUbseLcneVfe ::SetUp()
{
    Test::SetUp();
}

void TestUbseLcneVfe ::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneVfe, GetVfeEid)
{
    UbseLcneIouInfo iouInfo;
    iouInfo.slotId = "1";
    iouInfo.ubpuId = "1";
    iouInfo.iouId = "1";
    std::vector<UbseLcneFeInfo> allFeInfos;
    std::string host = "127.0.0.1";
    int port = 34256;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneVfeEid::ParseGetFeListResponse).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneVfeEid::UpdateVfeEid).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseLcneVfeEid::GetInstance().GetVfeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_OK, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneVfe, GetVfeEid_fail)
{
    UbseLcneIouInfo iouInfo;
    iouInfo.slotId = "1";
    iouInfo.ubpuId = "1";
    iouInfo.iouId = "1";
    std::vector<UbseLcneFeInfo> allFeInfos;
    std::string host = "127.0.0.1";
    int port = 34256;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneVfeEid::ParseGetFeListResponse).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneVfeEid::UpdateVfeEid).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseLcneVfeEid::GetInstance().GetVfeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
    MOCKER_CPP(&UbseLcneVfeEid::ParseGetFeListResponse).stubs().will(returnValue(UBSE_ERROR));
    ret = UbseLcneVfeEid::GetInstance().GetVfeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_BAD_REQ);
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    EXPECT_EQ(UBSE_ERROR, ret);
    rsp.body = "";
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    ret = UbseLcneVfeEid::GetInstance().GetVfeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_ERROR));
    ret = UbseLcneVfeEid::GetInstance().GetVfeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneVfe, UpdateVfeEid)
{
    UbseLcneIouInfo iouInfo;
    iouInfo.slotId = "1";
    iouInfo.ubpuId = "1";
    iouInfo.iouId = "1";
    std::vector<UbseLcneFeInfo> allFeInfos;
    std::string host = "127.0.0.1";
    int port = 34256;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneVfeEid::ParseGetFeEidResponse).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseLcneVfeEid::GetInstance().UpdateVfeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_OK, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneVfe, UpdateVfeEid_fail)
{
    UbseLcneIouInfo iouInfo;
    iouInfo.slotId = "1";
    iouInfo.ubpuId = "1";
    iouInfo.iouId = "1";
    std::vector<UbseLcneFeInfo> allFeInfos;
    std::string host = "127.0.0.1";
    int port = 34256;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneVfeEid::ParseGetFeEidResponse).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseLcneVfeEid::GetInstance().UpdateVfeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
    rsp.body = "";
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    ret = UbseLcneVfeEid::GetInstance().UpdateVfeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_BAD_REQ);
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    ret = UbseLcneVfeEid::GetInstance().UpdateVfeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_ERROR));
    ret = UbseLcneVfeEid::GetInstance().UpdateVfeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneVfe, ParseGetFeListResponse)
{
    const std::string responseStr = R"(<?xml version="1.0" encoding="UTF-8"?>
<mue-ue-binding-info xmlns="urn:huawei:yang:huawei-vbussw-service">
  <slot-id>1</slot-id>
  <ubpu-id>2</ubpu-id>
  <iou-id>3</iou-id>
  <mue-ue-bindings>
    <mue-ue-binding>
      <mue-id>9</mue-id>
      <ue-id>92 93</ue-id>
    </mue-ue-binding>
    <mue-ue-binding>
      <mue-id>10</mue-id>
      <ue-id>94 95</ue-id>
    </mue-ue-binding>
    <mue-ue-binding>
      <mue-id>11</mue-id>
    </mue-ue-binding>
  </mue-ue-bindings>
</mue-ue-binding-info>
)";
    std::vector<UbseLcneFeInfo> allFeInfos;
    auto ret = UbseLcneVfeEid::GetInstance().ParseGetFeListResponse(responseStr, allFeInfos);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(4, allFeInfos.size());
    EXPECT_EQ("1", allFeInfos[0].slotId);
    EXPECT_EQ("2", allFeInfos[0].ubpuId);
    EXPECT_EQ("3", allFeInfos[0].iouId);
    EXPECT_EQ("92", allFeInfos[0].entityId);
    EXPECT_EQ("93", allFeInfos[1].entityId);
    EXPECT_EQ("94", allFeInfos[2].entityId);
    EXPECT_EQ("95", allFeInfos[3].entityId);
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneVfe, ParseGetFeEidResponse)
{
    const std::string responseStr = R"(<?xml version="1.0" encoding="UTF-8"?>
<entity-urma-communication-info xmlns="urn:huawei:yang:huawei-vbussw-service">
  <slot-id>1</slot-id>
  <ubpu-id>2</ubpu-id>
  <iou-id>3</iou-id>
  <urma-communication-entity-ids>
    <urma-communication-entity-id>
      <entity-id>2</entity-id>
      <urma-communication-infos>
        <urma-communication-info>
          <urma-eid>0000:0000:0000:0040:0010:0000:DF00:05C3</urma-eid>
          <upi>0x7FFF</upi>
        </urma-communication-info>
      </urma-communication-infos>
    </urma-communication-entity-id>
    <urma-communication-entity-id>
      <entity-id>92</entity-id>
      <urma-communication-infos>
        <urma-communication-info>
          <urma-eid>0000:0000:0000:0060:0010:0000:DF00:05C4</urma-eid>
          <upi>0x7FFF</upi>
          <interface-name>400GUB8/1/2</interface-name>
        </urma-communication-info>
        <urma-communication-info>
          <urma-eid>0000:0000:0000:0060:0010:0000:DF00:05C5</urma-eid>
          <upi>0x7FFF</upi>
          <port-group-id>1</port-group-id>
        </urma-communication-info>
        <urma-communication-info>
          <urma-eid>0000:0000:0000:0060:0010:0000:DF00:05C6</urma-eid>
          <upi>0x7FFF</upi>
          <interface-name>400GUB8/1/4</interface-name>
        </urma-communication-info>
      </urma-communication-infos>
    </urma-communication-entity-id>
    <urma-communication-entity-id>
      <entity-id>93</entity-id>
      <urma-communication-infos>
        <urma-communication-info>
          <urma-eid>0000:0000:0000:0080:0010:0000:DF00:05C4</urma-eid>
          <upi>0x7FFF</upi>
          <interface-name>400GUB8/2/2</interface-name>
        </urma-communication-info>
        <urma-communication-info>
          <urma-eid>0000:0000:0000:0080:0010:0000:DF00:05C5</urma-eid>
          <upi>0x7FFF</upi>
          <port-group-id>1</port-group-id>
        </urma-communication-info>
        <urma-communication-info>
          <urma-eid>0000:0000:0000:0080:0010:0000:DF00:05C6</urma-eid>
          <upi>0x7FFF</upi>
          <interface-name>400GUB8/2/4</interface-name>
        </urma-communication-info>
      </urma-communication-infos>
    </urma-communication-entity-id>
  </urma-communication-entity-ids>  
</entity-urma-communication-info>
)";
    std::vector<UbseLcneFeInfo> allFeInfos;
    UbseLcneFeInfo feInfo;
    feInfo.slotId = "1";
    feInfo.ubpuId = "2";
    feInfo.iouId = "3";
    feInfo.entityId = "92";
    feInfo.fetype = UbseLcneFeType::VIRTUAL_TYPE;
    allFeInfos.emplace_back(feInfo);
    feInfo.entityId = "93";
    allFeInfos.emplace_back(feInfo);
    auto ret = UbseLcneVfeEid::GetInstance().ParseGetFeEidResponse(responseStr, allFeInfos);
    EXPECT_EQ(UBSE_OK, ret);
    std::string Eid = allFeInfos[0].eidGroups[0].primaryEid;
    EXPECT_EQ("0000:0000:0000:0060:0010:0000:DF00:05C5", Eid);    
    Eid = allFeInfos[0].eidGroups[0].portEids["1"];
    EXPECT_EQ("0000:0000:0000:0060:0010:0000:DF00:05C4", Eid);
    Eid = allFeInfos[0].eidGroups[0].portEids["3"];
    EXPECT_EQ("0000:0000:0000:0060:0010:0000:DF00:05C6", Eid);
    Eid = allFeInfos[0].eidGroups[1].primaryEid;
    EXPECT_EQ("0000:0000:0000:0080:0010:0000:DF00:05C5", Eid);    
    Eid = allFeInfos[1].eidGroups[1].portEids["1"];
    EXPECT_EQ("0000:0000:0000:0080:0010:0000:DF00:05C4", Eid);
    Eid = allFeInfos[1].eidGroups[1].portEids["3"];
    EXPECT_EQ("0000:0000:0000:0080:0010:0000:DF00:05C6", Eid);      
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneVfe, GetPortIdFromInterfaceName)
{
    std::string intfaceName = "400GUB8/2/2";
    uint32_t portId;
    auto ret = UbseLcneVfeEid::GetInstance().GetPortIdFromInterfaceName(intfaceName, portId);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(1, portId);
    intfaceName = "400GUB8/2/1";
    ret = UbseLcneVfeEid::GetInstance().GetPortIdFromInterfaceName(intfaceName, portId);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(0, portId);
    intfaceName = "400GUB8/2/";
    ret = UbseLcneVfeEid::GetInstance().GetPortIdFromInterfaceName(intfaceName, portId);
    EXPECT_EQ(UBSE_ERROR, ret);
    intfaceName = "400GUB8/2/A";
    ret = UbseLcneVfeEid::GetInstance().GetPortIdFromInterfaceName(intfaceName, portId);
    EXPECT_EQ(UBSE_ERROR, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneVfe, FindVfeInVector)
{
    std::string slotId = "1";
    std::string ubpuId = "2";
    std::string iouId = "3";
    std::string entityId = "4";
    std::vector<UbseLcneFeInfo> allFeInfos;
    UbseLcneFeInfo * ret = UbseLcneVfeEid::GetInstance().FindVfeInVector(slotId, ubpuId, iouId, entityId, allFeInfos);
    EXPECT_EQ(nullptr, ret);
    UbseLcneFeInfo feInfo;
    feInfo.slotId = "1";
    feInfo.ubpuId = "2";
    feInfo.iouId = "3";
    feInfo.entityId = "1";
    feInfo.fetype = UbseLcneFeType::VIRTUAL_TYPE;
    allFeInfos.emplace_back(feInfo);
    ret = UbseLcneVfeEid::GetInstance().FindVfeInVector(slotId, ubpuId, iouId, entityId, allFeInfos);
    EXPECT_EQ(nullptr, ret);         
    feInfo.entityId = "2";
    allFeInfos.emplace_back(feInfo);    
    feInfo.entityId = "3";
    allFeInfos.emplace_back(feInfo);   
    feInfo.entityId = "4";
    allFeInfos.emplace_back(feInfo);
    ret = UbseLcneVfeEid::GetInstance().FindVfeInVector(slotId, ubpuId, iouId, entityId, allFeInfos);            
    EXPECT_EQ("4", ret->entityId);
    GlobalMockObject::verify();
}

} // namespace ubse::ut::lcne