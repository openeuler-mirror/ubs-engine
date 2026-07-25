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
#include "test_ubse_lcne_fe.h"

#include <mockcpp/mockcpp.hpp>
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_http_module.h"
#include "lcne/ubse_lcne_fe_eid.h"

namespace ubse::lcne {
using namespace ubse::adapter_plugins::mti;
using namespace ubse::http;

void TestUbseLcneFe ::SetUp()
{
    Test::SetUp();
}

void TestUbseLcneFe ::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneFe, GetFeEid)
{
    UbseMtiIouInfo iouInfo;
    std::vector<UbseMtiFeInfo> allFeInfos;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneFeEid::ParseGetFeEidResponse).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneFeEid::UpdateFeType).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseLcneFeEid::GetInstance().GetFeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneFe, GetFeEid_FailUpdateType)
{
    UbseMtiIouInfo iouInfo;
    std::vector<UbseMtiFeInfo> allFeInfos;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneFeEid::ParseGetFeEidResponse).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneFeEid::UpdateFeType).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseLcneFeEid::GetInstance().GetFeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneFe, GetFeEid_RspError)
{
    UbseMtiIouInfo iouInfo;
    std::vector<UbseMtiFeInfo> allFeInfos;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_ERROR));
    auto ret = UbseLcneFeEid::GetInstance().GetFeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_BAD_REQ);
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    ret = UbseLcneFeEid::GetInstance().GetFeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "";
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    ret = UbseLcneFeEid::GetInstance().GetFeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneFe, GetFeEid_ParseResponseFailed)
{
    UbseMtiIouInfo iouInfo;
    std::vector<UbseMtiFeInfo> allFeInfos;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneFeEid::ParseGetFeEidResponse).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseLcneFeEid::GetInstance().GetFeEid(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneFe, GetComUrmaEidClos)
{
    MOCKER_CPP(&UbseLcneFeEid::GetFeEid).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneFeEid::GetComEidInfo).stubs().will(returnValue(UBSE_OK));
    UbseMtiEidGroup feInfo;
    UbseMtiIouInfo iouInfo;
    UbseResult ret = UbseLcneFeEid::GetInstance().GetComUrmaEidClos(iouInfo, feInfo);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneFe, GetComUrmaEidClos_Fail)
{
    MOCKER_CPP(&UbseLcneFeEid::GetFeEid).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneFeEid::GetComEidInfo).stubs().will(returnValue(UBSE_ERROR));
    UbseMtiEidGroup feInfo;
    UbseMtiIouInfo iouInfo;
    UbseResult ret = UbseLcneFeEid::GetInstance().GetComUrmaEidClos(iouInfo, feInfo);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneFe, UpdateFeType)
{
    UbseMtiIouInfo iouInfo;
    std::vector<UbseMtiFeInfo> allFeInfos;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneFeEid::ParseFeTypeListResponse).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseLcneFeEid::GetInstance().UpdateFeType(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneFe, UpdateFeType_fail)
{
    UbseMtiIouInfo iouInfo;
    std::vector<UbseMtiFeInfo> allFeInfos;

    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseLcneFeEid::ParseFeTypeListResponse).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseLcneFeEid::GetInstance().UpdateFeType(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
    rsp.body = "";
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    ret = UbseLcneFeEid::GetInstance().UpdateFeType(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_BAD_REQ);
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    ret = UbseLcneFeEid::GetInstance().UpdateFeType(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
    MOCKER_CPP(func1).stubs().with(outBound(req), outBound(rsp)).will(returnValue(UBSE_ERROR));
    ret = UbseLcneFeEid::GetInstance().UpdateFeType(iouInfo, allFeInfos);
    EXPECT_EQ(UBSE_ERROR, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneFe, ParseFeTypeListResponse)
{
    const std::string responseStr = R"(<vbussw-service xmlns="urn:huawei:yang:huawei-vbussw-service">
    <mue-ue-binding-infos>
    <mue-ue-binding-info>
    <slot-id>1</slot-id>
    <ubpu-id>1</ubpu-id>
    <iou-id>1</iou-id>
    <mue-ue-bindings>
        <mue-ue-binding>
        <mue-id>2</mue-id>
        <ue-id>78</ue-id>
        </mue-ue-binding>
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
    </mue-ue-binding-infos>
    </vbussw-service>
    )";
    std::vector<UbseMtiFeInfo> allFeInfos;
    allFeInfos.push_back({"1", "1", "1", "2"});
    allFeInfos.push_back({"1", "1", "1", "9"});
    allFeInfos.push_back({"1", "1", "1", "93"});
    auto ret = UbseLcneFeEid::GetInstance().ParseFeTypeListResponse(responseStr, allFeInfos);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ("2", allFeInfos[0].entityId);
    EXPECT_EQ(UbseMtiFeType::PHYSICAL_TYPE, allFeInfos[0].fetype);
    EXPECT_EQ("9", allFeInfos[1].entityId);
    EXPECT_EQ(UbseMtiFeType::PHYSICAL_TYPE, allFeInfos[1].fetype);
    EXPECT_EQ("93", allFeInfos[2].entityId);
    EXPECT_EQ(UbseMtiFeType::VIRTUAL_TYPE, allFeInfos[2].fetype);
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneFe, ParseGetFeEidResponse)
{
    const std::string responseStr = R"(<vbussw-service xmlns="urn:huawei:yang:huawei-vbussw-service">
    <entity-urma-communication-infos>
    <entity-urma-communication-info>
    <slot-id>1</slot-id>
    <ubpu-id>2</ubpu-id>
    <iou-id>3</iou-id>
    <urma-communication-entity-ids>
        <urma-communication-entity-id>
        <entity-id>2</entity-id>
        <urma-communication-infos>
            <urma-communication-info>
                <urma-eid>0000:0000:0000:0040:0010:0000:fd00:05c1</urma-eid>
                <upi>0x7FFF</upi>
                <interface-name>400GUB1/1/2</interface-name>
            </urma-communication-info>
            <urma-communication-info>
                <urma-eid>0000:0000:0000:0060:0010:0000:fd00:05c2</urma-eid>
                <upi>0x7FFF</upi>
                <port-group-id>1</port-group-id>
            </urma-communication-info>
            <urma-communication-info>
                <urma-eid>0000:0000:0000:0060:0010:0000:fd00:05c3</urma-eid>
                <upi>0x7FFF</upi>
                <interface-name>400GUB1/1/4</interface-name>
            </urma-communication-info>
        </urma-communication-infos>
        </urma-communication-entity-id>
        <urma-communication-entity-id>
        <entity-id>92</entity-id>
        <urma-communication-infos>
            <urma-communication-info>
                <urma-eid>0000:0000:0000:0060:0010:0000:fd00:05c4</urma-eid>
                <upi>0x7FFF</upi>
                <interface-name>400GUB1/1/2</interface-name>
            </urma-communication-info>
            <urma-communication-info>
                <urma-eid>0000:0000:0000:0060:0010:0000:fd00:05c5</urma-eid>
                <upi>0x7FFF</upi>
                <port-group-id>1</port-group-id>
            </urma-communication-info>
            <urma-communication-info>
                <urma-eid>0000:0000:0000:0060:0010:0000:fd00:05c6</urma-eid>
                <upi>0x7FFF</upi>
                <interface-name>400GUB1/1/4</interface-name>
            </urma-communication-info>
        </urma-communication-infos>
        </urma-communication-entity-id>
        <urma-communication-entity-id>
        <entity-id>93</entity-id>
        <urma-communication-infos>
            <urma-communication-info>
                <urma-eid>0000:0000:0000:0080:0010:0000:fd00:05c4</urma-eid>
                <upi>0x7FFF</upi>
                <interface-name>400GUB1/1/2</interface-name>
            </urma-communication-info>
            <urma-communication-info>
                <urma-eid>0000:0000:0000:0080:0010:0000:fd00:05c5</urma-eid>
                <upi>0x7FFF</upi>
                <port-group-id>1</port-group-id>
            </urma-communication-info>
            <urma-communication-info>
                <urma-eid>0000:0000:0000:0080:0010:0000:fd00:05c6</urma-eid>
                <upi>0x7FFF</upi>
                <interface-name>400GUB1/1/4</interface-name>
            </urma-communication-info>
        </urma-communication-infos>
        </urma-communication-entity-id>
    </urma-communication-entity-ids>
    </entity-urma-communication-info>
    </entity-urma-communication-infos>
    </vbussw-service>
    )";
    std::vector<UbseMtiFeInfo> allFeInfos;
    auto ret = UbseLcneFeEid::GetInstance().ParseGetFeEidResponse(responseStr, allFeInfos);
    EXPECT_EQ(UBSE_OK, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneFe, GetPortIdFromInterfaceName)
{
    std::string intfaceName = "400GUB1/1/2";
    uint32_t portId;
    auto ret = UbseLcneFeEid::GetInstance().GetPortIdFromInterfaceName(intfaceName, portId);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(1, portId);
    intfaceName = "400GUB1/1/1";
    ret = UbseLcneFeEid::GetInstance().GetPortIdFromInterfaceName(intfaceName, portId);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(0, portId);
    intfaceName = "400GUB1/1/";
    ret = UbseLcneFeEid::GetInstance().GetPortIdFromInterfaceName(intfaceName, portId);
    EXPECT_EQ(UBSE_ERROR, ret);
    intfaceName = "400GUB1/1/A";
    ret = UbseLcneFeEid::GetInstance().GetPortIdFromInterfaceName(intfaceName, portId);
    EXPECT_EQ(UBSE_ERROR, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneFe, FindVfeInVector)
{
    std::string entityId = "4";
    UbseMtiIouInfo iouInfo{"1", "1", "1"};
    std::vector<UbseMtiFeInfo> allFeInfos;
    UbseMtiFeInfo* ret = UbseLcneFeEid::GetInstance().FindVfeInVector(iouInfo, entityId, allFeInfos);
    EXPECT_EQ(nullptr, ret);
    UbseMtiFeInfo feInfo;
    feInfo.slotId = "1";
    feInfo.ubpuId = "2";
    feInfo.iouId = "3";
    feInfo.entityId = "1";
    feInfo.fetype = UbseMtiFeType::VIRTUAL_TYPE;
    allFeInfos.emplace_back(feInfo);
    ret = UbseLcneFeEid::GetInstance().FindVfeInVector(iouInfo, entityId, allFeInfos);
    EXPECT_EQ(nullptr, ret);
    GlobalMockObject::verify();
}
} // namespace ubse::lcne
