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
#include "ubse_mem_controller.h"
#include "mp_smap_module.h"
#include "process_mem_pid_manager_def.h"

namespace process_mem::pid::bridge {
constexpr const char* MEMPOOLING_PATH = "/usr/lib64/libmempooling.so";

using MigrateOut = std::function<int(const std::vector<mempooling::smap::MigrateOutPayload>&, int)>;
using Remove = std::function<int(const uint16_t, const std::vector<pid_t>&, int)>;
using NoMigrateBack = std::function<uint32_t(const std::string&)>;
using RemoteToRemote = std::function<int(const mempooling::smap::MigrateEscapeMsg&)>;
using ProcessConfigQuery = std::function<int(int, mempooling::smap::ProcessPayload*, int, int*)>;

class ProcessMemPidBridge {
public:
    static uint32_t Init();
    static uint32_t UnInit();

    static uint32_t RegisterConfigIpcHandlers();

    static uint32_t MemoryReturn(const std::string& name);

    static uint32_t SendReturnRequestToNode(const std::string& nodeId,
                                            const std::vector<def::ReturnRequestItem>& items);

    static void ProcessMemReturnRequestHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);

    inline static MigrateOut rmrsMigrateOut;
    inline static Remove rmrsRemove;
    inline static NoMigrateBack rmrsFreeWithMigrate;
    inline static RemoteToRemote rmrsRemoteToRemote;
    inline static ProcessConfigQuery rmrsProcessConfigQuery;
    inline static void* memPoolingHandle = nullptr;
};
} // namespace process_mem::pid::bridge
#endif
