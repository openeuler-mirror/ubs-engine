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

#include "ubse_http_module.h"
#include "ubse_node_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_plugin_module.h"

namespace ubse::plugin {
DYNAMIC_CREATE(UbsePluginModule, ubse::node::UbseNodeModule);
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_PLUGIN_MID)
UbseResult UbsePluginModule::Initialize()
{
    return UBSE_OK;
}

void UbsePluginModule::UnInitialize() {}

UbseResult UbsePluginModule::Start()
{
    return ubsePluginManager.Start();
}

void UbsePluginModule::Stop()
{
    ubsePluginManager.DeInitializePlugins();
}

void *UbsePluginModule::GetLoadedPlugin(const std::string &pluginName)
{
    return ubsePluginManager.GetLoadedPlugin(pluginName);
}

bool UbsePluginModule::GetPluginLoaded(const std::string &pluginName)
{
    return ubsePluginManager.GetLoadedPlugin(pluginName) != nullptr;
}
void UbsePluginModule::GetLoadedPluginsName(std::vector<std::string> &plugins)
{
    ubsePluginManager.GetLoadedPlugins(plugins);
}
bool UbsePluginModule::GetPluginReadyStatus(const std::string &pluginName)
{
    std::shared_lock<std::shared_mutex> lock(pluginReadyMapMutex);
    auto item = pluginReadyMap.find(pluginName);
    if (item == pluginReadyMap.end()) {
        return false;
    }
    return item->second;
}
void UbsePluginModule::NotifyPluginReadyStatus(const std::string &pluginName, bool flag)
{
    std::unique_lock<std::shared_mutex> lock(pluginReadyMapMutex);
    UBSE_LOG_DEBUG << "plugin: " << pluginName << " notify ready flag: " << flag;
    pluginReadyMap[pluginName] = flag;
}
} // namespace ubse::plugin