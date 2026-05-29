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

#include "test_ubse_mem_topology_info_manager.h"
#include "ubse_error.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_mem_topology_info_manager.cpp"

namespace ubse::mem_scheduler::ut {
using namespace ubse::mem::strategy;
using namespace ubse::nodeController;
using namespace tc::rs::mem;

void TestUbseMemTopologyInfoManager::SetUp()
{
    Test::SetUp();
}
void TestUbseMemTopologyInfoManager::TearDown()
{
    Test::TearDown();
}

TEST_F(TestUbseMemTopologyInfoManager, TestFillTopoNumaInfoByNumaLoc)
{
    UbseNumaInfo numaInfo;
    UbseMemNumaLoc ubseMemNumaLoc;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 0;
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNumaInfo)
        .stubs()
        .will(returnValue(std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex)));
    ubse::nodeController::UbseAllocator allocator = ubse::nodeController::UbseAllocator::BUDDY_HIGHMEM;
    uint32_t pmd_mapping = 100;
    EXPECT_NO_THROW(
        UbseMemTopologyInfoManager::GetInstance().FillTopoNumaInfoByNumaLoc(numaInfo, allocator, pmd_mapping));
}

TEST_F(TestUbseMemTopologyInfoManager, TestNodesInit)
{
    std::vector<NodeDataWithNumaInfo> nodeDatas{};
    EXPECT_NO_THROW(UbseMemTopologyInfoManager::GetInstance().NodesInit(nodeDatas));
}

TEST_F(TestUbseMemTopologyInfoManager, TestSetAvailNumas)
{
    StrategyParam strategyParam;
    UbseMemNumaLoc ubseMemNumaLoc;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 0;
    std::vector<std::shared_ptr<MemNumaInfo>> numaList{
        std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex)};
    EXPECT_NO_THROW(UbseMemTopologyInfoManager::GetInstance().SetAvailNumas(strategyParam, numaList));
}

TEST_F(TestUbseMemTopologyInfoManager, TestSetNumaMemCapacities)
{
    StrategyParam strategyParam;
    UbseMemNumaLoc ubseMemNumaLoc;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 0;
    std::vector<std::shared_ptr<MemNumaInfo>> numaList{
        std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex)};
    EXPECT_NO_THROW(UbseMemTopologyInfoManager::GetInstance().SetNumaMemCapacities(strategyParam, numaList));
}

TEST_F(TestUbseMemTopologyInfoManager, TestSetMaxMemParam)
{
    StrategyParam strategyParam;
    UbseMemNumaLoc ubseMemNumaLoc;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 0;
    std::vector<std::shared_ptr<MemNumaInfo>> numaList{
        std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex)};
    std::string nodeId = "1";
    MOCKER_CPP(&UbseMemTopologyInfoManager::NodeIndexToId).stubs().will(returnValue(nodeId));
    EXPECT_NO_THROW(UbseMemTopologyInfoManager::GetInstance().SetMaxMemParam(strategyParam, numaList));
}

TEST_F(TestUbseMemTopologyInfoManager, TestSetMaxMemParam2)
{
    StrategyParam strategyParam;
    UbseMemNumaLoc ubseMemNumaLoc;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 0;
    std::vector<std::shared_ptr<MemNumaInfo>> numaList{
        std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex)};
    std::string nodeId = "1";
    MOCKER_CPP(&UbseMemTopologyInfoManager::NodeIndexToId).stubs().will(returnValue(nodeId));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNodePoolMemSize).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(UbseMemTopologyInfoManager::GetInstance().SetMaxMemParam(strategyParam, numaList));
}

TEST_F(TestUbseMemTopologyInfoManager, TestAllocOneNode)
{
    NodeData nodeData;
    std::string nodeId = "1";
    MOCKER_CPP(&UbseMemTopologyInfoManager::NodeIndexToId).stubs().will(returnValue(nodeId));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNodePoolMemSize).stubs().will(returnValue(UBSE_OK));
    EXPECT_NO_THROW(UbseMemTopologyInfoManager::GetInstance().AllocOneNode(nodeData));
}

TEST_F(TestUbseMemTopologyInfoManager, TestGetNodeInfoById)
{
    NodeData nodeData;
    std::string nodeId = "1";
    EXPECT_NO_THROW(UbseMemTopologyInfoManager::GetInstance().GetNodeInfoById(nodeId));
}

TEST_F(TestUbseMemTopologyInfoManager, TestGetSocketCnaInfo)
{
    const UbseMemNumaLoc memIdLocBorrow;
    const UbseMemNumaLoc memIdLocLend;
    SocketCnaTopoInfo socketCnaTopoInfo;
    UbseNodeMemCnaInfoInput cnaInfoInput{memIdLocBorrow.nodeId, memIdLocLend.nodeId,
                                         std::to_string(memIdLocLend.socketId)};
    UbseNodeMemCnaInfoOutput cnaInfoOutput{};
    MOCKER_CPP(&UbseNodeMemGetTopologyCnaInfo).stubs().with(any(), outBound(cnaInfoOutput)).will(returnValue(UBSE_OK));
    auto ret =
        UbseMemTopologyInfoManager::GetInstance().GetSocketCnaInfo(memIdLocBorrow, memIdLocLend, socketCnaTopoInfo);
    ASSERT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemTopologyInfoManager, TestGetSocketTotalLentMem)
{
    const uint64_t MB_1 = 1024;
    std::string nodeId = "1";
    int sotckeId = 36;
    uint64_t socketTotalLentMem = 0;
    std::vector<std::shared_ptr<MemNumaInfo>> numaInfo;
    UbseMemNumaLoc loc{"1", 36, 0};
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc{0, 0, 0};
    GlobalNumaIndex globalIndex = 0;
    std::shared_ptr<MemNumaInfo> numa = std::make_shared<MemNumaInfo>(loc, ubseMemNumaIndexLoc, globalIndex);
    numa->mMemLent = MB_1;
    numaInfo.push_back(numa);
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetAllNumaInfo).stubs().will(returnValue(numaInfo));
    auto ret = UbseMemTopologyInfoManager::GetInstance().GetSocketTotalLentMem(nodeId, sotckeId, socketTotalLentMem);
    ASSERT_EQ(UBSE_OK, ret);
}

TEST_F(TestUbseMemTopologyInfoManager, TestInitCoordinate)
{
    GTEST_SKIP();
    std::array<std::set<NodeIndex>, tc::rs::mem::NUM_HOSTS> neighborNodes;
    UbseMemCoordinateDesc coordinateDesc;
    tc::rs::mem::StrategyParam strategyParam;
    const uint32_t totalHostsNum = 1;

    auto ret = InitCoordinate(neighborNodes, coordinateDesc, strategyParam, totalHostsNum);
    ASSERT_EQ(true, ret);
}

TEST_F(TestUbseMemTopologyInfoManager, TestInitSingleNode)
{
    int32_t totalHostsNum = 2;
    std::set<uint16_t> nodeIndexList;
    nodeIndexList.insert(0);
    nodeIndexList.insert(1);
    UbseMemCoordinateDesc coordinateDesc;
    tc::rs::mem::StrategyParam strategyParam;

    EXPECT_NO_THROW(InitSingleNode(totalHostsNum, nodeIndexList, coordinateDesc, strategyParam));
}

TEST_F(TestUbseMemTopologyInfoManager, TestGetNodePoolMemSize)
{
    NodeData nodeData;
    std::string nodeId = "1";
    uint64_t nodePoolMemSize;
    std::vector<std::shared_ptr<MemNumaInfo>> numaList;
    EXPECT_NO_THROW(UbseMemTopologyInfoManager::GetInstance().GetNodePoolMemSize(nodeId, nodePoolMemSize, numaList));
}

TEST_F(TestUbseMemTopologyInfoManager, TestGetActualNodeMemTotal)
{
    NodeData nodeData;
    std::string nodeId = "1";
    uint64_t nodePoolMemSize;
    std::vector<std::shared_ptr<MemNumaInfo>> numaList;
    EXPECT_NO_THROW(UbseMemTopologyInfoManager::GetInstance().GetActualNodeMemTotal(nodeId, nodePoolMemSize, numaList));
}

TEST_F(TestUbseMemTopologyInfoManager, TestUbseMemGetIntersectionWithAllNeighbor)
{
    NodeIndex localNode;
    std::array<std::set<NodeIndex>, tc::rs::mem::NUM_HOSTS> neighborNodes;
    std::set<NodeIndex> intersection1;
    std::set<NodeIndex> intersection2;
    EXPECT_NO_THROW(UbseMemGetIntersectionWithAllNeighbor(localNode, neighborNodes, intersection1, intersection2));
}

TEST_F(TestUbseMemTopologyInfoManager, TestLocateXandY)
{
    tc::rs::mem::StrategyParam strategyParam;
    UbseMemCoordinateDesc coordinateDesc;
    int8_t locatedX;
    int8_t locatedY;
    EXPECT_NO_THROW(LocateXandY(strategyParam, coordinateDesc, locatedX, locatedY));
}

TEST_F(TestUbseMemTopologyInfoManager, TestGenerateCoordinate)
{
    std::array<std::set<NodeIndex>, tc::rs::mem::NUM_HOSTS> neighborNodes;
    tc::rs::mem::StrategyParam strategyParam;
    std::set<uint16_t> nodeIndexList;
    EXPECT_NO_THROW(
        UbseMemTopologyInfoManager::GetInstance().GenerateCoordinate(neighborNodes, strategyParam, nodeIndexList));
}

TEST_F(TestUbseMemTopologyInfoManager, TestGetNodeIdAndSocketIdFromNodeSocketString)
{
    const std::string nodeSocketStr;
    std::string nodeId;
    std::string socketId;
    EXPECT_NO_THROW(GetNodeIdAndSocketIdFromNodeSocketString(nodeSocketStr, nodeId, socketId));
}

TEST_F(TestUbseMemTopologyInfoManager, TestSetMemOutHardLimit)
{
    StrategyParam strategyParam;
    UbseMemNumaLoc ubseMemNumaLoc;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 0;
    std::vector<std::shared_ptr<MemNumaInfo>> numaList{
        std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex)};
    EXPECT_NO_THROW(UbseMemTopologyInfoManager::GetInstance().SetMemOutHardLimit(strategyParam, numaList));
}

TEST_F(TestUbseMemTopologyInfoManager, TestSetMemOutHardLimit2)
{
    StrategyParam strategyParam;
    UbseMemNumaLoc ubseMemNumaLoc;
    ubseMemNumaLoc.nodeId = "1";
    ubseMemNumaLoc.numaId = 1;
    ubseMemNumaLoc.socketId = 1;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 0;
    std::vector<std::shared_ptr<MemNumaInfo>> numaList{
        std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex)};
    EXPECT_NO_THROW(UbseMemTopologyInfoManager::GetInstance().SetMemOutHardLimit(strategyParam, numaList));
}

TEST_F(TestUbseMemTopologyInfoManager, TestSetMemOutHardLimit3)
{
    StrategyParam strategyParam;
    UbseMemNumaLoc ubseMemNumaLoc;
    ubseMemNumaLoc.nodeId = "1";
    ubseMemNumaLoc.numaId = 1;
    ubseMemNumaLoc.socketId = 1;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 0;
    auto memNodeInfo = std::make_shared<MemNodeInfo>(1, NodeData{});
    std::vector<std::shared_ptr<MemNumaInfo>> numaList{
        std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex)};
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNodeInfoById).stubs().will(returnValue(memNodeInfo));
    EXPECT_NO_THROW(UbseMemTopologyInfoManager::GetInstance().SetMemOutHardLimit(strategyParam, numaList));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNodeInfoById).reset();
}

TEST_F(TestUbseMemTopologyInfoManager, TestSetMemOutHardLimit4)
{
    StrategyParam strategyParam;
    UbseMemNumaLoc ubseMemNumaLoc;
    ubseMemNumaLoc.nodeId = "1";
    ubseMemNumaLoc.numaId = 1;
    ubseMemNumaLoc.socketId = 1;
    UbseMemNumaIndexLoc ubseMemNumaIndexLoc;
    GlobalNumaIndex globalIndex = 0;
    auto memNodeInfo = std::make_shared<MemNodeInfo>(1, NodeData{});
    std::vector<std::shared_ptr<MemNumaInfo>> numaList{
        std::make_shared<MemNumaInfo>(ubseMemNumaLoc, ubseMemNumaIndexLoc, globalIndex)};
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNodeInfoById).stubs().will(returnValue(memNodeInfo));
    MOCKER_CPP(&mem::strategy::MemNodeInfo::GetMCurNumaIndex).stubs().will(returnValue((int16_t)1));
    EXPECT_NO_THROW(UbseMemTopologyInfoManager::GetInstance().SetMemOutHardLimit(strategyParam, numaList));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetNodeInfoById).reset();
    MOCKER_CPP(&mem::strategy::MemNodeInfo::GetMCurNumaIndex).reset();
}

uint32_t MockUbseMemGetTopologyInfo(std::unordered_map<std::string, std::vector<MemNodeData>>& nodeTopology)
{
    nodeTopology["1"] = {{}};
    return 0;
}

TEST_F(TestUbseMemTopologyInfoManager, TestUbseMemTransTopoToNeighborSet)
{
    std::array<std::set<NodeIndex>, tc::rs::mem::NUM_HOSTS> neighborNodes;
    std::set<uint16_t> nodeIndexList;
    MOCKER_CPP(UbseMemGetTopologyInfo).stubs().will(invoke(MockUbseMemGetTopologyInfo));
    EXPECT_NO_THROW(
        UbseMemTopologyInfoManager::GetInstance().UbseMemTransTopoToNeighborSet(neighborNodes, nodeIndexList));
    MOCKER_CPP(UbseMemGetTopologyInfo).reset();
}

TEST_F(TestUbseMemTopologyInfoManager, TestUbseMemTransTopoToNeighborSet2)
{
    std::array<std::set<NodeIndex>, tc::rs::mem::NUM_HOSTS> neighborNodes;
    std::set<uint16_t> nodeIndexList;
    MOCKER_CPP(UbseMemGetTopologyInfo).stubs().will(invoke(MockUbseMemGetTopologyInfo));
    MOCKER_CPP(GetNodeIdAndSocketIdFromNodeSocketString).stubs().will(returnValue(true));
    EXPECT_NO_THROW(
        UbseMemTopologyInfoManager::GetInstance().UbseMemTransTopoToNeighborSet(neighborNodes, nodeIndexList));
    MOCKER_CPP(UbseMemGetTopologyInfo).reset();
    MOCKER_CPP(GetNodeIdAndSocketIdFromNodeSocketString).reset();
}
} // namespace ubse::mem_scheduler::ut