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

#include "test_ubse_urma_controller_manager.h"
#include "ubse_election.h"
#include "ubse_node_com_urma_collector.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_def.h"
#include "ubse_urma_uvs_module.h"

namespace ubse::urmaControllerManager::ut {
using namespace ubse::urmaController;
using namespace ubse::urma;
using namespace ubse::election;
using namespace ubse::adapter_plugins::mti;
using namespace ubse::nodeController;

std::vector<UbseMtiFeInfo> g_feInfosOneBoundary{

};

std::vector<std::vector<UbseMtiFeInfo>> g_feInfos{
    {{.slotId = "0", .ubpuId = "0", .iouId = "0", .entityId = "0", .eidGroups = std::vector<UbseMtiEidGroup>(2)}},
    {{.slotId = "1", .ubpuId = "1", .iouId = "1", .entityId = "1", .eidGroups = std::vector<UbseMtiEidGroup>(2)}},
    {{.slotId = "0", .ubpuId = "0", .iouId = "0", .entityId = "2", .eidGroups = std::vector<UbseMtiEidGroup>(1)}},
    {{.slotId = "1", .ubpuId = "1", .iouId = "1", .entityId = "3", .eidGroups = std::vector<UbseMtiEidGroup>(1)}}};

std::vector<std::vector<UbseMtiFeInfo>> g_feInfosWithMultiPerIou{
    {{.slotId = "0", .ubpuId = "0", .iouId = "0", .entityId = "0", .eidGroups = std::vector<UbseMtiEidGroup>(2)},
     {.slotId = "0", .ubpuId = "0", .iouId = "0", .entityId = "1", .eidGroups = std::vector<UbseMtiEidGroup>(1)}},
    {{.slotId = "1", .ubpuId = "1", .iouId = "1", .entityId = "2", .eidGroups = std::vector<UbseMtiEidGroup>(2)},
     {.slotId = "1", .ubpuId = "1", .iouId = "1", .entityId = "3", .eidGroups = std::vector<UbseMtiEidGroup>(1)}}};
TEST_F(TestUbseUrmaControllerManager, ConstructNewUrmaInfoWhenFeInfosInvalid)
{
    UbseMtiFeInfo fe0{.slotId = "0", .ubpuId = "0", .iouId = "0", .entityId = "0"};
    std::vector<std::vector<UbseMtiFeInfo>> feInfosInvalid{};
    auto ret = UbseUrmaControllerManager::GetInstance().ConstructNewUrmaInfo("0", feInfosInvalid);
    EXPECT_EQ(UBSE_ERROR_INVAL, ret);
    feInfosInvalid.push_back({fe0});
    ret = UbseUrmaControllerManager::GetInstance().ConstructNewUrmaInfo("0", feInfosInvalid);
    UbseMtiFeInfo fe1{.entityId = "1zxfcadsfasf"};
    feInfosInvalid.push_back({fe1});
    ret = UbseUrmaControllerManager::GetInstance().ConstructNewUrmaInfo("0", feInfosInvalid);
    EXPECT_EQ(UBSE_ERROR_INVAL, ret);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, ConstructNewUrmaInfoWhenFeInfos)
{
    std::vector<std::vector<UbseMtiFeInfo>> feInfos = g_feInfos;
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    uint64_t preUrmaId = UbseUrmaControllerManager::GetInstance().globalUrmaId;
    UbseUrmaControllerManager::GetInstance().feIdMap = {};
    auto ret = UbseUrmaControllerManager::GetInstance().ConstructNewUrmaInfo("0", feInfos);
    uint64_t postUrmaId = UbseUrmaControllerManager::GetInstance().globalUrmaId;
    EXPECT_EQ(UBSE_OK, ret);
    EXPECT_EQ(preUrmaId + 1, postUrmaId);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, ConstructNewUrmaInfo_RvalueOverload)
{
    std::vector<std::vector<UbseMtiFeInfo>> feInfos = g_feInfos;
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().feIdMap = {};
    auto ret = UbseUrmaControllerManager::GetInstance().ConstructNewUrmaInfo("0", std::move(feInfos));
    EXPECT_EQ(UBSE_OK, ret);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, GetLocalUrmaDevInfo)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    UbseUrmaInfo urmaInfo;
    const std::string urmaName = "GetLocalUrmaDevInfo";
    auto ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    EXPECT_EQ(ret, UBSE_ERROR);
    GlobalMockObject::verify();

    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);

    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().nodeInfos["1"] = {};
    ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);

    GlobalMockObject::verify();
    UbseRoleInfo role1;
    role1.nodeId = "1";
    UbseUrmaControllerManager::GetInstance().nodeInfos["1"].urmaList[urmaName] = {};
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role1)).will(returnValue(UBSE_OK));
    ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo(urmaName, urmaInfo);
    EXPECT_EQ(ret, UBSE_OK);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, AllocByUrmaName)
{
    // 获取当前节点信息失败
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    std::string urmaInfoName = "urmaId_1";
    std::vector<std::string> feNames;
    std::string eid;
    auto ret = UbseUrmaControllerManager::GetInstance().AllocByUrmaName(urmaInfoName, feNames, eid);
    EXPECT_EQ(ret, UBSE_ERROR);
    GlobalMockObject::verify();

    // nodeInfos不存在当前节点信息
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    ret = UbseUrmaControllerManager::GetInstance().AllocByUrmaName(urmaInfoName, feNames, eid);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);

    // 存在当前节点信息，不存在对应的urma name
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList = {};
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    ret = UbseUrmaControllerManager::GetInstance().AllocByUrmaName(urmaInfoName, feNames, eid);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);

    // 信息存在
    std::vector<std::vector<UbseMtiFeInfo>> lcneFeInfos = g_feInfos;
    auto urmaFe0 = std::make_shared<UbseFeInfo>(
        UbseFeInfo{.slotId = "0", .ubpuId = "0", .iouId = "0", .entityId = "0", .fetype = FeType::PHYSICAL_TYPE});
    auto urmaFe1 = std::make_shared<UbseFeInfo>(
        UbseFeInfo{.slotId = "1", .ubpuId = "1", .iouId = "1", .entityId = "1", .fetype = FeType::PHYSICAL_TYPE});
    UbseUrmaInfo urmaInfo{.urmaDevEid = "123", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::ACTIVED};
    EidGroup group0{.primaryEid = "123", .feInfo = urmaFe0};
    urmaInfo.eidGroups.push_back(group0);
    EidGroup group1{.primaryEid = "234", .feInfo = urmaFe1};
    urmaInfo.eidGroups.push_back(group1);
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList[urmaInfoName] = urmaInfo;
    ret = UbseUrmaControllerManager::GetInstance().AllocByUrmaName(urmaInfoName, feNames, eid);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(feNames.size(), 3);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, SetActiveState)
{
    // 不存在节点信息
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    std::string eid = "123";
    std::string nodeId = "0";
    UbseUrmaInfo urmaInfo{.urmaDevEid = "123", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    UbseUrmaInfo getInfo;
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urmaId_1"] = urmaInfo;
    UbseUrmaControllerManager::GetInstance().SetActiveState(eid, nodeId);
    for (auto info : UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList) {
        if (info.second.urmaDevEid == eid) {
            getInfo = info.second;
            break;
        }
    }
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    EXPECT_EQ(getInfo.state, UrmaDevState::ACTIVED);
}

TEST_F(TestUbseUrmaControllerManager, SetAllUrmaInfoToActiveForNode)
{
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    std::string eid = "123";
    std::string nodeId = "0";
    UbseUrmaInfo urmaInfo{.urmaDevEid = "123", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    UbseUrmaInfo getInfo;
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urmaId_1"] = urmaInfo;
    UbseUrmaControllerManager::GetInstance().SetAllUrmaInfoToActiveForNode(nodeId);
    for (auto info : UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList) {
        if (info.second.urmaDevEid == eid) {
            getInfo = info.second;
            break;
        }
    }
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    EXPECT_EQ(getInfo.state, UrmaDevState::ACTIVED);
}

TEST_F(TestUbseUrmaControllerManager, SetInactiveState)
{
    // 不存在节点信息
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    std::string eid = "123";
    std::string nodeId = "0";
    UbseUrmaInfo urmaInfo{.urmaDevEid = "123", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    UbseUrmaInfo getInfo;
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urmaId_1"] = urmaInfo;
    UbseUrmaControllerManager::GetInstance().SetInactiveState(eid, nodeId);
    for (auto info : UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList) {
        if (info.second.urmaDevEid == eid) {
            getInfo = info.second;
            break;
        }
    }
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    EXPECT_EQ(getInfo.state, UrmaDevState::INACTIVED);
}

TEST_F(TestUbseUrmaControllerManager, GetUrmaNodeInfo)
{
    // 不存在节点信息
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    std::string eid = "123";
    std::string nodeId = "0";
    UbseUrmaInfo urmaInfo{.urmaDevEid = eid, .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    UbseUrmaInfo getInfo;
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urmaId_1"] = urmaInfo;
    auto nodeInfo = UbseUrmaControllerManager::GetInstance().GetUrmaNodeInfo(nodeId);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    EXPECT_EQ(nodeInfo.urmaList["urmaId_1"].urmaDevEid, eid);
}

TEST_F(TestUbseUrmaControllerManager, GetUrmaUpdateTimeStamp)
{
    // 不存在节点信息
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    std::string eid = "123";
    std::string nodeId = "0";
    auto zeroTimeStamp = UbseUrmaControllerManager::GetInstance().GetUrmaUpdateTimeStamp("0");
    EXPECT_EQ(zeroTimeStamp, 0);
    UbseUrmaControllerManager::GetInstance().feIdMap = {};
    std::vector<std::vector<UbseMtiFeInfo>> feInfos = g_feInfos;
    UbseUrmaControllerManager::GetInstance().ConstructNewUrmaInfo("0", feInfos);
    auto timeStamp = UbseUrmaControllerManager::GetInstance().GetUrmaUpdateTimeStamp("0");
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().feIdMap = {};
    EXPECT_NE(timeStamp, 0);
}

TEST_F(TestUbseUrmaControllerManager, GetHostUrmaDev)
{
    UbseUrmaUvsNodeInfo nodeInfo1{.nodeId = "0"};
    UbseUrmaUvsNodeInfo nodeInfo2{.nodeId = "1"};
    UbseUrmaUvsAggrDev aggrDev1{.urmaDevEid = "12345"};
    UbseUrmaUvsAggrDev aggrDev2{.urmaDevEid = "1234"};
    nodeInfo1.devList.push_back(aggrDev1);
    nodeInfo2.devList.push_back(aggrDev2);
    std::vector<UbseUrmaUvsNodeInfo> hostUrmaInfos{nodeInfo1, nodeInfo2};
    UbseUrmaUvsNodeInfo uvsInfo{.nodeId = "1"};
    GetHostUrmaDev(hostUrmaInfos, uvsInfo);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    EXPECT_EQ(uvsInfo.devList[0].urmaDevEid, "1234");
}

TEST_F(TestUbseUrmaControllerManager, GetAllUvsInfo)
{
    // 获取通信urma失败
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetAllComUrma).stubs().will(returnValue(UBSE_ERROR));
    std::vector<UbseUrmaUvsNodeInfo> uvsInfos;
    auto ret = UbseUrmaControllerManager::GetInstance().GetAllUvsInfo(uvsInfos);
    EXPECT_EQ(ret, UBSE_ERROR);
    GlobalMockObject::verify();

    UbseUrmaInfo urmaInfo1{.urmaDevEid = "123", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    urmaInfo1.eidGroups.resize(2);
    urmaInfo1.eidGroups[0].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo1.eidGroups[1].feInfo = std::make_shared<UbseFeInfo>();
    UbseUrmaInfo urmaInfo2{.urmaDevEid = "1234", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    urmaInfo2.eidGroups.resize(2);
    urmaInfo2.eidGroups[0].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo2.eidGroups[1].feInfo = std::make_shared<UbseFeInfo>();
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urmaId_1"] = urmaInfo1;
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urmaId_2"] = urmaInfo2;
    UbseUrmaControllerManager::GetInstance().nodeInfos["1"].urmaList["urmaId_3"] = urmaInfo1;
    UbseUrmaControllerManager::GetInstance().nodeInfos["1"].urmaList["urmaId_4"] = urmaInfo2;
    UbseUrmaUvsNodeInfo nodeInfo1{.nodeId = "0"};
    UbseUrmaUvsNodeInfo nodeInfo2{.nodeId = "1"};
    UbseUrmaUvsAggrDev aggrDev1{.urmaDevEid = "1234"};
    UbseUrmaUvsAggrDev aggrDev2{.urmaDevEid = "1234"};
    nodeInfo1.devList.push_back(aggrDev1);
    nodeInfo2.devList.push_back(aggrDev2);
    std::vector<UbseUrmaUvsNodeInfo> hostUrmaInfos{nodeInfo1, nodeInfo2};
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetAllComUrma)
        .stubs()
        .with(outBound(hostUrmaInfos))
        .will(returnValue(UBSE_OK));
    ret = UbseUrmaControllerManager::GetInstance().GetAllUvsInfo(uvsInfos);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(uvsInfos.size(), 2);
}

TEST_F(TestUbseUrmaControllerManager, SetUrmaSubPath)
{
    UbseUrmaInfo urmaInfo1{.urmaDevEid = "123", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    urmaInfo1.eidGroups.resize(2);
    urmaInfo1.eidGroups[0].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo1.eidGroups[1].feInfo = std::make_shared<UbseFeInfo>();
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urmaId_1"] = urmaInfo1;
    UbseUrmaControllerManager::GetInstance().SetUrmaSubPath("123", "SetUrmaSubPath");
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urmaId_1"].subPath, "SetUrmaSubPath");
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, SetFeName)
{
    UbseUrmaInfo urmaInfo1{.urmaDevEid = "123", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    urmaInfo1.eidGroups.resize(2);
    urmaInfo1.eidGroups[0].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo1.eidGroups[1].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo1.eidGroups[0].primaryEid = "1::";
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urmaId_1"] = urmaInfo1;
    UbseUrmaControllerManager::GetInstance().SetFeName("1::", "SetFeName");
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urmaId_1"].eidGroups[0].feInfo->name,
              "SetFeName");
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, SetAllUrmaInfoToInactiveForNode)
{
    UbseUrmaInfo urmaInfo1{.urmaDevEid = "123", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    urmaInfo1.eidGroups.resize(2);
    urmaInfo1.eidGroups[0].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo1.eidGroups[1].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo1.eidGroups[0].primaryEid = "1::";
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urmaId_1"] = urmaInfo1;
    UbseUrmaControllerManager::GetInstance().SetAllUrmaInfoToInactiveForNode("0");
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urmaId_1"].state,
              UrmaDevState::INACTIVED);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, InsertNewNodeInfo)
{
    UbseUrmaInfo urmaInfo1{.urmaDevEid = "0123", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    urmaInfo1.eidGroups.resize(2);
    urmaInfo1.eidGroups[0].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo1.eidGroups[1].feInfo = std::make_shared<UbseFeInfo>();
    UbseUrmaInfo urmaInfo2{.urmaDevEid = "01234", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    urmaInfo2.eidGroups.resize(2);
    urmaInfo2.eidGroups[0].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo2.eidGroups[1].feInfo = std::make_shared<UbseFeInfo>();
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaNodeInfo nodeInfo1;
    nodeInfo1.urmaList["urmaId_1"] = urmaInfo1;
    nodeInfo1.nodeId = "0";
    nodeInfo1.updateTimeStamp = 2;
    UbseUrmaNodeInfo nodeInfo2;
    nodeInfo2.updateTimeStamp = 3;
    nodeInfo2.urmaList["urmaId_2"] = urmaInfo2;
    UbseUrmaControllerManager::GetInstance().InsertNewNodeInfo("0", nodeInfo1);
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList.count("urmaId_1"), 1);
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList.count("urmaId_2"), 0);
    UbseUrmaControllerManager::GetInstance().InsertNewNodeInfo("0", nodeInfo2);
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList.count("urmaId_1"), 0);
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList.count("urmaId_2"), 1);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, GetUrmaQos)
{
    // 查询的urma不存在
    UrmaQosProfile qosProfile;
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    auto ret = UbseUrmaControllerManager::GetInstance().GetUrmaQos("no_exist", qosProfile);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);
    GlobalMockObject::verify();

    // 查询的urma存在
    UbseUrmaInfo urmaInfo{.urmaDevEid = "0123", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    urmaInfo.urmaQosProfile.profileName = "qos";
    MOCKER_CPP(&UbseUrmaControllerManager::GetLocalUrmaDevInfoInner)
        .stubs()
        .with(_, outBound(urmaInfo))
        .will(returnValue(UBSE_OK));
    ret = UbseUrmaControllerManager::GetInstance().GetUrmaQos("no_exist", qosProfile);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(qosProfile.profileName, "qos");
}

TEST_F(TestUbseUrmaControllerManager, SetUrmaQos)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    UrmaQosProfile urmaQosProfile;
    urmaQosProfile.profileName = "SetUrmaQos";
    // 查询节点失败
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaControllerManager::GetInstance().SetUrmaQos("SetUrmaQos", urmaQosProfile);
    EXPECT_EQ(ret, UBSE_ERROR);
    GlobalMockObject::verify();

    // 查询的nodeInfo不存在
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    ret = UbseUrmaControllerManager::GetInstance().SetUrmaQos("SetUrmaQos", urmaQosProfile);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);

    // 查询的urmaInfo不存在
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"] = {};
    ret = UbseUrmaControllerManager::GetInstance().SetUrmaQos("SetUrmaQos", urmaQosProfile);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);

    // 查询的urmaInfo存在
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["SetUrmaQos"] = {};
    ret = UbseUrmaControllerManager::GetInstance().SetUrmaQos("SetUrmaQos", urmaQosProfile);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["SetUrmaQos"].urmaQosProfile.profileName,
              "SetUrmaQos");
}

TEST_F(TestUbseUrmaControllerManager, GetAllUrmaInfo_GetCurrentNodeInfoFails)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    std::vector<std::string> names;
    std::vector<uint32_t> status;
    std::vector<uint64_t> hwResIds;
    auto ret = UbseUrmaControllerManager::GetInstance().GetAllUrmaInfo(names, status, hwResIds);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, GetAllUrmaInfo_NoNodeInfo)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(QueryAllPortsDown).stubs().will(returnValue(UBSE_OK));
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    std::vector<std::string> names;
    std::vector<uint32_t> status;
    std::vector<uint64_t> hwResIds;
    auto ret = UbseUrmaControllerManager::GetInstance().GetAllUrmaInfo(names, status, hwResIds);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, GetAllUrmaInfo_EmptyUrmaList)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(QueryAllPortsDown).stubs().will(returnValue(UBSE_OK));
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"] = {};
    std::vector<std::string> names;
    std::vector<uint32_t> status;
    std::vector<uint64_t> hwResIds;
    auto ret = UbseUrmaControllerManager::GetInstance().GetAllUrmaInfo(names, status, hwResIds);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_TRUE(names.empty());
    EXPECT_TRUE(status.empty());
    EXPECT_TRUE(hwResIds.empty());
}

TEST_F(TestUbseUrmaControllerManager, GetAllUrmaInfo_AllPortsDown)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(QueryAllPortsDown).stubs().will(returnValue(UBSE_ERROR));
    auto feInfo = std::make_shared<UbseFeInfo>();
    UbseUrmaInfo urmaInfo{.urmaDevEid = "eid_1", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    urmaInfo.hwResId = 42;
    urmaInfo.eidGroups.push_back({.primaryEid = "peid_1", .feInfo = feInfo});
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urma_1"] = urmaInfo;
    std::vector<std::string> names;
    std::vector<uint32_t> status;
    std::vector<uint64_t> hwResIds;
    auto ret = UbseUrmaControllerManager::GetInstance().GetAllUrmaInfo(names, status, hwResIds);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(names.size(), 1);
    EXPECT_EQ(names[0], "urma_1");
    ASSERT_EQ(status.size(), 1);
    EXPECT_EQ(status[0], static_cast<uint32_t>(UrmaDevState::INACTIVED));
    ASSERT_EQ(hwResIds.size(), 1);
    EXPECT_EQ(hwResIds[0], 42);
}

TEST_F(TestUbseUrmaControllerManager, GetAllUrmaInfo_DeviceHealthy)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(QueryAllPortsDown).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(IsUdmaDevHealthy).stubs().will(returnValue(true));
    auto feInfo = std::make_shared<UbseFeInfo>();
    UbseUrmaInfo urmaInfo{.urmaDevEid = "eid_1", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    urmaInfo.hwResId = 42;
    urmaInfo.eidGroups.push_back({.primaryEid = "peid_1", .feInfo = feInfo});
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urma_1"] = urmaInfo;
    std::vector<std::string> names;
    std::vector<uint32_t> status;
    std::vector<uint64_t> hwResIds;
    auto ret = UbseUrmaControllerManager::GetInstance().GetAllUrmaInfo(names, status, hwResIds);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(names.size(), 1);
    EXPECT_EQ(names[0], "urma_1");
    ASSERT_EQ(status.size(), 1);
    EXPECT_EQ(status[0], static_cast<uint32_t>(UrmaDevState::ACTIVED));
    ASSERT_EQ(hwResIds.size(), 1);
    EXPECT_EQ(hwResIds[0], 42);
}

TEST_F(TestUbseUrmaControllerManager, GetAllUrmaInfo_DeviceUnhealthy)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(QueryAllPortsDown).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(IsUdmaDevHealthy).stubs().will(returnValue(false));
    auto feInfo = std::make_shared<UbseFeInfo>();
    UbseUrmaInfo urmaInfo{.urmaDevEid = "eid_1", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    urmaInfo.hwResId = 42;
    urmaInfo.eidGroups.push_back({.primaryEid = "peid_1", .feInfo = feInfo});
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urma_1"] = urmaInfo;
    std::vector<std::string> names;
    std::vector<uint32_t> status;
    std::vector<uint64_t> hwResIds;
    auto ret = UbseUrmaControllerManager::GetInstance().GetAllUrmaInfo(names, status, hwResIds);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(status.size(), 1);
    EXPECT_EQ(status[0], static_cast<uint32_t>(UrmaDevState::INACTIVED));
}

TEST_F(TestUbseUrmaControllerManager, GetAllUrmaInfo_MultipleDevicesMixedHealth)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(QueryAllPortsDown).stubs().will(returnValue(UBSE_OK));
    auto feInfo = std::make_shared<UbseFeInfo>();
    UbseUrmaInfo healthyInfo{.urmaDevEid = "eid_h", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    healthyInfo.hwResId = 1;
    healthyInfo.eidGroups.push_back({.primaryEid = "peid_h", .feInfo = feInfo});
    UbseUrmaInfo unhealthyInfo{
        .urmaDevEid = "eid_u", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::UNKNOWN};
    unhealthyInfo.hwResId = 2;
    unhealthyInfo.eidGroups.push_back({.primaryEid = "peid_u", .feInfo = feInfo});
    MOCKER_CPP(IsUdmaDevHealthy).stubs().with(std::string("peid_h")).will(returnValue(true));
    MOCKER_CPP(IsUdmaDevHealthy).stubs().with(std::string("peid_u")).will(returnValue(false));
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    auto& urmaList = UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList;
    urmaList["urma_h"] = healthyInfo;
    urmaList["urma_u"] = unhealthyInfo;
    std::vector<std::string> names;
    std::vector<uint32_t> status;
    std::vector<uint64_t> hwResIds;
    auto ret = UbseUrmaControllerManager::GetInstance().GetAllUrmaInfo(names, status, hwResIds);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(names.size(), 2);
    ASSERT_EQ(status.size(), 2);
    ASSERT_EQ(hwResIds.size(), 2);
    EXPECT_EQ(hwResIds[0], 1);
    EXPECT_EQ(hwResIds[1], 2);
    if (names[0] == "urma_h") {
        EXPECT_EQ(status[0], static_cast<uint32_t>(UrmaDevState::ACTIVED));
        EXPECT_EQ(status[1], static_cast<uint32_t>(UrmaDevState::INACTIVED));
    } else {
        EXPECT_EQ(status[0], static_cast<uint32_t>(UrmaDevState::INACTIVED));
        EXPECT_EQ(status[1], static_cast<uint32_t>(UrmaDevState::ACTIVED));
    }
}

TEST_F(TestUbseUrmaControllerManager, GetUrmaInfoForQuery_UbseGetCurrentNodeInfoFails)
{
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    std::vector<UbseUrmaInfoForQuery> devInfos;
    UbseUrmaControllerManager::GetInstance().GetUrmaInfoForQuery(devInfos);
    EXPECT_TRUE(devInfos.empty());
}

TEST_F(TestUbseUrmaControllerManager, GetUrmaInfoForQuery_NoNodeInfo)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(QueryAllPortsDown).stubs().will(returnValue(UBSE_ERROR));
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    std::vector<UbseUrmaInfoForQuery> devInfos;
    UbseUrmaControllerManager::GetInstance().GetUrmaInfoForQuery(devInfos);
    EXPECT_TRUE(devInfos.empty());
}

TEST_F(TestUbseUrmaControllerManager, GetUrmaInfoForQuery_EmptyUrmaList)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(QueryAllPortsDown).stubs().will(returnValue(UBSE_ERROR));
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"] = {};
    std::vector<UbseUrmaInfoForQuery> devInfos;
    UbseUrmaControllerManager::GetInstance().GetUrmaInfoForQuery(devInfos);
    EXPECT_TRUE(devInfos.empty());
}

TEST_F(TestUbseUrmaControllerManager, GetUrmaInfoForQuery_WrongEidGroupCount)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(QueryAllPortsDown).stubs().will(returnValue(UBSE_ERROR));
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaInfo urmaInfo{
        .urmaDevEid = "dev_eid_1", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::ACTIVED};
    urmaInfo.eidGroups.push_back({.primaryEid = "peid_1"});
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urma_1"] = urmaInfo;
    std::vector<UbseUrmaInfoForQuery> devInfos;
    UbseUrmaControllerManager::GetInstance().GetUrmaInfoForQuery(devInfos);
    EXPECT_TRUE(devInfos.empty());
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, GetUrmaInfoForQuery_ValidEntry)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(QueryAllPortsDown).stubs().will(returnValue(UBSE_ERROR));
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    auto feInfo = std::make_shared<UbseFeInfo>();
    feInfo->name = "fe_name";
    UbseUrmaInfo urmaInfo{
        .urmaDevEid = "dev_eid_1", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::ACTIVED};
    urmaInfo.urmaQosProfile.profileName = "qos_test";
    urmaInfo.eidGroups.push_back({.primaryEid = "peid_1", .feInfo = feInfo});
    urmaInfo.eidGroups.push_back({.primaryEid = "peid_2", .feInfo = feInfo});
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urma_1"] = urmaInfo;
    std::vector<UbseUrmaInfoForQuery> devInfos;
    UbseUrmaControllerManager::GetInstance().GetUrmaInfoForQuery(devInfos);
    ASSERT_EQ(devInfos.size(), 1);
    EXPECT_EQ(devInfos[0].urmaName, "urma_1");
    ASSERT_EQ(devInfos[0].feEids.size(), 2);
    EXPECT_EQ(devInfos[0].feEids[0], "peid_1");
    EXPECT_EQ(devInfos[0].feEids[1], "peid_2");
    ASSERT_EQ(devInfos[0].feNames.size(), 2);
    EXPECT_EQ(devInfos[0].feNames[0], "fe_name");
    EXPECT_EQ(devInfos[0].feNames[1], "fe_name");
    EXPECT_EQ(devInfos[0].devEid, "dev_eid_1");
    EXPECT_EQ(devInfos[0].bondingType, UrmaDevType::UNIQUE);
    EXPECT_EQ(devInfos[0].state, UrmaDevState::ACTIVED);
    EXPECT_EQ(devInfos[0].qosProfile.profileName, "qos_test");
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, GetUrmaInfoForQuery_NullFeInfo)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    MOCKER_CPP(QueryAllPortsDown).stubs().will(returnValue(UBSE_ERROR));
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaInfo urmaInfo{
        .urmaDevEid = "dev_eid_1", .urmaDevType = UrmaDevType::UNIQUE, .state = UrmaDevState::ACTIVED};
    urmaInfo.eidGroups.push_back({.primaryEid = "peid_1", .feInfo = nullptr});
    urmaInfo.eidGroups.push_back({.primaryEid = "peid_2", .feInfo = nullptr});
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urma_1"] = urmaInfo;
    std::vector<UbseUrmaInfoForQuery> devInfos;
    UbseUrmaControllerManager::GetInstance().GetUrmaInfoForQuery(devInfos);
    ASSERT_EQ(devInfos.size(), 1);
    ASSERT_EQ(devInfos[0].feNames.size(), 2);
    EXPECT_EQ(devInfos[0].feNames[0], "");
    EXPECT_EQ(devInfos[0].feNames[1], "");
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, AllocByUrmaName_NotActived)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaInfo urmaInfo;
    urmaInfo.state = UrmaDevState::INACTIVED;
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["urma_1"] = urmaInfo;
    std::vector<std::string> feNames;
    std::string eid;
    auto ret = UbseUrmaControllerManager::GetInstance().AllocByUrmaName("urma_1", feNames, eid);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, GetLocalUrmaDevInfoNameNotFound)
{
    UbseRoleInfo role;
    role.nodeId = "0";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"] = {};
    UbseUrmaInfo urmaInfo;
    auto ret = UbseUrmaControllerManager::GetInstance().GetLocalUrmaDevInfo("nonexistent", urmaInfo);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, SetAllUrmaInfoToInactiveForNodeEmptyNode)
{
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().SetAllUrmaInfoToInactiveForNode("non_existent");
    EXPECT_TRUE(UbseUrmaControllerManager::GetInstance().nodeInfos.empty());
}

TEST_F(TestUbseUrmaControllerManager, SetAllUrmaInfoToActiveForNodeEmptyNode)
{
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().SetAllUrmaInfoToActiveForNode("non_existent");
    EXPECT_TRUE(UbseUrmaControllerManager::GetInstance().nodeInfos.empty());
}

TEST_F(TestUbseUrmaControllerManager, ConstructNewUrmaInfo_TriggersFeInfoCmp)
{
    std::vector<std::vector<UbseMtiFeInfo>> feInfos = g_feInfosWithMultiPerIou;
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().feIdMap = {};
    auto ret = UbseUrmaControllerManager::GetInstance().ConstructNewUrmaInfo("0", feInfos);
    EXPECT_EQ(UBSE_OK, ret);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    UbseUrmaControllerManager::GetInstance().feIdMap = {};
}

TEST_F(TestUbseUrmaControllerManager, IsUdmaDevHealthy_HealthyPath)
{
    std::string feEid = "test_eid";
    std::string subpath = "/dev/uburma/test";
    MOCKER_CPP(UbseGetUrmaSubpathByEid).stubs().with(eq(feEid), outBound(subpath)).will(returnValue(UBSE_OK));
    bool result = IsUdmaDevHealthy(feEid);
    EXPECT_TRUE(result);
}

TEST_F(TestUbseUrmaControllerManager, IsUdmaDevHealthy_ReturnError)
{
    std::string feEid = "test_eid";
    std::string subpath = "";
    MOCKER_CPP(UbseGetUrmaSubpathByEid).stubs().with(eq(feEid), outBound(subpath)).will(returnValue(UBSE_ERROR));
    bool result = IsUdmaDevHealthy(feEid);
    EXPECT_FALSE(result);
}

TEST_F(TestUbseUrmaControllerManager, IsUdmaDevHealthy_EmptySubpath)
{
    std::string feEid = "test_eid";
    std::string subpath = "";
    MOCKER_CPP(UbseGetUrmaSubpathByEid).stubs().with(eq(feEid), outBound(subpath)).will(returnValue(UBSE_OK));
    bool result = IsUdmaDevHealthy(feEid);
    EXPECT_FALSE(result);
}
} // namespace ubse::urmaControllerManager::ut