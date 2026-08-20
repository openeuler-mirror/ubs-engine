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

#ifndef UBSE_MEM_SCHEDULER_SCORE_WEIGHT_H
#define UBSE_MEM_SCHEDULER_SCORE_WEIGHT_H

#include <cstdint>

#include "ubse_mem_scheduler_sub_health_mode.h"

namespace ubse::mem::scheduler {

struct ScoreWeights {
    double wLatency{0.13};
    double wRegionBalance{0.13};
    double wBalance{0.53};
    double wReliability{0.13};
    double wDivideNuma{0.08};
    double wSubHealth{0.0};

    // 历史兼容：等价于 ForBorrow(SubHealthMode::DISABLED)
    static ScoreWeights ForBorrow()
    {
        return ForBorrow(SubHealthMode::DISABLED);
    }

    // 借用权重。亚健康权重仅在 WEIGHT 模式下启用（wSubHealth=0.10），
    // 并等额扣减 wBalance（0.53-0.10=0.43）。DISABLED/EXCLUDE 模式下 wSubHealth=0.0，
    // 与历史版本完全一致。归一化合计恒为 1.00。
    static ScoreWeights ForBorrow(SubHealthMode mode)
    {
        ScoreWeights w{};
        if (mode == SubHealthMode::WEIGHT) {
            w.wBalance = 0.43;
            w.wSubHealth = 0.10;
        }
        return w;
    }

    static ScoreWeights ForShare()
    {
        ScoreWeights w;
        w.wLatency = 0.12;
        w.wRegionBalance = 0.12;
        w.wBalance = 0.52;
        w.wReliability = 0.12;
        w.wDivideNuma = 0.12;
        return w;
    }

    static ScoreWeights ForLenderBalance()
    {
        ScoreWeights w;
        w.wLatency = 0.13;
        w.wRegionBalance = 0.13;
        w.wBalance = 0.13;
        w.wReliability = 0.53;
        w.wDivideNuma = 0.08;
        return w;
    }
};

} // namespace ubse::mem::scheduler

#endif // UBSE_MEM_SCHEDULER_SCORE_WEIGHT_H
