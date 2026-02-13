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

#include "test_ubse_urma_controller.h"
#include "ubse_lcne_qos.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_def.h"

namespace ubse::urmaController::ut {
using namespace ubse::urmaController;
using namespace ubse::lcne;
using namespace ubse::urma;

TEST_F(TestUbseUrmaController, UbseUrmaBandWidthSet)
{
    std::string name = "urma1";
    uint32_t minBandWidth = 80;
    uint32_t maxBandWidth = 800;
    UbseUrmaInfo urmaInfo;
    EidGroup eidGroup;
    eidGroup.primaryEid = "0000:0000:0000:0060:0010:0000:DF00:05C5";
    urmaInfo.eidGroups.emplace_back(eidGroup);
    urmaInfo.urmaDevType = UrmaDevType::UNIQUE;
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(name, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    std::shared_ptr<UbseFeInfo> feInfo = std::make_shared<UbseFeInfo>();
    MOCKER_CPP(GetUrmaVfeFromEidGroup).stubs().will(returnValue(feInfo));
    MOCKER(&UbseLcneQos::CreatQosProfile).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseLcneQos::ApplyVfeQos).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseUrmaControllerManager::SetUrmaQos).stubs().will(returnValue(UBSE_OK));
    uint32_t ret = UrmaController::GetInstance().UbseUrmaBandWidthSet(name, minBandWidth, maxBandWidth);
    EXPECT_EQ(UBSE_OK, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseUrmaController, UbseUrmaBandWidthSet_fail)
{
    std::string name = "urma1";
    uint32_t minBandWidth = 80;
    uint32_t maxBandWidth = 800;
    UbseUrmaInfo urmaInfo;
    EidGroup eidGroup;
    eidGroup.primaryEid = "0000:0000:0000:0060:0010:0000:DF00:05C5";
    urmaInfo.eidGroups.emplace_back(eidGroup);
    urmaInfo.urmaDevType = UrmaDevType::UNIQUE;
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(name, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    MOCKER(&UbseUrmaControllerManager::SetUrmaQos).stubs().will(returnValue(UBSE_OK));
    std::shared_ptr<UbseFeInfo> feInfo = std::make_shared<UbseFeInfo>();
    MOCKER_CPP(GetUrmaVfeFromEidGroup).stubs().will(returnValue(feInfo));
    MOCKER(&UbseLcneQos::CreatQosProfile).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseLcneQos::ApplyVfeQos).stubs().will(returnValue(UBSE_ERROR_CONF_INVALID));
    uint32_t ret = UrmaController::GetInstance().UbseUrmaBandWidthSet(name, minBandWidth, maxBandWidth);
    EXPECT_EQ(UBSE_ERROR, ret);
    MOCKER_CPP(GetUrmaVfeFromEidGroup).stubs().will(returnValue(nullptr));
    ret = UrmaController::GetInstance().UbseUrmaBandWidthSet(name, minBandWidth, maxBandWidth);
    EXPECT_EQ(UBSE_ERROR, ret);
    MOCKER(&UbseLcneQos::CreatQosProfile).stubs().will(returnValue(UBSE_ERROR_CONF_INVALID));
    ret = UrmaController::GetInstance().UbseUrmaBandWidthSet(name, minBandWidth, maxBandWidth);
    EXPECT_EQ(UBSE_ERROR, ret);
    GlobalMockObject::verify();
    urmaInfo.urmaDevType = UrmaDevType::SHARED;
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(name, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    ret = UrmaController::GetInstance().UbseUrmaBandWidthSet(name, minBandWidth, maxBandWidth);
    EXPECT_EQ(UBSE_ERROR_NOT_SUPPORT, ret);
    GlobalMockObject::verify();
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo).stubs().will(returnValue(UBSE_ERROR));
    ret = UrmaController::GetInstance().UbseUrmaBandWidthSet(name, minBandWidth, maxBandWidth);
    EXPECT_EQ(UBSE_ERROR_NOT_EXIST, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseUrmaController, UbseUrmaBandWidthGet)
{
    std::string name = "urma1";
    uint32_t minBandWidth;
    uint32_t maxBandWidth;
    UrmaQosProfile urmaQosProfile;
    urmaQosProfile.minBandWidth = 80;
    urmaQosProfile.maxBandWidth = 160;
    urmaQosProfile.profileName = "urma1_profile";
    MOCKER(&UbseUrmaControllerManager::GetUrmaQos)
        .stubs()
        .with(name, outBound(urmaQosProfile))
        .will(returnValue(UBSE_OK));
    uint32_t ret = UrmaController::GetInstance().UbseUrmaBandWidthGet(name, minBandWidth, maxBandWidth);
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(80, minBandWidth);
    EXPECT_EQ(160, maxBandWidth);
    GlobalMockObject::verify();
}

TEST_F(TestUbseUrmaController, UbseUrmaBandWidthGet_fail)
{
    std::string name = "urma1";
    uint32_t minBandWidth;
    uint32_t maxBandWidth;
    MOCKER(&UbseUrmaControllerManager::GetUrmaQos).stubs().will(returnValue(UBSE_ERROR));
    uint32_t ret = UrmaController::GetInstance().UbseUrmaBandWidthGet(name, minBandWidth, maxBandWidth);
    EXPECT_EQ(UBSE_ERROR_NOT_EXIST, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseUrmaController, UbseUrmaBandWidthReset)
{
    std::string name = "urma1";
    UbseUrmaInfo urmaInfo;
    EidGroup eidGroup;
    eidGroup.primaryEid = "0000:0000:0000:0060:0010:0000:DF00:05C5";
    urmaInfo.eidGroups.emplace_back(eidGroup);
    urmaInfo.urmaDevType = UrmaDevType::UNIQUE;
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(name, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    std::shared_ptr<UbseFeInfo> feInfo = std::make_shared<UbseFeInfo>();
    MOCKER_CPP(GetUrmaVfeFromEidGroup).stubs().will(returnValue(feInfo));
    MOCKER(&UbseLcneQos::DeleteQosProfile).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseLcneQos::DeleteVfeQos).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseUrmaControllerManager::SetUrmaQos).stubs().will(returnValue(UBSE_OK));
    uint32_t ret = UrmaController::GetInstance().UbseUrmaBandWidthReset(name);
    EXPECT_EQ(UBSE_OK, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseUrmaController, UbseUrmaBandWidthReset_fail)
{
    std::string name = "urma1";
    UbseUrmaInfo urmaInfo;
    EidGroup eidGroup;
    eidGroup.primaryEid = "0000:0000:0000:0060:0010:0000:DF00:05C5";
    urmaInfo.eidGroups.emplace_back(eidGroup);
    urmaInfo.urmaDevType = UrmaDevType::UNIQUE;
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(name, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    std::shared_ptr<UbseFeInfo> feInfo = std::make_shared<UbseFeInfo>();
    MOCKER_CPP(GetUrmaVfeFromEidGroup).stubs().will(returnValue(feInfo));
    MOCKER(&UbseUrmaControllerManager::SetUrmaQos).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseLcneQos::DeleteQosProfile).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseLcneQos::DeleteVfeQos).stubs().will(returnValue(UBSE_ERROR));
    uint32_t ret = UrmaController::GetInstance().UbseUrmaBandWidthReset(name);
    EXPECT_EQ(UBSE_ERROR, ret);
    MOCKER_CPP(GetUrmaVfeFromEidGroup).stubs().will(returnValue(nullptr));
    ret = UrmaController::GetInstance().UbseUrmaBandWidthReset(name);
    EXPECT_EQ(UBSE_ERROR, ret);
    MOCKER(&UbseLcneQos::DeleteQosProfile).stubs().will(returnValue(UBSE_ERROR));
    ret = UrmaController::GetInstance().UbseUrmaBandWidthReset(name);
    EXPECT_EQ(UBSE_ERROR, ret);
    GlobalMockObject::verify();
    urmaInfo.urmaDevType = UrmaDevType::SHARED;
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(name, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    ret = UrmaController::GetInstance().UbseUrmaBandWidthReset(name);
    EXPECT_EQ(UBSE_ERROR_NOT_SUPPORT, ret);
    GlobalMockObject::verify();
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo).stubs().will(returnValue(UBSE_ERROR));
    ret = UrmaController::GetInstance().UbseUrmaBandWidthReset(name);
    EXPECT_EQ(UBSE_ERROR_NOT_EXIST, ret);
    GlobalMockObject::verify();
}

TEST_F(TestUbseUrmaController, UbseUrmaBandWidthUpdate)
{
    std::string name = "urma1";
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NO_THROW(UrmaController::GetInstance().UbseUrmaBandWidthUpdate(name));
    UbseUrmaInfo urmaInfo;
    EidGroup eidGroup;
    eidGroup.primaryEid = "0000:0000:0000:0060:0010:0000:DF00:05C5";
    urmaInfo.eidGroups.emplace_back(eidGroup);
    urmaInfo.urmaDevType = UrmaDevType::UNIQUE;
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfo)
        .stubs()
        .with(name, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    MOCKER(&UbseLcneQos::QureyQosProfile).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NO_THROW(UrmaController::GetInstance().UbseUrmaBandWidthUpdate(name));
    std::string proflieName = "Profile_urma1";
    UbseFeInfo ubseFeInfo;
    MOCKER_CPP(&UbseLcneQos::QueryVfeQos)
        .stubs()
        .with(outBound(ubseFeInfo), outBound(proflieName))
        .will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(UrmaController::GetInstance().UbseUrmaBandWidthUpdate(name));
    MOCKER_CPP(&UbseLcneQos::QueryVfeQos).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_NO_THROW(UrmaController::GetInstance().UbseUrmaBandWidthUpdate(name));
    GlobalMockObject::verify();
}

} // namespace ubse::urmaController::ut