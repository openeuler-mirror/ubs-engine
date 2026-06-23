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

#ifndef NODE_PROCESS_MANAGER_H
#define NODE_PROCESS_MANAGER_H

#include <sys/types.h>
#include <cstdint>
#include <memory>
#include <string>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "mock_lcne_server.h"

namespace ubse::it::infra {

using ubse::common::def::UbseResult;

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
};

class NodeProcessManager {
public:
    explicit NodeProcessManager(NodeProcessConfig config);

    UbseResult Start();
    UbseResult Stop();
    bool IsRunning() const;
    pid_t GetPid() const;
    UbseResult WaitForStartup(uint32_t timeoutMs);
    bool IsPrivilegedMode() const
    {
        return isPrivileged_;
    }

    const std::string& GetNodeId() const;
    const std::string& GetWorkDir() const;
    const std::string& GetUdsSocketPath() const;

private:
    void SetupChildNamespace(bool isPrivileged);
    void CreateSymlinkFallback();
    void CleanupSymlinks();
    std::string binaryPath_;
    std::string nodeId_;
    std::string nodeIp_;
    std::string workDir_;
    uint32_t slotId_ = 1;
    std::string clusterNodeIds_;
    std::string clusterIps_;
    std::vector<uint32_t> clusterSlotIds_;
    std::string stubLibDir_;
    pid_t childPid_;
    std::string udsSocketPath_;
    std::unique_ptr<MockLcneServer> mockLcneServer_;
    std::string lcneUdsWorkPath_;
    bool isPrivileged_{false};
    bool symlinkCreatedRun_{false};
    bool symlinkCreatedLog_{false};
};

} // namespace ubse::it::infra

#endif // NODE_PROCESS_MANAGER_H
