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

#ifndef PROCESS_MEM_PID_COLLECT_H
#define PROCESS_MEM_PID_COLLECT_H

#include <sys/types.h>
#include <cstdint>
#include <functional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "process_mem_pid_manager_def.h"

namespace process_mem::collect {

using CollectInfoMap = std::unordered_map<pid_t, std::unordered_map<uint32_t, size_t>>;
// std::unordered_map<pid, std::unordered_map<numa, memSize>>, 每个numa上内存分布单位为 byte.
using NumaMemDistributionCollectHandler = std::function<void(CollectInfoMap)>;

class ProcessMemPidCollect {
public:
    static ProcessMemPidCollect& GetInstance()
    {
        static ProcessMemPidCollect instance;
        return instance;
    }

    uint32_t Init();

    void UnInit();

    uint32_t CycleCollectNumaInfo();

    uint32_t RegisterCollectHandler(const std::string& name, NumaMemDistributionCollectHandler handler);

    void UnRegisterCollectHandler(const std::string& name);

    uint32_t CollectProcessNumaMemDistribution(pid_t pid, std::unordered_map<uint32_t, size_t>& numaMemDistribution);

    void CollectChildPids(pid_t parentPid, const def::ProcessMemPidConfigInfo& parentConfig,
                          CollectInfoMap& pidCollectInfo);

private:
    std::unordered_map<std::string, NumaMemDistributionCollectHandler> collectHandlers{};

    std::shared_mutex collectHandlersMutex{};
};
} // namespace process_mem::collect

#endif // PROCESS_MEM_PID_COLLECT_H
