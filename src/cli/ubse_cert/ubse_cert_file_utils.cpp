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

#include "ubse_cert_file_utils.h"
#include <sys/stat.h>
#include <climits>

namespace ubse::cli::cert {

bool FileUtils::IsSymlink(const std::string& filePath)
{
    struct stat buf {};
    if (lstat(filePath.c_str(), &buf) != 0) {
        return false;
    }
    return S_ISLNK(buf.st_mode);
}

bool FileUtils::IsHardLink(const std::string& filePath)
{
    struct stat fileStat {};
    if (stat(filePath.c_str(), &fileStat) != 0) {
        return false;
    }
    return fileStat.st_nlink > 1;
}

bool FileUtils::IsCanonicalPath(const std::string& filePath, std::string& errMsg)
{
    if (filePath.size() > PATH_MAX) {
        errMsg = filePath + " exceeds the maximum path length, which is " + std::to_string(PATH_MAX);
        return false;
    }
    if (IsSymlink(filePath)) {
        errMsg = filePath + " is a soft link, it's not supported.";
        return false;
    }
    if (IsHardLink(filePath)) {
        errMsg = filePath + " is a hard link, it's not supported.";
        return false;
    }
    char path[PATH_MAX + 1] = {};
    char* ret = realpath(filePath.c_str(), path);
    if (ret == nullptr) {
        errMsg = filePath + " fails to be parsed as a real path.";
        return false;
    }
    if (filePath != path) {
        errMsg = filePath + " is not canonical.";
        return false;
    }
    return true;
}
} // namespace ubse::cli::cert