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

#include "ubse_mem_scheduler_borrow_bandwidth_score.h"
#include <algorithm>
#include <cstdint>
#include <vector>

#include "ubse_math_util.h"

namespace ubse::mem::scheduler {

UbseResult BorrowBandwidthScore::ScoreNodes(const std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                            const SchedulerAccountManager& account, const SchedulerRequest& request,
                                            std::vector<double>& scores)
{
    auto importNodeId = request.importNodeId_;
    uint32_t toleranceMb = nodeInfo.GetBandwidthTolerance();
    uint64_t tolerance = 0;
    if (!ubse::utils::SizeMb2Byte(toleranceMb, tolerance) || tolerance == 0) {
        tolerance = ONE_M;
    }
    uint64_t requestSize = request.requestSize_;

    // 收集各候选 (节点, socket) 的当前借出量，单位：字节
    std::vector<uint64_t> lendAmounts;
    lendAmounts.reserve(nodes.size() * 2);
    for (const auto& node : nodes) {
        for (const auto& socketInfo : node.socketInfos) {
            uint64_t lent = account.GetTotalLentToBorrower(node.nodeId, socketInfo.socketId, importNodeId);
            lendAmounts.push_back(lent);
        }
    }

    // 逐个候选模拟借入：候选 i 加上 requestSize 后，重算集合的 max' 与 min'
    // spread = max' - min'；分带值 D = 0 表示 spread < tolerance（不含等号）
    std::vector<uint64_t> spreads(lendAmounts.size(), 0);
    std::vector<uint64_t> bandIndexes(lendAmounts.size(), 0);
    uint64_t minBand = UINT64_MAX;
    uint64_t maxBand = 0;
    for (size_t i = 0; i < lendAmounts.size(); ++i) {
        uint64_t maxLend = 0;
        uint64_t minLend = UINT64_MAX;
        for (size_t j = 0; j < lendAmounts.size(); ++j) {
            uint64_t amount = lendAmounts[j];
            if (i == j) {
                uint64_t incremented = 0;
                if (!ubse::utils::SafeAdd(amount, requestSize, incremented)) {
                    incremented = UINT64_MAX; // 溢出保护
                }
                amount = incremented;
            }
            maxLend = std::max(maxLend, amount);
            minLend = std::min(minLend, amount);
        }
        uint64_t spread = maxLend - minLend;
        spreads[i] = spread;
        // 整数除法：D = spread / tolerance，D = 0 ⇔ spread < tolerance（不含等号）
        bandIndexes[i] = spread / tolerance;
        minBand = std::min(minBand, bandIndexes[i]);
        maxBand = std::max(maxBand, bandIndexes[i]);
    }

    // min-max 归一化到 [0,1]：全部候选同带时为 0；直接按百分位四舍五入保留两位小数
    size_t idx = 0;
    for (const auto& node : nodes) {
        for (const auto& socketInfo : node.socketInfos) {
            double score = 0.0;
            if (maxBand != minBand) {
                uint64_t denom = maxBand - minBand;
                uint64_t num = bandIndexes[idx] - minBand;
                uint64_t hundredths = (num * 100 + denom / 2) / denom; // 四舍五入到百分位
                score = static_cast<double>(hundredths) / 100.0;
            }
            scores[idx] = score;
            RecordScore(node.nodeId, std::string("socketId=") + std::to_string(socketInfo.socketId) +
                                         ", lendAmount=" + std::to_string(lendAmounts[idx]) + ", requestSize=" +
                                         std::to_string(requestSize) + ", spread=" + std::to_string(spreads[idx]) +
                                         ", band=" + std::to_string(bandIndexes[idx]) +
                                         ", score=" + std::to_string(score));
            ++idx;
        }
    }
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
