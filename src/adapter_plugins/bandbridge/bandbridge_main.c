/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <linux/module.h>
#include "bandbridge_main.h"

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
