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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

struct bandbridge_mbuf {
    int sendbuf_size;
    int recvbuf_size;
    void* sendbuf;
    void* recvbuf;
};

#define BANDBRIDGE_DRV_NAME "/dev/bandbridge"
#define BANDBRIDGE_IOCTL_BASE 'X'
#define BANDBRIDGE_SEND_REQUEST _IOWR(BANDBRIDGE_IOCTL_BASE, 0, struct bandbridge_mbuf)

int main()
{
    int fd = -1;
    int ret;
    struct bandbridge_mbuf mbuf;

    mbuf.sendbuf_size = 64;
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