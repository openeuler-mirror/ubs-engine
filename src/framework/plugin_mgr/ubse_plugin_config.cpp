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

#include "ubse_plugin_config.h"

#include <map>

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_logger_module.h"

namespace ubse::plugin {
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::log;

UBSE_DEFINE_THIS_MODULE("ubse", UBSE_PLUGIN_MID)

const std::string PLUGIN_FILE_PREFIX = "plugin";

UbseResult UbsePluginConfig::LoadPluginConfigs()
{
    std::map<std::string, std::map<std::string, std::string>> configVals;

    auto ubseConfModule = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (ubseConfModule == nullptr) {
        UBSE_LOG_ERROR << "Unexpected nullptr value ubseConfModule";
        return UBSE_ERROR_NULLPTR;
    }

    UbseResult ret = ubseConfModule->GetAllConfigWithPrefix(PLUGIN_FILE_PREFIX, configVals);
    if (ret != UBSE_OK && ret != UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_PREFIX &&
        ret != UBSE_CONF_ERROR_KEY_OFFSETCONFIG_PREFIX_NO_CONTENT) {
        UBSE_LOG_WARN << "Failed to read plugin config," << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    if (configVals.empty()) {
        UBSE_LOG_ERROR << "No plugin configuration is read";
        return UBSE_OK;
    }
    std::unique_lock<std::shared_mutex> lock(pluginConfigsMutex);
    for (const auto &item : configVals) {
        std::map<std::string, std::string> kvMap = item.second;

        UbsePluginInfo ubsePluginConfig;
        ubsePluginConfig.name = kvMap["ubse.plugin.name"];
        ubsePluginConfig.pkg = kvMap["ubse.plugin.pkg"];
        ret = VerifyConfig(ubsePluginConfig, item.first);
        if (ret != UBSE_OK) {
            continue;
        }

        pluginConfigs[ubsePluginConfig.name] = ubsePluginConfig;
    }
    return UBSE_OK;
}

const std::map<std::string, UbsePluginInfo> &UbsePluginConfig::GetAllPluginConfigs() const
{
    std::shared_lock<std::shared_mutex> lock(pluginConfigsMutex);
    return pluginConfigs;
}

const UbsePluginInfo *UbsePluginConfig::GetPluginConfig(const std::string &pluginName) const
{
    std::shared_lock<std::shared_mutex> lock(pluginConfigsMutex);
    auto item = pluginConfigs.find(pluginName);
    if (item == pluginConfigs.end()) {
        return nullptr;
    }
    return &item->second;
}

UbseResult UbsePluginConfig::VerifyConfig(const UbsePluginInfo &pluginInfo, const std::string &fileName)
{
    // 空文件过滤
    if (pluginInfo.name.empty()) {
        UBSE_LOG_WARN << "plugin name is empty. related file: " << fileName;
        return UBSE_ERROR;
    }

    // 重复配置文件
    if (pluginConfigs.find(pluginInfo.name) != pluginConfigs.end()) {
        UBSE_LOG_WARN << "The plugin name: " << pluginInfo.name
                    << " has been configuration. related file: " << fileName;
        return UBSE_ERROR;
    }
    return UBSE_OK;
}
} // namespace ubse::plugin
