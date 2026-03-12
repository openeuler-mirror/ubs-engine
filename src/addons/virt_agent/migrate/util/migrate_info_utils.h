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

#ifndef VM_MIGRATE_INFO_UTILS
#define VM_MIGRATE_INFO_UTILS

#include <string>
#include "vm_error.h"

namespace vm {
using std::string;
class MigrateInfoBase {
public:
    uint32_t numaId;
    uint32_t pageSize;
    uint32_t socketId;
    std::string dstNodeId;
};
class MigrateInfoUtil {
public:
    static pid_t GetPidByVmUUID(const string &uuid);
    static VmResult GetNumaIdAndPageSizeByPid(pid_t pid, MigrateInfoBase &numaIdAndPageSize);
    static VmResult GetSocketIdByNumaId(uint32_t numaId, uint32_t *socketId);
private:
    static string cpuSocketPath;
    static string numaPathPrefix;
    static string procPathPrefix;
};
}
#endif // VM_MIGRATE_INFO_UTILS
