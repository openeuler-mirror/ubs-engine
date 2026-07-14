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
 * Description: bandbridge device head file
 * Author:
 * Create:
 * Note:
 * History:
 */

#ifndef BANDBRIDGE_DEVICE_H
#define BANDBRIDGE_DEVICE_H

#include "bandbridge_ctrlq.h"
#include "bandbridge_log.h"

#define BANDBRIDGE_NAME "bandbridge"

struct bandbridge_mbuf {
    int sendbuf_size;
    int recvbuf_size;
    void *sendbuf;
    void *recvbuf;
};

#define BANDBRIDGE_IOCTL_BASE 'X'
#define BANDBRIDGE_SEND_REQUEST _IOWR(BANDBRIDGE_IOCTL_BASE, 0, struct bandbridge_mbuf)

int bandbridge_cdev_register(void);
void bandbridge_cdev_unregister(void);

#endif // BANDBRIDGE_DEVICE_H
