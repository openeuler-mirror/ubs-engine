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

namespace ubse::mem::scheduler::ut {

using namespace ubse::common::def;

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

} // namespace ubse::mem::scheduler::ut
