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

#ifndef UBSE_LOGGER_BUFFER_H
#define UBSE_LOGGER_BUFFER_H

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "ubse_logger.h"

namespace ubse::log {
class RingBuffer {
public:
    explicit RingBuffer(uint32_t size);

    ~RingBuffer();

    bool IsEmpty() const;

    void Push(UbseLoggerEntry &&loggerEntry);
    void Pop(UbseLoggerEntry &loggerEntry);

    RingBuffer(RingBuffer const &) = delete;
    RingBuffer &operator=(RingBuffer const &) = delete;

public:
    uint32_t size;
    std::vector<UbseLoggerEntry> buffer{};
    std::atomic<uint32_t> left{};
    std::atomic<uint32_t> right{};
    std::atomic<bool> bufferFullWarned{false};
};

class LogBuffer {
public:
    explicit LogBuffer(uint32_t size) : readBuffer(size), writeBuffer(size) {}

    void Push(UbseLoggerEntry &&loggerEntry);
    bool Pop(UbseLoggerEntry &loggerEntry);
    void Swap();

public:
    std::shared_mutex mtx{};
    bool stop = false;

private:
    RingBuffer readBuffer;
    RingBuffer writeBuffer;
};
} // namespace ubse::log
#endif // UBSE_LOGGER_BUFFER_H
