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

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include "ubs_engine.h"
#include "ubs_error.h"
#include "ubs_engine_urma.h"
#include "ubse_ipc_client.h"

ubs_error_t ubse_map_sys_error(int sys_errno);

ubs_error_t ubse_map_daemon_error(uint32_t daemon_errno);

ubs_error_t ubse_mem_create_req_is_valid(const char *name, uint64_t size);

ubs_error_t ubse_mem_create_with_lender_req_is_valid(const char *name, const ubs_mem_lender_t *lender,
                                                     uint32_t lender_cnt);

ubs_error_t ubse_mem_create_with_candidate_req_is_valid(const char *name, uint64_t size, const uint32_t *slot_ids,
                                                        uint32_t slot_cnt);

ubs_error_t ubse_mem_name_is_valid(const char *name);

ubs_error_t ubse_size_is_valid(const uint64_t size);

ubs_error_t ubse_mem_nodes_t_is_valid(const ubs_mem_nodes_t *region);

ubs_error_t ubse_mem_shm_create_req_valid(const char *name, uint64_t size, const ubs_mem_nodes_t *region,
                                          const ubs_mem_nodes_t *provider);

ubs_error_t ubse_mem_shm_create_with_lender_req_valid(const char *name, const ubs_mem_nodes_t *region,
                                                      const ubs_mem_lender_t *lender);

size_t ubse_string_calc_size(const char *str, size_t max_len);

size_t ubse_mem_fd_create_req_calc_size(const char *name);

size_t ubse_mem_fd_create_with_lender_req_calc_size(const char *name, uint32_t lender_cnt);

size_t ubse_mem_fd_create_with_candidate_req_calc_size(const char *name, uint32_t slot_cnt);

size_t ubse_mem_fd_permission_req_calc_size(const char *name);

size_t ubse_mem_numa_create_req_calc_size(const char *name);

size_t ubse_mem_numa_create_with_lender_req_calc_size(const char *name, uint32_t lender_cnt);

size_t ubse_mem_numa_create_with_candidate_req_calc_size(const char *name, uint32_t slot_cnt);

ubs_error_t ubse_mem_shm_create_req_build(ubse_api_buffer_t *ptr, const char *name, const ubs_mem_nodes_t *region,
                                          const ubs_mem_nodes_t *provider);

ubs_error_t ubse_mem_shm_create_with_affinity_req_build(ubse_api_buffer_t *ptr, const char *name,
                                                        const ubs_mem_nodes_t *region, const ubs_mem_nodes_t *provider);

ubs_error_t ubse_mem_shm_create_with_lender_req_build(ubse_api_buffer_t *ptr, const char *name,
                                                      uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN], uint64_t flag,
                                                      const ubs_mem_nodes_t *region, const ubs_mem_lender_t *lender);

ubs_error_t ubse_mem_shm_attach_req_build(ubse_api_buffer_t *ptr, const char *name);

ubs_error_t ubse_mem_shm_common_req_build(ubse_api_buffer_t *ptr, const char *name);

ubs_error_t ubse_mem_shm_get_req_build(ubse_api_buffer_t *ptr, const char *name);

ubs_error_t ubse_mem_shm_detach_req_build(ubse_api_buffer_t *ptr, const char *name);

ubs_error_t ubse_mem_shm_delete_req_build(ubse_api_buffer_t *ptr, const char *name);

ubs_error_t ubse_mem_shm_fault_get_req_build(const char *name, ubse_api_buffer_t *ptr);

ubs_error_t ubse_string_pack(const char *str, size_t max_len, uint8_t *buffer);

ubs_error_t ubse_mem_fd_create_req_pack(const char *name, uint64_t size, const ubs_mem_fd_owner_t *owner, mode_t mode,
                                        ubs_mem_distance_t distance, uint8_t *buffer);

ubs_error_t ubse_mem_fd_create_with_lender_req_pack(const char *name, const ubs_mem_fd_owner_t *owner, mode_t mode,
                                                    const ubs_mem_lender_t *lender, uint32_t lender_cnt,
                                                    uint8_t *buffer);

ubs_error_t ubse_mem_fd_create_with_candidate_req_pack(const char *name, uint64_t size, const ubs_mem_fd_owner_t *owner,
                                                       mode_t mode, const uint32_t *slot_ids, uint32_t slot_cnt,
                                                       uint8_t *buffer);

ubs_error_t ubse_mem_fd_permission_req_pack(const char *name, const ubs_mem_fd_owner_t *owner, mode_t mode,
                                            uint8_t *buffer);

ubs_error_t ubse_mem_numa_create_req_pack(const char *name, uint64_t size, ubs_mem_distance_t distance,
                                          uint8_t *buffer);

ubs_error_t ubse_mem_numa_create_with_lender_req_pack(const char *name, const ubs_mem_lender_t *lender,
                                                      uint32_t lender_cnt, uint8_t *buffer);

ubs_error_t ubse_mem_numa_create_with_candidate_req_pack(const char *name, uint64_t size, const uint32_t *slot_ids,
                                                         uint32_t slot_cnt, uint8_t *buffer);

ubs_error_t ubse_mem_shm_create_req_pack(const char *name, uint64_t size, uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN],
                                         uint64_t flag, const ubs_mem_nodes_t *region, const ubs_mem_nodes_t *provider,
                                         uint8_t *buffer);

ubs_error_t ubse_mem_shm_create_with_affinity_req_pack(const char *name, uint64_t size,
                                                       uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN], uint64_t flag,
                                                       const ubs_mem_nodes_t *region, const ubs_mem_nodes_t *provider,
                                                       uint8_t *buffer, uint32_t affinity_socket_id);

ubs_error_t ubse_mem_shm_create_with_lender_req_pack(const char *name, const uint8_t usr_info[UBS_MEM_MAX_USR_INFO_LEN],
                                                     uint64_t flag, const ubs_mem_nodes_t *region,
                                                     const ubs_mem_lender_t *lender, uint8_t *buffer);

ubs_error_t ubse_mem_shm_attach_req_pack(const char *name, const ubs_mem_fd_owner_t *owner, mode_t mode,
                                         uint8_t *buffer);

ubs_error_t ubse_mem_shm_common_pack(const char *name, uint8_t *buffer);

ubs_error_t ubse_mem_shm_get_req_pack(const char *name, uint8_t *buffer);

ubs_error_t ubse_mem_shm_detach_req_pack(const char *name, uint8_t *buffer);

ubs_error_t ubse_mem_shm_delete_req_pack(const char *name, uint8_t *buffer);

ubs_error_t ubse_mem_shm_fault_get_req_pack(const char *name, uint8_t *buffer);

ubs_error_t ubse_mem_fd_desc_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_fd_desc_t *fd_desc);

ubs_error_t ubse_mem_numa_desc_unpack(const uint8_t *buffer, uint32_t len, ubs_mem_numa_desc_t *numa_desc);

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

ubs_error_t ubse_mem_shm_attach_resp_unpack(uint8_t *buffer, uint32_t length, ubs_mem_shm_desc_t **shm_desc_ptr);

ubs_error_t ubse_mem_shm_get_resp_unpack(uint8_t *buffer, uint32_t length, ubs_mem_shm_desc_t **shm_desc_ptr);

ubs_error_t ubse_mem_shm_list_resp_unpack(uint8_t *buffer, uint32_t length, ubs_mem_shm_desc_t **shm_descs_ptr,
                                          uint32_t *shm_desc_cnt);

ubs_error_t ubse_mem_shm_fault_get_resp_unpack(uint8_t *buffer, uint32_t len, ubs_mem_memids_fault_t *shm_status);

ubs_error_t ubse_urma_dev_unpack(const uint8_t *buffer, uint32_t len, ubs_urma_dev_t **urma_devices,
                                 uint32_t *urma_cnt);

ubs_error_t ubse_urma_dev_info_unpack(const uint8_t *buffer, uint32_t len, ubs_urma_dev_info_t *dev_info);

ubs_error_t ubse_urma_qos_unpack(const uint8_t *buffer, uint32_t len, uint32_t *minBandWidth, uint32_t *maxBandWidth);



#endif // LIBUBSE_HELPER_H