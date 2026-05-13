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

#include "ubs_virt_agent_container.h"
#include <ubse_ipc_client.h>
#include <ubse_ipc_log.h>
#include "mem_container_msg.h"
#include "src/sdk/c/include/ubs_error.h"
#include "vm_sdk_def.h"

using namespace vm;

// Conversion from PidParamForGoSDK to PidParamForGo
void convertPidParamFromGoSDKToC(pid_param& pidParamForGoSdk, pid_param_fo_c& pidParamForC)
{
    pidParamForC = *reinterpret_cast<pid_param_fo_c*>(&pidParamForGoSdk);
}

// Conversion from ContainerIdListForGoSDK to ContainerIdListForC
void convertContainerIdListFromGoSDKToC(container_id_list& containerIdListForGoSDK,
                                        container_id_list_for_c& containerIdListForC)
{
    containerIdListForC = *reinterpret_cast<container_id_list_for_c*>(&containerIdListForGoSDK);
}

// Conversion from container_pid_info_for_c to container_pid_info_for_go_sdk
void convertContainerIdInfoFromCToGoSDK(container_pid_info_for_c& containerIdInfoForC,
                                        container_pid_info& containerIdInfoForGoSDK)
{
    containerIdInfoForGoSDK = *reinterpret_cast<container_pid_info*>(&containerIdInfoForC);
}

// Conversion from WaterMarkForGoSDK to WaterMarkForC
void convertWaterMarkForGoSDKToC(watermark_t& waterMarkForGoSDK, WaterMark& waterMarkForC)
{
    waterMarkForC = *reinterpret_cast<WaterMark*>(&waterMarkForGoSDK);
}

// Conversion from PidMemInfoForGo to PidMemInfoForGoSDK
void convertPidMemInfoFromCToGoSDK(pid_mem_info_for_c& pidMemInfoForGo, pid_mem_info& pidMemInfoForGoSDK)
{
    pidMemInfoForGoSDK = *reinterpret_cast<pid_mem_info*>(&pidMemInfoForGo);
}

int32_t ubse_output_unpack(uint8_t* buffer, uint32_t len, pid_mem_info** pidInfos, uint32_t* InfoSize)
{
    MemContainerPidMemInfoOutputMsg msg{buffer, len};
    auto ret = msg.Deserialize();
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "Failed to deserialize MemContainerPidMemInfoOutputMsg. Error code: " << ret;
        return VA_ERROR_DESERIALIZE_FAILED;
    }

    std::vector<pid_mem_info_for_c> pidMemInfoList = msg.GetPidInfos();
    *InfoSize = static_cast<uint32_t>(pidMemInfoList.size());
    if (*InfoSize == 0) {
        IPC_LOG_WARN << "Memory allocation *InfoSize == 0.";
        return VA_SUCCESS;
    }
    *pidInfos = (pid_mem_info*)calloc(*InfoSize, sizeof(pid_mem_info_for_c));
    if (*pidInfos == nullptr) {
        IPC_LOG_ERROR << "Memory allocation failed for pid_mem_infos.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    for (uint32_t i = 0; i < *InfoSize; ++i) {
        convertPidMemInfoFromCToGoSDK(pidMemInfoList[i], (*pidInfos)[i]);
        (*pidInfos)[i].localNumaCount = pidMemInfoList[i].localNumaCount;
    }
    IPC_LOG_INFO << "ubse_output_unpack success.";
    return VA_SUCCESS;
}

int32_t ubse_output_unpack_for_containerInfos(uint8_t* buffer, uint32_t len, container_pid_info** containerInfos,
                                              uint32_t* InfoSize)
{
    ContainerPidsForCInputMsg msg{buffer, len};
    auto ret = msg.Deserialize();
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "Failed to deserialize MemContainerPidMemInfoOutputMsg. Error code: " << ret;
        return VA_ERROR_DESERIALIZE_FAILED;
    }

    std::vector<container_pid_info_for_c> containerPidInfoC = msg.GetContainerPidInfos();
    *InfoSize = static_cast<uint32_t>(containerPidInfoC.size());
    if (*InfoSize == 0) {
        IPC_LOG_WARN << "Memory allocation *InfoSize == 0.";
        return VA_SUCCESS;
    }
    *containerInfos = (container_pid_info*)calloc(*InfoSize, sizeof(container_pid_info_for_c));
    if (*containerInfos == nullptr) {
        IPC_LOG_ERROR << "Memory allocation failed for pid_mem_infos.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    for (uint32_t i = 0; i < *InfoSize; ++i) {
        convertContainerIdInfoFromCToGoSDK(containerPidInfoC[i], (*containerInfos)[i]);
        (*containerInfos)[i].pidsCount = containerPidInfoC[i].pidsCount;
    }
    IPC_LOG_INFO << "ubse_output_unpack_for_containerInfos success.";
    return VA_SUCCESS;
}

int32_t ubs_container_info_query(pid_param* param, pid_mem_info** pidInfos, uint32_t* InfoSize)
{
    if (!param || strnlen(param->srcNid, SDK_NO_16) == 0 || strnlen(param->srcNid, SDK_NO_16) >= SDK_NO_16 ||
        param->pids_size == 0 || param->pids_size > SDK_NO_2048 || !pidInfos || !InfoSize) {
        IPC_LOG_ERROR << "Invalid parameters: param is null or srcNid is empty or pids_size is invalid or "
                         "InfoSize is null.";
        return VM_ERROR;
    }
    pid_param_fo_c pidParamForC;
    convertPidParamFromGoSDKToC(*param, pidParamForC);
    MemContainerPidMemInfoInputMsg inputMsg;
    inputMsg.SetInputInfos(pidParamForC);
    auto ret = inputMsg.Serialize();
    if (ret != VM_OK) {
        IPC_LOG_ERROR << "Input param serialize failed.";
        return VM_ERROR;
    }
    ubse_api_buffer_t request_buffer = {.buffer = inputMsg.SerializedData(), .length = inputMsg.SerializedDataSize()};

    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    ret = ubse_invoke_call(UBS_VA_CONTAINER, UBS_VA_CONTAINER_GET_MEM_INFO_FOR_PID, &request_buffer, &response_buffer);
    ubse_api_buffer_delete(&request_buffer);
    if (ret != VM_OK) {
        IPC_LOG_ERROR << "UBS_VA_CONTAINER ubse_invoke_call failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VM_ERROR;
    }

    ret = ubse_output_unpack(response_buffer.buffer, response_buffer.length, pidInfos, InfoSize);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_output_unpack failed with error code = " << ret;
    }

    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t ubs_container_inject_waterLine(watermark_t* param)
{
    if (!param) {
        IPC_LOG_ERROR << "Invalid parameters: param is null or srcNid is empty or pids is empty";
        return VM_ERROR;
    }
    WaterMark waterMarkForC;
    convertWaterMarkForGoSDKToC(*param, waterMarkForC);
    UpdateWaterLineForCInputMsg inputMsg;
    inputMsg.SetInputInfos(waterMarkForC);
    auto ret = inputMsg.Serialize();
    if (ret != VM_OK) {
        IPC_LOG_ERROR << "Input param serialize failed.";
        return VM_ERROR;
    }
    ubse_api_buffer_t request_buffer = {.buffer = inputMsg.SerializedData(), .length = inputMsg.SerializedDataSize()};

    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    ret = ubse_invoke_call(UBS_VA_CONTAINER, UBS_VA_CONTAINER_UPDATE_WATERLINE, &request_buffer, &response_buffer);
    ubse_api_buffer_delete(&request_buffer);
    if (ret != VM_OK) {
        IPC_LOG_ERROR << "UBS_VA_CONTAINER ubse_invoke_call failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VM_ERROR;
    }
    ubse_api_buffer_free(&response_buffer);
    return VM_OK;
}

int32_t ubs_container_get_container_pids(container_id_list* containerIdList, container_pid_info** pidInfo,
                                         uint32_t* InfoSize)
{
    if (!containerIdList || !pidInfo || !InfoSize || (*containerIdList).containerIdSize > SDK_NO_128) {
        IPC_LOG_ERROR << "Invalid parameters: param is null or InfoSize is null or containerIdSize is invalid.";
        return VM_ERROR;
    }
    for (size_t i = 0; i < (*containerIdList).containerIdSize; i++) {
        if (strnlen((*containerIdList).containerId[i], SDK_NO_128) >= SDK_NO_128) {
            IPC_LOG_ERROR << "The containerId " << i << " is invalid.";
            return VM_INVALID_PARAM_ERROR;
        }
    }
    container_id_list_for_c containerIdListForC;
    convertContainerIdListFromGoSDKToC(*containerIdList, containerIdListForC);
    ContainerIdListForCInputMsg inputMsg;
    inputMsg.SetInputInfos(containerIdListForC);
    auto ret = inputMsg.Serialize();
    if (ret != VM_OK) {
        IPC_LOG_ERROR << "Input param serialize failed.";
        return VM_ERROR;
    }
    ubse_api_buffer_t request_buffer = {.buffer = inputMsg.SerializedData(), .length = inputMsg.SerializedDataSize()};

    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    ret = ubse_invoke_call(UBS_VA_CONTAINER, UBS_VA_CONTAINER_GET_CONTAINER_PIDS, &request_buffer, &response_buffer);
    ubse_api_buffer_delete(&request_buffer);
    if (ret != VM_OK) {
        IPC_LOG_ERROR << "UBS_VA_CONTAINER ubse_invoke_call failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VM_ERROR;
    }

    ret = ubse_output_unpack_for_containerInfos(response_buffer.buffer, response_buffer.length, pidInfo, InfoSize);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_output_unpack failed with error code = " << ret;
    }

    ubse_api_buffer_free(&response_buffer);
    return ret;
}

int32_t waterline_mem_borrow_output_unpack(uint8_t* buffer, uint32_t len, char*** borrowIds, uint32_t* idsSize)
{
    MemContainerWaterLineMemBorrowOutputMsg outputMsg{buffer, len};
    auto ret = outputMsg.Deserialize();
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "Output param deserialize failed.";
        return VM_ERROR;
    }

    std::vector<std::string> borrowIdList = outputMsg.GetBorrowIds();
    *idsSize = static_cast<uint32_t>(borrowIdList.size());
    if (*idsSize == 0) {
        IPC_LOG_ERROR << "Memory allocation *idsSize == 0.";
        return VA_SUCCESS;
    }
    *borrowIds = (char**)calloc(*idsSize, sizeof(char*));
    if (*borrowIds == nullptr) {
        IPC_LOG_ERROR << "Memory allocation failed.";
        return VA_ERROR_MEM_ALLOCATE_FAILED;
    }
    // Allocate memory for each string and copy the strings
    for (uint32_t i = 0; i < *idsSize; ++i) {
        size_t strLen = borrowIdList[i].size() + 1; // +1 for null terminator
        (*borrowIds)[i] = (char*)calloc(strLen, sizeof(char));
        if ((*borrowIds)[i] == nullptr) {
            // Free previously allocated memory if allocation fails
            for (uint32_t j = 0; j < i; ++j) {
                free((*borrowIds)[j]);
                (*borrowIds)[j] = nullptr;
            }
            free(*borrowIds);
            *borrowIds = nullptr;
            IPC_LOG_ERROR << "Memory allocation failed for string " << i;
            return VA_ERROR_MEM_ALLOCATE_FAILED;
        }
        ret = strcpy_s((*borrowIds)[i], strLen, borrowIdList[i].c_str());
        if (ret != EOK) {
            // Free previously allocated memory if strcpy_s fails
            for (uint32_t j = 0; j <= i; ++j) {
                free((*borrowIds)[j]);
                (*borrowIds)[j] = nullptr;
            }
            free(*borrowIds);
            *borrowIds = nullptr;
            IPC_LOG_ERROR << "strcpy_s failed with error code = " << ret;
            return VA_ERROR_MEM_COPY_FAILED;
        }
    }

    IPC_LOG_INFO << "Output unpack success.";
    return VA_SUCCESS;
}

void convert_mem_borrow_request_t_to_c(mem_borrow_request_t& memBorrowRequest_t, MemBorrowRequestC& memBorrowRequestC)
{
    memBorrowRequestC = *reinterpret_cast<MemBorrowRequestC*>(&memBorrowRequest_t);
}

int32_t ubs_virt_agent_waterline_mem_borrow(mem_borrow_request_t* memBorrowRequest, char*** borrowIds,
                                            uint32_t* idsSize)
{
    if (!memBorrowRequest || strnlen(memBorrowRequest->borrowParam.srcNid, SDK_NO_16) == 0 ||
        strnlen(memBorrowRequest->borrowParam.srcNid, SDK_NO_16) >= SDK_NO_16 || !borrowIds || !idsSize) {
        IPC_LOG_ERROR << "Invalid parameters: param is invalid.";
        return VM_ERROR;
    }
    MemBorrowRequestC memBorrowRequestC;
    convert_mem_borrow_request_t_to_c(*memBorrowRequest, memBorrowRequestC);
    MemContainerWaterLineMemBorrowInputMsg inputMsg{memBorrowRequestC};
    auto ret = inputMsg.Serialize();
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "Input param serialize failed.";
        return VM_ERROR;
    }
    ubse_api_buffer_t request_buffer = {.buffer = inputMsg.SerializedData(), .length = inputMsg.SerializedDataSize()};

    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    ret = ubse_invoke_call(UBS_VA_CONTAINER, UBS_VA_CONTAINER_WATERLINE_MEM_BORROW, &request_buffer, &response_buffer);
    ubse_api_buffer_delete(&request_buffer);
    if (ret != VM_OK) {
        IPC_LOG_ERROR << "UBS_VA_CONTAINER ubse_invoke_call failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VM_ERROR;
    }

    ret = waterline_mem_borrow_output_unpack(response_buffer.buffer, response_buffer.length, borrowIds, idsSize);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Output unpack failed with error code = " << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return ret;
}

void convert_mem_migrate_request_t_to_c(mem_migrate_request_t& memMigrateRequest_t,
                                        MemMigrateRequestC& memMigrateRequestC)
{
    memMigrateRequestC = *reinterpret_cast<MemMigrateRequestC*>(&memMigrateRequest_t);
}

int32_t ubs_virt_agent_waterline_mem_migrate(mem_migrate_request_t* memMigrateRequest)
{
    if (!memMigrateRequest || strnlen(memMigrateRequest->borrowParam.srcNid, SDK_NO_16) == 0 ||
        strnlen(memMigrateRequest->borrowParam.srcNid, SDK_NO_16) >= SDK_NO_16 ||
        memMigrateRequest->borrowIdsSize > SDK_NO_64) {
        IPC_LOG_ERROR << "Invalid parameters: param is invalid";
        return VM_ERROR;
    }
    for (size_t i = 0; i < memMigrateRequest->borrowIdsSize; i++) {
        if (strnlen(memMigrateRequest->borrowIds[i], SDK_NO_128) >= SDK_NO_128) {
            IPC_LOG_ERROR << "The borrowIds " << i << " is invalid.";
            return VM_INVALID_PARAM_ERROR;
        }
    }
    MemMigrateRequestC memMigrateRequestC;
    convert_mem_migrate_request_t_to_c(*memMigrateRequest, memMigrateRequestC);
    MemContainerWaterLineMemMigrateInputMsg inputMsg{memMigrateRequestC};
    auto ret = inputMsg.Serialize();
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "Input param serialize failed.";
        return VM_ERROR;
    }
    ubse_api_buffer_t request_buffer = {.buffer = inputMsg.SerializedData(), .length = inputMsg.SerializedDataSize()};

    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    ret = ubse_invoke_call(UBS_VA_CONTAINER, UBS_VA_CONTAINER_WATERLINE_MEM_MIGRATE, &request_buffer, &response_buffer);
    ubse_api_buffer_delete(&request_buffer);
    if (ret != VM_OK) {
        IPC_LOG_ERROR << "UBS_VA_CONTAINER ubse_invoke_call failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VM_ERROR;
    }

    ubse_api_buffer_free(&response_buffer);
    return ret;
}

void convert_mem_return_request_t_to_c(return_request_t& memReturnRequest_t, MemReturnRequestC& memReturnRequestC)
{
    memReturnRequestC = *reinterpret_cast<MemReturnRequestC*>(&memReturnRequest_t);
}

int32_t ubs_virt_agent_waterline_mem_return(return_request_t* memReturnRequest)
{
    if (!memReturnRequest || strnlen(memReturnRequest->borrowParam.srcNid, SDK_NO_16) == 0 ||
        strnlen(memReturnRequest->borrowParam.srcNid, SDK_NO_16) >= SDK_NO_16 ||
        memReturnRequest->borrowIdsSize > SDK_NO_64) {
        IPC_LOG_ERROR << "Invalid parameters: param is null";
        return VM_ERROR;
    }
    for (size_t i = 0; i < memReturnRequest->borrowIdsSize; i++) {
        if (strnlen(memReturnRequest->borrowIds[i], SDK_NO_128) >= SDK_NO_128) {
            IPC_LOG_ERROR << "The borrowIds " << i << " is invalid.";
            return VM_INVALID_PARAM_ERROR;
        }
    }
    MemReturnRequestC memReturnRequestC;
    convert_mem_return_request_t_to_c(*memReturnRequest, memReturnRequestC);
    MemContainerWaterLineMemReturnInputMsg inputMsg{memReturnRequestC};
    auto ret = inputMsg.Serialize();
    if (ret != VA_SUCCESS) {
        IPC_LOG_ERROR << "Input param serialize failed.";
        return VM_ERROR;
    }
    ubse_api_buffer_t request_buffer = {.buffer = inputMsg.SerializedData(), .length = inputMsg.SerializedDataSize()};

    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    ret = ubse_invoke_call(UBS_VA_CONTAINER, UBS_VA_CONTAINER_WATERLINE_MEM_RETURN, &request_buffer, &response_buffer);
    ubse_api_buffer_delete(&request_buffer);
    if (ret != VM_OK) {
        IPC_LOG_ERROR << "UBS_VA_CONTAINER ubse_invoke_call failed with error code = " << ret;
        ubse_api_buffer_free(&response_buffer);
        return VM_ERROR;
    }

    ubse_api_buffer_free(&response_buffer);
    return ret;
}