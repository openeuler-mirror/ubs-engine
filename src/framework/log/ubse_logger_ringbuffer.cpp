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

#include "ubse_logger_ringbuffer.h"
#include <iostream>

namespace ubse::log {
RingBuffer::RingBuffer(uint32_t size) : size_(size), right_(0)
{
    buffer_.resize(size);
}

RingBuffer::~RingBuffer()
{
    buffer_.clear();
}

bool RingBuffer::IsEmpty() const
{
    return left_ == right_;
}

void RingBuffer::Push(UbseLoggerEntry&& loggerEntry)
{
    uint32_t writeIndex = right_.fetch_add(1, std::memory_order_relaxed);
    if (writeIndex < size_) {
        buffer_[writeIndex] = std::move(loggerEntry);
        bool expected = true;
        if (bufferFullWarned_.compare_exchange_strong(expected, false, std::memory_order_relaxed)) {
            std::cout << "Log buffer recovered." << std::endl;
        }
    } else {
        bool expected = false;
        if (bufferFullWarned_.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
            std::cout << "Log buffer is full, dropping logs." << std::endl;
        }
        right_--;
    }
}

void RingBuffer::Pop(UbseLoggerEntry& loggerEntry)
{
    loggerEntry = std::move(buffer_[left_]);
    left_++;
}

void LogBuffer::Push(UbseLoggerEntry&& loggerEntry)
{
    // 需要支持多线程同时写入
    std::shared_lock<std::shared_mutex> lock(mtx_);
    if (stop_) {
        return;
    }
    writeBuffer_.Push(std::move(loggerEntry));
}

bool LogBuffer::Pop(UbseLoggerEntry& loggerEntry)
{
    // 读为单线程操作
    if (readBuffer_.IsEmpty()) {
        if (writeBuffer_.IsEmpty()) {
            return false;
        } else {
            Swap();
        }
    }
    readBuffer_.Pop(loggerEntry);
    return true;
}

void LogBuffer::Swap()
{
    std::unique_lock<std::shared_mutex> lock(mtx_);
    std::swap(readBuffer_.buffer_, writeBuffer_.buffer_);
    readBuffer_.left_.store(0);
    readBuffer_.right_.store(writeBuffer_.right_.load());
    writeBuffer_.left_.store(0);
    writeBuffer_.right_.store(0);
}
} // namespace ubse::log