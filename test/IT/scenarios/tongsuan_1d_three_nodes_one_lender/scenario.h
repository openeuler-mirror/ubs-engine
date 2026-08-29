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

#ifndef TONGSUAN_1D_THREE_NODES_ONE_LENDER_SCENARIO_H
#define TONGSUAN_1D_THREE_NODES_ONE_LENDER_SCENARIO_H

#include "it_scenario_fixture.h"

/**
 * @brief 三节点单一借出节点场景。
 *
 * 节点"1"/"2"配置 [ubse.memory] isLender=false（不允许借出）且 [ubse.election] election.candidate=false；
 * 节点"3"配置 isLender=true（唯一借出节点）且 election.candidate=true。
 * 用于验证：节点"1" CLI 指定链路创建 NUMA 内存后，借出节点（lend_node）为节点"3"。
 */
IT_DEFINE_SCENARIO(Tongsuan1dThreeNodesOneLenderScenario,
                   MakeBuilder()
                       .Tongsuan()
                       .Nodes({ubse::it::infra::NodeSpec{"1", "127.0.0.2", 8082, 1},
                               ubse::it::infra::NodeSpec{"2", "127.0.0.3", 8083, 2},
                               ubse::it::infra::NodeSpec{"3", "127.0.0.4", 8084, 3}})
                       .WithNodeConfig("1", "ubse.memory", "isLender", "false")
                       .WithNodeConfig("2", "ubse.memory", "isLender", "false")
                       .WithNodeConfig("3", "ubse.memory", "isLender", "true")
                       .WithNodeConfig("1", "ubse.election", "election.candidate", "false")
                       .WithNodeConfig("2", "ubse.election", "election.candidate", "false")
                       .WithNodeConfig("3", "ubse.election", "election.candidate", "true")
                       .WithNodeConfig("1", "ubse.log", "log.level", "DEBUG")
                       .WithNodeConfig("2", "ubse.log", "log.level", "DEBUG")
                       .WithNodeConfig("3", "ubse.log", "log.level", "DEBUG")
                       .Start(cluster_))

#endif // TONGSUAN_1D_THREE_NODES_ONE_LENDER_SCENARIO_H
