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

#include "test_ubse_mem_controller_pre_online.h"
#include <mockcpp/mockcpp.hpp>
#include "ubse_com.h"
#include "ubse_election.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_msg.h"
#include "ubse_mem_util.h"
#include "ubse_node_controller.h"
#include "ubse_conf_module.h"
#include "ubse_mem_controller_pre_online.cpp"

namespace ubse::mem_controller::ut {
using namespace ubse::mem::controller;
using namespace ubse::mem::util;
using namespace ubse::election;
using namespace ubse::context;
using namespace ubse::config;

void TestUbseMemControllerPreOnline::SetUp()
{
    g_globalStop.store(false);
    Test::SetUp();
}
void TestUbseMemControllerPreOnline::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerPreOnline, PreOnlineInit)
{
    EXPECT_EQ(UBSE_ERROR_MODULE_LOAD_FAILED, PreOnlineInit());
}

TEST_F(TestUbseMemControllerPreOnline, IsClusterPreOnLineReady)
{
    EXPECT_EQ(false, IsClusterPreOnLineReady());
}

TEST_F(TestUbseMemControllerPreOnline, PreOnlineHandler)
{
    ubse::nodeController::UbseNodeInfo ubseNode;
    ubseNode.nodeId = "1";
    ubseNode.clusterState = UbseNodeClusterState::UBSE_NODE_UNKNOWN;
    EXPECT_EQ(PreOnlineHandler(ubseNode), UBSE_OK);

    ubseNode.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    EXPECT_EQ(PreOnlineHandler(ubseNode), UBSE_OK);

    ubseNode.clusterState = UbseNodeClusterState::UBSE_NODE_SMOOTHING;
    EXPECT_EQ(PreOnlineHandler(ubseNode), UBSE_OK);

    MOCKER(IsPreOnLineEnable).reset();
    MOCKER(IsNodeOnLine).reset();
    MOCKER(IsPreOnLineEnable).stubs().will(returnValue(true));
    MOCKER(IsNodeOnLine).stubs().will(returnValue(false));
    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(std::make_shared<UbseConfModule>()));
    EXPECT_EQ(PreOnlineHandler(ubseNode), UBSE_OK);

    MOCKER(&UbseConfModule::GetConf<bool>).stubs().with(any(), any(), outBound(true)).will(returnValue(UBSE_OK));
    EXPECT_EQ(PreOnlineHandler(ubseNode), UBSE_OK);
}

TEST_F(TestUbseMemControllerPreOnline, LcneTopologyChangeHandler)
{
    std::string eventId;
    std::string eventMessage;
    EXPECT_EQ(UBSE_OK, LcneTopologyChangeHandler(eventId, eventMessage));

    eventMessage = "DOWN;1-1:2-1|UP;1-1:3-1|";
    MOCKER(IsPreOnLineEnable).reset();
    MOCKER(IsNodeOnLine).reset();
    MOCKER(IsPreOnLineEnable).stubs().will(returnValue(true));
    MOCKER(IsNodeOnLine).stubs().will(returnValue(false));
    EXPECT_EQ(UBSE_OK, LcneTopologyChangeHandler(eventId, eventMessage));

    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> map;
    ubse::nodeController::UbseNodeInfo info1;
    info1.nodeId = "1";
    ubse::nodeController::UbseNodeInfo info2;
    info2.nodeId = "2";
    map["1"] = info1;
    map["2"] = info2;
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(map));
    EXPECT_EQ(UBSE_OK, LcneTopologyChangeHandler(eventId, eventMessage));

    MOCKER(&UbseNodeController::GetAllNodes).reset();
    ubse::nodeController::UbseNodeInfo info3;
    info3.nodeId = "3";
    map["3"] = info3;
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(map));
    EXPECT_EQ(UBSE_OK, LcneTopologyChangeHandler(eventId, eventMessage));
}

TEST_F(TestUbseMemControllerPreOnline, GeneratePreOnLineCna)
{
    std::string nodeId = "1";
    ubse::nodeController::UbseNodeInfo nodeInfo{};
    UbseCpuInfo cpuInfo;
    UbsePortInfo portInfo;
    portInfo.portId = "0";
    portInfo.remoteSlotId = "2";
    portInfo.remoteChipId = "1";
    cpuInfo.portInfos["0"] = portInfo;
    cpuInfo.socketId = 36;
    cpuInfo.busNodeCna = 1;
    cpuInfo.eid = "1A";
    UbseCpuLocation location{
        .nodeId = "1",
        .chipId = 1
    };
    nodeInfo.cpuInfos[location] = cpuInfo;
    ubse::nodeController::UbseNodeInfo remoteNode{};
    remoteNode.nodeId = "2";
    UbseCpuLocation location1{ remoteNode.nodeId, 1 };
    UbseCpuInfo remoteCpuInfo{};
    remoteCpuInfo.busNodeCna = 1;
    remoteCpuInfo.eid = "1A";
    remoteNode.cpuInfos[location1] = remoteCpuInfo;
    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo)).then(returnValue(remoteNode));
    EXPECT_NO_THROW(GeneratePreOnLineCna("1"));
}

TEST_F(TestUbseMemControllerPreOnline, FilterLcneRemote)
{
    std::string nodeId = "1";
    ubse::nodeController::UbseNodeInfo nodeInfo{};
    UbseCpuInfo cpuInfo;
    UbsePortInfo portInfo;
    portInfo.portId = "0";
    portInfo.remoteSlotId = "2";
    portInfo.remoteChipId = "1";
    portInfo.portStatus = PortStatus::UP;
    cpuInfo.portInfos["0"] = portInfo;
    cpuInfo.socketId = 36;
    cpuInfo.busNodeCna = 1;
    cpuInfo.eid = "1A";
    UbseCpuLocation location{
        .nodeId = "1",
        .chipId = 1
    };
    nodeInfo.cpuInfos[location] = cpuInfo;
    ubse::nodeController::UbseNodeInfo remoteNode{};
    remoteNode.nodeId = "2";
    UbseCpuLocation location1{ remoteNode.nodeId, 1 };
    UbseCpuInfo remoteCpuInfo{};
    remoteCpuInfo.busNodeCna = 1;
    remoteCpuInfo.eid = "1A";
    remoteNode.cpuInfos[location1] = remoteCpuInfo;
    remoteNode.clusterState = UbseNodeClusterState::UBSE_NODE_UNKNOWN;
    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(remoteNode));
    EXPECT_NO_THROW(FilterLcneRemote(nodeInfo));

    MOCKER(&UbseNodeController::GetNodeById).reset();
    remoteNode.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    EXPECT_NO_THROW(FilterLcneRemote(nodeInfo));

    MOCKER(&UbseNodeController::GetNodeById).stubs().will(returnValue(remoteNode));
    EXPECT_NO_THROW(FilterLcneRemote(nodeInfo));
}

TEST_F(TestUbseMemControllerPreOnline, PreOnLineExec)
{
    GTEST_SKIP();
    ubse::nodeController::UbseNodeInfo nodeInfo{};
    nodeInfo.nodeId = "1";
    EXPECT_EQ(UBSE_OK, PreOnLineExec(nodeInfo, true));

    std::unordered_set<std::string> remoteNodeIdSet;
    remoteNodeIdSet.insert("2");
    MOCKER(FilterLcneRemote).stubs().will(returnValue(remoteNodeIdSet));
    MOCKER(&UbseContext::GetModule<task_executor::UbseTaskExecutorModule>)
        .stubs()
        .will(returnValue(std::make_shared<task_executor::UbseTaskExecutorModule>()));
    EXPECT_NO_THROW(PreOnLineExec(nodeInfo, true));
}

TEST_F(TestUbseMemControllerPreOnline, handlePreOnLineTask)
{
    PreOnLineResp resp;
    resp.taskName = "task";
    resp.operateNode = "1";
    resp.ret = 0;
    EXPECT_NO_THROW(handlePreOnLineTask(resp));
}

TEST_F(TestUbseMemControllerPreOnline, PreOnLineSize)
{
    EXPECT_NE(PreOnLineSize(), UBSE_OK);

    MOCKER(&UbseContext::GetModule<UbseConfModule>).stubs().will(returnValue(std::make_shared<UbseConfModule>()));
    EXPECT_NE(PreOnLineSize(), UBSE_OK);

    uint64_t invalid = 127; // 127为非法值
    MOCKER(&UbseConfModule::GetConf<uint64_t>)
        .stubs()
        .with(any(), any(), outBound(invalid))
        .will(returnValue(UBSE_OK));
    EXPECT_NE(PreOnLineSize(), UBSE_OK);
}

TEST_F(TestUbseMemControllerPreOnline, IsNodeOnLineNotExists)
{
    // 准备测试数据
    const std::string nodeId = "not_exist_node";
    // 执行测试
    EXPECT_FALSE(IsNodeOnLine(nodeId));
}

TEST_F(TestUbseMemControllerPreOnline, IsNodeOnLine_ExistsAndOnline)
{
    // 准备测试数据
    const std::string nodeId = "node_online";

    // 模拟节点状态为ONLINE
    {
        std::unique_lock<std::shared_mutex> lock(preOnLineMutexMap);
        nodePreOnLine[nodeId] = PreOnLineState::ONLINE;
    }

    // 执行测试并验证结果
    EXPECT_TRUE(IsNodeOnLine(nodeId));
}

TEST_F(TestUbseMemControllerPreOnline, PreOnlineHandlerTwoNodes)
{
    ubse::nodeController::UbseNodeInfo ubseNode;
    ubseNode.nodeId = "1";
    ubseNode.clusterState = UbseNodeClusterState::UBSE_NODE_SMOOTHING;

    // 模拟2个节点
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodes;
    ubse::nodeController::UbseNodeInfo node2;
    node2.nodeId = "2";
    nodes["1"] = ubseNode;
    nodes["2"] = node2;
    MOCKER(&UbseNodeController::GetAllNodes).reset();
    MOCKER(IsPreOnLineEnable).reset();
    MOCKER(IsNodeOnLine).reset();
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodes));
    MOCKER(IsPreOnLineEnable).stubs().will(returnValue(true));
    MOCKER(IsNodeOnLine).stubs().will(returnValue(false));
    EXPECT_EQ(PreOnlineHandler(ubseNode), UBSE_OK);
}

TEST_F(TestUbseMemControllerPreOnline, PreOnlineHandlerMultipleNodes)
{
    ubse::nodeController::UbseNodeInfo ubseNode;
    ubseNode.nodeId = "1";
    ubseNode.clusterState = UbseNodeClusterState::UBSE_NODE_SMOOTHING;

    // 模拟3个节点
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodes;
    ubse::nodeController::UbseNodeInfo node2;
    node2.nodeId = "2";
    ubse::nodeController::UbseNodeInfo node3;
    node3.nodeId = "3";
    nodes["1"] = ubseNode;
    nodes["2"] = node2;
    nodes["3"] = node3;
    MOCKER(&UbseNodeController::GetAllNodes).reset();
    MOCKER(IsPreOnLineEnable).reset();
    MOCKER(IsNodeOnLine).reset();
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodes));
    MOCKER(IsPreOnLineEnable).stubs().will(returnValue(true));
    MOCKER(IsNodeOnLine).stubs().will(returnValue(false));
    EXPECT_EQ(PreOnlineHandler(ubseNode), UBSE_OK);
    // 验证为当前节点创建了线程
}
TEST_F(TestUbseMemControllerPreOnline, LcneTopologyChangeHandler_Additional)
{
    std::string eventId;
    std::string eventMessage;

    // 测试空消息
    eventMessage = "";
    EXPECT_EQ(UBSE_OK, LcneTopologyChangeHandler(eventId, eventMessage));

    // 测试预上线功能被禁用
    MOCKER(IsPreOnLineEnable).stubs().will(returnValue(false));
    eventMessage = "DOWN;1-1:2-1|UP;1-1:3-1|";
    EXPECT_EQ(UBSE_OK, LcneTopologyChangeHandler(eventId, eventMessage));

    // 重置Mock，测试集群为空的情况
    MOCKER(&UbseNodeController::GetAllNodes).reset();
    MOCKER(IsPreOnLineEnable).reset();
    MOCKER(IsNodeOnLine).reset();
    MOCKER(IsPreOnLineEnable).stubs().will(returnValue(true));
    MOCKER(IsNodeOnLine).stubs().will(returnValue(false));
    MOCKER(&UbseNodeController::GetAllNodes)
        .stubs()
        .will(returnValue(std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo>()));
    EXPECT_EQ(UBSE_OK, LcneTopologyChangeHandler(eventId, eventMessage));

    // 测试集群只有一个节点
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> singleNodeMap;
    ubse::nodeController::UbseNodeInfo singleNode;
    singleNode.nodeId = "1";
    singleNodeMap["1"] = singleNode;
    MOCKER(&UbseNodeController::GetAllNodes).reset();
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(singleNodeMap));
    EXPECT_EQ(UBSE_OK, LcneTopologyChangeHandler(eventId, eventMessage));

    // 测试集群有两个节点
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> twoNodesMap;
    ubse::nodeController::UbseNodeInfo node1;
    ubse::nodeController::UbseNodeInfo node2;
    node1.nodeId = "1";
    node2.nodeId = "2";
    twoNodesMap["1"] = node1;
    twoNodesMap["2"] = node2;
    MOCKER(&UbseNodeController::GetAllNodes).reset();
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(twoNodesMap));
    EXPECT_EQ(UBSE_OK, LcneTopologyChangeHandler(eventId, eventMessage));

    // 测试集群有多个节点
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> multiNodesMap;
    ubse::nodeController::UbseNodeInfo node3;
    node3.nodeId = "3";
    multiNodesMap["1"] = node1;
    multiNodesMap["2"] = node2;
    multiNodesMap["3"] = node3;
    MOCKER(&UbseNodeController::GetAllNodes).reset();
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(multiNodesMap));
    EXPECT_EQ(UBSE_OK, LcneTopologyChangeHandler(eventId, eventMessage));
}
TEST_F(TestUbseMemControllerPreOnline, HandlePreOnLineTask_TaskNotFound)
{
    PreOnLineResp reply;
    reply.taskName = "non_existent_task";
    reply.operateNode = "1";
    reply.ret = UBSE_OK;

    // 由于任务不存在，应输出警告日志
    EXPECT_NO_THROW(handlePreOnLineTask(reply));
}

TEST_F(TestUbseMemControllerPreOnline, HandlePreOnLineTask_TaskFailed)
{
    PreOnLineResp reply;
    reply.taskName = "test_task";
    reply.operateNode = "1";
    reply.ret = UBSE_ERROR; // 假设UBSE_ERROR表示任务失败

    // 初始化任务列表
    {
        std::unique_lock<std::shared_mutex> lock(preOnLineTaskMutexMap);
        PreOnLineTask task;
        task.nodeId = "1";
        task.preOnLineNodes = {"1", "2"};
        task.expectState = PreOnLineState::ONLINE;
        preOnLineTask[reply.taskName] = task;
        nodePreOnLine["1"] = PreOnLineState::ONLINE;
    }

    // 调用handlePreOnLineTask处理失败的任务
    EXPECT_NO_THROW(handlePreOnLineTask(reply));

    // 验证任务状态和节点状态
    {
        std::unique_lock<std::shared_mutex> lock(preOnLineTaskMutexMap);
        auto iter = preOnLineTask.find(reply.taskName);
        ASSERT_NE(iter, preOnLineTask.end());
        EXPECT_EQ(iter->second.expectState, PreOnLineState::OFFLINE);
        EXPECT_FALSE(iter->second.preOnLineNodes.empty());
        EXPECT_EQ(nodePreOnLine["1"], PreOnLineState::OFFLINE);
    }

    reply.operateNode = "2";
    EXPECT_NO_THROW(handlePreOnLineTask(reply));
    // 验证任务状态和节点状态
    {
        std::unique_lock<std::shared_mutex> lock(preOnLineTaskMutexMap);
        auto iter = preOnLineTask.find(reply.taskName);
        ASSERT_EQ(iter, preOnLineTask.end());
    }
}

TEST_F(TestUbseMemControllerPreOnline, HandlePreOnLineTask_TaskSucceeded)
{
    PreOnLineResp reply;
    reply.taskName = "test_task";
    reply.operateNode = "1";
    reply.ret = UBSE_OK;

    // 初始化任务列表
    {
        std::unique_lock<std::shared_mutex> lock(preOnLineTaskMutexMap);
        PreOnLineTask task;
        task.preOnLineNodes = {"1"};
        task.expectState = PreOnLineState::ONLINE;
        preOnLineTask[reply.taskName] = task;
        nodePreOnLine["1"] = PreOnLineState::ONLINE;
    }

    // 调用handlePreOnLineTask处理成功的任务
    EXPECT_NO_THROW(handlePreOnLineTask(reply));

    // 验证任务和节点状态
    {
        std::unique_lock<std::shared_mutex> lock(preOnLineTaskMutexMap);
        auto iter = preOnLineTask.find(reply.taskName);
        EXPECT_EQ(iter, preOnLineTask.end()); // 任务应被移除
        EXPECT_EQ(nodePreOnLine["1"], PreOnLineState::ONLINE);
    }
}
}
