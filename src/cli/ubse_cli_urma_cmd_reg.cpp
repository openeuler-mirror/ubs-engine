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

#include "ubse_cli_urma_cmd_reg.h"
#include <netinet/in.h>
#include <securec.h>
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
static const std::string URMA_INTERNAL_ERROR = "ERROR: Internal error.";
static const std::string URMA_NODE_ID_ERROR =
    "ERROR: Invalid request param,The option is as follow: node-id(1 ~ max node-id).";
static const std::string URMA_EMPTY_ERROR = "ERROR: The urma List is empty.";
static const std::string URMA_QUERY_OPTION_DES =
    "Query urma information by node-id, the option is as follow: node-id(1 ~ max node-id).";
static const std::string URMA_QOS_QUERY_OPTION_DES =
    "Query urma-qos information by node-id, the option is as follow: node-id(1 ~ max node-id).";
static const std::string URMA_CLI_URMA_STATUS_ERROR = "ERROR: Invalid URMA status.";

void UbseCliRegUrmaModule::UbseCliSignUp()
{
    this->cmd.emplace_back(UbseCliQueryUrmaQos());
    this->cmd.emplace_back(UbseCliQueryUrmaDevInfo());
    return;
}
UbseCliCommandInfo UbseCliRegUrmaModule::UbseCliQueryUrmaQos()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("display")
        .UbseCliSetType("urma-qos")
        .UbseCliAddOption("n", URMA_NODE_OPT, URMA_QOS_QUERY_OPTION_DES)
        .UbseCliSetFunc(UbseQueryUrmaQosFunc);
    return builder.UbseCliBuild();
}

UbseCliCommandInfo UbseCliRegUrmaModule::UbseCliQueryUrmaDevInfo()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("display")
        .UbseCliSetType("urma")
        .UbseCliAddOption("n", URMA_NODE_OPT, URMA_QUERY_OPTION_DES)
        .UbseCliSetFunc(UbseQueryUrmaDevInfoFunc);
    return builder.UbseCliBuild();
}

constexpr uint32_t URMA_INFO_STATUS_NUM = 3;
std::array<std::string, URMA_INFO_STATUS_NUM> urmaStatusArray = {"active", "inactive", "unknow"};

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::UbseCliProcessUrmaDevInfoTable(
    UbseDeSerialization &ubse_de_serial, uint32_t urma_size)
{
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_4, UBSE_CLI_NUM_4 * UBSE_CLI_NUM_10);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "bonding-name");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "fe1-name");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "fe2-name");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "status");
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    for (uint32_t i = 0; i < urma_size; i++) {
        row = variable_cell_builder.UbseCliAddRow();
        std::string uramName;
        std::string fe1Name;
        std::string fe2Name;
        uint32_t urmaStatus;
        ubse_de_serial >> uramName >> fe1Name >> fe2Name >> urmaStatus;
        if (!ubse_de_serial.Check()) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        if (urmaStatus >= urmaStatusArray.size()) {
            return UbseCliStringPromptReply(URMA_CLI_URMA_STATUS_ERROR);
        }

        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, uramName);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, fe1Name);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, fe2Name);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, urmaStatusArray[urmaStatus]);
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}
std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::UbseCliProcessUrmaQosTable(uint32_t nodeId,
                                                                                    UbseDeSerialization &ubse_de_serial,
                                                                                    uint32_t urma_size)
{
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_5, UBSE_CLI_NUM_4 * UBSE_CLI_NUM_10);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "uram-dev");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "minBandWidth(Gbps)");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "maxBandWidth(Gbps)");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "fe1-name");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, "fe2-name");
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    for (uint32_t i = 0; i < urma_size; i++) {
        row = variable_cell_builder.UbseCliAddRow();
        std::string uramName;
        std::string fe1Name;
        std::string fe2Name;
        uint32_t urmaStatus;
        ubse_de_serial >> uramName >> fe1Name >> fe2Name >> urmaStatus;
        if (!ubse_de_serial.Check()) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        /* 继续查询Qos带宽 */
        UbseSerialization ubse_req_serial;
        ubse_req_serial << nodeId << uramName;
        if (!ubse_req_serial.Check()) {
            return UbseCliStringPromptReply(URMA_INTERNAL_ERROR);
        }
        ubse_api_buffer_t request_buffer{ubse_req_serial.GetBuffer(),
                                         static_cast<uint32_t>(ubse_req_serial.GetLength())};
        ubse_api_buffer_t response_buffer = {NULL, 0};
        auto ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_CLI_QOS_GET, &request_buffer, &response_buffer);
        UbseCliBufferGuard ubseCliBufferGuard(response_buffer);
        if (ret != UBSE_OK) {
            return UbseCliStringPromptReply(URMA_INTERNAL_ERROR);
        }
        uint32_t minBandWidth = 0;
        uint32_t maxBandWidth = 0;
        UbseDeSerialization ubse_de_serial_qos(response_buffer.buffer, response_buffer.length);
        ubse_de_serial_qos >> minBandWidth >> maxBandWidth;
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, uramName);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, std::to_string(minBandWidth));
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, std::to_string(maxBandWidth));
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, fe1Name);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, fe2Name);
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::UbseQueryUrmaQosFunc([
    [maybe_unused]] const std::map<std::string, std::string> &params)
{
    auto urmaNode = params.find(URMA_NODE_OPT);
    if (urmaNode == params.end()) {
        return UbseCliStringPromptReply(URMA_NODE_ID_ERROR);
    }
    uint32_t urmaType = 0; // 暂时只支持独享
    uint32_t nodeId;
    try {
        nodeId = static_cast<uint32_t>(std::stoul(urmaNode->second));
    } catch (const std::exception &e) {
        return UbseCliStringPromptReply(URMA_NODE_ID_ERROR);
    }
    UbseSerialization ubse_req_serial;
    ubse_req_serial << nodeId << urmaType;
    if (!ubse_req_serial.Check()) {
        return UbseCliStringPromptReply(URMA_INTERNAL_ERROR);
    }
    ubse_api_buffer_t ubse_req_buffer{ubse_req_serial.GetBuffer(), static_cast<uint32_t>(ubse_req_serial.GetLength())};
    ubse_api_buffer_t ubse_res_buffer{};
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_CLI_DEV_GET, &ubse_req_buffer, &ubse_res_buffer);
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
    return UbseCliProcessUrmaQosTable(nodeId, ubse_de_serial, urmaSize);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegUrmaModule::UbseQueryUrmaDevInfoFunc([
    [maybe_unused]] const std::map<std::string, std::string> &params)
{
    auto urmaNode = params.find(URMA_NODE_OPT);
    if (urmaNode == params.end()) {
        return UbseCliStringPromptReply(URMA_NODE_ID_ERROR);
    }
    uint32_t nodeId;
    try {
        nodeId = static_cast<uint32_t>(std::stoul(urmaNode->second));
    } catch (const std::exception &e) {
        return UbseCliStringPromptReply(URMA_NODE_ID_ERROR);
    }
    UbseSerialization ubse_req_serial;
    uint32_t urmaType = 0; // 暂时只支持独享

    ubse_req_serial << nodeId << urmaType;
    if (!ubse_req_serial.Check()) {
        return UbseCliStringPromptReply(URMA_INTERNAL_ERROR);
    }
    ubse_api_buffer_t ubse_req_buffer{ubse_req_serial.GetBuffer(), static_cast<uint32_t>(ubse_req_serial.GetLength())};
    ubse_api_buffer_t ubse_res_buffer{};
    uint32_t ret = ubse_invoke_call(UBSE_URMA, UBSE_URMA_CLI_DEV_GET, &ubse_req_buffer, &ubse_res_buffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
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

} // namespace ubse::cli::reg