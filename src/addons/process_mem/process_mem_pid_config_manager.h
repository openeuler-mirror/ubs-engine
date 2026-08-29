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
    static uint64_t GetExactStartTime(pid_t pid);

    static uint32_t PersistProcMemConfig(const def::ProcessMemNewConfigInfo& config);
    static void GetAllPersistedProcMemConfigs(std::vector<def::ProcessMemNewConfigInfo>& configs);
    static uint32_t DeleteProcMemConfig(bool isPid, const std::string& identifier);

    static uint32_t PersistFossilConfig(pid_t pid, const def::FossilPidConfigInfo& fossil);
    static uint32_t DeleteFossilConfig(pid_t pid);
    static void GetAllFossilConfigs(std::vector<std::pair<pid_t, def::FossilPidConfigInfo>>& fossils);

    static void QueryProcMemConfigCallback(const std::string& keyPrefix, const std::string& key,
                                           const UbseByteBuffer& buff, void* ctx);
    static void QueryFossilConfigCallback(const std::string& keyPrefix, const std::string& key,
                                          const UbseByteBuffer& buff, void* ctx);
};
} // namespace process_mem::manager
#endif
