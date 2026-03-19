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
#include <linux/limits.h>

#include "ubse_common_def.h"

namespace ubse::utils {
using std::string;
using std::vector;
using namespace ubse::common::def;

class UbseFileUtil {
public:
    static UbseResult GetFileInfo(const string &path, vector<string> &info);

    static UbseResult IsAbsolutePath(const std::string &path);

    static std::vector<std::string> ListFiles(const std::string &directory, const std::regex &pattern);

    static bool IsDirectory(const std::string &dir);

    /**
     * @brief 通过绝对路径创建一个目录，并赋权限
     * @note 如果该目录已经存在，则只会进行赋权限
     * @param dir 目标目录路径
     * @param permission 权限值
     * @return 0位成功，非0位失败
     */
    static UbseResult CreateAndChmodDirectory(const std::string &dir, mode_t permission = 0755);

    static bool CheckFileExists(const std::string &path);

    /**
     * @brief 设置文件的所有者和属组
     * @param path 文件路径
     * @param uid 用户ID，如果为0则保持不变
     * @param gid 组ID，如果为0则保持不变
     * @return bool 成功返回true，失败返回false
     */
    static bool SetFileOwnership(const std::string &path, uid_t uid = 0, gid_t gid = 0);

    /**
     * @brief 设置文件权限
     * @param path 文件路径
     * @param mode 权限模式，如0644、0755等
     * @return bool 成功返回true，失败返回false
     */
    static bool SetFilePermissions(const std::string &path, mode_t mode);

    /**
     * @brief 同时设置文件所有权和权限
     * @param path 文件路径
     * @param uid 用户ID
     * @param gid 组ID
     * @param mode 权限模式
     * @return bool 成功返回true，失败返回false
     */
    static bool SetFileAttributes(const std::string &path, uid_t uid = 0, gid_t gid = 0, mode_t mode = 0);

    /**
     * @brief 规范化路径
     * @param path 文件路径
     * @return bool 成功返回true，失败返回false
     */
    static bool CanonicalPath(std::string &path);
};
} // namespace ubse::utils

#endif // UBSE_UBSE_FILE_UTIL_H
