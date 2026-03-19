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

#ifndef VM_MIGRATE_STATE_STORAGE_H
#define VM_MIGRATE_STATE_STORAGE_H

#include <string>
#include <ubse_storage.h>
#include "vm_error.h"
#include "vm_struct.h"
#include "vm_lock.h"

namespace vm {
using namespace ubse::storage;

class MigrateStateStorage {
public:
    MigrateStateStorage() = default;
    static VmResult SaveMigrateState(NumaVMInfoMap &numaVmInfoMap, const VMBasicInfo &vmBasicInfo);
    static VmResult DelMigrateState(NumaVMInfoMap &numaVmInfoMap, const VMBasicInfo &vmBasicInfo);
    static VmResult GetMigrateStates(NumaVMInfoMap &numaVmInfoMap);
    static void QueryHandler(const std::string &keyPrefix, const std::string &key, const UbseByteBuffer &buff,
                             void *ctx);
    static std::string ToString(const NumaVMInfoMap &numaVmInfoMap);
private:
    static VmResult OpMigrateState(NumaVMInfoMap &numaVmInfoMap, const VMBasicInfo &vmBasicInfo, bool isDelete);
    static ReadWriteLock migrateStateLock;
};
} // namespace vm

#endif // VM_MIGRATE_STATE_STORAGE_H
