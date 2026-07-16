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

#include "ubse_mem_scheduler_divide_numa_score.h"
#include <algorithm>
#include "ubse_math_util.h"

namespace ubse::mem::scheduler {
namespace {
constexpr uint64_t HALF_BLOCK_SIZE = 64; // blockSize / 2
} // namespace

UbseResult DivideNumaScore::ScoreNodes(const std::vector<NodeInfo>& nodes, const SchedulerNodeManager& nodeInfo,
                                       const SchedulerAccountManager& account, const SchedulerRequest& request,
                                       std::vector<double>& scores)
{
    auto requestSize = request.requestSize_;
    if (requestSize == 0) {
        for (auto& s : scores) {
            s = 0.0;
        }
        RecordWarning("requestSize is 0, cannot compute divide numa");
        return UBSE_OK;
    }

    size_t idx = 0;
    for (const auto& node : nodes) {
        for (const auto& socketInfo : node.socketInfos) {
            std::vector<uint64_t> freeSizes;
            for (auto numaId : socketInfo.numaInfos) {
                auto* numaPtr = nodeInfo.GetNumaInfo(node.nodeId, numaId);
                if (numaPtr == nullptr) {
                    continue;
                }
                freeSizes.push_back(numaPtr->GetMemFreeSize());
            }
            std::sort(freeSizes.rbegin(), freeSizes.rend());
            uint64_t accumulated = 0;
            size_t numaCount = 0;
            for (auto freeSize : freeSizes) {
                if (accumulated >= requestSize) {
                    break;
                }
                uint64_t safeAdded = 0;
                if (ubse::utils::SafeAdd(accumulated, freeSize, safeAdded)) {
                    accumulated = safeAdded;
                }
                ++numaCount;
            }
            if (numaCount == 0) {
                scores[idx] = 1.0;
            } else {
                scores[idx] = static_cast<double>(numaCount) * static_cast<double>(HALF_BLOCK_SIZE) /
                              static_cast<double>(requestSize);
            }
            RecordScore(node.nodeId, std::string("socketId=") + std::to_string(socketInfo.socketId) + ", numaCount=" +
                                         std::to_string(numaCount) + ", score=" + std::to_string(scores[idx]));
            ++idx;
        }
    }
    return UBSE_OK;
}

} // namespace ubse::mem::scheduler
