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

#ifndef CLUSTER_MANAGER_H
#define CLUSTER_MANAGER_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "it_config_builder.h"
#include "node_process_manager.h"
#include "ubse_common_def.h"
#include "ubse_error.h"

namespace ubse::it::infra {

using ubse::common::def::UbseResult;

class ClusterManager {
public:
    ClusterManager(const std::string& binaryPath, const std::string& baseWorkDir,
                   const std::vector<NodeConfig>& nodeConfigs);

    UbseResult StartCluster();
    UbseResult StopCluster();
    NodeProcessManager& GetNodeProcess(const std::string& nodeId);
    bool IsClusterReady() const;

private:
    std::string binaryPath_;
    std::string baseWorkDir_;
    std::vector<NodeConfig> nodeConfigs_;
    std::map<std::string, std::unique_ptr<NodeProcessManager>> nodes_;
};

} // namespace ubse::it::infra

#endif // CLUSTER_MANAGER_H