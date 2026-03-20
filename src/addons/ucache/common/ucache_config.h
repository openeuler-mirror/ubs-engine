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

#ifndef UCACHE_CONFIG_H
#define UCACHE_CONFIG_H

#include <cstdint>
#include <string>
#include <vector>

#include "ubse_conf.h"
#include "ubse_logger.h"
#include "ucache_config_check.h"
#include "ucache_error.h"

namespace ucache {
inline constexpr auto UCACHE_MODULE_NAME = "ucache_plugin";
#define UCACHE_MODULE_CODE UcacheConfig::GetInstance().GetModuleCode()

constexpr uint64_t MIN_BORROW_SIZE = 0x8000000UL;
constexpr uint64_t MAX_BORROW_SIZE = 0x100000000UL;
constexpr float MIN_BALANCE_THRESHOLD = 1.0f;

class UcacheConfig {
public:
    static UcacheConfig &GetInstance()
    {
        // 使用 C++11 的局部静态变量特性，线程安全
        static UcacheConfig instance;
        return instance;
    }

    uint32_t Initialize(const uint16_t modCode);

    inline uint16_t GetModuleCode()
    {
        return moduleCode;
    }
    uint32_t GetExportInterval() const;
    void SetNodeIds(std::vector<std::string> newNodeIds);
    std::vector<std::string> GetNodeIds() const;
    uint64_t GetBorrowSize() const;
    uint32_t GetMaxActionSize() const;
    float GetBalanceThreshold() const;
    float GetScarcityThreshold() const;
    uint32_t GetBottleneckThreshold() const;
    uint32_t GetBottleneckShortSize() const;
    uint32_t GetBottleneckShortThreshold() const;
    uint32_t GetBottleneckLongSize() const;
    uint32_t GetBottleneckLongThreshold() const;
    uint32_t GetMaxReliableTimes() const;
    uint32_t GetMasterInterval() const;
    uint64_t GetMasterMaxBorrowSize() const;

    UcacheConfig(const UcacheConfig &) = delete;
    UcacheConfig &operator=(const UcacheConfig &) = delete;

private:
    UcacheConfig() = default;
    ~UcacheConfig() = default;

    uint32_t LoadConfig();
    uint32_t LoadStrategyConfig();
    uint32_t LoadMasterConfig();
    uint32_t LoadBottleneckConfig();
    uint32_t RackGetUInt32AndCheck(const std::string &key, uint32_t &val);
    template <typename Func, typename ValueType>
    uint32_t GetConfigValue(Func &&func, const std::string &section, const std::string &key, ValueType &value)
    {
        uint32_t ret = func(section, key, value);
        if (ret != UCACHE_OK) {
            UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "get config value failed, section: " << section << ", key: " << key << ", ret: " << ret;
            return ret;
        }
        return UCACHE_OK;
    }
    void JudgeThreshold(uint32_t shortThreshold, uint32_t longThreshold);
    void JudgeWindowLength(uint32_t shortSize, uint32_t longSize);
    // 插件配置
    uint16_t moduleCode = 0;

    // 采集配置
    uint32_t exportInterval = 10;

    // 借用策略配置
    uint64_t borrowSize = 0x40000000UL;
    uint32_t maxActionSize = 10;
    float balanceThreshold = 1.5f;
    float scarcityThreshold = 0.01f;

    // 应用瓶颈识别配置
    uint32_t bottleneckThreshold;
    uint32_t bottleneckShortSize;
    uint32_t bottleneckShortThreshold;
    uint32_t bottleneckLongSize;
    uint32_t bottleneckLongThreshold;
    // 借用策略数据最大可靠周期
    uint32_t maxReliableTimes = 3;
    // master主流程间隔（s）
    uint32_t masterInterval = 10;
    // 最大借用内存量，默认4G
    uint64_t masterMaxBorrowSize = 0x100000000UL;

    // 通用配置
    std::vector<std::string> nodeIds{};
};

} // namespace ucache

#endif
