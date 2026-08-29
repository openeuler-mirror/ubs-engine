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

#ifndef TONGSUAN_1D_TWO_NODES_LENDER_FALSE_SCENARIO_H
#define TONGSUAN_1D_TWO_NODES_LENDER_FALSE_SCENARIO_H

#include "it_scenario_fixture.h"

/**
 * @brief 双节点 isLender 配置场景（false）。
 *
 * 节点"2"配置 [ubse.memory] isLender=false（不允许借出内存），
 * 节点"1"保持默认 isLender=true。
 * 用于验证 CLI 查询 display memory -t config 与节点配置一致（不涉及重启）。
 */
IT_DEFINE_SCENARIO(Tongsuan1dTwoNodesLenderFalseScenario, MakeBuilder()
                                                              .Tongsuan()
                                                              .TwoNode()
                                                              .WithNodeConfig("2", "ubse.memory", "isLender", "false")
                                                              .WithNodeConfig("1", "ubse.log", "log.level", "DEBUG")
                                                              .WithNodeConfig("2", "ubse.log", "log.level", "DEBUG")
                                                              .Start(cluster_))

#endif // TONGSUAN_1D_TWO_NODES_LENDER_FALSE_SCENARIO_H
