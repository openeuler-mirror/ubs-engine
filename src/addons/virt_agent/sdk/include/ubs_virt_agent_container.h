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

#ifndef UBS_VIRT_AGENT_CONTAINER_H
#define UBS_VIRT_AGENT_CONTAINER_H

#include <sys/types.h>
#include "ubs_virt_agent_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char srcNid[SDK_NO_16];
    pid_t pids[SDK_NO_2048];
    size_t pids_size;
} pid_param;

typedef struct {
    pid_t pid;
    uint64_t localUsedMem;
    uint16_t localNumaIds[SDK_NO_64];
    size_t localNumaCount;
    uint64_t remoteUsedMem;
} pid_mem_info;

typedef struct {
    uint16_t highWaterMark;
    uint16_t lowWaterMark;
} watermark_t;

typedef struct {
    char containerId[SDK_NO_128][SDK_NO_128];
    size_t containerIdSize;
} container_id_list;

typedef struct {
    pid_t pids[SDK_NO_2048];
    size_t pidsCount;
    char *containerId;
} container_pid_info;

// mem borrow
typedef struct {
    int socketId;
    int numaId;
} src_location_t;

typedef struct {
    char srcNid[SDK_NO_16];
    src_location_t srcLocations[SDK_NO_16];
    size_t srcLocationsSize;
} borrow_param_t;

typedef struct {
    borrow_param_t borrowParam;
    uint64_t borrowSizes[SDK_NO_64];
    size_t borrowSizesSize;
    watermark_t waterMark;
} mem_borrow_request_t;

// mem migrate
typedef struct {
    pid_t pid;
    int ratio;
} container_param_t;

typedef struct {
    borrow_param_t borrowParam;
    char borrowIds[SDK_NO_64][SDK_NO_128];
    size_t borrowIdsSize;
    container_param_t containerParam[SDK_NO_64];
    size_t containerParamSize;
} mem_migrate_request_t;

// mem return
typedef struct {
    borrow_param_t borrowParam;
    char borrowIds[SDK_NO_64][SDK_NO_128];
    size_t borrowIdsSize;
    pid_t pids[SDK_NO_64];
    size_t pidsSize;
} return_request_t;

/**
 * @brief  query container memory information for given PIDs
 * @param param     [IN]  input parameters including source node and PID list
 * @param pidInfos  [OUT] array of PID memory information returned by the interface
 * @param InfoSize  [OUT] number of PID memory info entries returned
 * @return 0 for success, non-zero for error
 */
int32_t ubs_container_info_query(pid_param *param, pid_mem_info **pidInfos, uint32_t *InfoSize);

/**
 * @brief  inject or update container memory watermark (waterLine)
 * @param param [IN] input parameters containing watermark information
 * @return 0 for success, non-zero for error
 */
int32_t ubs_container_inject_waterLine(watermark_t *param);

/**
 * @brief  get PIDs of containers from a given list of container IDs
 * @param containerIdList [IN]  list of container IDs
 * @param pidInfo         [OUT] array of PID information returned by the interface
 * @param InfoSize        [OUT] number of PID information entries returned
 * @return 0 for success, non-zero for error
 */
int32_t ubs_container_get_container_pids(container_id_list *containerIdList, container_pid_info **param,
                                         uint32_t *InfoSize);

/**
 * @brief  perform memory borrow operation based on waterline strategy
 * @param memBorrowRequest [IN]  memory borrow request parameters including source node and requested memory info
 * @param borrowIds        [OUT] array of borrowed memory IDs returned by the interface
 * @param idsSize          [OUT] number of borrowed memory IDs returned
 * @return 0 for success, non-zero for error
 */
int32_t ubs_virt_agent_waterline_mem_borrow(mem_borrow_request_t *memBorrowRequest, char ***borrowIds,
                                            uint32_t *idsSize);

/**
 * @brief  perform memory migrate operation based on waterline strategy
 * @param memMigrateRequest [IN] memory migrate request parameters including source node and migration details
 * @return 0 for success, non-zero for error
 */
int32_t ubs_virt_agent_waterline_mem_migrate(mem_migrate_request_t *memMigrateRequest);

/**
 * @brief  return previously borrowed memory based on waterline strategy
 * @param memReturnRequest [IN] memory return request parameters including source node and borrowed memory IDs
 * @return 0 for success, non-zero for error
 */
int32_t ubs_virt_agent_waterline_mem_return(return_request_t *returnRequest);

#ifdef __cplusplus
}
#endif
#endif // UBS_VIRT_AGENT_CONTAINER_H
