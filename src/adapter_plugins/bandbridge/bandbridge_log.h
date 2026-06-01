/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * Description: bandbridge log head file
 * Author:
 * Create:
 * Note:
 * History:
 */

#ifndef BANDBRIDGE_LOG_H
#define BANDBRIDGE_LOG_H

#include <linux/printk.h>
#include <linux/types.h>

enum bandbridge_log_level {
    BANDBRIDGE_LOG_LEVEL_EMERG = 0,
    BANDBRIDGE_LOG_LEVEL_ALERT = 1,
    BANDBRIDGE_LOG_LEVEL_CRIT = 2,
    BANDBRIDGE_LOG_LEVEL_ERR = 3,
    BANDBRIDGE_LOG_LEVEL_WARNING = 4,
    BANDBRIDGE_LOG_LEVEL_NOTICE = 5,
    BANDBRIDGE_LOG_LEVEL_INFO = 6,
    BANDBRIDGE_LOG_LEVEL_DEBUG = 7,
    BANDBRIDGE_LOG_LEVEL_MAX = 8,
};

/* add log head info, LogTag_BANDBRIDGE|function|[line]| */
#define BANDBRIDGE_LOG_TAG "LogTag_BANDBRIDGE"
/* use debug log */
#define bandbridge_log(l, format, args...) pr_##l("%s|%s:[%d]|" format, BANDBRIDGE_LOG_TAG, __func__, __LINE__, ##args)
/* use default log, info/warn/err */
#define bandbridge_default_log(l, format, args...) \
    ((void)pr_##l("%s|%s:[%d]|" format, BANDBRIDGE_LOG_TAG, __func__, __LINE__, ##args))

#define BANDBRIDGE_RATELIMIT_INTERVAL (5 * HZ)
#define BANDBRIDGE_RATELIMIT_BURST 100

extern uint32_t g_bandbridge_log_level;

#define bandbridge_log_info(...)                                 \
    do {                                                         \
        if (g_bandbridge_log_level >= BANDBRIDGE_LOG_LEVEL_INFO) \
            bandbridge_default_log(info, __VA_ARGS__);           \
    } while (0)

#define bandbridge_log_err(...)                                 \
    do {                                                        \
        if (g_bandbridge_log_level >= BANDBRIDGE_LOG_LEVEL_ERR) \
            bandbridge_default_log(err, __VA_ARGS__);           \
    } while (0)

#define bandbridge_log_warn(...)                                    \
    do {                                                            \
        if (g_bandbridge_log_level >= BANDBRIDGE_LOG_LEVEL_WARNING) \
            bandbridge_default_log(warn, __VA_ARGS__);              \
    } while (0)

#define bandbridge_log_debug(...)                                 \
    do {                                                          \
        if (g_bandbridge_log_level >= BANDBRIDGE_LOG_LEVEL_DEBUG) \
            bandbridge_log(debug, __VA_ARGS__);                   \
    } while (0)

/* Rate Limited log to avoid soft lockup crash by quantities of printk */
/* Current limit is 100 log every 5 seconds */
#define bandbridge_log_info_rl(...)                                                                    \
    do {                                                                                               \
        static DEFINE_RATELIMIT_STATE(_rs, BANDBRIDGE_RATELIMIT_INTERVAL, BANDBRIDGE_RATELIMIT_BURST); \
        if ((__ratelimit(&_rs)) && (g_bandbridge_log_level >= BANDBRIDGE_LOG_LEVEL_INFO))              \
            bandbridge_log(info, __VA_ARGS__);                                                         \
    } while (0)

#define bandbridge_log_err_rl(...)                                                                     \
    do {                                                                                               \
        static DEFINE_RATELIMIT_STATE(_rs, BANDBRIDGE_RATELIMIT_INTERVAL, BANDBRIDGE_RATELIMIT_BURST); \
        if ((__ratelimit(&_rs)) && (g_bandbridge_log_level >= BANDBRIDGE_LOG_LEVEL_ERR))               \
            bandbridge_log(err, __VA_ARGS__);                                                          \
    } while (0)

#define bandbridge_log_warn_rl(...)                                                                    \
    do {                                                                                               \
        static DEFINE_RATELIMIT_STATE(_rs, BANDBRIDGE_RATELIMIT_INTERVAL, BANDBRIDGE_RATELIMIT_BURST); \
        if ((__ratelimit(&_rs)) && (g_bandbridge_log_level >= BANDBRIDGE_LOG_LEVEL_WARNING))           \
            bandbridge_log(warn, __VA_ARGS__);                                                         \
    } while (0)

#endif // BANDBRIDGE_LOG_H
