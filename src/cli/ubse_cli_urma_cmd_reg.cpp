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

#include "ubse_cli_urma_cmd_reg.h"
#include <netinet/in.h>
#include <securec.h>
#include <sstream>
#include "ubse_cli_buffer_guard.h"
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_serial_util.h"

namespace ubse::cli::reg {
UBSE_CLI_REGISTER_MODULE("CLI_URMA_MODULE", UbseCliRegUrmaModule);
using namespace ubse::cli::framework;
using namespace ubse::serial;
using namespace ubse::common::def;

static const std::string DE_SERIALIZATION_ERROR = "ERROR: Deserialization failed in client.";
static const std::string URMA_NODE_OPT = "node";
static const std::string URMA_DEVICE_OPT = "dev";
static const std::string URMA_INTERNAL_ERROR = "ERROR: Internal error.";
static const std::string URMA_NODE_STATE_ERROR = "ERROR: Node state is abnormal, maybe fault or node down.";
static const std::string URMA_NODE_ID_ERROR =
    "ERROR: Invalid request param,The option is as follow: node-id(1 ~ max node-id).";
static const std::string URMA_EMPTY_ERROR = "ERROR: The urma List is empty.";
static const std::string URMA_ACTIVATE_OPTION_DES =
    "Query urma information for a specific node. Parameter 'node' between 1 and the maximum node ID.";
static const std::string URMA_QUERY_OPTION_DES_NODE =
    "Query urma information for a specific node. Parameter 'node' between 1 and the maximum node ID."
    "If omitted, defaults to querying the local node.";
static const std::string URMA_QUERY_OPTION_DES_NAME =
    "Query urma information for a specific device. Parameter 'dev' expects a valid device name string."
    "If omitted, defaults to querying all devices.";
static const std::string URMA_QOS_QUERY_OPTION_DES =
    "Query urma-qos information by node-id, the option is as follow: node-id(1 ~ max node-id).";
static const std::string URMA_CLI_URMA_STATUS_ERROR = "ERROR: Invalid URMA status.";
static const std::string URMA_NAME_FORMAT_ERROR = "ERROR: Invalid device name list format.";
constexpr uint32_t URMA_INFO_STATUS_NUM = 3;
std::array<std::string, URMA_INFO_STATUS_NUM> urmaStatusArray = {"active", "inactive", "unknown"};
static const std::string SERIALIZATION_ERROR = "ERROR: Serialization failed in client.";
static const std::string URMA_INVAL_ERROR = "ERROR: Argument may be invalid.";
static const std::string URMA_AGAIN_ERROR = "ERROR: Internal error, please try again.";

void UbseCliRegUrmaModule::UbseCliSignUp()
{
    this->cmd_.emplace_back(UbseCliQueryUrmaDevInfo());
    return;
}

UbseCliCommandInfo UbseCliRegUrmaModule::UbseCliQueryUrmaDevInfo()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("display")
        .UbseCliSetType("urma")
        .UbseCliAddOption("n", URMA_NODE_OPT, URMA_QUERY_OPTION_DES_NODE)
        .UbseCliAddOption("d", URMA_DEVICE_OPT, URMA_QUERY_OPTION_DES_NAME)
        .UbseCliSetFunc(UbseQueryUrmaDevInfoFunc);
    return builder.UbseCliBuild();
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::UbseCliProcessUrmaDevInfoTable(
    UbseDeSerialization& ubse_de_serial, uint32_t urma_size)
{
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_7, UBSE_CLI_NUM_7 * UBSE_CLI_NUM_10);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "urma-name");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "dev-eid");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "dev1-name");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "dev2-name");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, "dev1-eid");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, "dev2-eid");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, "status");
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    for (uint32_t i = 0; i < urma_size; i++) {
        row = variable_cell_builder.UbseCliAddRow();
        std::string uramName{};
        uint32_t urmaType{};
        std::vector<std::string> feNames;
        std::vector<std::string> feEids;
        uint32_t urmaStatus{};
        std::string devEid{};
        ubse_de_serial >> uramName >> urmaType >> devEid >> feNames >> feEids >> urmaStatus;
        if ((!ubse_de_serial.Check()) || (feNames.size() <= 1 || feEids.size() <= 1)) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        if (urmaStatus >= urmaStatusArray.size()) {
            return UbseCliStringPromptReply(URMA_CLI_URMA_STATUS_ERROR);
        }
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, uramName);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, devEid);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, feNames[0]);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, feNames[1]);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, feEids[0]);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, feEids[1]);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, urmaStatusArray[urmaStatus]);
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}
std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::UbseCliProcessUrmaQosTable(UbseDeSerialization& ubse_de_serial,
                                                                                    uint32_t urma_size)
{
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_5, UBSE_CLI_NUM_4 * UBSE_CLI_NUM_10);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "urma-name");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "minBandWidth(Gbps)");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "maxBandWidth(Gbps)");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "fe1-name");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, "fe2-name");
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    for (uint32_t i = 0; i < urma_size; i++) {
        row = variable_cell_builder.UbseCliAddRow();
        std::string uramName{};
        std::vector<std::string> feNames;
        uint32_t minBandWidth{};
        uint32_t maxBandWidth{};
        ubse_de_serial >> uramName >> feNames >> minBandWidth >> maxBandWidth;
        if ((!ubse_de_serial.Check()) || (feNames.size() <= 1)) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, uramName);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, std::to_string(minBandWidth));
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, std::to_string(maxBandWidth));
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, feNames[0]);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, feNames[1]);
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::UbseQueryUrmaQosFunc(
    [[maybe_unused]] const std::map<std::string, std::string>& params)
{
    auto urmaNode = params.find(URMA_NODE_OPT);
    if (urmaNode == params.end()) {
        return UbseCliStringPromptReply(URMA_NODE_ID_ERROR);
    }
    uint32_t nodeId;
    try {
        nodeId = static_cast<uint32_t>(std::stoul(urmaNode->second));
    } catch (const std::exception& e) {
        return UbseCliStringPromptReply(URMA_NODE_ID_ERROR);
    }
    UbseSerialization ubse_req_serial;
    ubse_req_serial << nodeId;
    if (!ubse_req_serial.Check()) {
        return UbseCliStringPromptReply(URMA_INTERNAL_ERROR);
    }
    ubse_api_buffer_t ubse_req_buffer{ubse_req_serial.GetBuffer(), static_cast<uint32_t>(ubse_req_serial.GetLength())};
    ubse_api_buffer_t ubse_res_buffer{};
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_CLI_QOS_GET, &ubse_req_buffer, &ubse_res_buffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(std::string("ERROR: Internal error with error code " + std::to_string(ret)));
    }
    UbseDeSerialization ubse_de_serial(ubse_res_buffer.buffer, ubse_res_buffer.length);
    uint32_t urmaSize{};
    ubse_de_serial >> urmaSize;
    if (!ubse_de_serial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    if (urmaSize == 0) {
        return UbseCliStringPromptReply(URMA_EMPTY_ERROR);
    }
    return UbseCliProcessUrmaQosTable(ubse_de_serial, urmaSize);
}

std::vector<std::string> UbseCliRegUrmaModule::ParseCommaSeparatedDeviceList(const std::string& deviceStr)
{
    std::vector<std::string> result;
    if (deviceStr.empty()) {
        return result;
    }

    std::stringstream ss(deviceStr);
    std::string token;

    while (std::getline(ss, token, ',')) {
        // 去除首尾空格
        size_t first = token.find_first_not_of(' ');
        if (first == std::string::npos) {
            continue; // 整个token都是空格，跳过
        }
        size_t last = token.find_last_not_of(' ');
        std::string trimmed = token.substr(first, last - first + 1);
        if (!trimmed.empty()) {
            result.push_back(std::move(trimmed));
        }
    }

    return result;
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::ParseAndValidateUrmaParams(
    const std::map<std::string, std::string>& params, uint32_t& nodeId, std::vector<std::string>& deviceNameList)
{
    auto urmaNodeCli = params.find(URMA_NODE_OPT);
    auto urmaDeviceCli = params.find(URMA_DEVICE_OPT);
    // 处理节点参数（可选）
    if (urmaNodeCli == params.end()) {
        nodeId = UINT32_MAX; // 默认值：表示查询本节点的 URMA 信息
    } else {
        try {
            nodeId = static_cast<uint32_t>(std::stoul(urmaNodeCli->second));
        } catch (const std::exception& e) {
            return UbseCliStringPromptReply(URMA_NODE_ID_ERROR);
        }
    }
    // 处理设备名称参数（可选）
    if (urmaDeviceCli == params.end()) {
        deviceNameList.clear(); // 不传设备参数，deviceNameList 为空
    } else {
        deviceNameList = ParseCommaSeparatedDeviceList(urmaDeviceCli->second);
        if (deviceNameList.empty()) {
            return UbseCliStringPromptReply(URMA_NAME_FORMAT_ERROR);
        }
    }
    // 校验：显式传入 UINT32_MAX 或非法类型值视为错误
    if (nodeId == UINT32_MAX && urmaNodeCli != params.end()) {
        return UbseCliStringPromptReply(URMA_NODE_ID_ERROR);
    }
    return nullptr; // 解析成功
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::UbseQueryUrmaDevInfoFunc(
    [[maybe_unused]] const std::map<std::string, std::string>& params)
{
    uint32_t nodeId{};
    std::vector<std::string> deviceNameList{};
    auto parseResult = ParseAndValidateUrmaParams(params, nodeId, deviceNameList);
    if (parseResult != nullptr) {
        return parseResult;
    }
    UbseSerialization ubse_req_serial;
    ubse_req_serial << nodeId;
    // 序列化设备名称列表
    uint32_t deviceListSize = static_cast<uint32_t>(deviceNameList.size());
    ubse_req_serial << deviceListSize;
    for (const auto& deviceName : deviceNameList) {
        ubse_req_serial << deviceName;
    }
    if (!ubse_req_serial.Check()) {
        return UbseCliStringPromptReply(URMA_INTERNAL_ERROR);
    }
    ubse_api_buffer_t ubse_req_buffer{ubse_req_serial.GetBuffer(), static_cast<uint32_t>(ubse_req_serial.GetLength())};
    ubse_api_buffer_t ubse_res_buffer{};
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_CLI_DEV_GET, &ubse_req_buffer, &ubse_res_buffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
    if (ret == UBSE_ERR_NOT_EXIST) {
        return UbseCliStringPromptReply(URMA_NODE_ID_ERROR);
    }
    if (ret == UBSE_ERROR_INVAL) {
        return UbseCliStringPromptReply(URMA_NODE_STATE_ERROR);
    }
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(URMA_INTERNAL_ERROR);
    }
    UbseDeSerialization ubse_de_serial(ubse_res_buffer.buffer, ubse_res_buffer.length);
    uint32_t urmaSize{};
    ubse_de_serial >> urmaSize;
    if (!ubse_de_serial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    if (urmaSize == 0) {
        return UbseCliStringPromptReply(URMA_EMPTY_ERROR);
    }
    return UbseCliProcessUrmaDevInfoTable(ubse_de_serial, urmaSize);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::UbseActivateUrmaDevInfoFunc(
    [[maybe_unused]] const std::map<std::string, std::string>& params)
{
    auto urmaNode = params.find(URMA_NODE_OPT);
    if (urmaNode == params.end()) {
        return UbseCliStringPromptReply(URMA_NODE_ID_ERROR);
    }
    uint32_t nodeId;
    try {
        nodeId = static_cast<uint32_t>(std::stoul(urmaNode->second));
    } catch (const std::exception& e) {
        return UbseCliStringPromptReply(URMA_NODE_ID_ERROR);
    }
    auto urmaName = params.find(URMA_DEVICE_OPT);
    if (urmaName == params.end()) {
        return UbseCliStringPromptReply(URMA_DEVICE_OPT + " is required.");
    }
    UbseSerialization ubse_req_serial;
    ubse_req_serial << std::to_string(nodeId) << urmaName->second;
    if (!ubse_req_serial.Check()) {
        return UbseCliStringPromptReply(SERIALIZATION_ERROR);
    }
    ubse_api_buffer_t ubse_req_buffer{ubse_req_serial.GetBuffer(), static_cast<uint32_t>(ubse_req_serial.GetLength())};
    ubse_api_buffer_t ubse_res_buffer{};
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_CLI_DEV_ACTIVATE, &ubse_req_buffer, &ubse_res_buffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
    if (ret == UBSE_ERROR_INVAL) {
        return UbseCliStringPromptReply(URMA_NODE_STATE_ERROR);
    }
    if (ret == UBSE_ERROR_AGAIN) {
        return UbseCliStringPromptReply(URMA_AGAIN_ERROR);
    }
    if (ret == UBSE_ERROR_DESERIALIZE_FAILED) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(URMA_INTERNAL_ERROR);
    }
    std::string result = "Activate urma node " + std::to_string(nodeId) + " successfully.";
    return UbseCliStringPromptReply(result);
}
} // namespace ubse::cli::reg