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

#ifndef TONGSUAN_1D_FOUR_NODES_DEFERRED_START_SCENARIO_H
#define TONGSUAN_1D_FOUR_NODES_DEFERRED_START_SCENARIO_H

#include "it_scenario_fixture.h"

/**
 * @brief 四节点延迟启动场景。
 *
 * 使用 DeferNodes({"4"}) 延迟启动节点 4:
 *   - StartCluster 阶段只启动节点 1/2/3 并等待选举收敛 (1 主 + 1 备 + 1 代理);
 *   - 节点 4 的配置文件在 Phase 0 已生成, 但 daemon 未启动.
 *
 * 测试用例验证:
 *   - P0: deferred 节点初始未运行, 其余节点正常运行
 *   - P0: StartNode 带 configOverrides 启动节点 4 (启动前重新生成配置), 集群收敛为 1 主 + 1 备 + 2 代理
 *   - P0: StopNode 优雅停止节点 4
 *   - P0: 停止后带新配置重启节点 4
 */
IT_DEFINE_SCENARIO(Tongsuan1dFourNodesDeferredStartScenario,
                   MakeBuilder().Tongsuan().FourNode().DeferNodes({"4"}).Start(cluster_))

#endif // TONGSUAN_1D_FOUR_NODES_DEFERRED_START_SCENARIO_H
