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

#ifndef UBS_VIRT_AGENT_MEM_FRAGMENTATION_H
#define UBS_VIRT_AGENT_MEM_FRAGMENTATION_H

#include "mem_fragmentation_msg.h"
#include "ubs_virt_agent_base.h"

using namespace vm;
using namespace vm::mem_fragmentation;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t numaId;   // CPU NUMA ID
    int16_t socketId; // CPU socket ID
    bool isLocal;
    uint64_t pageSize; // VM page size, default is 2 MB huge pages (2048 KBytes)
    int64_t usedMem;
} vm_numa_info_t;

typedef struct {
    char nodeId[VIRT_MEM_MAX_NODE_ID_LENGTH]; // Physical node ID (from control-plane configuration file)
    char hostName[UBS_VA_HOST_NAME_MAX];      // Physical node host name (from VM XML definition)
    char uuid[VIRT_MAX_UUID_LENGTH];          // VM UUID (from VM XML definition)
    char name[VIRT_MAX_NAME_LENGTH];          // VM name (from VM XML definition)
    char state[VIRT_MAX_STATE_LENGTH];        // VM state
    int64_t vmCreateTime;
    uint64_t maxMem;
    pid_t pid;
} vm_meta_data_t;

typedef struct {
    time_t timestamp;
    vm_meta_data_t metadata;
    vm_numa_info_t* numaInfo;
    uint64_t numaInfoCount;
} vm_domain_info_t;

/**
 * @brief  query numa node memory fragmentation information
 * @param node_list [OUT] numa node information list
 * @param node_cnt  [OUT] number of numa nodes returned
 * @return 0 for success, non-zero for error
 */
virt_agent_ret_t ubs_virt_agent_mem_fragmentation_node_info(numa_info_t** node_list, uint32_t* node_cnt);

/**
 * @brief  query vm memory fragmentation information
 * @param vm_info_list [OUT] vm domain information list
 * @param vm_info_cnt  [OUT] number of vm domain information entries returned
 * @return 0 for success, non-zero for error
 */
virt_agent_ret_t ubs_virt_agent_mem_fragmentation_vm_info(vm_domain_info_t** vm_info_list, uint32_t* vm_info_cnt);

/**
 * @brief  set numa node anti-affinity configuration for memory fragmentation optimization
 * @param dict [IN] node anti-affinity dictionary, including node keys and their anti-affinity node lists
 * @return 0 for success, non-zero for error
 */
virt_agent_ret_t ubs_virt_agent_mem_fragmentation_node_anti_affinity(const NodeAntiDictionary* dict);

/**
 * @brief  calculate memory borrow strategy based on source memory fragmentation information
 * @param src_param       [IN] source memory borrow input parameters
 * @param borrow_strategy [OUT] calculated memory borrow strategy result
 * @return 0 for success, non-zero for error
 */
virt_agent_ret_t ubs_virt_agent_mem_borrow_strategy(const src_memory_borrow_param* src_param,
                                                    borrow_strategy_c* borrow_strategy);

/**
 * @brief Executes a memory borrowing operation based on the provided borrowing strategy.
 * This function performs the actual memory borrowing process according to the specified
 * borrowing strategy. Upon successful execution, it populates the output result structure
 * with the list of borrowed memory identifiers and corresponding destination NUMA node IDs.
 *
 * @param borrow_setting [IN]  The borrowing strategy configuration, including source node,
 *                             target nodes, memory size, and other execution parameters.
 * @param result         [OUT]  Output structure to store the result of the borrowing operation.
 *                             On success: contains the list of borrowed memory IDs and their
 *                             corresponding destination NUMA IDs.
 *                             On failure: result fields are cleared/undefined.
 *
 * @return 0 if successful, a non-zero error code otherwise.
 */
virt_agent_ret_t ubs_virt_agent_mem_borrow_execute(const borrow_setting_c* borrow_setting, mem_borrow_result_c* result);

/**
 * @brief  calculate memory migrate strategy based on memory fragmentation information
 * @param srcParam [IN]  source parameters for memory migrate strategy calculation
 * @param strategy [OUT] calculated memory migrate strategy result
 * @return 0 for success, non-zero for error
 */
virt_agent_ret_t ubs_virt_agent_mem_migrate_strategy(const MemMigrateStrategySrcParam* srcParam,
                                                     MemMigrateStrategy* strategy);

/**
 * @brief  execute memory migrate operation based on calculated migrate strategy
 * @param srcParam [IN] source parameters for memory migrate execution
 * @return 0 for success, non-zero for error
 */
virt_agent_ret_t ubs_virt_agent_mem_migrate_execute(const MemMigrateExecuteSrcParam* srcParam);

/**
 * @brief Returns borrowed memory for a specified node.
 *
 * Initiates the return of previously borrowed memory. If asynchronous, a task ID is returned
 * for later status querying.
 *
 * @param isAsync      [IN]  If true, performs async return; otherwise, sync.
 * @param task_id      [OUT] Task ID string (valid only if isAsync is true).
 * @param task_id_len  [OUT] Length of the task ID string.
 *
 * @return VA_SUCCESS on success, error code otherwise.
 */
virt_agent_ret_t ubs_virt_agent_mem_return(bool isAsync, char** task_id, uint32_t* task_id_len);

/**
 * @brief Synchronously query async task status and result.
 * @param task_id     Task ID string (null-terminated)
 * @param task_id_len Length of task_id (excluding null terminator)
 * @param result      Output: task status, result code, error msg, serialized data
 * @return VM_OK on success, error code on failure
 */
virt_agent_ret_t ubs_virt_agent_sync_task_query(char* task_id, uint32_t task_id_len, async_task_info_c* result);

/**
 * @brief  rollback borrowed memory for the specified node and borrow id list
 * @param srcParam  [IN] source parameters for memory rollback execution
 */
virt_agent_ret_t ubs_virt_agent_mem_rollback(const RollbackSrcParam* srcParam);

/** ==============big memory virtual machine============== */
virt_agent_ret_t ubs_virt_agent_mem_fragmentation_node_info_list(node_info_list_s *node_info_list);

virt_agent_ret_t ubs_virt_agent_mem_borrow(const mem_borrow_param_s *param, const bool is_async,
                                           mem_borrow_result_s *result);

virt_agent_ret_t ubs_virt_agent_page_swap_enable(const pid_t pid, const page_swap_enable_s *page_swap_enable);
#ifdef __cplusplus
}
#endif
#endif // UBS_VIRT_AGENT_MEM_FRAGMENTATION_H
