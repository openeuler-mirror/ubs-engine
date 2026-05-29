/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ucache_config.h"

#include <cstdint>
#include <iostream>
#include <mutex>
#include <sstream>
#include <vector>
#include "ubse_election.h"

namespace ucache {
using namespace ubse::config;
using namespace ubse::log;
using namespace ubse::election;

uint32_t UcacheConfig::Initialize(const uint16_t modCode)
{
    moduleCode = modCode;
    return LoadConfig();
}

uint32_t UcacheConfig::RackGetUInt32AndCheck(const std::string& key, uint32_t& val)
{
    // find range from key
    auto it = Uint32RangeCheck.find(key);
    if (it == Uint32RangeCheck.end()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "key not found in Uint32RangeCheck, key: " << key;
        return UCACHE_ERR;
    }
    const auto& range = it->second;

    // load val from .conf file
    uint32_t ret = UbseGetUInt("plugin_ucache", key, val);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Get config failed, key: " << key << ", ret: " << ret;
        if (!range.critical) {
            val = range.defaultValue;
            return UCACHE_OK;
        }
        return ret;
    }
    // execute range check
    if (val < range.minValue || val > range.maxValue) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "The value of the key is invalid, key: " << key << ", val: " << val << ", should range from "
            << range.minValue << " to " << range.maxValue;
        if (!range.critical) {
            val = range.defaultValue;
            return UCACHE_OK;
        }
        return UCACHE_ERR;
    }
    return UCACHE_OK;
}
void UcacheConfig::JudgeWindowLength(uint32_t shortSize, uint32_t longSize)
{
    if (shortSize < longSize) {
        UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "bottleneckShortSize = " << shortSize << " and bottleneckLongSize = " << longSize << "is valid";
    } else if (shortSize >= longSize) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "bottleneckShortSize = " << shortSize << ", bottleneckLongSize = " << longSize;
    }
}

void UcacheConfig::JudgeThreshold(uint32_t shortThreshold, uint32_t longThreshold)
{
    if (shortThreshold >= longThreshold) {
        UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "bottleneckShortThreshold = " << shortThreshold << ", bottleneckLongThreshold = " << longThreshold;
    } else if (shortThreshold < longThreshold) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "bottleneckShortThreshold = " << shortThreshold << ", bottleneckLongThreshold = " << longThreshold
            << "is valid";
    }
}

uint32_t UcacheConfig::LoadBottleneckConfig()
{
    std::vector<std::pair<std::string, uint32_t&>> configs = {{"bottleneck.threshold", bottleneckThreshold},
                                                              {"bottleneck.short.size", bottleneckShortSize},
                                                              {"bottleneck.short.threshold", bottleneckShortThreshold},
                                                              {"bottleneck.long.size", bottleneckLongSize},
                                                              {"bottleneck.long.threshold", bottleneckLongThreshold}};

    for (auto& [key, value] : configs) {
        uint32_t ret = RackGetUInt32AndCheck(key, value);
        if (ret != UCACHE_OK) {
            return ret;
        }
    }

    JudgeThreshold(bottleneckShortThreshold, bottleneckLongThreshold);

    JudgeWindowLength(bottleneckShortThreshold, bottleneckLongThreshold);

    return UCACHE_OK;
}

uint32_t UcacheConfig::LoadStrategyConfig()
{
    uint32_t ret = UCACHE_OK;
    if ((ret = GetConfigValue(UbseGetULong, "plugin_ucache", "strategy.borrow_size", borrowSize)) != UCACHE_OK) {
        return ret;
    }

    if (borrowSize < MIN_BORROW_SIZE || borrowSize > MAX_BORROW_SIZE) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "strategy.borrow_size is invalid, should range from " << MIN_BORROW_SIZE << " to " << MAX_BORROW_SIZE
            << ", current value: " << borrowSize;
        return UCACHE_ERR;
    }

    if ((ret = GetConfigValue(UbseGetUInt, "plugin_ucache", "strategy.maxActionSize", maxActionSize)) != UCACHE_OK) {
        return ret;
    }

    ret = GetConfigValue(UbseGetFloat, "plugin_ucache", "strategy.balanceThreshold", balanceThreshold);
    if (ret != UCACHE_OK) {
        return ret;
    }
    if (balanceThreshold < MIN_BALANCE_THRESHOLD) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "strategy.balanceThreshold is invalid, should large than " << MIN_BALANCE_THRESHOLD
            << ", current value: " << balanceThreshold;
        return UCACHE_ERR;
    }

    ret = GetConfigValue(UbseGetFloat, "plugin_ucache", "strategy.scarcityThreshold", scarcityThreshold);
    if (ret != UCACHE_OK) {
        return ret;
    }
    if (scarcityThreshold < 0) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "strategy.scarcityThreshold is invalid, should not less than "
            << "0"
            << ", current value: " << scarcityThreshold;
        return UCACHE_ERR;
    }

    ret = RackGetUInt32AndCheck("strategy.maxReliableTimes", maxReliableTimes);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Get config failed, key: strategy.maxReliableTimes, ret: " << ret;
    }
    return ret;
}

const uint32_t MAX_MASTER_INTERVAL = 15;
const uint32_t MIN_MASTER_INTERVAL = 1;
uint32_t UcacheConfig::LoadMasterConfig()
{
    uint32_t ret = UCACHE_OK;
    ret = UbseGetUInt("plugin_ucache", "master.masterInterval", masterInterval);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Get config failed, key: master.masterInterval, ret: " << ret;
        return ret;
    }
    if (masterInterval > MAX_MASTER_INTERVAL || masterInterval < MIN_MASTER_INTERVAL) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "masterInterval is invalid,  should range from " << MIN_MASTER_INTERVAL << " to " << MAX_MASTER_INTERVAL
            << ", current value: " << masterInterval;
        return UCACHE_ERR;
    }

    ret = UbseGetULong("plugin_ucache", "master.maxBorrowSize", masterMaxBorrowSize);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Get config failed, key: master.maxBorrowSize, ret: " << ret;
        return ret;
    }

    if (masterMaxBorrowSize % borrowSize != 0) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "master.maxBorrowSize is invalid, should be an integer multiple of " << borrowSize
            << ", current value: " << masterMaxBorrowSize;
        return UCACHE_ERR;
    }

    return ret;
}

uint32_t UcacheConfig::LoadConfig()
{
    uint32_t ret = UCACHE_OK;

    if ((ret = UbseGetNodeIds(nodeIds)) != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "UbseGetNodeIds failed.";
        return ret;
    }
    for (auto id : nodeIds) {
        UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "nodeId is: " << id;
    }
    if (nodeIds.empty()) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "nodeIds is empty.";
        return UCACHE_ERR;
    }

    if ((ret = GetConfigValue(UbseGetUInt, "plugin_ucache", "ucache.export.interval", exportInterval)) != UCACHE_OK) {
        return ret;
    }

    if (exportInterval <= 0) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "exportInterval is invalid, exportInterval: " << exportInterval;
        return UCACHE_ERR;
    }

    if ((ret = LoadMasterConfig()) != UCACHE_OK) {
        return ret;
    }

    if ((ret = LoadStrategyConfig()) != UCACHE_OK) {
        return ret;
    }

    if ((ret = LoadBottleneckConfig()) != UCACHE_OK) {
        return ret;
    }

    return UCACHE_OK;
}

void UcacheConfig::SetNodeIds(std::vector<std::string> newNodeIds)
{
    nodeIds = newNodeIds;
}

std::vector<std::string> UcacheConfig::GetNodeIds() const
{
    return nodeIds;
}

uint32_t UcacheConfig::GetExportInterval() const
{
    return exportInterval;
}

uint64_t UcacheConfig::GetBorrowSize() const
{
    return borrowSize;
}

uint32_t UcacheConfig::GetMaxActionSize() const
{
    return maxActionSize;
}

float UcacheConfig::GetBalanceThreshold() const
{
    return balanceThreshold;
}

float UcacheConfig::GetScarcityThreshold() const
{
    return scarcityThreshold;
}

uint32_t UcacheConfig::GetBottleneckThreshold() const
{
    return bottleneckThreshold;
}
uint32_t UcacheConfig::GetBottleneckShortSize() const
{
    return bottleneckShortSize;
}
uint32_t UcacheConfig::GetBottleneckShortThreshold() const
{
    return bottleneckShortThreshold;
}
uint32_t UcacheConfig::GetBottleneckLongSize() const
{
    return bottleneckLongSize;
}
uint32_t UcacheConfig::GetBottleneckLongThreshold() const
{
    return bottleneckLongThreshold;
}

uint32_t UcacheConfig::GetMaxReliableTimes() const
{
    return maxReliableTimes;
}

uint32_t UcacheConfig::GetMasterInterval() const
{
    return masterInterval;
}

uint64_t UcacheConfig::GetMasterMaxBorrowSize() const
{
    return masterMaxBorrowSize;
}

} // namespace ucache
