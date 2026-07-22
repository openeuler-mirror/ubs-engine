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

/**
 * @file test_scheduler_node_change.cpp
 * @brief 端到端用例: NodeObjChangeHandler 节点注册
 *
 * 用例 1: SchedulerStartSuccess — 单节点注册验证
 * 用例 2: TwoNodeInit — 双节点注册验证
 */

#include "test_scheduler_fixture.h"

namespace ubse::mem::scheduler::ut {
namespace {

using namespace ubse::common::def;

} // namespace

/**
 * @brief 端到端用例 1: SchedulerStartSuccess
 *
 * 测试 NodeObjChangeHandler 节点注册和初始化成功。
 *
 * 前置状态: Scheduler 已 Init, 无节点注册
 *
 * 测试输入:
 *   UbseNodeInfo{nodeId: "1", hostName: "host-1", allocator: BUDDY_HIGHMEM,
 *                blockSize: 128, isLender: true, clusterState: WORKING,
 *                2 sockets x 2 numas}
 *
 * 操作步骤:
 *   1. 调用 NodeObjChangeHandler(nodeInfo)
 *
 * 预期输出:
 *   1. 返回值 == UBSE_OK
 *   2. GetNodeInfo("1") != nullptr
 *   3. GetNodeInfo("1")->GetNodeId() == "1"
 *   4. GetNodeInfo("1")->GetAllocator() == BUDDY_HIGHMEM
 *   5. GetNodeInfo("1")->GetBlockSize() == 128
 *   6. GetNodeInfo("1")->IsLender() == true
 *   7. GetNodeInfo("1")->GetClusterState() == WORKING
 *   8. GetSocketInfo("1", 36) != nullptr
 *   9. GetSocketInfo("1", 276) != nullptr
 *   10. 4 个 NUMA(0/1/2/3) 均可查到且 ID 正确
 *   11. GetAllNodes().size() == 1, socketInfos.size() == 2
 */
TEST_F(TestSchedulerEndToEnd, SchedulerStartSuccess)
{
    auto nodeInfo = MakeNodeInfo("1", "host-1");

    auto ret = SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeInfo);
    EXPECT_EQ(ret, UBSE_OK);

    auto& impl = SchedulerImpl::GetInstance();
    auto cachedNode = impl.nodeInfo_->GetNodeInfo("1");
    ASSERT_NE(cachedNode, nullptr);
    EXPECT_EQ(cachedNode->GetNodeId(), "1");
    EXPECT_EQ(cachedNode->GetAllocator(), UbseAllocator::BUDDY_HIGHMEM);
    EXPECT_EQ(cachedNode->GetBlockSize(), 128u);
    EXPECT_EQ(cachedNode->IsLender(), true);
    EXPECT_EQ(cachedNode->GetClusterState(), UbseNodeClusterState::UBSE_NODE_WORKING);

    EXPECT_NE(impl.nodeInfo_->GetSocketInfo("1", 36), nullptr);
    EXPECT_NE(impl.nodeInfo_->GetSocketInfo("1", 276), nullptr);

    EXPECT_NE(impl.nodeInfo_->GetNumaInfo("1", 0), nullptr);
    EXPECT_EQ(impl.nodeInfo_->GetNumaInfo("1", 0)->GetNumaId(), 0u);
    EXPECT_NE(impl.nodeInfo_->GetNumaInfo("1", 1), nullptr);
    EXPECT_EQ(impl.nodeInfo_->GetNumaInfo("1", 1)->GetNumaId(), 1u);
    EXPECT_NE(impl.nodeInfo_->GetNumaInfo("1", 2), nullptr);
    EXPECT_EQ(impl.nodeInfo_->GetNumaInfo("1", 2)->GetNumaId(), 2u);
    EXPECT_NE(impl.nodeInfo_->GetNumaInfo("1", 3), nullptr);
    EXPECT_EQ(impl.nodeInfo_->GetNumaInfo("1", 3)->GetNumaId(), 3u);

    auto allNodes = impl.nodeInfo_->GetAllNodes();
    ASSERT_EQ(allNodes.size(), 1u);
    EXPECT_EQ(allNodes[0].nodeId, "1");
    EXPECT_EQ(allNodes[0].socketInfos.size(), 2u);
}

/**
 * @brief 端到端用例 2: TwoNodeInit
 *
 * 测试两节点注册成功后节点缓存正确。
 *
 * 前置状态: Scheduler 已 Init, 无节点注册
 *
 * 测试输入:
 *   nodeInfo1: {nodeId: "1", hostName: "host-1", ...} // 2 sockets x 2 numa
 *   nodeInfo2: {nodeId: "2", hostName: "host-2", ...} // 2 sockets x 2 numa
 *
 * 操作步骤:
 *   1. 调用 NodeObjChangeHandler(nodeInfo1)
 *   2. 调用 NodeObjChangeHandler(nodeInfo2)
 *
 * 预期输出:
 *   1. 两次调用返回值均 == UBSE_OK
 *   2. GetAllNodes().size() == 2
 *   3. GetNodeInfo("1") != nullptr 且 GetNodeInfo("2") != nullptr
 *   4. 两个节点均有 2 个 Socket(36, 276)
 *   5. 每个 Socket 均有 2 个 NUMA
 */
TEST_F(TestSchedulerEndToEnd, TwoNodeInit)
{
    auto nodeInfo1 = MakeNodeInfo("1", "host-1");
    auto nodeInfo2 = MakeNodeInfo("2", "host-2");

    auto ret1 = SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeInfo1);
    EXPECT_EQ(ret1, UBSE_OK);

    auto ret2 = SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeInfo2);
    EXPECT_EQ(ret2, UBSE_OK);

    auto& impl = SchedulerImpl::GetInstance();
    auto cached1 = impl.nodeInfo_->GetNodeInfo("1");
    ASSERT_NE(cached1, nullptr);
    EXPECT_EQ(cached1->GetNodeId(), "1");

    auto cached2 = impl.nodeInfo_->GetNodeInfo("2");
    ASSERT_NE(cached2, nullptr);
    EXPECT_EQ(cached2->GetNodeId(), "2");

    auto allNodes = impl.nodeInfo_->GetAllNodes();
    ASSERT_EQ(allNodes.size(), 2u);

    for (const auto& node : allNodes) {
        ASSERT_EQ(node.socketInfos.size(), 2u);
        std::set<uint32_t> socketIds;
        for (const auto& socket : node.socketInfos) {
            socketIds.insert(socket.socketId);
        }
        EXPECT_TRUE(socketIds.count(36) == 1);
        EXPECT_TRUE(socketIds.count(276) == 1);

        for (const auto& socket : node.socketInfos) {
            EXPECT_EQ(socket.numaInfos.size(), 2u);
        }
    }
}

/**
 * @brief 端到端用例 3: NodeRegisterEmptyNodeId
 *
 * 测试空 nodeId 注册返回错误。
 *
 * 前置状态: Scheduler 已 Init, 无节点注册
 *
 * 测试输入:
 *   UbseNodeInfo{nodeId: "", hostName: "host-1", ...}
 *
 * 操作步骤:
 *   1. 调用 NodeObjChangeHandler(nodeInfo)
 *
 * 预期输出:
 *   1. 返回值 == UBSE_ERROR
 */
TEST_F(TestSchedulerEndToEnd, NodeRegisterEmptyNodeId)
{
    auto nodeInfo = MakeNodeInfo("1", "host-1");
    nodeInfo.nodeId = "";

    auto ret = SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeInfo);
    EXPECT_EQ(ret, UBSE_ERROR);

    // 节点未注册到缓存
    auto& impl = SchedulerImpl::GetInstance();
    EXPECT_EQ(impl.nodeInfo_->GetNodeInfo("1"), nullptr);
}

/**
 * @brief 端到端用例 4: NodeRegisterInitException
 *
 * 测试 InitOneNodeData 中 SchedulerNodeInfo 构造异常时返回错误。
 *
 * 前置状态: Scheduler 已 Init, 无节点注册
 *
 * 测试输入:
 *   UbseNodeInfo{
 *     nodeId: "1", cpuInfos: {chipId: ""},  // std::stoi 抛异常
 *     ...
 *   }
 *
 * 操作步骤:
 *   1. 调用 NodeObjChangeHandler(nodeInfo)
 *
 * 预期输出:
 *   1. 返回值 == UBSE_ERROR_INVAL
 *   2. GetNodeInfo("1") != nullptr（节点已注册，chipId 降级为 0）
 */
TEST_F(TestSchedulerEndToEnd, NodeRegisterChipIdInvalid)
{
    auto nodeInfo = MakeNodeInfo("1", "host-1");
    // 将 chipId 置空使 ConvertStrToUint32 失败，降级为 chipId=0
    UbseCpuLocation cpuKey0{nodeInfo.nodeId, 0};
    nodeInfo.cpuInfos[cpuKey0].chipId = "";

    auto ret = SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeInfo);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);

    // InitOneNodeData 成功，节点已注册，chipId 降级为 0
    auto& impl = SchedulerImpl::GetInstance();
    EXPECT_NE(impl.nodeInfo_->GetNodeInfo("1"), nullptr);
}

/**
 * @brief 端到端用例 5: NodeReRegisterWithUpdatedState
 *
 * 测试同一 nodeId 重复注册, clusterState 被正确更新且不重新初始化。
 *
 * 前置状态: Scheduler 已 Init, 无节点注册
 *
 * 测试输入:
 *   第一次: clusterState=WORKING
 *   第二次: clusterState=SMOOTHING（相同 nodeId）
 *
 * 操作步骤:
 *   1. 调用 NodeObjChangeHandler(nodeInfo) — clusterState=WORKING
 *   2. 修改 clusterState 为 SMOOTHING
 *   3. 调用 NodeObjChangeHandler(nodeInfo) — 再次注册
 *
 * 预期输出:
 *   1. 两次返回值均 == UBSE_OK
 *   2. GetNodeInfo("1")->GetClusterState() == SMOOTHING
 *   3. GetAllNodes().size() == 1（未重复创建）
 */
TEST_F(TestSchedulerEndToEnd, NodeReRegisterWithUpdatedState)
{
    auto nodeInfo = MakeNodeInfo("1", "host-1");

    auto ret1 = SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeInfo);
    EXPECT_EQ(ret1, UBSE_OK);

    nodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_SMOOTHING;
    auto ret2 = SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeInfo);
    EXPECT_EQ(ret2, UBSE_OK);

    auto& impl = SchedulerImpl::GetInstance();
    auto cached = impl.nodeInfo_->GetNodeInfo("1");
    ASSERT_NE(cached, nullptr);
    EXPECT_EQ(cached->GetClusterState(), UbseNodeClusterState::UBSE_NODE_SMOOTHING);

    // 未重复创建节点
    EXPECT_EQ(impl.nodeInfo_->GetAllNodes().size(), 1u);
}

// ==================== P2: Node Register FAULT State ====================

/**
 * @brief 6: NodeRegisterFaultState
 *
 * clusterState=FAULT 的节点注册时，NodeObjChangeHandler 跳过初始化返回 UBSE_OK。
 */
TEST_F(TestSchedulerEndToEnd, NodeRegisterFaultState)
{
    auto nodeInfo = MakeNodeInfo("1", "host-1");
    nodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_FAULT;

    auto ret = SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeInfo);
    EXPECT_EQ(ret, UBSE_OK);

    auto& impl = SchedulerImpl::GetInstance();
    EXPECT_EQ(impl.nodeInfo_->GetNodeInfo("1"), nullptr);
}

} // namespace ubse::mem::scheduler::ut
