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
 * Description: bandbridge sample file
 * Author:
 * Create:
 * Note:
 * History:
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct bandbridge_mbuf {
    int sendbuf_size;
    int recvbuf_size;
    void *sendbuf;
    void *recvbuf;
};

#define BANDBRIDGE_DRV_NAME "/dev/bandbridge"
#define BANDBRIDGE_IOCTL_BASE 'X'
#define BANDBRIDGE_SEND_REQUEST _IOWR(BANDBRIDGE_IOCTL_BASE, 0, struct bandbridge_mbuf)

int main()
{
    int fd = -1;
    int ret;
    struct bandbridge_mbuf mbuf;

    // 设置发送缓冲区大小为64
    mbuf.sendbuf_size = 64;
    // 设置接收缓冲区大小为64
    mbuf.recvbuf_size = 64;
    mbuf.sendbuf = malloc(mbuf.sendbuf_size);
    mbuf.recvbuf = malloc(mbuf.recvbuf_size);
    if (mbuf.sendbuf == NULL || mbuf.recvbuf == NULL) {
        printf("#### malloc memory failed\n");
        return -1;
    }

    fd = open(BANDBRIDGE_DRV_NAME, O_RDWR);
    if (fd < 0) {
        printf("#### open fd failed\n");
        free(mbuf.sendbuf);
        free(mbuf.recvbuf);
        return -1;
    }

    ret = ioctl(fd, BANDBRIDGE_SEND_REQUEST, &mbuf);
    if (ret != 0) {
        printf("#### ioctl failed\n");
    }

    free(mbuf.sendbuf);
    free(mbuf.recvbuf);
    close(fd);
    return 0;
}
