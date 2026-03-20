/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "mp_file_util.h"

#include <regex>
#include <cstdlib>
#include <climits>
#include <fstream>

#include "ubse_logger.h"
#include "mp_configuration.h"

namespace mempooling {
using std::ifstream;
using namespace ubse::log;

/**
 * 读取文件内容
 * @param path const string &: 文件路径
 * @param info vector<string> &: 文件内容, 按行输出
 * @return MEM_POOLING_RES
 */
MEM_POOLING_RES MpFileUtil::GetFileInfo(const string &path, vector<string> &info)
{
    auto realFileName = std::make_unique<char[]>(PATH_MAX);
    if (realFileName == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "make unique realFileName failed.";
        return MEM_POOLING_ERROR;
    }
    if (realpath(path.c_str(), realFileName.get()) == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Get real path failed, errno = " << errno << ".";
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "Get real path failed, path = " << path << ".";
        return MEM_POOLING_ERROR;
    }
    ifstream file((string(realFileName.get())));
    if (!file.is_open()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Open file failed.";
        return MEM_POOLING_ERROR;
    }
    try {
        string line;
        while (getline(file, line)) {
            info.push_back(line);
        }
    } catch (const std::exception &e) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "read file failed: " << e.what();
        file.close();
        return MEM_POOLING_ERROR;
    }
    file.close();
    return MEM_POOLING_OK;
}

} // namespace mempooling