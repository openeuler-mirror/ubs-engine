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

#ifndef UBSE_MEM_SCHEDULER_SUB_HEALTH_MODE_H
#define UBSE_MEM_SCHEDULER_SUB_HEALTH_MODE_H

#include <string>

namespace ubse::mem::scheduler {

// 链路亚健康感知模式，由 ubse.conf 的 subHealthPenaltyEnabled + subHealth.strategy 在启动时派生。

enum class SubHealthMode {
    DISABLED,
    EXCLUDE,
    WEIGHT,
};

// 由 enable + strategy 派生 SubHealthMode：enable=false → DISABLED；
// enable=true 时 exclude → EXCLUDE，其余（含非法值）→ WEIGHT。
inline SubHealthMode ResolveSubHealthMode(bool enable, const std::string& strategy)
{
    if (!enable) {
        return SubHealthMode::DISABLED;
    }
    if (strategy == "exclude") {
        return SubHealthMode::EXCLUDE;
    }
    return SubHealthMode::WEIGHT;
}

// 从 ubse.conf 读取配置并派生 SubHealthMode；读取失败时返回 DISABLED（fail-safe）。
SubHealthMode ResolveSubHealthModeFromConfig();

} // namespace ubse::mem::scheduler

#endif // UBSE_MEM_SCHEDULER_SUB_HEALTH_MODE_H
