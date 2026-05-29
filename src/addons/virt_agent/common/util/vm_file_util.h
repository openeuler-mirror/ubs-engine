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

#ifndef VM_FILE_UTIL_H
#define VM_FILE_UTIL_H

#include <string>
#include <vector>

#include "vm_error.h"

namespace vm {
using std::string;
using std::vector;

class VmFileUtil {
public:
    /**
     * @brief Get the content of the specified file
     * @param path const string &: file path
     * @param info vector<string> &: file content, output by line
     * @return Operation result, 0 indicates success, and any non-zero value indicates an exception
     */
    static VmResult GetFileInfo(const string& path, vector<string>& info);

    static bool CanonicalPath(std::string& path);
};
} // namespace vm

#endif
