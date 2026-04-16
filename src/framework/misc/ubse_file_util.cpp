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

#include "ubse_file_util.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>

#include <fstream>
#include <memory>
#include <regex>

#include "ubse_error.h"
#include "ubse_logger_module.h"

namespace ubse::utils {
UBSE_DEFINE_THIS_MODULE("ubse");

using std::ifstream;
using namespace ubse::log;

/**
 * 读取文件内容
 * @param path const string &: 文件路径
 * @param info vector<string> &: 文件内容, 按行输出
 * @return UbseResult
 */
UbseResult UbseFileUtil::GetFileInfo(const string &path, vector<string> &info)
{
    auto realFileName = std::make_unique<char[]>(PATH_MAX);
    if (!realFileName) {
        return UBSE_ERROR;
    }
    if ((realpath(path.c_str(), realFileName.get()) == nullptr) && (errno == ENOENT)) {
        return UBSE_ERROR;
    }
    ifstream file((string(realFileName.get())));
    if (!file.is_open()) {
        return UBSE_ERROR;
    }
    try {
        string line;
        while (getline(file, line)) {
            info.push_back(line);
        }
    } catch (const std::exception &e) {
        file.close();
        return UBSE_ERROR;
    }
    file.close();
    return UBSE_OK;
}

/**
 * @brief 检查给定路径是否为 Unix/Linux 系统的绝对路径。
 *
 * 该函数使用filesystem的is_absolute函数判断是否为绝对路径。
 *
 * @param path 要检查的路径。
 * @return UbseResult 如果路径是绝对路径，则返回 UBSE_OK，
 *         否则返回 UBSE_ERROR。
 */
UbseResult UbseFileUtil::IsAbsolutePath(const std::string &path)
{
    // Unix/Linux 的绝对路径以/开头
    if (path.empty() || path[NO_0] != '/') {
        return UBSE_ERROR;
    }
    std::filesystem::path p(path);
    try {
        if (p.is_absolute()) {
            return UBSE_OK;
        }
        return UBSE_ERROR;
    } catch (const std::exception &e) {
        return UBSE_ERROR;
    }
}

std::vector<std::string> UbseFileUtil::ListFiles(const std::string &directory, const std::regex &pattern)
{
    std::vector<std::string> files;
    const auto dir = opendir(directory.c_str());
    if (dir == nullptr) {
        return files; // 返回空向量
    }

    dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        const std::string fileName = entry->d_name;
        if (fileName != "." && fileName != "..") {
            if (std::regex_match(fileName, pattern)) {
                files.push_back(fileName);
            }
        }
    }

    closedir(dir);
    return files; // 返回文件名向量
}

bool UbseFileUtil::IsDirectory(const string &dir)
{
    struct stat info {};
    if (stat(dir.data(), &info) != 0) {
        return false; // 无法访问路径
    }
    return S_ISDIR(info.st_mode); // 检查是否为目录
}

UbseResult UbseFileUtil::CreateAndChmodDirectory(const string &dir, mode_t permission)
{
    if (IsAbsolutePath(dir) != UBSE_OK) {
        UBSE_LOG_ERROR << "The directory must be an absolute path, current path=" << dir;
        return UBSE_ERROR;
    }
    if (!IsDirectory(dir)) { // 检查是否为目录
        if (mkdir(dir.c_str(), permission) == 0) {
            UBSE_LOG_INFO << "Success to create directory=" << dir;
        } else {
            UBSE_LOG_ERROR << "Failed to create directory=" << dir;
            return UBSE_ERROR;
        }
    }
    // 目录存在也要确保权限为755
    auto ret = chmod(dir.c_str(), permission);
    if (ret != 0) {
        UBSE_LOG_ERROR << "Failed to chmod to directory=" << dir << ", ret=" << strerror(errno);
        return UBSE_ERROR_ACCES;
    }
    return UBSE_OK;
}

bool UbseFileUtil::CheckFileExists(const string &path)
{
    struct stat buffer{};
    return (stat(path.c_str(), &buffer) == 0);
}

/**
 * @brief 设置文件的所有者和属组
 * @param path 文件路径
 * @param uid 用户ID，如果为0则保持不变
 * @param gid 组ID，如果为0则保持不变
 * @return bool 成功返回true，失败返回false
 */
bool UbseFileUtil::SetFileOwnership(const std::string &path, uid_t uid, gid_t gid)
{
    // 检查文件是否存在
    if (access(path.c_str(), F_OK) != 0) {
        return false;
    }

    // 如果uid和gid都为0，则不需要修改
    if (uid == 0 && gid == 0) {
        return true;
    }

    // 获取当前文件的所有权信息
    struct stat fileStat{};
    if (stat(path.c_str(), &fileStat) != 0) {
        return false;
    }

    // 如果参数为0，则保持原值
    uid_t targetUid = (uid == 0) ? fileStat.st_uid : uid;
    gid_t targetGid = (gid == 0) ? fileStat.st_gid : gid;

    // 设置文件所有权
    if (chown(path.c_str(), targetUid, targetGid) != 0) {
        // 错误处理
        return false;
    }

    return true;
}

/* *
 * @brief 设置文件权限
 * @param path 文件路径
 * @param mode 权限模式，如0644、0755等
 * @return bool 成功返回true，失败返回false
 */
bool UbseFileUtil::SetFilePermissions(const std::string &path, mode_t mode)
{
    // 检查文件是否存在
    if (access(path.c_str(), F_OK) != 0) {
        return false;
    }

    // 设置文件权限
    if (chmod(path.c_str(), mode) != 0) {
        // 错误处理
        return false;
    }

    return true;
}

/* *
 * @brief 同时设置文件所有权和权限
 * @param path 文件路径
 * @param uid 用户ID
 * @param gid 组ID
 * @param mode 权限模式
 * @return bool 成功返回true，失败返回false
 */
bool UbseFileUtil::SetFileAttributes(const std::string &path, uid_t uid, gid_t gid, mode_t mode)
{
    bool success = true;
    // 设置权限（如果mode不为0）
    if (mode != 0) {
        success = SetFilePermissions(path, mode);
    }
    // 设置所有权（如果uid或gid不为0）
    if (uid != 0 || gid != 0) {
        success = success && SetFileOwnership(path, uid, gid);
    }

    return success;
}

/* *
 * @brief 规范化路径
 * @param path 文件路径
 * @return bool 成功返回true，失败返回false
 */
bool UbseFileUtil::CanonicalPath(std::string &path)
{
    if (path.empty() || path.size() > PATH_MAX) {
        return false;
    }

    /* It will allocate memory to store path */
    char *realPath = realpath(path.c_str(), nullptr);
    if (realPath == nullptr) {
        return false;
    }

    path = realPath;
    free(realPath);
    return true;
}
} // namespace ubse::utils