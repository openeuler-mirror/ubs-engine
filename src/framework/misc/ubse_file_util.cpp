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

#include <fstream>
#include <memory>
#include <regex>

#include "ubse_error.h"
#include "ubse_logger_module.h"

namespace ubse::utils {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_UTILS_MID)

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
    std::unique_ptr<char[]> realFileName(new (std::nothrow) char[PATH_MAX]);
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
 * 判断路径名是否匹配正则
 * @param dirPath const string &: 路径
 * @param pattern const string &: 正则表达式
 * @return
 */
UbseResult UbseFileUtil::IsSpecifiedPath(const string &dirPath, const string &pattern)
{
    const std::regex designPattern(pattern);
    if (!std::regex_match(dirPath, designPattern)) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

std::string UbseFileUtil::GetExecutablePath()
{
    char filePath[PATH_MAX];

    // 读取符号链接 获取可执行文件的路径
    const auto count = readlink("/proc/self/exe", filePath, sizeof(filePath) - 1);
    if (count <= 0) {
        return {};
    }

    filePath[count] = '\0'; // 确保字符串以 null 结尾
    return {filePath};
}

std::string UbseFileUtil::GetLibDir()
{
    const std::string executablePath = GetExecutablePath();
    if (executablePath.empty()) {
        return {};
    }

    // 查找最后一个斜杠，获取可执行文件目录 bin/
    const size_t lastSlash = executablePath.find_last_of('/');
    if (lastSlash == std::string::npos) {
        return {};
    }

    // 通过 bin/ 目录定位 lib/ 目录
    return executablePath.substr(0, lastSlash) + "/../lib/";
}
} // namespace ubse::utils