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

#ifndef UBS_ENGINE_OBJECT_DEF_H
#define UBS_ENGINE_OBJECT_DEF_H

#include <string>
#include <unordered_map>
#include <vector>

static const std::unordered_map<std::string, std::vector<std::string>> ALL_OBJECTS = {
    {"mem.fd",
     {"ubs_mem_fd_create", "ubs_mem_fd_create_with_lender", "ubs_mem_fd_create_with_candidate", "ubs_mem_fd_permission",
      "ubs_mem_fd_get", "ubs_mem_fd_list", "ubs_mem_fd_delete", "ubs_mem_fd_fault_register",
      "ubs_mem_fd_get_memid_by_import"}},
    {"mem.numa",
     {"ubs_mem_numa_create", "ubs_mem_numa_create_with_lender", "ubs_mem_numa_create_with_candidate",
      "ubs_mem_numa_get", "ubs_mem_numa_list", "ubs_mem_numa_delete", "ubs_mem_numa_fault_register",
      "ubs_mem_numa_get_memid_by_import"}},
    {"mem.shm",
     {"ubs_mem_shm_create", "ubs_mem_shm_create_with_affinity", "ubs_mem_shm_create_with_lender", "ubs_mem_shm_attach",
      "ubs_mem_shm_get", "ubs_mem_shm_list", "ubs_mem_shm_list_with_prefix", "ubs_mem_shm_detach", "ubs_mem_shm_delete",
      "ubs_mem_shm_fault_get", "ubs_mem_shm_fault_register", "ubs_mem_shm_get_memid_by_import"}},
    {"mem.stat", {"ubs_mem_numastat_get"}},
    {"topo", {"ubs_topo_node_list", "ubs_topo_node_local_get", "ubs_topo_link_list"}},
    {"urma",
     {"ubs_urma_dev_get", "ubs_urma_dev_alloc", "ubs_urma_dev_free", "ubs_urma_qos_create", "ubs_urma_qos_delete",
      "ubs_urma_qos_get"}}};

#endif // UBS_ENGINE_OBJECT_DEF_H
