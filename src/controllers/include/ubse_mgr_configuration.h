/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MEM_STRATEGY_SERVER_CONFIGURATION_H
#define MEM_STRATEGY_SERVER_CONFIGURATION_H

#include "ubse_mem_constants.h"
#include "src/controllers/mem/algorithm/strategy/mem_pool_strategy.h"
#include "src/framework/config/ubse_conf_module.h"
#include "src/framework/context/ubse_context.h"
#include "src/framework/misc/ubse_str_util.h"
#include "ubse_logger_inner.h"

namespace ubse::mem::strategy {
using namespace ubse::log;
#define UBSE_LOG_ERROR UBSE_LOGGER_ERROR(gModuleName, gModuleId)
constexpr auto OCK_UBSE_MEMORY_HTRACE_PATH = "htrace.path";
// mem
constexpr auto MEM_MANAGER_CONFIG_SECTION_NAME = "ubse.memory";
constexpr auto MEM_LOG_CONFIG_SECTION_NAME = "ubse.log";
constexpr auto OCK_MEM_NODE_POOL_MEMORY_RATIO = "node.pool.memory.ratio";
constexpr auto OCK_MEM_SYSTEM_POOL_MEMORY_RATIO = "system.pool.memory.ratio";
constexpr auto OCK_VM_ENABLE = "vm";
constexpr auto OCK_UCACHE_ENABLE = "ucache";
constexpr auto OCK_MEM_SERVER_ALGO_LOG_LEVEL = "log.level";
// mem max borrow limit
constexpr auto OCK_MEM_BORROW_MAX_LIMIT = "borrow.max.limit";
// socket max importable memory
constexpr auto OCK_MEM_SOCKET_IMPORT_MAX_LIMIT = "socket.import.max.limit";

template <typename T>
ubse::common::def::UbseResult GetUbseConf(const std::string &section, const std::string &configKey, T &configValue)
{
    // 调取UbseConfModule类的单例对象
    auto &ctxRef = ubse::context::UbseContext::GetInstance();
    auto cfgPtr = ctxRef.GetModule<ubse::config::UbseConfModule>();
    if (cfgPtr == nullptr) {
        UBSE_LOGGER_ERROR("ubse", UBSE_CONTROLLER_MID) << "Failed to get configuration module instance, ";
        return UBSE_ERROR;
    }
    return cfgPtr->GetConf(section, configKey, configValue);
}

class MemMgrConfiguration {
public:
    void GetObmmSystemPoolMemRatioFromConf();

    [[nodiscard]] tc::rs::mem::LogLevel GetAlgoLogLevel() const;

    [[nodiscard]] std::string GetHtracePath() const;

    void GetBlockSizeFromConf();

    inline static MemMgrConfiguration &GetInstance()
    {
        static MemMgrConfiguration instance;
        return instance;
    }
    MemMgrConfiguration(const MemMgrConfiguration &other) = delete;
    MemMgrConfiguration(MemMgrConfiguration &&other) = delete;
    MemMgrConfiguration &operator=(const MemMgrConfiguration &other) = delete;
    MemMgrConfiguration &operator=(MemMgrConfiguration &&other) noexcept = delete;
    void Init();

    uint64_t GetSystemPoolMemRatio()
    {
        return systemPoolMemRatio;
    }

    uint64_t GetMaxBorrowSize()
    {
        return maxBorrowSize;
    }

    uint64_t GetUnitSize()
    {
        return blockSize;
    }

    uint64_t GetMaxSocketImportSize()
    {
        return maxSocketImportSize;
    }

private:
    uint64_t systemPoolMemRatio{MAX_OBMM_SYSTEM_POOL_MEM_RATIO};
    uint64_t maxBorrowSize{MAX_BORROW_MEM_PER_NODE};
    uint64_t blockSize{OBMM_MEMORY_BLOCK_SIZE_CONFIG * ONE_M};
    uint64_t maxSocketImportSize{MAX_IMPORT_MEM_SIZE_PER_SOCKET * ONE_M};
    MemMgrConfiguration() = default;
};
} // namespace ubse::mem::strategy

#endif // SERVER_CONFIGURATION_H