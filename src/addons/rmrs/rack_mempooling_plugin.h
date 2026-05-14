/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * 临时示例，很快删除
 */

#ifndef RACK_MEMPOOLING_PLUGIN_H
#define RACK_MEMPOOLING_PLUGIN_H
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 插件初始化函数; 初始化Rack插件
 * @param modCode 插件模块代码, 英语表示区分不同的插件示例
 * @return 初始化操作结果, 0为成功, 非0为异常
 */
uint32_t UbsePluginInit(const uint16_t modCode);

/**
 * @brief 插件去初始化函数;
 * @return nothing
 */
void UbsePluginDeInit(void);

#ifdef __cplusplus
}

#endif

#endif
