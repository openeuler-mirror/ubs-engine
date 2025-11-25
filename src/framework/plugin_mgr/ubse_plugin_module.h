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

#ifndef UBSE_PLUGIN_MODULE_H
#define UBSE_PLUGIN_MODULE_H

#include <mutex>
#include <shared_mutex>

#include "ubse_module.h"
#include "ubse_plugin_manager.h"

namespace ubse::plugin {
using namespace ubse::module;
class UbsePluginModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    /* *
     * @brief 获取已加载插件句柄
     *
     * @param pluginName 插件名称
     * @return void* 插件句柄
     */
    void *GetLoadedPlugin(const std::string &pluginName);

    /* *
     * @brief 判断插件是否已加载
     *
     * @param pluginName 插件名称
     * @return true 插件已加载
     * @return false 插件未加载
     */
    bool GetPluginLoaded(const std::string &pluginName);

    /* *
     * @brief 获取启动插件名称数组
     *
     * @param plugins [out] 插件名称数组
     * @return 0 名称列表获取成功，非 0 获取失败
     */
    void GetLoadedPluginsName(std::vector<std::string> &plugins);
    /* *
     * 获取插件是否就绪，就绪代表插件已经能够向外部提供功能，标志位由插件自己向插件模块设置
     * @param pluginName[in]: 插件名
     * @return 就绪状态 true: 就绪, false: 未就绪;
     */
    bool GetPluginReadyStatus(const std::string &pluginName);
    /* *
     * 插件方向插件模块通知自己是否已就绪
     * @param pluginName[in]: 插件名
     * @param status[in] ture: 就绪， false: 未就绪
     */
    void NotifyPluginReadyStatus(const std::string &pluginName, bool flag);

private:
    UbsePluginManager ubsePluginManager;
    mutable std::shared_mutex pluginReadyMapMutex;
    std::map<std::string, bool> pluginReadyMap;
};
}
#endif // UBSE_PLUGIN_MODULE_H
