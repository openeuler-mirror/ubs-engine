/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "mp_string_util.h"

#include <limits>
#include <regex>
#include <stdexcept>

namespace mempooling {

MEM_POOLING_RES MpStringUtil::SafeStopid(const std::string &str, pid_t &ret) noexcept
{
    if (str.empty()) {
        return MEM_POOLING_ERROR;
    }
    try {
        auto value = std::stoi(str);
        if (value > std::numeric_limits<pid_t>::max() || value < 0) {
            return MEM_POOLING_ERROR_EXCEEDS_RANGE;
        }
        ret = static_cast<pid_t>(value);
        return MEM_POOLING_OK;
    } catch (const std::invalid_argument &e) {
        return MEM_POOLING_ERROR_INVAL;
    } catch (const std::out_of_range &e) {
        return MEM_POOLING_ERROR_EXCEEDS_RANGE;
    }
}

MEM_POOLING_RES MpStringUtil::SafeStoul(const std::string &str, uint32_t &ret) noexcept
{
    if (str.empty()) {
        return MEM_POOLING_ERROR;
    }
    try {
        unsigned long value = std::stoul(str);
        if (value > std::numeric_limits<uint32_t>::max()) {
            return MEM_POOLING_ERROR_EXCEEDS_RANGE;
        }
        ret = static_cast<uint32_t>(value);
        return MEM_POOLING_OK;
    } catch (const std::invalid_argument &e) {
        return MEM_POOLING_ERROR_INVAL;
    } catch (const std::out_of_range &e) {
        return MEM_POOLING_ERROR_EXCEEDS_RANGE;
    }
}

MEM_POOLING_RES MpStringUtil::SafeStoull(const std::string &str, uint64_t &ret) noexcept
{
    if (str.empty()) {
        return MEM_POOLING_ERROR;
    }
    try {
        unsigned long long value = std::stoull(str);
        if (value > std::numeric_limits<uint64_t>::max()) {
            return MEM_POOLING_ERROR_EXCEEDS_RANGE;
        }
        ret = static_cast<uint64_t>(value);
        return MEM_POOLING_OK;
    } catch (const std::invalid_argument &e) {
        return MEM_POOLING_ERROR_INVAL;
    } catch (const std::out_of_range &e) {
        return MEM_POOLING_ERROR_EXCEEDS_RANGE;
    }
}

uint64_t MpStringUtil::SafeStoullOld(const std::string &str)
{
    if (str.empty()) {
        return 0;
    }
    try {
        unsigned long long value = std::stoull(str);
        if (value > std::numeric_limits<uint64_t>::max()) {
            throw std::out_of_range("Value out of range for uint64_t");
        }
        return static_cast<uint64_t>(value);
    } catch (const std::invalid_argument &e) {
        throw std::invalid_argument("Invalid argument: " + str);
    } catch (const std::out_of_range &e) {
        throw std::out_of_range("Out of range: " + str);
    }
}

MEM_POOLING_RES MpStringUtil::SafeStoi16(const std::string &str, int16_t &ret) noexcept
{
    if (str.empty()) {
        return MEM_POOLING_ERROR;
    }
    try {
        int value = std::stoi(str);
        if (value < std::numeric_limits<int16_t>::min() || value > std::numeric_limits<int16_t>::max()) {
            return MEM_POOLING_ERROR_EXCEEDS_RANGE;
        }
        ret = static_cast<int16_t>(value);
        return MEM_POOLING_OK;
    } catch (const std::invalid_argument &e) {
        return MEM_POOLING_ERROR_INVAL;
    } catch (const std::out_of_range &e) {
        return MEM_POOLING_ERROR_EXCEEDS_RANGE;
    }
}

int16_t MpStringUtil::SafeStoi16Old(const std::string &str)
{
    if (str.empty()) {
        return 0;
    }
    try {
        int value = std::stoi(str);
        if (value < std::numeric_limits<int16_t>::min() || value > std::numeric_limits<int16_t>::max()) {
            throw std::out_of_range("Value out of range for int16_t");
        }
        return static_cast<int16_t>(value);
    } catch (const std::invalid_argument &e) {
        throw std::invalid_argument("Invalid argument: " + str);
    } catch (const std::out_of_range &e) {
        throw std::out_of_range("Out of range: " + str);
    }
}

MEM_POOLING_RES MpStringUtil::SafeStou16(const std::string &str, uint16_t &ret) noexcept
{
    if (str.empty()) {
        return MEM_POOLING_ERROR;
    }
    try {
        unsigned int value = std::stoul(str);
        if (value > std::numeric_limits<uint16_t>::max()) {
            return MEM_POOLING_ERROR_EXCEEDS_RANGE;
        }
        ret = static_cast<uint16_t>(value);
        return MEM_POOLING_OK;
    } catch (const std::invalid_argument &e) {
        return MEM_POOLING_ERROR_INVAL;
    } catch (const std::out_of_range &e) {
        return MEM_POOLING_ERROR_EXCEEDS_RANGE;
    }
}

MEM_POOLING_RES MpStringUtil::SafeStoi64(const std::string &str, int64_t &ret) noexcept
{
    if (str.empty()) {
        return MEM_POOLING_ERROR;
    }
    try {
        long long value = std::stoll(str);
        if (value < std::numeric_limits<int64_t>::min() || value > std::numeric_limits<int64_t>::max()) {
            return MEM_POOLING_ERROR_EXCEEDS_RANGE;
        }
        ret = static_cast<int64_t>(value);
        return MEM_POOLING_OK;
    } catch (const std::invalid_argument &e) {
        return MEM_POOLING_ERROR_INVAL;
    } catch (const std::out_of_range &e) {
        return MEM_POOLING_ERROR_EXCEEDS_RANGE;
    }
}

MEM_POOLING_RES MpStringUtil::SafeStof(const std::string &str, float_t &ret) noexcept
{
    if (str.empty()) {
        return MEM_POOLING_ERROR;
    }
    try {
        auto value = std::stof(str);
        ret = static_cast<float_t>(value);
        return MEM_POOLING_OK;
    } catch (const std::invalid_argument &e) {
        return MEM_POOLING_ERROR_INVAL;
    } catch (const std::out_of_range &e) {
        return MEM_POOLING_ERROR_EXCEEDS_RANGE;
    }
}
} // namespace mempooling
