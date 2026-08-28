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

#ifndef IT_ELECTION_DEFERRED_START_CASES_H
#define IT_ELECTION_DEFERRED_START_CASES_H

#include "it_cluster.h"

namespace ubse::it::tests::election {

// P0: deferred 节点初始未运行, 其余节点正常运行并收敛
void RunDeferredStartNodeNotRunningTest(ubse::it::infra::ItCluster& cluster);

// P0: StartNode 带 configOverrides 启动 deferred 节点, 校验配置写入 + 集群收敛
void RunDeferredStartNodeWithConfigOverrideTest(ubse::it::infra::ItCluster& cluster);

// P0: StopNode 优雅停止 deferred 节点 + 幂等
void RunDeferredStopNodeTest(ubse::it::infra::ItCluster& cluster);

// P0: 停止后带新配置重启 deferred 节点, 验证"修改配置 -> 重启"流程
void RunDeferredRestartNodeWithNewConfigTest(ubse::it::infra::ItCluster& cluster);

// P0: 4节点集群；3，4号节点未启动场景，集群仅有一主一备
void RunDeferredTwoNodesStartNodeNotRunningTest(ubse::it::infra::ItCluster& cluster);

// P0: 4节点集群；启动3，4号节点
void RunDeferredTwoNodesStartNodeWithConfigOverrideTest(ubse::it::infra::ItCluster& cluster);

} // namespace ubse::it::tests::election

#endif // IT_ELECTION_DEFERRED_START_CASES_H
