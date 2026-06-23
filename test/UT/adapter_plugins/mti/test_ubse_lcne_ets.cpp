/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "ubse_error.h"
#include "ubse_http_module.h"
#include "ubse_mti_interface_default.h"
#include "lcne/ubse_lcne_ets.h"

namespace ubse::ut::lcne {
using namespace ubse::adapter_plugins::mti;
using namespace ubse::common::def;
using namespace ubse::http;
using namespace ubse::lcne;

namespace {
UbseHttpRequest g_request;
UbseHttpResponse g_response;
UbseResult g_httpRet = UBSE_OK;

const std::string ETS_PROFILE_XML =
    "<ub-qos xmlns=\"urn:huawei:yang:huawei-ub-qos\"><ets-profiles><ets-profile><name>test-ets</name>"
    "<vls><vl><vl-index>0</vl-index><priority-group-id>1</priority-group-id>"
    "</vl><vl><vl-index>10</vl-index>"
    "<priority-group-id>2</priority-group-id></vl></vls>"
    "<priority-groups><priority-group><priority-group-id>1</priority-group-id>"
    "<cir>1000</cir><cbs>4096</cbs>"
    "</priority-group></priority-groups></ets-profile></ets-profiles></ub-qos>";
const std::string ETS_PROFILE_WRAPPED_XML =
    "<ub-qos xmlns=\"urn:huawei:yang:huawei-ub-qos\"><ets-profiles><ets-profile><name>test-ets1</name>"
    "<vls><vl><vl-index>1</vl-index><priority-group-id>1</priority-group-id>"
    "</vl></vls>"
    "<priority-groups><priority-group><priority-group-id>1</priority-group-id>"
    "<cir>1000</cir><cbs>4096</cbs>"
    "</priority-group></priority-groups></ets-profile></ets-profiles></ub-qos>";
const std::string ETS_PROFILES_XML =
    "<ub-qos xmlns=\"urn:huawei:yang:huawei-ub-qos\"><ets-profiles><ets-profile><name>test-ets1</name>"
    "<vls><vl><vl-index>1</vl-index><priority-group-id>1</priority-group-id>"
    "</vl></vls><priority-groups>"
    "<priority-group><priority-group-id>1</priority-group-id>"
    "<cir>1000</cir><cbs>4096</cbs></priority-group></priority-groups></ets-profile>"
    "<ets-profile><name>test-ets2</name><vls><vl><vl-index>2</vl-index>"
    "<priority-group-id>2</priority-group-id></vl></vls>"
    "<priority-groups><priority-group><priority-group-id>2</priority-group-id>"
    "<cir>2000</cir><cbs>8192</cbs>"
    "</priority-group></priority-groups></ets-profile></ets-profiles></ub-qos>";
const std::string ETS_PROFILE_NAME_ONLY_XML =
    "<ub-qos xmlns=\"urn:huawei:yang:huawei-ub-qos\"><ets-profiles><ets-profile>"
    "<name>test-ets-name-only</name></ets-profile></ets-profiles></ub-qos>";

UbseResult MockHttpSend(UbseHttpRequest& request, UbseHttpResponse& response)
{
    g_request = request;
    response = g_response;
    return g_httpRet;
}

UbseMtiEtsProfile MakeEtsProfile()
{
    UbseMtiEtsProfile profile;
    profile.profileName = "test-ets";
    profile.vls.push_back({0, 1, UbseEtsScheduleMode::DWRR, 10});
    profile.vls.push_back({10, 2, UbseEtsScheduleMode::SP, 1});
    profile.priorityGroups.push_back({1, UbseEtsScheduleMode::DWRR, 20, 1000, 4096});
    return profile;
}

void MockHttpResponse(int status, const std::string& body = "", UbseResult ret = UBSE_OK)
{
    g_request = {};
    g_response = {};
    g_response.status = status;
    g_response.body = body;
    g_httpRet = ret;
    MOCKER_CPP(&UbseHttpModule::HttpSend).stubs().will(invoke(MockHttpSend));
}
} // namespace

class TestUbseLcneEts : public testing::Test {
public:
    void SetUp() override
    {
        testing::Test::SetUp();
        g_request = {};
        g_response = {};
        g_httpRet = UBSE_OK;
    }

    void TearDown() override
    {
        testing::Test::TearDown();
        GlobalMockObject::verify();
    }
};

TEST_F(TestUbseLcneEts, BuildEtsProfileXmlSuccess)
{
    std::string xmlStr;
    EXPECT_EQ(UbseLcneEts::GetInstance().BuildEtsProfileXml(MakeEtsProfile(), xmlStr), UBSE_OK);
    EXPECT_NE(xmlStr.find("<ets-profiles>"), std::string::npos);
    EXPECT_NE(xmlStr.find("<name>test-ets</name>"), std::string::npos);
    EXPECT_NE(xmlStr.find("<vl-index>10</vl-index>"), std::string::npos);
    EXPECT_NE(xmlStr.find("<priority-groups>"), std::string::npos);
}

TEST_F(TestUbseLcneEts, BuildInterfaceEtsApplicationXmlSuccess)
{
    std::string xmlStr;
    EXPECT_EQ(UbseLcneEts::GetInstance().BuildInterfaceEtsApplicationXml("default", xmlStr), UBSE_OK);
    EXPECT_NE(xmlStr.find("<ets-application xmlns=\"urn:huawei:yang:huawei-ub-qos\">"), std::string::npos);
    EXPECT_NE(xmlStr.find("<ets-profile-name>default</ets-profile-name>"), std::string::npos);
}

TEST_F(TestUbseLcneEts, ParseEtsProfileResponseSuccess)
{
    UbseMtiEtsProfile profile;
    EXPECT_EQ(UbseLcneEts::GetInstance().ParseEtsProfileResponse(ETS_PROFILE_XML, profile), UBSE_OK);
    EXPECT_EQ(profile.profileName, "test-ets");
    ASSERT_EQ(profile.vls.size(), 2);
    EXPECT_EQ(profile.vls[0].vlIndex, 0);
    EXPECT_EQ(profile.vls[1].vlIndex, 10);
    ASSERT_EQ(profile.priorityGroups.size(), 1);
    EXPECT_EQ(profile.priorityGroups[0].cir, 1000);
    EXPECT_EQ(profile.priorityGroups[0].cbs, 4096);
}

TEST_F(TestUbseLcneEts, ParseEtsProfileResponseWrappedSuccess)
{
    UbseMtiEtsProfile profile;
    EXPECT_EQ(UbseLcneEts::GetInstance().ParseEtsProfileResponse(ETS_PROFILE_WRAPPED_XML, profile), UBSE_OK);
    EXPECT_EQ(profile.profileName, "test-ets1");
    ASSERT_EQ(profile.vls.size(), 1);
    EXPECT_EQ(profile.vls[0].vlIndex, 1);
    ASSERT_EQ(profile.priorityGroups.size(), 1);
    EXPECT_EQ(profile.priorityGroups[0].priorityGroupId, 1);
}

TEST_F(TestUbseLcneEts, ParseEtsProfileResponseNameOnlySuccess)
{
    UbseMtiEtsProfile profile = MakeEtsProfile();
    EXPECT_EQ(UbseLcneEts::GetInstance().ParseEtsProfileResponse(ETS_PROFILE_NAME_ONLY_XML, profile), UBSE_OK);
    EXPECT_EQ(profile.profileName, "test-ets-name-only");
    EXPECT_TRUE(profile.vls.empty());
    EXPECT_TRUE(profile.priorityGroups.empty());
}

TEST_F(TestUbseLcneEts, ParseAllEtsProfilesResponseNameOnlySuccess)
{
    std::vector<UbseMtiEtsProfile> profiles = {MakeEtsProfile()};
    EXPECT_EQ(UbseLcneEts::GetInstance().ParseAllEtsProfilesResponse(ETS_PROFILE_NAME_ONLY_XML, profiles), UBSE_OK);
    ASSERT_EQ(profiles.size(), 1);
    EXPECT_EQ(profiles[0].profileName, "test-ets-name-only");
    EXPECT_TRUE(profiles[0].vls.empty());
    EXPECT_TRUE(profiles[0].priorityGroups.empty());
}

TEST_F(TestUbseLcneEts, ParseEtsProfileResponseInvalidNumberFailed)
{
    std::string responseXml =
        "<ub-qos xmlns=\"urn:huawei:yang:huawei-ub-qos\"><ets-profiles><ets-profile><name>test-ets</name>"
        "<vls><vl><vl-index>invalid</vl-index><priority-group-id>1</priority-group-id>"
        "</vl></vls>"
        "<priority-groups><priority-group><priority-group-id>1</priority-group-id>"
        "<cir>1000</cir><cbs>4096</cbs>"
        "</priority-group></priority-groups></ets-profile></ets-profiles></ub-qos>";
    UbseMtiEtsProfile profile;
    EXPECT_EQ(UbseLcneEts::GetInstance().ParseEtsProfileResponse(responseXml, profile), UBSE_ERROR);
}

TEST_F(TestUbseLcneEts, ParseInterfaceEtsProfileResponseSuccess)
{
    std::string profileName;
    std::string responseXml = R"(<ets-application xmlns="urn:huawei:yang:huawei-ub-qos">
  <ets-profile-name>default</ets-profile-name>
</ets-application>)";
    EXPECT_EQ(UbseLcneEts::GetInstance().ParseInterfaceEtsProfileResponse(responseXml, profileName), UBSE_OK);
    EXPECT_EQ(profileName, "default");
}

TEST_F(TestUbseLcneEts, CreateEtsProfileSuccess)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_NO_CONTENT));
    UbseMtiInterfaceDefault mtiInterface;
    EXPECT_EQ(mtiInterface.UbseCreateEtsProfile(MakeEtsProfile()), UBSE_OK);
    EXPECT_EQ(g_request.method, "PATCH");
    EXPECT_EQ(g_request.path, "/restconf/data/huawei-ub-qos:ub-qos/ets-profiles");
    ASSERT_NE(g_request.headers.find("Accept"), g_request.headers.end());
    EXPECT_EQ(g_request.headers.find("Accept")->second, "application/yang-data+xml");
    ASSERT_NE(g_request.headers.find("Content-Type"), g_request.headers.end());
    EXPECT_EQ(g_request.headers.find("Content-Type")->second, "application/xml");
    EXPECT_NE(g_request.body.find("<ets-profiles>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<ets-profile>"), std::string::npos);
    EXPECT_EQ(g_request.body.find("xmlns"), std::string::npos);
    EXPECT_NE(g_request.body.find("<name>test-ets</name>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<vl-index>10</vl-index>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<priority-groups>"), std::string::npos);
    EXPECT_EQ(g_request.body.find("<schedule-mode>"), std::string::npos);
    EXPECT_EQ(g_request.body.find("<weight>"), std::string::npos);
}

TEST_F(TestUbseLcneEts, AddEtsVlsToProfileSuccess)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_NO_CONTENT));
    UbseMtiInterfaceDefault mtiInterface;
    EXPECT_EQ(mtiInterface.UbseAddEtsVlsToProfile("test-ets", MakeEtsProfile().vls), UBSE_OK);
    EXPECT_EQ(g_request.method, "PATCH");
    EXPECT_EQ(g_request.path, "/restconf/data/huawei-ub-qos:ub-qos/ets-profiles");
    ASSERT_NE(g_request.headers.find("Content-Type"), g_request.headers.end());
    EXPECT_EQ(g_request.headers.find("Content-Type")->second, "application/xml");
    EXPECT_NE(g_request.body.find("<ets-profiles>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<ets-profile>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<name>test-ets</name>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<vls>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<vl-index>10</vl-index>"), std::string::npos);
    EXPECT_EQ(g_request.body.find("<priority-groups>"), std::string::npos);
}

TEST_F(TestUbseLcneEts, AddEtsPriorityGroupsToProfileSuccess)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_NO_CONTENT));
    UbseMtiInterfaceDefault mtiInterface;
    EXPECT_EQ(mtiInterface.UbseAddEtsPriorityGroupsToProfile("test-ets", MakeEtsProfile().priorityGroups), UBSE_OK);
    EXPECT_EQ(g_request.method, "PATCH");
    EXPECT_EQ(g_request.path, "/restconf/data/huawei-ub-qos:ub-qos/ets-profiles");
    ASSERT_NE(g_request.headers.find("Content-Type"), g_request.headers.end());
    EXPECT_EQ(g_request.headers.find("Content-Type")->second, "application/xml");
    EXPECT_NE(g_request.body.find("<ets-profiles>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<ets-profile>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<name>test-ets</name>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<priority-groups>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<cir>1000</cir>"), std::string::npos);
    EXPECT_EQ(g_request.body.find("<vls>"), std::string::npos);
}

TEST_F(TestUbseLcneEts, AddEtsVlsAndPriorityGroupsToProfileSuccess)
{
    auto etsProfile = MakeEtsProfile();
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_NO_CONTENT));
    UbseMtiInterfaceDefault mtiInterface;
    EXPECT_EQ(
        mtiInterface.UbseAddEtsVlsAndPriorityGroupsToProfile("test-ets", etsProfile.vls, etsProfile.priorityGroups),
        UBSE_OK);
    EXPECT_EQ(g_request.method, "PATCH");
    EXPECT_EQ(g_request.path, "/restconf/data/huawei-ub-qos:ub-qos/ets-profiles");
    ASSERT_NE(g_request.headers.find("Content-Type"), g_request.headers.end());
    EXPECT_EQ(g_request.headers.find("Content-Type")->second, "application/xml");
    EXPECT_NE(g_request.body.find("<ets-profiles>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<ets-profile>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<name>test-ets</name>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<vls>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<vl-index>10</vl-index>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<priority-groups>"), std::string::npos);
    EXPECT_NE(g_request.body.find("<cir>1000</cir>"), std::string::npos);
}

TEST_F(TestUbseLcneEts, CreateEtsProfileStatusFailed)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK));
    EXPECT_EQ(UbseLcneEts::GetInstance().CreateEtsProfile(MakeEtsProfile()), UBSE_ERROR);
}

TEST_F(TestUbseLcneEts, DeleteEtsProfileSuccess)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_NO_CONTENT));
    UbseMtiInterfaceDefault mtiInterface;
    EXPECT_EQ(mtiInterface.UbseDeleteEtsProfile("test-ets"), UBSE_OK);
    EXPECT_EQ(g_request.method, "DELETE");
    EXPECT_EQ(g_request.path, "/restconf/data/huawei-ub-qos:ub-qos/ets-profiles/ets-profile=test-ets");
}

TEST_F(TestUbseLcneEts, RemoveEtsVlsFromProfileSuccess)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_NO_CONTENT));
    UbseMtiInterfaceDefault mtiInterface;
    EXPECT_EQ(mtiInterface.UbseRemoveEtsVlsFromProfile("test-ets"), UBSE_OK);
    EXPECT_EQ(g_request.method, "DELETE");
    EXPECT_EQ(g_request.path, "/restconf/data/huawei-ub-qos:ub-qos/ets-profiles/ets-profile=test-ets/vls");
}

TEST_F(TestUbseLcneEts, RemoveEtsPriorityGroupsFromProfileSuccess)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_NO_CONTENT));
    UbseMtiInterfaceDefault mtiInterface;
    EXPECT_EQ(mtiInterface.UbseRemoveEtsPriorityGroupsFromProfile("test-ets"), UBSE_OK);
    EXPECT_EQ(g_request.method, "DELETE");
    EXPECT_EQ(g_request.path, "/restconf/data/huawei-ub-qos:ub-qos/ets-profiles/ets-profile=test-ets/priority-groups");
}

TEST_F(TestUbseLcneEts, QueryEtsProfileSuccess)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK), ETS_PROFILE_WRAPPED_XML);
    UbseMtiEtsProfile profile;
    UbseMtiInterfaceDefault mtiInterface;
    EXPECT_EQ(mtiInterface.UbseQueryEtsProfile("test-ets1", profile), UBSE_OK);
    EXPECT_EQ(g_request.method, "GET");
    EXPECT_EQ(g_request.path, "/restconf/data/huawei-ub-qos:ub-qos/ets-profiles/ets-profile=test-ets1");
    EXPECT_EQ(profile.profileName, "test-ets1");
    ASSERT_EQ(profile.vls.size(), 1);
}

TEST_F(TestUbseLcneEts, QueryAllEtsProfilesSuccess)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK), ETS_PROFILES_XML);
    std::vector<UbseMtiEtsProfile> profiles;
    UbseMtiInterfaceDefault mtiInterface;
    EXPECT_EQ(mtiInterface.UbseQueryAllEtsProfiles(profiles), UBSE_OK);
    EXPECT_EQ(g_request.method, "GET");
    EXPECT_EQ(g_request.path, "/restconf/data/huawei-ub-qos:ub-qos/ets-profiles");
    ASSERT_EQ(profiles.size(), 2);
    EXPECT_EQ(profiles[0].profileName, "test-ets1");
    EXPECT_EQ(profiles[1].profileName, "test-ets2");
    ASSERT_EQ(profiles[1].vls.size(), 1);
}

TEST_F(TestUbseLcneEts, QueryAllEtsProfilesEmptyBodySuccess)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK));
    std::vector<UbseMtiEtsProfile> profiles = {MakeEtsProfile()};
    EXPECT_EQ(UbseLcneEts::GetInstance().QueryAllEtsProfiles(profiles), UBSE_OK);
    EXPECT_TRUE(profiles.empty());
}

TEST_F(TestUbseLcneEts, QueryAllEtsProfilesEmptyContainerSuccess)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK),
                     R"(<ub-qos xmlns="urn:huawei:yang:huawei-ub-qos"/>)");
    std::vector<UbseMtiEtsProfile> profiles = {MakeEtsProfile()};
    EXPECT_EQ(UbseLcneEts::GetInstance().QueryAllEtsProfiles(profiles), UBSE_OK);
    EXPECT_TRUE(profiles.empty());
}

TEST_F(TestUbseLcneEts, QueryEtsProfileEmptyBodyNotExist)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK));
    UbseMtiEtsProfile profile;
    EXPECT_EQ(UbseLcneEts::GetInstance().QueryEtsProfile("test-ets", profile), UBSE_MTI_ERROR_NOT_EXIST);
}

TEST_F(TestUbseLcneEts, QueryEtsProfileEmptyContainerNotExist)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK),
                     R"(<ub-qos xmlns="urn:huawei:yang:huawei-ub-qos"/>)");
    UbseMtiEtsProfile profile = MakeEtsProfile();
    EXPECT_EQ(UbseLcneEts::GetInstance().QueryEtsProfile("test-ets", profile), UBSE_MTI_ERROR_NOT_EXIST);
    EXPECT_TRUE(profile.profileName.empty());
}

TEST_F(TestUbseLcneEts, ApplyEtsProfileToInterfaceSuccess)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_NO_CONTENT));
    UbseMtiInterfaceDefault mtiInterface;
    EXPECT_EQ(mtiInterface.UbseApplyEtsProfileToInterface("200GUB0/1/2", "default"), UBSE_OK);
    EXPECT_EQ(g_request.method, "PUT");
    EXPECT_EQ(g_request.path, "/restconf/data/huawei-ifm:ifm/interfaces/interface=(200GUB0/1/2)/huawei-ub-qos:ub-qos/"
                              "ets-application");
    EXPECT_NE(g_request.body.find("<ets-profile-name>default</ets-profile-name>"), std::string::npos);
}

TEST_F(TestUbseLcneEts, RemoveEtsProfileFromInterfaceSuccess)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_NO_CONTENT));
    UbseMtiInterfaceDefault mtiInterface;
    EXPECT_EQ(mtiInterface.UbseRemoveEtsProfileFromInterface("200GUB0/1/2"), UBSE_OK);
    EXPECT_EQ(g_request.method, "DELETE");
    EXPECT_EQ(g_request.path, "/restconf/data/huawei-ifm:ifm/interfaces/interface=(200GUB0/1/2)/huawei-ub-qos:ub-qos/"
                              "ets-application");
}

TEST_F(TestUbseLcneEts, QueryAllInterfaceEtsProfileSuccess)
{
    std::string responseXml = R"(<ifm xmlns="urn:huawei:yang:huawei-ifm">
  <interfaces>
    <interface>
      <name>400GUB1/1/2</name>
      <ub-qos xmlns="urn:huawei:yang:huawei-ub-qos">
        <ets-application>
          <ets-profile-name>test-ets1</ets-profile-name>
        </ets-application>
      </ub-qos>
    </interface>
    <interface>
      <name>400GUB1/1/3</name>
      <ub-qos xmlns="urn:huawei:yang:huawei-ub-qos">
        <ets-application>
          <ets-profile-name>test-ets1</ets-profile-name>
        </ets-application>
      </ub-qos>
    </interface>
  </interfaces>
</ifm>)";
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK), responseXml);
    std::vector<UbseMtiInterfaceEtsApplication> applications;
    UbseMtiInterfaceDefault mtiInterface;
    EXPECT_EQ(mtiInterface.UbseQueryAllInterfaceEtsProfile(applications), UBSE_OK);
    EXPECT_EQ(g_request.method, "GET");
    EXPECT_EQ(g_request.path,
              "/restconf/data/huawei-ifm:ifm/interfaces/interface/huawei-ub-qos:ub-qos/ets-application");
    ASSERT_EQ(applications.size(), 2);
    EXPECT_EQ(applications[0].interfaceName, "400GUB1/1/2");
    EXPECT_EQ(applications[0].etsProfileName, "test-ets1");
    EXPECT_EQ(applications[1].interfaceName, "400GUB1/1/3");
    EXPECT_EQ(applications[1].etsProfileName, "test-ets1");
}

TEST_F(TestUbseLcneEts, QueryAllInterfaceEtsProfileEmptyBodySuccess)
{
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK));
    std::vector<UbseMtiInterfaceEtsApplication> applications = {{"old-interface", "old-profile"}};
    EXPECT_EQ(UbseLcneEts::GetInstance().QueryAllInterfaceEtsProfile(applications), UBSE_OK);
    EXPECT_TRUE(applications.empty());
}

TEST_F(TestUbseLcneEts, QueryAllInterfaceEtsProfileEmptyContainerSuccess)
{
    std::string responseXml = R"(<ifm xmlns="urn:huawei:yang:huawei-ifm">
  <interfaces>
    <interface>
      <name>400GUB1/1/2</name>
    </interface>
  </interfaces>
</ifm>)";
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK), responseXml);
    std::vector<UbseMtiInterfaceEtsApplication> applications = {{"old-interface", "old-profile"}};
    EXPECT_EQ(UbseLcneEts::GetInstance().QueryAllInterfaceEtsProfile(applications), UBSE_OK);
    EXPECT_TRUE(applications.empty());
}

TEST_F(TestUbseLcneEts, QueryInterfaceEtsProfileSuccess)
{
    std::string responseXml = R"(<ets-application xmlns="urn:huawei:yang:huawei-ub-qos">
  <ets-profile-name>default</ets-profile-name>
</ets-application>)";
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK), responseXml);
    std::string profileName;
    UbseMtiInterfaceDefault mtiInterface;
    EXPECT_EQ(mtiInterface.UbseQueryInterfaceEtsProfile("200GUB0/1/2", profileName), UBSE_OK);
    EXPECT_EQ(g_request.method, "GET");
    EXPECT_EQ(g_request.path, "/restconf/data/huawei-ifm:ifm/interfaces/interface=(200GUB0/1/2)/huawei-ub-qos:ub-qos/"
                              "ets-application");
    EXPECT_EQ(profileName, "default");
}

TEST_F(TestUbseLcneEts, QueryInterfaceEtsProfileNoApplicationSuccess)
{
    std::string responseXml = R"(<ifm xmlns="urn:huawei:yang:huawei-ifm">
  <interfaces>
    <interface>
      <name>400GUB1/1/2</name>
    </interface>
  </interfaces>
</ifm>)";
    MockHttpResponse(static_cast<int>(UbseHttpStatusCode::UBSE_HTTP_STATUS_CODE_OK), responseXml);
    std::string profileName = "old-profile";
    EXPECT_EQ(UbseLcneEts::GetInstance().QueryInterfaceEtsProfile("400GUB1/1/2", profileName), UBSE_OK);
    EXPECT_EQ(g_request.method, "GET");
    EXPECT_EQ(g_request.path, "/restconf/data/huawei-ifm:ifm/interfaces/interface=(400GUB1/1/2)/huawei-ub-qos:ub-qos/"
                              "ets-application");
    EXPECT_TRUE(profileName.empty());
}

} // namespace ubse::ut::lcne
