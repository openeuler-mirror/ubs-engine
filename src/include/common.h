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

#ifndef COMMON_H
#define COMMON_H
#include <string>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <climits>
#include <sys/stat.h>

namespace ubse::cli::cert {

constexpr uint32_t MAX_FILE_SIZE = 2 * 1024 * 1024; // 2 * 1024 * 1024: 文件支持2M文件

class FileUtil {
public:
    static bool CanonicalPath(std::string &path);
    static bool Exist(const std::string &path, const int &mode = 0);
    static bool CheckFileStat(const std::string &filePath);
    static bool GetFolderPath(const std::string &filePath, std::string &folderPath);
    inline bool GetFileName(const std::string &filePath, std::string &fileName);
    static bool IsRelativePath(const std::string& filePath);
};

inline bool FileUtil::Exist(const std::string &path, const int &mode)
{
    return access(path.c_str(), mode) != -1;
}

inline bool FileUtil::CanonicalPath(std::string &path)
{
    if (path.empty() || path.size() > PATH_MAX) {
        return false;
    }
    char pathBuf[PATH_MAX + 1] = {0};
    if (realpath(path.c_str(), pathBuf) == nullptr) {
        return false;
    }
    path = pathBuf;
    return true;
}

inline bool FileUtil::CheckFileStat(const std::string &filePath)
{
    struct stat st;
    if (stat(filePath.c_str(), &st) != 0) {
        return false;
    }

    if ((st.st_mode & S_IFMT) != S_IFREG || st.st_size > MAX_FILE_SIZE) {
        return false;
    }

    return true;
}

inline bool FileUtil::GetFolderPath(const std::string &filePath, std::string &folderPath)
{
    const size_t pos = filePath.find_last_of("/");
    if (pos == std::string::npos) {
        return false;
    }
    folderPath = filePath.substr(0, pos);
    return true;
}

inline bool FileUtil::GetFileName(const std::string &filePath, std::string &fileName)
{
    const size_t pos = filePath.find_last_of("/");
    if (pos == std::string::npos) {
        return false;
    }
    fileName = filePath.substr(pos);
    return true;
}

inline bool FileUtil::IsRelativePath(const std::string& filePath)
{
    if (filePath.length() == 0) {
        return false;
    }

    if (filePath[0] != '/') {
        return false;
    }

    if (strstr(filePath.c_str(), "/../") != nullptr || strstr(filePath.c_str(), "/./") != nullptr) {
        return false;
    }

    return true;
}

template <typename T> class AutoFreePtr {
public:
    explicit AutoFreePtr(T *obj, bool isArray = false) : mObj(obj), mIsArray(isArray) {}

    ~AutoFreePtr()
    {
        if (mObj == nullptr) {
            return;
        }

        if (mIsArray) {
            delete[] mObj;
        } else {
            delete mObj;
        }
    }

    /*
     * @brief Get obj
     */
    inline T *Get() const
    {
        return mObj;
    }

    /*
     * @brief Set the inner object to null, after this is called the inner object is nullptr, then no delete happens
     *
     */
    inline void SetNull()
    {
        mObj = nullptr;
    }

    AutoFreePtr() = delete;
    AutoFreePtr(const AutoFreePtr<T> &) = delete;
    AutoFreePtr(AutoFreePtr<T> &&) = delete;

    AutoFreePtr<T> &operator = (T *newObj) = delete;
    AutoFreePtr<T> &operator = (const AutoFreePtr<T> &other) = delete;
    AutoFreePtr<T> &operator = (AutoFreePtr<T> &&other) = delete;

private:
    T *mObj = nullptr;
    bool mIsArray = false;
};

class StrUtil {
public:
    static bool StrToLong(const std::string &src, long &value);
};
inline bool StrUtil::StrToLong(const std::string &src, long &value)
{
    char *remain = nullptr;
    errno = 0;
    value = std::strtol(src.c_str(), &remain, 10L); // 10 is decimal digits
    if ((value == 0 && src != "0") || remain == nullptr || strlen(remain) > 0 || errno == ERANGE) {
        return false;
    }
    return true;
}
}

#endif // COMMON_H

