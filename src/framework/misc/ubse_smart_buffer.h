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

#ifndef UBSE_SMART_BUFFER_H
#define UBSE_SMART_BUFFER_H
#include <cstdint>    // for uint8_t
#include <functional> // for function
#include <memory>     // for shared_ptr, unique_ptr

#include "ubse_def.h" // for UbseByteBuffer

namespace ubse::utils {

class UbseSmartBuffer {
public:
    UbseSmartBuffer();
    ~UbseSmartBuffer();

    UbseSmartBuffer(uint8_t *data, const size_t len, std::function<void(uint8_t *)> deleter);
    explicit UbseSmartBuffer(std::unique_ptr<uint8_t[]> &&buf, size_t len);

    // 移动构造/赋值
    UbseSmartBuffer(UbseSmartBuffer &&other) noexcept;
    UbseSmartBuffer &operator=(UbseSmartBuffer &&other) noexcept;

    // 禁用拷贝
    UbseSmartBuffer(const UbseSmartBuffer &) = delete;
    UbseSmartBuffer &operator=(const UbseSmartBuffer &) = delete;

    uint8_t *GetData() noexcept;
    const uint8_t *GetData() const noexcept;
    std::shared_ptr<uint8_t[]> GetSharedData() const noexcept;
    size_t GetSize() const noexcept;

    // 转换 C Buffer 到 C++ 智能 Buffer
    static UbseSmartBuffer FromBuffer(const UbseByteBuffer &buf, const bool shouldRelease = true);

private:
    struct Impl; // 前置声明实现类

    std::unique_ptr<Impl> pimpl_;
};
} // namespace ubse::utils
#endif // UBSE_SMART_BUFFER_H
