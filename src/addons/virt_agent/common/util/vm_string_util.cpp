/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "vm_string_util.h"

#include <algorithm>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include "vm_def.h"

namespace vm {
std::unordered_map<std::string, uint64_t> VmStringUtil::unitMap = {{"KB", BYTE2KB}, {"MB", BYTE2MB}, {"GB", BYTE2GB}};

uint32_t VmStringUtil::SafeStoul(const std::string& str)
{
    if (str.empty()) {
        return 0;
    }
    try {
        unsigned long value = std::stoul(str);
        if (value > std::numeric_limits<uint32_t>::max()) {
            throw std::out_of_range("Value out of range for uint32_t");
        }
        return static_cast<uint32_t>(value);
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument("Invalid argument: " + str);
    } catch (const std::out_of_range& e) {
        throw std::out_of_range("Out of range: " + str);
    }
}

uint64_t VmStringUtil::SafeStoull(const std::string& str)
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
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument("Invalid argument: " + str);
    } catch (const std::out_of_range& e) {
        throw std::out_of_range("Out of range: " + str);
    }
}

int16_t VmStringUtil::SafeStoi16(const std::string& str)
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
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument("Invalid argument: " + str);
    } catch (const std::out_of_range& e) {
        throw std::out_of_range("Out of range: " + str);
    }
}

int32_t VmStringUtil::SafeStoi32(const std::string& str)
{
    if (str.empty()) {
        return 0;
    }
    try {
        int value = std::stoi(str);
        if (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max()) {
            throw std::out_of_range("Value out of range for int32_t");
        }
        return static_cast<int32_t>(value);
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument("Invalid argument: " + str);
    } catch (const std::out_of_range& e) {
        throw std::out_of_range("Out of range: " + str);
    }
}

uint16_t VmStringUtil::SafeStou16(const std::string& str)
{
    if (str.empty()) {
        return 0;
    }
    try {
        unsigned long value = std::stoul(str);
        if (value > std::numeric_limits<uint16_t>::max()) {
            throw std::out_of_range("Value out of range for uint16_t");
        }
        return static_cast<uint16_t>(value);
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument("Invalid argument: " + str);
    } catch (const std::out_of_range& e) {
        throw std::out_of_range("Out of range: " + str);
    }
}

int64_t VmStringUtil::SafeStoi64(const std::string& str)
{
    if (str.empty()) {
        return 0;
    }
    try {
        long long value = std::stoll(str);
        if (value < std::numeric_limits<int64_t>::min() || value > std::numeric_limits<int64_t>::max()) {
            throw std::out_of_range("Value out of range for int64_t");
        }
        return static_cast<int64_t>(value);
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument("Invalid argument: " + str);
    } catch (const std::out_of_range& e) {
        throw std::out_of_range("Out of range: " + str);
    }
}
float_t VmStringUtil::SafeStof(const std::string& str)
{
    if (str.empty()) {
        return 0;
    }
    try {
        auto value = std::stof(str);
        return static_cast<float_t>(value);
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument("Invalid argument: " + str);
    } catch (const std::out_of_range& e) {
        throw std::out_of_range("Out of range: " + str);
    }
}
std::string VmStringUtil::GenerateUUID()
{
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    uint64_t part1 = dis(gen);
    uint64_t part2 = dis(gen);

    std::ostringstream oss;
    oss << std::hex << part1 << part2;

    std::string uuidStr = oss.str();
    return uuidStr.substr(0, 32); // Extract the first 32 characters as a simplified UUID
}
pid_t VmStringUtil::SafeStopid(const std::string& str)
{
    if (str.empty()) {
        return 0;
    }
    try {
        unsigned long value = std::stoul(str);
        if (value > std::numeric_limits<pid_t>::max()) {
            throw std::out_of_range("Value out of range for pid_t");
        }
        return static_cast<pid_t>(value);
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument("Invalid argument: " + str);
    } catch (const std::out_of_range& e) {
        throw std::out_of_range("Out of range: " + str);
    }
}

uint64_t VmStringUtil::ValToByte(const uint64_t val, const std::string& unit)
{
    auto toHigher = [](const std::string& str) -> std::string {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](const unsigned char c) { return std::toupper(c); });
        return result;
    };
    if (const auto upperUnit = toHigher(unit); unitMap.find(upperUnit) != unitMap.end()) {
        return val << unitMap[upperUnit];
    }
    return val;
}

uint16_t VmStringUtil::SafeNotEmptyStou16(const std::string& str)
{
    if (str.empty()) {
        throw std::invalid_argument("Invalid argument: " + str);
    }
    try {
        unsigned long value = std::stoul(str);
        if (value > std::numeric_limits<uint16_t>::max()) {
            throw std::out_of_range("Value out of range for uint16_t");
        }
        return static_cast<uint16_t>(value);
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument("Invalid argument: " + str);
    } catch (const std::out_of_range& e) {
        throw std::out_of_range("Out of range: " + str);
    }
}

uint64_t VmStringUtil::SafeNotEmptyStoull(const std::string& str)
{
    if (str.empty()) {
        throw std::invalid_argument("Invalid argument: " + str);
    }
    try {
        unsigned long long value = std::stoull(str);
        if (value > std::numeric_limits<uint64_t>::max()) {
            throw std::out_of_range("Value out of range for uint64_t");
        }
        return static_cast<uint64_t>(value);
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument("Invalid argument: " + str);
    } catch (const std::out_of_range& e) {
        throw std::out_of_range("Out of range: " + str);
    }
}

pid_t VmStringUtil::SafeNotEmptyStopid(const std::string& str)
{
    if (str.empty()) {
        throw std::invalid_argument("Invalid argument: " + str);
    }
    try {
        unsigned long value = std::stoul(str);
        if (value > std::numeric_limits<pid_t>::max()) {
            throw std::out_of_range("Value out of range for pid_t");
        }
        return static_cast<pid_t>(value);
    } catch (const std::invalid_argument& e) {
        throw std::invalid_argument("Invalid argument: " + str);
    } catch (const std::out_of_range& e) {
        throw std::out_of_range("Out of range: " + str);
    }
}

void VmStringUtil::StrSplit(const std::string& src, const std::string& sep, std::vector<std::string>& out)
{
    if (sep == "") {
        return;
    }
    std::string::size_type startPos = 0;
    std::string::size_type endPos = src.find(sep);
    std::string tmpStr;
    while (endPos != std::string::npos) {
        tmpStr = src.substr(startPos, endPos - startPos);
        out.emplace_back(tmpStr);
        startPos = endPos + sep.size();
        endPos = src.find(sep, startPos);
    }
    if (startPos != src.length()) {
        tmpStr = src.substr(startPos);
        out.emplace_back(tmpStr);
    }
}
} // namespace vm
