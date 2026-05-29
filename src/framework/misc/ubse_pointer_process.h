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

#ifndef UBSE_POINTER_PROCESS_H
#define UBSE_POINTER_PROCESS_H

#include <cstddef>
#include <memory>
#include <type_traits> // For std::is_void_v
#include <functional>

template <typename T>
void SafeFree(T& ptr)
{
    if (ptr) {
        free(ptr);
        ptr = nullptr;
    }
}

template <typename T>
void SafeDelete(T& ptr)
{
    if (ptr) {
        delete ptr;
        ptr = nullptr;
    }
}

template <typename T>
void SafeDeleteArray(T*& ptr, size_t ptrLen = 1)
{
    static_assert(!std::is_void_v<T>, "Cannot delete array of void");
    if (ptr && ptrLen != 0) {
        delete[] ptr;
        ptr = nullptr;
    }
}

template <typename T, typename... Args>
std::shared_ptr<T> SafeMakeShared(Args&&... args)
{
    T* raw = new (std::nothrow) T(std::forward<Args>(args)...);
    if (!raw) {
        return nullptr;
    }
    return std::shared_ptr<T>(raw);
}

template <typename... Args>
std::unique_ptr<uint8_t[]> SafeMakeUnique(size_t size, Args&&... args)
{
    auto raw = new (std::nothrow) uint8_t[size]{std::forward<Args>(args)...};
    if (!raw) {
        return nullptr;
    }
    return std::unique_ptr<uint8_t[]>(raw);
}

template <typename T, typename... Args>
std::unique_ptr<T> SafeMakeUnique(Args&&... args)
{
    T* raw = new (std::nothrow) T(std::forward<Args>(args)...);
    if (!raw) {
        return nullptr;
    }
    return std::unique_ptr<T>(raw);
}

template <typename T, typename... Args>
std::unique_ptr<T, std::function<void(T *)>> SafeMakeUniqueWithFreeFunc(const std::function<void(T *)> &freeFunc,
                                                                        Args &&...args)
{
    T *raw = new (std::nothrow) T(std::forward<Args>(args)...);
    if (!raw) {
        return nullptr;
    }
    return std::unique_ptr<T, decltype(freeFunc)>(raw, freeFunc);
}

template <typename... Args>
std::shared_ptr<uint8_t[]> SafeMakeShared(size_t size, Args&&... args)
{
    auto raw = new (std::nothrow) uint8_t[size]{std::forward<Args>(args)...};
    if (!raw) {
        return nullptr;
    }
    return std::shared_ptr<uint8_t[]>(raw);
}

#endif // UBSE_POINTER_PROCESS_H
