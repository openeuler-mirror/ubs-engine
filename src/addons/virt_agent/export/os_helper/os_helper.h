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

#ifndef VM_OS_HELPER_H
#define VM_OS_HELPER_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "vm_error.h"

namespace vm {
class OsHelper {
public:
    static VmResult GetPidsByContainerIds(const std::unordered_set<std::string>& containerIds,
                                          std::unordered_map<std::string, std::vector<pid_t>>& containerInfos);

private:
    static std::string procPathPrefix;

    static std::string ParseContainerFile(const std::string& cgroupPath);
};
} // namespace vm

#endif // VM_OS_HELPER_H
