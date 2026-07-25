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

#ifndef UBSE_PLUGIN_CONFIG_H
#define UBSE_PLUGIN_CONFIG_H
#include <mutex>
#include <optional>
#include <shared_mutex>

#include "ubse_common_def.h"

namespace ubse::plugin {
using ubse::common::def::UbseResult;

struct UbsePluginInfo {
    /* *
     * 插件的名称 ubse.plugin.name
     */
    std::string name;
    /* *
     * 插件的包名 ubse.plguin.pkg
     */
    std::string pkg;

    bool operator==(const UbsePluginInfo& other) const
    {
        return name == other.name && pkg == other.pkg;
    }
};

class UbsePluginConfig {
public:
    UbseResult LoadPluginConfigs();
    ;
    // 增删改查方法
    const std::map<std::string, UbsePluginInfo>& GetAllPluginConfigs() const;

private:
    UbseResult VerifyConfig(const UbsePluginInfo& pluginInfo, const std::string& fileName);
    mutable std::shared_mutex pluginConfigsMutex_;
    std::map<std::string, UbsePluginInfo> pluginConfigs_; // 存储所有插件的配置信息
};
} // namespace ubse::plugin
#endif // UBSE_PLUGIN_CONFIG_H
