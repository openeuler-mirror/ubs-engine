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

#ifndef LIBUBSE_HELPER_H
#define LIBUBSE_HELPER_H

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "ubs_engine.h"
#include "ubs_engine_urma.h"
#include "ubs_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UBSE_NODE_SIZE (sizeof(uint32_t) + sizeof(uint32_t) * UBS_TOPO_SOCKET_NUM + HOST_NAME_MAX) // ubse_node_t size

#define UBSE_LINK_SIZE (sizeof(uint32_t) * 4)

#define UBSE_MEM_LENDER_SIZE \
    (sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t)) // ubse_mem_lender_t size

#define UBSE_MEM_NUMA_DESC_SIZE (UBS_MEM_MAX_NAME_LENGTH + sizeof(int64_t)) // ubse_mem_numa_desc_size

ubs_error_t ubse_map_sys_error(int sys_errno);

ubs_error_t ubse_map_daemon_error(int daemon_errno);

ubs_error_t ubse_mem_create_req_is_valid(const char *name, uint64_t size);

ubs_error_t ubse_mem_create_with_lender_req_is_valid(const char *name, const ubs_mem_lender_t *lender);

ubs_error_t ubse_mem_name_is_valid(const char *name);

ubs_error_t ubse_mem_fd_create_req_pack(const char *name, uint64_t size, ubs_mem_distance_t distance,
                                        const ubs_mem_fd_owner_t *owner, mode_t mode, uint8_t *buffer);

ubs_error_t ubse_mem_fd_create_with_lender_req_pack(const char *name, const ubs_mem_fd_owner_t *owner, mode_t mode,
                                                    const ubs_mem_lender_t *lender, uint8_t *buffer);

ubs_error_t ubse_mem_fd_delete_req_pack(const char *name, uint8_t *buffer);

ubs_error_t ubse_mem_numa_create_req_pack(const char *name, uint64_t size, ubs_mem_distance_t distance,
                                          uint8_t *buffer);

ubs_error_t ubse_mem_numa_create_with_lender_req_pack(const char *name, const ubs_mem_lender_t *lender,
                                                      uint8_t *buffer);

ubs_error_t ubse_mem_fd_desc_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_fd_desc_t *fd_desc);

ubs_error_t ubse_mem_unma_desc_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_numa_desc_t *numa_desc);

ubs_error_t ubse_mem_fd_desc_list_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_fd_desc_t **fd_descs,
                                         uint32_t *fd_desc_cnt);

ubs_error_t ubse_mem_numa_desc_list_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_numa_desc_t **numa_descs,
                                           uint32_t *numa_desc_cnt);

ubs_error_t ubse_node_list_unpack(const uint8_t *buffer, uint32_t len, ubs_topo_node_t **node_list, uint32_t *node_cnt);

ubs_error_t ubse_node_unpack(const uint8_t *buffer, uint32_t len, ubs_topo_node_t *node);

ubs_error_t ubse_node_cpu_topo_list_unpack(const uint8_t *buffer, uint32_t len, ubs_topo_link_t **cpu_links,
                                           uint32_t *cpu_link_cnt);

ubs_error_t ubse_node_numa_mem_list_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_numastat_t **numa_mem_list,
                                           uint32_t *numa_mem_cnt);

ubs_error_t ubse_urma_dev_get_unpack(const uint8_t *buffer, uint32_t len, urma_device_t **urma_devices,
                                     uint32_t *urma_cnt);

ubs_error_t ubse_urma_dev_alloc_unpack(const uint8_t *buffer, uint32_t len, ubs_urma_dev_path_t *dev_info);
#ifdef __cplusplus
}
#endif
#endif // LIBUBSE_HELPER_H