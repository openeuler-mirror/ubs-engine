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

#ifndef UBSE_UBSE_FILE_UTIL_H
#define UBSE_UBSE_FILE_UTIL_H

#include <regex>
#include <string>
#include <vector>

#include "ubse_common_def.h"

namespace ubse::utils {
using std::string;
using std::vector;
using namespace ubse::common::def;

class UbseFileUtil {
public:
    static UbseResult GetFileInfo(const string &path, vector<string> &info);

    static UbseResult IsSpecifiedPath(const string &dirPath, const string &pattern);

    static std::string GetExecutablePath();

    static std::string GetLibDir();
};
} // namespace ubse::utils

#endif // UBSE_UBSE_FILE_UTIL_H
