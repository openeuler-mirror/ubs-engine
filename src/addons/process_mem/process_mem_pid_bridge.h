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
#include <array>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "ubse_def.h"
#include "ubse_mem_controller.h"
#include "mp_smap_module.h"
#include "process_mem_pid_manager_def.h"

namespace process_mem::pid::bridge {
using MigrateOut = std::function<int(const std::vector<mempooling::smap::MigrateOutPayload>&, int)>;
using Remove = std::function<int(const uint16_t, const std::vector<pid_t>&, int)>;
using NoMigrateBack = std::function<uint32_t(const std::string&)>;
using RemoteToRemote = std::function<int(const mempooling::smap::MigrateEscapeMsg&)>;
using ProcessConfigQuery = std::function<int(int, mempooling::smap::ProcessPayload*, int, int*)>;

class ProcessMemPidBridge {
public:
    // 同 pid 的迁出/迁回/归还串行化: smap migrateOut payload 为全量预期迁出声明,
    // 借出、被动归还、再平衡、对账并发下发会互相覆盖导致账实不符
    static std::mutex& GetPidOpMutex(pid_t pid)
    {
        return pidOpMutexes_[static_cast<size_t>(pid) % kPidOpMutexNum];
    }

    static uint32_t Init();
    static uint32_t UnInit();

    static uint32_t RegisterConfigIpcHandlers();

    static uint32_t MemoryReturn(const std::string& name);

    static int MigrateOutToNumas(pid_t pid, const std::vector<std::pair<int, uint64_t>>& numaTargetsBytes,
                                 const std::string& logCtx = {});

    static uint32_t SendReturnRequestToNode(const std::string& nodeId,
                                            const std::vector<def::ReturnRequestItem>& items);

    static void ProcessMemReturnRequestHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);

    inline static MigrateOut rmrsMigrateOut;
    inline static Remove rmrsRemove;
    inline static NoMigrateBack rmrsFreeWithMigrate;
    inline static RemoteToRemote rmrsRemoteToRemote;
    inline static ProcessConfigQuery rmrsProcessConfigQuery;
    inline static void* memPoolingHandle = nullptr;

private:
    static constexpr uint32_t kPidOpMutexNum = 64;
    inline static std::array<std::mutex, kPidOpMutexNum> pidOpMutexes_{};
};
} // namespace process_mem::pid::bridge
#endif
