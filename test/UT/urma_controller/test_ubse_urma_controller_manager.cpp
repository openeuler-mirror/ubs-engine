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