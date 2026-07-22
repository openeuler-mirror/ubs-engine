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

#include "scenario.h"
#include "tests/election/election_cases.h"
#include "tests/fault/fault_cases.h"
#include "tests/smoke/smoke_cases.h"
#include "tests/urma/urma_qos_cases.h"

using ubse::it::infra::Tongsuan1dFullMeshSingleNodeScenario;

// 冒烟测试：验证单节点启动后进程存活
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, SmokeStartupAndSurvive)
{
    ubse::it::tests::smoke::RunStartupTest(Cluster());
}

// 选举测试：验证单节点集群中节点"1"成为主节点
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, ElectionConvergence)
{
    ubse::it::tests::election::RunSingleNodeElectionTest(Cluster());
}

// URMA QoS 创建单优先级测试：创建优先级0带宽100Gbps配置
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, UrmaQosCreateSinglePriority)
{
    ubse::it::tests::urma_qos::RunQosCreateSinglePriorityTest(Cluster());
}

// URMA QoS 创建+查询双优先级测试
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, UrmaQosCreateAndQuery)
{
    ubse::it::tests::urma_qos::RunQosCreateAndQueryTest(Cluster());
}

// URMA QoS 全生命周期测试：创建→查询→删除→查询空
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, UrmaQosFullLifecycle)
{
    ubse::it::tests::urma_qos::RunQosCreateQueryDeleteTest(Cluster());
}

// ==================== CLI urma-qos 测试 ====================

TEST_F(Tongsuan1dFullMeshSingleNodeScenario, CliUrmaQosCreateSuccess)
{
    ubse::it::tests::urma_qos::RunCliCreateSuccessTest(Cluster());
}

TEST_F(Tongsuan1dFullMeshSingleNodeScenario, CliUrmaQosCreateDualPri)
{
    ubse::it::tests::urma_qos::RunCliCreateDualPriTest(Cluster());
}

TEST_F(Tongsuan1dFullMeshSingleNodeScenario, CliUrmaQosCreateMissingParams)
{
    ubse::it::tests::urma_qos::RunCliCreateMissingParamsTest(Cluster());
}

TEST_F(Tongsuan1dFullMeshSingleNodeScenario, CliUrmaQosCreateInvalidPri)
{
    ubse::it::tests::urma_qos::RunCliCreateInvalidPriTest(Cluster());
}

TEST_F(Tongsuan1dFullMeshSingleNodeScenario, CliUrmaQosCreateDuplicatePri)
{
    ubse::it::tests::urma_qos::RunCliCreateDuplicatePriTest(Cluster());
}

TEST_F(Tongsuan1dFullMeshSingleNodeScenario, CliUrmaQosCreateCountExceed)
{
    ubse::it::tests::urma_qos::RunCliCreateCountExceedTest(Cluster());
}

TEST_F(Tongsuan1dFullMeshSingleNodeScenario, CliUrmaQosCreateMismatch)
{
    ubse::it::tests::urma_qos::RunCliCreateMismatchTest(Cluster());
}

TEST_F(Tongsuan1dFullMeshSingleNodeScenario, CliUrmaQosCreateZeroBandwidth)
{
    ubse::it::tests::urma_qos::RunCliCreateZeroBandwidthTest(Cluster());
}

TEST_F(Tongsuan1dFullMeshSingleNodeScenario, CliUrmaQosDeleteSuccess)
{
    ubse::it::tests::urma_qos::RunCliDeleteSuccessTest(Cluster());
}

TEST_F(Tongsuan1dFullMeshSingleNodeScenario, CliUrmaQosDisplayEmpty)
{
    ubse::it::tests::urma_qos::RunCliDisplayEmptyTest(Cluster());
}

TEST_F(Tongsuan1dFullMeshSingleNodeScenario, CliUrmaQosFullLifecycle)
{
    ubse::it::tests::urma_qos::RunCliFullLifecycleTest(Cluster());
}

// 单节点BMC故障：注入BMC下电事件，验证单节点场景下RAS直接ack成功
TEST_F(Tongsuan1dFullMeshSingleNodeScenario, BmcFaultSingleNode)
{
    ubse::it::tests::fault::RunBmcFaultSingleNodeTest(Cluster(), "1");
}
