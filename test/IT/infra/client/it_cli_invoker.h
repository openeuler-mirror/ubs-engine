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

#ifndef IT_CLI_INVOKER_H
#define IT_CLI_INVOKER_H

#include <cstdint>
#include <string>
#include <vector>

#include "ubse_election.h"
#include "it_node_info.h"

namespace ubse::it::infra {

class ItCliInvoker {
public:
    ItCliInvoker(const std::string& cliBinaryPath, const std::string& udsSocketPath);

    // --- Raw CLI queries ---
    int32_t QueryClusterInfo(std::vector<ItNodeInfo>& nodeInfos);
    int32_t QueryNodeInfo(ItNodeInfo& nodeInfo, const std::string& nodeId = "");
    int32_t QueryTopoCpu(std::vector<ItTopoCpuLink>& topoLinks);
    int32_t QueryMemBorrowDetail(std::vector<ItMemBorrowDetail>& borrowDetails);
    std::string ExecCli(const std::string& args) const;

    // --- Memory CLI operations ---
    // Create memory with short options (-t, -n, -s, -l, -r), returns parsed creation info
    int32_t CreateMemoryNuma(ItMemCreateInfo& createInfo, const std::string& name, const std::string& size,
                             const std::string& link = "", bool useLongOptions = false);
    int32_t CreateMemoryFd(ItMemCreateInfo& createInfo, const std::string& name, const std::string& size,
                           bool useLongOptions = false);
    int32_t CreateMemoryShare(ItMemCreateInfo& createInfo, const std::string& name, const std::string& size,
                              const std::string& region, bool useLongOptions = false);
    // Delete memory with short options (-n, -t)
    int32_t DeleteMemory(const std::string& name, const std::string& type = "numa", bool useLongOptions = false);
    // Display memory with short options (-t, -bt, -n, -a), returns parsed table records
    int32_t DisplayMemoryBorrowDetail(std::vector<ItMemBorrowDetail>& borrowDetails, const std::string& borrowType = "",
                                      const std::string& name = "", bool useLongOptions = false);
    int32_t DisplayMemoryNodeBorrow(std::vector<ItNodeBorrowInfo>& nodeBorrows, bool useLongOptions = false);
    int32_t DisplayMemoryNodeLend(std::vector<ItNodeLendInfo>& nodeLends, bool useLongOptions = false);
    std::string DisplayMemoryNumaStatus(bool showAll = false, bool useLongOptions = false);
    std::string DisplayMemoryConfig(bool useLongOptions = false);

    // --- Election convenience methods (moved from ItSdkClient) ---
    int32_t GetRole(std::string& role);
    int32_t GetMasterNodeId(std::string& masterNodeId);
    int32_t GetCurrentNodeId(std::string& currentNodeId);
    int32_t GetAllNodeInfos(std::vector<ubse::election::UbseRoleInfo>& roleInfos);

private:
    std::string ExecuteCommand(const std::string& command) const;
    int32_t ParseTableOutput(const std::string& output, std::vector<ItNodeInfo>& nodeInfos) const;
    static std::string ExtractNodeId(const std::string& nodeName);

    std::string cliBinaryPath_;
    std::string udsSocketPath_;
};

} // namespace ubse::it::infra

#endif // IT_CLI_INVOKER_H