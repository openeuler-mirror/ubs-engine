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

#ifndef MOCK_LCNE_SERVER_H
#define MOCK_LCNE_SERVER_H

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "httplib.h"

namespace ubse::it::infra {

using ubse::common::def::UbseResult;

/**
 * @brief Mock LCNE HTTP server for IT testing (UDS only).
 *
 * Supports two topology modes:
 *   - FULL_MESH: slot-based XML (BusInstanceXml, TopologyNodesXml, etc.)
 *   - CLOS: nodeId-based XML (ClosBusInstanceXml, ClosTopologyNodesXml, etc.)
 *         CLOS uses keyed single-query for ⑦⑧, skips ⑥ static-urma-eids.
 */
class MockLcneServer {
public:
    /**
     * @brief Constructor.
     * @param udsPath Unix domain socket path
     * @param slotId Slot ID for FULL_MESH XML generation
     * @param clusterSlotIds All slot IDs in the cluster (for topology links)
     * @param isClos true: use CLOS XML generators; false: use FULL_MESH generators
     * @param nodeId CLOS node ID (= serverIdx + 1), only used when isClos=true
     */
    MockLcneServer(const std::string& udsPath, uint32_t slotId, const std::vector<uint32_t>& clusterSlotIds = {},
                   bool isClos = false, uint32_t nodeId = 1);

    ~MockLcneServer();

    UbseResult Start();
    UbseResult Stop();
    bool IsReady();
    UbseResult WaitForReady(uint32_t timeoutMs);

    uint32_t GetSlotId() const
    {
        return slotId_;
    }
    uint32_t GetNodeId() const
    {
        return nodeId_;
    }
    bool IsClos() const
    {
        return isClos_;
    }

private:
    void RegisterHandlers();
    void RegisterFullMeshHandlers();
    void RegisterClosHandlers();

    httplib::Server server_;
    std::string udsPath_;
    uint32_t slotId_ = 1;
    uint32_t nodeId_ = 1;
    bool isClos_ = false;
    std::vector<uint32_t> clusterSlotIds_;
    std::thread serverThread_;
    std::atomic_bool running_;
};

} // namespace ubse::it::infra

#endif // MOCK_LCNE_SERVER_H
