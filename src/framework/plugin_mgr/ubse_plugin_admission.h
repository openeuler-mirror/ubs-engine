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

#ifndef UBSE_PLUGIN_ADMISSION_H
#define UBSE_PLUGIN_ADMISSION_H
#include <mutex>
#include <optional>
#include <shared_mutex>

#include "ubse_common_def.h"

namespace ubse::plugin {
using ubse::common::def::UbseResult;
class UbsePluginAdmission {
public:
    UbseResult LoadAdmissionConfig();
    UbseResult ProcessPluginValue(const std::string& pkgName, const std::string& pkgValue);

    // 增删改查方法
    const std::map<std::string, uint16_t>& GetAllowedPlugins() const;
    std::optional<uint16_t> GetPluginConfig(const std::string& pluginName) const;

private:
    mutable std::shared_mutex allowedPluginsMutex_;
    std::map<std::string, uint16_t> allowedPlugins_; // 存储允许加载的插件
};
} // namespace ubse::plugin
#endif // UBSE_PLUGIN_ADMISSION_H
