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

#include "test_scheduler_node_manager.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_mem_scheduler_node_manager.h"
#include "ubse_node_controller.h"

namespace ubse::mem::scheduler::ut {

using namespace ubse::common::def;
using namespace ubse::nodeController;

void TestSchedulerNodeManager::SetUp()
{
    Test::SetUp();
}

void TestSchedulerNodeManager::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

namespace {

UbseNodeInfo MakeNodeInfo(const std::string& nodeId, bool withNuma = true)
{
    UbseNodeInfo info{};
    info.nodeId = nodeId;
    info.hostName = "host-" + nodeId;
    info.allocator = UbseAllocator::BUDDY_HIGHMEM;
    info.blockSize = 128;
    info.isLender = true;
    info.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;

    if (withNuma) {
        UbseNumaLocation loc0{nodeId, 0};
        UbseNumaInfo numa0{};
        numa0.location = loc0;
        numa0.socketId = 36;
        numa0.size = 4096;
        numa0.freeSize = 2048;
        info.numaInfos[loc0] = numa0;

        UbseCpuLocation cpuKey0{nodeId, 0};
        UbseCpuInfo cpu0{};
        cpu0.socketId = 36;
        cpu0.chipId = "0";
        info.cpuInfos[cpuKey0] = cpu0;
    }
    return info;
}

} // namespace

TEST_F(TestSchedulerNodeManager, UpdateNodeInfoEmptyNodeIdReturnsError)
{
    SchedulerNodeManager mgr;
    auto info = MakeNodeInfo("");
    auto ret = mgr.UpdateNodeInfo(info);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestSchedulerNodeManager, UpdateNodeInfoCreatesNewNode)
{
    SchedulerNodeManager mgr;
    auto info = MakeNodeInfo("1");
    auto ret = mgr.UpdateNodeInfo(info);
    EXPECT_EQ(ret, UBSE_OK);

    auto node = mgr.GetNodeInfo("1");
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->GetNodeId(), "1");
}

TEST_F(TestSchedulerNodeManager, GetAllNodesAfterUpdate)
{
    SchedulerNodeManager mgr;
    mgr.UpdateNodeInfo(MakeNodeInfo("1"));
    mgr.UpdateNodeInfo(MakeNodeInfo("2"));

    auto nodes = mgr.GetAllNodes();
    EXPECT_EQ(nodes.size(), 2u);
}

TEST_F(TestSchedulerNodeManager, GetNodeInfoNotFoundReturnsNull)
{
    SchedulerNodeManager mgr;
    EXPECT_EQ(mgr.GetNodeInfo("999"), nullptr);
}

TEST_F(TestSchedulerNodeManager, GetSocketInfoFromNode)
{
    SchedulerNodeManager mgr;
    mgr.UpdateNodeInfo(MakeNodeInfo("1"));

    auto socket = mgr.GetSocketInfo("1", 36);
    ASSERT_NE(socket, nullptr);
}

TEST_F(TestSchedulerNodeManager, GetNumaInfoFromNode)
{
    SchedulerNodeManager mgr;
    mgr.UpdateNodeInfo(MakeNodeInfo("1"));

    auto numa = mgr.GetNumaInfo("1", 0);
    ASSERT_NE(numa, nullptr);
    EXPECT_EQ(numa->GetNumaId(), 0u);
}

TEST_F(TestSchedulerNodeManager, UpdateAllNumaMemInfoWithBuddyHighmem)
{
    SchedulerNodeManager mgr;
    mgr.UpdateNodeInfo(MakeNodeInfo("1"));

    auto nodeMap = std::unordered_map<std::string, UbseNodeInfo>{{"1", MakeNodeInfo("1")}};
    auto ret = mgr.UpdateAllNumaMemInfo(nodeMap);
    EXPECT_EQ(ret, UBSE_OK);

    auto numa = mgr.GetNumaInfo("1", 0);
    ASSERT_NE(numa, nullptr);
    EXPECT_GT(numa->GetMemTotalSize(), 0u);
}

TEST_F(TestSchedulerNodeManager, IsFullyConnectedDefault)
{
    SchedulerNodeManager mgr;
    EXPECT_TRUE(mgr.IsFullyConnected());
}

TEST_F(TestSchedulerNodeManager, GetProviderNodeListEmptyWithoutConfig)
{
    SchedulerNodeManager mgr;

    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<config::UbseConfModule>()));

    mgr.UpdateProviderNodeList("1", "host-1");
    EXPECT_TRUE(mgr.GetProviderNodeList().empty());
}

TEST_F(TestSchedulerNodeManager, GetGroupNodesEmptyWithoutConfig)
{
    SchedulerNodeManager mgr;

    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>)
        .stubs()
        .will(returnValue(std::shared_ptr<config::UbseConfModule>()));

    mgr.UpdateGroupNodeList("1", "host-1");
    EXPECT_TRUE(mgr.GetGroupNodes("1").empty());
}

// ==================== SchedulerMode Tests ====================

TEST_F(TestSchedulerNodeManager, SchedulerModeFreePriority)
{
    SchedulerNodeManager mgr;
    auto mockConfModule = std::make_shared<config::UbseConfModule>();
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>).stubs().will(returnValue(mockConfModule));

    std::string modeStr = "free-priority";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("scheduler.mode")), outBound(modeStr))
        .will(returnValue(UBSE_OK));

    std::string providerStr = "";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("provider")), outBound(providerStr))
        .will(returnValue(UBSE_OK));

    std::string groupStr = "";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("group")), outBound(groupStr))
        .will(returnValue(UBSE_OK));

    std::string pageSize = "4096";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("os")), eq(std::string("page_size")), outBound(pageSize))
        .will(returnValue(UBSE_OK));

    mgr.InitSchedulerMode();
    EXPECT_EQ(mgr.GetSchedulerMode(), SchedulerMode::FreePriority);
    EXPECT_FALSE(mgr.GetLenderBalance());
}

TEST_F(TestSchedulerNodeManager, SchedulerModeReliabilityPriority)
{
    SchedulerNodeManager mgr;
    auto mockConfModule = std::make_shared<config::UbseConfModule>();
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>).stubs().will(returnValue(mockConfModule));

    std::string modeStr = "reliability-priority";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("scheduler.mode")), outBound(modeStr))
        .will(returnValue(UBSE_OK));

    std::string providerStr = "";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("provider")), outBound(providerStr))
        .will(returnValue(UBSE_OK));

    std::string groupStr = "";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("group")), outBound(groupStr))
        .will(returnValue(UBSE_OK));

    std::string pageSize = "4096";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("os")), eq(std::string("page_size")), outBound(pageSize))
        .will(returnValue(UBSE_OK));

    mgr.InitSchedulerMode();
    EXPECT_EQ(mgr.GetSchedulerMode(), SchedulerMode::ReliabilityPriority);
    EXPECT_TRUE(mgr.GetLenderBalance());
}

TEST_F(TestSchedulerNodeManager, SchedulerModePerformancePriority)
{
    SchedulerNodeManager mgr;
    auto mockConfModule = std::make_shared<config::UbseConfModule>();
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>).stubs().will(returnValue(mockConfModule));

    std::string modeStr = "performance-priority";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("scheduler.mode")), outBound(modeStr))
        .will(returnValue(UBSE_OK));

    std::string providerStr = "";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("provider")), outBound(providerStr))
        .will(returnValue(UBSE_OK));

    std::string groupStr = "";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("group")), outBound(groupStr))
        .will(returnValue(UBSE_OK));

    std::string pageSize = "4096";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("os")), eq(std::string("page_size")), outBound(pageSize))
        .will(returnValue(UBSE_OK));

    mgr.InitSchedulerMode();
    EXPECT_EQ(mgr.GetSchedulerMode(), SchedulerMode::PerformancePriority);
    EXPECT_FALSE(mgr.GetLenderBalance());
}

TEST_F(TestSchedulerNodeManager, SchedulerModeFallbackToLenderBalanceTrue)
{
    SchedulerNodeManager mgr;
    auto mockConfModule = std::make_shared<config::UbseConfModule>();
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>).stubs().will(returnValue(mockConfModule));

    // scheduler.mode not set → fallback to lender.balance
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("scheduler.mode")), outBound(std::string("")))
        .will(returnValue(UBSE_ERROR));

    bool lenderBal = true;
    MOCKER_CPP(&config::UbseConfModule::GetConf<bool>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("lender.balance")), outBound(lenderBal))
        .will(returnValue(UBSE_OK));

    std::string providerStr = "";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("provider")), outBound(providerStr))
        .will(returnValue(UBSE_OK));

    std::string groupStr = "";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("group")), outBound(groupStr))
        .will(returnValue(UBSE_OK));

    std::string pageSize = "4096";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("os")), eq(std::string("page_size")), outBound(pageSize))
        .will(returnValue(UBSE_OK));

    mgr.InitSchedulerMode();
    EXPECT_EQ(mgr.GetSchedulerMode(), SchedulerMode::ReliabilityPriority);
    EXPECT_TRUE(mgr.GetLenderBalance());
}

TEST_F(TestSchedulerNodeManager, SchedulerModeFallbackToLenderBalanceFalse)
{
    SchedulerNodeManager mgr;
    auto mockConfModule = std::make_shared<config::UbseConfModule>();
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>).stubs().will(returnValue(mockConfModule));

    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("scheduler.mode")), outBound(std::string("")))
        .will(returnValue(UBSE_ERROR));

    bool lenderBal = false;
    MOCKER_CPP(&config::UbseConfModule::GetConf<bool>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("lender.balance")), outBound(lenderBal))
        .will(returnValue(UBSE_OK));

    std::string providerStr = "";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("provider")), outBound(providerStr))
        .will(returnValue(UBSE_OK));

    std::string groupStr = "";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("group")), outBound(groupStr))
        .will(returnValue(UBSE_OK));

    std::string pageSize = "4096";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("os")), eq(std::string("page_size")), outBound(pageSize))
        .will(returnValue(UBSE_OK));

    mgr.InitSchedulerMode();
    EXPECT_EQ(mgr.GetSchedulerMode(), SchedulerMode::FreePriority);
    EXPECT_FALSE(mgr.GetLenderBalance());
}

TEST_F(TestSchedulerNodeManager, SchedulerModeUnknownFallbackToFreePriority)
{
    SchedulerNodeManager mgr;
    auto mockConfModule = std::make_shared<config::UbseConfModule>();
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>).stubs().will(returnValue(mockConfModule));

    std::string modeStr = "unknown";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("scheduler.mode")), outBound(modeStr))
        .will(returnValue(UBSE_OK));

    std::string providerStr = "";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("provider")), outBound(providerStr))
        .will(returnValue(UBSE_OK));

    std::string groupStr = "";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("group")), outBound(groupStr))
        .will(returnValue(UBSE_OK));

    std::string pageSize = "4096";
    MOCKER_CPP(&config::UbseConfModule::GetConf<std::string>)
        .stubs()
        .with(eq(std::string("os")), eq(std::string("page_size")), outBound(pageSize))
        .will(returnValue(UBSE_OK));

    mgr.InitSchedulerMode();
    EXPECT_EQ(mgr.GetSchedulerMode(), SchedulerMode::FreePriority);
    EXPECT_FALSE(mgr.GetLenderBalance());
}

// ==================== BandwidthTolerance Tests ====================

TEST_F(TestSchedulerNodeManager, BandwidthToleranceValid)
{
    SchedulerNodeManager mgr;
    auto mockConfModule = std::make_shared<config::UbseConfModule>();
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>).stubs().will(returnValue(mockConfModule));

    uint32_t tolerance = 256;
    MOCKER_CPP(&config::UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("bandwidth.tolerance")), outBound(tolerance))
        .will(returnValue(UBSE_OK));

    mgr.InitBandwidthTolerance(128);
    EXPECT_EQ(mgr.GetBandwidthTolerance(), 256u);
}

TEST_F(TestSchedulerNodeManager, BandwidthToleranceBelowBlockSizeFallsBack)
{
    SchedulerNodeManager mgr;
    auto mockConfModule = std::make_shared<config::UbseConfModule>();
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>).stubs().will(returnValue(mockConfModule));

    uint32_t tolerance = 64; // < blockSize(128)
    MOCKER_CPP(&config::UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("bandwidth.tolerance")), outBound(tolerance))
        .will(returnValue(UBSE_OK));

    mgr.InitBandwidthTolerance(128);
    EXPECT_EQ(mgr.GetBandwidthTolerance(), 256u); // 2 * 128
}

TEST_F(TestSchedulerNodeManager, BandwidthToleranceNotSetUsesDefault)
{
    SchedulerNodeManager mgr;
    auto mockConfModule = std::make_shared<config::UbseConfModule>();
    MOCKER(&context::UbseContext::GetModule<config::UbseConfModule>).stubs().will(returnValue(mockConfModule));

    MOCKER_CPP(&config::UbseConfModule::GetConf<uint32_t>)
        .stubs()
        .with(eq(std::string("ubse.memory")), eq(std::string("bandwidth.tolerance")), outBound(uint32_t(0)))
        .will(returnValue(UBSE_ERROR));

    mgr.InitBandwidthTolerance(128);
    EXPECT_EQ(mgr.GetBandwidthTolerance(), 256u); // 2 * 128
}

} // namespace ubse::mem::scheduler::ut
