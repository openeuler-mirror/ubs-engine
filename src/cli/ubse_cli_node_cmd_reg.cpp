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
#include "ubse_cli_node_cmd_reg.h"
#include "ubse_common_def.h"
#include "ubse_cli_buffer_guard.h"
#include "ubse_ipc_common.h"
#include "ubse_serial_util.h"
#include "ubse_error.h"

namespace ubse::cli::reg {
UBSE_CLI_REGISTER_MODULE("CLI_NODE_MODULE", UbseCliRegNodeModule);
using namespace ubse::cli::framework;
using namespace ubse::common::def;
using namespace ubse::serial;
static const std::string DE_SERIALIZATION_ERROR = "ERROR: Deserialization failed in client.";
static const std::string TOPO_EMPTY = "INFO: The topo information is empty.";
static const std::string NODE_INTERNAL_ERROR = "ERROR: Internal error with error code ";
static const std::string CLUSTRR_EMPTY_ERROR = "ERROR: Failed to obtain cluster information.";

static const uint16_t TOPO_MODULE_CODE = 0x002;
static const uint16_t TOPO_QUERY_UBSE_TOPOLOGY_OP_CODE = 0x004;

static const std::string NODE_ID_OPT = "node";
static const std::string NODE_OPT_DESC = "Specify node ID to query, "
                                         "The range is from 1 to 255. The example is : 1 or 99. "
                                         "If not specified, query local node.";
static const std::string NODE_ID_MISSING = "ERROR: Missing node ID. \"-n/--node\" parameter is required with value.";
static const std::string INVALID_NODE_ID = "ERROR: Invalid node ID. Please specify a number between 1-255.";

// 节点错误信息
static const std::string NODE_NOT_FOUND_MSG = "ERROR: Node %s does not exist in the cluster";
static const std::string NODE_UNREACHABLE_MSG = "ERROR: Failed to connect to node %s."
                                                " The node may be down or unreachable";
static const std::string NODE_NOT_ACTIVE_MSG = "ERROR: Node %s is not active";
static const std::string NODE_NOT_RESPONDING_MSG = "ERROR: Node %s is not responding";
constexpr size_t NODE_LENGTH = 80; // hostname(slot_id), hostname最长为64


static void InitializeTable(UbseCliResBuilder &variable_cell_builder)
{
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(variable_cell_builder.UbseCliGetRows());
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "socketId");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "remote_node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "remote_socketId");
    variable_cell_builder.UbseCliAddBottomlineSeparate();
}

static UbseResult UbseCliProcessTopologyData(UbseCliResBuilder &variable_cell_builder,
    UbseDeSerialization &ubse_de_serial, size_t ubse_node_topo_size)
{
    for (size_t k_i = 0; k_i < ubse_node_topo_size; k_i++) {
        size_t row = variable_cell_builder.UbseCliAddRow();
        std::string ubse_node{};
        std::string usbe_socketId{};
        ubse_de_serial >> ubse_node >> usbe_socketId;
        if (!ubse_de_serial.Check()) {
            return UBSE_ERROR;
        }
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, std::string("node-" + ubse_node));
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, usbe_socketId);
        size_t ubse_one_step_node_topo_size{};
        ubse_de_serial >> ubse_one_step_node_topo_size;
        if (!ubse_de_serial.Check()) {
            return UBSE_ERROR;
        }
        if (ubse_one_step_node_topo_size == 0) {
            variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "none");
            variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "none");
            continue;
        }
        std::string ubse_remote_node{};
        std::string usbe_remote_socketId{};
        ubse_de_serial >> ubse_remote_node >> usbe_remote_socketId;
        if (!ubse_de_serial.Check()) {
            return UBSE_ERROR;
        }
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, std::string("node-" + ubse_remote_node));
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, usbe_remote_socketId);
        for (size_t v_i = 1; v_i < ubse_one_step_node_topo_size; ++v_i) {
            row = variable_cell_builder.UbseCliAddRow();
            variable_cell_builder.UbseCliAddNolineSeparate(row);
            ubse_de_serial >> ubse_remote_node >> usbe_remote_socketId;
            if (!ubse_de_serial.Check()) {
                return UBSE_ERROR;
            }
            variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, std::string("node-" + ubse_remote_node));
            variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, usbe_remote_socketId);
        }
        if (!ubse_de_serial.Check()) {
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegNodeModule::UbseCliProcessTopologyDataTable(
    UbseDeSerialization &ubse_de_serial, size_t ubse_node_topo_size)
{
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_4, UBSE_CLI_NUM_10 + UBSE_CLI_NUM_10);

    InitializeTable(variable_cell_builder);
    auto ret = UbseCliProcessTopologyData(variable_cell_builder, ubse_de_serial, ubse_node_topo_size);
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegNodeModule::UbseQueryUbseTopologyFunc([
    [maybe_unused]] const std::map<std::string, std::string> &params)
{
    UbseSerialization ubse_req_serial(NO_8);
    ubse_api_buffer_t ubse_req_buffer{ ubse_req_serial.GetBuffer(),
        static_cast<uint32_t>(ubse_req_serial.GetLength()) };
    ubse_api_buffer_t ubse_res_buffer{};
    uint32_t ret =
        ubse_invoke_call(TOPO_MODULE_CODE, TOPO_QUERY_UBSE_TOPOLOGY_OP_CODE, &ubse_req_buffer, &ubse_res_buffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(std::string("ERROR: Internal error with error code " + std::to_string(ret)));
    }
    UbseDeSerialization ubse_de_serial(ubse_res_buffer.buffer, ubse_res_buffer.length);
    size_t ubse_node_topo_size{};
    ubse_de_serial >> ubse_node_topo_size;
    if (!ubse_de_serial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    if (ubse_node_topo_size == 0) {
        return UbseCliStringPromptReply(TOPO_EMPTY);
    }
    return UbseCliProcessTopologyDataTable(ubse_de_serial, ubse_node_topo_size);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegNodeModule::UbseCliProcessClusterDataTable(
    UbseDeSerialization &ubse_de_serial, size_t size)
{
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_3, UBSE_CLI_NUM_4 * UBSE_CLI_NUM_10);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "role");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "bonding-eid");
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    for (size_t i = 0; i < size; i++) {
        row = variable_cell_builder.UbseCliAddRow();
        std::string node{};
        std::string role{};
        std::string bondingEid{};
        ubse_de_serial >> node >> role >> bondingEid;
        if (!ubse_de_serial.Check()) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, node);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, role);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, bondingEid);
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegNodeModule::UbseQueryClusterInfoFunc([
        [maybe_unused]] const std::map<std::string, std::string> &params)
{
    UbseSerialization ubse_req_serial(UBSE_CLI_NUM_8);
    ubse_api_buffer_t ubse_req_buffer{ubse_req_serial.GetBuffer(), static_cast<uint32_t>(ubse_req_serial.GetLength())};
    ubse_api_buffer_t ubse_res_buffer{};
    uint32_t ret = ubse_invoke_call(UBSE_NODE, UBSE_CLUSTER_INFO, &ubse_req_buffer, &ubse_res_buffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(NODE_INTERNAL_ERROR + std::to_string(ret));
    }
    UbseDeSerialization ubse_de_serial(ubse_res_buffer.buffer, ubse_res_buffer.length);
    size_t size = 0;
    ubse_de_serial >> size;
    if (!ubse_de_serial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    if (size == 0) {
        return UbseCliStringPromptReply(CLUSTRR_EMPTY_ERROR);
    }
    return UbseCliProcessClusterDataTable(ubse_de_serial, size);
}

UbseCliCommandInfo UbseCliRegNodeModule::UbseCliGenerateQueryUbseTopology()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("display").UbseCliSetType("topo").UbseCliSetFunc(UbseQueryUbseTopologyFunc);
    return builder.UbseCliBuild();
}

UbseCliCommandInfo UbseCliRegNodeModule::UbseCliQueryClusterInfo()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("display").UbseCliSetType("cluster").UbseCliSetFunc(UbseQueryClusterInfoFunc);
    return builder.UbseCliBuild();
}


std::string ExtractNodeId(const std::string& nodeName)
{
    std::string::size_type openParen = nodeName.find('(');
    std::string::size_type closeParen = nodeName.find(')');
    if (openParen != std::string::npos && closeParen != std::string::npos && closeParen > openParen + 1) {
        return nodeName.substr(openParen + 1, closeParen - openParen - 1);
    }
    return nodeName;
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegNodeModule::BuildNodeInfoTable(const std::string &node,
                                                                            const std::string &role,
                                                                            const std::string &bondingEid)
{
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_3, NODE_LENGTH);

    // 添加表头
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "role");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "bonding-eid");
    variable_cell_builder.UbseCliAddBottomlineSeparate();

    // 添加数据行
    row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, node);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, role);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, bondingEid);
    variable_cell_builder.UbseCliAddBottomlineSeparate();

    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

bool UbseCliRegNodeModule::ValidateNodeId(const std::string &nodeId, uint32_t &parsedId)
{
    // 检查是否为纯数字
    if (nodeId.empty() || !std::all_of(nodeId.begin(), nodeId.end(), ::isdigit)) {
        return false;
    }
    try {
        unsigned long temp = std::stoul(nodeId);
        // 检查是否超出uint32_t范围
        if (temp > std::numeric_limits<uint32_t>::max()) {
            return false;
        }
        parsedId = static_cast<uint32_t>(temp);
    } catch (const std::exception &) {
        return false;
    }
    // 检查范围 1-255
    return parsedId >= 1 && parsedId <= 255;
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegNodeModule::ProcessNodeInfoResponse(UbseDeSerialization &ubse_de_serial)
{
    // 读取错误码
    uint32_t errorCode = 0;
    ubse_de_serial >> errorCode;
    if (!ubse_de_serial.Check()) return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    // 读取节点信息
    std::string node;
    std::string role;
    std::string bondingEid;
    ubse_de_serial >> node >> role >> bondingEid;
    if (!ubse_de_serial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    // 表示成功
    if (errorCode == UBSE_OK) {
        return BuildNodeInfoTable(node, role, bondingEid);
    }
    // 格式化错误信息
    std::string errorMsg;
    if (errorCode == UBSE_ERR_NODE_NOT_FOUND) {
        errorMsg = "ERROR: Node " + node + " does not exist in the cluster";
        return UbseCliStringPromptReply(errorMsg);
    } else if (errorCode == UBSE_ERR_NODE_NOT_ACTIVE) {
        std::string nodeId = ExtractNodeId(node);
        if (nodeId.empty()) {
            nodeId = node;
        }
        errorMsg = "ERROR: Node " + nodeId + " is not active";
    } else if (errorCode == UBSE_ERR_NODE_NOT_RESPONDING) {
        std::string nodeId = ExtractNodeId(node);
          if (nodeId.empty()) {
            nodeId = node;
        }
        errorMsg = "ERROR: Node " + nodeId + " is not responding";
    } else {
        return UbseCliStringPromptReply("ERROR: Unknown error code " + std::to_string(errorCode));
    }
    // 对于非"NOT_FOUND"错误，显示表格
    UbseCliResBuilder combined_builder(UBSE_CLI_NUM_3, NODE_LENGTH);
    // 添加错误信息行
    size_t row = combined_builder.UbseCliAddRow();
    combined_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, errorMsg);
    combined_builder.UbseCliAddBottomlineSeparate();
    // 添加表头
    row = combined_builder.UbseCliAddRow();
    combined_builder.UbseCliAddlineSeparate(row);
    combined_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "node");
    combined_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "role");
    combined_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "bonding-eid");
    combined_builder.UbseCliAddBottomlineSeparate();
    // 添加数据行
    row = combined_builder.UbseCliAddRow();
    combined_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, node);
    combined_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, role);
    combined_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, bondingEid);
    combined_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(combined_builder.UbseCliVariableCellBuild());
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegNodeModule::QueryNodeInfo(const std::string& nodeId)
{
    // 构建请求
    serial::UbseSerialization ubse_req_serial;

    // 序列化节点ID，即使是空字符串
    ubse_req_serial << nodeId;

    if (!ubse_req_serial.Check()) {
        return UbseCliStringPromptReply("ERROR: Request serialization failed");
    }

    ubse_api_buffer_t ubse_req_buffer{ubse_req_serial.GetBuffer(),
                                      static_cast<uint32_t>(ubse_req_serial.GetLength())};
    ubse_api_buffer_t ubse_res_buffer{};

    // 使用UBSE_NODE_CLI_NODE_INFO操作码
    uint32_t ret = ubse_invoke_call(UBSE_NODE, UBSE_NODE_CLI_NODE_INFO,
                                    &ubse_req_buffer, &ubse_res_buffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);

    // 处理IPC错误
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply("ERROR: IPC call failed with error code " + std::to_string(ret));
    }

    // 处理响应
    if (ubse_res_buffer.length == 0 || ubse_res_buffer.buffer == nullptr) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }

    UbseDeSerialization ubse_de_serial(ubse_res_buffer.buffer, ubse_res_buffer.length);
    if (!ubse_de_serial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }

    return ProcessNodeInfoResponse(ubse_de_serial);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegNodeModule::UbseCliQueryNodeInfoFunc(
    const std::map<std::string, std::string> &params)
{
    std::string nodeIdStr;
    return QueryNodeInfo(nodeIdStr);
}

UbseCliCommandInfo UbseCliRegNodeModule::UbseCliQueryNodeInfo()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("display")
        .UbseCliSetType("node")
        .UbseCliSetFunc(UbseCliQueryNodeInfoFunc);
    return builder.UbseCliBuild();
}

void UbseCliRegNodeModule::UbseCliSignUp()
{
    this->cmd.emplace_back(UbseCliGenerateQueryUbseTopology());
    this->cmd.emplace_back(UbseCliQueryClusterInfo());
    this->cmd.emplace_back(UbseCliQueryNodeInfo());
}
} // namespace ubse::cli::reg
