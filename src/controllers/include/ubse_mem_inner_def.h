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

#ifndef UBSE_MEM_INNER_DEF_H
#define UBSE_MEM_INNER_DEF_H
#ifdef __cplusplus
extern "C" {
#endif

#define UBSE_MEM_RESOURCE_ID_MAX_LEN 48
#define UBSE_MEM_EXPORT_NUMA_MAX_NUM 2
#define UBSE_MEM_NUMA_MAX_NUM 8
#define UBSE_MEM_CUSTOM_DATA_MAX_LEN 128
#define UBSE_MEM_NUMA_BIND_CORE_MAX_NUM 128
#define UBSE_MEM_ID_MAX_LENGTH 48 // 节点id，共享内存name，最大限制48
#define UBSE_MEM_MAX_TOPOLOGY_HOSTS 16
#define UBSE_MEM_RESOURCE_KIND_LEN 16
#define UBSE_MEM_CONFIG_EXECUTE_STATUS_LEN 16
#define UBSE_MEM_CONFIG_EXECUTE_STATUS_TYPE_LEN 8
#define UBSE_MEM_BORROW_TYPE_LEN 16
#define UBSE_MEM_HIGH_WATERMARK_DEFAULT 88
#define UBSE_MEM_LOW_WATERMARK_DEFAULT 11
#define UBSE_MEM_APP_BORROW_REQUEST "APP_BORROW"
#define UBSE_MEM_SHARE_REQUEST "SHARE"
#define UBSE_MEM_WATER_BORROW_REQUEST "WATER_BORROW"
#define UBSE_MEM_APP_PRI_REQUEST "APP_PRI_BORROW"
#define UBSE_MEM_CREATE_TYPE "CREATE"
#define UBSE_MEM_DELETE_TYPE "DELETE"
#define UBSE_MEM_ATTACH_TYPE "ATTACH"
#define UBSE_MEM_DETACH_TYPE "DETACH"

typedef enum tag_ubse_mem_perf_level {
    UBSE_MEM_PERF_LEVEL0, // 对应直连
    UBSE_MEM_PERF_LEVEL1, // 对应1跳节点
    UBSE_MEM_PERF_LEVEL2, // 对应超过一跳节点
} ubse_mem_perf_level;

typedef enum tag_ubse_mem_shm_region_type {
    UBSE_MEM_ALL2ALL_SHARE, /* *SHM域类型为域内任何节点提供内存都可以被域内所有节点共享访问 */
    UBSE_MEM_ONE2ALL_SHARE, /* *SHM域类型为域内单一节点作为提供方都可以被域内所有节点共享访问 */
} ubse_mem_shm_region_type;
#ifdef __cplusplus
}
#endif
#endif // UBSE_MEM_INNER_DEF_H
