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

#ifndef RACK_MANAGER_UBSE_REQUEST_ID_UTIL_H
#define RACK_MANAGER_UBSE_REQUEST_ID_UTIL_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace rack::utils {
// 定义位偏移量和掩码
constexpr uint8_t REQUEST_TYPE_BITS = 8;
constexpr uint8_t SLOT_ID_BITS = 8;
constexpr uint8_t TIME_STAMP_BITS = 32;
constexpr uint8_t COUNT_BITS = 16;

constexpr uint8_t REQUEST_TYPE_SHIFT = COUNT_BITS + TIME_STAMP_BITS + SLOT_ID_BITS;
constexpr uint8_t SLOT_ID_SHIFT = COUNT_BITS + TIME_STAMP_BITS;
constexpr uint8_t TIME_STAMP_SHIFT = COUNT_BITS;

enum class UbseRequestType : uint8_t {
    SDK_REQUEST = 1,
    INNER_REQUEST = 2,
};

class UbseRequestIdUtil {
public:
    explicit UbseRequestIdUtil(UbseRequestType requestType);

    uint64_t GenerateRequestId(uint8_t slotId);

    static UbseRequestType ParseRequestType(uint64_t requestId);

private:
    const UbseRequestType requestType;
    std::atomic<uint32_t> lastTimestamp{};
    std::atomic<uint16_t> counter;
    static const std::chrono::steady_clock::time_point PROGRAM_START_TIME;

    // 获取当前时间戳（毫秒）
    static uint32_t GetCurrentTimestamp();
};
} // namespace rack::utils
#endif // RACK_MANAGER_UBSE_REQUEST_ID_UTIL_H
