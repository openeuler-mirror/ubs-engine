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

#ifndef UBSE_MANAGER_UBSE_PACK_UTIL_H
#define UBSE_MANAGER_UBSE_PACK_UTIL_H

#include <securec.h>
#include <cstdint>
#include <string>

namespace ubse::utils {
class UbsePackUtil {
public:
    // 构造函数：初始化解包缓冲区
    explicit UbsePackUtil(uint8_t* buffer, size_t bufferSize) noexcept : ptr_(buffer), remaining_(bufferSize) {}
    bool UbsePackUChar(unsigned char value)
    {
        if (remaining_ < sizeof(unsigned char)) {
            return false;
        }
        errno_t ret = memcpy_s(ptr_, sizeof(unsigned char), &value, sizeof(unsigned char));
        if (ret != EOK) {
            return false;
        }
        ptr_ += sizeof(unsigned char);
        remaining_ -= sizeof(unsigned char);
        return true;
    }
    bool UbsePackUint8(uint8_t value)
    {
        if (remaining_ < sizeof(uint8_t)) {
            return false;
        }
        errno_t ret = memcpy_s(ptr_, sizeof(uint8_t), &value, sizeof(uint8_t));
        if (ret != EOK) {
            return false;
        }
        ptr_ += sizeof(uint8_t);
        remaining_ -= sizeof(uint8_t);
        return true;
    }
    bool UbsePackUint32(uint32_t value)
    {
        if (remaining_ < sizeof(uint32_t)) {
            return false;
        }
        errno_t ret = memcpy_s(ptr_, sizeof(uint32_t), &value, sizeof(uint32_t));
        if (ret != EOK) {
            return false;
        }
        ptr_ += sizeof(uint32_t);
        remaining_ -= sizeof(uint32_t);
        return true;
    }
    bool UbsePackUint16(uint16_t value)
    {
        if (remaining_ < sizeof(uint16_t)) {
            return false;
        }
        errno_t ret = memcpy_s(ptr_, sizeof(uint16_t), &value, sizeof(uint16_t));
        if (ret != EOK) {
            return false;
        }
        ptr_ += sizeof(uint16_t);
        remaining_ -= sizeof(uint16_t);
        return true;
    }
    bool UbsePackInt32(int32_t value)
    {
        if (remaining_ < sizeof(int32_t)) {
            return false;
        }
        errno_t ret = memcpy_s(ptr_, sizeof(int32_t), &value, sizeof(int32_t));
        if (ret != EOK) {
            return false;
        }
        ptr_ += sizeof(int32_t);
        remaining_ -= sizeof(int32_t);
        return true;
    }

    bool UbsePackUint64(uint64_t value)
    {
        if (remaining_ < sizeof(uint64_t)) {
            return false;
        }
        errno_t ret = memcpy_s(ptr_, sizeof(uint64_t), &value, sizeof(uint64_t));
        if (ret != EOK) {
            return false;
        }
        ptr_ += sizeof(uint64_t);
        remaining_ -= sizeof(uint64_t);
        return true;
    }

    bool UbsePackInt64(int64_t value)
    {
        if (remaining_ < sizeof(int64_t)) {
            return false;
        }
        errno_t ret = memcpy_s(ptr_, sizeof(int64_t), &value, sizeof(int64_t));
        if (ret != EOK) {
            return false;
        }
        ptr_ += sizeof(int64_t);
        remaining_ -= sizeof(int64_t);
        return true;
    }

    bool UbsePackString(const std::string& str, uint32_t maxLen)
    {
        // 计算实际要写入的长度（不超过maxLen）
        auto len = static_cast<uint32_t>(str.length());
        if (len > maxLen) {
            len = maxLen;
        }

        // 检查长度前缀和字符串内容的总空间是否足够
        if (remaining_ < (sizeof(uint32_t) + len)) {
            return false;
        }

        // 写入长度前缀（32位无符号整数）
        if (!UbsePackUint32(len)) {
            return false;
        }

        // 写入字符串内容
        if (len > 0) {
            // 使用安全的内存复制函数
            errno_t ret = memcpy_s(ptr_, len, str.data(), len);
            if (ret != EOK) {
                return false;
            }
            ptr_ += len;
            remaining_ -= len;
        }
        return true;
    }

private:
    uint8_t* ptr_;     // 指针位置
    size_t remaining_; // 剩余缓冲区大小
};

class UbseUnpackUtil {
public:
    // 构造函数：初始化解包缓冲区
    explicit UbseUnpackUtil(const uint8_t* buffer, uint32_t len) noexcept : ptr_(buffer), remaining_(len) {}

    // 解包 unsigned char
    bool UnpackUChar(unsigned char& value) noexcept
    {
        if (remaining_ < sizeof(unsigned char)) {
            return false; // 缓冲区不足
        }

        auto ret = memcpy_s(&value, sizeof(unsigned char), ptr_, sizeof(unsigned char));
        if (ret != EOK) {
            return false;
        }
        // 更新位置
        ptr_ += sizeof(unsigned char);
        remaining_ -= sizeof(unsigned char);
        return true; // 成功
    }
    // 解包 uint8_t
    bool UnpackUint8(uint8_t& value) noexcept
    {
        if (remaining_ < sizeof(uint8_t)) {
            return false; // 缓冲区不足
        }

        auto ret = memcpy_s(&value, sizeof(uint8_t), ptr_, sizeof(uint8_t));
        if (ret != EOK) {
            return false;
        }
        // 更新位置
        ptr_ += sizeof(uint8_t);
        remaining_ -= sizeof(uint8_t);
        return true; // 成功
    }
    // 解包 uint16_t
    bool UnpackUint16(uint16_t& value) noexcept
    {
        if (remaining_ < sizeof(uint16_t)) {
            return false; // 缓冲区不足
        }

        auto ret = memcpy_s(&value, sizeof(uint16_t), ptr_, sizeof(uint16_t));
        if (ret != EOK) {
            return false;
        }
        // 更新位置
        ptr_ += sizeof(uint16_t);
        remaining_ -= sizeof(uint16_t);
        return true; // 成功
    }
    // 解包 uint32_t
    bool UnpackUint32(uint32_t& value) noexcept
    {
        if (remaining_ < sizeof(uint32_t)) {
            return false; // 缓冲区不足
        }

        auto ret = memcpy_s(&value, sizeof(uint32_t), ptr_, sizeof(uint32_t));
        if (ret != EOK) {
            return false;
        }
        // 更新位置
        ptr_ += sizeof(uint32_t);
        remaining_ -= sizeof(uint32_t);
        return true; // 成功
    }

    // 解包 uint64_t
    bool UnpackUint64(uint64_t& value) noexcept
    {
        if (remaining_ < sizeof(uint64_t)) {
            return false; // 缓冲区不足
        }
        auto ret = memcpy_s(&value, sizeof(uint64_t), ptr_, sizeof(uint64_t));
        if (ret != EOK) {
            return false;
        }

        ptr_ += sizeof(uint64_t);
        remaining_ -= sizeof(uint64_t);
        return true;
    }

    // 解包字符串 (带长度前缀)
    bool UnpackString(std::string& str, uint32_t maxLen) noexcept
    {
        uint32_t len = 0;
        if (!UnpackUint32(len)) {
            return false; // 长度解码失败
        }

        if (len > maxLen) {
            return false;
        }
        if (len == 0) {
            str.clear();
            return true;
        }

        if (remaining_ < len) {
            // 回退已消耗的长度字段
            ptr_ -= sizeof(uint32_t);
            remaining_ += sizeof(uint32_t);
            return false; // 数据不足
        }

        // 直接构造字符串，避免额外的内存拷贝
        str.assign(reinterpret_cast<const char*>(ptr_), len); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

        // 更新位置
        ptr_ += len;
        remaining_ -= len;
        return true;
    }

    // 解包枚举
    template <typename EnumType>
    bool UnpackEnum(EnumType& enumValue)
    {
        static_assert(std::is_enum_v<EnumType>, "UnpackEnum only supports enumeration types");
        // 拷贝数据
        uint32_t rawValue;
        if (!UnpackUint32(rawValue)) {
            return false;
        }

        // 转换为枚举类型
        enumValue = static_cast<EnumType>(rawValue);
        return true;
    }

private:
    const uint8_t* ptr_; // 当前解包指针位置
    uint32_t remaining_; // 剩余缓冲区长度（字节数）
};
} // namespace ubse::utils
#endif // UBSE_MANAGER_UBSE_PACK_UTIL_H
