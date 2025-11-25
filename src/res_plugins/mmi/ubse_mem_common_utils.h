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

#ifndef UBSE_MEM_COMMON_UTILS_H
#define UBSE_MEM_COMMON_UTILS_H

#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include "ubse_common_def.h"
#include "ubse_mem_types.h"
#include "ubse_error.h"
#include "ubse_mem_obj.h"

namespace ubse::mmi {
using namespace ubse::common::def;
using namespace ubse::mem::obj;
class RmCommonUtils {
public:
    static UbseResult GetFileFirstLine(const std::string &path, std::string &line);

    void GenerateNodeSocketStr(const NodeId &lendNodeId, const SocketId &lendSocketId, std::string &nodeSocketStr);

    inline bool StrToULL(const std::string &src, uint64_t &value, int base = 10L)
    {
        char *remain = nullptr;
        errno = 0;
        value = std::strtoull(src.c_str(), &remain, base);
        if (base == 16U && value == 0 && src == "0x0") {
            return true;
        }
        if ((value == 0 && src != "0") || remain == nullptr || strlen(remain) > 0 || errno == ERANGE) {
            return false;
        }
        return true;
    }

    inline bool StrToULong(const std::string &src, uint64_t &value)
    {
        char *remain = nullptr;
        errno = 0;
        value = std::strtoul(src.c_str(), &remain, 10L); // 10 is decimal digits
        if ((value == 0 && src != "0") || remain == nullptr || strlen(remain) > 0 || errno == ERANGE) {
            return false;
        }
        return true;
    }

    inline bool StrToLong(const std::string &src, long &value)
    {
        char *remain = nullptr;
        errno = 0;
        value = std::strtol(src.c_str(), &remain, 10L); // 10 is decimal digits
        if ((value == 0 && src != "0") || remain == nullptr || strlen(remain) > 0 || errno == ERANGE) {
            return false;
        }
        return true;
    }

    template <typename T>
    inline bool IsValidUint8(const T value)
    {
        static_assert(std::is_integral<T>::value, "T must be an integral type.");
        if (value < 0u || value > UINT8_MAX) {
            UBSE_LOG_ERROR << "Value " << std::to_string(value) << " is not in valid uint8.";
            return false;
        }
        return true;
    }

    template <typename T>
    inline bool IsValidUint32(const T value)
    {
        static_assert(std::is_integral<T>::value, "T must be an integral type.");
        if (value < 0u || value > UINT32_MAX) {
            UBSE_LOG_ERROR << "Value " << std::to_string(value) << " is not in valid uint32.";
            return false;
        }
        return true;
    }

    template <typename T>
    inline bool IsValidUint16(const T value)
    {
        static_assert(std::is_integral<T>::value, "T must be an integral type.");
        if (value < 0u || value > UINT16_MAX) {
            UBSE_LOG_ERROR << "Value " << std::to_string(value) << " is not in valid uint16.";
            return false;
        }
        return true;
    }

    static int HashStringToByte(const std::string &input);

    static char *GetStrError(int errNum, char *buf, size_t bufSize)
    {
#if defined(_XOPEN_SOURCE) && defined(_POSIX_C_SOURCE) && defined(_GNU_SOURCE) && \
    (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && !_GNU_SOURCE
        strerror_r(errNum, buf, bufSize - 1);
        return buf;
#else
        return strerror_r(errNum, buf, bufSize - 1);
#endif
    }

    inline uint64_t SizeMb2Byte(uint64_t size)
    {
        return size << MOVE20_BIT;
    }

    template <typename T>
    static std::string IntListToStr(const std::vector<T> &list)
    {
        std::ostringstream oss;
        for (size_t i = 0; i < list.size(); ++i) {
            if (i > 0) {
                oss << ","; // 使用逗号作为分隔符
            }
            oss << list[i];
        }
        return oss.str();
    }

    inline std::string MemToStr(const std::vector<uint64_t> &list)
    {
        return IntListToStr(list);
    }

    inline std::string MemToStr(const std::vector<UbseMemObmmInfo> &list)
    {
        std::vector<uint64_t> tmp;
        tmp.reserve(list.size());
        for (const auto &info : list) {
            tmp.push_back(info.memId);
        }
        return IntListToStr(tmp);
    }

    inline std::string MemToStr(const std::vector<UbseMemImportResult> &list)
    {
        std::vector<uint64_t> tmp;
        tmp.reserve(list.size());
        for (const auto &info : list) {
            tmp.push_back(info.memId);
        }
        return IntListToStr(tmp);
    }

    template <typename T>
    inline std::string TranStructToStr(const T &obj)
    {
        std::ostringstream oss;
        oss.str("");
        oss.clear();
        oss << obj;
        return oss.str();
    }

    template <typename T>
    void SafeFree(T &ptr)
    {
        static_assert(std::is_pointer<T>::value, "T must be a pointer type.");
        if (ptr) {
            free(ptr);
            ptr = nullptr;
        }
    }

    template <typename T>
    void SafeDelete(T *&ptr)
    {
        if (ptr) {
            delete ptr;
            ptr = nullptr;
        }
    }

    template <typename T>
    void SafeDeleteArray(T *&ptr)
    {
        if (ptr) {
            delete[] ptr;
            ptr = nullptr;
        }
    }

    static RmCommonUtils &GetInstance()
    {
        static RmCommonUtils instance;
        return instance;
    }
    RmCommonUtils(const RmCommonUtils &other) = delete;
    RmCommonUtils(RmCommonUtils &&other) = delete;
    RmCommonUtils &operator=(const RmCommonUtils &other) = delete;
    RmCommonUtils &operator=(RmCommonUtils &&other) noexcept = delete;

private:
    RmCommonUtils() = default;
};

} // namespace ubse::mmi

#endif // UBSE_MEM_COMMON_UTILS_H
