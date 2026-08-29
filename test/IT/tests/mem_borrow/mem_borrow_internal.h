/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef IT_MEM_BORROW_INTERNAL_H
#define IT_MEM_BORROW_INTERNAL_H

#include "ubse_common_def.h"
#include "it_sdk_client.h"
#include "ubs_engine_mem.h"
#include "ubs_engine_topo.h"
#include "ubs_error.h"

#include <unistd.h>

#include <cstdint>
#include <cstdlib>

namespace ubse::it::tests::mem_borrow {

// 借出方节点/槽位（多节点用例通用）
constexpr const char* lenderNodeId = "1"; // 借出方节点
constexpr uint32_t lenderSlotId = 1;      // 借出方槽位

constexpr uint64_t fdSize = UBS_MEM_MIN_SIZE;               // 4MB
constexpr uint64_t numaSize = UBS_MEM_MIN_SIZE;             // 4MB
constexpr uint64_t shmSize = UBS_MEM_MIN_SIZE;              // 4MB
constexpr uint64_t fdSize129M = 129ULL * 1024ULL * 1024ULL; // 129MB, 128M一块 → 2块

// 轮询等待FD内存就绪（最多60s）
inline void WaitForFdReady(ubse::it::infra::ItSdkClient& sdk, const char* name)
{
    for (int i = 0; i < 60; i++) {
        ubs_mem_fd_desc_t desc{};
        int32_t ret = sdk.MemFdGet(name, &desc);
        if (ret == UBS_SUCCESS && desc.mem_stage == UBSE_EXIST) {
            return;
        }
        sleep(1);
    }
}

// 轮询等待SHM内存就绪（最多60s），就绪返回UBS_SUCCESS，超时返回UBS_ENGINE_ERR_TIMEOUT
inline int32_t WaitForShmReady(ubse::it::infra::ItSdkClient& sdk, const char* name)
{
    for (int i = 0; i < 60; i++) {
        ubs_mem_shm_desc_t* desc = nullptr;
        int32_t ret = sdk.MemShmGet(name, &desc);
        if (ret == UBS_SUCCESS && desc != nullptr && desc->mem_stage == UBSE_EXIST) {
            free(desc);
            return UBS_SUCCESS;
        }
        if (desc != nullptr) {
            free(desc);
        }
        sleep(1);
    }
    return UBS_ENGINE_ERR_TIMEOUT;
}

// 通过topo链路获取借出节点的port_id/socket_id/numa_id，直接填充lender结构体
// 调用前需设置lender.slot_id；找到链路返回true；未找到或查询失败返回false
inline bool FillLenderTopoInfo(ubse::it::infra::ItSdkClient& sdk, uint32_t localSlotId, ubs_mem_lender_t& lender)
{
    // 从链路获取port_id和socket_id
    ubs_topo_link_t* links = nullptr;
    uint32_t linkCnt = 0;
    if (sdk.TopoLinkList(&links, &linkCnt) != UBS_SUCCESS) {
        free(links);
        return false;
    }
    bool found = false;
    for (uint32_t i = 0; i < linkCnt; i++) {
        if (links[i].slot_id == localSlotId && links[i].peer_slot_id == lender.slot_id) {
            lender.port_id = links[i].peer_port_id;
            lender.socket_id = links[i].peer_socket_id;
            found = true;
            break;
        }
    }
    free(links);
    if (!found) {
        return false;
    }

    // 从节点列表获取numa_id: 在借出节点中找到匹配socket_id的socket，取其第一个numa
    ubs_topo_node_t* nodes = nullptr;
    uint32_t nodeCnt = 0;
    if (sdk.TopoNodeList(&nodes, &nodeCnt) != UBS_SUCCESS) {
        free(nodes);
        return false;
    }
    bool numaFound = false;
    for (uint32_t i = 0; i < nodeCnt; i++) {
        if (nodes[i].slot_id != lender.slot_id) {
            continue;
        }
        for (uint32_t s = 0; s < UBS_TOPO_SOCKET_NUM; s++) {
            if (nodes[i].socket_id[s] == lender.socket_id) {
                lender.numa_id = nodes[i].numa_ids[s][0];
                numaFound = true;
                break;
            }
        }
        break;
    }
    free(nodes);
    return numaFound;
}

} // namespace ubse::it::tests::mem_borrow

#endif // IT_MEM_BORROW_INTERNAL_H
