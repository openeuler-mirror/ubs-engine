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

#ifndef VM_MIGRATE_H
#define VM_MIGRATE_H

#include <ubse_api_server_def.h>

#include "mem_migrate_msg.h"
#include "vm_error.h"
#include "vm_strategy_struct.h"
#include "vm_struct.h"

namespace vm {
using namespace api::server;
constexpr int RETRY_TIMES = 3;
// If migration fails, enable hot/cold page migration and update the VM state to migratable
const std::string ENABLE_AND_MIGRATE_FAILED = "true";
// Before migration, disable hot/cold page migration and update the VM state to migrating
const std::string DISABLE_AND_MIGRATING = "false";
// Before migration, do not handle hot/cold page migration, only update the VM state to migrating
const std::string NONE_AND_MIGRATING = "none_migrating";
// When migration succeeds, do not handle hot/cold page migration, only update the VM state to migration successful
const std::string NONE_AND_MIGRATE_SUCCESS = "none_migrate_success";
// If migration fails, only update the VM state to migratable
const std::string NONE_AND_MIGRATE_FAILED = "none_migrate_failed";

struct PageFlowResultParam {
    std::string ToJson();
    uint32_t ret{};
    std::string msg{};
};

class VmMigrate {
public:
    static VmResult Register();
    static uint32_t UpdatePageFlowAndStatus(const UbseIpcMessage &req, const UbseRequestContext &context);

private:
    static VmResult ProcessRequest(MemMigrateInputParams &inputParams, UbseIpcMessage &response);

    static VmResult BuildIpcResponse(const std::string &message, UbseIpcMessage &resp);

    static VmResult FindNodeIdPid(const std::string &uuid, std::string &nodeId, pid_t &pid,
                                  NumaMemInfoMap &numaMemInfoMap);

    static VmResult ToUpdateVmStatus(const NumaMemInfoMap &numaMemInfoMap, const std::string &uuid, pid_t pid,
                                     VmMigrateStatus vmMigrateStatus);
    static VmResult UpdateVmStatusByMigrateStatus(const std::string &opt, const NumaMemInfoMap &numaMemInfoMap,
                                                  const std::string &uuid, pid_t pid, UbseIpcMessage &response);
};
} // namespace vm

#endif // VM_MIGRATE_H
