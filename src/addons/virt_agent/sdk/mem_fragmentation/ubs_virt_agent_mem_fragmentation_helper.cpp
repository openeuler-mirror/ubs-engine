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

#include "ubs_virt_agent_mem_fragmentation_helper.h"

#include <ubse_ipc_log.h>
#include <vector>
#include "ubs_virt_agent_mem_fragmentation.h"

virt_agent_ret_t ubse_node_info_unpack(uint8_t *buffer, uint32_t len, numa_info_t **numa_infos, uint32_t *node_cnt)
{
    vm::MemFragmentationMsg msg{buffer, len};
    auto ret = msg.Deserialize();
    if (ret != 0) {
        IPC_LOG_ERROR << "Failed to deserialize MemFragmentationMsg. Error code: " << ret;
        return VA_ERROR_DESERIALIZE_FAILED;
    }

    std::vector<numa_info_t> numaInfoList{};
    ret = msg.GetNumaInfo(numaInfoList);
    if (ret != 0) {
        IPC_LOG_ERROR << "Failed to get numa info. Error code: " << ret;
        return VA_ERROR_BASE;
    }
    *node_cnt = static_cast<uint32_t>(numaInfoList.size());
    if (*node_cnt == 0) {
        return VA_SUCCESS;
    }
    *numa_infos = (numa_info_t *)calloc(*node_cnt, sizeof(numa_info_t));
    if (*numa_infos == nullptr) {
        IPC_LOG_ERROR << "Memory allocation failed for numa_infos.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    for (uint32_t i = 0; i < *node_cnt; ++i) {
        (*numa_infos)[i] = numaInfoList[i];
    }
    return VA_SUCCESS;
}

virt_agent_ret_t ubse_vm_info_unpack(uint8_t *buffer, uint32_t len, vm_domain_info_t **vm_infos, uint32_t *node_cnt)
{
    vm::MemFragmentationVmInfoMsg msg{buffer, len};
    auto ret = msg.Deserialize();
    if (ret != 0) {
        IPC_LOG_ERROR << "Failed to deserialize MemFragmentationVmInfoMsg. Error code: " << ret;
        return VA_ERROR_DESERIALIZE_FAILED;
    }

    std::vector<vm_domain_info_for_c> vmInfoList = msg.GetVmInfo();
    *node_cnt = static_cast<uint32_t>(vmInfoList.size());
    if (*node_cnt == 0) {
        return VA_SUCCESS;
    }
    *vm_infos = (vm_domain_info_t *)calloc(*node_cnt, sizeof(vm_domain_info_t));
    if (*vm_infos == nullptr) {
        IPC_LOG_ERROR << "Memory allocation failed for numa_infos.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    for (uint32_t i = 0; i < *node_cnt; ++i) {
        (*vm_infos)[i] = *reinterpret_cast<vm_domain_info_t*>(&vmInfoList[i]);
    }
    return VA_SUCCESS;
}

uint8_t* allocate_memory(size_t buffer_size)
{
    uint8_t* buffer = new (std::nothrow) uint8_t[buffer_size];
    if (buffer == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory.";
        return nullptr;
    }
    return buffer;
}


virt_agent_ret_t serialize_data(const NodeAntiDictionary& node_dict, uint8_t* buffer)
{
    uint32_t entries_count = node_dict.entry_count;
    if (memcpy_s(buffer, sizeof(uint32_t), &entries_count, sizeof(uint32_t)) != 0) {
        IPC_LOG_ERROR << "Failed to copy entries_count.";
        return VA_ERROR_MEM_COPY_FAILED;
    }
    buffer += sizeof(uint32_t);
    for (size_t i = 0; i < node_dict.entry_count; ++i) {
        const struct KeyValuePair& entry = node_dict.entries[i];

        uint32_t key_length = strlen(entry.key) + 1;
        if (memcpy_s(buffer, sizeof(uint32_t), &key_length, sizeof(uint32_t)) != 0) {
            IPC_LOG_ERROR << "Failed to copy key_length.";
            return VA_ERROR_MEM_COPY_FAILED;
        }
        buffer += sizeof(uint32_t);

        if (memcpy_s(buffer, key_length, entry.key, key_length) != 0) {
            IPC_LOG_ERROR << "Failed to copy key.";
            return VA_ERROR_MEM_COPY_FAILED;
        }
        buffer += key_length;

        uint32_t value_count = entry.value_count;
        if (memcpy_s(buffer, sizeof(uint32_t), &value_count, sizeof(uint32_t)) != 0) {
            IPC_LOG_ERROR << "Failed to copy value_count.";
            return VA_ERROR_MEM_COPY_FAILED;
        }
        buffer += sizeof(uint32_t);

        for (size_t j = 0; j < value_count; ++j) {
            uint32_t value_length = strlen(entry.value[j]) + 1;
            if (memcpy_s(buffer, sizeof(uint32_t), &value_length, sizeof(uint32_t)) != 0) {
                IPC_LOG_ERROR << "Failed to copy value_length.";
                return VA_ERROR_MEM_COPY_FAILED;
            }
            buffer += sizeof(uint32_t);

            if (memcpy_s(buffer, value_length, entry.value[j], value_length) != 0) {
                IPC_LOG_ERROR << "Failed to copy value.";
                return VA_ERROR_MEM_COPY_FAILED;
            }
            buffer += value_length;
        }
    }
    return VA_SUCCESS;
}

virt_agent_ret_t ubse_mem_borrow_strategy_msg_unpack(uint8_t *buffer, uint32_t len, borrow_strategy_c *borrow_strategy)
{
    MemFragmentationMemBorrowStrategyOutputMsg msg{buffer, len};
    auto ret = msg.Deserialize();
    if (ret != 0) {
        IPC_LOG_ERROR << "Failed to deserialize MemFragmentationMemBorrowStrategyMsg. Error code: " << ret;
        return VA_ERROR_DESERIALIZE_FAILED;
    }

    *borrow_strategy = msg.GetMemBorrowStrategyOutputMsg();
    return VA_SUCCESS;
}

void free_borrow_ids_ptr(char ***borrow_ids_ptr, size_t length)
{
    if (borrow_ids_ptr == nullptr || *borrow_ids_ptr == nullptr) {
        return;
    }

    char **array = *borrow_ids_ptr;

    for (size_t i = 0; i < length; ++i) {
        if (array[i] != nullptr) {
            free(array[i]);
            array[i] = nullptr;
        }
    }

    free(array);
    *borrow_ids_ptr = nullptr;
}

virt_agent_ret_t ubse_mem_borrow_execute_msg_unpack(uint8_t *buffer, uint32_t len, mem_borrow_result_c *result)
{
    IPC_LOG_INFO << "ubse_mem_borrow_execute_msg_unpack start.";
    MemBorrowExecuteResultMsg msg(buffer, len);
    auto ret = msg.Deserialize();
    if (ret != VM_OK) {
        IPC_LOG_ERROR << "Failed to deserialize MemBorrowExecuteResultMsg. Error code: " << ret;
        return VA_ERROR_DESERIALIZE_FAILED;
    }
    *result = msg.GetBorrowResult();
    IPC_LOG_INFO << "ubse_mem_borrow_execute_msg_unpack end. " << "borrow_ids_size: " << result->borrow_ids_size
                 << ", present_numa_ids_size: " << result->present_numa_ids_size;

    return VA_SUCCESS;
}

virt_agent_ret_t ubse_mem_task_info_query_msg_unpack(uint8_t *buffer, uint32_t len, async_task_info_c *result)
{
    IPC_LOG_INFO << "UnpackTaskInfoFromResponse start.";
    MemTaskResultQueryMsg msg(buffer, len);
    VmResult ret = msg.Deserialize();
    if (ret != VM_OK) {
        IPC_LOG_ERROR << "Failed to deserialize MemTaskResultQueryMsg. Error code: " << ret;
        return VA_ERROR_DESERIALIZE_FAILED;
    }
    *result = msg.GetTaskResult();
    return VA_SUCCESS;
}

virt_agent_ret_t ubse_mem_migrate_strategy_msg_unpack(uint8_t *buffer, uint32_t len, MemMigrateStrategy* strategy)
{
    MemFragmentationMemMigrateStrategyOutputMsg msg{buffer, len};
    auto ret = msg.Deserialize();
    if (ret != 0) {
        IPC_LOG_ERROR << "Failed to deserialize MemFragmentationMemMigrateStrategyOutputMsg. Error code: " << ret;
        return VA_ERROR_DESERIALIZE_FAILED;
    }
    auto outputMsg = msg.GetOutputMsg();
    (*strategy).vmInfoListSize = outputMsg.vmInfoListSize;
    (*strategy).waitingTime = outputMsg.waitingTime;
    (*strategy).vmInfoList = new(std::nothrow) VmMigrateStrategy[outputMsg.vmInfoListSize];
    if ((*strategy).vmInfoList == nullptr) {
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    for (uint32_t i = 0; i < outputMsg.vmInfoListSize; i++) {
        (*strategy).vmInfoList[i].pid = outputMsg.vmInfoList[i].pid;
        (*strategy).vmInfoList[i].memSize = outputMsg.vmInfoList[i].memSize;
        (*strategy).vmInfoList[i].destNumaId = outputMsg.vmInfoList[i].destNumaId;
    }
    return VA_SUCCESS;
}