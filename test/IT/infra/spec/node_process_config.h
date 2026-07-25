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

#ifndef NODE_PROCESS_CONFIG_H
#define NODE_PROCESS_CONFIG_H

#include <cstdint>
#include <string>
#include <vector>

namespace ubse::it::infra {

/**
 * @brief Configuration for a single daemon process.
 *
 * Extracted from node_process_manager.h to allow ItNode and other
 * components to reference the config without depending on the full
 * NodeProcessManager class.
 */
struct NodeProcessConfig {
    std::string binaryPath;
    std::string nodeId;
    std::string nodeIp;
    std::string workDir;
    uint32_t slotId = 1;
    std::string stubLibDir;
    std::string clusterNodeIds;
    std::string clusterIps;
    std::vector<uint32_t> clusterSlotIds;
    std::string sceneType;
    uint32_t meshType = 1;
    std::string lcneUdsPath;
};

} // namespace ubse::it::infra

#endif // NODE_PROCESS_CONFIG_H
