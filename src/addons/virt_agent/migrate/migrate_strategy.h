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

#ifndef VM_MIGRATE_STRATEGY_H
#define VM_MIGRATE_STRATEGY_H

#include <ubse_api_server_def.h>

#include <vm_error.h>
#include "migrate_info_utils.h"

namespace vm {
using namespace api::server;

enum class MigrateStrategy : uint32_t
{
    MULTICOPY_MIGRATE_POLICY = 0,
    ONECOPY_MIGRATE_POLICY = 1,
    HAM_MIGRATE_POLICY = 2,
    CROSS_RACK_MULTICOPY_MIGRATE_POLICY = 3,
};

enum class UbseVmResult : uint32_t
{
    // Basic values of VM error codes
    VM_ERROR_BASE = 2000,
    VM_DRIVER_NOT_FIND = VM_ERROR_BASE + 1,
    VM_DRIVER_DLOPEN_FAILED = VM_ERROR_BASE + 2,
    VM_FUNC_DLSYM_FAILED = VM_ERROR_BASE + 3,
    VM_BUFFER_NULL_POINTER = VM_ERROR_BASE + 4,
    VM_RESPONSE_OFFSET_NULL_POINTER = VM_ERROR_BASE + 5,
    VM_ROOT_TABLE_NULL_POINTER = VM_ERROR_BASE + 6,
    VM_DEST_HOST_NAME_NULL_POINTER = VM_ERROR_BASE + 7,
    VM_BUILDER_BUFFER_NULL_POINTER = VM_ERROR_BASE + 8,
    VM_MIGRATE_STRATEGY_NULL_POINTER = VM_ERROR_BASE + 9,
    VM_DEST_HOST_NAME_EMPTY = VM_ERROR_BASE + 10,
    VM_GET_NODE_TOPOLOGY_INFO_FAILED = VM_ERROR_BASE + 11,
    VM_VERIFY_RESPONSE_FAILED = VM_ERROR_BASE + 12,
    VM_CHECK_CIRCLE_FAILED = VM_ERROR_BASE + 13,
    VM_GET_HOSTNAME_NODEID_FAILED = VM_ERROR_BASE + 14,
    VM_GET_CUR_NODE_INFO_FAILED = VM_ERROR_BASE + 15,
    VM_GEN_CHECK_REQ_BODY_FAILED = VM_ERROR_BASE + 16,
    VM_ALLOCATE_MEM_FAILED = VM_ERROR_BASE + 17,
    VM_MEM_COPY_FAILED = VM_ERROR_BASE + 18,
    VM_REST_REP_FAILED = VM_ERROR_BASE + 19,
    VM_PARSE_CHECK_REP_BODY_FAILED = VM_ERROR_BASE + 20,
    VM_GET_REP_RESULT_FAILED = VM_ERROR_BASE + 21,
    VM_ERROR_SERIALIZE_FAILED = VM_ERROR_BASE + 22,
    VM_ERROR_DESERIALIZE_FAILED = VM_ERROR_BASE + 23,
    VM_DEST_UUID_EMPTY = VM_ERROR_BASE + 24
};

// In the UB environment, the memory threshold of the OneCopy migration policy is used for VMs. MB
const uint32_t UB_VM_MEMORY_BOUNDARY = 4 * 1024;

// In the HCCS environment, the memory threshold of the OneCopy migration policy is used for VMs. MB
const uint32_t HCCS_VM_MEMORY_BOUNDARY = 2 * 1024;

// The minimum memory threshold of the OneCopy migration policy. MB
const uint32_t MIN_VM_MEMORY_BOUNDARY = 256;

// The maximum memory threshold of the OneCopy migration policy. MB
const uint32_t MAX_VM_MEMORY_BOUNDARY = 4 * 1024;

// Huge page specifications required for ham migration. MB
const uint32_t HUGE_PAGE_SIZE = 2 * 1024;

const std::string UB_VM_MEMORY_BOUNDARY_KEY = "mig.migrateOneCopyMemoryBound";

class VirtMigrateStrategy {
public:
    static VmResult Register();

private:
    static uint32_t GetMigrateStrategy(const UbseIpcMessage& req, const UbseRequestContext& context);
    static uint32_t MakeMigrateStrategyDecision(uint32_t vmMemoryMB, const std::string& uuid,
                                                const std::string& destHostName, uint32_t destNumaId,
                                                uint32_t* migrateStrategy);
    static uint32_t GetMigrateOneCopyMemoryBound();
    static uint32_t MakeHamMigrateDecision(const std::string& uuid, const std::string& destHostName,
                                           uint32_t destNumaId, uint32_t* migrateStrategy);
    static uint32_t GetLocalMigrateInfo(MigrateInfoBase& migrateInfoLocal, const std::string& uuid);
    static uint32_t GetRemoteMigrateInfo(MigrateInfoBase& migrateInfoRemote, const std::string& destHostName,
                                         uint32_t destNumaId, const std::string& dstNid);
    static uint32_t GetMigrateInfo(MigrateInfoBase& migrateInfoLocal, MigrateInfoBase& migrateInfoRemote,
                                   const std::string& uuid, const std::string& destHostName, uint32_t destNumaId);
};
} // namespace vm

#endif // VM_MIGRATE_STRATEGY_H
