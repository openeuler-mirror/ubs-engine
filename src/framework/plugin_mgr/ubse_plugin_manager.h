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

#ifndef UBSE_PLUGIN_MANAGER_H
#define UBSE_PLUGIN_MANAGER_H

#include <cstdint>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_plugin_admission.h"
#include "ubse_plugin_config.h"

namespace ubse::plugin {
using namespace ubse::common::def;
using namespace ubse::context;
using UbsePluginInitFunc = uint32_t (*)(const uint16_t);
using UbsePluginDeInitFunc = void (*)();

/**
 * UbsePluginManager: 插件管理
 */
class UbsePluginManager {
public:
    /**
     * 插件管理模块启动:读取插件配置和准入配置，并初始化所有插件
     * @return UbseResult
     */
    UbseResult Start();

    /**
     * 卸載所有插件,调用去初始化函数，并关闭so文件
     */
    void DeInitializePlugins();

    /**
     * 获取已加载插件
     * @param loadedPlugins 已加载插件集合
     */
    void GetLoadedPlugins(std::vector<std::string> &loadedPlugins);

    /**
     * @brief 获取已加载插件句柄
     *
     * @param pluginName 插件名称
     * @return void* 插件句柄
     */
    void *GetLoadedPlugin(const std::string &pluginName);

private:
    /**
     * 加载、初始化所有允许的插件
     * @return UbseResult
     */
    UbseResult LoadAndInitPlugins();

    /**
     * 单独加载并初始化某个插件
     * @param pluginName 插件名称
     * @param fileName so文件路径
     * @return UbseResult
     */
    UbseResult LoadAndInitPlugin(const std::string &pluginName, const std::string &fileName);

    /**
     * 卸载单个插件，调用趋势化函数，并关闭so文件
     * @param pluginName 插件名称
     * @return UbseResult
     */
    UbseResult DeInitializePlugin(const std::string &pluginName);

    /**
     * 加载指定插件的模块（so文件）。
     * @param pluginName  插件名称
     * @param fileName so文件路径
     * @return UbseResult
     */
    UbseResult LoadPluginModule(const std::string &pluginName, const std::string &fileName);

    /**
     * 获取指定插件的初始化函数。
     * @param pluginName 插件名称
     * @param funcName 函数名称
     * @return UbsePluginInitFunc 初始化函数指针
     */
    UbsePluginInitFunc GetInitFunction(const std::string &pluginName, const std::string &funcName);

    UbseResult InitProcessPlugin(const std::string &pluginName, const uint16_t &moduleCode);

private:
    // 插件准入配置管理器，用于加载和管理插件准入信息
    UbsePluginAdmission ubsePluginAdmission_;

    // 插件配置管理器, 用于加载和管理插件配置信息
    UbsePluginConfig ubsePluginConfig_;

    mutable std::shared_mutex loadedPluginModulesMutex_;
    // 存储所有已加载插件的模块句柄
    std::map<std::string, void *> loadedPluginModules_;
};
} // namespace ubse::plugin
#endif // UBSE_PLUGIN_MANAGER_H
