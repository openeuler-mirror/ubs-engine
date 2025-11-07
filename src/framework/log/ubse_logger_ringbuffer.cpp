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

#include <iostream>
#include "ubse_logger_ringbuffer.h"

namespace ubse::log {
RingBuffer::RingBuffer(uint32_t size) : size(size), right(0)
{
    buffer.resize(size);
}

RingBuffer::~RingBuffer()
{
    buffer.clear();
}

bool RingBuffer::IsEmpty() const
{
    return left == right;
}

void RingBuffer::Push(UbseLoggerEntry &&loggerEntry)
{
    uint32_t writeIndex = right.fetch_add(1, std::memory_order_relaxed);
    if (writeIndex < size) {
        buffer[writeIndex] = std::move(loggerEntry);
        bool expected = true;
        if (bufferFullWarned.compare_exchange_strong(expected, false, std::memory_order_relaxed)) {
            std::cout << "Log buffer recovered." << std::endl;
        }
    } else {
        bool expected = false;
        if (bufferFullWarned.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
            std::cout << "Log buffer is full, dropping logs." << std::endl;
        }
        right--;
    }
}

void RingBuffer::Pop(UbseLoggerEntry &loggerEntry)
{
    loggerEntry = std::move(buffer[left]);
    left++;
}

void LogBuffer::Push(UbseLoggerEntry &&loggerEntry)
{
    // 需要支持多线程同时写入
    std::shared_lock<std::shared_mutex> lock(mtx);
    if (stop) {
        return;
    }
    writeBuffer.Push(std::move(loggerEntry));
}

bool LogBuffer::Pop(UbseLoggerEntry &loggerEntry)
{
    // 读为单线程操作
    if (readBuffer.IsEmpty()) {
        if (writeBuffer.IsEmpty()) {
            return false;
        } else {
            Swap();
        }
    }
    readBuffer.Pop(loggerEntry);
    return true;
}

void LogBuffer::Swap()
{
    std::unique_lock<std::shared_mutex> lock(mtx);
    std::swap(readBuffer.buffer, writeBuffer.buffer);
    readBuffer.left.store(0);
    readBuffer.right.store(writeBuffer.right.load());
    writeBuffer.left.store(0);
    writeBuffer.right.store(0);
}
} // namespace ubse::log