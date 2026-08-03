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

#ifndef TONGSUAN_1D_TWO_NODES_RUNTIME_FAULT_SCENARIO_H
#define TONGSUAN_1D_TWO_NODES_RUNTIME_FAULT_SCENARIO_H

#include "it_scenario_fixture.h"

/**
 * @brief 双节点 RUNTIME OBMM/COM 故障场景。
 *
 * OBMM 故障通过共享内存（ObmmStubControl）运行时注入，无需重启节点。
 * HCOM 故障通过共享内存（ComStubControl）运行时注入，无需重启节点。
 *
 * 场景间完全隔离：TearDownTestSuite 停止集群并销毁共享内存。
 */
IT_DEFINE_SCENARIO(Tongsuan1dTwoNodesRunTimeFaultScenario, MakeBuilder()
                                                               .Tongsuan()
                                                               .TwoNode()
                                                               .WithNodeConfig("1", "ubse.log", "log.level", "DEBUG")
                                                               .WithNodeConfig("2", "ubse.log", "log.level", "DEBUG")
                                                               .Start(cluster_))

#endif // TONGSUAN_1D_TWO_NODES_RUNTIME_FAULT_SCENARIO_H
