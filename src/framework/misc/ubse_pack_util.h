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

#ifndef UBSE_PACK_UTIL_H
#define UBSE_PACK_UTIL_H

#include <securec.h>
#include <cstdint>
#include <string>
namespace ubse::utils {
class UbsePackUtil {
public:
    explicit UbsePackUtil(uint8_t *buffer, size_t bufferSize) noexcept : ptr(buffer), remaining(bufferSize) {}
    bool UbsePackUint32(uint32_t value)
    {
        if (remaining < sizeof(uint32_t)) {
            return false;
        }
        uint32_t net_value = htonl(value);
        errno_t err = memcpy_s(ptr, sizeof(uint32_t), &net_value, sizeof(uint32_t));
        if (err != EOK) {
            return false;
        }
        ptr += sizeof(uint32_t);
        remaining -= sizeof(uint32_t);
        return true;
    }

    bool UbsePackUint64(uint64_t value)
    {
        if (remaining < sizeof(uint64_t)) {
            return false;
        }
        errno_t err = memcpy_s(ptr, sizeof(uint64_t), &value, sizeof(uint64_t));
        if (err != EOK) {
            return false;
        }
        ptr += sizeof(uint64_t);
        remaining -= sizeof(uint64_t);
        return true;
    }

    bool UbsePackString(const std::string &str, uint32_t maxlen)
    {
        auto len = static_cast<uint32_t>(str.length());
        if (len > maxlen) {
            len = maxlen;
        }
        if (remaining < sizeof(uint32_t) + len) {
            return false;
        }
        if (!UbsePackUint32(len)) {
            return false;
        }

        if (len > 0) {
            errno_t ret = memcpy_s(ptr, len, str.c_str(), len);
            if (ret != EOK) {
                return false;
            }
            ptr += len;
            remaining -= len;
        }
        return true;
    }

private:
    uint8_t *ptr;
    size_t remaining;
};
} // namespace ubse::utils

#endif // UBSE_PACK_UTIL_H
