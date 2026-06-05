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

#include <securec.h>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include "ubse_common_def.h"
#include "ubse_conf.h"
#include "ubse_error.h"
#include "ubse_mem_types.h"
#include "ubse_mmi_interface.h"

namespace ubse::mmi {
#define MODULE_LOG_NAME "ubse"
using ubse::adapter_plugins::mmi::UbseMemAlgoResult;
using ubse::adapter_plugins::mmi::UbseMemImportResult;
using ubse::adapter_plugins::mmi::UbseMemObmmInfo;
using ubse::adapter_plugins::mmi::UbseMemPrivData;
using ubse::common::def::UbseResult;
constexpr uint32_t BASE_16 = 16;

void CopyObmmMemDescValue(const ubse_mem_obmm_mem_desc& src, obmm_mem_desc* des, uint64_t hpa);
void CopyObmmMemDescValue(const obmm_mem_desc* src, ubse_mem_obmm_mem_desc& des);

inline void CopyUbMemPrivData(UbMemPrivData& des, const UbseMemPrivData& src)
{
    des.one_pth = src.onePth;
    des.wr_delay_comp = src.wrDelayComp;
    des.reduce_delay_comp = src.reduceDelayComp;
    des.cmo_delay_comp = src.cmoDelayComp;
    des.so = src.so;
    des.ad_tr_ochip = src.adTrOchip;
    des.cacheable_flag = src.cacheableFlag;
    des.mar_id = src.marId;
    des.rsv0 = src.rsv0;
}
inline void CopyUbMemPrivData(UbseMemPrivData& des, const UbMemPrivData& src)
{
    des.onePth = src.one_pth;
    des.wrDelayComp = src.wr_delay_comp;
    des.reduceDelayComp = src.reduce_delay_comp;
    des.cmoDelayComp = src.cmo_delay_comp;
    des.so = src.so;
    des.adTrOchip = src.ad_tr_ochip;
    des.cacheableFlag = src.cacheable_flag;
    des.marId = src.mar_id;
    des.rsv0 = src.rsv0;
}

inline void ConstructUbMemPrivData(UbMemPrivData& ubPrivData, const uint16_t& marId, const uint16_t wrDelayComp)
{
    ubPrivData.one_pth = 0u;
    ubPrivData.wr_delay_comp = wrDelayComp;
    ubPrivData.reduce_delay_comp = 0u;
    ubPrivData.cmo_delay_comp = 0u;
    ubPrivData.so = 0u;
    ubPrivData.ad_tr_ochip = 1u;
    ubPrivData.cacheable_flag = ubse::config::UbseIsMemBorrowCcSupported() ? 1u : 0u;
    ubPrivData.mar_id = marId;
    ubPrivData.rsv0 = 0u;
}

bool ParsePreOnlineEidStr(const std::string& eid, uint32_t& value);

inline std::string GenerateExportKey(const std::string& name, const std::string& importNodeId)
{
    return name + "_" + importNodeId;
}

UbseResult CopyUbseMemAlgoResult(const UbseMemAlgoResult& algoResult, const std::string& name,
                                 UbseMemLocalObmmCustomMeta& customMeta, const bool isAppendNodeId);

UbseResult SetMaskFromRegionIndex(const std::vector<uint32_t>& regionNodeIndex, uint32_t& mask);

inline bool IsBitSet(uint32_t mask, int bit)
{
    return (mask & (1u << bit)) != 0;
}
class RmCommonUtils {
public:
    static UbseResult GetFileFirstLine(const std::string& path, std::string& line);

    void GenerateNodeChipPortStr(const NodeId& lendNodeId, const ChipId& lendChipId, const PortId& portId,
                                 std::string& nodeChipPortStr);

    inline bool StrToULL(const std::string& src, uint64_t& value, int base = 10L)
    {
        char* remain = nullptr;
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

    inline bool StrToULong(const std::string& src, uint64_t& value)
    {
        char* remain = nullptr;
        errno = 0;
        value = std::strtoul(src.c_str(), &remain, 10L); // 10 is decimal digits
        if ((value == 0 && src != "0") || remain == nullptr || strlen(remain) > 0 || errno == ERANGE) {
            return false;
        }
        return true;
    }

    inline bool StrToLong(const std::string& src, long& value)
    {
        char* remain = nullptr;
        errno = 0;
        value = std::strtol(src.c_str(), &remain, 10L); // 10 is decimal digits
        if ((value == 0 && src != "0") || remain == nullptr || strlen(remain) > 0 || errno == ERANGE) {
            return false;
        }
        return true;
    }

    inline bool StartWith(const std::string& src, const std::string& start)
    {
        if (src.size() >= start.size()) {
            return !src.compare(0, start.size(), start);
        }
        return false;
    }

    template <typename T>
    inline bool IsValidUint8(const T value)
    {
        static_assert(std::is_integral<T>::value, "T must be an integral type.");
        if (value < 0u || value > UINT8_MAX) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Value " << std::to_string(value) << " is not in valid uint8.";
            return false;
        }
        return true;
    }

    template <typename T>
    inline bool IsValidUint32(const T value)
    {
        static_assert(std::is_integral<T>::value, "T must be an integral type.");
        if (value < 0u || value > UINT32_MAX) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Value " << std::to_string(value) << " is not in valid uint32.";
            return false;
        }
        return true;
    }

    template <typename T>
    inline bool IsValidUint16(const T value)
    {
        static_assert(std::is_integral<T>::value, "T must be an integral type.");
        if (value < 0u || value > UINT16_MAX) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Value " << std::to_string(value) << " is not in valid uint16.";
            return false;
        }
        return true;
    }

    static int HashStringToByte(const std::string& input);

    static char* GetStrError(int errNum, char* buf, size_t bufSize)
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

    inline uint64_t SizeByte2Mb(uint64_t size)
    {
        return size >> MOVE20_BIT;
    }

    template <typename T>
    static std::string IntListToStr(const std::vector<T>& list)
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

    inline std::string MemToStr(const std::vector<uint64_t>& list)
    {
        return IntListToStr(list);
    }

    inline std::string MemToStr(const std::vector<UbseMemObmmInfo>& list)
    {
        std::vector<uint64_t> tmp;
        tmp.reserve(list.size());
        for (const auto& info : list) {
            tmp.push_back(info.memId);
        }
        return IntListToStr(tmp);
    }

    inline std::string MemToStr(const std::vector<UbseMemImportResult>& list)
    {
        std::vector<uint64_t> tmp;
        tmp.reserve(list.size());
        for (const auto& info : list) {
            tmp.push_back(info.memId);
        }
        return IntListToStr(tmp);
    }

    template <typename T>
    inline std::string TranStructToStr(const T& obj)
    {
        std::ostringstream oss;
        oss.str("");
        oss.clear();
        oss << obj;
        return oss.str();
    }

    template <typename T>
    void SafeFree(T& ptr)
    {
        static_assert(std::is_pointer<T>::value, "T must be a pointer type.");
        if (ptr) {
            free(ptr);
            ptr = nullptr;
        }
    }

    template <typename T>
    void SafeDelete(T*& ptr)
    {
        if (ptr) {
            delete ptr;
            ptr = nullptr;
        }
    }

    template <typename T>
    void SafeDeleteArray(T*& ptr)
    {
        if (ptr) {
            delete[] ptr;
            ptr = nullptr;
        }
    }

    template <typename T>
    T SafeAdd(T a, T b, bool& isOverflow)
    {
        T result;
        isOverflow = __builtin_add_overflow(a, b, &result);
        return result;
    }

    static RmCommonUtils& GetInstance()
    {
        static RmCommonUtils instance;
        return instance;
    }
    RmCommonUtils(const RmCommonUtils& other) = delete;
    RmCommonUtils(RmCommonUtils&& other) = delete;
    RmCommonUtils& operator=(const RmCommonUtils& other) = delete;
    RmCommonUtils& operator=(RmCommonUtils&& other) noexcept = delete;

private:
    RmCommonUtils() = default;
};
#undef MODULE_LOG_NAME
} // namespace ubse::mmi

#endif // UBSE_MEM_COMMON_UTILS_H
