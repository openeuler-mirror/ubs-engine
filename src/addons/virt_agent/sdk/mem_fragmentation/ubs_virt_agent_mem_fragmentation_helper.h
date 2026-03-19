/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBS_VIRT_AGENT_MEM_FRAGMENTATION_HELPER_H
#define UBS_VIRT_AGENT_MEM_FRAGMENTATION_HELPER_H

#include <cstdint>
#include "ubs_virt_agent_mem_fragmentation.h"
#include "mem_fragmentation_msg.h"

using namespace vm;
typedef struct {
    const uint8_t *ptr;
    size_t remaining;
} unpack_ctx_t;

virt_agent_ret_t ubse_node_info_unpack(uint8_t *buffer, uint32_t len, numa_info_t **numa_infos, uint32_t *node_cnt);

virt_agent_ret_t ubse_vm_info_unpack(uint8_t *buffer, uint32_t len, vm_domain_info_t **vm_infos, uint32_t *node_cnt);

uint8_t* allocate_memory(size_t buffer_size);

virt_agent_ret_t serialize_data(const NodeAntiDictionary& node_dict, uint8_t* buffer);

virt_agent_ret_t ubse_mem_borrow_strategy_msg_unpack(uint8_t *buffer, uint32_t len, borrow_strategy_c *borrow_strategy);

virt_agent_ret_t ubse_mem_borrow_execute_msg_unpack(uint8_t *buffer, uint32_t len, mem_borrow_result_c *result);

virt_agent_ret_t ubse_mem_task_info_query_msg_unpack(uint8_t *buffer, uint32_t len, async_task_info_c *result);

virt_agent_ret_t ubse_mem_migrate_strategy_msg_unpack(uint8_t *buffer, uint32_t len, MemMigrateStrategy* strategy);

#endif // UBS_VIRT_AGENT_MEM_FRAGMENTATION_HELPER_H
