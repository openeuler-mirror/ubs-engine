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
#include "tests/election/election_deferred_start_cases.h"

using ubse::it::infra::Tongsuan1dFourNodesDeferredStartScenario;

// P0-DeferredStart-NotRunning: 节点 4 延迟启动, StartCluster 后应未运行; 其余节点正常运行并收敛
TEST_F(Tongsuan1dFourNodesDeferredStartScenario, P0DeferredNodeNotRunning)
{
    ubse::it::tests::election::RunDeferredStartNodeNotRunningTest(Cluster());
}

// P0-DeferredStart-StartWithConfigOverride: 带 configOverrides 启动节点 4,
// 启动前应重新生成配置 (写入 log.level=DEBUG), 集群收敛为 1 主 + 1 备 + 2 代理
TEST_F(Tongsuan1dFourNodesDeferredStartScenario, P0StartNodeWithConfigOverride)
{
    ubse::it::tests::election::RunDeferredStartNodeWithConfigOverrideTest(Cluster());
}

// P0-DeferredStart-StopNode: 优雅停止节点 4 (正常关闭, 区别于 KillNode 的 SIGKILL)
TEST_F(Tongsuan1dFourNodesDeferredStartScenario, P0StopNode)
{
    ubse::it::tests::election::RunDeferredStopNodeTest(Cluster());
}

// P0-DeferredStart-RestartWithNewConfig: 停止后带新配置重启节点 4, 验证"修改配置 -> 重启"流程
TEST_F(Tongsuan1dFourNodesDeferredStartScenario, P0RestartNodeWithNewConfig)
{
    ubse::it::tests::election::RunDeferredRestartNodeWithNewConfigTest(Cluster());
}
