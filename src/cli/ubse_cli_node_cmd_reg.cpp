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
#include <securec.h>
#include "ubse_cli_buffer_guard.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_pointer_process.h"
#include "ubse_serial_util.h"

namespace ubse::cli::reg {
UBSE_CLI_REGISTER_MODULE("CLI_NODE_MODULE", UbseCliRegNodeModule);
using namespace ubse::cli::framework;
using namespace ubse::common::def;
using namespace ubse::serial;

static const std::string DE_SERIALIZATION_ERROR = "ERROR: Deserialization failed in client.";
static const std::string NODE_INTERNAL_ERROR = "ERROR: Internal error with error code ";
static const std::string CLUSTRR_EMPTY_ERROR = "ERROR: Failed to obtain cluster information.";

static const std::string TYPE_CPU = "cpu";

// display topo option reg
static const std::string DISPLAY_TOPO_T_OPTION = "type";
// display topo option desc
static const std::string DISPLAY_TOPO_TYPE_OPTION_TIP =
    "Query topology information in a supernode. The option is as follows: cpu."
    "This command is not supported in CLOS mesh type topo";
// display node input option error
static const std::string DISPLAY_TOPO_TYPE_OPTION_REQUIRED =
    "ERROR: The request option -t or --type is required, and the supported param is as follows: cpu.";
static const std::string DISPLAY_TOPO_TYPE_PARAM_INVALID =
    "ERROR: Invalid type. The supported param is as follows: cpu.";

// display node option reg
static const std::string DISPLAY_NODE_N_OPTION = "node";
// display node option desc
static const std::string DISPLAY_NODE_N_OPTION_TIP =
    "Specify the node ID. The range is from 1 to 255, the default value is local node ID."
    "Parameter 'node' is not supported in CLOS mesh type topo";
// display node input option error
static const std::string DISPLAY_NODE_PARAM_INVALID = "ERROR: Invalid node ID. Please specify a number between 1-255.";
static const std::string DISPLAY_NOT_SUPPORT =
    "ERROR: this command is not supported in current topo";
static const std::string DISPLAY_NODE_NOT_SUPPORT =
    "ERROR: Invalid request param, param -n is not supported in current topo";

constexpr size_t NODE_LENGTH = 80; // hostname(slot_id), hostname最长为64

std::shared_ptr<UbseCliResultEcho> UbseCliRegNodeModule::UbseCliProcessClusterDataTable(
    UbseDeSerialization &ubse_de_serial, size_t size)
{
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_4, NODE_LENGTH);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "role");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "bonding-eid");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "guid");
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    for (size_t i = 0; i < size; i++) {
        row = variable_cell_builder.UbseCliAddRow();
        std::string node{};
        std::string role{};
        std::string bondingEid{};
        std::string guid;
        ubse_de_serial >> node >> role >> bondingEid >> guid;
        if (!ubse_de_serial.Check()) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, node);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, role);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, bondingEid);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, guid);
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegNodeModule::UbseCliQueryClusterInfoFunc([
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

uint32_t ParseCliPhysicalLink(CliPhysicalLink &link, UbseDeSerialization &inStream)
{
    inStream
        >> link.node >> link.socketId >> link.portId >> link.interfaceName
        >> link.peerNode >> link.peerSocketId >> link.peerPortId >> link.peerInterfaceName  >> link.linkId;

    if (!inStream.Check()) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

uint32_t DeSerializePhysicalLinks(std::vector<CliPhysicalLink> &links, uint8_t *buffer, size_t size)
{
    UbseResult ret = UBSE_OK;
    UbseDeSerialization inStream(buffer, size);
    size_t num = 0;
    inStream >> num;
    if (!inStream.Check() || num > SIZE_MAX) {return UBSE_ERROR;}
    links.reserve(num);
    for (size_t i = 0; i < num; i++) {
        UbseDeSerialization item;
        inStream >> item;
        CliPhysicalLink link{};
        ret = ParseCliPhysicalLink(link, item);
        if (ret != UBSE_OK) {
            return ret;
        }
        links.push_back(link);
        if (!inStream.Check()) {
            return UBSE_ERROR;
        }
    }
    return UBSE_OK;
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegNodeModule::BuildCpuTopoTable(const std::vector<CliPhysicalLink> &links)
{
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_9, NODE_LENGTH);

    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "cpu topo:");
    variable_cell_builder.UbseCliAddBottomlineSeparate();

    // 表头
    row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "link-id");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "socket");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "port");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, "interface-name");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, "peer-node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, "peer-socket");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_8, "peer-port");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_9, "peer-interface-name");

    variable_cell_builder.UbseCliAddBottomlineSeparate();

    uint32_t availableCount = 0;
    uint32_t totalCount = static_cast<uint32_t>(links.size());

    for (size_t i = 0; i < links.size(); ++i) {
        row = variable_cell_builder.UbseCliAddRow();
        const auto &link = links[i];

        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, link.linkId);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, link.node);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, link.socketId);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, link.portId);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, link.interfaceName);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, link.peerNode);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, link.peerSocketId);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_8, link.peerPortId);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_9, link.peerInterfaceName);

        if (link.linkId != "-") {
            availableCount++;
        }
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();

    std::map<size_t, std::string> totalCountRow = {{UBSE_CLI_NUM_9, "Total Links: " + std::to_string(totalCount)}};
    variable_cell_builder.UbseCliAddMergeRow(totalCountRow);

    std::map<size_t, std::string> validCountRow = {
        {UBSE_CLI_NUM_9, "Available Links: " + std::to_string(availableCount)}};
    variable_cell_builder.UbseCliAddMergeRow(validCountRow);

    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegNodeModule::UbseCliSDKQueryCpuTopoFunc([
    [maybe_unused]] const std::map<std::string, std::string> &params)
{
    auto it_type = params.find(DISPLAY_TOPO_T_OPTION);
    if (it_type == params.end()) {
        return UbseCliStringPromptReply(DISPLAY_TOPO_TYPE_OPTION_REQUIRED);
    }
    if (it_type->second != TYPE_CPU) {
        return UbseCliStringPromptReply(DISPLAY_TOPO_TYPE_PARAM_INVALID);
    }

    uint16_t moduleCode = ubse_ipc_module_code_t::UBSE_NODE;
    uint16_t opCode = ubse_ipc_node_op_code_t::UBSE_NODE_CLI_CPU_TOPO_LIST;

    std::string request = "query cpu";
    ubse_api_buffer_t reqBuffer{};
    reqBuffer.length = static_cast<uint32_t>(request.length());
    reqBuffer.buffer = new (std::nothrow) uint8_t[reqBuffer.length];
    if (reqBuffer.buffer == nullptr) {
        return UbseCliStringPromptReply("ERROR: Memory alloc failed");
    }
    if (memcpy_s(reqBuffer.buffer, reqBuffer.length, request.c_str(), reqBuffer.length) != EOK) {
        SafeDeleteArray(reqBuffer.buffer);
        return UbseCliStringPromptReply("ERROR: Memory copy failed");
    }

    ubse_api_buffer_t respBuffer{};
    uint32_t ret = ubse_invoke_call(moduleCode, opCode, &reqBuffer, &respBuffer);
    SafeDeleteArray(reqBuffer.buffer);

    if (ret != UBSE_OK) {
        ubse_api_buffer_free(&respBuffer);
        if (ret == UBSE_ERR_NOT_SUPPORTED) {
            return UbseCliStringPromptReply(DISPLAY_NOT_SUPPORT);
        }
        return UbseCliStringPromptReply("ERROR: IPC call failed");
    }

    std::vector<CliPhysicalLink> response;
    if (DeSerializePhysicalLinks(response, respBuffer.buffer, respBuffer.length) != UBSE_OK) {
        ubse_api_buffer_free(&respBuffer);
        return UbseCliStringPromptReply("ERROR: Deserialize failed");
    }

    std::shared_ptr<UbseCliResultEcho> tableInfo = BuildCpuTopoTable(response);

    ubse_api_buffer_free(&respBuffer);
    return tableInfo;
}

UbseCliCommandInfo UbseCliRegNodeModule::UbseCliQueryClusterInfo()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("display").UbseCliSetType("cluster").UbseCliSetFunc(UbseCliQueryClusterInfoFunc);
    return builder.UbseCliBuild();
}

UbseCliCommandInfo UbseCliRegNodeModule::UbseCliQueryCpuTopology()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("display")
        .UbseCliSetType("topo")
        .UbseCliAddOption("t", DISPLAY_TOPO_T_OPTION, DISPLAY_TOPO_TYPE_OPTION_TIP)
        .UbseCliSetFunc(UbseCliSDKQueryCpuTopoFunc);
    return builder.UbseCliBuild();
}

// display node 命令相关函数
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

std::shared_ptr<UbseCliResultEcho> UbseCliRegNodeModule::BuildNodeInfoTable(
    const std::string &node, const std::string &role, const std::string &bondingEid, const std::string &guid)
{
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_4, NODE_LENGTH);

    // 添加表头
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "role");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "bonding-eid");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "guid");
    variable_cell_builder.UbseCliAddBottomlineSeparate();

    // 添加数据行
    row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, node);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, role);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, bondingEid);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, guid);
    variable_cell_builder.UbseCliAddBottomlineSeparate();

    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
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
    std::string guid;
    ubse_de_serial >> node >> role >> bondingEid >> guid;
    if (!ubse_de_serial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    // 表示成功
    if (errorCode == UBSE_OK) {
        return BuildNodeInfoTable(node, role, bondingEid, guid);
    }
    // 格式化错误信息
    std::string errorMsg;
    if (errorCode == UBSE_ERR_NODE_NOT_FOUND) {
        errorMsg = "ERROR: Node " + node + " does not exist in the cluster";
        return UbseCliStringPromptReply(errorMsg);
    } else if (errorCode == UBSE_ERR_NODE_NOT_ACTIVE) {
        std::string nodeId = ExtractNodeId(node);
        if (nodeId.empty()) nodeId = node;
        errorMsg = "ERROR: Node " + nodeId + " is not active";
    } else if (errorCode == UBSE_ERR_NODE_NOT_RESPONDING) {
        std::string nodeId = ExtractNodeId(node);
        if (nodeId.empty()) nodeId = node;
        errorMsg = "ERROR: Node " + nodeId + " is not responding";
    } else {
        return UbseCliStringPromptReply("ERROR: Unknown error code " + std::to_string(errorCode));
    }
    // 对于非"NOT_FOUND"错误，显示表格
    UbseCliResBuilder combined_builder(UBSE_CLI_NUM_4, NODE_LENGTH);
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
    combined_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "guid");
    combined_builder.UbseCliAddBottomlineSeparate();
    // 添加数据行
    row = combined_builder.UbseCliAddRow();
    combined_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, node);
    combined_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, role);
    combined_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, bondingEid);
    combined_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, guid);
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
        if (ret == UBSE_ERR_NOT_SUPPORTED) {
            return UbseCliStringPromptReply(DISPLAY_NODE_NOT_SUPPORT);
        }
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
    auto it_node = params.find(DISPLAY_NODE_N_OPTION);
    std::string nodeIdStr = "";
    if (it_node != params.end()) {
        // 如果有-n参数
        if (it_node->second.empty()) {
            return UbseCliStringPromptReply(DISPLAY_NODE_PARAM_INVALID);
        }
        nodeIdStr = it_node->second;
        // 验证节点ID格式
        uint32_t nodeId = 0;
        if (!ValidateNodeId(nodeIdStr, nodeId)) {
            return UbseCliStringPromptReply(DISPLAY_NODE_PARAM_INVALID);
        }
    }
    // 如果没有-n参数，nodeIdStr保持为空
    return QueryNodeInfo(nodeIdStr);
}

UbseCliCommandInfo UbseCliRegNodeModule::UbseCliQueryNodeInfo()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("display")
        .UbseCliSetType("node")
        .UbseCliAddOption("n", DISPLAY_NODE_N_OPTION, DISPLAY_NODE_N_OPTION_TIP)
        .UbseCliSetFunc(UbseCliQueryNodeInfoFunc);
    return builder.UbseCliBuild();
}

void UbseCliRegNodeModule::UbseCliSignUp()
{
    this->cmd_.emplace_back(UbseCliQueryClusterInfo());
    this->cmd_.emplace_back(UbseCliQueryCpuTopology());
    this->cmd_.emplace_back(UbseCliQueryNodeInfo());
}
} // namespace ubse::cli::reg