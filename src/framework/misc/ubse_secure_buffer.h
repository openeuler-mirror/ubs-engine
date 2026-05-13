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
#ifndef UBSE_SAFE_ARRAY_H
#define UBSE_SAFE_ARRAY_H
#include <string>
#include <vector>

namespace ubse::utils {

inline void SecureZeroMemory(void* ptr, std::size_t len)
{
    if (!ptr || len == 0) {
        return;
    }

    // 方法：使用 volatile 指针遍历，确保写入操作不会被编译器优化掉
    volatile auto* p = static_cast<volatile unsigned char*>(ptr);
    while (len--) {
        *p++ = 0;
    }
}

template <class T>
class SecureAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    SecureAllocator() = default;
    template <typename U>
    SecureAllocator(const SecureAllocator<U>&) noexcept
    {
    }

    // 分配内存
    pointer allocate(size_type n)
    {
        if (n == 0) {
            return nullptr;
        }
        if (n > std::size_t(-1) / sizeof(T)) {
            throw std::bad_alloc();
        }

        // 分配原始内存
        auto ptr = static_cast<pointer>(::operator new(n * sizeof(T), std::nothrow));
        SecureZeroMemory(ptr, n * sizeof(T));
        return ptr;
    }

    // 释放内存
    void deallocate(pointer p, size_type n) noexcept
    {
        if (p == nullptr) {
            return;
        }

        SecureZeroMemory(p, n * sizeof(T));
        ::operator delete(p);
    }
};

struct SecureBuffer {
    // 使用自定义分配器的 vector 存储字符
    using SecureData = std::vector<char, SecureAllocator<char>>;
    SecureData data;

    SecureBuffer() = default;

    // 从 std::string 或 char* 构造
    explicit SecureBuffer(const std::string& pwd)
    {
        if (!pwd.empty()) {
            data.resize(pwd.size() + 1); // 预分配，减少扩容导致的潜在复制
            std::copy(pwd.begin(), pwd.end(), data.begin());
            data[pwd.size()] = '\0';
        }
    }

    const char* c_str() const
    {
        return data.empty() ? "" : data.data();
    }

    std::size_t size() const
    {
        return data.empty() ? 0 : data.size() - 1;
    }

    // 显式清理方法（如果需要提前清除）
    void Wipe()
    {
        if (!data.empty()) {
            SecureZeroMemory(data.data(), data.size());
            data.clear();
        }
    }

    // 析构时自动清理（RAII）
    ~SecureBuffer()
    {
        Wipe();
    }

    // 禁用拷贝构造和赋值，防止意外复制敏感数据到新的内存位置
    SecureBuffer(const SecureBuffer&) = delete;
    SecureBuffer& operator=(const SecureBuffer&) = delete;

    // 允许移动，但移动后源对象必须处于已清理状态
    SecureBuffer(SecureBuffer&& other) noexcept : data(std::move(other.data)) {}

    SecureBuffer& operator=(SecureBuffer&& other) noexcept
    {
        if (this != &other) {
            Wipe(); // 清理当前数据
            data = std::move(other.data);
        }
        return *this;
    }
};
} // namespace ubse::utils

#endif