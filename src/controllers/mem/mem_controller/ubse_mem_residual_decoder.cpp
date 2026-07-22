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

#include "ubse_mem_residual_decoder.h"

#include <mutex>
#include <unordered_set>
#include "ubse_error.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"

namespace ubse::mem::controller {

using namespace adapter_plugins::mti::mami;

struct MamiWithdrawKeyHash {
    size_t operator()(const UbseMamiMemWithdraw& key) const
    {
        return std::hash<uint32_t>()(key.ubpuId) ^ std::hash<uint32_t>()(key.iouId) ^ std::hash<uint32_t>()(key.marId) ^
               std::hash<uint8_t>()(key.decoderIdx);
    }
};

struct MamiWithdrawKeyEqual {
    bool operator()(const UbseMamiMemWithdraw& a, const UbseMamiMemWithdraw& b) const
    {
        return a.ubpuId == b.ubpuId && a.iouId == b.iouId && a.marId == b.marId && a.decoderIdx == b.decoderIdx;
    }
};

using ResidualDecoderSet = std::unordered_set<UbseMamiMemWithdraw, MamiWithdrawKeyHash, MamiWithdrawKeyEqual>;
static std::mutex g_residualDecoderMutex;
static ResidualDecoderSet g_residualDecoderSet;

void AddToResidualDecoderSet(uint32_t ubpuId, uint32_t iouId, uint32_t marId, uint8_t decoderIdx, uint64_t handle)
{
    UbseMamiMemWithdraw entry{ubpuId, iouId, marId, decoderIdx, handle};
    std::lock_guard<std::mutex> lock(g_residualDecoderMutex);
    g_residualDecoderSet.insert(entry);
}

uint32_t TryCleanResidualDecoderEntry(uint32_t ubpuId, uint32_t iouId, uint32_t marId, uint8_t decoderIdx)
{
    UbseMamiMemWithdraw residualEntry;
    {
        std::lock_guard<std::mutex> lock(g_residualDecoderMutex);
        UbseMamiMemWithdraw queryKey{ubpuId, iouId, marId, decoderIdx, 0ULL};
        auto it = g_residualDecoderSet.find(queryKey);
        if (it == g_residualDecoderSet.end()) {
            return UBSE_OK;
        }
        residualEntry = *it;
        g_residualDecoderSet.erase(it);
    }
    int retry = 3;
    auto res = UBSE_OK;
    while (retry > 0) {
        res = adapter_plugins::mti::UbseMtiInterface::GetInstance().DeleteDecoderEntry(residualEntry);
        if (res == UBSE_OK) {
            break;
        }
        retry--;
    }
    if (res != UBSE_OK) {
        std::lock_guard<std::mutex> lock(g_residualDecoderMutex);
        g_residualDecoderSet.insert(residualEntry);
        return res;
    }
    return UBSE_OK;
}

void RemoveFromResidualDecoderSet(uint32_t ubpuId, uint32_t iouId, uint32_t marId, uint8_t decoderIdx)
{
    UbseMamiMemWithdraw queryKey{ubpuId, iouId, marId, decoderIdx, 0ULL};
    std::lock_guard<std::mutex> lock(g_residualDecoderMutex);
    g_residualDecoderSet.erase(queryKey);
}

} // namespace ubse::mem::controller
