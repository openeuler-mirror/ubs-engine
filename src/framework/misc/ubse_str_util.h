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

#ifndef UBSE_MANAGER_UBSE_STR_UTIL_H
#define UBSE_MANAGER_UBSE_STR_UTIL_H

#include <locale>
#include <set>
#include <string>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_error.h"

namespace ubse::utils {
using common::def::UbseResult;

void Split(const std::string& src, const std::string& sep, std::vector<std::string>& out);

void Split(const std::string& src, const std::string& sep, std::set<std::string>& out);

UbseResult SplitSysSentryMsg(const std::string& faultInfo, uint64_t& msgId, std::string& cna, std::string& eid);

UbseResult ConvertStrToInt(const std::string& str, int& outValue);

UbseResult ConvertStrToLong(const std::string& str, long& outValue);

UbseResult ConvertStrToUint16(const std::string& str, uint16_t& outValue, const int base = 10);

// 默认10进制, 使用其他进制时需要传入base
UbseResult ConvertStrToUint32(const std::string& str, uint32_t& outValue, int base = 10);

std::string Trim(const std::string& str, const std::locale& loc = std::locale{"C"});

UbseResult ConvertStrToUint64(const std::string& str, uint64_t& outValue);

std::string GenerateRandomStr(uint32_t size);

void StrSplit(const std::string& src, const std::string& sep, std::vector<std::string>& out);

bool StrToULong(const std::string& src, uint64_t& value);

bool StrToUint(const std::string& src, uint32_t& value);

std::string RemoveDashes(const std::string& str);

// 16进制字符转换辅助函数
int HexCharToInt(char c);

// 将两个16进制字符转换为一个字节
bool HexPairToByte(char high, char low, uint8_t& result);
} // namespace ubse::utils
#endif // UBSE_MANAGER_UBSE_STR_UTIL_H
