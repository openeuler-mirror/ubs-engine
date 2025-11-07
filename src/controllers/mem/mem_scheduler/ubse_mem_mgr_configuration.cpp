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

#include "ubse_mgr_configuration.h"
namespace ubse::mem::strategy {

void MemMgrConfiguration::Init()
{
    GetObmmSystemPoolMemRatioFromConf();
    GetBlockSizeFromConf();
}

[[nodiscard]] tc::rs::mem::LogLevel MemMgrConfiguration::GetAlgoLogLevel() const
{
    std::string tmp;
    if (GetUbseConf(MEM_LOG_CONFIG_SECTION_NAME, OCK_MEM_SERVER_ALGO_LOG_LEVEL, tmp) != UBSE_OK) {
        UBSE_LOGGER_ERROR("ubse", UBSE_CONTROLLER_MID)
            << "rmMemConfigInitFuncs get error , can not get " << OCK_MEM_SERVER_ALGO_LOG_LEVEL;
        return {};
    }
    if (tmp == "DEBUG") {
        UBSE_LOGGER_DEBUG("ubse", UBSE_CONTROLLER_MID) << "The algo log level is debug.";
        return tc::rs::mem::LogLevel::DEBUG;
    }
    if (tmp == "INFO") {
        UBSE_LOGGER_DEBUG("ubse", UBSE_CONTROLLER_MID) << "The algo log level is info.";
        return tc::rs::mem::LogLevel::INFO;
    }
    if (tmp == "WARN") {
        UBSE_LOGGER_DEBUG("ubse", UBSE_CONTROLLER_MID) << "The algo log level is warn.";
        return tc::rs::mem::LogLevel::WARN;
    }
    if (tmp == "ERROR") {
        UBSE_LOGGER_DEBUG("ubse", UBSE_CONTROLLER_MID) << "The algo log level is error.";
        return tc::rs::mem::LogLevel::ERROR;
    }
    return tc::rs::mem::LogLevel::ERROR;
}

[[nodiscard]] std::string MemMgrConfiguration::GetHtracePath() const
{
    std::string tmp;
    if (GetUbseConf(MEM_MANAGER_CONFIG_SECTION_NAME, OCK_UBSE_MEMORY_HTRACE_PATH, tmp) != UBSE_OK) {
        UBSE_LOGGER_ERROR("ubse", UBSE_CONTROLLER_MID)
            << "rmMemConfigInitFuncs get error " << OCK_UBSE_MEMORY_HTRACE_PATH;
        return {};
    }
    return tmp;
}

void MemMgrConfiguration::GetObmmSystemPoolMemRatioFromConf()
{
    std::string tmp;
    if (GetUbseConf(MEM_MANAGER_CONFIG_SECTION_NAME, OCK_MEM_SYSTEM_POOL_MEMORY_RATIO, tmp) != UBSE_OK) {
        UBSE_LOGGER_ERROR("ubse", UBSE_CONTROLLER_MID)
            << "rmMemConfigInitFuncs get error " << OCK_MEM_SYSTEM_POOL_MEMORY_RATIO
            << " use default value: " << MAX_OBMM_SYSTEM_POOL_MEM_RATIO;
        return;
    }

    if (ubse::utils::ConvertStrToUint64(tmp, systemPoolMemRatio) != UBSE_OK) {
        UBSE_LOGGER_ERROR("ubse", UBSE_CONTROLLER_MID) << "Failed to convert str to unsigned long, confItemStr" << tmp
                                                       << " use default value:" << MAX_OBMM_SYSTEM_POOL_MEM_RATIO;
    }
    return;
}

void MemMgrConfiguration::GetBlockSizeFromConf()
{
    static const std::unordered_set<std::string> legalValues{"128", "256", "512", "1024"};
    std::string value;
    auto ret = GetUbseConf(MEM_MANAGER_CONFIG_SECTION_NAME, OBMM_MEMORY_BLOCK_SIZE_CONFIG_KEY, value);
    if (ret == UBSE_OK && (legalValues.find(value) == legalValues.end())) {
        UBSE_LOGGER_ERROR("ubse", UBSE_CONTROLLER_MID)
            << "rmMemConfigInitFuncs get error" << value << " use default value: " << OBMM_MEMORY_BLOCK_SIZE_CONFIG;
        return;
    }
    uint64_t intValue = 0;
    if (ubse::utils::ConvertStrToUint64(value, intValue) != UBSE_OK) {
        UBSE_LOGGER_ERROR("ubse", UBSE_CONTROLLER_MID) << "Failed to convert str to unsigned long, confItemStr" << value
                                                       << " use default value: " << OBMM_MEMORY_BLOCK_SIZE_CONFIG;
        return;
    }
    blockSize = intValue * ONE_M;
    return;
}

} // namespace ubse::mem::strategy
