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

namespace ubse::mem::scheduler {

struct ScoreWeights {
    double wLatency{0.13};
    double wRegionBalance{0.13};
    double wBalance{0.53};
    double wReliability{0.13};
    double wDivideNuma{0.08};
    double wBandwidth{0.0};
    double wAffinity{0.0}; // 同平面优先权重, 仅请求显式启用时非 0

    static ScoreWeights ForBorrow()
    {
        return ScoreWeights{};
    }

    static ScoreWeights ForShare()
    {
        ScoreWeights w;
        w.wLatency = 0.12;
        w.wRegionBalance = 0.12;
        w.wBalance = 0.52;
        w.wReliability = 0.12;
        w.wDivideNuma = 0.12;
        w.wBandwidth = 0.0;
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
        w.wBandwidth = 0.0;
        return w;
    }

    static ScoreWeights ForPerformancePriority()
    {
        ScoreWeights w;
        // 带宽 0.52 + 借用可靠性 0.25 + 内存利用率 0.12 = 0.89，剩余 0.11 由时延/区域均衡/NUMA 划分分摊（0.04/0.04/0.03）
        w.wLatency = 0.04;
        w.wRegionBalance = 0.04;
        w.wBalance = 0.12;
        w.wBandwidth = 0.52;
        w.wReliability = 0.25;
        w.wDivideNuma = 0.03;
        return w;
    }
};

} // namespace ubse::mem::scheduler

#endif // UBSE_MEM_SCHEDULER_SCORE_WEIGHT_H
