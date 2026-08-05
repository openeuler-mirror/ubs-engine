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

// 通算1D双节点全互联场景 — SEI 降级关闭
// 配置 sei.enable=false，验证借用/归还全程 SEI 文件不变
IT_DEFINE_SCENARIO(Tongsuan1dTwoNodesSeiDisabledScenario, MakeBuilder()
                                                              .Tongsuan()
                                                              .TwoNode()
                                                              .WithNodeConfig("1", "ubse.log", "log.level", "DEBUG")
                                                              .WithNodeConfig("2", "ubse.log", "log.level", "DEBUG")
                                                              .WithNodeConfig("2", "ubse.memory", "sei.enable", "false")
                                                              .Start(cluster_))
