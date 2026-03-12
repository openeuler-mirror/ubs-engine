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

#ifndef UBS_VIRT_AGENT_OBJECT_DEF_H
#define UBS_VIRT_AGENT_OBJECT_DEF_H

#include <string>
#include <unordered_map>
#include <vector>

const std::string UBS_VA_VM_MIGRATE_PERMISSION = "vm.migrate";
const std::string UBS_VA_QUERY_PERMISSION = "vm.query";
const std::string UBS_VA_FRAGMENTATION_PERMISSION = "vm.fragmentation";
const std::string UBS_VA_CASE_CONF_PERMISSION = "vm.case_conf";
const std::string UBS_VA_CONTAINER_PERMISSION = "vm.container";

static const std::unordered_map<std::string, std::vector<std::string>> ALL_OBJECTS = {
    {UBS_VA_VM_MIGRATE_PERMISSION, {"ubs_virt_agent_make_migrate_decision", "update_page_flow_and_status",
                                    "RackSyncSendForHam", "RackAsyncSendForHam"}},
    {UBS_VA_QUERY_PERMISSION,
     {"ubs_virt_agent_mem_fragmentation_node_info", "ubs_virt_agent_mem_fragmentation_vm_info"}},
    {UBS_VA_FRAGMENTATION_PERMISSION,
     {"ubs_virt_agent_mem_fragmentation_node_anti_affinity", "ubs_virt_agent_mem_borrow_strategy",
      "ubs_virt_agent_mem_borrow_execute", "ubs_virt_agent_mem_migrate_strategy", "ubs_virt_agent_mem_migrate_execute",
      "ubs_virt_agent_mem_return", "ubs_virt_agent_mem_rollback", "ubs_virt_agent_sync_task_query"}},
    {UBS_VA_CASE_CONF_PERMISSION, {"ubs_virt_agent_case_conf_get", "ubs_virt_agent_case_conf_set"}},
    {UBS_VA_CONTAINER_PERMISSION,
     {"ubs_container_info_query", "ubs_container_inject_waterLine", "ubs_container_get_container_pids",
      "ubs_virt_agent_waterline_mem_borrow", "ubs_virt_agent_waterline_mem_migrate",
      "ubs_virt_agent_waterline_mem_return"}}
};

#endif // UBS_VIRT_AGENT_OBJECT_DEF_H