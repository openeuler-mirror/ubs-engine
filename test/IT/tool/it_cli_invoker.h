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

namespace ubse::it::infra {

struct ItNodeInfo {
    std::string node;
    std::string role;
    std::string bondingEid;
    std::string guid;
};

class ItCliInvoker {
public:
    ItCliInvoker(const std::string& cliBinaryPath, const std::string& udsSocketPath);

    int32_t QueryClusterInfo(std::vector<ItNodeInfo>& nodeInfos);
    int32_t QueryNodeInfo(ItNodeInfo& nodeInfo, const std::string& nodeId = "");
    std::string ExecCli(const std::string& args) const;

private:
    std::string ExecuteCommand(const std::string& command) const;
    int32_t ParseTableOutput(const std::string& output, std::vector<ItNodeInfo>& nodeInfos) const;
    static std::string ExtractNodeId(const std::string& nodeName);

    std::string cliBinaryPath_;
    std::string udsSocketPath_;
};

} // namespace ubse::it::infra

#endif // IT_CLI_INVOKER_H