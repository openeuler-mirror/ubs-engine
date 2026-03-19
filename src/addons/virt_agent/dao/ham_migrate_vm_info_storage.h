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

#ifndef HAM_MIGRATE_VM_INFO_STORAGE_H
#define HAM_MIGRATE_VM_INFO_STORAGE_H

#include <string>

#include <ubse_storage.h>
#include "ham_migrate_vm_info.h"
#include "vm_error.h"
#include "vm_lock.h"

namespace vm {
using namespace ubse::storage;

class HamMigrateVmInfoStorage {
public:
    HamMigrateVmInfoStorage() = default;
    static VmResult SetHamMigrateVmInfo(const HamMigrateVmInfo& hamMigrateVmInfo);
    static VmResult SetHamMigrateVmInfos(const std::vector<HamMigrateVmInfo>& hamMigrateVmInfos);
    static VmResult GetHamMigrateVmInfo(const std::string& nodeId, int pid, HamMigrateVmInfo& hamMigrateVmInfo);
    static void GetHamMigrateVmInfos(const std::string& nodeId,
                                         std::vector<HamMigrateVmInfo>& hamMigrateVmInfos);
    static VmResult GetAllHamMigrateVmInfos(std::vector<HamMigrateVmInfo>& hamMigrateVmInfos);
    static void GetHamMigrateVmInfosByDstNodeId(const std::string &dstNodeId,
        std::vector<HamMigrateVmInfo> &hamMigrateVmInfos);
    static VmResult DelHamMigrateVmInfo(const std::string& nodeId, int pid);
    static void QueryHandler(const std::string& keyPrefix, const std::string& key, const UbseByteBuffer& buff,
                         void* ctx);
    static std::string ToString(const std::vector<HamMigrateVmInfo> &HamMigrateVmInfos);

private:
    static VmResult OpHamMigrate(std::vector<HamMigrateVmInfo>& hamMigrateVmInfos,
        const std::function<void(std::vector<HamMigrateVmInfo>&)> &func);
    static ReadWriteLock hamMigrateLock;
    static std::vector<HamMigrateVmInfo> hamMigrateCache;
};
} // namespace vm

#endif // HAM_MIGRATE_VM_INFO_STORAGE_H
