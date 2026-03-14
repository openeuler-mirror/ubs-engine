/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBS_ENGINE_MOCK_INIT_H
#define UBS_ENGINE_MOCK_INIT_H

#include "mem_pool_strategy.h"

using namespace tc::rs::mem;

/** 1630代际完整拓扑, 内存池化算法参数初始化 */
StrategyParam GetRs1630DefaultParam();

/** 1630代际不完整拓扑 (2host 7numa), 内存池化算法参数初始化 */
StrategyParam GetRs1630DefaultParamMissingNuma();

/** 1650代际, 内存池化算法参数初始化 */
StrategyParam GetRs1650DefaultParam(int numHosts);


StrategyParam GetRs1D8DefaultParam(int numHosts);

#endif // UBS_ENGINE_MOCK_INIT_H
