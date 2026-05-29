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

#include "ubs_engine_topo.h"

#include <securec.h>
#include <cstring>

#include "ubse_ipc_client.h"
#include "ubse_ipc_common.h"
#include "ubse_ipc_log.h"
#include "libubse_helper.h"
#include "ubs_error.h"

int32_t ubs_topo_node_local_get(ubs_topo_node_t* node)
{
    // 参数校验
    if (node == nullptr) {
        IPC_LOG_ERROR << "Node pointer is null";
        return UBS_ERR_NULL_POINTER;
    }
    // 打包
    ubse_api_buffer_t request_buffer = {nullptr, 0};
    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    auto ret = ubse_invoke_call(UBSE_NODE, UBSE_NODE_GET, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_node_unpack(response_buffer.buffer, response_buffer.length, node);
    ubse_api_buffer_free(&response_buffer);
    return static_cast<int32_t>(ret);
}

int32_t ubs_topo_node_list(ubs_topo_node_t** node_list, uint32_t* node_cnt)
{
    // 参数校验
    if (node_list == nullptr || node_cnt == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: node_list or node_cnt is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    // 打包
    ubse_api_buffer_t request_buffer = {nullptr, 0};
    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    auto ret = ubse_invoke_call(UBSE_NODE, UBSE_NODE_LIST, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error code: " << ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_node_list_unpack(response_buffer.buffer, response_buffer.length, node_list, node_cnt);
    ubse_api_buffer_free(&response_buffer);
    return static_cast<int32_t>(ret);
}

int32_t ubs_topo_link_list(ubs_topo_link_t** cpu_links, uint32_t* cpu_link_cnt)
{
    // 参数校验
    if (cpu_links == nullptr || cpu_link_cnt == nullptr) {
        IPC_LOG_ERROR << "Invalid parameters: cpu_links or cpu_link_cnt is nullptr";
        return UBS_ERR_NULL_POINTER;
    }
    // 打包
    ubse_api_buffer_t request_buffer = {nullptr, 0};
    // 调用接口
    ubse_api_buffer_t response_buffer = {.buffer = nullptr, .length = 0};
    auto ret = ubse_invoke_call(UBSE_NODE, UBSE_NODE_CPU_TOPO_LIST, &request_buffer, &response_buffer);
    ubse_api_buffer_free(&request_buffer);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "ubse_invoke_call failed with error=" << ret;
        ubse_api_buffer_free(&response_buffer);
        return ubse_map_daemon_error(ret);
    }
    // 解包
    ret = ubse_node_cpu_topo_list_unpack(response_buffer.buffer, response_buffer.length, cpu_links, cpu_link_cnt);
    if (ret != UBS_SUCCESS) {
        IPC_LOG_ERROR << "Failed to unpack CPU topology data, error" << ret;
    }
    ubse_api_buffer_free(&response_buffer);
    return static_cast<int32_t>(ret);
}
