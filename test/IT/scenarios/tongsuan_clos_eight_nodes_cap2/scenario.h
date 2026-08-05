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

// 通算CLOS八节点场景(cluster.pod.capability=2)：启动八节点CLOS集群,每个pod含2节点(1主1备),全局共4个主
IT_DEFINE_SCENARIO(TongsuanClosEightNodesCap2Scenario,
                   MakeBuilder().Tongsuan().EightNode().MeshType(0x81)
                       .WithConfig("ubse.rpc", "cluster.pod.capability", "2")
                       .Start(cluster_))
