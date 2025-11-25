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

#ifndef UBSE_PLUGIN_ERROR_H
#define UBSE_PLUGIN_ERROR_H
#include "ubse_error.h"

/* **************************************** */
/* 插件模块错误码定义                         */
/* **************************************** */

/* 0X100E1000 配置文件加载失败 */
#define UBSE_PLUGIN_ERROR_FILE_LOADED_ERROR (UBSE_MID_HI16(UBSE_PLUGIN_MID) | UBSE_ERROR_USERNO(0x00))

/* 0X100E1001 插件配置不存在 */
#define UBSE_PLUGIN_ERROR_CONFIG_NOT_FOUND (UBSE_MID_HI16(UBSE_PLUGIN_MID) | UBSE_ERROR_USERNO(0x01))

/* 0X100E1002 插件不准入 */
#define UBSE_PLUGIN_ERROR_ADMISSION_DENIED (UBSE_MID_HI16(UBSE_PLUGIN_MID) | UBSE_ERROR_USERNO(0x02))

/* 0X100E1003 插件初始化失败 */
#define UBSE_PLUGIN_ERROR_PLUGIN_INIT_FAILED (UBSE_MID_HI16(UBSE_PLUGIN_MID) | UBSE_ERROR_USERNO(0x03))

/* 0X100E1004 插件已加载 */
#define UBSE_PLUGIN_ERROE_LOAD_AGAIN (UBSE_MID_HI16(UBSE_PLUGIN_MID) | UBSE_ERROR_USERNO(0x04))

#endif // UBSE_PLUGIN_ERROR_H
