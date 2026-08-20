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

#include "test_scheduler_sub_health_filter.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_mem_scheduler_account_manager.h"
#include "ubse_mem_scheduler_node_manager.h"
#include "ubse_mem_scheduler_request.h"
#include "scheduler_filter/ubse_mem_scheduler_sub_health_filter.h"

namespace ubse::mem::scheduler::ut {

namespace {

NodeInfo MakeNode(const NodeId& id, SocketId socketId)
{
    NodeInfo n;
    n.nodeId = id;
    SocketInfo s;
    s.socketId = socketId;
    s.numaInfos = {0};
    n.socketInfos.push_back(s);
    return n;
}

NodeInfo MakeNodeTwoSockets(const NodeId& id, SocketId s1, SocketId s2)
{
    NodeInfo n;
    n.nodeId = id;
    SocketInfo a;
    a.socketId = s1;
    a.numaInfos = {0};
    SocketInfo b;
    b.socketId = s2;
    b.numaInfos = {1};
    n.socketInfos.push_back(a);
    n.socketInfos.push_back(b);
    return n;
}

} // namespace

void TestSchedulerSubHealthFilter::SetUp()
{
    Test::SetUp();
}

void TestSchedulerSubHealthFilter::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

// 全部健康 → 所有节点/socket 保留
TEST_F(TestSchedulerSubHealthFilter, AllHealthyKeepsAllSockets)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager account;
    SubHealthFilter filter;
    SchedulerRequest req;
    req.requestNodeId_ = "1";
    req.importSocketId_ = 0; // Numa 借用，走 IsSocketPairSubHealthy

    std::vector<NodeInfo> nodes{MakeNode("2", 36), MakeNode("3", 36)};

    MOCKER(&SchedulerNodeManager::IsSocketPairSubHealthy).stubs().will(returnValue(false));

    EXPECT_EQ(UBSE_OK, filter.FilterNodes(nodes, nodeMgr, account, req));
    ASSERT_EQ(nodes.size(), 2u);
    EXPECT_EQ(nodes[0].socketInfos.size(), 1u);
    EXPECT_EQ(nodes[1].socketInfos.size(), 1u);
}

// 第一个 socket 亚健康被剔除，第二个健康保留
TEST_F(TestSchedulerSubHealthFilter, SubHealthySocketRemoved)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager account;
    SubHealthFilter filter;
    SchedulerRequest req;
    req.requestNodeId_ = "1";
    req.importSocketId_ = 0; // Numa 借用

    std::vector<NodeInfo> nodes{MakeNodeTwoSockets("2", 36, 37)};

    MOCKER(&SchedulerNodeManager::IsSocketPairSubHealthy)
        .stubs()
        .will(returnValue(true))
        .then(returnValue(false));

    EXPECT_EQ(UBSE_OK, filter.FilterNodes(nodes, nodeMgr, account, req));
    ASSERT_EQ(nodes.size(), 1u);
    ASSERT_EQ(nodes[0].socketInfos.size(), 1u);
    EXPECT_EQ(nodes[0].socketInfos[0].socketId, 37u);
}

// 节点所有 socket 均亚健康 → 节点被整体移除
TEST_F(TestSchedulerSubHealthFilter, AllSocketsSubHealthyRemovesNode)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager account;
    SubHealthFilter filter;
    SchedulerRequest req;
    req.requestNodeId_ = "1";
    req.importSocketId_ = 0; // Numa 借用

    std::vector<NodeInfo> nodes{MakeNode("2", 36), MakeNode("3", 37)};

    MOCKER(&SchedulerNodeManager::IsSocketPairSubHealthy).stubs().will(returnValue(true));

    EXPECT_EQ(UBSE_OK, filter.FilterNodes(nodes, nodeMgr, account, req));
    EXPECT_TRUE(nodes.empty());
}

// importNode 不参与亚健康过滤（跳过）
TEST_F(TestSchedulerSubHealthFilter, ImportNodeNotFiltered)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager account;
    SubHealthFilter filter;
    SchedulerRequest req;
    req.requestNodeId_ = "1";
    req.importSocketId_ = 0; // Numa 借用

    std::vector<NodeInfo> nodes{MakeNode("1", 36), MakeNode("2", 37)};

    MOCKER(&SchedulerNodeManager::IsSocketPairSubHealthy).stubs().will(returnValue(true));

    EXPECT_EQ(UBSE_OK, filter.FilterNodes(nodes, nodeMgr, account, req));
    // importNode="1" 被跳过保留；node "2" 的 socket 亚健康被剔除后整体移除
    ASSERT_EQ(nodes.size(), 1u);
    EXPECT_EQ(nodes[0].nodeId, "1");
    EXPECT_EQ(nodes[0].socketInfos.size(), 1u);
}

} // namespace ubse::mem::scheduler::ut
