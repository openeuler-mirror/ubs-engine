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
#include "ubse_cli_mem_cmd_reg.h"
#include "ubse_cli_buffer_guard.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_serial_util.h"

namespace ubse::cli::reg {
UBSE_CLI_REGISTER_MODULE("CLI_MEM_MODULE", UbseCliRegMemModule);
using namespace ubse::cli::framework;
using namespace ubse::serial;

const std::string OPTION = "type";
const std::string OPENTION_TIP = "Queries the memory information of a specified option. The options are as follows: "
    "node_borrow, borrow_detail.";
const std::string INVALID_OPTION_TIP =
    "ERROR: Invalid request param,The options are as follows: node_borrow, borrow_detail.";

static const std::string NODE_BORROW_EMPTY = "INFO: The node borrow information is empty.";
static const std::string BORROW_DETAIL_EMPTY = "INFO: The borrow detail information is empty.";

static const std::string DE_SERIALIZATION_ERROR = "ERROR: Deserialization failed.";
static const std::string MEMORY_INTERNAL_ERROR = "ERROR: Internal error with error code ";
static const std::string MEMORY_EMPTY_ERROR = "ERROR: Failed to obtain memory information";

static const uint16_t MEM_MODULE_CODE = 0x0001;
static const uint16_t MEM_NODE_BORROW_OP_CODE = 0x0011;
static const uint16_t MEM_BORROW_DETAIL_OP_CODE = 0x0012;

const int ONE_M = 1024 * 1024;

std::string ConvertStrToMbStr(const char *data)
{
    if (data == nullptr) {
        return std::to_string(0);
    }
    char *remain = nullptr;
    uint64_t num = std::strtoull(data, &remain, 10L);
    if (remain == data) {
        return std::to_string(0);
    }
    return std::to_string(num / ONE_M);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliQueryNodeBorrowInfo()
{
    UbseSerialization ubse_req_serial(8); // Create a blank request.
    ubse_api_buffer_t ubse_req_buffer{ ubse_req_serial.GetBuffer(),
        static_cast<uint32_t>(ubse_req_serial.GetLength()) };
    ubse_api_buffer_t ubse_res_buffer{};
    uint32_t ret = ubse_invoke_call(MEM_MODULE_CODE, MEM_NODE_BORROW_OP_CODE, &ubse_req_buffer, &ubse_res_buffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(std::string("ERROR: Internal error with error code " + std::to_string(ret)));
    }
    UbseDeSerialization ubse_de_serial(ubse_res_buffer.buffer, ubse_res_buffer.length);
    size_t node_borrow_account_size{};
    ubse_de_serial >> node_borrow_account_size;
    if (!ubse_de_serial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    if (node_borrow_account_size == 0) {
        return UbseCliStringPromptReply(NODE_BORROW_EMPTY);
    }
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_3, UBSE_CLI_NUM_10 + UBSE_CLI_NUM_10);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "from");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "size");
    variable_cell_builder.UbseCliAddBottomlineSeparate();

    for (size_t i = 0; i < node_borrow_account_size; i++) {
        row = variable_cell_builder.UbseCliAddRow();
        std::string ubs_node{};
        std::string ubs_from{};
        std::string size{};
        ubse_de_serial >> ubs_node >> ubs_from >> size;
        if (!ubse_de_serial.Check()) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, ubs_node);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, ubs_from);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, size);
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

struct UbseOneBorrowDetailInfo {
    std::string name{};
    std::string type{};
    std::string borrowNode{};
    std::string borrowSize{};
    std::string lendNode{};
    std::string lendSize{};
    std::string status{};
};

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliProcessBorrowDetailData(
    UbseDeSerialization &ubse_de_serial, size_t node_borrow_detail_size)
{
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_7, UBSE_CLI_NUM_10 + UBSE_CLI_NUM_10);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "name");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "type");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "borrow_node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "borrow_size");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, "lend_node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, "lend_size");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, "status");
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    for (size_t i = 0; i < node_borrow_detail_size; i++) {
        row = variable_cell_builder.UbseCliAddRow();
        UbseOneBorrowDetailInfo ubse_one_borrow_detail_info;
        ubse_de_serial >> ubse_one_borrow_detail_info.name >> ubse_one_borrow_detail_info.type >>
            ubse_one_borrow_detail_info.borrowNode >> ubse_one_borrow_detail_info.borrowSize >>
            ubse_one_borrow_detail_info.lendNode >> ubse_one_borrow_detail_info.lendSize >>
            ubse_one_borrow_detail_info.status;
        if (!ubse_de_serial.Check()) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, ubse_one_borrow_detail_info.name);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, ubse_one_borrow_detail_info.type);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, ubse_one_borrow_detail_info.borrowNode);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4,
            ConvertStrToMbStr(ubse_one_borrow_detail_info.borrowSize.c_str()));
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, ubse_one_borrow_detail_info.lendNode);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_6,
            ConvertStrToMbStr(ubse_one_borrow_detail_info.lendSize.c_str()));
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, ubse_one_borrow_detail_info.status);
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliQueryBorrowDetailInfo()
{
    UbseSerialization ubse_req_serial(8);
    ubse_api_buffer_t ubse_req_buffer{ ubse_req_serial.GetBuffer(),
        static_cast<uint32_t>(ubse_req_serial.GetLength()) };
    ubse_api_buffer_t ubse_res_buffer{};
    uint32_t ret = ubse_invoke_call(MEM_MODULE_CODE, MEM_BORROW_DETAIL_OP_CODE, &ubse_req_buffer, &ubse_res_buffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(std::string("ERROR: Internal error with error code " + std::to_string(ret)));
    }
    UbseDeSerialization ubse_de_serial(ubse_res_buffer.buffer, ubse_res_buffer.length);
    size_t node_borrow_detail_size{};
    ubse_de_serial >> node_borrow_detail_size;
    if (!ubse_de_serial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    if (node_borrow_detail_size == 0) {
        return UbseCliStringPromptReply(BORROW_DETAIL_EMPTY);
    }
    return UbseCliProcessBorrowDetailData(ubse_de_serial, node_borrow_detail_size);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliMemQueryFunc([
    [maybe_unused]] const std::map<std::string, std::string> &params)
{
    auto it_kind = params.find(OPTION);
    std::string bodyString;
    if (it_kind == params.end()) {
        return UbseCliStringPromptReply(INVALID_OPTION_TIP);
    } else if (it_kind->second == "node_borrow") {
        return UbseCliQueryNodeBorrowInfo();
    } else if (it_kind->second == "borrow_detail") {
        return UbseCliQueryBorrowDetailInfo();
    }
    return UbseCliStringPromptReply(INVALID_OPTION_TIP);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliCheckMemoryStatusFunc([
    [maybe_unused]] const std::map<std::string, std::string> &params)
{
    UbseSerialization ubse_req_serial(UBSE_CLI_NUM_8);
    ubse_api_buffer_t ubse_req_buffer{ubse_req_serial.GetBuffer(), static_cast<uint32_t>(ubse_req_serial.GetLength())};
    ubse_api_buffer_t ubse_res_buffer{};
    uint32_t ret = ubse_invoke_call(UBSE_MEM, USBE_MEM_CLI_CHECK_STATUS, &ubse_req_buffer, &ubse_res_buffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(MEMORY_INTERNAL_ERROR + std::to_string(ret));
    }
    UbseDeSerialization ubse_de_serial(ubse_res_buffer.buffer, ubse_res_buffer.length);
    size_t size = 0;
    ubse_de_serial >> size;
    if (!ubse_de_serial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    if (size == 0) {
        return UbseCliStringPromptReply(MEMORY_EMPTY_ERROR);
    }
    return UbseCliMemoryStatusData(ubse_de_serial, size);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliMemoryStatusData(UbseDeSerialization &ubse_de_serial,
    size_t size)
{
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_2, UBSE_CLI_NUM_2 * UBSE_CLI_NUM_10);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "status");
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    for (size_t i = 0; i < size; i++) {
        row = variable_cell_builder.UbseCliAddRow();
        std::string node{};
        std::string status{};
        ubse_de_serial >> node >> status;
        if (!ubse_de_serial.Check()) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, node);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, status);
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

UbseCliCommandInfo UbseCliRegMemModule::UbseCliQueryMem()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("display")
        .UbseCliSetType("memory")
        .UbseCliAddOption("t", OPTION, OPENTION_TIP)
        .UbseCliSetFunc(UbseCliMemQueryFunc);
    return builder.UbseCliBuild();
}

UbseCliCommandInfo UbseCliRegMemModule::UbseCliCheckMemoryStatus()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("check")
        .UbseCliSetType("memory")
        .UbseCliSetFunc(UbseCliCheckMemoryStatusFunc);
    return builder.UbseCliBuild();
}

void UbseCliRegMemModule::UbseCliSignUp()
{
    this->cmd.emplace_back(UbseCliQueryMem());
    this->cmd.emplace_back(UbseCliCheckMemoryStatus());
}
} // namespace ubse::cli::reg
