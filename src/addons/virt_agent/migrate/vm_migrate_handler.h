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

#ifndef VM_MIGRATE_STATUS_H
#define VM_MIGRATE_STATUS_H
#include <atomic>

#include "vm_error.h"
#include "vm_struct.h"

namespace vm {
class VmMigrateHandler {
public:
    VmMigrateHandler() : intervalSeconds(5), init(false) {} // 5s interval

public:
    static void FlushExpireDataThread();
    static void Stop();

private:
    VmResult InitVmMigrateData();
    void FlushExpireData();
    static void FlushExpireVm(const VMNodeLocInfo& nodeLoc, const std::unordered_map<std::string, VMBasicInfo>& vmMap,
                              time_t currentTime);
    uint32_t intervalSeconds{};
    bool init{};
    static std::atomic<bool> exitFlag;
};
} // namespace vm

#endif // VM_MIGRATE_STATUS_H
