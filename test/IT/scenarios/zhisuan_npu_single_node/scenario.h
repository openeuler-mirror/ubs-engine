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

#include "it_scenario_fixture.h"

// 智算NPU单节点场景：启动单节点AI集群（禁用选举和Mock插件），验证NPU设备分配/释放/查询
IT_DEFINE_SCENARIO(ZhisuanNpuSingleNodeScenario,
                   MakeBuilder().SingleNode().SceneType("ai").NoMockPlugin().StartNoElection(cluster_))
