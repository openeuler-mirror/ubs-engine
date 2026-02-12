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

#ifndef UBSE_CLI_NODE_CMD_REG_H
#define UBSE_CLI_NODE_CMD_REG_H

#include "ubse_cli_reg.h"
#include "ubse_cli_reg_builder.h"
#include "ubse_serial_util.h"
namespace ubse::cli::reg {
using namespace ubse::cli::framework;
using namespace ubse::serial;
class UbseCliRegNodeModule : public UbseCliRegModule {
public:
    void UbseCliSignUp() override;

private:
    static UbseCliCommandInfo UbseCliGenerateQueryUbseTopology();
    static UbseCliCommandInfo UbseCliQueryClusterInfo();
    static std::shared_ptr<UbseCliResultEcho> UbseCliProcessTopologyDataTable(UbseDeSerialization &ubse_de_serial,
        size_t ubse_node_topo_size);
    static std::shared_ptr<UbseCliResultEcho> UbseQueryUbseTopologyFunc([
        [maybe_unused]] const std::map<std::string, std::string> &params);
    static std::shared_ptr<UbseCliResultEcho> UbseCliProcessClusterDataTable(UbseDeSerialization &ubse_de_serial,
        size_t size);
    static std::shared_ptr<UbseCliResultEcho> UbseQueryClusterInfoFunc([
        [maybe_unused]] const std::map<std::string, std::string> &params);

    static UbseCliCommandInfo UbseCliQueryNodeInfo();
    static std::shared_ptr<UbseCliResultEcho> BuildNodeInfoTable(const std::string &node, const std::string &role,
                                                                 const std::string &bondingEid);
    static std::shared_ptr<UbseCliResultEcho> UbseCliQueryNodeInfoFunc(
        const std::map<std::string, std::string> &params);
    // 辅助函数
    static bool ValidateNodeId(const std::string &nodeId, uint32_t &parsedId);
    static std::shared_ptr<UbseCliResultEcho> QueryNodeInfo(const std::string &nodeId);
    static std::shared_ptr<UbseCliResultEcho> ProcessNodeInfoResponse(UbseDeSerialization &ubse_de_serial);
};
}

#endif