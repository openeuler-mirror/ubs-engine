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
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;
namespace ubse::module {
const std::string PLUGIN_DIR = "/usr/lib64/ubse_plugin";
constexpr size_t SO_EXTENSION_LENGTH = 3;
constexpr std::string_view PLUGIN_EXTENSION = ".so";
void UbsePluginLoader::DiscoverAndLoad()
{
    if (!fs::exists(PLUGIN_DIR)) {
        std::cout << "[UbsePluginLoader] Plugin directory does not exist: " << PLUGIN_DIR << std::endl;
        return;
    }

    std::cout << "[UbsePluginLoader] Scanning plugin directory" << std::endl;

    for (const auto &entry : fs::directory_iterator(PLUGIN_DIR)) {
        if (entry.is_regular_file()) {
            std::string path = entry.path().string();
            // 简单的后缀匹配筛选 .so 文件
            if (path.size() >= SO_EXTENSION_LENGTH &&
                path.substr(path.size() - SO_EXTENSION_LENGTH) == PLUGIN_EXTENSION) {
                std::cout << "[UbsePluginLoader] Loading plugin: " << path << std::endl;

                // RTLD_LAZY: 延迟绑定，首次使用符号时才解析
                void *handle = dlopen(path.c_str(), RTLD_LAZY | RTLD_NODELETE);
                if (!handle) {
                    std::cerr << "[UbsePluginLoader] Failed to load " << path << ": " << dlerror() << std::endl;
                    continue;
                }

                handles_.push_back(handle);
                std::cout << "[UbsePluginLoader] Successfully loaded: " << path << std::endl;
            }
        }
    }

    std::cout << "[UbsePluginLoader] Total plugins loaded: " << handles_.size() << std::endl;
}
void UbsePluginLoader::UnloadAll()
{
    for (void *handle : handles_) {
        dlclose(handle);
    }
    handles_.clear();
    std::cout << "[UbsePluginLoader] All plugins unloaded." << std::endl;
}

} // namespace ubse::module