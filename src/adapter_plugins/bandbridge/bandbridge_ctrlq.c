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
 * Description: bandbridge control queue implementation
 * Author:
 * Create:
 * Note:
 * History:
 */

#include <linux/delay.h>
#include "bandbridge_ctrlq.h"

struct bandbridge_ctrlq_info g_ctrlq_info;
void* __iomem g_ctrlq_va = NULL;
static int bandbridge_ctrlq_timeout = 1000;

int bandbridge_ctrlq_get_sq_size(void)
{
    struct bandbridge_ctrlq_ring* sq = &g_ctrlq_info.sq;
    return sq->depth * CTRLQ_BB_SIZE;
}

int bandbridge_ctrlq_get_rq_size(void)
{
    struct bandbridge_ctrlq_ring* rq = &g_ctrlq_info.rq;
    return rq->depth * CTRLQ_BB_SIZE;
}

static int bandbridge_ctrlq_remain_space(void)
{
    u16 used;
    struct bandbridge_ctrlq_ring* sq = &g_ctrlq_info.sq;
    sq->ci = reg_read(CTRLQ_TX_HEAD_REG) & CTRLQ_TX_HEAD_MASK;
    used = (sq->pi - sq->ci + sq->depth) % sq->depth;
    return sq->depth - used;
}

int bandbridge_ctrlq_check_sq_enough(int sendbuf_size)
{
    int need_size = DIV_ROUND_UP(sendbuf_size, CTRLQ_BB_SIZE);
    if (need_size > bandbridge_ctrlq_remain_space()) {
        return -ENOSPC;
    }
    return 0;
}

static int bandbridge_ctrlq_rq_is_empty(void)
{
    struct bandbridge_ctrlq_ring* rq = &g_ctrlq_info.rq;
    rq->pi = reg_read(CTRLQ_RX_TAIL_REG) & CTRLQ_RX_TAIL_MASK;
    return rq->pi == rq->ci;
}

static void bandbridge_ctrlq_update_rq_ci(u8 bb_num)
{
    struct bandbridge_ctrlq_ring* rq = &g_ctrlq_info.rq;
    rq->ci = (rq->ci + bb_num) % rq->depth;
    reg_write(CTRLQ_RX_HEAD_REG, rq->ci);
}

static void bandbridge_ctrlq_reset_rq_ci(void)
{
    struct bandbridge_ctrlq_ring* rq = &g_ctrlq_info.rq;
    rq->pi = reg_read(CTRLQ_RX_TAIL_REG) & CTRLQ_RX_TAIL_MASK;
    rq->ci = rq->pi;
    reg_write(CTRLQ_RX_HEAD_REG, rq->ci);
}

void bandbridge_ctrlq_send_to_sq(void* sendbuf, int sendbuf_size)
{
    struct bandbridge_ctrlq_ring* sq = &g_ctrlq_info.sq;
    u32 total_size = sendbuf_size;
    u32 size;
    u32 offset = 0;
    int num = DIV_ROUND_UP(sendbuf_size, CTRLQ_BB_SIZE);
    int cnt = 0;
    u8* addr;

    sq->pi = reg_read(CTRLQ_TX_TAIL_REG) & CTRLQ_TX_TAIL_MASK;
    while (cnt < num) {
        addr = sq->base_addr + sq->pi * CTRLQ_BB_SIZE;
        size = min(total_size, (u32)CTRLQ_BB_SIZE);
        memcpy_toio(addr, (u8*)sendbuf + offset, size);
        total_size -= size;
        offset += size;
        sq->pi++;
        if (sq->pi >= sq->depth) {
            sq->pi = 0;
        }
        cnt++;
    }

    reg_write(CTRLQ_TX_TAIL_REG, sq->pi);
}

static void bandbridge_ctrlq_read_data_from_rq(void* recvbuf, u8 bb_num)
{
    struct bandbridge_ctrlq_ring* rq = &g_ctrlq_info.rq;
    u16 pos = rq->ci;
    int i;

    for (i = 0; i < bb_num; i++) {
        memcpy_fromio(recvbuf + i * CTRLQ_BB_SIZE, (u8*)rq->base_addr + pos * CTRLQ_BB_SIZE, CTRLQ_BB_SIZE);
        pos = (pos + 1) % rq->depth;
    }
}

int bandbridge_ctrlq_receive_from_rq(void* recvbuf, int* recvbuf_size, u16 sseq)
{
    struct bandbridge_ctrlq_ring* rq = &g_ctrlq_info.rq;
    struct bandbridge_ctrlq_msg_header head;
    u32 timeout = 0;
    u8* addr;
    u8 bb_num;
    u16 rseq;

    while (timeout < bandbridge_ctrlq_timeout) {
        if (bandbridge_ctrlq_rq_is_empty()) {
            msleep(CTRLQ_TX_WAIT_TIME);
            timeout++;
            bandbridge_log_info("ctrlq rq is empty\n");
            continue;
        }
        rq->ci = reg_read(CTRLQ_RX_HEAD_REG) & CTRLQ_RX_HEAD_MASK;
        addr = rq->base_addr + rq->ci * CTRLQ_BB_SIZE;
        memcpy_fromio(&head, addr, CTRLQ_BB_SIZE);
        rseq = le16_to_cpu(head.seq);
        bb_num = head.bb_num;

        if (unlikely(!bb_num)) {
            bandbridge_log_err("bb_num is invalid.\n");
            bandbridge_ctrlq_reset_rq_ci();
        }

        // 两端的seq高位为01
        if ((rseq & 0x7FFF) != (sseq & 0x7FFF)) {
            bandbridge_log_info("sseq is not match %d %d\n", rseq, sseq);
            bandbridge_ctrlq_update_rq_ci(bb_num);
            continue;
        }

        if (bb_num * CTRLQ_BB_SIZE > *recvbuf_size) {
            bandbridge_ctrlq_update_rq_ci(bb_num);
            bandbridge_log_err("recvbuf is not enough\n");
            return -ENOSPC;
        }

        bandbridge_ctrlq_read_data_from_rq(recvbuf, bb_num);
        bandbridge_ctrlq_update_rq_ci(bb_num);
        return 0;
    }
    bandbridge_log_err("receive from rq timeout\n");
    return -ETIMEDOUT;
}

static int bandbridge_ctrlq_map_reg(void)
{
    u64 ub_ctrlq_pa = UB_REG_BASEADDR + CTRLQ_PER_OFFSET * CTRLQ_FE15_INDEX;
    g_ctrlq_va = reg_map("ctrlq", ub_ctrlq_pa, CTRLQ_REG_LEN);
    if (!g_ctrlq_va) {
        bandbridge_log_err("ctrlq reg map failed\n");
        return -ENOMEM;
    }

    return 0;
}

static void bandbridge_ctrlq_unmap_reg(void)
{
    if (g_ctrlq_va) {
        iounmap(g_ctrlq_va);
        g_ctrlq_va = NULL;
    }
}

static int bandbridge_ctrlq_map_queue(void)
{
    u16 depth_reg_val;
    u32 addr_h;
    u32 addr_l;
    u64 ctrlq_addr;
    size_t size;

    struct bandbridge_ctrlq_ring* sq = &g_ctrlq_info.sq;
    struct bandbridge_ctrlq_ring* rq = &g_ctrlq_info.rq;

    depth_reg_val = (u16)reg_read(CTRLQ_TX_DEPTH_REG) & CTRLQ_TX_DEPTH_MASK;
    sq->depth = depth_reg_val * CTRLQ_DEPTH_UINT;

    depth_reg_val = (u16)reg_read(CTRLQ_RX_DEPTH_REG) & CTRLQ_RX_DEPTH_MASK;
    rq->depth = depth_reg_val * CTRLQ_DEPTH_UINT;

    if (sq->depth == 0 || rq->depth == 0) {
        bandbridge_log_err("ctrlq depth is 0\n");
        return -EINVAL;
    }

    sq->pi = reg_read(CTRLQ_TX_TAIL_REG) & CTRLQ_TX_TAIL_MASK;
    sq->ci = reg_read(CTRLQ_TX_HEAD_REG) & CTRLQ_TX_HEAD_MASK;

    rq->pi = reg_read(CTRLQ_RX_TAIL_REG) & CTRLQ_RX_TAIL_MASK;
    reg_write(CTRLQ_RX_HEAD_REG, rq->pi);
    rq->ci = rq->pi;

    addr_l = reg_read(CTRLQ_TX_BASEADDR_L_REG);
    addr_h = reg_read(CTRLQ_TX_BASEADDR_H_REG);
    // 将addr_h和addr_l拼接成一个64位地址，addr_h作为高32位，addr_l作为低32位
    ctrlq_addr = ((u64)addr_h << 32) | addr_l;
    size = sq->depth * CTRLQ_BB_SIZE;
    sq->base_addr = ioremap(ctrlq_addr, size);
    if (!sq->base_addr) {
        bandbridge_log_err("ctrlq tx queue map failed\n");
        return -ENOMEM;
    }

    addr_l = reg_read(CTRLQ_RX_BASEADDR_L_REG);
    addr_h = reg_read(CTRLQ_RX_BASEADDR_H_REG);
    // 将addr_h和addr_l拼接成一个64位地址，addr_h作为高32位，addr_l作为低32位
    ctrlq_addr = ((u64)addr_h << 32) | addr_l;
    size = rq->depth * CTRLQ_BB_SIZE;
    rq->base_addr = ioremap(ctrlq_addr, size);
    if (!rq->base_addr) {
        iounmap(sq->base_addr);
        bandbridge_log_err("ctrlq rx queue map failed\n");
        return -ENOMEM;
    }

    return 0;
}

static void bandbridge_ctrlq_unmap_queue(void)
{
    struct bandbridge_ctrlq_ring* sq = &g_ctrlq_info.sq;
    struct bandbridge_ctrlq_ring* rq = &g_ctrlq_info.rq;

    if (sq->base_addr) {
        iounmap(sq->base_addr);
        sq->base_addr = NULL;
    }

    if (rq->base_addr) {
        iounmap(rq->base_addr);
        rq->base_addr = NULL;
    }
}

int bandbridge_ctrlq_init(void)
{
    int ret;

    ret = bandbridge_ctrlq_map_reg();
    if (ret != 0) {
        return ret;
    }

    ret = bandbridge_ctrlq_map_queue();
    if (ret != 0) {
        bandbridge_ctrlq_unmap_reg();
        return ret;
    }

    return ret;
}

void bandbridge_ctrlq_deinit(void)
{
    bandbridge_ctrlq_unmap_queue();
    bandbridge_ctrlq_unmap_reg();
}
