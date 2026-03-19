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

#include "ubse_request_id_util.h"

namespace ubse::utils {
const std::chrono::steady_clock::time_point UbseRequestIdUtil::PROGRAM_START_TIME = std::chrono::steady_clock::now();

UbseRequestIdUtil::UbseRequestIdUtil(UbseRequestType requestType) : requestType_(requestType), counter_(0) {}
uint64_t UbseRequestIdUtil::GenerateRequestId(uint8_t slotId)
{
    // 使用时间戳和计数器的组合
    uint32_t currentTs = GetCurrentTimestamp();
    uint32_t lastTs = lastTimestamp_.load(std::memory_order_relaxed);
    uint16_t currentCounter;
    if (currentTs == lastTs) {
        // 同一毫秒内，递增计数器
        currentCounter = ++counter_;

        // 检查计数器是否溢出
        if (currentCounter == 0) {
            // 计数器溢出，等待到下一毫秒
            while (GetCurrentTimestamp() == currentTs) {
                std::this_thread::yield();
            }
            currentTs = GetCurrentTimestamp();
            counter_.store(1, std::memory_order_relaxed);
            lastTimestamp_.store(currentTs, std::memory_order_relaxed);
            currentCounter = 1;
        }
    } else {
        // 新的毫秒，重置计数器
        lastTimestamp_.store(currentTs, std::memory_order_relaxed);
        counter_.store(1, std::memory_order_relaxed);
        currentCounter = 1;
    }

    // 组合所有部分形成最终ID
    return (static_cast<uint64_t>(requestType_) << REQUEST_TYPE_SHIFT) |
           (static_cast<uint64_t>(slotId) << SLOT_ID_SHIFT) | (static_cast<uint64_t>(currentTs) << TIME_STAMP_SHIFT) |
           currentCounter;
}

UbseRequestType UbseRequestIdUtil::ParseRequestType(uint64_t requestId)

{
    uint32_t requestTypeMask = (1 << REQUEST_TYPE_BITS) - 1;
    return static_cast<UbseRequestType>((requestId >> REQUEST_TYPE_SHIFT) & requestTypeMask);
}

uint32_t UbseRequestIdUtil::GetCurrentTimestamp()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - PROGRAM_START_TIME);
    return static_cast<uint32_t>(elapsed.count());
}
} // namespace ubse::utils