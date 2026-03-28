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

#ifndef VM_SDK_DEF_H
#define VM_SDK_DEF_H

typedef enum {
    UBS_VA_VM_MIGRATE = 0x1001,         // Module code for VM Migration
    UBS_VA_QUERY = 0x1002,              // Module code for Query
    UBS_VA_MEM_FRAGMENTATION = 0x1003,  // Module code for Memory Fragmentation
    UBS_VA_CASE_CONF = 0x1004,          // Module code for Case Conf
    UBS_VA_CONTAINER = 0x1005,          // Module code for container
} ubs_virt_agent_module_code_t;

typedef enum {
    UBS_VA_MAKE_DECISION = 0x0001,                    // Op code for making migration decision
    UBS_VA_PAGE_FLOW_AND_UPDATE_STATUS = 0x0002,      // Op code for update page flow and vm status
    UBS_VA_HAM_NORTH = 0x0003,                        // Op code for ham migration
} ubse_virt_agent_ham_migrate_op_code_t;

typedef enum {
    UBS_VA_NUMA_INFO = 0x0001,          // Op code for NUMA information
    UBS_VA_VM_INFO = 0x0002,            // Op code for VM information
} ubs_virt_agent_query_op_code_t;

typedef enum {
    UBS_VA_NODE_ANTI_AFFINITY = 0x0001,         // Op code for NODE_ANTI_AFFINITY
    UBS_VA_MEM_BORROW_STRATEGY = 0x0002,        // Op code for MEM_BORROW_STRATEGY
    UBS_VA_MEM_BORROW_EXECUTE = 0x0003,         // Op code for MEM_BORROW_EXECUTE
    UBS_VA_MEM_MIGRATE_STRATEGY = 0x0004,       // Op code for MEM_MIGRATE_STRATEGY
    UBS_VA_MEM_MIGRATE_EXECUTE = 0x0005,        // Op code for MEM_MIGRATE_EXECUTE
    UBS_VA_MEM_RETURN = 0x0006,                 // Op code for MEM_RETURN
    UBS_VA_MEM_ROLLBACK = 0x0007,               // Op code for MEM_ROLLBACK
    UBS_VA_SYNC_TASK_QUERY = 0x0008,
} ubs_virt_agent_mem_fragmentation_op_code_t;

typedef enum {
    UBS_VA_CASE_CONF_GET = 0x0001,      // Op code for getting case configuration
    UBS_VA_CASE_CONF_SET = 0x0002,      // Op code for setting case configuration
} ubs_virt_agent_case_conf_op_code_t;

typedef enum {
    UBS_VA_CONTAINER_GET_MEM_INFO_FOR_PID = 0x0001,    // Op code for getting mem info for pid
    UBS_VA_CONTAINER_UPDATE_WATERLINE = 0x0002,        // Op code for update water line
    UBS_VA_CONTAINER_GET_CONTAINER_PIDS = 0x0003,      // Op code for getting container pid list
    UBS_VA_CONTAINER_WATERLINE_MEM_BORROW = 0x0004,    // Op code for waterline mem borrow
    UBS_VA_CONTAINER_WATERLINE_MEM_MIGRATE = 0x0005,   // Op code for waterline mem migrate
    UBS_VA_CONTAINER_WATERLINE_MEM_RETURN = 0x0006,    // Op code for waterline mem return
} ubs_virt_agent_container_op_code_t;

typedef enum {
    UBS_VA_HAM_MIGRATE_SCENE = 0x0001,
    UBS_VA_FAST_RECOVERY_SCENE = 0x0002,
} ubs_virt_agent_libvirt_scene_code_t;
#endif // VM_SDK_DEF_H
