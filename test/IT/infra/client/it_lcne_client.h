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

#ifndef IT_LCNE_CLIENT_H
#define IT_LCNE_CLIENT_H

#include <cstdint>
#include <string>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_error.h"

namespace ubse::it::infra {

using ubse::common::def::UbseResult;

/**
 * @brief LCNE 拓扑节点端口信息
 */
struct LcnePortInfo {
    uint32_t portId;           // 端口ID
    std::string interfaceName; // 接口名 (e.g. 400GUB1/0/1)
    std::string status;        // 端口状态 (up/down)
    uint32_t remoteSlot;       // 对端节点ID
    uint32_t remoteUbpu;       // 对端UBPU ID
    uint32_t remoteIou;        // 对端IOU ID
    uint32_t remotePortId;     // 对端端口ID
};

/**
 * @brief LCNE 拓扑节点信息
 */
struct LcneNodeInfo {
    uint32_t slot;                   // 节点ID
    uint32_t ubpu;                   // UBPU ID
    uint32_t iou;                    // IOU ID
    std::vector<LcnePortInfo> ports; // 端口列表
};

/**
 * @brief LCNE 拓扑连接信息
 */
struct LcneLinkInfo {
    uint32_t localSlot;              // 本端节点ID
    uint32_t localUbpu;              // 本端UBPU ID (LCNE chipId)
    uint32_t localSocketId;          // 本端Socket ID (OS socketId, mapped from Ubpu)
    uint32_t localPort;              // 本端端口ID
    std::string localInterfaceName;  // 本端接口名
    uint32_t remoteSlot;             // 对端节点ID
    uint32_t remoteUbpu;             // 对端UBPU ID (LCNE chipId)
    uint32_t remoteSocketId;         // 对端Socket ID (OS socketId, mapped from Ubpu)
    uint32_t remotePort;             // 对端端口ID
    std::string remoteInterfaceName; // 对端接口名
};

/**
 * @brief LCNE logic-entity信息（从logic-entities接口获取）
 */
struct LcneLogicEntityInfo {
    std::string busInstanceEid; // Bus Instance EID
    std::string guid;           // GUID
    std::string type;           // 类型 (host/guest)
    std::string upi;            // UPI
    std::string state;          // 状态 (online/offline)
};

/**
 * @brief LCNE HTTP client wrapper for IT testing.
 *
 * Provides direct access to mock LCNE server's HTTP endpoints
 * for topology and CNA information retrieval.
 */
class ItLcneClient {
public:
    explicit ItLcneClient(const std::string& udsPath);

    ~ItLcneClient();

    /**
     * @brief Get topology nodes information from LCNE.
     * @param nodes [out] Node topology information list
     * @return UBSE_OK on success, error code on failure
     */
    UbseResult GetTopologyNodes(std::vector<LcneNodeInfo>& nodes);

    /**
     * @brief Get topology links information from LCNE.
     * @param links [out] Link information list (slot-ubpu-port -> remote_slot-remote_ubpu-remote_port)
     * @return UBSE_OK on success, error code on failure
     */
    UbseResult GetTopologyLinks(std::vector<LcneLinkInfo>& links);

    /**
     * @brief Get logic-entities information from LCNE.
     * @param entities [out] Logic entity information list
     * @return UBSE_OK on success, error code on failure
     */
    UbseResult GetLogicEntities(std::vector<LcneLogicEntityInfo>& entities);

    /**
     * @brief Build Ubpu to SocketId mapping based on topology data and system files.
     * Similar to UbseNodeController::UbseSocketIdChange logic:
     * 1. Collect all Ubpu (chipId) from topology nodes, sort them
     * 2. Collect all SocketId from system files (/sys/devices/system/node/has_cpu, etc.), sort them
     * 3. Build mapping: LCNE chipId -> OS socketId (sorted order)
     * 4. Apply mapping to LcneLinkInfo structures
     * @param links [in/out] Link information list to update with socketId
     * @return UBSE_OK on success, error code on failure
     */
    UbseResult BuildUbpuToSocketIdMapping(std::vector<LcneLinkInfo>& links);

private:
    std::string udsPath_;
};

} // namespace ubse::it::infra

#endif // IT_LCNE_CLIENT_H