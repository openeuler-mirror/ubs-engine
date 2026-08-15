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

#include "test_ubse_node_com_urma_collector.h"
#include "ubse_error.h"
#include "ubse_mti_eid_interface.h"
#include "ubse_mti_interface.h"
#include "ubse_node_mgr.h"
namespace ubse::node_controller::ut {
using namespace ubse::adapter_plugins::mti;
using namespace ubse::urma;
using namespace ubse::utils;
using namespace ubse::nodeMgr;

void TestUbseNodeComUrmaCollector::SetUp()
{
    Test::SetUp();
}

void TestUbseNodeComUrmaCollector::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseNodeComUrmaCollector, GetAllHostPlanningBondings)
{
    // 构造两个节点的静态信息：node "1" 有 2 个 FE，node "2" 有 1 个 FE
    std::vector<UbseNodeStaticInfo> clusterNodes;
    UbseNodeStaticInfo node1{};
    node1.nodeId = "1";
    node1.bonding0Eid = "devEid1";
    node1.feEidList["1"] = UbseMtiEidGroup{"entityId1", "primaryEid1", {{"port1", "eid1"}}};
    node1.feEidList["2"] = UbseMtiEidGroup{"entityId2", "primaryEid2", {{"port2", "eid2"}}};
    UbseNodeStaticInfo node2{};
    node2.nodeId = "2";
    node2.bonding0Eid = "devEid2";
    node2.feEidList["3"] = UbseMtiEidGroup{"entityId3", "primaryEid3", {{"port3", "eid3"}}};
    clusterNodes = {node1, node2};

    MOCKER_CPP(ubse::nodeMgr::GetAllNodes).stubs().will(returnValue(clusterNodes));

    UbseNodeComUrmaCollector collector;
    std::vector<UbseUrmaUvsNodeInfo> hostUrmaInfos;
    auto ret = collector.GetAllHostPlanningBondings(hostUrmaInfos);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(hostUrmaInfos.size(), 2U);

    // 校验 node "1"：urmaDevEid 与 feList 数量
    EXPECT_EQ(hostUrmaInfos[0].nodeId, "1");
    EXPECT_EQ(hostUrmaInfos[0].devList.size(), 1U);
    EXPECT_EQ(hostUrmaInfos[0].devList[0].urmaDevEid, "devEid1");
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList.size(), 2U);
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList[0].ubpuId, "1");
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList[0].entityId, "entityId1");
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList[0].primaryEid, "primaryEid1");
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList[0].portEid["port1"], "eid1");
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList[1].ubpuId, "2");
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList[1].entityId, "entityId2");
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList[1].primaryEid, "primaryEid2");
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList[1].portEid["port2"], "eid2");

    // 校验 node "2"：urmaDevEid 与 feList 数量
    EXPECT_EQ(hostUrmaInfos[1].nodeId, "2");
    EXPECT_EQ(hostUrmaInfos[1].devList.size(), 1U);
    EXPECT_EQ(hostUrmaInfos[1].devList[0].urmaDevEid, "devEid2");
    EXPECT_EQ(hostUrmaInfos[1].devList[0].feList.size(), 1U);
    EXPECT_EQ(hostUrmaInfos[1].devList[0].feList[0].ubpuId, "3");
    EXPECT_EQ(hostUrmaInfos[1].devList[0].feList[0].entityId, "entityId3");
    EXPECT_EQ(hostUrmaInfos[1].devList[0].feList[0].primaryEid, "primaryEid3");
    EXPECT_EQ(hostUrmaInfos[1].devList[0].feList[0].portEid["port3"], "eid3");
}

TEST_F(TestUbseNodeComUrmaCollector, SetComUrmaInfo)
{
    UbseNodeComUrmaCollector collector;
    std::vector<PhysicalLink> physicalLinks;
    std::string curNodeId = "1";
    UbseUrmaUvsFe srcFe1{"1", "entityId1", "primaryEid1", {{"port1", "eid1"}}};
    UbseUrmaUvsFe srcFe2{"2", "entityId2", "primaryEid2", {{"port2", "eid2"}}};
    collector.comUrmaInfos[curNodeId].feList = {srcFe1, srcFe2};
    collector.comUrmaInfos[curNodeId].urmaDevEid = utils::GenerateUrmaDevEid(0, 1, 0, 0);
    UbseNodeInfo nodeInfo{};
    nodeInfo.nodeId = curNodeId;
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(nodeInfo));
    MOCKER_CPP(&UbseNodeController::IsHostBondingRegistered).stubs().will(returnValue(true));
    MOCKER_CPP(&UbsePushTopoAndBondingToUvs).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&UbseActiveBonding).stubs().will(returnValue(UBSE_OK));
    auto ret = collector.SetComUrma(physicalLinks, true);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeComUrmaCollector, SetComUrmaInfo_CurNodeEmpty)
{
    UbseNodeComUrmaCollector collector;
    std::vector<PhysicalLink> physicalLinks;
    std::string curNodeId = "1";
    collector.comUrmaInfos[curNodeId].urmaDevEid = utils::GenerateUrmaDevEid(0, 1, 0, 0);
    UbseNodeInfo nodeInfo{};
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(nodeInfo));
    auto ret = collector.SetComUrma(physicalLinks, true);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeComUrmaCollector, SetComUrmaInfo_HostBondingNotRegist)
{
    UbseNodeComUrmaCollector collector;
    std::vector<PhysicalLink> physicalLinks;
    std::string curNodeId = "1";
    collector.comUrmaInfos[curNodeId].urmaDevEid = utils::GenerateUrmaDevEid(0, 1, 0, 0);
    UbseNodeInfo nodeInfo{};
    nodeInfo.nodeId = curNodeId;
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(nodeInfo));
    MOCKER_CPP(&UbseNodeController::IsHostBondingRegistered).stubs().will(returnValue(false));
    auto ret = collector.SetComUrma(physicalLinks, true);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeComUrmaCollector, SetComUrmaInfo_PushTopoFailed)
{
    UbseNodeComUrmaCollector collector;
    std::vector<PhysicalLink> physicalLinks;
    std::string curNodeId = "1";
    collector.comUrmaInfos[curNodeId].urmaDevEid = utils::GenerateUrmaDevEid(0, 1, 0, 0);
    UbseNodeInfo nodeInfo{};
    nodeInfo.nodeId = curNodeId;
    MOCKER_CPP(&UbseNodeController::GetCurNode).stubs().will(returnValue(nodeInfo));
    MOCKER_CPP(&UbseNodeController::IsHostBondingRegistered).stubs().will(returnValue(true));
    MOCKER_CPP(&UbsePushTopoAndBondingToUvs).stubs().will(returnValue(UBSE_ERROR));
    auto ret = collector.SetComUrma(physicalLinks, true);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeComUrmaCollector, FillComUrmaInfoClos_Failed)
{
    adapter_plugins::mti::UbseMtiNodeInfo nodeInfo{"1", "eid1"};
    adapter_plugins::mti::UbseMtiInterface& mtiInterface = adapter_plugins::mti::UbseMtiInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(mtiInterface, &adapter_plugins::mti::UbseMtiInterface::GetLocalNodeInfo)
        .stubs()
        .with(outBound(nodeInfo))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP_VIRTUAL(mtiInterface, &adapter_plugins::mti::UbseMtiInterface::GetMtiComEid)
        .stubs()
        .will(returnValue(UBSE_ERROR));

    UbseNodeComUrmaCollector collector;
    auto ret = collector.FillComUrmaInfoClos();
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseNodeComUrmaCollector, ProcessClusterNode)
{
    UbseNodeComUrmaCollector collector;
    std::string curNodeId = "1";
    UbseUrmaUvsFe srcFe1{"1", "entityId1", "primaryEid1", {{"port1", "eid1"}}};
    UbseUrmaUvsFe srcFe2{"2", "entityId2", "primaryEid2", {{"port2", "eid2"}}};
    collector.comUrmaInfos[curNodeId].feList = {srcFe1, srcFe2};
    collector.comUrmaInfos[curNodeId].urmaDevEid = utils::GenerateUrmaDevEid(0, 1, 0, 0);
    uint32_t serverIdx = 0;

    // 测试当前节点跳过处理场景
    auto ret = collector.ProcessClusterNode(curNodeId, serverIdx);
    EXPECT_EQ(ret, UBSE_OK);

    // 测试ProcessFeDevice失败场景
    MOCKER_CPP(&OverwriteEid).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    serverIdx = 1; // targetNodeId = 2 (1+1)
    ret = collector.ProcessClusterNode(curNodeId, serverIdx);
    EXPECT_EQ(ret, UBSE_ERROR);

    // 测试正常处理流程
    serverIdx = 1;
    ret = collector.ProcessClusterNode(curNodeId, serverIdx);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(collector.comUrmaInfos.size(), 2);
    EXPECT_EQ(collector.comUrmaInfos["2"].urmaDevEid, utils::GenerateUrmaDevEid(0, 2, 0, 0));
    EXPECT_EQ(collector.comUrmaInfos["2"].feList.size(), 2);
}

TEST_F(TestUbseNodeComUrmaCollector, GetPlanningHostBondingByNodeId)
{
    UbseNodeComUrmaCollector collector;
    std::string curNodeId = "1";
    std::vector<UbseUrmaUvsNodeInfo> hostUrmaInfos;
    UbseUrmaUvsFe srcFe1{"1", "entityId1", "primaryEid1", {{"port1", "eid1"}}};
    UbseUrmaUvsFe srcFe2{"2", "entityId2", "primaryEid2", {{"port2", "eid2"}}};
    collector.comUrmaInfos[curNodeId].feList = {srcFe1, srcFe2};
    collector.comUrmaInfos[curNodeId].urmaDevEid = utils::GenerateUrmaDevEid(0, 2, 0, 0);

    auto ret = collector.GetPlanningHostBondingByNodeId(curNodeId, hostUrmaInfos);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(hostUrmaInfos.size(), 1);
    EXPECT_EQ(hostUrmaInfos[0].nodeId, "1");
    EXPECT_EQ(hostUrmaInfos[0].devList.size(), 1);
    EXPECT_EQ(hostUrmaInfos[0].devList[0].urmaDevEid, utils::GenerateUrmaDevEid(0, 2, 0, 0));
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList.size(), 2);
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList[0].primaryEid, "primaryEid1");
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList[0].entityId, "entityId1");
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList[0].portEid["port1"], "eid1");
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList[1].primaryEid, "primaryEid2");
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList[1].entityId, "entityId2");
    EXPECT_EQ(hostUrmaInfos[0].devList[0].feList[1].portEid["port2"], "eid2");

    ret = collector.GetPlanningHostBondingByNodeId("2", hostUrmaInfos);
    EXPECT_EQ(ret, UBSE_ERROR);
    EXPECT_EQ(hostUrmaInfos.size(), 0);
}

TEST_F(TestUbseNodeComUrmaCollector, GetCurNodeIouList)
{
    UbseNodeComUrmaCollector collector;
    std::vector<UbseMtiIouInfo> iouList;
    adapter_plugins::mti::UbseMtiInterface& mtiInterface = adapter_plugins::mti::UbseMtiInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(mtiInterface, &adapter_plugins::mti::UbseMtiInterface::GetCurNodeTopo)
        .stubs()
        .will(returnValue(UBSE_OK));

    auto ret = collector.GetCurNodeIouList(iouList);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseNodeComUrmaCollector, GetCurNodeIouList_Failed)
{
    UbseNodeComUrmaCollector collector;
    std::vector<UbseMtiIouInfo> iouList;
    adapter_plugins::mti::UbseMtiInterface& mtiInterface = adapter_plugins::mti::UbseMtiInterface::GetInstance();
    MOCKER_CPP_VIRTUAL(mtiInterface, &adapter_plugins::mti::UbseMtiInterface::GetCurNodeTopo)
        .stubs()
        .will(returnValue(UBSE_ERROR));

    auto ret = collector.GetCurNodeIouList(iouList);
    EXPECT_EQ(ret, UBSE_ERROR);
}
} // namespace ubse::node_controller::ut