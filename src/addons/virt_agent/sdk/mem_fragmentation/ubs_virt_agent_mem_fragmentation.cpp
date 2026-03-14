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

#include "ubs_virt_agent_mem_fragmentation.h"

#include <securec.h>
#include <ubse_ipc_client.h>
#include <ubse_ipc_log.h>
#include "src/sdk/c/include/ubs_error.h"

#include "ubs_virt_agent_mem_fragmentation_helper.h"
#include "vm_sdk_def.h"

virt_agent_ret_t ubs_virt_agent_mem_fragmentation_node_info(numa_info_t **node_list, uint32_t *node_cnt)
{
    if (node_list == nullptr || node_cnt == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: node_list or node_cnt is nullptr or node_cnt is invalid.";
        return VA_ERROR_INVALID_PARAM;
    }
    ubse_api_buffer_t request_buffer = {nullptr, 0};
    ubse_api_buffer_t response_buffer = {nullptr, 0};
    uint32_t ret = ubse_invoke_call(UBS_VA_QUERY, UBS_VA_NUMA_INFO, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VA_ERROR_BASE;
    }
    ret = ubse_node_info_unpack(response_buffer.buffer, response_buffer.length, node_list, node_cnt);
    ubse_api_buffer_free(&response_buffer);
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "ubse_node_info_unpack failed with error code = " << ret;
        return VA_ERROR_DESERIALIZE_FAILED;
    }
    return VA_SUCCESS;
}

virt_agent_ret_t ubs_virt_agent_mem_fragmentation_vm_info(vm_domain_info_t **vm_info_list, uint32_t *vm_info_cnt)
{
    if (vm_info_list == nullptr || vm_info_cnt == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: vm_info_list or vm_info_cnt is nullptr or vm_info_cnt is invalid.";
        return VA_ERROR_INVALID_PARAM;
    }
    ubse_api_buffer_t request_buffer = {nullptr, 0};
    ubse_api_buffer_t response_buffer = {nullptr, 0};
    uint32_t ret = ubse_invoke_call(UBS_VA_QUERY, UBS_VA_VM_INFO, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VA_ERROR_BASE;
    }
    ret = ubse_vm_info_unpack(response_buffer.buffer, response_buffer.length, vm_info_list, vm_info_cnt);
    ubse_api_buffer_free(&response_buffer);
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "ubse_node_info_unpack failed with error code = " << ret;
        return VA_ERROR_DESERIALIZE_FAILED;
    }
    return VA_SUCCESS;
}

virt_agent_ret_t ubs_virt_agent_mem_fragmentation_node_anti_affinity(const NodeAntiDictionary *dict)
{
    if (dict == nullptr || dict->entry_count == 0 || dict->entry_count > MAX_NODE_NUM) {
        IPC_LOG_ERROR << "Invalid parameters: nodeAntiAffinityMapJson is nullptr or entry_count is out of range.";
        return VA_ERROR_INVALID_PARAM;
    }
    for (size_t i = 0; i < dict->entry_count; i++) {
        if (strnlen(dict->entries[i].key, VIRT_MEM_MAX_NODE_ID_LENGTH) >= VIRT_MEM_MAX_NODE_ID_LENGTH ||
            dict->entries[i].value_count > MAX_NODE_NUM) {
            IPC_LOG_ERROR << "Entries[" << i << "].key is nullptr or value is nullptr or key is out of range.";
            return VA_ERROR_INVALID_PARAM;
        }
        for (size_t j = 0; j < dict->entries[i].value_count; j++) {
            if (strnlen(dict->entries[i].value[j], VIRT_MEM_MAX_NODE_ID_LENGTH) >= VIRT_MEM_MAX_NODE_ID_LENGTH) {
                IPC_LOG_ERROR << "Entries[" << i << "].value[" << j << "] is nullptr.";
                return VA_ERROR_INVALID_PARAM;
            }
        }
    }

    const NodeAntiDictionary &node_dict = *dict;
    size_t buffer_size = sizeof(node_dict);
    uint8_t *buffer = allocate_memory(buffer_size);
    if (buffer == nullptr) {
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }

    virt_agent_ret_t res_code = serialize_data(node_dict, buffer);
    if (res_code != VA_SUCCESS) {
        SafeDeleteArray(buffer);
        return res_code;
    }

    ubse_api_buffer_t request_buffer = {buffer, static_cast<uint32_t>(buffer_size)};
    ubse_api_buffer_t response_buffer = {nullptr, 0};

    uint32_t ret =
        ubse_invoke_call(UBS_VA_MEM_FRAGMENTATION, UBS_VA_NODE_ANTI_AFFINITY, &request_buffer, &response_buffer);
    SafeDeleteArray(request_buffer.buffer);
    ubse_api_buffer_free(&response_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code = " << ret;
        return VA_ERROR_BASE;
    }

    return VA_SUCCESS;
}

virt_agent_ret_t ubs_virt_agent_mem_borrow_strategy(const src_memory_borrow_param *src_param,
                                                    borrow_strategy_c *borrow_strategy)
{
    IPC_LOG_INFO << "Start ubs_virt_agent_mem_borrow_strategy";
    if (src_param == nullptr || borrow_strategy == nullptr ||
        strnlen(src_param->src_nid, VIRT_MEM_MAX_NODE_ID_LENGTH) >= VIRT_MEM_MAX_NODE_ID_LENGTH) {
        IPC_LOG_ERROR << "Invalid parameters: param, mem_borrow_strategy or length is , or src_nid is invalid.";
        return VA_ERROR_INVALID_PARAM;
    }
    MemFragmentationMemBorrowStrategyInputMsg inputMsg{*src_param};
    auto ret = inputMsg.Serialize();
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "MemBorrowStrategyInputMsg Serialize failed.";
        return VA_ERROR_SERIALIZE_FAILED;
    }
    ubse_api_buffer_t request_buffer{inputMsg.SerializedData(), inputMsg.SerializedDataSize()};
    if (request_buffer.buffer == nullptr || request_buffer.length == 0) {
        IPC_LOG_ERROR << "Failed to allocate memory for request_buffer.buffer.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    ubse_api_buffer_t response_buffer = {nullptr, 0};
    ret = ubse_invoke_call(UBS_VA_MEM_FRAGMENTATION, UBS_VA_MEM_BORROW_STRATEGY, &request_buffer, &response_buffer);
    ubse_api_buffer_delete(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VA_ERROR_BASE;
    }
    ret = ubse_mem_borrow_strategy_msg_unpack(response_buffer.buffer, response_buffer.length, borrow_strategy);
    ubse_api_buffer_free(&response_buffer);
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "ubse_mem_borrow_strategy_unpack failed with error code = " << ret;
        return VA_ERROR_DESERIALIZE_FAILED;
    }
    IPC_LOG_INFO << "End ubs_virt_agent_mem_borrow_strategy";
    return VA_SUCCESS;
}

virt_agent_ret_t ubs_virt_agent_mem_borrow_execute(const borrow_setting_c *borrow_setting, mem_borrow_result_c *result)
{
    IPC_LOG_INFO << "Start ubs_virt_agent_mem_borrow_execute";
    if (borrow_setting == nullptr || borrow_setting->borrow_strategy.dest_numa_infos_size > MAX_DEST_PARAM_SIZE ||
        result == nullptr || result->borrow_ids_ptr == nullptr || result->present_numa_ids_ptr == nullptr ||
        strnlen(borrow_setting->borrow_strategy.src_host_id, VIRT_MEM_MAX_NODE_ID_LENGTH) >=
        VIRT_MEM_MAX_NODE_ID_LENGTH) {
        IPC_LOG_ERROR << "Invalid parameters: borrow_strategy or borrow_ids_size or present_numa_ids_size is null,"
                         "or dest_numa_infos_size is invalid, or borrow_ids_ptr is null or borrow_ids_ptr is null,"
                         "or src_host_id is invalid.";
        return VA_ERROR_INVALID_PARAM;
    }
    for (uint32_t i = 0; i < borrow_setting->borrow_strategy.dest_numa_infos_size; i++) {
        if (borrow_setting->borrow_strategy.dest_numa_infos[i].numa_nums > MAX_DEST_NUMA_NUM) {
            IPC_LOG_ERROR << "Invalid parameters: numa_nums in dest_numa_infos[" << i << "] is invalid.";
            return VA_ERROR_INVALID_PARAM;
        }
    }

    MemBorrowSettingMsg msg(*borrow_setting);
    auto res = msg.Serialize();
    if (res != VA_SUCCESS) {
        IPC_LOG_ERROR << "MemBorrowSettingMsg Serialize failed.";
        return VA_ERROR_SERIALIZE_FAILED;
    }
    ubse_api_buffer_t request_buffer{msg.SerializedData(), msg.SerializedDataSize()};
    if (request_buffer.buffer == nullptr || request_buffer.length == 0) {
        IPC_LOG_ERROR << "Failed to allocate memory for request_buffer.buffer.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    ubse_api_buffer_t response_buffer = {nullptr, 0};
    uint32_t ret =
        ubse_invoke_call(UBS_VA_MEM_FRAGMENTATION, UBS_VA_MEM_BORROW_EXECUTE, &request_buffer, &response_buffer);
    ubse_api_buffer_delete(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VA_ERROR_BASE;
    }
    ret = ubse_mem_borrow_execute_msg_unpack(response_buffer.buffer, response_buffer.length, result);
    ubse_api_buffer_free(&response_buffer);
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "ubse_mem_borrow_execute_unpack failed with error code = " << ret;
        return VA_ERROR_DESERIALIZE_FAILED;
    }
    IPC_LOG_INFO << "End ubs_virt_agent_mem_borrow_execute";
    return VA_SUCCESS;
}

virt_agent_ret_t ubs_virt_agent_mem_migrate_strategy(const MemMigrateStrategySrcParam *srcParam,
                                                     MemMigrateStrategy *strategy)
{
    IPC_LOG_INFO << "Start ubs_virt_agent_mem_migrate_strategy";
    if (srcParam == nullptr || strategy == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: srcParam is nullptr or strategy is nullptr.";
        return VA_ERROR_INVALID_PARAM;
    }
    if ((*srcParam).vmInfoListSize > MAX_VM_NUM || (*srcParam).vmInfoListSize == 0 ||
        strnlen((*srcParam).borrowInNode, VIRT_MEM_MAX_NODE_ID_LENGTH) >= VIRT_MEM_MAX_NODE_ID_LENGTH) {
        IPC_LOG_ERROR << "Invalid parameters: vmInfoListSize or the size of borrowInNode is InValid.";
        return VA_ERROR_INVALID_PARAM;
    }
    MemFragmentationMemMigrateStrategyInputMsg inputMsg{};
    auto ret = inputMsg.SetInputMsg(*srcParam);
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "MemMigrateStrategyInputMsg SetInputMsg failed.";
        return VA_ERROR_BASE;
    }
    ret = inputMsg.Serialize();
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "MemMigrateStrategyInputMsg Serialize failed.";
        return VA_ERROR_SERIALIZE_FAILED;
    }
    ubse_api_buffer_t request_buffer{inputMsg.SerializedData(), inputMsg.SerializedDataSize()};
    if (request_buffer.buffer == nullptr || request_buffer.length == 0) {
        IPC_LOG_ERROR << "Failed to allocate memory for request_buffer.buffer.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    ubse_api_buffer_t response_buffer = {nullptr, 0};
    ret = ubse_invoke_call(UBS_VA_MEM_FRAGMENTATION, UBS_VA_MEM_MIGRATE_STRATEGY, &request_buffer, &response_buffer);
    ubse_api_buffer_delete(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VA_ERROR_BASE;
    }
    ret = ubse_mem_migrate_strategy_msg_unpack(response_buffer.buffer, response_buffer.length, strategy);
    ubse_api_buffer_free(&response_buffer);
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "ubse_mem_migrate_strategy_unpack failed with error code = " << ret;
        return VA_ERROR_BASE;
    }
    IPC_LOG_INFO << "End ubs_virt_agent_mem_migrate_strategy";
    return VA_SUCCESS;
}

static bool UnpackTaskIdFromResponse(const ubse_api_buffer_t &response_buffer, char **task_id, uint32_t *task_id_len)
{
    if (response_buffer.buffer == nullptr || response_buffer.length == 0) {
        IPC_LOG_ERROR << "Invalid response buffer: buffer is null or length is 0";
        return false;
    }

    const char *response_str = reinterpret_cast<const char *>(response_buffer.buffer);
    size_t response_str_len = strnlen(response_str, response_buffer.length);
    if (response_str_len == 0) {
        IPC_LOG_ERROR << "Response buffer contains empty string";
        return false;
    }

    *task_id = new (std::nothrow) char[response_str_len + 1];
    if (*task_id == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for task_id, length = " << response_str_len;
        return false;
    }

    errno_t result = memcpy_s(*task_id, response_str_len + 1, response_str, response_str_len);
    if (result != 0) {
        IPC_LOG_ERROR << "memcpy_s failed for task_id: " << result;
        delete[] *task_id;
        *task_id = nullptr;
        return false;
    }

    (*task_id)[response_str_len] = '\0';
    *task_id_len = static_cast<uint32_t>(response_str_len);
    IPC_LOG_INFO << "Task ID parsed: " << *task_id << ", length = " << response_str_len;
    return true;
}

virt_agent_ret_t ubs_virt_agent_mem_return(bool isAsync, char **task_id, uint32_t *task_id_len)
{
    if (task_id == nullptr) {
        IPC_LOG_ERROR << "task_id is null";
        return VA_ERROR_INVALID_PARAM;
    }

    if (task_id_len == nullptr) {
        IPC_LOG_ERROR << "task_id_len is null";
        return VA_ERROR_INVALID_PARAM;
    }
    ubse_api_buffer_t request_buffer = {nullptr, 0};

    request_buffer.length = sizeof(bool);
    request_buffer.buffer = new (std::nothrow) uint8_t[request_buffer.length];
    if (request_buffer.buffer == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for request_buffer.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }

    bool async_flag = isAsync;
    errno_t copy_result = memcpy_s(request_buffer.buffer, request_buffer.length, &async_flag, sizeof(bool));
    if (copy_result != 0) {
        IPC_LOG_ERROR << "memcpy_s failed with error code = " << copy_result;
        ubse_api_buffer_delete(&request_buffer);
        return VA_ERROR_MEM_COPY_FAILED;
    }

    ubse_api_buffer_t response_buffer = {nullptr, 0};
    uint32_t ret = ubse_invoke_call(UBS_VA_MEM_FRAGMENTATION, UBS_VA_MEM_RETURN, &request_buffer, &response_buffer);
    ubse_api_buffer_delete(&request_buffer);

    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VA_ERROR_BASE;
    }

    if (!UnpackTaskIdFromResponse(response_buffer, task_id, task_id_len)) {
        IPC_LOG_ERROR << "UnpackTaskIdFromResponse failed.";
        ubse_api_buffer_free(&response_buffer);
        return VA_ERROR_BASE;
    }

    ubse_api_buffer_free(&response_buffer);
    return VA_SUCCESS;
}

virt_agent_ret_t ubs_virt_agent_sync_task_query(char *task_id, uint32_t task_id_len, async_task_info_c *result)
{
    if (task_id == nullptr || result == nullptr || task_id_len == 0) {
        IPC_LOG_ERROR << "Invalid input parameters: task_id or result is null";
        return VA_ERROR_INVALID_PARAM;
    }

    ubse_api_buffer_t request_buffer = {nullptr, 0};
    request_buffer.length = task_id_len;
    request_buffer.buffer = new (std::nothrow) uint8_t[request_buffer.length];
    if (request_buffer.buffer == nullptr) {
        IPC_LOG_ERROR << "Failed to allocate memory for request_buffer.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }

    errno_t copy_result = memcpy_s(request_buffer.buffer, request_buffer.length, task_id, task_id_len);
    if (copy_result != 0) {
        IPC_LOG_ERROR << "memcpy_s failed with error code = " << copy_result;
        ubse_api_buffer_delete(&request_buffer);
        return VA_ERROR_MEM_COPY_FAILED;
    }

    ubse_api_buffer_t response_buffer = {nullptr, 0};
    uint32_t ret =
        ubse_invoke_call(UBS_VA_MEM_FRAGMENTATION, UBS_VA_SYNC_TASK_QUERY, &request_buffer, &response_buffer);
    ubse_api_buffer_delete(&request_buffer);

    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VA_ERROR_BASE;
    }

    if (ubse_mem_task_info_query_msg_unpack(response_buffer.buffer, response_buffer.length, result) != VA_SUCCESS) {
        IPC_LOG_ERROR << "ubse_mem_task_info_query_msg_unpack failed.";
        ubse_api_buffer_free(&response_buffer);
        return VA_ERROR_BASE;
    }

    ubse_api_buffer_free(&response_buffer);
    IPC_LOG_INFO << "Sync task query succeeded for task_id: " << task_id;
    return VA_SUCCESS;
}

virt_agent_ret_t ubs_virt_agent_mem_migrate_execute(const MemMigrateExecuteSrcParam *srcParam)
{
    IPC_LOG_INFO << "Start ubs_virt_agent_mem_migrate_execute";
    if (srcParam == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: srcParam is null.";
        return VA_ERROR_INVALID_PARAM;
    }
    if ((*srcParam).vmInfoListSize > MAX_VM_NUM || (*srcParam).vmInfoListSize == 0 ||
        (*srcParam).borrowIdsCount > MAX_BORROW_ID_COUNT ||
        strnlen((*srcParam).borrowInNode, VIRT_MEM_MAX_NODE_ID_LENGTH) >= VIRT_MEM_MAX_NODE_ID_LENGTH) {
        IPC_LOG_ERROR << "Invalid parameters: vmInfoListSize or borrowIdsCount or the size of borrowInNode is InValid.";
        return VA_ERROR_INVALID_PARAM;
    }
    for (uint32_t i = 0; i < (*srcParam).borrowIdsCount; i++) {
        if (strnlen((*srcParam).borrowIds[i], MAX_BORROW_ID_LENGTH) >= MAX_BORROW_ID_LENGTH) {
            IPC_LOG_ERROR << "Invalid parameters: borrowIds[" <<  i << "] is invalid.";
            return VA_ERROR_INVALID_PARAM;
        }
    }
    MemFragmentationMemMigrateExecuteInputMsg inputMsg{};
    auto ret = inputMsg.SetInputMsg(*srcParam);
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "MemMigrateExecuteInputMsg SetInputMsg failed.";
        return VA_ERROR_BASE;
    }
    ret = inputMsg.Serialize();
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "MemMigrateExecuteInputMsg Serialize failed.";
        return VA_ERROR_SERIALIZE_FAILED;
    }
    ubse_api_buffer_t request_buffer{inputMsg.SerializedData(), inputMsg.SerializedDataSize()};
    if (request_buffer.buffer == nullptr || request_buffer.length == 0) {
        IPC_LOG_ERROR << "Failed to allocate memory for request_buffer.buffer.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    ubse_api_buffer_t  response_buffer = {nullptr, 0};
    ret = ubse_invoke_call(UBS_VA_MEM_FRAGMENTATION, UBS_VA_MEM_MIGRATE_EXECUTE, &request_buffer,
                           &response_buffer);
    ubse_api_buffer_delete(&request_buffer);
    ubse_api_buffer_free(&response_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code = " << ret;
        return VA_ERROR_BASE;
    }
    IPC_LOG_INFO << "End ubs_virt_agent_mem_migrate_execute";
    return VA_SUCCESS;
}

virt_agent_ret_t ubs_virt_agent_mem_rollback(const RollbackSrcParam *srcParam)
{
    if (srcParam == nullptr || strnlen(srcParam->node_id, VIRT_MEM_MAX_NODE_ID_LENGTH) >= VIRT_MEM_MAX_NODE_ID_LENGTH ||
        srcParam->borrow_id_size == 0 || srcParam->borrow_id_size > MAX_BORROW_ID_COUNT) {
        IPC_LOG_ERROR << "Invalid parameters: node_id, borrow_id_list or borrow_id_size is invalid.";
        return VA_ERROR_INVALID_PARAM;
    }
    for (uint32_t i = 0; i < srcParam->borrow_id_size; i++) {
        if (strnlen(srcParam->borrow_id_list[i], MAX_BORROW_ID_LENGTH) >= MAX_BORROW_ID_LENGTH) {
            IPC_LOG_ERROR << "Invalid parameters: borrow_id_list[" <<  i << "] is invalid.";
            return VA_ERROR_INVALID_PARAM;
        }
    }
    RollbackParams rollbackParams(srcParam);
    MemRollbackMsg rollbackMsg(rollbackParams);

    auto ret = rollbackMsg.Serialize();
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "MemRollbackMsg Serialize failed.";
        return VA_ERROR_SERIALIZE_FAILED;
    }

    ubse_api_buffer_t requestBuffer = {rollbackMsg.SerializedData(), rollbackMsg.SerializedDataSize()};

    ubse_api_buffer_t responseBuffer = {nullptr, 0};
    uint32_t invokeRet = ubse_invoke_call(UBS_VA_MEM_FRAGMENTATION, UBS_VA_MEM_ROLLBACK,
                                          &requestBuffer, &responseBuffer);
    ubse_api_buffer_delete(&requestBuffer);
    ubse_api_buffer_free(&responseBuffer);
    if (invokeRet != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code = " << invokeRet;
        return VA_ERROR_BASE;
    }

    return VA_SUCCESS;
}