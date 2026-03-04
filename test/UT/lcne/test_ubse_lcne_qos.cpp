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
#include "test_ubse_lcne_qos.h"
#include <mockcpp/mockcpp.hpp>
#include "lcne/ubse_lcne_qos.h"
#include "src/framework/misc/ubse_pointer_process.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_http_module.h"
#include "ubse_topology_interface.h"
#include "ubse_xml.h"

namespace ubse::ut::lcne {
using namespace ubse::lcne;
using namespace ubse::context;
using namespace ubse::utils;

void TestUbseLcneQos ::SetUp()
{
    Test::SetUp();
}

void TestUbseLcneQos ::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseLcneQos, CreatQosProfile)
{
    std::string host = "127.0.0.1";
    int port = 8799;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_CREATED);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));

    UbseLcneQosProfile ubseQosProfile;
    ubseQosProfile.proflieName = "proflie1";
    ubseQosProfile.maxBandWidth = 100;
    ubseQosProfile.minBandWidth = 10;
    UbseResult ret = UbseLcneQos::GetInstance().CreatQosProfile(ubseQosProfile);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneQos, CreatQosProfile_fail)
{
    std::string host = "127.0.0.1";
    int port = 8799;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_CREATED);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_ERROR));
    UbseLcneQosProfile ubseQosProfile;
    ubseQosProfile.proflieName = "proflie1";
    ubseQosProfile.maxBandWidth = 100;
    ubseQosProfile.minBandWidth = 10;
    UbseResult ret = UbseLcneQos::GetInstance().CreatQosProfile(ubseQosProfile);
    EXPECT_EQ(UBSE_ERROR, ret);
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_BAD_REQ);
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    ret = UbseLcneQos::GetInstance().CreatQosProfile(ubseQosProfile);
    EXPECT_EQ(UBSE_ERROR, ret);
    const auto func2 = &UbseLcneQos::BuildQoSProfileXml;
    MOCKER_CPP(func2).stubs().will(returnValue(UBSE_ERROR_NULLPTR));
    ret = UbseLcneQos::GetInstance().CreatQosProfile(ubseQosProfile);
    EXPECT_EQ(UBSE_ERROR_NULLPTR, ret);
}

TEST_F(TestUbseLcneQos, DeleteQosProfile)
{
    std::string host = "127.0.0.1";
    int port = 8799;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_NO_CONTENT);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    std::string proflieName = "proflie1";
    UbseResult ret = UbseLcneQos::GetInstance().DeleteQosProfile(proflieName);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneQos, DeleteQosProfile_fail)
{
    std::string host = "127.0.0.1";
    int port = 8799;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_BAD_REQ);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    std::string proflieName = "proflie1";
    UbseResult ret = UbseLcneQos::GetInstance().DeleteQosProfile(proflieName);
    EXPECT_EQ(UBSE_ERROR, ret);
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_ERROR));
    proflieName = "proflie1";
    ret = UbseLcneQos::GetInstance().DeleteQosProfile(proflieName);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneQos, QureyQosProfile)
{
    std::string responseXml = R"(<tqos-profile xmlns="urn:huawei:yang:huawei-ub-qos">
        <name>proflie1</name>
        <schedule>
            <mode>dwrr</mode>
            <weight>12</weight>
        </schedule>
        <shaper>
            <cir>1000</cir>
            <cbs>4194304</cbs>
            <pir>2000</pir>
            <pbs>4194304</pbs>
        </shaper>
    </tqos-profile>
    )";
    std::string host = "127.0.0.1";
    int port = 8799;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = responseXml;
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneQos::ParseQosProfileResponse;
    MOCKER_CPP(func2).stubs().will(returnValue(UBSE_OK));
    std::string proflieName = "proflie1";
    UbseLcneQosProfile ubseQosProfile;
    UbseResult ret = UbseLcneQos::GetInstance().QureyQosProfile(proflieName, ubseQosProfile);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneQos, QureyQosProfile_fail)
{
    std::string host = "127.0.0.1";
    int port = 8799;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "responseXml";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneQos::ParseQosProfileResponse;
    MOCKER_CPP(func2).stubs().will(returnValue(UBSE_ERROR));
    std::string proflieName = "proflie1";
    UbseLcneQosProfile ubseQosProfile;
    UbseResult ret = UbseLcneQos::GetInstance().QureyQosProfile(proflieName, ubseQosProfile);
    EXPECT_EQ(UBSE_ERROR, ret);
    rsp.body = "";
    ret = UbseLcneQos::GetInstance().QureyQosProfile(proflieName, ubseQosProfile);
    EXPECT_EQ(UBSE_ERROR, ret);
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_BAD_REQ);
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    ret = UbseLcneQos::GetInstance().QureyQosProfile(proflieName, ubseQosProfile);
    EXPECT_EQ(UBSE_ERROR, ret);
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_ERROR));
    ret = UbseLcneQos::GetInstance().QureyQosProfile(proflieName, ubseQosProfile);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneQos, ApplyVfeQos)
{
    std::string host = "127.0.0.1";
    int port = 8799;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_CREATED);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    std::string proflieName = "proflie1";
    UbseLcneFeInfo ubseFeInfo;
    UbseResult ret = UbseLcneQos::GetInstance().ApplyVfeQos(ubseFeInfo, proflieName);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneQos, ApplyVfeQos_fail)
{
    std::string host = "127.0.0.1";
    int port = 8799;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    std::string proflieName = "proflie1";
    UbseLcneFeInfo ubseFeInfo;
    UbseResult ret = UbseLcneQos::GetInstance().ApplyVfeQos(ubseFeInfo, proflieName);
    EXPECT_EQ(UBSE_ERROR, ret);
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_ERROR));
    ret = UbseLcneQos::GetInstance().ApplyVfeQos(ubseFeInfo, proflieName);
    EXPECT_EQ(UBSE_ERROR, ret);
    const auto func2 = &UbseLcneQos::BuildQoSXml;
    MOCKER_CPP(func2).stubs().will(returnValue(UBSE_ERROR));
    ret = UbseLcneQos::GetInstance().ApplyVfeQos(ubseFeInfo, proflieName);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneQos, DeleteVfeQos)
{
    std::string host = "127.0.0.1";
    int port = 8799;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_NO_CONTENT);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    std::string proflieName = "proflie1";
    UbseLcneFeInfo ubseFeInfo;
    UbseResult ret = UbseLcneQos::GetInstance().DeleteVfeQos(ubseFeInfo);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneQos, DeleteVfeQos_fail)
{
    std::string host = "127.0.0.1";
    int port = 8799;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "Rsp_body";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    std::string proflieName = "proflie1";
    UbseLcneFeInfo ubseFeInfo;
    UbseResult ret = UbseLcneQos::GetInstance().DeleteVfeQos(ubseFeInfo);
    EXPECT_EQ(UBSE_ERROR, ret);
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_ERROR));
    ret = UbseLcneQos::GetInstance().ApplyVfeQos(ubseFeInfo, proflieName);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneQos, QueryVfeQos)
{
    std::string responseXml = R"(<tqos-entity-profile apply xmlns="urn:huawei:yang:huawei-vbussw-service">
        <slot-id>1</slot-id>
        <ubpu-id>1</ubpu-id>
        <iou-id>1</iou-id>
        <entity-id>1</entity-id>
        <profile-name>profile1</profile-name>
    </tqos-entity-profile>
    )";
    std::string host = "127.0.0.1";
    int port = 8799;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = responseXml;
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneQos::ParseVfeQosResponse;
    MOCKER_CPP(func2).stubs().will(returnValue(UBSE_OK));
    std::string proflieName;
    UbseLcneFeInfo ubseFeInfo;
    UbseResult ret = UbseLcneQos::GetInstance().QueryVfeQos(ubseFeInfo, proflieName);
    EXPECT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseLcneQos, QueryVfeQos_fail)
{
    std::string host = "127.0.0.1";
    int port = 8799;
    UbseHttpRequest req;
    UbseHttpResponse rsp;
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK);
    rsp.body = "responseXml";
    const auto func1 = &UbseHttpModule::HttpSend;
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    const auto func2 = &UbseLcneQos::ParseVfeQosResponse;
    MOCKER_CPP(func2).stubs().will(returnValue(UBSE_ERROR));
    std::string proflieName;
    UbseLcneFeInfo ubseFeInfo;
    UbseResult ret = UbseLcneQos::GetInstance().QueryVfeQos(ubseFeInfo, proflieName);
    EXPECT_EQ(UBSE_ERROR, ret);
    rsp.body = "";
    ret = UbseLcneQos::GetInstance().QueryVfeQos(ubseFeInfo, proflieName);
    EXPECT_EQ(UBSE_ERROR, ret);
    rsp.status = static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_BAD_REQ);
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_OK));
    ret = UbseLcneQos::GetInstance().QueryVfeQos(ubseFeInfo, proflieName);
    EXPECT_EQ(UBSE_ERROR, ret);
    MOCKER_CPP(func1).stubs().with(host, port, outBound(req), outBound(rsp)).will(returnValue(UBSE_ERROR));
    ret = UbseLcneQos::GetInstance().QueryVfeQos(ubseFeInfo, proflieName);
    EXPECT_EQ(UBSE_ERROR, ret);
}

TEST_F(TestUbseLcneQos, BuildQoSProfileXml)
{
    std::string xmlStr;
    UbseLcneQosProfile ubseQosProfile;
    ubseQosProfile.proflieName = "profile1";
    ubseQosProfile.minBandWidth = 80;
    ubseQosProfile.maxBandWidth = 160;
    UbseResult ret = UbseLcneQos::GetInstance().BuildQoSProfileXml(ubseQosProfile, xmlStr);
    EXPECT_EQ(UBSE_OK, ret);
    std::string responseXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<tqos-profile xmlns="urn:huawei:yang:huawei-ub-qos">
  <name>profile1</name>
  <schedule>
    <mode>dwrr</mode>
    <weight>12</weight>
  </schedule>
  <shaper>
    <cir>10</cir>
    <cbs>4194304</cbs>
    <pir>20</pir>
    <pbs>4194304</pbs>
  </shaper>
</tqos-profile>
)";
    EXPECT_EQ(responseXml, xmlStr);
}

TEST_F(TestUbseLcneQos, BuildQoSXml)
{
    std::string xmlStr;
    std::string proflieName = "profile1";
    UbseLcneFeInfo ubseFeInfo;
    ubseFeInfo.slotId = "1";
    ubseFeInfo.ubpuId = "2";
    ubseFeInfo.iouId = "3";
    ubseFeInfo.entityId = "4";    
    UbseResult ret = UbseLcneQos::GetInstance().BuildQoSXml(ubseFeInfo, proflieName, xmlStr);
    EXPECT_EQ(UBSE_OK, ret);
    std::string responseXml = R"(<?xml version="1.0" encoding="UTF-8"?>
<tqos-entity-profile-apply xmlns="urn:huawei:yang:huawei-vbussw-service">
  <slot-id>1</slot-id>
  <ubpu-id>2</ubpu-id>
  <iou-id>3</iou-id>
  <entity-id>4</entity-id>
  <profile-name>profile1</profile-name>
</tqos-entity-profile-apply>
)";
    EXPECT_EQ(responseXml, xmlStr);
}

TEST_F(TestUbseLcneQos, ParseQosProfileResponse)
{
    std::string body = R"(<?xml version="1.0" encoding="UTF-8"?>
<tqos-profile xmlns="urn:huawei:yang:huawei-ub-qos">
  <name>profile1</name>
  <schedule>
    <mode>dwrr</mode>
    <weight>12</weight>
  </schedule>
  <shaper>
    <cir>10</cir>
    <cbs>4194304</cbs>
    <pir>20</pir>
    <pbs>4194304</pbs>
  </shaper>
</tqos-profile>
)";
    UbseLcneQosProfile ubseQosProfile;
    UbseResult ret = UbseLcneQos::GetInstance().ParseQosProfileResponse(body, ubseQosProfile);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(80, ubseQosProfile.minBandWidth);
    EXPECT_EQ(160, ubseQosProfile.maxBandWidth);
}

TEST_F(TestUbseLcneQos, ParseVfeQosResponse)
{
    std::string body = R"(<?xml version="1.0" encoding="UTF-8"?>
<tqos-entity-profile-apply xmlns="urn:huawei:yang:huawei-vbussw-service">
  <slot-id>1</slot-id>
  <ubpu-id>1</ubpu-id>
  <iou-id>1</iou-id>
  <entity-id>1</entity-id>
  <profile-name>profile1</profile-name>
</tqos-entity-profile-apply>
)";
    std::string proflieName;
    UbseResult ret = UbseLcneQos::GetInstance().ParseVfeQosResponse(body, proflieName);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ("profile1", proflieName);
}

} // namespace ubse::ut::lcne