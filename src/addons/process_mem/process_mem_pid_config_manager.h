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
#ifndef PROCESS_MEM_PID_CONFIG_MANAGER_H
#define PROCESS_MEM_PID_CONFIG_MANAGER_H
#include "ubse_def.h"
#include "process_mem_pid_manager_def.h"
namespace process_mem::manager {
class ProcessMemPidConfigManager {
public:
    static void PersistPidConfigInfo(const def::ProcessMemPidInfo& pidInfo);

    static void GetAllPersistedPidConfigInfo(std::vector<def::ProcessMemPidInfo>& pidConfigInfos);

    static bool CheckPidConfigInfo(const def::ProcessMemPidInfo& pidInfo);

    static uint64_t GetExactStartTime(pid_t pid);

    static void QueryPidConfigCallback(const std::string& keyPrefix, const std::string& key, const UbseByteBuffer& buff,
                                       void* ctx);

    static void DeletePidConfigInfo(pid_t pid);

    static bool IsPidInfoExist(pid_t pid, uint64_t startTime);
};
} // namespace process_mem::manager
#endif