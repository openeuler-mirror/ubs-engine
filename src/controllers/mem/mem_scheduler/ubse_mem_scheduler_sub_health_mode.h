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

// 链路亚健康感知模式。由 ubse.conf 的 subHealthPenaltyEnabled + subHealth.strategy 在进程启动时派生。

enum class SubHealthMode {
    DISABLED,
    EXCLUDE,
    WEIGHT,
};

// 由 enable + strategy 两参数派生 SubHealthMode（纯函数，无副作用，可独立单测）。
//   enable=false → DISABLED（strategy 被忽略）
//   enable=true  → exclude → EXCLUDE；weight 或任何非法值 → WEIGHT（默认回退）
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

// 从 ubse.conf 读取 subHealthPenaltyEnabled 与 subHealth.strategy 并派生 SubHealthMode。
// 配置缺失或读取失败时返回 DISABLED（fail-safe，行为与历史版本一致）。
SubHealthMode ResolveSubHealthModeFromConfig();

} // namespace ubse::mem::scheduler

#endif // UBSE_MEM_SCHEDULER_SUB_HEALTH_MODE_H
