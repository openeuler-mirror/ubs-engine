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
#include "ubse_smbios.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_manager.h"
#include "ubse_urma_def.h"
#include "ubse_urma_uvs_module.h"
#include "test_ubse_urma_controller.h"

namespace ubse::urmaController {
UbseResult InferEidGroup(uint32_t serverIdx, urma::EidGroup& group);
UbseResult InferUrmaListDevInfo(uint32_t serverIdx, uint32_t nodeId,
                                std::map<std::string, UbseUrmaInfo, urma::UrmaNameCompare>& urmaList);
UbseResult SplitFeInfosForClos(const std::string& nodeId, const std::vector<std::vector<UbseMtiFeInfo>>& feInfos,
                               std::vector<std::vector<UbseMtiFeInfo>>& containerFeInfos,
                               std::vector<std::vector<UbseMtiFeInfo>>& hostFeInfos);
UbseResult FilterFeInfosForNonClos(const std::string& nodeId, const std::vector<std::vector<UbseMtiFeInfo>>& feInfos,
                                   std::vector<std::vector<UbseMtiFeInfo>>& containerFeInfos,
                                   std::vector<std::vector<UbseMtiFeInfo>>& hostFeInfos);
} // namespace ubse::urmaController

namespace ubse::urmaControllerManager::ut {
using namespace ubse::urmaController;
using namespace ubse::urma;
using namespace ubse::election;
using namespace ubse::adapter_plugins::mti;
using namespace ubse::adapter_plugins::smbios;
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

TEST_F(TestUbseUrmaControllerManager, GetHostUrmaDev_Success)
{
    UbseUrmaUvsNodeInfo nodeInfo{.nodeId = "1"};
    UbseUrmaUvsAggrDev aggrDev{.urmaDevEid = "1234"};
    nodeInfo.devList.push_back(aggrDev);
    std::vector<UbseUrmaUvsNodeInfo> mockHostUrmaInfos{nodeInfo};
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(_, outBound(mockHostUrmaInfos))
        .will(returnValue(UBSE_OK));
    UbseUrmaUvsNodeInfo uvsInfo{.nodeId = "1"};
    auto ret = GetHostUrmaDev("1", uvsInfo);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(uvsInfo.devList[0].urmaDevEid, "1234");
}

TEST_F(TestUbseUrmaControllerManager, GetHostUrmaDev_ComFailure)
{
    MOCKER_CPP(&UbseNodeComUrmaCollector::GetPlanningHostBondingByNodeId).stubs().will(returnValue(UBSE_ERROR));
    UbseUrmaUvsNodeInfo uvsInfo{.nodeId = "1"};
    auto ret = GetHostUrmaDev("1", uvsInfo);
    EXPECT_EQ(ret, UBSE_ERROR);
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
TEST_F(TestUbseUrmaControllerManager, CompareStringsNumerically)
{
    EXPECT_TRUE(CompareStringsNumerically("10", "100"));
    EXPECT_FALSE(CompareStringsNumerically("100", "10"));
    EXPECT_TRUE(CompareStringsNumerically("5", "10"));
    EXPECT_FALSE(CompareStringsNumerically("10", "5"));
    EXPECT_TRUE(CompareStringsNumerically("0010", "0100"));
    EXPECT_TRUE(CompareStringsNumerically("abc", "abd"));
    EXPECT_FALSE(CompareStringsNumerically("abd", "abc"));
}

TEST_F(TestUbseUrmaControllerManager, ConvertLcneFeTypeToUrmaFeType_AllTypes)
{
    EXPECT_EQ(ConvertLcneFeTypeToUrmaFeType(UbseMtiFeType::PHYSICAL_TYPE), FeType::PHYSICAL_TYPE);
    EXPECT_EQ(ConvertLcneFeTypeToUrmaFeType(UbseMtiFeType::VIRTUAL_TYPE), FeType::VIRTUAL_TYPE);
    EXPECT_EQ(ConvertLcneFeTypeToUrmaFeType(UbseMtiFeType::BUTT_TYPE), FeType::BUTT_TYPE);
}

TEST_F(TestUbseUrmaControllerManager, GenerateHwResId)
{
    UbseMtiFeInfo fe{.iouId = "2", .entityId = "3"};
    uint64_t expected = (static_cast<uint64_t>(2) << 32) | static_cast<uint64_t>(3);
    EXPECT_EQ(GenerateHwResId(fe), expected);

    fe.iouId = "1";
    fe.entityId = "2";
    EXPECT_GT(GenerateHwResId(fe), 0);
}

TEST_F(TestUbseUrmaControllerManager, ValidateLcneFeInfo)
{
    std::vector<std::vector<UbseMtiFeInfo>> feInfos(1);
    EXPECT_FALSE(ValidateLcneFeInfo(feInfos));

    feInfos.resize(2);
    feInfos[0].resize(1);
    feInfos[1].resize(2);
    EXPECT_FALSE(ValidateLcneFeInfo(feInfos));

    feInfos[0].clear();
    feInfos[1].clear();
    UbseMtiFeInfo fe{.slotId = "not_a_number", .ubpuId = "0", .iouId = "0", .entityId = "0"};
    feInfos[0].push_back(fe);
    feInfos[1].push_back(fe);
    EXPECT_FALSE(ValidateLcneFeInfo(feInfos));

    feInfos.resize(3);
    for (auto& vec : feInfos)
        vec.clear();
    fe.slotId = "0";
    for (auto& vec : feInfos)
        vec.push_back(fe);
    EXPECT_TRUE(ValidateLcneFeInfo(feInfos));

    feInfos.resize(2);
    feInfos[0].clear();
    feInfos[1].clear();
    feInfos[0].push_back(fe);
    feInfos[1].push_back(fe);
    EXPECT_TRUE(ValidateLcneFeInfo(feInfos));

    feInfos[0].clear();
    feInfos[1].clear();
    EXPECT_FALSE(ValidateLcneFeInfo(feInfos));
}

TEST_F(TestUbseUrmaControllerManager, CalculateFeTopoType)
{
    auto& mgr = UbseUrmaControllerManager::GetInstance();
    // ALL_PFE: 2 IOUs, 6 PFE each
    std::vector<std::vector<UbseMtiFeInfo>> feInfos(2);
    for (size_t i = 0; i < 2; ++i) {
        for (int j = 0; j < 6; ++j) {
            UbseMtiFeInfo fe{.slotId = "0",
                             .ubpuId = std::to_string(j),
                             .iouId = std::to_string(i),
                             .entityId = std::to_string(j),
                             .fetype = UbseMtiFeType::PHYSICAL_TYPE};
            feInfos[i].push_back(fe);
        }
    }
    CalculateFeTopoType(feInfos);
    EXPECT_EQ(mgr.GetFeTopoType(), FeTopoType::ALL_PFE);

    // PFE_VFE_HYBRID: 2 IOUs, 1PFE+5VFE each
    feInfos.clear();
    feInfos.resize(2);
    for (size_t i = 0; i < 2; ++i) {
        UbseMtiFeInfo pfe{.slotId = "0",
                          .ubpuId = "0",
                          .iouId = std::to_string(i),
                          .entityId = "0",
                          .fetype = UbseMtiFeType::PHYSICAL_TYPE};
        feInfos[i].push_back(pfe);
        for (int j = 0; j < 5; ++j) {
            UbseMtiFeInfo vfe{.slotId = "0",
                              .ubpuId = std::to_string(j + 1),
                              .iouId = std::to_string(i),
                              .entityId = std::to_string(j + 1),
                              .fetype = UbseMtiFeType::VIRTUAL_TYPE};
            feInfos[i].push_back(vfe);
        }
    }
    CalculateFeTopoType(feInfos);
    EXPECT_EQ(mgr.GetFeTopoType(), FeTopoType::PFE_VFE_HYBRID);

    // INVALID: <6 FE per IOU
    feInfos.clear();
    feInfos.resize(2);
    for (size_t i = 0; i < 2; ++i) {
        for (int j = 0; j < 3; ++j) {
            UbseMtiFeInfo fe{.slotId = "0",
                             .ubpuId = std::to_string(j),
                             .iouId = std::to_string(i),
                             .entityId = std::to_string(j),
                             .fetype = UbseMtiFeType::PHYSICAL_TYPE};
            feInfos[i].push_back(fe);
        }
    }
    CalculateFeTopoType(feInfos);
    EXPECT_EQ(mgr.GetFeTopoType(), FeTopoType::INVALID);

    // Mismatch between IOUs
    feInfos.clear();
    feInfos.resize(2);
    for (int j = 0; j < 6; ++j) {
        UbseMtiFeInfo fe{.slotId = "0",
                         .ubpuId = std::to_string(j),
                         .iouId = "0",
                         .entityId = std::to_string(j),
                         .fetype = UbseMtiFeType::PHYSICAL_TYPE};
        feInfos[0].push_back(fe);
    }
    UbseMtiFeInfo pfe{
        .slotId = "0", .ubpuId = "0", .iouId = "1", .entityId = "0", .fetype = UbseMtiFeType::PHYSICAL_TYPE};
    feInfos[1].push_back(pfe);
    for (int j = 0; j < 5; ++j) {
        UbseMtiFeInfo vfe{.slotId = "0",
                          .ubpuId = std::to_string(j + 1),
                          .iouId = "1",
                          .entityId = std::to_string(j + 1),
                          .fetype = UbseMtiFeType::VIRTUAL_TYPE};
        feInfos[1].push_back(vfe);
    }
    CalculateFeTopoType(feInfos);
    EXPECT_EQ(mgr.GetFeTopoType(), FeTopoType::INVALID);
}

TEST_F(TestUbseUrmaControllerManager, MakeEidGroup)
{
    UbseMtiEidGroup src;
    src.primaryEid = "test_eid";
    src.portEids["key1"] = "val1";
    auto feInfo = std::make_shared<UbseFeInfo>();
    feInfo->entityId = "test_entity";
    EidGroup result = MakeEidGroup(src, feInfo);
    EXPECT_EQ(result.primaryEid, "test_eid");
    EXPECT_EQ(result.portEids["key1"], "val1");
    EXPECT_EQ(result.feInfo, feInfo);
}

TEST_F(TestUbseUrmaControllerManager, GenerateUniqueFeId)
{
    UbseUrmaControllerManager::GetInstance().globalFeId = 0;
    uint16_t id1 = UbseUrmaControllerManager::GetInstance().GenerateUniqueFeId();
    uint16_t id2 = UbseUrmaControllerManager::GetInstance().GenerateUniqueFeId();
    EXPECT_EQ(id1, 1);
    EXPECT_EQ(id2, 2);
}

TEST_F(TestUbseUrmaControllerManager, GenerateUrmaDevId)
{
    UbseUrmaControllerManager::GetInstance().globalUrmaId = 0;
    uint64_t id1 = UbseUrmaControllerManager::GetInstance().GenerateUrmaDevId();
    uint64_t id2 = UbseUrmaControllerManager::GetInstance().GenerateUrmaDevId();
    EXPECT_EQ(id1, 1);
    EXPECT_EQ(id2, 2);
}

TEST_F(TestUbseUrmaControllerManager, GetLocalUrmaDevInfoByName)
{
    auto& mgr = UbseUrmaControllerManager::GetInstance();
    ClearNodeInfosForTest();
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    UbseUrmaInfo urmaInfo;
    EXPECT_EQ(mgr.GetLocalUrmaDevInfoByName("test", urmaInfo), UBSE_URMACONTRL_ERROR_GET_NODE_INFO_FAILED);
    GlobalMockObject::verify();

    UbseRoleInfo role;
    role.nodeId = "nonexistent";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    EXPECT_EQ(mgr.GetLocalUrmaDevInfoByName("test", urmaInfo), UBSE_URMACONTRL_ERROR_DEV_NOT_EXIST);
    GlobalMockObject::verify();

    role.nodeId = "0";
    mgr.nodeInfos["0"];
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    EXPECT_EQ(mgr.GetLocalUrmaDevInfoByName("nonexistent", urmaInfo), UBSE_URMACONTRL_ERROR_DEV_NOT_EXIST);
    GlobalMockObject::verify();

    UbseUrmaInfo expectedInfo{
        .urmaDevEid = "test_eid", .urmaDevType = UrmaDevType::SHARED, .state = UrmaDevState::ACTIVED};
    mgr.nodeInfos["0"].urmaList["test_urma"] = expectedInfo;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    EXPECT_EQ(mgr.GetLocalUrmaDevInfoByName("test_urma", urmaInfo), UBSE_OK);
    EXPECT_EQ(urmaInfo.urmaDevEid, "test_eid");
    mgr.nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, AllocUrmaDev)
{
    auto& mgr = UbseUrmaControllerManager::GetInstance();
    std::vector<std::string> feNames;
    std::string eid;
    ClearNodeInfosForTest();
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(mgr.AllocUrmaDev("test", feNames, eid), UBSE_URMACONTRL_ERROR_GET_NODE_INFO_FAILED);
    GlobalMockObject::verify();

    UbseRoleInfo role;
    role.nodeId = "nonexistent";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    EXPECT_EQ(mgr.AllocUrmaDev("test", feNames, eid), UBSE_URMACONTRL_ERROR_DEV_NOT_EXIST);
    GlobalMockObject::verify();

    role.nodeId = "0";
    mgr.nodeInfos["0"];
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    EXPECT_EQ(mgr.AllocUrmaDev("nonexistent", feNames, eid), UBSE_URMACONTRL_ERROR_DEV_NOT_EXIST);
    mgr.nodeInfos = {};
    GlobalMockObject::verify();

    UbseUrmaInfo urmaInfo{.urmaDevEid = "eid", .urmaDevType = UrmaDevType::SHARED, .state = UrmaDevState::INACTIVED};
    urmaInfo.subPath = "subpath";
    mgr.nodeInfos["0"].urmaList["test_urma"] = urmaInfo;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    EXPECT_EQ(mgr.AllocUrmaDev("test_urma", feNames, eid), UBSE_URMACONTRL_ERROR_DEV_NOT_INACTIVE);
    mgr.nodeInfos = {};
    GlobalMockObject::verify();

    urmaInfo.state = UrmaDevState::ACTIVED;
    urmaInfo.eidGroups.resize(1);
    urmaInfo.eidGroups[0].feInfo = nullptr;
    urmaInfo.eidGroups[0].primaryEid = "eid1";
    mgr.nodeInfos["0"].urmaList["test_urma"] = urmaInfo;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    EXPECT_EQ(mgr.AllocUrmaDev("test_urma", feNames, eid), UBSE_OK);
    EXPECT_EQ(eid, "eid");
    EXPECT_GE(feNames.size(), 1);
    mgr.nodeInfos = {};
    GlobalMockObject::verify();

    urmaInfo.eidGroups.resize(2);
    urmaInfo.eidGroups[0].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo.eidGroups[0].feInfo->name = "fe0_name";
    urmaInfo.eidGroups[1].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo.eidGroups[1].feInfo->name = "fe1_name";
    mgr.nodeInfos["0"].urmaList["test_urma"] = urmaInfo;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    EXPECT_EQ(mgr.AllocUrmaDev("test_urma", feNames, eid), UBSE_OK);
    EXPECT_GE(feNames.size(), 3);
    EXPECT_EQ(eid, "eid");
    EXPECT_EQ(feNames[0], "subpath");
    mgr.nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, SetUrmaDevStateByDevEid)
{
    auto& mgr = UbseUrmaControllerManager::GetInstance();
    ClearNodeInfosForTest();
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().will(returnValue(UBSE_ERROR));
    mgr.SetUrmaDevStateByDevEid("test_eid", UrmaDevState::ACTIVED);

    GlobalMockObject::verify();

    UbseRoleInfo role;
    role.nodeId = "nonexistent";
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    mgr.nodeInfos = {};
    mgr.SetUrmaDevStateByDevEid("test_eid", UrmaDevState::ACTIVED);

    GlobalMockObject::verify();

    role.nodeId = "0";
    UbseUrmaInfo urmaInfo{.urmaDevEid = "other_eid", .state = UrmaDevState::UNKNOWN};
    mgr.nodeInfos["0"].urmaList["urma1"] = urmaInfo;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    mgr.SetUrmaDevStateByDevEid("nonexistent_eid", UrmaDevState::ACTIVED);
    EXPECT_EQ(mgr.nodeInfos["0"].urmaList["urma1"].state, UrmaDevState::UNKNOWN);
    mgr.nodeInfos = {};
    GlobalMockObject::verify();

    urmaInfo.urmaDevEid = "target_eid";
    mgr.nodeInfos["0"].urmaList["urma1"] = urmaInfo;
    MOCKER_CPP(UbseGetCurrentNodeInfo).stubs().with(outBound(role)).will(returnValue(UBSE_OK));
    mgr.SetUrmaDevStateByDevEid("target_eid", UrmaDevState::ACTIVED);
    EXPECT_EQ(mgr.nodeInfos["0"].urmaList["urma1"].state, UrmaDevState::ACTIVED);
    mgr.nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, SetAllUrmaDevStateForNode)
{
    auto& mgr = UbseUrmaControllerManager::GetInstance();
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(UbseNodeInfo{}));
    mgr.SetAllUrmaDevStateForNode(UrmaDevState::ACTIVED);

    GlobalMockObject::verify();

    UbseNodeInfo curNode;
    curNode.nodeId = "nonexistent";
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    mgr.nodeInfos = {};
    mgr.SetAllUrmaDevStateForNode(UrmaDevState::ACTIVED);

    GlobalMockObject::verify();

    ClearNodeInfosForTest();
    curNode.nodeId = "0";
    UbseUrmaInfo urmaInfo1{.state = UrmaDevState::UNKNOWN};
    UbseUrmaInfo urmaInfo2{.state = UrmaDevState::UNKNOWN};
    mgr.nodeInfos["0"].urmaList["urma1"] = urmaInfo1;
    mgr.nodeInfos["0"].urmaList["urma2"] = urmaInfo2;
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    mgr.SetAllUrmaDevStateForNode(UrmaDevState::ACTIVED);
    EXPECT_EQ(mgr.nodeInfos["0"].urmaList["urma1"].state, UrmaDevState::ACTIVED);
    EXPECT_EQ(mgr.nodeInfos["0"].urmaList["urma2"].state, UrmaDevState::ACTIVED);
    mgr.nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, GetUrmaNodeInfoMisc)
{
    auto& mgr = UbseUrmaControllerManager::GetInstance();
    mgr.nodeInfos = {};
    auto nodeInfo = mgr.GetUrmaNodeInfo("new_node");
    EXPECT_EQ(nodeInfo.nodeId, "new_node");
    EXPECT_TRUE(nodeInfo.urmaList.empty());
    GlobalMockObject::verify();

    ClearNodeInfosForTest();
    UbseUrmaInfo urmaInfo{.urmaDevEid = "eid123"};
    mgr.nodeInfos["0"].urmaList["urma1"] = urmaInfo;
    mgr.nodeInfos["0"].nodeId = "0";
    nodeInfo = mgr.GetUrmaNodeInfo("0");
    EXPECT_EQ(nodeInfo.nodeId, "0");
    EXPECT_EQ(nodeInfo.urmaList["urma1"].urmaDevEid, "eid123");
    mgr.nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, GetUrmaUpdateTimeStamp)
{
    auto& mgr = UbseUrmaControllerManager::GetInstance();
    EXPECT_EQ(mgr.GetUrmaUpdateTimeStamp("nonexistent"), 0);
    ClearNodeInfosForTest();
    mgr.nodeInfos["0"].updateTimeStamp = 12345;
    EXPECT_EQ(mgr.GetUrmaUpdateTimeStamp("0"), 12345);
    mgr.nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, InsertNewNodeInfo_OlderTimestamp)
{
    ClearNodeInfosForTest();
    UbseUrmaNodeInfo existingNode;
    existingNode.nodeId = "0";
    existingNode.updateTimeStamp = 100;
    UbseUrmaInfo existingInfo{.urmaDevEid = "old_eid"};
    existingNode.urmaList["old_urma"] = existingInfo;
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"] = existingNode;

    UbseUrmaNodeInfo newNode;
    newNode.updateTimeStamp = 50;
    UbseUrmaInfo newInfo{.urmaDevEid = "new_eid"};
    newNode.urmaList["new_urma"] = newInfo;
    UbseUrmaControllerManager::GetInstance().InsertNewNodeInfo("0", newNode);
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList.count("old_urma"), 1);
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList.count("new_urma"), 0);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, IsLcneFeUsed)
{
    auto& mgr = UbseUrmaControllerManager::GetInstance();
    std::map<std::string, UbseUrmaInfo, UrmaNameCompare> urmaList;
    UbseUrmaInfo urmaInfo;
    urmaInfo.eidGroups.resize(2);
    auto fe0 = std::make_shared<UbseFeInfo>();
    fe0->slotId = "s0";
    fe0->ubpuId = "u0";
    fe0->iouId = "i0";
    fe0->entityId = "e0";
    auto fe1 = std::make_shared<UbseFeInfo>();
    fe1->slotId = "s1";
    fe1->ubpuId = "u1";
    fe1->iouId = "i1";
    fe1->entityId = "e1";
    urmaInfo.eidGroups[0].feInfo = fe0;
    urmaInfo.eidGroups[1].feInfo = fe1;
    urmaList["urma1"] = urmaInfo;
    UbseMtiFeInfo fe0_in{.slotId = "s0", .ubpuId = "u0", .iouId = "i0", .entityId = "e0"};
    UbseMtiFeInfo fe1_in{.slotId = "s1", .ubpuId = "u1", .iouId = "i1", .entityId = "e1"};
    EXPECT_TRUE(mgr.IsLcneFeUsed(fe0_in, fe1_in, urmaList));

    fe0->slotId = "s0";
    fe0->entityId = "e0";
    fe0->ubpuId = "";
    fe0->iouId = "";
    fe1->slotId = "s1";
    fe1->entityId = "e1";
    fe1->ubpuId = "";
    fe1->iouId = "";
    UbseMtiFeInfo feX{.slotId = "sX", .ubpuId = "uX", .iouId = "iX", .entityId = "eX"};
    UbseMtiFeInfo feY{.slotId = "sY", .ubpuId = "uY", .iouId = "iY", .entityId = "eY"};
    EXPECT_FALSE(mgr.IsLcneFeUsed(feX, feY, urmaList));

    urmaInfo.eidGroups.resize(1);
    urmaInfo.eidGroups[0].feInfo = fe0;
    urmaList["urma1"] = urmaInfo;
    EXPECT_FALSE(mgr.IsLcneFeUsed(UbseMtiFeInfo{.slotId = "s0"}, UbseMtiFeInfo{.slotId = "s1"}, urmaList));

    urmaInfo.eidGroups.resize(2);
    urmaInfo.eidGroups[0].feInfo = nullptr;
    urmaInfo.eidGroups[1].feInfo = nullptr;
    urmaList["urma1"] = urmaInfo;
    EXPECT_FALSE(mgr.IsLcneFeUsed(UbseMtiFeInfo{.slotId = "s0"}, UbseMtiFeInfo{.slotId = "s1"}, urmaList));
}

TEST_F(TestUbseUrmaControllerManager, ParseFeIdsFromEid)
{
    std::pair<uint16_t, uint16_t> feIds;
    EXPECT_NE(ParseFeIdsFromEid("short", feIds), UBSE_OK);
    EXPECT_NE(ParseFeIdsFromEid("", feIds), UBSE_OK);
    EXPECT_NE(ParseFeIdsFromEid("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx", feIds), UBSE_OK);
    std::string wrongDelim = "0000-0000-0001-0002-0003-0004-0005-0006";
    EXPECT_NE(ParseFeIdsFromEid(wrongDelim, feIds), UBSE_OK);
    EXPECT_EQ(ParseFeIdsFromEid("0000:0000:0001:0002:0003:0004:0005:0006", feIds), UBSE_OK);
}

TEST_F(TestUbseUrmaControllerManager, FindMatchingFeInfo)
{
    std::map<std::string, UbseUrmaInfo, UrmaNameCompare> urmaList;
    uint64_t hwResId = 0;
    EXPECT_EQ(FindMatchingFeInfo(urmaList, "nonexistent", "nonexistent", hwResId), nullptr);

    UbseUrmaInfo urmaInfo;
    urmaInfo.hwResId = 42;
    urmaInfo.eidGroups.resize(1);
    urmaInfo.eidGroups[0].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo.eidGroups[0].feInfo->ubpuId = "target_ubpu";
    urmaInfo.eidGroups[0].feInfo->entityId = "target_entity";
    urmaList["urma1"] = urmaInfo;
    hwResId = 0;
    auto result = FindMatchingFeInfo(urmaList, "target_ubpu", "target_entity", hwResId);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->ubpuId, "target_ubpu");
    EXPECT_EQ(hwResId, 42);
}

TEST_F(TestUbseUrmaControllerManager, FetchCurNodeComDev)
{
    UbseUrmaUvsNodeInfo nodeInfo{.nodeId = "1"};
    UbseUrmaUvsAggrDev aggrDev{.urmaDevEid = "com_eid"};
    UbseUrmaUvsFe fe{.primaryEid = "fe_eid"};
    aggrDev.feList.push_back(fe);
    aggrDev.feList.push_back(fe);
    nodeInfo.devList.push_back(aggrDev);
    std::vector<UbseUrmaUvsNodeInfo> mockHostUrmaInfos{nodeInfo};
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(_, outBound(mockHostUrmaInfos))
        .will(returnValue(UBSE_OK));
    UbseUrmaUvsAggrDev comDev;
    EXPECT_EQ(FetchCurNodeComDev("1", comDev), UBSE_OK);
    EXPECT_EQ(comDev.urmaDevEid, "com_eid");
    GlobalMockObject::verify();

    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId).stubs().will(returnValue(UBSE_ERROR));
    EXPECT_EQ(FetchCurNodeComDev("1", comDev), UBSE_ERROR);
    GlobalMockObject::verify();

    nodeInfo.devList.resize(2);
    mockHostUrmaInfos = {nodeInfo};
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(_, outBound(mockHostUrmaInfos))
        .will(returnValue(UBSE_OK));
    EXPECT_EQ(FetchCurNodeComDev("1", comDev), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, BuildHostUrmaDev)
{
    auto& mgr = UbseUrmaControllerManager::GetInstance();
    std::map<std::string, UbseUrmaInfo, UrmaNameCompare> urmaList;
    UbseUrmaInfo urmaInfo;
    urmaInfo.hwResId = 100;
    urmaInfo.eidGroups.resize(1);
    urmaInfo.eidGroups[0].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo.eidGroups[0].feInfo->ubpuId = "fe_ubpu";
    urmaInfo.eidGroups[0].feInfo->entityId = "fe_entity";
    urmaList["urma1"] = urmaInfo;
    UbseUrmaUvsAggrDev comDev{.urmaDevEid = "com_eid"};
    UbseUrmaUvsFe fe{.ubpuId = "fe_ubpu", .entityId = "fe_entity", .primaryEid = "fe_primary_eid"};
    comDev.feList.push_back(fe);
    UbseUrmaInfo urmaDev;
    EXPECT_EQ(mgr.BuildHostUrmaDev(comDev, urmaList, urmaDev), UBSE_OK);
    EXPECT_EQ(urmaDev.urmaDevEid, "com_eid");
    EXPECT_EQ(urmaDev.hwResId, 100);
    GlobalMockObject::verify();

    comDev.feList.clear();
    fe.ubpuId = "nobody";
    fe.entityId = "noentity";
    comDev.feList.push_back(fe);
    EXPECT_EQ(mgr.BuildHostUrmaDev(comDev, urmaList, urmaDev), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, InsertHostUrmaDevInner_NotClosType)
{
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    auto ret = UbseUrmaControllerManager::GetInstance().InsertHostUrmaDevInner();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerManager, InsertHostUrmaDevInner_InvalidFeTopo)
{
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    UbseUrmaControllerManager::GetInstance().SetFeTopoType(FeTopoType::INVALID);
    auto ret = UbseUrmaControllerManager::GetInstance().InsertHostUrmaDevInner();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, InsertHostUrmaDevInner_HostBondingOccupied)
{
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    UbseUrmaControllerManager::GetInstance().SetFeTopoType(FeTopoType::ALL_PFE);
    MOCKER_CPP(&UbseNodeController::IsHostBondingRegistered).stubs().will(returnValue(true));
    auto ret = UbseUrmaControllerManager::GetInstance().InsertHostUrmaDevInner();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerManager, ProcessUrmaBondings)
{
    auto& mgr = UbseUrmaControllerManager::GetInstance();
    ClearNodeInfosForTest();
    std::vector<std::vector<UbseMtiFeInfo>> feInfos;
    feInfos.resize(1);
    std::map<std::string, UbseUrmaInfo, UrmaNameCompare> urmaList;
    EXPECT_EQ(mgr.ProcessUrmaBondings("0", 0, feInfos, urmaList), UBSE_OK);

    feInfos.clear();
    feInfos.resize(2);
    UbseMtiFeInfo fe{.slotId = "0", .ubpuId = "0", .iouId = "0", .entityId = "0"};
    feInfos[0].push_back(fe);
    feInfos[1].push_back(fe);
    EXPECT_EQ(mgr.ProcessUrmaBondings("0", 0, feInfos, urmaList), UBSE_OK);
}

TEST_F(TestUbseUrmaControllerManager, SortContainerFeInfos)
{
    std::vector<std::vector<UbseMtiFeInfo>> feInfos{{{.slotId = "0", .ubpuId = "2", .iouId = "1", .entityId = "2"},
                                                     {.slotId = "0", .ubpuId = "2", .iouId = "1", .entityId = "1"}},
                                                    {{.slotId = "0", .ubpuId = "1", .iouId = "0", .entityId = "2"},
                                                     {.slotId = "0", .ubpuId = "1", .iouId = "0", .entityId = "1"}}};
    SortContainerFeInfos(feInfos);
    EXPECT_EQ(feInfos[0][0].ubpuId, "1");
    EXPECT_EQ(feInfos[1][0].ubpuId, "2");
    EXPECT_EQ(feInfos[0][0].entityId, "1");
    EXPECT_EQ(feInfos[0][1].entityId, "2");
}

TEST_F(TestUbseUrmaControllerManager, AppendUrmaListToUvsDevList)
{
    std::map<std::string, UbseUrmaInfo, UrmaNameCompare> urmaList;
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "eid1";
    urmaInfo.eidGroups.resize(1);
    urmaInfo.eidGroups[0].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo.eidGroups[0].feInfo->ubpuId = "ubpu1";
    urmaInfo.eidGroups[0].feInfo->entityId = "entity1";
    urmaInfo.eidGroups[0].primaryEid = "primary1";
    urmaInfo.eidGroups[0].portEids["key1"] = "val1";
    urmaList["urma1"] = urmaInfo;
    UbseUrmaInfo urmaInfo2;
    urmaInfo2.urmaDevEid = "eid2";
    urmaInfo2.eidGroups.resize(2);
    urmaInfo2.eidGroups[0].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo2.eidGroups[0].feInfo->ubpuId = "ubpu2";
    urmaInfo2.eidGroups[0].feInfo->entityId = "entity2";
    urmaInfo2.eidGroups[0].primaryEid = "primary2";
    urmaInfo2.eidGroups[1].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo2.eidGroups[1].feInfo->ubpuId = "ubpu3";
    urmaInfo2.eidGroups[1].feInfo->entityId = "entity3";
    urmaInfo2.eidGroups[1].primaryEid = "primary3";
    urmaList["urma2"] = urmaInfo2;
    UbseUrmaUvsNodeInfo uvsInfo{.nodeId = "0"};
    EXPECT_EQ(AppendUrmaListToUvsDevList(urmaList, uvsInfo), UBSE_OK);
    EXPECT_EQ(uvsInfo.devList.size(), 2);
    EXPECT_EQ(uvsInfo.devList[0].urmaDevEid, "eid1");
    EXPECT_EQ(uvsInfo.devList[1].urmaDevEid, "eid2");
    EXPECT_EQ(uvsInfo.devList[0].feList.size(), 1);
    EXPECT_EQ(uvsInfo.devList[1].feList.size(), 2);

    urmaInfo.eidGroups[0].feInfo = nullptr;
    urmaList.clear();
    urmaList["urma1"] = urmaInfo;
    EXPECT_EQ(AppendUrmaListToUvsDevList(urmaList, uvsInfo), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, FillUrmaUvsNodeInfo)
{
    UbseUrmaNodeInfo nodeInfo{.nodeId = "0"};
    UbseUrmaUvsNodeInfo tmpUvsInfo;
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    EXPECT_EQ(FillUrmaUvsNodeInfo(true, nodeInfo, tmpUvsInfo), UBSE_OK);
    EXPECT_EQ(tmpUvsInfo.nodeId, "0");
    GlobalMockObject::verify();

    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    UbseUrmaControllerManager::GetInstance().SetFeTopoType(FeTopoType::INVALID);
    EXPECT_EQ(FillUrmaUvsNodeInfo(false, nodeInfo, tmpUvsInfo), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, IsSameFeWithHostUrmaDev)
{
    UbseUrmaUvsNodeInfo hostUrmaInfo;
    UbseUrmaUvsAggrDev dev;
    UbseUrmaUvsFe fe0{.primaryEid = "primary0"};
    UbseUrmaUvsFe fe1{.primaryEid = "primary1"};
    dev.feList.push_back(fe0);
    dev.feList.push_back(fe1);
    hostUrmaInfo.devList.push_back(dev);

    UbseMtiFeInfo lcneFe;
    UbseMtiEidGroup group{.primaryEid = "primary0"};
    lcneFe.eidGroups.push_back(group);
    EXPECT_TRUE(IsSameFeWithHostUrmaDev("1", hostUrmaInfo, lcneFe));

    lcneFe.eidGroups[0].primaryEid = "other";
    EXPECT_FALSE(IsSameFeWithHostUrmaDev("1", hostUrmaInfo, lcneFe));
}

TEST_F(TestUbseUrmaControllerManager, SwapCommEidGroupToHostPosition)
{
    std::vector<UbseMtiEidGroup> eidGroups(5);
    eidGroups[0].primaryEid = "a";
    eidGroups[1].primaryEid = "b";
    eidGroups[2].primaryEid = "c";
    eidGroups[3].primaryEid = "d";
    eidGroups[4].primaryEid = "e";
    SwapCommEidGroupToHostPosition(eidGroups, "c", 4);
    EXPECT_EQ(eidGroups[4].primaryEid, "c");
    EXPECT_EQ(eidGroups[2].primaryEid, "e");

    eidGroups.resize(2);
    eidGroups[0].primaryEid = "a";
    eidGroups[1].primaryEid = "b";
    SwapCommEidGroupToHostPosition(eidGroups, "notfound", 0);
    EXPECT_EQ(eidGroups[0].primaryEid, "a");
    EXPECT_EQ(eidGroups[1].primaryEid, "b");
}

TEST_F(TestUbseUrmaControllerManager, FilterCommEidGroupFromContainer)
{
    std::vector<std::vector<UbseMtiFeInfo>> containerFeInfos(1);
    UbseMtiFeInfo fe;
    UbseMtiEidGroup group1;
    group1.primaryEid = "to_filter";
    UbseMtiEidGroup group2;
    group2.primaryEid = "keep";
    fe.eidGroups.push_back(group1);
    fe.eidGroups.push_back(group2);
    containerFeInfos[0].push_back(fe);

    UbseUrmaUvsNodeInfo hostUrmaInfo;
    UbseUrmaUvsAggrDev dev;
    UbseUrmaUvsFe fe0;
    fe0.primaryEid = "to_filter";
    dev.feList.push_back(fe0);
    hostUrmaInfo.devList.push_back(dev);

    FilterCommEidGroupFromContainer(containerFeInfos, hostUrmaInfo);
    EXPECT_EQ(containerFeInfos[0][0].eidGroups.size(), 1);
    EXPECT_EQ(containerFeInfos[0][0].eidGroups[0].primaryEid, "keep");
}
TEST_F(TestUbseUrmaControllerManager, ConstructAndInsertUrmaBondingRollback)
{
    ClearNodeInfosForTest();
    UbseUrmaControllerManager::GetInstance().globalUrmaId = 100;
    UbseUrmaControllerManager::GetInstance().globalFeId = 200;

    std::map<std::string, UbseUrmaInfo, UrmaNameCompare> urmaList;
    UbseUrmaInfo info;
    urmaList["dev1"] = info;
    urmaList["dev2"] = info;
    urmaList["dev3"] = info;

    UrmaDevRollbackState state;
    state.feId = 10;
    state.urmaId = 20;
    state.urmaDevs.push_back("dev1");
    state.urmaDevs.push_back("dev2");

    UbseUrmaControllerManager::GetInstance().ConstructAndInsertUrmaBondingRollback(state, urmaList);
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().globalUrmaId.load(), 20);
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().globalFeId.load(), 10);
    EXPECT_EQ(urmaList.count("dev1"), 0);
    EXPECT_EQ(urmaList.count("dev2"), 0);
    EXPECT_EQ(urmaList.count("dev3"), 1);
    UbseUrmaControllerManager::GetInstance().globalUrmaId = 0;
    UbseUrmaControllerManager::GetInstance().globalFeId = 0;
}

TEST_F(TestUbseUrmaControllerManager, DeleteOtherNodesUrmaInfo_NotClos)
{
    ClearNodeInfosForTest();
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].nodeId = "0";
    UbseUrmaControllerManager::GetInstance().nodeInfos["1"].nodeId = "1";
    MOCKER_CPP(&UbseSmbios::GetMeshType).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    UbseUrmaControllerManager::GetInstance().DeleteOtherNodesUrmaInfo("0");
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().nodeInfos.size(), 2);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, DeleteOtherNodesUrmaInfo_GetMeshTypeFails)
{
    ClearNodeInfosForTest();
    MOCKER_CPP(&UbseSmbios::GetMeshType).stubs().will(returnValue(UBSE_ERROR));
    UbseUrmaControllerManager::GetInstance().DeleteOtherNodesUrmaInfo("0");
}

TEST_F(TestUbseUrmaControllerManager, GetHostUrmaDev_Misc)
{
    UbseUrmaUvsNodeInfo uvsInfo{.nodeId = "1"};
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseNodeController::IsHostBondingRegistered).stubs().will(returnValue(false));
    EXPECT_EQ(GetHostUrmaDev("1", uvsInfo), UBSE_OK);
    GlobalMockObject::verify();

    UbseUrmaUvsNodeInfo nodeInfo{.nodeId = "1"};
    std::vector<UbseUrmaUvsNodeInfo> mockHostUrmaInfos{nodeInfo};
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseNodeController::IsHostBondingRegistered).stubs().will(returnValue(true));
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(_, outBound(mockHostUrmaInfos))
        .will(returnValue(UBSE_OK));
    uvsInfo.nodeId = "1";
    EXPECT_EQ(GetHostUrmaDev("1", uvsInfo), UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, InsertHostUrmaDevInner_CurNodeEmpty)
{
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    UbseUrmaControllerManager::GetInstance().SetFeTopoType(FeTopoType::ALL_PFE);
    MOCKER_CPP(&UbseNodeController::IsHostBondingRegistered).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(UbseNodeInfo{}));
    auto ret = UbseUrmaControllerManager::GetInstance().InsertHostUrmaDevInner();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, InsertHostUrmaDevInner_CurNodeInfoNoUrmaDev)
{
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    UbseUrmaControllerManager::GetInstance().SetFeTopoType(FeTopoType::ALL_PFE);
    MOCKER_CPP(&UbseNodeController::IsHostBondingRegistered).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    auto ret = UbseUrmaControllerManager::GetInstance().InsertHostUrmaDevInner();
    EXPECT_EQ(ret, UBSE_ERROR);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, FilterFeInfosForNonClos_Success)
{
    std::vector<std::vector<UbseMtiFeInfo>> feInfos;
    std::vector<std::vector<UbseMtiFeInfo>> containerFeInfos;
    std::vector<std::vector<UbseMtiFeInfo>> hostFeInfos;

    UbseUrmaUvsNodeInfo hostUvsInfo;
    hostUvsInfo.nodeId = "1";
    UbseUrmaUvsAggrDev dev;
    UbseUrmaUvsFe fe0;
    fe0.ubpuId = "0";
    fe0.entityId = "entity0";
    fe0.primaryEid = "pri_eid_0";
    UbseUrmaUvsFe fe1;
    fe1.ubpuId = "1";
    fe1.entityId = "entity1";
    fe1.primaryEid = "pri_eid_1";
    dev.feList.push_back(fe0);
    dev.feList.push_back(fe1);
    hostUvsInfo.devList.push_back(dev);
    std::vector<UbseUrmaUvsNodeInfo> hostUrmaInfos{hostUvsInfo};

    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(_, outBound(hostUrmaInfos))
        .will(returnValue(UBSE_OK));

    auto ret = FilterFeInfosForNonClos("1", feInfos, containerFeInfos, hostFeInfos);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerManager, FilterFeInfosForNonClos_NodeNotFound)
{
    std::vector<std::vector<UbseMtiFeInfo>> feInfos;
    std::vector<std::vector<UbseMtiFeInfo>> containerFeInfos;
    std::vector<std::vector<UbseMtiFeInfo>> hostFeInfos;

    UbseUrmaUvsNodeInfo hostUvsInfo;
    hostUvsInfo.nodeId = "other";
    UbseUrmaUvsAggrDev dev;
    UbseUrmaUvsFe fe0;
    fe0.primaryEid = "pri_eid";
    dev.feList.push_back(fe0);
    dev.feList.push_back(fe0);
    hostUvsInfo.devList.push_back(dev);
    std::vector<UbseUrmaUvsNodeInfo> hostUrmaInfos{hostUvsInfo};

    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(_, outBound(hostUrmaInfos))
        .will(returnValue(UBSE_OK));

    auto ret = FilterFeInfosForNonClos("1", feInfos, containerFeInfos, hostFeInfos);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUrmaControllerManager, FilterFeInfosForNonClos_GetPlanningFails)
{
    std::vector<std::vector<UbseMtiFeInfo>> feInfos;
    std::vector<std::vector<UbseMtiFeInfo>> containerFeInfos;
    std::vector<std::vector<UbseMtiFeInfo>> hostFeInfos;

    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId).stubs().will(returnValue(UBSE_ERROR));

    auto ret = FilterFeInfosForNonClos("1", feInfos, containerFeInfos, hostFeInfos);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerManager, FilterFeInfosForNonClos_EmptyHostUrmaInfos)
{
    std::vector<std::vector<UbseMtiFeInfo>> feInfos;
    std::vector<std::vector<UbseMtiFeInfo>> containerFeInfos;
    std::vector<std::vector<UbseMtiFeInfo>> hostFeInfos;

    std::vector<UbseUrmaUvsNodeInfo> hostUrmaInfos;
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(_, outBound(hostUrmaInfos))
        .will(returnValue(UBSE_OK));

    auto ret = FilterFeInfosForNonClos("1", feInfos, containerFeInfos, hostFeInfos);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUrmaControllerManager, GenerateUrmaDevName_NonClos)
{
    ClearNodeInfosForTest();
    UbseUrmaControllerManager::GetInstance().globalUrmaId = 0;
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));

    UbseMtiFeInfo fe0;
    fe0.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    UbseMtiFeInfo fe1;
    fe1.fetype = UbseMtiFeType::PHYSICAL_TYPE;

    auto name = UbseUrmaControllerManager::GetInstance().GenerateUrmaDevName("0", fe0, fe1, 0);
    EXPECT_EQ(name, "bonding_dev_1");
    UbseUrmaControllerManager::GetInstance().globalUrmaId = 0;
}

TEST_F(TestUbseUrmaControllerManager, FilterFeInfosForNonClos_WithDataFiltering)
{
    std::vector<std::vector<UbseMtiFeInfo>> feInfos(1);
    UbseMtiFeInfo lcneFe;
    lcneFe.slotId = "0";
    lcneFe.ubpuId = "0";
    lcneFe.iouId = "0";
    lcneFe.entityId = "entity0";
    lcneFe.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    UbseMtiEidGroup group;
    group.primaryEid = "keep_eid";
    lcneFe.eidGroups.push_back(group);
    feInfos[0].push_back(lcneFe);

    std::vector<std::vector<UbseMtiFeInfo>> containerFeInfos;
    std::vector<std::vector<UbseMtiFeInfo>> hostFeInfos;

    UbseUrmaUvsNodeInfo hostUvsInfo;
    hostUvsInfo.nodeId = "1";
    UbseUrmaUvsAggrDev dev;
    UbseUrmaUvsFe fe0;
    fe0.ubpuId = "0";
    fe0.entityId = "entity0";
    fe0.primaryEid = "comm_eid";
    UbseUrmaUvsFe fe1;
    fe1.ubpuId = "0";
    fe1.entityId = "entity0";
    fe1.primaryEid = "comm_eid2";
    dev.feList.push_back(fe0);
    dev.feList.push_back(fe1);
    hostUvsInfo.devList.push_back(dev);
    std::vector<UbseUrmaUvsNodeInfo> hostUrmaInfos{hostUvsInfo};

    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(_, outBound(hostUrmaInfos))
        .will(returnValue(UBSE_OK));

    auto ret = FilterFeInfosForNonClos("1", feInfos, containerFeInfos, hostFeInfos);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerManager, ConstructAndInsertUrmaBonding_NullFeInfo)
{
    ClearNodeInfosForTest();
    std::map<std::string, UbseUrmaInfo, UrmaNameCompare> urmaList;

    UbseMtiFeInfo lcneFe0;
    lcneFe0.slotId = "0";
    lcneFe0.ubpuId = "0";
    lcneFe0.iouId = "0";
    lcneFe0.entityId = "0";
    lcneFe0.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    UbseMtiEidGroup group;
    group.primaryEid = "eid";
    lcneFe0.eidGroups.push_back(group);

    UbseMtiFeInfo lcneFe1;
    lcneFe1.slotId = "0";
    lcneFe1.ubpuId = "0";
    lcneFe1.iouId = "0";
    lcneFe1.entityId = "0";
    lcneFe1.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    lcneFe1.eidGroups.push_back(group);

    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId).stubs().will(returnValue(UBSE_ERROR));

    auto ret =
        UbseUrmaControllerManager::GetInstance().ConstructAndInsertUrmaBonding(0, "0", lcneFe0, lcneFe1, urmaList);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, BuildUvsTopoNodeInfo_CurNodeEmpty)
{
    ClearNodeInfosForTest();
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(UbseNodeInfo{}));
    std::vector<UbseUrmaUvsNodeInfo> uvsInfos;
    auto ret = UbseUrmaControllerManager::GetInstance().BuildUvsTopoNodeInfo(false, 0, 1, uvsInfos);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, CheckAndStoreUrmaBonding_ValidateFails)
{
    std::vector<std::vector<UbseMtiFeInfo>> feInfos(1);
    std::vector<std::vector<UbseMtiFeInfo>> containerFeInfos;
    std::vector<std::vector<UbseMtiFeInfo>> hostFeInfos;
    auto ret = CheckAndStoreUrmaBonding("0", feInfos, containerFeInfos, hostFeInfos);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerManager, ParseFeIdsFromEid_MultipleEdgeCases)
{
    std::pair<uint16_t, uint16_t> feIds;
    EXPECT_NE(ParseFeIdsFromEid("", feIds), UBSE_OK);
    EXPECT_NE(ParseFeIdsFromEid("short", feIds), UBSE_OK);
    std::string wrongDelim = "0000-0000-0001-0002-0003-0004-0005-0006";
    EXPECT_NE(ParseFeIdsFromEid(wrongDelim, feIds), UBSE_OK);
}

TEST_F(TestUbseUrmaControllerManager, ConstructAndInsertUrmaBonding_MismatchedFeTypes)
{
    ClearNodeInfosForTest();
    std::map<std::string, UbseUrmaInfo, UrmaNameCompare> urmaList;

    UbseMtiFeInfo lcneFe0;
    lcneFe0.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    UbseMtiFeInfo lcneFe1;
    lcneFe1.fetype = UbseMtiFeType::VIRTUAL_TYPE;

    auto ret =
        UbseUrmaControllerManager::GetInstance().ConstructAndInsertUrmaBonding(0, "0", lcneFe0, lcneFe1, urmaList);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, ConstructAndInsertUrmaBonding_EmptyEidGroups)
{
    ClearNodeInfosForTest();
    std::map<std::string, UbseUrmaInfo, UrmaNameCompare> urmaList;

    UbseMtiFeInfo lcneFe0;
    lcneFe0.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    lcneFe0.slotId = "0";
    lcneFe0.ubpuId = "0";
    lcneFe0.iouId = "0";
    lcneFe0.entityId = "0";

    UbseMtiFeInfo lcneFe1;
    lcneFe1.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    lcneFe1.slotId = "0";
    lcneFe1.ubpuId = "0";
    lcneFe1.iouId = "0";
    lcneFe1.entityId = "0";

    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId).stubs().will(returnValue(UBSE_ERROR));

    auto ret =
        UbseUrmaControllerManager::GetInstance().ConstructAndInsertUrmaBonding(0, "0", lcneFe0, lcneFe1, urmaList);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, ConstructAndInsertUrmaBonding_SuccessSinglePair)
{
    ClearNodeInfosForTest();
    UbseUrmaControllerManager::GetInstance().globalUrmaId = 0;
    UbseUrmaControllerManager::GetInstance().globalFeId = 0;
    std::map<std::string, UbseUrmaInfo, UrmaNameCompare> urmaList;

    UbseMtiFeInfo lcneFe0;
    lcneFe0.slotId = "0";
    lcneFe0.ubpuId = "0";
    lcneFe0.iouId = "0";
    lcneFe0.entityId = "0";
    lcneFe0.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    UbseMtiEidGroup group0;
    group0.primaryEid = "eid_0";
    group0.portEids["port0"] = "val0";
    lcneFe0.eidGroups.push_back(group0);

    UbseMtiFeInfo lcneFe1;
    lcneFe1.slotId = "0";
    lcneFe1.ubpuId = "0";
    lcneFe1.iouId = "0";
    lcneFe1.entityId = "0";
    lcneFe1.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    UbseMtiEidGroup group1;
    group1.primaryEid = "eid_1";
    group1.portEids["port1"] = "val1";
    lcneFe1.eidGroups.push_back(group1);

    UbseUrmaUvsNodeInfo hostUvsInfo;
    hostUvsInfo.nodeId = "0";
    UbseUrmaUvsAggrDev dev;
    UbseUrmaUvsFe fe0;
    fe0.ubpuId = "0";
    fe0.entityId = "0";
    fe0.primaryEid = "eid_0";
    dev.feList.push_back(fe0);
    dev.feList.push_back(fe0);
    hostUvsInfo.devList.push_back(dev);
    std::vector<UbseUrmaUvsNodeInfo> hostUrmaInfos{hostUvsInfo};

    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(_, outBound(hostUrmaInfos))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));

    auto ret =
        UbseUrmaControllerManager::GetInstance().ConstructAndInsertUrmaBonding(0, "0", lcneFe0, lcneFe1, urmaList);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_GE(urmaList.size(), 1);
    UbseUrmaControllerManager::GetInstance().globalUrmaId = 0;
    UbseUrmaControllerManager::GetInstance().globalFeId = 0;
}

TEST_F(TestUbseUrmaControllerManager, InferOtherNodesUrmaDevInfo_NotClos)
{
    ClearNodeInfosForTest();
    MOCKER_CPP(&UbseSmbios::GetMeshType).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseSmbios::GetServerIdx).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    auto ret = UbseUrmaControllerManager::GetInstance().InferOtherNodesUrmaDevInfo(false, "0", 0, 1);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerManager, ConstructNewUrmaInfo_NonClosPath)
{
    ClearNodeInfosForTest();
    UbseUrmaControllerManager::GetInstance().globalUrmaId = 0;
    UbseUrmaControllerManager::GetInstance().globalFeId = 0;

    std::vector<std::vector<UbseMtiFeInfo>> feInfos(2);
    UbseMtiFeInfo fe0;
    fe0.slotId = "0";
    fe0.ubpuId = "0";
    fe0.iouId = "0";
    fe0.entityId = "0";
    fe0.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    UbseMtiEidGroup g0;
    g0.primaryEid = "eid0";
    fe0.eidGroups.push_back(g0);
    feInfos[0].push_back(fe0);

    UbseMtiFeInfo fe1;
    fe1.slotId = "0";
    fe1.ubpuId = "1";
    fe1.iouId = "1";
    fe1.entityId = "0";
    fe1.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    UbseMtiEidGroup g1;
    g1.primaryEid = "eid1";
    fe1.eidGroups.push_back(g1);
    feInfos[1].push_back(fe1);

    UbseUrmaUvsNodeInfo hostUvs;
    hostUvs.nodeId = "0";
    UbseUrmaUvsAggrDev dev;
    UbseUrmaUvsFe uvsFe;
    uvsFe.ubpuId = "0";
    uvsFe.entityId = "0";
    uvsFe.primaryEid = "eid0";
    dev.feList.push_back(uvsFe);
    dev.feList.push_back(uvsFe);
    hostUvs.devList.push_back(dev);
    std::vector<UbseUrmaUvsNodeInfo> hostUrmaInfos{hostUvs};

    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseSmbios::GetServerIdx).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(_, outBound(hostUrmaInfos))
        .will(returnValue(UBSE_OK));

    auto ret = UbseUrmaControllerManager::GetInstance().ConstructNewUrmaInfo("0", feInfos);
    EXPECT_EQ(ret, UBSE_OK);
    UbseUrmaControllerManager::GetInstance().globalUrmaId = 0;
    UbseUrmaControllerManager::GetInstance().globalFeId = 0;
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, ConstructNewUrmaInfo_ValidateFeInfosFails)
{
    ClearNodeInfosForTest();
    std::vector<std::vector<UbseMtiFeInfo>> feInfos(1);
    auto ret = UbseUrmaControllerManager::GetInstance().ConstructNewUrmaInfo("0", feInfos);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerManager, DeleteOtherNodesUrmaInfo_ClosPath)
{
    ClearNodeInfosForTest();
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].nodeId = "0";
    UbseUrmaControllerManager::GetInstance().nodeInfos["1"].nodeId = "1";
    UbseUrmaControllerManager::GetInstance().nodeInfos["2"].nodeId = "2";

    UbseMeshType meshType = UbseMeshType::CLOS;
    MOCKER_CPP(&UbseSmbios::GetMeshType).stubs().with(outBound(meshType)).will(returnValue(UBSE_OK));

    UbseUrmaControllerManager::GetInstance().DeleteOtherNodesUrmaInfo("0");
    EXPECT_EQ(UbseUrmaControllerManager::GetInstance().nodeInfos.size(), 1);
    EXPECT_NE(UbseUrmaControllerManager::GetInstance().nodeInfos.find("0"),
              UbseUrmaControllerManager::GetInstance().nodeInfos.end());
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, ProcessUrmaBondings_SinglePair)
{
    ClearNodeInfosForTest();
    UbseUrmaControllerManager::GetInstance().globalUrmaId = 0;
    UbseUrmaControllerManager::GetInstance().globalFeId = 0;

    std::vector<std::vector<UbseMtiFeInfo>> feInfos(2);
    UbseMtiFeInfo fe0;
    fe0.slotId = "0";
    fe0.ubpuId = "0";
    fe0.iouId = "0";
    fe0.entityId = "0";
    fe0.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    UbseMtiEidGroup g0;
    g0.primaryEid = "eid0";
    fe0.eidGroups.push_back(g0);
    feInfos[0].push_back(fe0);

    UbseMtiFeInfo fe1;
    fe1.slotId = "0";
    fe1.ubpuId = "1";
    fe1.iouId = "1";
    fe1.entityId = "0";
    fe1.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    UbseMtiEidGroup g1;
    g1.primaryEid = "eid1";
    fe1.eidGroups.push_back(g1);
    feInfos[1].push_back(fe1);

    UbseUrmaUvsNodeInfo hostUvs;
    hostUvs.nodeId = "0";
    UbseUrmaUvsAggrDev dev;
    UbseUrmaUvsFe uvsFe;
    uvsFe.ubpuId = "0";
    uvsFe.entityId = "0";
    uvsFe.primaryEid = "eid0";
    dev.feList.push_back(uvsFe);
    dev.feList.push_back(uvsFe);
    hostUvs.devList.push_back(dev);
    std::vector<UbseUrmaUvsNodeInfo> hostUrmaInfos{hostUvs};

    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(_, outBound(hostUrmaInfos))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));

    std::map<std::string, UbseUrmaInfo, UrmaNameCompare> urmaList;
    auto ret = UbseUrmaControllerManager::GetInstance().ProcessUrmaBondings("0", 0, feInfos, urmaList);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_GE(urmaList.size(), 1);
    UbseUrmaControllerManager::GetInstance().globalUrmaId = 0;
    UbseUrmaControllerManager::GetInstance().globalFeId = 0;
}

TEST_F(TestUbseUrmaControllerManager, ConstructAndInsertUrmaBonding_GetPlanningFails)
{
    ClearNodeInfosForTest();
    std::map<std::string, UbseUrmaInfo, UrmaNameCompare> urmaList;
    UbseMtiFeInfo fe0;
    fe0.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    UbseMtiFeInfo fe1;
    fe1.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaControllerManager::GetInstance().ConstructAndInsertUrmaBonding(0, "0", fe0, fe1, urmaList);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, ConstructNewUrmaInfo_RvalueOverload)
{
    ClearNodeInfosForTest();
    UbseUrmaControllerManager::GetInstance().globalUrmaId = 0;
    UbseUrmaControllerManager::GetInstance().globalFeId = 0;

    std::vector<std::vector<UbseMtiFeInfo>> feInfos(2);
    UbseMtiFeInfo fe0;
    fe0.slotId = "0";
    fe0.ubpuId = "0";
    fe0.iouId = "0";
    fe0.entityId = "0";
    fe0.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    UbseMtiEidGroup g0;
    g0.primaryEid = "eid0";
    fe0.eidGroups.push_back(g0);
    feInfos[0].push_back(fe0);
    UbseMtiFeInfo fe1;
    fe1.slotId = "0";
    fe1.ubpuId = "1";
    fe1.iouId = "1";
    fe1.entityId = "0";
    fe1.fetype = UbseMtiFeType::PHYSICAL_TYPE;
    UbseMtiEidGroup g1;
    g1.primaryEid = "eid1";
    fe1.eidGroups.push_back(g1);
    feInfos[1].push_back(fe1);

    UbseUrmaUvsNodeInfo hostUvs;
    hostUvs.nodeId = "0";
    UbseUrmaUvsAggrDev dev;
    UbseUrmaUvsFe uvsFe;
    uvsFe.ubpuId = "0";
    uvsFe.entityId = "0";
    uvsFe.primaryEid = "eid0";
    dev.feList.push_back(uvsFe);
    dev.feList.push_back(uvsFe);
    hostUvs.devList.push_back(dev);
    std::vector<UbseUrmaUvsNodeInfo> hostInfos{hostUvs};

    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseSmbios::GetServerIdx).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(_, outBound(hostInfos))
        .will(returnValue(UBSE_OK));

    auto ret = UbseUrmaControllerManager::GetInstance().ConstructNewUrmaInfo("0", std::move(feInfos));
    EXPECT_EQ(ret, UBSE_OK);
    UbseUrmaControllerManager::GetInstance().globalUrmaId = 0;
    UbseUrmaControllerManager::GetInstance().globalFeId = 0;
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, InferOtherNodesUrmaDevInfo_NoBaseNode)
{
    ClearNodeInfosForTest();
    UbseMeshType meshType = UbseMeshType::CLOS;
    MOCKER_CPP(&UbseSmbios::GetMeshType).stubs().with(outBound(meshType)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseSmbios::GetServerIdx).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseUrmaControllerManager::GetInstance().InferOtherNodesUrmaDevInfo(false, "nonexistent", 0, 1);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerManager, InferOtherNodesUrmaDevInfo_GetMeshFails)
{
    ClearNodeInfosForTest();
    MOCKER_CPP(&UbseSmbios::GetMeshType).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseUrmaControllerManager::GetInstance().InferOtherNodesUrmaDevInfo(false, "0", 0, 1);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, BuildUvsTopoNodeInfo_HostOnly)
{
    ClearNodeInfosForTest();
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    UbseUrmaInfo urmaInfo;
    urmaInfo.urmaDevEid = "eid";
    urmaInfo.eidGroups.resize(1);
    urmaInfo.eidGroups[0].feInfo = std::make_shared<UbseFeInfo>();
    urmaInfo.eidGroups[0].feInfo->ubpuId = "0";
    urmaInfo.eidGroups[0].feInfo->entityId = "0";
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].urmaList["dev0"] = urmaInfo;
    UbseUrmaControllerManager::GetInstance().nodeInfos["0"].nodeId = "0";

    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));

    std::vector<UbseUrmaUvsNodeInfo> uvsInfos;
    auto ret = UbseUrmaControllerManager::GetInstance().BuildUvsTopoNodeInfo(true, 0, 1, uvsInfos);
    EXPECT_EQ(ret, UBSE_OK);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, InsertHostUrmaDevInner_CurNodeNoUrmaData)
{
    UbseNodeInfo curNode;
    curNode.nodeId = "0";
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    UbseUrmaControllerManager::GetInstance().SetFeTopoType(FeTopoType::ALL_PFE);
    MOCKER_CPP(&UbseNodeController::IsHostBondingRegistered).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(curNode));
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
    auto ret = UbseUrmaControllerManager::GetInstance().InsertHostUrmaDevInner();
    EXPECT_EQ(ret, UBSE_ERROR);
    UbseUrmaControllerManager::GetInstance().nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, UbseEidGroupCmp_BasicComparison)
{
    struct UbseEidGroupCmp {
        bool operator()(const UbseMtiEidGroup& left, const UbseMtiEidGroup& right) const
        {
            return left.primaryEid < right.primaryEid;
        }
    };
    UbseEidGroupCmp cmp;
    UbseMtiEidGroup left, right;
    left.primaryEid = "a";
    right.primaryEid = "b";
    EXPECT_TRUE(cmp(left, right));
    EXPECT_FALSE(cmp(right, left));
}

TEST_F(TestUbseUrmaControllerManager, InferEidGroup_NullFeInfo)
{
    urma::EidGroup group;
    group.feInfo = nullptr;
    auto ret = urmaController::InferEidGroup(0, group);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestUbseUrmaControllerManager, InferOneNodeUrmaDevInfo_EmptyLists)
{
    ClearNodeInfosForTest();
    auto& mgr = UbseUrmaControllerManager::GetInstance();
    UbseUrmaNodeInfo nodeInfo;
    nodeInfo.nodeId = "0";
    mgr.nodeInfos["0"] = nodeInfo; // empty urmaList and hostUrmaList

    auto ret = mgr.InferOneNodeUrmaDevInfo(false, 0, "0");
    EXPECT_EQ(ret, UBSE_OK);
    mgr.nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, SplitFeInfosForClos_GetPlanningHostBondingFails)
{
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId).stubs().will(returnValue(UBSE_ERROR));
    std::vector<std::vector<UbseMtiFeInfo>> feInfos;
    std::vector<std::vector<UbseMtiFeInfo>> containerFeInfos;
    std::vector<std::vector<UbseMtiFeInfo>> hostFeInfos;
    auto ret = urmaController::SplitFeInfosForClos("0", feInfos, containerFeInfos, hostFeInfos);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseUrmaControllerManager, SplitFeInfosForClos_EmptyFeInfos)
{
    std::vector<UbseUrmaUvsNodeInfo> hostUrmaInfos;
    UbseUrmaUvsNodeInfo hostInfo;
    hostInfo.nodeId = "0";
    hostUrmaInfos.push_back(hostInfo);
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId)
        .stubs()
        .with(_, outBound(hostUrmaInfos))
        .will(returnValue(UBSE_OK));
    std::vector<std::vector<UbseMtiFeInfo>> feInfos;
    std::vector<std::vector<UbseMtiFeInfo>> containerFeInfos;
    std::vector<std::vector<UbseMtiFeInfo>> hostFeInfos;
    auto ret = urmaController::SplitFeInfosForClos("0", feInfos, containerFeInfos, hostFeInfos);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerManager, InferUrmaListDevInfo_EmptyList)
{
    std::map<std::string, UbseUrmaInfo, urma::UrmaNameCompare> urmaList;
    auto ret = urmaController::InferUrmaListDevInfo(0, 1, urmaList);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseUrmaControllerManager, InferOtherNodesUrmaDevInfo_LoopSkipSelf)
{
    ClearNodeInfosForTest();
    auto& mgr = UbseUrmaControllerManager::GetInstance();
    UbseUrmaNodeInfo nodeInfo;
    nodeInfo.nodeId = "0";
    mgr.nodeInfos["0"] = nodeInfo; // base node exists

    UbseMeshType meshType = UbseMeshType::CLOS;
    uint32_t curServerIdx = 0; // serverIdx starts at 0, equals curServerIdx, loop skips
    MOCKER_CPP(&UbseSmbios::GetMeshType).stubs().with(outBound(meshType)).will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseSmbios::GetServerIdx).stubs().with(outBound(curServerIdx)).will(returnValue(UBSE_OK));

    auto ret = mgr.InferOtherNodesUrmaDevInfo(false, "0", 0, 1);
    EXPECT_EQ(ret, UBSE_OK);
    mgr.nodeInfos = {};
}

TEST_F(TestUbseUrmaControllerManager, FilterFeInfosForNonClos_IsClosType)
{
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(true));
    std::vector<std::vector<UbseMtiFeInfo>> feInfos;
    std::vector<std::vector<UbseMtiFeInfo>> containerFeInfos;
    std::vector<std::vector<UbseMtiFeInfo>> hostFeInfos;
    auto ret = urmaController::FilterFeInfosForNonClos("0", feInfos, containerFeInfos, hostFeInfos);
    EXPECT_EQ(ret, UBSE_ERR_NOT_SUPPORTED);
}

TEST_F(TestUbseUrmaControllerManager, FilterFeInfosForNonClos_PlanHostBondingFails)
{
    MOCKER_CPP(&UbseSmbios::IsClosType).stubs().will(returnValue(false));
    MOCKER_CPP(&UbseNodeController::GetPlanningHostBondingByNodeId).stubs().will(returnValue(UBSE_ERROR));
    std::vector<std::vector<UbseMtiFeInfo>> feInfos;
    std::vector<std::vector<UbseMtiFeInfo>> containerFeInfos;
    std::vector<std::vector<UbseMtiFeInfo>> hostFeInfos;
    auto ret = urmaController::FilterFeInfosForNonClos("0", feInfos, containerFeInfos, hostFeInfos);
    EXPECT_EQ(ret, UBSE_ERROR);
}

} // namespace ubse::urmaControllerManager::ut
