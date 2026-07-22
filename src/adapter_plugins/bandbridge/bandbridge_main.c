// SPDX-License-Identifier: GPL-2.0
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
 * Description: bandbridge kernel module
 * Author:
 * Create:
 * Note:
 * History:
 */

#include "bandbridge_main.h"
#include <linux/module.h>

static int __init bandbridge_init(void)
{
    int ret;
    ret = bandbridge_ctrlq_init();
    if (ret != 0) {
        bandbridge_log_err("bandbridge ctrlq init failed, ret:%d.\n", ret);
        return -1;
    }

    ret = bandbridge_cdev_register();
    if (ret != 0) {
        bandbridge_ctrlq_deinit();
        bandbridge_log_err("bandbridge cdev register failed, ret:%d.\n", ret);
        return -1;
    }

    return 0;
}

static void __exit bandbridge_exit(void)
{
    bandbridge_cdev_unregister();
    bandbridge_ctrlq_deinit();
}

module_init(bandbridge_init);
module_exit(bandbridge_exit);

MODULE_DESCRIPTION("Kernel module for bandbridge");
MODULE_AUTHOR("huawei");
MODULE_LICENSE("GPL v2");
