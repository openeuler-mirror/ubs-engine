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
void Split(const std::string &src, const std::string &sep, std::vector<std::string> &out);

void Split(const std::string &src, const std::string &sep, std::set<std::string> &out);

common::def::UbseResult SplitSysSentryMsg(const std::string &faultInfo, uint32_t &msgId, std::string &cna,
                                          std::string &eid);

common::def::UbseResult ConvertStrToInt(const std::string &str, int &outValue);

// 默认10进制, 使用其他进制时需要传入base
common::def::UbseResult ConvertStrToUint32(const std::string &str, uint32_t &outValue, int base = 10);

uint32_t StrToUint32(const std::string &str);

uint64_t StrToUint64(const std::string &str);

std::string Trim(const std::string &str, const std::locale &loc = std::locale{"C"});

common::def::UbseResult CopyStringToCharArray(const std::string &src, char *dest, size_t size);

common::def::UbseResult CopyCharArray(char *dest, size_t destSize, const char *src);

common::def::UbseResult ConvertStrToUint64(const std::string &str, uint64_t &outValue);
} // namespace ubse::utils
#endif // UBSE_MANAGER_UBSE_STR_UTIL_H
