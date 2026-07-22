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

#include "ubse_plugin_manager.h"

#include <dlfcn.h>
#include <securec.h>
#include <iostream>

#include "ubse_error.h"
#include "ubse_file_util.h"
#include "ubse_json_util.h"
#include "ubse_logger.h"

namespace ubse::plugin {
using namespace ubse::context;
using namespace ubse::log;
using namespace ubse::utils;
UBSE_DEFINE_THIS_MODULE("ubse");

static const std::unordered_set<std::string> g_pluginCannotFail{"mempooling", "virt_agent"};

/**
 * 加载并初始化准入插件
 * 1. 读取插件配置： 名称、包名、加载进程
 * 2. 读取准入配置
 * 3. 加载准入配置so，调用UbsePluginInit初始化插件
 * @return UbseResult
 */
UbseResult UbsePluginManager::Start()
{
    // 读取插件配置： 名称、包名、加载进程
    UbseResult ret = ubsePluginConfig_.LoadPluginConfigs();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "plugin config load failed, ret: " << FormatRetCode(ret);
        return UBSE_PLUGIN_ERROR_FILE_LOADED_ERROR;
    }

    // 读取准入配置
    ret = ubsePluginAdmission_.LoadAdmissionConfig();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "plugin admission config load failed," << FormatRetCode(ret);
        return UBSE_PLUGIN_ERROR_FILE_LOADED_ERROR;
    }

    // 加载准入配置so，调用UbsePluginInit初始化插件
    ret = LoadAndInitPlugins();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "plugin load and init failed," << FormatRetCode(ret);
        return UBSE_PLUGIN_ERROR_FILE_LOADED_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbsePluginManager::LoadAndInitPlugins()
{
    const auto& configs = ubsePluginConfig_.GetAllPluginConfigs();
    for (const auto& [pluginName, pluginInfo] : configs) {
        UBSE_LOG_INFO << "starting to load plugin; plugin name: " << pluginName.c_str();
        if (const auto ret = LoadAndInitPlugin(pluginName, pluginInfo.pkg); UBSE_OK != ret) {
            if (g_pluginCannotFail.find(pluginName) != g_pluginCannotFail.end() &&
                ret == UBSE_PLUGIN_ERROR_PLUGIN_INIT_FAILED) {
                UBSE_LOG_ERROR << "plugin init failed, related plugin name: " << pluginName << ".";
                std::cerr << "The plugin init failed. For details about the plugin, see the ubse log file, "
                             "related plugin name: "
                          << pluginName << "." << std::endl;
                return ret;
            }
            UBSE_LOG_WARN << "plugin is not loaded, related plugin name: " << pluginName << ".";
            std::cout << "The plugin is not loaded. For details about the plugin, see the ubse log file, "
                         "related plugin name: "
                      << pluginName << "." << std::endl;
            continue;
        }
        UBSE_LOG_INFO << "plugin: " << pluginName << " loaded successfully.";
    }
    return UBSE_OK;
}

UbseResult UbsePluginManager::LoadAndInitPlugin(const std::string& pluginName, const std::string& fileName)
{
    std::unique_lock<std::shared_mutex> lock(loadedPluginModulesMutex_);
    auto moduleCode = ubsePluginAdmission_.GetPluginConfig(pluginName);
    if (!moduleCode.has_value()) {
        UBSE_LOG_WARN
            << "plugin configuration is available, but no admission configuration is available, related plugin name: "
            << pluginName;
        return UBSE_PLUGIN_ERROR_ADMISSION_DENIED;
    }

    // 加载插件
    UbseResult ret = LoadPluginModule(pluginName, fileName);
    if (ret == UBSE_PLUGIN_ERROR_LOAD_AGAIN) {
        return UBSE_OK;
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "plugin so file load failed, related plugin name: " << pluginName << "," << FormatRetCode(ret);
        return UBSE_PLUGIN_ERROR_PLUGIN_INIT_FAILED;
    }

    // 初始化插件
    ret = InitProcessPlugin(pluginName, moduleCode.value());
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "plugin init failed, related plugin name: " << pluginName;
        loadedPluginModules_.erase(pluginName);
        return UBSE_PLUGIN_ERROR_PLUGIN_INIT_FAILED;
    }
    return UBSE_OK;
}

UbseResult UbsePluginManager::InitProcessPlugin(const std::string& pluginName, const uint16_t& moduleCode)
{
    ProcessMode processMode = UbseContext::GetInstance().GetProcessMode();

    const std::string funcName = "UbsePluginInit";
    const auto initFunc = GetInitFunction(pluginName, funcName);
    if (initFunc == nullptr) {
        UBSE_LOG_WARN << static_cast<int>(processMode)
                      << " get plugin init func failed, related plugin name: " << pluginName;
        return UBSE_ERROR;
    }

    if (const uint32_t ret = initFunc(moduleCode); ret != UBSE_OK) {
        UBSE_LOG_WARN << static_cast<int>(processMode) << " plugin " << funcName << " exec failed,"
                      << FormatRetCode(ret) << ", related plugin name: " << pluginName;
        return UBSE_PLUGIN_ERROR_PLUGIN_INIT_FAILED;
    }

    return UBSE_OK;
}
void UbsePluginManager::DeInitializePlugins()
{
    std::unique_lock<std::shared_mutex> lock(loadedPluginModulesMutex_);
    for (auto it = loadedPluginModules_.begin(); it != loadedPluginModules_.end();) {
        // 先保存下一个迭代器的位置
        const auto current = it++;

        // 逐个去初始化
        DeInitializePlugin(current->first);
    }
}

/**
 * 去初始化插件
 * 1. 调用UbsePluginDeInit去初始化插件
 * 2. 关闭插件
 * @return
 */
UbseResult UbsePluginManager::DeInitializePlugin(const std::string& pluginName)
{
    const auto it = loadedPluginModules_.find(pluginName);
    if (it == loadedPluginModules_.end()) {
        UBSE_LOG_WARN << "The plugin is not loaded, plugin name: " << pluginName;
        return UBSE_PLUGIN_ERROR_CONFIG_NOT_FOUND;
    }
    if (void* handle = it->second) {
        // 调用UbsePluginDeInit去初始化插件
        UBSE_LOG_DEBUG << "start to deInit func, plugin name:" << pluginName;
        auto deInitFunc = (UbsePluginDeInitFunc)dlsym(handle, "UbsePluginDeInit");
        if (deInitFunc) {
            deInitFunc();
            UBSE_LOG_INFO << "deInit Func success; plugin name: " << pluginName;
        } else {
            UBSE_LOG_WARN << "Unable to find UbsePluginDeInit function; plugin name: " << pluginName;
        }
    }
    loadedPluginModules_.erase(pluginName);
    return UBSE_OK;
}

UbseResult UbsePluginManager::LoadPluginModule(const std::string& pluginName, const std::string& fileName)
{
    if (loadedPluginModules_.find(pluginName) != loadedPluginModules_.end()) {
        UBSE_LOG_INFO << "The plugin: " << pluginName << " has been loaded and does not need to be loaded again.";
        return UBSE_PLUGIN_ERROR_LOAD_AGAIN; // 插件已经加载
    }

    void* handle = dlopen(fileName.c_str(), RTLD_LAZY);
    if (handle == nullptr) {
        UBSE_LOG_WARN << "load " << fileName << " failed, " << dlerror();
        return UBSE_ERROR_NULLPTR;
    }
    loadedPluginModules_[pluginName] = handle;
    return UBSE_OK;
}

UbsePluginInitFunc UbsePluginManager::GetInitFunction(const std::string& pluginName, const std::string& funcName)
{
    void* handle = loadedPluginModules_[pluginName];
    if (handle == nullptr) {
        UBSE_LOG_WARN << "Invalid handle for plugin: " << pluginName;
        return nullptr;
    }

    auto func = reinterpret_cast<UbsePluginInitFunc>(
        dlsym(handle, funcName.c_str())); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (func == nullptr) {
        UBSE_LOG_WARN << "Failed to get UbsePluginInit for plugin: " << pluginName;
    }
    return func;
}

void UbsePluginManager::GetLoadedPlugins(std::vector<std::string>& loadedPlugins)
{
    std::shared_lock<std::shared_mutex> lock(loadedPluginModulesMutex_);
    for (const auto& item : loadedPluginModules_) {
        loadedPlugins.push_back(item.first);
    }
}

void* UbsePluginManager::GetLoadedPlugin(const string& pluginName)
{
    std::shared_lock<std::shared_mutex> lock(loadedPluginModulesMutex_);
    if (loadedPluginModules_.find(pluginName) == loadedPluginModules_.end()) {
        UBSE_LOG_WARN << "The plugin: " << pluginName << " has not been loaded and does not need to be loaded again.";
        return nullptr;
    }
    return loadedPluginModules_[pluginName];
}

} // namespace ubse::plugin
