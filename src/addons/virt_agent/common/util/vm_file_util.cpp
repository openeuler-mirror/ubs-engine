/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "vm_file_util.h"

#include <cstdlib>
#include <climits>
#include <fstream>

#include <ubse_logger.h>
#include "vm_def.h"

namespace vm {
UBSE_DEFINE_THIS_MODULE("virt_agent_plugin");
using std::ifstream;
using namespace ubse::log;

VmResult VmFileUtil::GetFileInfo(const string &path, vector<string> &info)
{
    try {
        std::vector<char> realFileName(PATH_MAX + NO_1);

        if (realpath(path.c_str(), realFileName.data()) == nullptr) {
            if (errno == ENOENT) {
                // The process directory may be gone, and files may not be found.
                // The calling service should determine whether it is necessary to print logs.
                return VM_WARN;
            } else {
                UBSE_LOG_ERROR << "realpath failed for " << path << ", errno=" << errno;
                return VM_ERROR;
            }
        }
        ifstream file(realFileName.data());
        if (!file.is_open()) {
            UBSE_LOG_ERROR << "Open file failed.";
            return VM_ERROR;
        }

        string line;
        while (getline(file, line)) {
            info.push_back(line);
        }
    } catch (const std::exception &e) {
        UBSE_LOG_ERROR << "read file failed: " << e.what();
        return VM_ERROR;
    }
    return VM_OK;
}

bool VmFileUtil::CanonicalPath(string &path)
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
} // namespace vm