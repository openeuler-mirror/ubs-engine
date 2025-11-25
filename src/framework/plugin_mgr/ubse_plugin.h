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

#ifndef UBSE_MANAGER_UBSE_PLUGIN_H
#define UBSE_MANAGER_UBSE_PLUGIN_H
#include <string>
namespace ubse::plugin {
/**
 * 获取插件初始化结果；初始化成功标志位插件是否成功执行初始化函数
 * @param pluginName[in]: 插件名
 * @return  初始化结果 true: 初始化成功, false: 未初始化成功
 */
bool GetPluginInitRes(const std::string &pluginName);

/**
 * 获取插件是否就绪，就绪代表插件已经能够向外部提供功能，标志位由插件自己向插件模块设置
 * @param pluginName[in]: 插件名
 * @return 就绪状态 true: 就绪, false: 未就绪;
 */
bool GetPluginReadyStatus(const std::string &pluginName);

/**
 * 插件方向插件模块通知自己是否已就绪
 * @param pluginName[in]: 插件名
 * @param status[in] ture: 就绪， false: 未就绪
 * @return 操作结果:0 成功 非0 失败
 */
uint32_t NotifyPluginReadyStatus(const std::string &pluginName, bool status);
}
#endif // UBSE_MANAGER_UBSE_PLUGIN_H
