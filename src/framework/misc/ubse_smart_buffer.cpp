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

#include "ubse_smart_buffer.h"

#include <securec.h> // for memcpy_s

#include "ubse_pointer_process.h"

namespace ubse::utils {
struct UbseSmartBuffer::Impl {
    std::shared_ptr<uint8_t[]> buffer;
    size_t length = 0;

    // 原始构造函数（保持兼容性）
    Impl(uint8_t *data, size_t len, std::function<void(uint8_t *)> d)
        : buffer(data, std::move(d)), // 移动 d 到 shared_ptr 的删除器，避免拷贝
          length(len)
    {
    }

    // 从 unique_ptr<uint8_t[]> 构造
    Impl(std::unique_ptr<uint8_t[]> data, size_t len)
        : buffer(data.release(), [](uint8_t *p) { delete[] p; }), // 接管所有权 + 正确删除器
          length(len)
    {
    }

    /* *
     * @brief 从源缓冲区深拷贝创建一个新的 Impl 对象，并可选地使用自定义的 deleter 释放源缓冲区
     * @param[in] src 源缓冲区指针
     * @param[in] len 源缓冲区的长度
     * @param[in] deleter 用于释放源缓冲区的自定义删除器（可选）
     * @return 返回一个指向新创建的 Impl 对象的 unique_ptr
     * @note 该函数会深拷贝 src 指向的缓冲区数据到一个新的内存块中。
     * @note 如果提供了 deleter，函数会在内部调用 deleter(src) 来释放源缓冲区的内存。
     * @note 调用者需确保 src 在函数调用期间有效，否则会导致未定义行为。
     * @note 如果没有提供 deleter，调用者仍需自行管理 src 的生命周期。
     */
    static std::unique_ptr<Impl> CreateFromCopy(uint8_t *src, size_t len,
                                                const std::function<void(uint8_t *)> &deleter = nullptr)
    {
        // 分配内存并深拷贝数据
        auto copy = std::make_unique<uint8_t[]>(len);
        errno_t ret = memcpy_s(copy.get(), len, src, len);
        if (ret != 0) {
            if (deleter) {
                deleter(src); // 即使拷贝失败，也释放 src
            }
            return nullptr;
        }

        if (deleter) { // 释放 src 的内存
            deleter(src);
        }

        // 创建 Impl 对象，并将 copy 的所有权转移给它
        return std::make_unique<Impl>(std::move(copy), len);
    }
};

// 接口类实现
UbseSmartBuffer::UbseSmartBuffer() = default;
UbseSmartBuffer::~UbseSmartBuffer() = default;

UbseSmartBuffer::UbseSmartBuffer(uint8_t *data, const size_t len, std::function<void(uint8_t *)> deleter)
    : pimpl_(SafeMakeUnique<Impl>(data, len, std::move(deleter)))
{
}

UbseSmartBuffer::UbseSmartBuffer(std::unique_ptr<uint8_t[]> &&buf, size_t len)
    : pimpl_(SafeMakeUnique<Impl>(std::move(buf), len))
{
}

UbseSmartBuffer::UbseSmartBuffer(UbseSmartBuffer &&other) noexcept : pimpl_(std::move(other.pimpl_)) {}
UbseSmartBuffer &UbseSmartBuffer::operator=(UbseSmartBuffer &&other) noexcept
{
    if (this != &other) {
        pimpl_ = std::move(other.pimpl_);
    }
    return *this;
}

uint8_t *UbseSmartBuffer::GetData() noexcept
{
    return pimpl_ ? pimpl_->buffer.get() : nullptr;
}

const uint8_t *UbseSmartBuffer::GetData() const noexcept
{
    return pimpl_ ? pimpl_->buffer.get() : nullptr;
}

std::shared_ptr<uint8_t[]> UbseSmartBuffer::GetSharedData() const noexcept
{
    return pimpl_ ? pimpl_->buffer : nullptr;
}

size_t UbseSmartBuffer::GetSize() const noexcept
{
    return pimpl_ ? pimpl_->length : 0;
}

/**
 * @brief 从UbseByteBuffer对象创建一个新的UbseSmartBuffer对象
 * @param[in] buf 用于初始化UbseSmartBuffer的UbseByteBuffer对象
 * @param[in] shouldRelease 是否释放原始缓冲区的内存
 * @return 返回一个UbseSmartBuffer对象，如果buf.data为空或buf.len为0，则返回空对象
 * @note 如果shouldRelease为true，函数会使用buf.freeFunc来释放原始缓冲区的内存。
 * @note 调用者在shouldRelease为true时，应确保不再使用已经被释放的buf对象。
 */
UbseSmartBuffer UbseSmartBuffer::FromBuffer(const UbseByteBuffer &buf, const bool shouldRelease)
{
    if (!buf.data || buf.len == 0) {
        return {};
    }

    auto impl = Impl::CreateFromCopy(buf.data, buf.len, shouldRelease ? buf.freeFunc : nullptr);

    UbseSmartBuffer result;
    result.pimpl_ = std::move(impl);
    return result;
}
} // namespace ubse::utils
