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

#ifndef UBSE_SEQUENCE_COUNTER_H
#define UBSE_SEQUENCE_COUNTER_H

#include <atomic>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace ubse::framework::misc {

/**
 * @brief 线程安全的序列号计数器
 * 提供线程安全的序列号生成功能，支持：
 * 1. 获取下一个序列号
 * 2. 重置序列号
 * 3. 获取当前序列号
 * 4. 溢出处理
 */
template <typename T>
class UbseSequenceCounter {
public:
    /**
     * @brief 构造函数
     * @param initialValue 初始值，默认为0
     * @param maxValue 最大值，默认为T的最大值
     */
    explicit UbseSequenceCounter(T initialValue = 0, T maxValue = std::numeric_limits<T>::max())
        : counter_(initialValue),
          maxValue_(maxValue),
          initialValue_(initialValue)
    {
        if (initialValue > maxValue) {
            throw std::invalid_argument("Initial value cannot be greater than max value");
        }
    }

    /**
     * @brief 禁止拷贝构造
     */
    UbseSequenceCounter(const UbseSequenceCounter &) = delete;

    /**
     * @brief 禁止拷贝赋值
     */
    UbseSequenceCounter &operator=(const UbseSequenceCounter &) = delete;

    /**
     * @brief 允许移动构造
     */
    UbseSequenceCounter(UbseSequenceCounter &&other) noexcept
        : counter_(other.counter_.load()),
          maxValue_(other.maxValue_),
          initialValue_(other.initialValue_)
    {
    }

    /**
     * @brief 允许移动赋值
     */
    UbseSequenceCounter &operator=(UbseSequenceCounter &&other) noexcept
    {
        if (this != &other) {
            counter_.store(other.counter_.load());
            maxValue_ = other.maxValue_;
            initialValue_ = other.initialValue_;
        }
        return *this;
    }

    /**
     * @brief 安全获取下一个序列号（溢出时重置）
     * @return 下一个序列号
     */
    T GetNextSafe()
    {
        T current = counter_.load(std::memory_order_relaxed);
        T next;

        do {
            next = (current >= maxValue_) ? initialValue_ : current + 1;
        } while (!counter_.compare_exchange_weak(current, next, std::memory_order_release, std::memory_order_relaxed));

        return next;
    }

    /**
     * @brief 获取当前序列号（不递增）
     * @return 当前序列号
     */
    T GetCurrent() const
    {
        return counter_.load(std::memory_order_acquire);
    }

    /**
     * @brief 重置序列号为初始值
     */
    void Reset()
    {
        counter_.store(initialValue_, std::memory_order_release);
    }

    /**
     * @brief 获取最大值
     * @return 最大值
     */
    T GetMaxValue() const
    {
        return maxValue_;
    }

    /**
     * @brief 获取初始值
     * @return 初始值
     */
    T GetInitialValue() const
    {
        return initialValue_;
    }

private:
    std::atomic<T> counter_;
    T maxValue_;
    T initialValue_;
};

} // namespace ubse::framework::misc

#endif // UBSE_SEQUENCE_COUNTER_H