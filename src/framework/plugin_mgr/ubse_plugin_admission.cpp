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

#include "ubse_plugin_admission.h"

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

UbseResult UbsePluginAdmission::ProcessPluginValue(const std::string &pkgName, const std::string &pkgValue)
{
    try {
        int tempValue = std::stoi(pkgValue);
        // 200，moduleCode需要大于200
        if (tempValue <= 200 ||
            (!std::all_of(pkgValue.begin(), pkgValue.end(), [](char c) { return std::isdigit(c); }))) {
            UBSE_LOG_ERROR << "Invalid argument, " << pkgName << "=" << pkgValue;
            return UBSE_ERROR_INVAL;
        }
        allowedPlugins[pkgName] = static_cast<uint16_t>(tempValue);
        return UBSE_OK;
    } catch (const std::invalid_argument &ret) {
        UBSE_LOG_ERROR << "Invalid argument, " << pkgName << "=" << pkgValue;
        return UBSE_ERROR_INVAL;
    } catch (const std::out_of_range &ret) {
        UBSE_LOG_ERROR << "Out of range for int, " << pkgName << "=" << pkgValue;
        return UBSE_ERROR;
    }
}

UbseResult UbsePluginAdmission::LoadAdmissionConfig()
{
    std::map<std::string, std::map<std::string, std::string>> configVals;

    auto ubseConfModule = UbseContext::GetInstance().GetModule<UbseConfModule>();
    if (ubseConfModule == nullptr) {
        UBSE_LOG_ERROR << "Unexpected nullptr value ubseConfModule";
        return UBSE_ERROR_NULLPTR;
    }

    UbseResult ret = ubseConfModule->GetAllConfigWithPrefix("ubse_plugin_admission", configVals);
    if (ret != UBSE_OK && ret != UBSE_CONF_ERROR_KEY_OFFSETCONFIG_NO_PREFIX &&
        ret != UBSE_CONF_ERROR_KEY_OFFSETCONFIG_PREFIX_NO_CONTENT) {
        UBSE_LOG_ERROR << "Failed to read plugin config, " << FormatRetCode(ret);
        return UBSE_ERROR;
    }

    if (configVals.empty()) {
        UBSE_LOG_WARN << "No admission plugin configuration is read";
        return UBSE_OK;
    }
    std::unique_lock<std::shared_mutex> lock(allowedPluginsMutex);
    for (const auto &item : configVals) {
        for (const auto &kvMap : item.second) {
            std::string pkgName = kvMap.first;
            std::string pkgValue = kvMap.second;
            if (allowedPlugins.find(pkgName) != allowedPlugins.end()) {
                UBSE_LOG_WARN << "The plugin name: " << pkgName << " has been configuration in admission file.";
                continue;
            }
            UbseResult result = ProcessPluginValue(pkgName, pkgValue);
            if (result != UBSE_OK) {
                return result;
            }
        }
    }
    return UBSE_OK;
}

const std::map<std::string, uint16_t> &UbsePluginAdmission::GetAllowedPlugins() const
{
    std::shared_lock<std::shared_mutex> lock(allowedPluginsMutex);
    return allowedPlugins;
}

const uint16_t *UbsePluginAdmission::GetPluginConfig(const std::string &pluginName) const
{
    std::shared_lock<std::shared_mutex> lock(allowedPluginsMutex);
    auto item = allowedPlugins.find(pluginName);
    if (item == allowedPlugins.end()) {
        return nullptr;
    }
    return &item->second;
}
} // namespace ubse::plugin
