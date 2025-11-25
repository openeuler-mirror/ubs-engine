/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MEM_OBMM_DEF_H
#define UBSE_MEM_OBMM_DEF_H
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

struct ubse_mem_obmm_mem_desc {
    uint64_t addr;
    uint64_t length;
    uint32_t tokenid;
    uint32_t scna;
    uint32_t dcna;
    uint32_t seid;
    uint32_t deid;
    uint16_t marId;
} __attribute__((aligned(8)));

#ifdef __cplusplus
}
#endif
#endif // UBSE_MEM_OBMM_DEF_H