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

#ifndef BANDBRIDGE_CTRLQ_H
#define BANDBRIDGE_CTRLQ_H

#include <asm/io.h>
#include "bandbridge_log.h"
#define CTRLQ_BB_SIZE 32

//#define UB_REG_BASEADDR 0x3300000000
#define UB_REG_BASEADDR 0x3319000000
//#define CTRLQ_FE15_INDEX 17
#define CTRLQ_FE15_INDEX 1
#define CTRLQ_PER_OFFSET 0x20000
#define CTRLQ_REG_LEN 0x20000

#define CTRLQ_TX_BASEADDR_L_REG 0x18800
#define CTRLQ_TX_BASEADDR_H_REG 0x18804
#define CTRLQ_TX_DEPTH_REG 0x18808
#define CTRLQ_TX_TAIL_REG 0x18810
#define CTRLQ_TX_HEAD_REG 0x18814
#define CTRLQ_RX_BASEADDR_L_REG 0x18818
#define CTRLQ_RX_BASEADDR_H_REG 0x1881c
#define CTRLQ_RX_DEPTH_REG 0x18820
#define CTRLQ_RX_TAIL_REG 0x18824
#define CTRLQ_RX_HEAD_REG 0x18828

#define CTRLQ_TX_DEPTH_MASK 0x000007FFL
#define CTRLQ_TX_TAIL_MASK 0x0000FFFFL
#define CTRLQ_TX_HEAD_MASK 0x0000FFFFL
#define CTRLQ_RX_DEPTH_MASK 0x000007FFL
#define CTRLQ_RX_TAIL_MASK 0x0000FFFFL
#define CTRLQ_RX_HEAD_MASK 0x0000FFFFL

#define CTRLQ_DEPTH_UINT 8
#define CTRLQ_TX_WAIT_TIME 10

struct bandbridge_ctrlq_ring {
    u16 ci;
    u16 pi;
    u16 depth;
    void __iomem* base_addr;
};

struct bandbridge_ctrlq_info {
    struct bandbridge_ctrlq_ring sq;
    struct bandbridge_ctrlq_ring rq;
};

struct bandbridge_ctrlq_msg_header {
    u8 version;
    u8 service_type;
    u8 bb_num;
    u8 opcode;
    u8 ret;
    u16 seq;
    u8 resv[25];
};

int bandbridge_ctrlq_init(void);
void bandbridge_ctrlq_deinit(void);
int bandbridge_ctrlq_get_sq_size(void);
int bandbridge_ctrlq_get_rq_size(void);
int bandbridge_ctrlq_check_sq_enough(int sendbuf_size);
void bandbridge_ctrlq_send_to_sq(void* sendbuf, int sendbuf_size);
int bandbridge_ctrlq_receive_from_rq(void* recvbuf, int* recvbuf_size, u16 sseq);
extern void* __iomem g_ctrlq_va;

static inline void* __iomem reg_map(char* name, u64 base_addr, int len)
{
    void* __iomem va = NULL;
    va = ioremap(base_addr, len);
    if (!va)
        bandbridge_log_err("%s reg map failed\n", name);

    return va;
}

static inline u32 reg_read(u32 reg_offset)
{
    u32 val;
    val = readl(g_ctrlq_va + reg_offset);
    return val;
}

static inline void reg_write(u32 reg_offset, u32 value)
{
    writel(value, g_ctrlq_va + reg_offset);
}

#endif //BANDBRIDGE_CTRLQ_H
