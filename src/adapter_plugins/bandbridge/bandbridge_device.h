/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef BANDBRIDGE_DEVICE_H
#define BANDBRIDGE_DEVICE_H

#include "bandbridge_log.h"
#include "bandbridge_ctrlq.h"

#define BANDBRIDGE_NAME "bandbridge"

struct bandbridge_mbuf {
    int sendbuf_size;
    int recvbuf_size;
    void* sendbuf;
    void* recvbuf;
};

#define BANDBRIDGE_IOCTL_BASE 'X'
#define BANDBRIDGE_SEND_REQUEST _IOWR(BANDBRIDGE_IOCTL_BASE, 0, struct bandbridge_mbuf)

int bandbridge_cdev_register(void);
void bandbridge_cdev_unregister(void);

#endif // BANDBRIDGE_DEVICE_H
