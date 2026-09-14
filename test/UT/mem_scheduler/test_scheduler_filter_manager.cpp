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

#include "test_scheduler_filter_manager.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_mem_scheduler_account_manager.h"
#include "ubse_mem_scheduler_filter_manager.h"
#include "ubse_mem_scheduler_node_manager.h"
#include "ubse_node_controller.h"
#include "scheduler_filter/ubse_mem_scheduler_free_memory_filter.h"
#include "scheduler_filter/ubse_mem_scheduler_shared_pool_filter.h"

namespace ubse::mem::scheduler::ut {

using namespace ubse::common::def;

namespace {
constexpr uint64_t BYTES_PER_GB = 1073741824;

UbseNodeInfo MakePoolNode(const std::string& nodeId, uint32_t nodeMaxLendGb)
{
    UbseNodeInfo info{};
    info.nodeId = nodeId;
    info.hostName = "host-" + nodeId;
    info.allocator = UbseAllocator::BUDDY_HIGHMEM;
    info.blockSize = 128;
    info.isLender = true;
    info.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    info.nodeMaxLendGb = nodeMaxLendGb;

    ubse::nodeController::UbseNumaLocation loc0{nodeId, 0};
    UbseNumaInfo numa0{};
    numa0.location = loc0;
    numa0.socketId = 36;
    numa0.size = 4096;
    numa0.freeSize = 2048;
    info.numaInfos[loc0] = numa0;

    ubse::nodeController::UbseCpuLocation cpuKey0{nodeId, 0};
    ubse::nodeController::UbseCpuInfo cpu0{};
    cpu0.socketId = 36;
    cpu0.chipId = "0";
    info.cpuInfos[cpuKey0] = cpu0;
    return info;
}

NodeInfo MakePoolNodeView(const std::string& nodeId)
{
    NodeInfo node;
    node.nodeId = nodeId;
    node.socketInfos = {{36, {0}}};
    return node;
}

constexpr uint64_t MB_BYTES = 1024 * 1024;

// FreeMemoryFilter 用例：单 socket(36) 挂两个 NUMA 的 lender 节点，容量由用例显式设定
UbseNodeInfo MakeTwoNumaLender(const std::string& nodeId, uint32_t blockSizeMb)
{
    UbseNodeInfo info{};
    info.nodeId = nodeId;
    info.hostName = "host-" + nodeId;
    info.allocator = UbseAllocator::BUDDY_HIGHMEM;
    info.blockSize = blockSizeMb;
    info.isLender = true;
    info.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;

    for (uint32_t numaId : {0U, 1U}) {
        ubse::nodeController::UbseNumaLocation loc{nodeId, numaId};
        UbseNumaInfo numa{};
        numa.location = loc;
        numa.socketId = 36;
        numa.size = 0;
        numa.freeSize = 0;
        info.numaInfos[loc] = numa;
    }

    ubse::nodeController::UbseCpuLocation cpuKey{nodeId, 0};
    ubse::nodeController::UbseCpuInfo cpu{};
    cpu.socketId = 36;
    cpu.chipId = "0";
    info.cpuInfos[cpuKey] = cpu;
    return info;
}

NodeInfo MakeTwoNumaLenderView(const std::string& nodeId)
{
    NodeInfo node;
    node.nodeId = nodeId;
    node.socketInfos = {{36, {0, 1}}};
    return node;
}
} // namespace

void TestSchedulerFilterManager::SetUp()
{
    Test::SetUp();
}

void TestSchedulerFilterManager::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

TEST_F(TestSchedulerFilterManager, InitRegistersAllFilters)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerFilterManager mgr(&nodeMgr, &accMgr);

    mgr.Init();

    EXPECT_NE(mgr.FindFilterByName("ConfigConsistencyFilter"), nullptr);
    EXPECT_NE(mgr.FindFilterByName("RoleConflictFilter"), nullptr);
    EXPECT_NE(mgr.FindFilterByName("LenderRoleFilter"), nullptr);
    EXPECT_NE(mgr.FindFilterByName("SocketAffinityFilter"), nullptr);
    EXPECT_NE(mgr.FindFilterByName("GroupFilter"), nullptr);
    EXPECT_NE(mgr.FindFilterByName("ProviderFilter"), nullptr);
    EXPECT_NE(mgr.FindFilterByName("NodeStateFilter"), nullptr);
    EXPECT_NE(mgr.FindFilterByName("RequestedProvidersFilter"), nullptr);
    EXPECT_NE(mgr.FindFilterByName("LendCountFilter"), nullptr);
    EXPECT_NE(mgr.FindFilterByName("SpecifiedLenderFilter"), nullptr);
    EXPECT_NE(mgr.FindFilterByName("TopoReachabilityFilter"), nullptr);
    EXPECT_NE(mgr.FindFilterByName("MaxLentSizeFilter"), nullptr);
    EXPECT_NE(mgr.FindFilterByName("RadiusBorrowFilter"), nullptr);
    EXPECT_NE(mgr.FindFilterByName("RadiusLenderFilter"), nullptr);
    EXPECT_NE(mgr.FindFilterByName("RegionFilter"), nullptr);
    EXPECT_NE(mgr.FindFilterByName("FreeMemoryFilter"), nullptr);
}

TEST_F(TestSchedulerFilterManager, RegisterFilterAddsToMap)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerFilterManager mgr(&nodeMgr, &accMgr);

    EXPECT_EQ(mgr.FindFilterByName("NonExistent"), nullptr);
}

TEST_F(TestSchedulerFilterManager, FilterNodesWithEmptyListPassthrough)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerFilterManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    std::vector<NodeInfo> nodes;
    SchedulerRequest req;
    auto result = mgr.FilterNodes(nodes, req);

    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestSchedulerFilterManager, FilterByNamesSkipsUnknownFilter)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerFilterManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    std::vector<NodeInfo> nodes;
    SchedulerRequest req;
    req.filterNames_ = {"NonExistentFilter"};

    auto result = mgr.FilterByNames(nodes, req.filterNames_, req);
    EXPECT_EQ(result, UBSE_OK);
}

TEST_F(TestSchedulerFilterManager, FilterNodesEmptyNameListKeepsNodes)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerFilterManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    NodeInfo node;
    node.nodeId = "1";
    std::vector<NodeInfo> nodes = {node};
    SchedulerRequest req;

    auto result = mgr.FilterNodes(nodes, req);
    EXPECT_EQ(result, UBSE_OK);
    ASSERT_EQ(nodes.size(), 1u);
    EXPECT_EQ(nodes[0].nodeId, "1");
}

TEST_F(TestSchedulerFilterManager, FilterByNamesExecutesFiltersSequentially)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    SchedulerFilterManager mgr(&nodeMgr, &accMgr);
    mgr.Init();

    NodeInfo n1;
    n1.nodeId = "1";
    NodeInfo n2;
    n2.nodeId = "2";
    std::vector<NodeInfo> nodes = {n1, n2};

    SchedulerRequest req;
    req.filterNames_ = {"NodeStateFilter", "LenderRoleFilter"};

    auto result = mgr.FilterByNames(nodes, req.filterNames_, req);
    EXPECT_EQ(result, UBSE_OK);
}

// ==================== SharedPoolFilter Tests ====================

TEST_F(TestSchedulerFilterManager, SharedPoolFilterUnconfiguredCapUnlimited)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    nodeMgr.UpdateNodeInfo(MakePoolNode("1", 0)); // nodeMaxLendGb=0: 未配置, 不限制

    auto* numa = nodeMgr.GetNumaInfo("1", 0);
    ASSERT_NE(numa, nullptr);
    numa->AddNumaLentSize(100 * BYTES_PER_GB);

    SharedPoolFilter filter;
    std::vector<NodeInfo> nodes = {MakePoolNodeView("1")};
    SchedulerRequest req;
    req.requestSize_ = 6 * BYTES_PER_GB;

    auto ret = filter.FilterNodes(nodes, nodeMgr, accMgr, req);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nodes.size(), 1u);
    EXPECT_EQ(nodes[0].nodeId, "1");
}

TEST_F(TestSchedulerFilterManager, SharedPoolFilterRejectsWhenProjectedExceedsCap)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    nodeMgr.UpdateNodeInfo(MakePoolNode("1", 20));

    auto* numa = nodeMgr.GetNumaInfo("1", 0);
    ASSERT_NE(numa, nullptr);
    numa->AddNumaLentSize(15 * BYTES_PER_GB);

    SharedPoolFilter filter;
    std::vector<NodeInfo> nodes = {MakePoolNodeView("1")};
    SchedulerRequest req;
    req.requestSize_ = 6 * BYTES_PER_GB; // 15 + 6 = 21GB > 20GB 上限

    auto ret = filter.FilterNodes(nodes, nodeMgr, accMgr, req);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_TRUE(nodes.empty());
}

TEST_F(TestSchedulerFilterManager, SharedPoolFilterKeepsWhenProjectedEqualsCap)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    nodeMgr.UpdateNodeInfo(MakePoolNode("1", 20));

    auto* numa = nodeMgr.GetNumaInfo("1", 0);
    ASSERT_NE(numa, nullptr);
    numa->AddNumaLentSize(15 * BYTES_PER_GB);

    SharedPoolFilter filter;
    std::vector<NodeInfo> nodes = {MakePoolNodeView("1")};
    SchedulerRequest req;
    req.requestSize_ = 5 * BYTES_PER_GB; // 15 + 5 = 20GB 恰好等于上限, 保留

    auto ret = filter.FilterNodes(nodes, nodeMgr, accMgr, req);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nodes.size(), 1u);
    EXPECT_EQ(nodes[0].nodeId, "1");
}

TEST_F(TestSchedulerFilterManager, SharedPoolFilterSafeAddOverflowRejects)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    nodeMgr.UpdateNodeInfo(MakePoolNode("1", 20));

    auto* numa = nodeMgr.GetNumaInfo("1", 0);
    ASSERT_NE(numa, nullptr);
    numa->AddNumaLentSize(UINT64_MAX / 2); // lent + request 溢出 uint64

    SharedPoolFilter filter;
    std::vector<NodeInfo> nodes = {MakePoolNodeView("1")};
    SchedulerRequest req;
    req.requestSize_ = 6 * BYTES_PER_GB;

    auto ret = filter.FilterNodes(nodes, nodeMgr, accMgr, req);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_TRUE(nodes.empty());
}

TEST_F(TestSchedulerFilterManager, SharedPoolFilterUnknownNodeSkipped)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;

    SharedPoolFilter filter;
    std::vector<NodeInfo> nodes = {MakePoolNodeView("1")}; // nodeMgr 中无此节点
    SchedulerRequest req;
    req.requestSize_ = 6 * BYTES_PER_GB;

    auto ret = filter.FilterNodes(nodes, nodeMgr, accMgr, req);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nodes.size(), 1u);
    EXPECT_EQ(nodes[0].nodeId, "1");
}

// ==================== FreeMemoryFilter Tests ====================

/**
 * 可用量必须按 NUMA 逐块折算后再求和：numa0 185MB→150MB(3 块)，numa1 15MB→0 块，
 * 合计 150MB < 200MB 请求，节点应被剔除。
 * 若按原始字节求和(185+15=200MB ≥ 200MB)会被误判为可借出，放行后在分配侧才失败。
 */
TEST_F(TestSchedulerFilterManager, FreeMemoryFilterAlignsAvailableByBlockPerNuma)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    nodeMgr.UpdateNodeInfo(MakeTwoNumaLender("2", 50));

    auto* numa0 = nodeMgr.GetNumaInfo("2", 0);
    ASSERT_NE(numa0, nullptr);
    numa0->UpdateNumaMemorySize(250 * MB_BYTES, 65 * MB_BYTES, 185 * MB_BYTES); // 可用 185MB
    auto* numa1 = nodeMgr.GetNumaInfo("2", 1);
    ASSERT_NE(numa1, nullptr);
    numa1->UpdateNumaMemorySize(60 * MB_BYTES, 45 * MB_BYTES, 15 * MB_BYTES); // 可用 15MB，不足一块

    FreeMemoryFilter filter;
    std::vector<NodeInfo> nodes = {MakeTwoNumaLenderView("2")};
    SchedulerRequest req;
    req.requestSize_ = 200 * MB_BYTES;

    auto ret = filter.FilterNodes(nodes, nodeMgr, accMgr, req);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_TRUE(nodes.empty());
}

/**
 * 整块折算后刚好满足请求时保留节点：150MB(3 块) + 50MB(1 块) = 200MB == 请求 200MB。
 */
TEST_F(TestSchedulerFilterManager, FreeMemoryFilterKeepsNodeWhenAlignedBlocksEnough)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    nodeMgr.UpdateNodeInfo(MakeTwoNumaLender("2", 50));

    auto* numa0 = nodeMgr.GetNumaInfo("2", 0);
    ASSERT_NE(numa0, nullptr);
    numa0->UpdateNumaMemorySize(250 * MB_BYTES, 65 * MB_BYTES, 185 * MB_BYTES); // 可用 185MB → 150MB
    auto* numa1 = nodeMgr.GetNumaInfo("2", 1);
    ASSERT_NE(numa1, nullptr);
    numa1->UpdateNumaMemorySize(100 * MB_BYTES, 40 * MB_BYTES, 60 * MB_BYTES); // 可用 60MB → 50MB

    FreeMemoryFilter filter;
    std::vector<NodeInfo> nodes = {MakeTwoNumaLenderView("2")};
    SchedulerRequest req;
    req.requestSize_ = 200 * MB_BYTES;

    auto ret = filter.FilterNodes(nodes, nodeMgr, accMgr, req);
    EXPECT_EQ(ret, UBSE_OK);
    ASSERT_EQ(nodes.size(), 1u);
    EXPECT_EQ(nodes[0].nodeId, "2");
    ASSERT_EQ(nodes[0].socketInfos.size(), 1u);
    EXPECT_EQ(nodes[0].socketInfos[0].socketId, 36);
}

} // namespace ubse::mem::scheduler::ut
