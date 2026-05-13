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

#include "ubse_str_util.h"

#include <securec.h>
#include <limits>
#include <random>
#include <regex>

namespace ubse::utils {
void Split(const std::string& src, const std::string& sep, std::vector<std::string>& out)
{
    if (sep.empty()) {
        return;
    }
    std::string::size_type pos1 = 0;
    std::string::size_type pos2 = src.find(sep);

    std::string tmpStr;
    while (pos2 != std::string::npos) {
        tmpStr = src.substr(pos1, pos2 - pos1);
        out.emplace_back(tmpStr);
        pos1 = pos2 + sep.size();
        pos2 = src.find(sep, pos1);
    }

    if (pos1 != src.length()) {
        tmpStr = src.substr(pos1);
        out.emplace_back(tmpStr);
    }
}

void Split(const std::string& src, const std::string& sep, std::set<std::string>& out)
{
    if (sep.empty()) {
        return;
    }
    std::string::size_type pos1 = 0;
    std::string::size_type pos2 = src.find(sep);

    std::string tmpStr;
    while (pos2 != std::string::npos) {
        tmpStr = src.substr(pos1, pos2 - pos1);
        out.insert(tmpStr);
        pos1 = pos2 + sep.size();
        pos2 = src.find(sep, pos1);
    }

    if (pos1 != src.length()) {
        tmpStr = src.substr(pos1);
        out.insert(tmpStr);
    }
}

// 函数用于从字符串中提取指定键的值
std::string ExtractValue(const std::string& input, const std::string& key)
{
    size_t keyPos = input.find(key + ":");
    if (keyPos == std::string::npos) {
        return "";
    }
    keyPos += key.length() + 1;
    size_t endPos = input.find_first_of(",}", keyPos);
    return input.substr(keyPos, endPos - keyPos);
}

const size_t VALUE_COUNT = 2;
common::def::UbseResult SplitSysSentryMsg(const std::string& faultInfo, uint64_t& msgId, std::string& cna,
                                          std::string& eid)
{
    std::vector<std::string> strVec;
    ubse::utils::Split(faultInfo, "_", strVec);
    if (strVec.size() < VALUE_COUNT) {
        return UBSE_ERROR;
    }
    std::string msgIdStr = strVec[0];
    if (ConvertStrToUint64(msgIdStr, msgId) != UBSE_OK) {
        return UBSE_ERROR;
    }
    cna = ExtractValue(strVec[1], "cna");
    eid = ExtractValue(strVec[1], "eid");
    return UBSE_OK;
}

common::def::UbseResult ConvertStrToInt(const std::string& str, int& outValue)
{
    try {
        outValue = std::stoi(str);
        return UBSE_OK;
    } catch (...) {
        return UBSE_ERROR;
    }
}

common::def::UbseResult ConvertStrToLong(const std::string& str, long& outValue)
{
    try {
        outValue = std::stol(str);
        return UBSE_OK;
    } catch (...) {
        return UBSE_ERROR;
    }
}

common::def::UbseResult ConvertStrToUint16(const std::string& str, uint16_t& outValue, const int base)
{
    try {
        unsigned long value = std::stoul(str, nullptr, base);
        // 检查是否超出 uint16_t 范围
        if (value > std::numeric_limits<uint16_t>::max()) {
            return UBSE_ERROR; // 超出范围
        }
        outValue = static_cast<uint16_t>(value);
        return UBSE_OK;
    } catch (...) {
        return UBSE_ERROR;
    }
}

common::def::UbseResult ConvertStrToUint32(const std::string& str, uint32_t& outValue, const int base)
{
    try {
        unsigned long value = std::stoul(str, nullptr, base);
        // 检查是否超出 uint32_t 范围
        if (value > std::numeric_limits<uint32_t>::max()) {
            return UBSE_ERROR; // 超出范围
        }
        outValue = static_cast<uint32_t>(value);
        return UBSE_OK;
    } catch (...) {
        return UBSE_ERROR;
    }
}

common::def::UbseResult ConvertStrToUint64(const std::string& str, uint64_t& outValue)
{
    try {
        unsigned long long value = std::stoull(str);
        // 检查是否超出 uint64_t 范围
        if (value > std::numeric_limits<uint64_t>::max()) {
            return UBSE_ERROR; // 超出范围
        }
        outValue = static_cast<uint64_t>(value);
        return UBSE_OK;
    } catch (const std::invalid_argument&) {
        return UBSE_ERROR; // 输入无效
    } catch (const std::out_of_range&) {
        return UBSE_ERROR; // 超出范围
    }
}

std::string GenerateRandomStr(uint32_t size)
{
    std::random_device rd;                      // 获取随机数种子
    std::mt19937 gen(rd());                     // 使用梅森旋转算法生成随机数
    std::uniform_int_distribution<> dist(0, 9); // 生成 0 到 9 的随机数字
    std::string randomID;

    for (size_t i = 0; i < size; ++i) {
        randomID += std::to_string(dist(gen)); // 随机选择数字并转换为字符串
    }
    return randomID;
}

void StrSplit(const std::string& src, const std::string& sep, std::vector<std::string>& out)
{
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

bool StrToULong(const std::string& src, uint64_t& value)
{
    char* remain = nullptr;
    errno = 0;
    value = std::strtoul(src.c_str(), &remain, 10L); // 10 is decimal digits
    if ((value == 0 && src != "0") || remain == nullptr || strlen(remain) > 0 || errno == ERANGE) {
        return false;
    }
    return true;
}

bool StrToUint(const std::string& src, uint32_t& value)
{
    char* remain = nullptr;
    errno = 0;
    value = std::strtoul(src.c_str(), &remain, 10L); // 10 is decimal digits
    if ((value == 0 && src != "0") || remain == nullptr || strlen(remain) > 0 || errno == ERANGE) {
        return false;
    }
    return true;
}

std::string Trim(const std::string& str, const std::locale& loc)
{
    std::string s = str;
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&loc](char ch) { return !std::isspace(ch, loc); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [&loc](char ch) { return !std::isspace(ch, loc); }).base(), s.end());
    return s;
}

} // namespace ubse::utils