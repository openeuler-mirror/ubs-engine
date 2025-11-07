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
#include <unistd.h>
#include <climits>
#include <iostream>
#include <regex>

namespace {
constexpr long MIN_MALLOC_SIZE = 1;
constexpr long DEFAULT_MAX_DATA_SIZE = 4294967296;
constexpr int PER_PERMISSION_MASK_RWX = 0b111;
}

namespace ubse::cli::cert {
static long g_defaultMaxDataSize = DEFAULT_MAX_DATA_SIZE;
static const mode_t FILE_MODE = 0740;

size_t FileUtils::GetFileSize(const std::string &filePath)
{
    if (!FileUtils::CheckFileExists(filePath)) {
        std::cerr << "|check file path|END|returnF|path is:" << filePath << "|File does not exist." << std::endl;
        return 0;
    }
    std::string baseDir = "/";
    std::string errMsg;
    if (!FileUtils::RegularFilePath(filePath, baseDir, errMsg)) {
        std::cerr << "|regular file|END|returnF||Regular file failed by " << errMsg << std::endl;
        return 0;
    }

    FILE *fp = fopen(filePath.c_str(), "rb");
    if (fp == nullptr) {
        std::cerr << "|open file|END|returnF|path is:" << filePath.c_str() << "|Failed to open file." << std::endl;
        return 0;
    }
    auto ret = fseek(fp, 0, SEEK_END);
    if (ret != 0) {
        std::cerr << "|seek to end of file|END|returnF||Error seeking to end of file, ret is:" << ret << std::endl;
        fclose(fp);
        return 0;
    }
    size_t fileSize = static_cast<size_t>(ftell(fp));
    ret = fseek(fp, 0, SEEK_SET);
    if (ret != 0) {
        std::cerr << "|seek to set of file|END|returnF||Error seeking to set of file, ret is:" << ret << std::endl;
        fclose(fp);
        return 0;
    }
    if (fclose(fp) != 0) {
        std::cerr << "|close file||||Closing file failed." << std::endl;
    }
    return fileSize;
}

bool FileUtils::CheckDataSize(long size)
{
    if ((size > g_defaultMaxDataSize) || (size < MIN_MALLOC_SIZE)) {
        std::cerr << "|check input date size|END|returnF||Input data size(" << size << ") out of range[" <<
            MIN_MALLOC_SIZE << "," << g_defaultMaxDataSize << "]." << std::endl;
        return false;
    }

    return true;
}

bool FileUtils::CheckFileExists(const std::string &filePath)
{
    struct stat buffer;
    return (stat(filePath.c_str(), &buffer) == 0);
}

bool FileUtils::CheckDirectoryExists(const std::string &dirPath)
{
    struct stat buffer;
    if (stat(dirPath.c_str(), &buffer) != 0) {
        return false;
    }
    return (S_ISDIR(buffer.st_mode) == 1);
}

bool FileUtils::IsSymlink(const std::string &filePath)
{
    struct stat buf;
    if (lstat(filePath.c_str(), &buf) != 0) {
        return false;
    }
    return S_ISLNK(buf.st_mode);
}

bool FileUtils::RegularFilePath(const std::string &filePath, const std::string &baseDir, std::string &errMsg)
{
    if (filePath.empty()) {
        errMsg = "The file path is empty.";
        return false;
    }
    if (baseDir.empty()) {
        errMsg = "The file path basedir is empty.";
        return false;
    }
    if (filePath.size() > PATH_MAX) {
        errMsg = "The file path exceeds the maximum value set by PATH_MAX.";
        return false;
    }
    if (IsSymlink(filePath)) {
        errMsg = "The file is a link.";
        return false;
    }
    char path[PATH_MAX + 1] = { 0x00 };
    char *ret = realpath(filePath.c_str(), path);
    if (ret == nullptr) {
        errMsg = "The path realpath parsing failed.";
        return false;
    }
    std::string realFilePath(path, path + strlen(path));

    std::string dir = baseDir.back() == '/' ? baseDir : baseDir + "/";
    if (realFilePath.rfind(dir, 0) != 0) {
        errMsg = "The file is invalid, it's not in baseDir directory.";
        return false;
    }

    return true;
}

bool FileUtils::RegularFilePath(const std::string &filePath, std::string &errMsg)
{
    if (filePath.empty()) {
        errMsg = "The file path is empty.";
        return false;
    }

    if (filePath.size() > PATH_MAX) {
        errMsg = "The file path exceeds the maximum value set by PATH_MAX.";
        return false;
    }
    if (IsSymlink(filePath)) {
        errMsg = "The file is a link.";
        return false;
    }
    char path[PATH_MAX + 1] = { 0x00 };
    char *ret = realpath(filePath.c_str(), path);
    if (ret == nullptr) {
        errMsg = "The path realpath parsing failed.";
        return false;
    }
    return true;
}

bool FileUtils::IsFileValid(const std::string &configFile, std::string &errMsg)
{
    if (!CheckFileExists(configFile)) {
        errMsg = "The input file is not a regular file or not exists";
        return false;
    }
    size_t fileSize = GetFileSize(configFile);
    if (fileSize == 0) {
        errMsg = "The input file is empty";
    } else if (!CheckDataSize(fileSize)) {
        errMsg = "Read input file failed, file is too large";
        return false;
    }
    return true;
}

bool FileUtils::CheckPermission(const std::string &filePath, const mode_t &mode, bool onlyCurrentUserOp,
    std::string &errMsg)
{
    struct stat buf;
    int ret = stat(filePath.c_str(), &buf);
    if (ret != 0) {
        errMsg = "Get file stat failed.";
        return false;
    }

    mode_t mask = PER_PERMISSION_MASK_RWX;
    const int perPermWidth = 3;
    std::vector<std::string> permMsg = { "Other group permission", "Owner group permission", "Owner permission" };
    for (int i = perPermWidth; i > 0; i--) {
        uint32_t curPerm = (buf.st_mode & (mask << ((i - 1) * perPermWidth))) >> ((i - 1) * perPermWidth);
        uint32_t maxPerm = (mode & (mask << ((i - 1) * perPermWidth))) >> ((i - 1) * perPermWidth);
        if ((curPerm | maxPerm) != maxPerm) {
            errMsg = " Check " + permMsg[i - 1] + " failed: Current permission is " + std::to_string(curPerm) +
                ", but required no greater than " + std::to_string(maxPerm) + ".";

            return false;
        }
        uint32_t readPerm = 4; // Read Perm
        uint32_t noPerm = 0;   // No Perm
        if (i != perPermWidth && curPerm != noPerm && curPerm != readPerm) {
            errMsg = " Check " + permMsg[i - 1] + " failed: Current permission is " + std::to_string(curPerm) +
                ", but required no write or execute permission.";
            if (onlyCurrentUserOp) {
                return false;
            }
        }
    }
    return true;
}

bool FileUtils::CheckFilePathExist(std::string &filePath, bool isFile)
{
    if (!FileUtil::CanonicalPath(filePath)) {
        std::cerr << "|check filePath|END|returnF|filePath is:" << filePath <<
            "|Check filePath failed, path is invalid or not exist." << std::endl;
        return false;
    }
    if (isFile && !FileUtil::CheckFileStat(filePath)) {
        std::cerr << "|check filePath|END|returnF|filePath is:" << filePath <<
            "|Check filePath failed, file stat is invalid." << std::endl;
        return false;
    }
    if (!FileUtil::Exist(filePath, F_OK | R_OK)) {
        std::cerr << "|check filePath|END|returnF|filePath is:" << filePath <<
            "|Check filePath failed, read access deny." << std::endl;
        return false;
    }
    return true;
}

std::string FileUtils::GetPathBeforeLastPart(const std::string &path)
{
    if (path.empty()) {
        return "";
    }
    size_t lastSlashPos = path.find_last_of('/');
    if (lastSlashPos == std::string::npos || (lastSlashPos == 0 && path.size() == 1)) {
        return "";
    }
    return path.substr(0, lastSlashPos);
}

bool FileUtils::IsEndWith(const std::string &str, const std::string &suffix)
{
    if (str.empty() || suffix.empty()) {
        return false;
    }
    if (suffix.size() > str.size()) {
        return false;
    }
    return str.rfind(suffix) == (str.size() - suffix.size());
}

bool FileUtils::IsFile(const std::string &path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !((st.st_mode & S_IFREG) == S_IFREG)) {
        // 如果stat失败，返回false
        return false;
    }
    return true;
}
}