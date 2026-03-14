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

#include <cmath>
#include <cstring>
#include <iomanip>
#include <set>
#include "mem_pool_config.h"
#include "mem_pool_strategy.h"
#include "mem_pool_strategy_impl.h"
#include "ubse_logger.h"

namespace tc::rs::mem {
UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");
BResult MemPoolConfig::NormalizeStrategy(StrategyParam &param)
{
    // 借用决策参数归一化
    float borrowSum = param.borrowParam.wLatencyCost + param.borrowParam.wRegionBalanceCost +
                      param.borrowParam.wBalanceCost + param.borrowParam.wReliabilityCost +
                      param.borrowParam.wDivideNumaCost;
    // 共享决策参数归一化
    float shareSum = param.shareParam.wLatencyCost + param.shareParam.wRegionBalanceCost +
                     param.shareParam.wBalanceCost + param.shareParam.wReliabilityCost +
                     param.shareParam.wDivideNumaCost;
    if (std::fabs(borrowSum) < 1e-9 || std::fabs(shareSum) < 1e-9) {
        UBSE_LOG_ERROR << "BorrowSum or shareSum is zero, division by zero.";
        throw std::invalid_argument("Error! NormalizeStrategy is false while borrowSum or shareSum is zero.");
    }

    param.borrowParam.wLatencyCost /= borrowSum;
    param.borrowParam.wRegionBalanceCost /= borrowSum;
    param.borrowParam.wBalanceCost /= borrowSum;
    param.borrowParam.wReliabilityCost /= borrowSum;
    param.borrowParam.wDivideNumaCost /= borrowSum;

    param.shareParam.wLatencyCost /= shareSum;
    param.shareParam.wRegionBalanceCost /= shareSum;
    param.shareParam.wBalanceCost /= shareSum;
    param.shareParam.wReliabilityCost /= shareSum;
    param.shareParam.wDivideNumaCost /= shareSum;

    return UBSE_OK;
}

void MemPoolConfig::MapTopologyToIndices()
{
    for (auto &i : memMeshLoc2HostIdx) {
        for (int &j : i) {
            j = -1;
        }
    }
    // 注: 使用hostMeshLocs数组时遍历全部元素, 若第i个元素的x==-1 && y==-1, 则不存在hostId为i的节点
    for (int i = 0; i < NUM_HOSTS; i++) {
        int8_t row = memStaticParam.hostMeshLocs[i].y;
        int8_t col = memStaticParam.hostMeshLocs[i].x;
        if (row >= 0 && col >= 0 && row < NUM_HOST_PER_COL && col < NUM_HOST_PER_ROW) {
            memMeshLoc2HostIdx[row][col] = i;
        }
    }
}
} // namespace tc::rs::mem