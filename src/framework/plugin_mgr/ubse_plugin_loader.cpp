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
#include "ubse_plugin_loader.h"
#include "ubse_plugin_admission.h"
#include <dlfcn.h>
#include <filesystem>
#include <iostream>

#include "ubse_error.h"
#include "ubse_logger_module.h"

namespace fs = std::filesystem;

namespace ubse::plugin {

UBSE_DEFINE_THIS_MODULE("ubse");

const std::string PLUGIN_DIR = "/usr/lib64/ubse_plugin";
constexpr size_t SO_EXTENSION_LENGTH = 3;
constexpr std::string_view PLUGIN_EXTENSION = ".so";
constexpr std::string_view LIB_PREFIX = "lib";

bool IsSoFile(const std::string &path)
{
    return path.size() >= SO_EXTENSION_LENGTH &&
           path.substr(path.size() - SO_EXTENSION_LENGTH) == PLUGIN_EXTENSION;
}

bool IsPluginAllowed(const std::string &pluginName, const std::map<std::string, uint16_t> &allowedPlugins)
{
    return allowedPlugins.find(pluginName) != allowedPlugins.end();
}

std::string PluginNameFromSoPath(const std::string &path)
{
    std::string filename = fs::path(path).filename().string();
    if (filename.size() > LIB_PREFIX.size() &&
        filename.substr(0, LIB_PREFIX.size()) == LIB_PREFIX) {
        filename = filename.substr(LIB_PREFIX.size());
    }
    if (filename.size() > SO_EXTENSION_LENGTH &&
        filename.substr(filename.size() - SO_EXTENSION_LENGTH) == PLUGIN_EXTENSION) {
        filename = filename.substr(0, filename.size() - SO_EXTENSION_LENGTH);
    }
    return filename;
}
/**
 * 插件发现规则：
 * 1. 插件目录必须存在
 * 2. 插件文件必须以 "lib" 开头， ".so" 结尾
 * 3. 准入过滤逻辑 ：只加载白名单中的插件
 */
void UbsePluginLoader::DiscoverAndLoad()
{
    if (!fs::exists(PLUGIN_DIR)) {
        std::cout << "Plugin directory does not exist: " << PLUGIN_DIR << std::endl;
        return;
    }

    UbsePluginAdmission admission;
    UbseResult loadRet = admission.LoadAdmissionConfig();
    const auto &allowedPlugins = admission.GetAllowedPlugins();
    if (loadRet != UBSE_OK || allowedPlugins.empty()) {
        UBSE_LOG_ERROR << "Plugin admission config load failed or empty, ret: "
                       << log::FormatRetCode(loadRet)
                       << ", allowedPlugins size: " << allowedPlugins.size()
                       << ", no plugins will be loaded";
        return;
    }

    std::cout  << "Scanning plugin directory: " << PLUGIN_DIR << std::endl;

    for (const auto &entry : fs::directory_iterator(PLUGIN_DIR)) {
        if (!entry.is_regular_file() || !IsSoFile(entry.path().string())) {
            continue;
        }

        const std::string path = entry.path().string();
        std::string pluginName = PluginNameFromSoPath(path);

        if (!IsPluginAllowed(pluginName, allowedPlugins)) {
            std::cerr << "Plugin denied: " << pluginName << " (" << path << ")" << std::endl;
            continue;
        }

        std::cout  << "Loading plugin: " << path << std::endl;
        void *handle = dlopen(path.c_str(), RTLD_LAZY | RTLD_NODELETE);
        if (!handle) {
            std::cerr << "Failed to load " << path << ": " << dlerror() << std::endl;
            continue;
        }

        handles_.push_back(handle);
        std::cout  << "Successfully loaded: " << path << std::endl;
    }

    std::cout  << "Total plugins loaded: " << handles_.size() << std::endl;
}

void UbsePluginLoader::UnloadAll()
{
    for (void *handle : handles_) {
        dlclose(handle);
    }
    handles_.clear();
    std::cout  << "All plugins unloaded." << std::endl;
}

void DiscoverAndLoad()
{
    UbsePluginLoader::GetInstance().DiscoverAndLoad();
}

void UnloadAll()
{
    UbsePluginLoader::GetInstance().UnloadAll();
}

} // namespace ubse::plugin
