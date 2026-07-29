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

#ifndef TONGSUAN_1D_TWO_NODES_UB_FEATURE_FAULT_SCENARIO_H
#define TONGSUAN_1D_TWO_NODES_UB_FEATURE_FAULT_SCENARIO_H

#include "it_scenario_fixture.h"
#include "it_ub_fault_cluster.h"

/**
 * @brief 双节点 UB 特性故障场景。
 *
 * 仅声明集群启动方式（通过 IT_DEFINE_SCENARIO 宏生成 SetUpTestSuite /
 * TearDownTestSuite / Cluster 等样板）。UB 特性故障状态机由 UbFaultCluster
 * helper 提供，测试用例通过 Fault() 访问。复用规则、状态机约束详见 it_ub_fault_machine.h。
 */
IT_DEFINE_SCENARIO(Tongsuan1dTwoNodesUbFeatureFaultScenario, MakeBuilder()
                                                                 .Tongsuan()
                                                                 .TwoNode()
                                                                 .WithNodeConfig("1", "ubse.log", "log.level", "DEBUG")
                                                                 .WithNodeConfig("2", "ubse.log", "log.level", "DEBUG")
                                                                 .Start(cluster_))

namespace ubse::it::infra {
/**
 * @brief 本场景共享的 UB 故障状态机。
 *
 * Meyers's singleton：首次调用时引用 Cluster() 构造，此后跨测试用例复用，
 * 最大化同维度故障态的复用（不重启节点）。
 */
inline UbFaultCluster& Fault()
{
    static UbFaultCluster inst{Tongsuan1dTwoNodesUbFeatureFaultScenario::Cluster()};
    return inst;
}
} // namespace ubse::it::infra

#endif // TONGSUAN_1D_TWO_NODES_UB_FEATURE_FAULT_SCENARIO_H
