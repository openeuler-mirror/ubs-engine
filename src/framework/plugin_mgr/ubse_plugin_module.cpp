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
#include "ubse_plugin_module.h"
#include "ubse_http_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_module.h"

namespace ubse::plugin {
CORE_MODULE_IMPL(UbsePluginModule);
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbsePluginModule::Initialize()
{
    return UBSE_OK;
}

void UbsePluginModule::UnInitialize() {
}

UbseResult UbsePluginModule::Start()
{
    return ubsePluginManager_.Start();
}

void UbsePluginModule::Stop()
{
    ubsePluginManager_.DeInitializePlugins();
}

bool UbsePluginModule::GetPluginLoaded(const std::string &pluginName)
{
    return ubsePluginManager_.GetLoadedPlugin(pluginName) != nullptr;
}
bool UbsePluginModule::GetPluginReadyStatus(const std::string &pluginName)
{
    std::shared_lock<std::shared_mutex> lock(pluginReadyMapMutex_);
    auto item = pluginReadyMap_.find(pluginName);
    if (item == pluginReadyMap_.end()) {
        return false;
    }
    return item->second;
}
void UbsePluginModule::NotifyPluginReadyStatus(const std::string &pluginName, bool flag)
{
    std::unique_lock<std::shared_mutex> lock(pluginReadyMapMutex_);
    UBSE_LOG_DEBUG << "plugin: " << pluginName << " notify ready flag: " << flag;
    pluginReadyMap_[pluginName] = flag;
}
} // namespace ubse::plugin