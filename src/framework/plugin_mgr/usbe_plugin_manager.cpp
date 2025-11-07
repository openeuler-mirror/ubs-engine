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

#include "ubse_http_module.h"
#include "ubse_file_util.h"
#include "ubse_logger.h"
#include "ubse_logger_inner.h"
#include "usbe_plugin_error.h"

namespace ubse::plugin {
using namespace ubse::context;
using namespace ubse::http;
using namespace ubse::log;
using namespace ubse::utils;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_PLUGIN_MID)

static const std::unordered_set<std::string> g_pluginCannotFail{ "memExport", "memMaster", "memAgent" };

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
    UbseResult ret = ubsePluginConfig.LoadPluginConfigs();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "plugin config load failed, ret: " << FormatRetCode(ret);
        return UBSE_PLUGIN_ERROR_FILE_LOADED_ERROR;
    }

    // 读取准入配置
    ret = ubsePluginAdmission.LoadAdmissionConfig();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "plugin admission config load failed," << FormatRetCode(ret);
        return UBSE_PLUGIN_ERROR_FILE_LOADED_ERROR;
    }

    // 加载准入配置so，调用UbsePluginInit初始化插件
    return LoadAndInitPlugins();
}

UbseResult UbsePluginManager::LoadAndInitPlugins()
{
    const auto &configs = ubsePluginConfig.GetAllPluginConfigs();
    const std::string libDir = UbseFileUtil::GetLibDir();
    for (const auto &configItem : configs) {
        const std::string &pluginName = configItem.first;
        UBSE_LOG_INFO << "starting to load plugin; plugin name: " << pluginName.c_str();
        auto ret = LoadAndInitPlugin(pluginName, libDir + configItem.second.pkg);
        if (UBSE_OK != ret) {
            if (g_pluginCannotFail.find(pluginName) != g_pluginCannotFail.end() &&
                ret == UBSE_PLUGIN_ERROR_PLUGIN_INIT_FAILED) {
                UBSE_LOG_ERROR << "plugin init failed, related plugin name: " << pluginName << ".";
                std::cerr << "The plugin init failed. For details about the plugin, see the ubse log file, "
                    "related plugin name: " <<
                    pluginName << "." << std::endl;
                return ret;
            }
            UBSE_LOG_WARN << "plugin is not loaded, related plugin name: " << pluginName << ".";
            std::cout << "The plugin is not loaded. For details about the plugin, see the ubse log file, "
                "related plugin name: " <<
                pluginName << "." << std::endl;
            continue;
        }
        UBSE_LOG_INFO << "plugin: " << pluginName << " loaded successfully.";
    }
    return UBSE_OK;
}

UbseResult UbsePluginManager::LoadAndInitPlugin(const std::string &pluginName, const std::string &soPath)
{
    std::unique_lock<std::shared_mutex> lock(loadedPluginModulesMutex);
    const uint16_t *moduleCode = ubsePluginAdmission.GetPluginConfig(pluginName);
    if (!moduleCode) {
        UBSE_LOG_WARN
            <<
            "plugin configuration is available, but no admission configuration is available, related plugin name: " <<
            pluginName;
        return UBSE_PLUGIN_ERROR_ADMISSION_DENIED;
    }

    // 加载插件
    UbseResult ret = LoadPluginModule(pluginName, soPath);
    if (ret == UBSE_PLUGIN_ERROE_LOAD_AGAIN) {
        return UBSE_OK;
    }
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "plugin so file load failed, related plugin name: " << pluginName << "," << FormatRetCode(ret);
        return UBSE_PLUGIN_ERROR_PLUGIN_INIT_FAILED;
    }

    // 初始化插件
    ret = InitProcessPlugin(pluginName, *moduleCode);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "plugin init failed, related plugin name: " << pluginName;
        loadedPluginModules.erase(pluginName);
        return UBSE_PLUGIN_ERROR_PLUGIN_INIT_FAILED;
    }
    return UBSE_OK;
}

UbseResult UbsePluginManager::InitProcessPlugin(const std::string &pluginName, const uint16_t &moduleCode)
{
    ProcessMode processMode = UbseContext::GetInstance().GetProcessMode();

    std::string funcName = "UbsePluginInit";
    auto initFunc = GetInitFunction(pluginName, funcName);
    if (initFunc == nullptr) {
        UBSE_LOG_WARN << processMode << " get plugin init func failed, related plugin name: " << pluginName;
        return UBSE_ERROR;
    }

    uint32_t ret = initFunc(moduleCode);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << processMode << " plugin " << funcName << " exec failed," << FormatRetCode(ret) <<
            ", related plugin name: " << pluginName;
        return UBSE_PLUGIN_ERROR_PLUGIN_INIT_FAILED;
    }

    return UBSE_OK;
}
void UbsePluginManager::DeInitializePlugins()
{
    std::unique_lock<std::shared_mutex> lock(loadedPluginModulesMutex);
    for (auto it = loadedPluginModules.begin(); it != loadedPluginModules.end();) {
        // 先保存下一个迭代器的位置
        auto current = it++;

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
UbseResult UbsePluginManager::DeInitializePlugin(const std::string &pluginName)
{
    auto it = loadedPluginModules.find(pluginName);
    if (it == loadedPluginModules.end()) {
        UBSE_LOG_WARN << "The plugin is not loaded, plugin name: " << pluginName;
        return UBSE_PLUGIN_ERROR_CONFIG_NOT_FOUND;
    }
    void *handle = it->second;
    if (handle) {
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
    loadedPluginModules.erase(pluginName);
    return UBSE_OK;
}

UbseResult UbsePluginManager::LoadPluginModule(const std::string &pluginName, const std::string &fileName)
{
    if (loadedPluginModules.find(pluginName) != loadedPluginModules.end()) {
        UBSE_LOG_INFO << "The plugin: " << pluginName << " has been loaded and does not need to be loaded again.";
        return UBSE_PLUGIN_ERROE_LOAD_AGAIN; // 插件已经加载
    }

    char *canonicalPath = realpath(fileName.c_str(), nullptr);
    if (canonicalPath == nullptr) {
        UBSE_LOG_WARN << "The path of the so file corresponding to the plugin " << pluginName
                    << " is invalid, file: " << fileName;
        return UBSE_ERROR_NULLPTR;
    }
    void *handle = dlopen(canonicalPath, RTLD_LAZY);
    free(canonicalPath);
    if (handle == nullptr) {
        UBSE_LOG_WARN << "load " << fileName << " failed, " << dlerror();
        return UBSE_ERROR_NULLPTR;
    }
    loadedPluginModules[pluginName] = handle;
    return UBSE_OK;
}

UbsePluginInitFunc UbsePluginManager::GetInitFunction(const std::string &pluginName, const std::string &funcName)
{
    void *handle = loadedPluginModules[pluginName];
    if (handle == nullptr) {
        UBSE_LOG_WARN << "Invalid handle for plugin: " << pluginName;
        return nullptr;
    }

    auto func = reinterpret_cast<UbsePluginInitFunc>(dlsym(handle, funcName.c_str()));
    if (func == nullptr) {
        UBSE_LOG_WARN << "Failed to get UbsePluginInit for plugin: " << pluginName;
    }
    return func;
}

void UbsePluginManager::GetLoadedPlugins(std::vector<std::string> &loadedPlugins)
{
    std::shared_lock<std::shared_mutex> lock(loadedPluginModulesMutex);
    for (const auto &item : loadedPluginModules) {
        loadedPlugins.push_back(item.first);
    }
}

void *UbsePluginManager::GetLoadedPlugin(const string &pluginName)
{
    std::shared_lock<std::shared_mutex> lock(loadedPluginModulesMutex);
    if (loadedPluginModules.find(pluginName) == loadedPluginModules.end()) {
        UBSE_LOG_WARN << "The plugin: " << pluginName << " has not been loaded and does not need to be loaded again.";
        return nullptr;
    }
    return loadedPluginModules[pluginName];
}
} // namespace ubse::plugin
