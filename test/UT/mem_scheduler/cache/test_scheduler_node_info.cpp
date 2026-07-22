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

#include "test_scheduler_node_info.h"

#include "scheduler_cache/ubse_mem_scheduler_node_info.h"

namespace ubse::mem::scheduler::ut {

using namespace ubse::mem::scheduler;
using namespace ubse::nodeController;

namespace {

UbseNodeInfo MakeSimpleNodeInfo(const std::string& nodeId)
{
    UbseNodeInfo info{};
    info.nodeId = nodeId;
    info.hostName = "host-" + nodeId;
    info.allocator = UbseAllocator::BUDDY_HIGHMEM;
    info.blockSize = 128;
    info.isLender = true;
    info.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;

    UbseNumaLocation loc0{nodeId, 0};
    UbseNumaInfo numa0{};
    numa0.location = loc0;
    numa0.socketId = 36;
    numa0.size = 4096;
    numa0.freeSize = 2048;
    info.numaInfos[loc0] = numa0;

    UbseNumaLocation loc1{nodeId, 1};
    UbseNumaInfo numa1{};
    numa1.location = loc1;
    numa1.socketId = 36;
    numa1.size = 4096;
    numa1.freeSize = 2048;
    info.numaInfos[loc1] = numa1;

    UbseNumaLocation loc2{nodeId, 2};
    UbseNumaInfo numa2{};
    numa2.location = loc2;
    numa2.socketId = 276;
    numa2.size = 2048;
    numa2.freeSize = 1024;
    info.numaInfos[loc2] = numa2;

    UbseCpuLocation cpuKey0{nodeId, 0};
    UbseCpuInfo cpu0{};
    cpu0.socketId = 36;
    cpu0.chipId = "0";
    info.cpuInfos[cpuKey0] = cpu0;

    UbseCpuLocation cpuKey1{nodeId, 1};
    UbseCpuInfo cpu1{};
    cpu1.socketId = 276;
    cpu1.chipId = "1";
    info.cpuInfos[cpuKey1] = cpu1;

    return info;
}

} // namespace

TEST_F(TestSchedulerNodeInfo, ConstructFromNodeInfo)
{
    auto info = MakeSimpleNodeInfo("1");
    SchedulerNodeInfo node(info);

    EXPECT_EQ(node.GetNodeId(), "1");
    EXPECT_EQ(node.GetAllocator(), UbseAllocator::BUDDY_HIGHMEM);
    EXPECT_EQ(node.GetBlockSize(), 128u);
    EXPECT_TRUE(node.IsLender());
    node.UpdateNodeClusterState(UbseNodeClusterState::UBSE_NODE_WORKING);
    EXPECT_EQ(node.GetClusterState(), UbseNodeClusterState::UBSE_NODE_WORKING);
}

TEST_F(TestSchedulerNodeInfo, GetSocketInfoFound)
{
    auto info = MakeSimpleNodeInfo("1");
    SchedulerNodeInfo node(info);

    auto socket = node.GetSocketInfo(36);
    ASSERT_NE(socket, nullptr);
}

TEST_F(TestSchedulerNodeInfo, GetSocketInfoNotFound)
{
    auto info = MakeSimpleNodeInfo("1");
    SchedulerNodeInfo node(info);

    EXPECT_EQ(node.GetSocketInfo(999), nullptr);
}

TEST_F(TestSchedulerNodeInfo, GetNumaInfoFound)
{
    auto info = MakeSimpleNodeInfo("1");
    SchedulerNodeInfo node(info);

    auto numa = node.GetNumaInfo(36, 0);
    ASSERT_NE(numa, nullptr);
    EXPECT_EQ(numa->GetNumaId(), 0u);
}

TEST_F(TestSchedulerNodeInfo, GetNodeInfoReturnsCorrectView)
{
    auto info = MakeSimpleNodeInfo("1");
    SchedulerNodeInfo node(info);

    auto view = node.GetNodeInfo();
    EXPECT_EQ(view.nodeId, "1");
    ASSERT_EQ(view.socketInfos.size(), 2u);
    std::set<uint32_t> ids;
    for (const auto& s : view.socketInfos) {
        ids.insert(s.socketId);
    }
    EXPECT_TRUE(ids.count(36) == 1);
    EXPECT_TRUE(ids.count(276) == 1);
}

} // namespace ubse::mem::scheduler::ut
