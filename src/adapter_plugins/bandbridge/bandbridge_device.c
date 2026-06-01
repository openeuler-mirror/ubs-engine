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
 * Description: bandbridge cdev ops file
 * Author:
 * Create:
 * Note:
 * History:
 */

#include <linux/capability.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "bandbridge_device.h"

struct bandbridge_ctx {
    dev_t bandbridge_devno;
    struct cdev bandbridge_cdev;
    struct class* bandbridge_cls;
    struct device* bandbridge_dev;
    struct mutex bandbridge_lock;
};

static struct bandbridge_ctx g_bandbridge_ctx = {0};

static int bandbridge_open(struct inode* inode, struct file* filp)
{
    if (!capable(CAP_NET_ADMIN)) {
        bandbridge_log_err("[bandbridge_open] permission denied.\n");
        return -EPERM;
    }
    int buf_size;

    struct bandbridge_mbuf* mbuf;
    mbuf = kmalloc(sizeof(*mbuf), GFP_KERNEL);
    if (!mbuf) {
        bandbridge_log_err("[bandbridge_open] alloc mbuf failed.\n");
        return -ENOMEM;
    }

    buf_size = bandbridge_ctrlq_get_sq_size();
    if (buf_size <= 0) {
        kfree(mbuf);
        bandbridge_log_err("[bandbridge_open] sq buf_size is invalid.\n");
        return -EINVAL;
    }
    mbuf->sendbuf = vmalloc(buf_size);
    if (!mbuf->sendbuf) {
        kfree(mbuf);
        bandbridge_log_err("[bandbridge_open] alloc mbuf->sendbuf failed.\n");
        return -ENOMEM;
    }

    buf_size = bandbridge_ctrlq_get_rq_size();
    if (buf_size <= 0) {
        vfree(mbuf->sendbuf);
        kfree(mbuf);
        bandbridge_log_err("[bandbridge_open] rq buf_size is invalid.\n");
        return -EINVAL;
    }
    mbuf->recvbuf = vmalloc(buf_size);
    if (!mbuf->recvbuf) {
        vfree(mbuf->sendbuf);
        kfree(mbuf);
        bandbridge_log_err("[bandbridge_open] alloc mbuf->recvbuf failed.\n");
        return -ENOMEM;
    }

    filp->private_data = mbuf;
    return 0;
}

static int bandbridge_close(struct inode* inode, struct file* filp)
{
    if (!capable(CAP_NET_ADMIN)) {
        bandbridge_log_err("[bandbridge_close] permission denied.\n");
        return -EPERM;
    }
    struct bandbridge_mbuf* mbuf = filp->private_data;
    if (mbuf) {
        if (mbuf->sendbuf) {
            vfree(mbuf->sendbuf);
        }
        if (mbuf->recvbuf) {
            vfree(mbuf->recvbuf);
        }
        kfree(mbuf);
    }
    return 0;
}

static long bandbridge_send_request(struct bandbridge_mbuf* mbuf, void __user* arg)
{
    if (!capable(CAP_NET_ADMIN)) {
        bandbridge_log_err("[bandbridge_send_request] permission denied.\n");
        return -EPERM;
    }
    struct bandbridge_mbuf tmpbuf;
    int ret;
    struct bandbridge_ctrlq_msg_header* head;
    u16 sseq;

    if (copy_from_user(&tmpbuf, arg, sizeof(tmpbuf))) {
        bandbridge_log_err("[bandbridge_send_request] copy from user failed 1.\n");
        return -EFAULT;
    }

    ret = bandbridge_ctrlq_check_sq_enough(tmpbuf.sendbuf_size);
    if (ret != 0) {
        bandbridge_log_err("[bandbridge_send_request] sq is not enough.\n");
        return -ENOSPC;
    }
    mutex_lock(&g_bandbridge_ctx.bandbridge_lock);
    if (copy_from_user(mbuf->sendbuf, tmpbuf.sendbuf, tmpbuf.sendbuf_size)) {
        mutex_unlock(&g_bandbridge_ctx.bandbridge_lock);
        bandbridge_log_err("[bandbridge_send_request] copy from user failed 2.\n");
        return -EFAULT;
    }
    mbuf->sendbuf_size = tmpbuf.sendbuf_size;
    mbuf->recvbuf_size = tmpbuf.recvbuf_size;
    bandbridge_ctrlq_send_to_sq(mbuf->sendbuf, mbuf->sendbuf_size);

    head = (struct bandbridge_ctrlq_msg_header*)mbuf->sendbuf;
    sseq = le16_to_cpu(head->seq);
    ret = bandbridge_ctrlq_receive_from_rq(mbuf->recvbuf, &mbuf->recvbuf_size, sseq);
    if (ret != 0) {
        mutex_unlock(&g_bandbridge_ctx.bandbridge_lock);
        bandbridge_log_err("[bandbridge_send_request] recv response from rq failed.\n");
        return ret;
    }

    if (copy_to_user(tmpbuf.recvbuf, mbuf->recvbuf, mbuf->recvbuf_size)) {
        mutex_unlock(&g_bandbridge_ctx.bandbridge_lock);
        bandbridge_log_err("[bandbridge_send_request] copy to user failed 3.\n");
        return -EFAULT;
    }
    tmpbuf.recvbuf_size = mbuf->recvbuf_size;
    mutex_unlock(&g_bandbridge_ctx.bandbridge_lock);

    if (copy_to_user(arg, &tmpbuf, sizeof(tmpbuf))) {
        bandbridge_log_err("[bandbridge_send_request] copy to user failed 4.\n");
        return -EFAULT;
    }

    return 0;
}

static long bandbridge_ioctl(struct file* filp, unsigned int cmd, unsigned long arg)
{
    if (!capable(CAP_NET_ADMIN)) {
        bandbridge_log_err("[bandbridge_ioctl] permission denied.\n");
        return -EPERM;
    }
    struct bandbridge_mbuf* mbuf = filp->private_data;

    switch (cmd) {
        case BANDBRIDGE_SEND_REQUEST:
            return bandbridge_send_request(mbuf, (void __user*)arg);
        default:
            return -EINVAL;
    }
}

static const struct file_operations g_bandbridge_global_ops = {
    .owner = THIS_MODULE,
    .open = bandbridge_open,
    .release = bandbridge_close,
    .unlocked_ioctl = bandbridge_ioctl,
};

static struct miscdevice bandbridge_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,      // 使用动态分配的设备号
    .name = BANDBRIDGE_NAME,          // 设备名称
    .fops = &g_bandbridge_global_ops, // 文件操作函数
};

int bandbridge_cdev_register(void)
{
    int ret = 0;

    mutex_init(&g_bandbridge_ctx.bandbridge_lock);

    ret = misc_register(&bandbridge_miscdev);
    if (ret != 0) {
        bandbridge_log_err("[bandbridge_cdev_register] misc_register failed, ret:%d.\n", ret);
        return ret;
    }

    bandbridge_log_info("[bandbridge_cdev_register] bandbridge misc device register success.\n");
    return 0;
}

void bandbridge_cdev_unregister(void)
{
    misc_deregister(&bandbridge_miscdev);
    bandbridge_log_info("[bandbridge_cdev_unregister] bandbridge misc device unregister success.\n");
}
