/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef PROCESS_MEM_PID_BRIDGE_H
#define PROCESS_MEM_PID_BRIDGE_H
#include <vector>

#include "ubse_def.h"
#include "ubse_ras.h"
#include "process_mem_pid_manager_def.h"
#include "src/include/ubse_mem_controller.h"

namespace process_mem::pid::bridge {
constexpr const char* MEMPOOLING_PATH = "/usr/lib64/libmempooling.so";

enum class MigrateMode : uint8_t {
    MIG_RATIO_MODE = 0, // 按照比例迁移
    MIG_MEMSIZE_MODE    // 按照内存大小迁移，单位kb
};

const int REMOTE_NUMA_NUM = 18;
const int SMAP_RATIO_MP = 25;

struct MigrateOutPayloadInner {
    int destNid{};             // remoteNumaId
    int ratio = SMAP_RATIO_MP; // 默认冷数据迁移比例
    uint64_t memSize{};                 // 内存迁移大小(KB)
    MigrateMode migrateMode{MIG_RATIO_MODE}; // 迁移模式，按比例或是大小
};

struct MigrateOutPayload {
    int srcNid{-1}; // 是否指定迁出源节点（-1表示不指定）
    pid_t pid{};    // 待迁移冷数据的虚机实例进程pid
    int count{};
    struct MigrateOutPayloadInner inner[REMOTE_NUMA_NUM]{};
};

using MigrateOut = std::function<int(const std::vector<MigrateOutPayload>&, int)>;
using Remove = std::function<int(const uint16_t, const std::vector<pid_t>&, int)>;
using NoMigrateBack = std::function<uint32_t (const std::string&)>;

struct MemoryBorrowRequest {
    std::string name;
    uint64_t size;
    uint8_t usrInfo[ubse::mem::controller::UBSE_MAX_USR_INFO_LEN]{};
};

class ProcessMemPidBridge {
public:
    static uint32_t Init();
    static uint32_t UnInit();
    static uint32_t MemoryBorrow(def::ProcessMemPidInfo& pidInfo,
                                 const ubse::mem::controller::UbseMemBorrower& borrower,
                                 const MemoryBorrowRequest& request,
                                 ubse::mem::controller::UbseMemNumaDesc& borrowInfo);

    static uint32_t MemoryReturn(const std::string& name);

    static uint32_t GetRemoteNumaSocketInfo(const ubse::mem::controller::UbseMemNumaDesc& desc, uint32_t& socketId,
                                            uint64_t& numaId);
    inline static MigrateOut rmrsMigrateOut;
    inline static Remove rmrmRemove;
    inline static NoMigrateBack rmrsFreeWithMigrate;
    inline static void* memPoolingHandle = nullptr;
    static uint32_t FaultHandler(ubse::ras::ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo);
};
} // namespace process_mem::pid::bridge
#endif
