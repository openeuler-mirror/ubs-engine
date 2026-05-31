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

#ifndef UBSE_OS_UTIL_H
#define UBSE_OS_UTIL_H

#include <iostream>
#include "ubse_common_def.h"

namespace ubse::utils {
using ubse::common::def::UbseResult;
class UbseOsUtil {
public:
    static UbseResult GetUserNameById(uid_t uid, std::string& userName);

    static UbseResult Exec(const std::string& cmd, std::string& res);

    static UbseResult GetUidByName(const std::string& username, uid_t& uid);

    static UbseResult GetNumaIdByPid(const uint64_t& pid, uint32_t& numaId);

    static UbseResult ReadFileContent(const std::string& filePath, std::string& res);
};
} // namespace ubse::utils
#endif // UBSE_OS_UTIL_H