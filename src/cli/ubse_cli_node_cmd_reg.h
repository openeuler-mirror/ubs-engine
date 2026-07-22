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
using ubse::cli::framework::UbseCliCommandInfo;
using ubse::cli::framework::UbseCliResultEcho;
using ubse::serial::UbseDeSerialization;

struct CliPhysicalLink {
    std::string node;          // hostname+节点id
    std::string socketId;      // socket id
    std::string portId;        // 端口id
    std::string interfaceName; // 端口name

    std::string peerNode;          // 对端节点id
    std::string peerSocketId;      // 对端socket id
    std::string peerPortId;        // 对端端口id
    std::string peerInterfaceName; // 对端端口name

    std::string linkId; // link id
};

class UbseCliRegNodeModule : public UbseCliRegModule {
public:
    void UbseCliSignUp() override;

private:
    static UbseCliCommandInfo UbseCliQueryClusterInfo();
    static UbseCliCommandInfo UbseCliQueryCpuTopology();
    static UbseCliCommandInfo UbseCliQueryNodeInfo(); // 新增

    // 表格处理函数
    static std::shared_ptr<UbseCliResultEcho> UbseCliProcessClusterDataTable(UbseDeSerialization& ubse_de_serial,
                                                                             size_t size);

    static std::shared_ptr<UbseCliResultEcho> BuildCpuTopoTable(const std::vector<CliPhysicalLink>& links);

    static std::shared_ptr<UbseCliResultEcho> BuildNodeInfoTable(const std::string& node, const std::string& role,
                                                                 const std::string& bondingEid,
                                                                 const std::string& guid);

    // 命令处理函数
    static std::shared_ptr<UbseCliResultEcho> UbseCliQueryClusterInfoFunc(
        [[maybe_unused]] const std::map<std::string, std::string>& params);

    static std::shared_ptr<UbseCliResultEcho> UbseCliSDKQueryCpuTopoFunc(
        [[maybe_unused]] const std::map<std::string, std::string>& params);

    static std::shared_ptr<UbseCliResultEcho> UbseCliQueryNodeInfoFunc(
        const std::map<std::string, std::string>& params);

    // 辅助函数
    static bool ValidateNodeId(const std::string& nodeId, uint32_t& parsedId);

    static std::shared_ptr<UbseCliResultEcho> QueryNodeInfo(const std::string& nodeId);

    static std::shared_ptr<UbseCliResultEcho> ProcessNodeInfoResponse(UbseDeSerialization& ubse_de_serial);
};
} // namespace ubse::cli::reg
#endif // UBSE_CLI_NODE_CMD_REG_H