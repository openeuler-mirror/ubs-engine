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

#include "ubse_conf.h"
#include <map>
namespace ubse::config {
/**
 * @brief 获取无符号短整型类型配置
 * @param[in] section: 配置节
 * @param[in] configKey: 配置参数key
 * @param[out] configValue: 配置参数的值
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetUInt(const std::string& section, const std::string& configKey, uint32_t& configValue)
{
    const std::map<std::string, uint32_t> mockData = {{
        {"ucache.export.interval", 10},
        {"strategy.maxActionSize", 10},
        {"bottleneck.threshold", 10},
        {"bottleneck.short.size", 10},
        {"bottleneck.short.threshold", 10},
        {"bottleneck.long.size", 10},
        {"bottleneck.long.threshold", 10},
    }};
    auto it = mockData.find(configKey);
    if (it != mockData.end()) {
        configValue = it->second;
    }
    return 0;
}

/**
 * @brief 获取Float类型配置
 * @param[in] section: 配置节
 * @param[in] configKey: 配置参数key
 * @param[out] configValue: 配置参数的值
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetFloat(const std::string& section, const std::string& configKey, float& configValue)
{
    if (configKey == "strategy.balanceThreshold") {
        configValue = 1.5f;
    }
    return 0;
}

/**
 * @brief 获取字符串类型配置
 * @param[in] section: 配置节
 * @param[in] configKey: 配置参数key
 * @param[out] configValue: 配置参数的值
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGeStr(const std::string& section, const std::string& configKey, std::string& configValue)
{
    if (configKey == "nodeId") {
        configValue = "NODE11347";
    } else if (configKey == "nodeIds") {
        configValue = "NODE11347,NODE11348";
    }
    return 0;
}

/**
 * @brief 获取布尔类型配置
 * @param[in] section: 配置节
 * @param[in] configKey: 配置参数key
 * @param[out] configValue: 配置参数的值
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetBool(const std::string& section, const std::string& configKey, bool& configValue)
{
    return 0;
}

/**
 * @brief 获取无符号长整数类型配置
 * @param[in] section: 配置节
 * @param[in] configKey: 配置参数key
 * @param[out] configValue: 配置参数的值
 * @return 成功返回0, 失败返回非0
 */
uint32_t UbseGetULong(const std::string& section, const std::string& configKey, uint64_t& configValue)
{
    if (configKey == "strategy.borrow_size") {
        configValue = 0x40000000UL;
    }
    return 0;
}

/**
 * @brief 获取配置事件ID
 * @param[in] section: 配置节
 * @param[out] string, 返回配置事件ID
 */
void RackGetConfigEventId(const std::string& section, std::string& eventId)
{
    return;
}
} // namespace ubse::config