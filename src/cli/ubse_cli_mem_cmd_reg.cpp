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
#include <chrono>
#include <future>
#include <regex>
#include <thread>

#include "ubse_cli_buffer_guard.h"
#include "ubse_cli_mem_attach.h"
#include "ubse_cli_mem_create.h"
#include "ubse_cli_mem_detach.h"
#include "ubse_cli_mem_pid.h"
#include "ubse_cli_mem_query.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_mem_controller.h"
#include "ubse_serial_util.h"
#include "ubse_str_util.h"
#include "src/sdk/c/include/ubs_error.h"

namespace ubse::cli::reg {
UBSE_CLI_REGISTER_MODULE("CLI_MEM_MODULE", UbseCliRegMemModule);
using namespace ubse::cli::framework;
using namespace ubse::serial;
using namespace ubse::mem::controller;

// public option reg
static const std::string PUBLIC_NAME_OPTION = "name";
// public option desc
static const std::string PUBLIC_NAME_OPTION_TIP =
    "Input a unique name. The name must not exceed 47 characters "
    "and can only include English letters, numbers, dots, colons, underscores, and hyphens.";
// public option input error
static const std::string PUBLIC_NAME_OPTION_REQUIRED =
    "ERROR: The request option -n or --name is required, and the supported name must not exceed 47 characters and can "
    "only include English letters, numbers, dots, colons, underscores, and hyphens.";
static const std::string PUBLIC_NAME_PARAM_INVALID =
    "ERROR: Invalid name. The name must not exceed 47 characters and can only include English letters, numbers, dots, "
    "colons, underscores, and hyphens.";

// display memory option reg
static const std::string DISPLAY_MEM_T_OPTION = "type";
static const std::string DISPLAY_MEM_BT_OPTION = "borrow-type";
static const std::string DISPLAY_MEM_N_OPTION = "name";
// display memory option desc
static const std::string DISPLAY_MEM_TYPE_OPTION_TIP =
    "Query the memory information of a specified option. The option is as follows: node_borrow, borrow_detail, "
    "node_lend, numa_status, config.";
static const std::string DISPLAY_MEM_BORROW_TYPE_OPTION_TIP =
    "Input the borrow-type to filter memory account. The option is as follows: "
    "fd, numa, share. Supported only when the type parameter is borrow_detail.";
static const std::string DISPLAY_MEM_NAME_OPTION_TIP =
    "Input a unique name to filter memory account. The name must not exceed 47 characters and can only include English "
    "letters, numbers, dots, colons, underscores, and hyphens. Supported only when the type parameter is "
    "borrow_detail.";
// display memory option input error
static const std::string DISPLAY_MEM_TYPE_OPTION_REQUIRED =
    "ERROR: The request option -t or --type is required, and the supported param is as follows: node_borrow, "
    "borrow_detail, node_lend, numa_status, config, pidInfo.";
static const std::string DISPLAY_MEM_TYPE_PARAM_INVALID =
    "ERROR: Invalid type. The supported param is as follows: node_borrow, "
    "borrow_detail, node_lend, numa_status, config, pidInfo.";
static const std::string DISPLAY_MEM_BORROW_TYPE_PARAM_INVALID =
    "ERROR: Invalid borrow-type. The supported param is as follows: numa, fd, share.";
static const std::string DISPLAY_MEM_NAME_PARAM_INVALID = PUBLIC_NAME_PARAM_INVALID;

const std::string PID_OPTION = "pid";
const std::string PID_OPTION_TIP = "Input the pid.";
const std::string EVICT_THRESHOLD_OPTION = "evict-thresh";
const std::string TARGET_EVICT_THRESHOLD_OPTION = "target-evict-thresh";
const std::string RECLAIM_THRESHOLD_OPTION = "reclaim-thresh";
const std::string THRESHOLD_OPTION_TIP = "Input the threshold";
const std::string SRC_NUMAID_OPTION = "src-numa";
const std::string SRC_NUMAID_OPTION_TIP = "Input the src-numa";
const std::string SIZE_OPTION = "size";
const std::string SIZE_OPTION_TIP = "Specify the size. The rang is from 128M to 256G. "
                                    "The example is : 1G or 1024M";
const std::string INVALID_SIZE_OPTION_TIP = "ERROR: Invalid size param. Please check the form.";

static const std::string DISPLAY_MEM_NAME_OPTION_UNSUPPORT =
    "ERROR: The -n or --name option only supports when the -t or --type parameter is borrow_detail.";
static const std::string DISPLAY_MEM_BORROW_TYPE_OPTION_UNSUPPORT =
    "ERROR: The -bt or --borrow-type option only supports when the -t or --type parameter is borrow_detail.";

// create memory option reg
static const std::string CREATE_MEM_T_OPTION = "type";
static const std::string CREATE_MEM_L_OPTION = "link";
static const std::string CREATE_MEM_S_OPTION = "size";
static const std::string CREATE_MEM_N_OPTION = PUBLIC_NAME_OPTION;
static const std::string CREATE_MEM_R_OPTION = "region";
// create memory option desc
static const std::string CREATE_MEM_TYPE_OPTION_TIP = "Specify the type. The option is as follows: numa, fd, share.";
static const std::string CREATE_MEM_LINK_OPTION_TIP =
    "Specify the link. The format is: nodeID/socketID/portID-nodeID/socketID/portID (e.g., 1/36/0-2/36/0). Supported "
    "only when the type parameter is numa.";
static const std::string CREATE_MEM_SIZE_OPTION_TIP =
    "Specify the size. The minimum allowed size is 4M. (e.g., 128M,1G).";
static const std::string CREATE_MEM_NAME_OPTION_TIP = PUBLIC_NAME_OPTION_TIP;
static const std::string CREATE_MEM_REGION_OPTION_TIP =
    "Specify the shared region node IDs. The format is: node1,node2 "
    "(e.g., 1,2). Supported only when the type parameter is share.";
// create memory option input error
static const std::string CREATE_MEM_NAME_OPTION_REQUIRED = PUBLIC_NAME_OPTION_REQUIRED;
static const std::string CREATE_MEM_NAME_PARAM_INVALID = PUBLIC_NAME_PARAM_INVALID;
static const std::string CREATE_MEM_TYPE_OPTION_REQUIRED =
    "ERROR: The request option -t or --type is required, and the supported param is as follows: numa, fd, share.";
static const std::string CREATE_MEM_TYPE_PARAM_INVALID =
    "ERROR: Invalid type. The supported param is as follows: numa, fd, share.";
static const std::string CREATE_MEM_SIZE_OPTION_REQUIRED =
    "ERROR: The request option -s or --size is required, and the minimum allowed size is 4M. (e.g., 128M,1G)";
static const std::string CREATE_MEM_SIZE_PARAM_INVALID =
    "ERROR: Invalid size. The minimum allowed size is 4M. (e.g., 128M,1G)";
static const std::string CREATE_MEM_LINK_PARAM_INVALID =
    "ERROR: Invalid link. The link must be hyphen-separated nodeID/socketID/portID pairs (e.g., 1/36/0-2/36/0).";
static const std::string CREATE_MEM_REGION_PARAM_INVALID =
    "ERROR: Invalid region. The region must be comma-separated numeric node IDs (e.g., 1,2,3).";
static const std::string CREATE_MEM_LINK_OPTION_UNSUPPORT =
    "ERROR: The -l or --link option only supports when the -t or --type parameter is numa.";
static const std::string CREATE_MEM_REGION_OPTION_UNSUPPORT =
    "ERROR: The -r or --region option only supports when the -t or --type parameter is share.";

// delete memory option reg
static const std::string DELETE_MEM_N_OPTION = PUBLIC_NAME_OPTION;
static const std::string DELETE_MEM_T_OPTION = "type";
// delete memory option desc
static const std::string DELETE_MEM_NAME_OPTION_TIP = PUBLIC_NAME_OPTION_TIP;
static const std::string DELETE_MEM_TYPE_OPTION_TIP =
    "Input the type to delete memory. The default value is numa. The option is as follows: fd, numa, share, addr.";
// delete memory option input error
static const std::string DELETE_MEM_NAME_OPTION_REQUIRED = PUBLIC_NAME_OPTION_REQUIRED;
static const std::string DELETE_MEM_NAME_PARAM_INVALID = PUBLIC_NAME_PARAM_INVALID;
static const std::string DELETE_MEM_TYPE_PARAM_INVALID =
    "ERROR: Invalid type. The supported param is as follows: numa, fd, share, addr.";

// attach memory option reg
static const std::string ATTACH_MEM_N_OPTION = PUBLIC_NAME_OPTION;
// attach memory option desc
static const std::string ATTACH_MEM_NAME_OPTION_TIP = PUBLIC_NAME_OPTION_TIP;
// attach memory option input error
static const std::string ATTACH_MEM_NAME_OPTION_REQUIRED = PUBLIC_NAME_OPTION_REQUIRED;
static const std::string ATTACH_MEM_NAME_PARAM_INVALID = PUBLIC_NAME_PARAM_INVALID;

// detach memory option reg
static const std::string DETACH_MEM_N_OPTION = PUBLIC_NAME_OPTION;
// detach memory option desc
static const std::string DETACH_MEM_NAME_OPTION_TIP = PUBLIC_NAME_OPTION_TIP;
// detach memory option input error
static const std::string DETACH_MEM_NAME_OPTION_REQUIRED = PUBLIC_NAME_OPTION_REQUIRED;
static const std::string DETACH_MEM_NAME_PARAM_INVALID = PUBLIC_NAME_PARAM_INVALID;

static const std::string NODE_BORROW_EMPTY = "INFO: The node borrow information is empty.";
static const std::string BORROW_DETAIL_EMPTY = "INFO: The borrow detail information is empty.";

static const std::string SERIALIZATION_ERROR = "ERROR: Serialization failed.";
static const std::string DE_SERIALIZATION_ERROR = "ERROR: Deserialization failed.";
static const std::string MEMORY_EMPTY_ERROR = "ERROR: Failed to obtain memory information";
static const std::string SET_TIMER_ERROR = "ERROR: Set timer failed. ";

static const uint16_t MEM_MODULE_CODE = UBSE_MEM;
static const uint16_t MEM_NODE_BORROW_OP_CODE = UBSE_MEM_CLI_NODE_BORROW;
static const uint16_t MEMORY_DELETE_OP_CODE = UBSE_MEM_CLI_DELETE_MEMORY;
static const uint16_t MEMORY_NUMA_STATE_QUERY_OP_CODE = UBSE_MEM_CLI_NUMA_STATE_QUERY;
const int8_t MEM_SUCCESS_CODE = UBSE_OK;
const int MAX_NAME_LENGTH = 47;
const uint32_t REQUEST_BUFFER_CAPACITY = 8;
const int8_t RETRY_WAIT_TIME = 10;
constexpr size_t NODE_LENGTH = 80; // hostname(slot_id), hostname最长为64

std::string FormatHostnameSlot(const std::string& hostname, uint32_t slotId)
{
    return (hostname.empty() ? "-" : hostname) + "(" + std::to_string(slotId) + ")";
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliQueryNodeBorrowInfo()
{
    UbseSerialization ubse_req_serial(REQUEST_BUFFER_CAPACITY); // Create a blank request.
    ubse_api_buffer_t ubse_req_buffer{ubse_req_serial.GetBuffer(), static_cast<uint32_t>(ubse_req_serial.GetLength())};
    ubse_api_buffer_t ubse_res_buffer{};
    uint32_t ret = ubse_invoke_call(MEM_MODULE_CODE, MEM_NODE_BORROW_OP_CODE, &ubse_req_buffer, &ubse_res_buffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(GetErrorMessage(ret));
    }
    UbseDeSerialization ubse_de_serial(ubse_res_buffer.buffer, ubse_res_buffer.length);
    size_t node_borrow_account_size{};
    ubse_de_serial >> array_len_capture(node_borrow_account_size);
    if (!ubse_de_serial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    if (node_borrow_account_size == 0) {
        return UbseCliStringPromptReply(NODE_BORROW_EMPTY);
    }
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_3, NODE_LENGTH);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "lend");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "size");
    variable_cell_builder.UbseCliAddBottomlineSeparate();

    for (size_t i = 0; i < node_borrow_account_size; i++) {
        row = variable_cell_builder.UbseCliAddRow();
        uint32_t borrowSlotId{};
        std::string borrowHostname{};
        uint32_t lendSlotId{};
        std::string lendHostname{};
        uint64_t size{};
        ubse_de_serial >> borrowSlotId >> borrowHostname >> lendSlotId >> lendHostname >> size;
        if (!ubse_de_serial.Check()) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, FormatHostnameSlot(borrowHostname, borrowSlotId));
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, FormatHostnameSlot(lendHostname, lendSlotId));
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, std::to_string(size));
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliQueryNodeLendInfo()
{
    ubse_api_buffer_t ubse_req_buffer{nullptr, 0};
    ubse_api_buffer_t ubse_res_buffer{};
    uint32_t ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_CLI_NODE_LEND, &ubse_req_buffer, &ubse_res_buffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(GetErrorMessage(ret));
    }
    UbseDeSerialization ubse_de_serial(ubse_res_buffer.buffer, ubse_res_buffer.length);
    size_t node_borrow_account_size{};
    ubse_de_serial >> array_len_capture(node_borrow_account_size);
    if (!ubse_de_serial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    if (node_borrow_account_size == 0) {
        return UbseCliStringPromptReply(NODE_BORROW_EMPTY);
    }
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_3, NODE_LENGTH);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "borrow");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "size");
    variable_cell_builder.UbseCliAddBottomlineSeparate();

    for (size_t i = 0; i < node_borrow_account_size; i++) {
        row = variable_cell_builder.UbseCliAddRow();
        uint32_t borrowSlotId{};
        std::string borrowHostname{};
        uint32_t lendSlotId{};
        std::string lendHostname{};
        uint64_t size{};
        ubse_de_serial >> borrowSlotId >> borrowHostname >> lendSlotId >> lendHostname >> size;
        if (!ubse_de_serial.Check()) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, FormatHostnameSlot(lendHostname, lendSlotId));
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, FormatHostnameSlot(borrowHostname, borrowSlotId));
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, std::to_string(size));
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

struct UbseNumaStatusInfo {
    std::string node{};
    std::string numa{};
    std::string total{};
    std::string used{};
    std::string freeSize{};
    std::string used_percent{};
};

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliProcessNumaStatusData(
    UbseDeSerialization& deSerialization, size_t numaInfoSize)
{
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_6, NODE_LENGTH);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "numa");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "total");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "used");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, "free");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, "used_percent");
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    for (size_t i = 0; i < numaInfoSize; i++) {
        row = variable_cell_builder.UbseCliAddRow();
        UbseNumaStatusInfo numaInfo{};
        deSerialization >> numaInfo.node >> numaInfo.numa >> numaInfo.total >> numaInfo.used >> numaInfo.freeSize >>
            numaInfo.used_percent;
        if (!deSerialization.Check()) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, numaInfo.node);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, numaInfo.numa);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, numaInfo.total);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, numaInfo.used);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, numaInfo.freeSize);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, numaInfo.used_percent);
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliQueryNumaStatus()
{
    ubse_api_buffer_t ubse_req_buffer{nullptr, 0};
    ubse_api_buffer_t ubse_res_buffer{};
    uint32_t ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_CLI_NUMA_STATUS, &ubse_req_buffer, &ubse_res_buffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(GetErrorMessage(ret));
    }
    UbseDeSerialization ubse_de_serial(ubse_res_buffer.buffer, ubse_res_buffer.length);
    size_t numaInfoSize{};
    ubse_de_serial >> array_len_capture(numaInfoSize);
    if (!ubse_de_serial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    if (numaInfoSize == 0) {
        return UbseCliStringPromptReply(BORROW_DETAIL_EMPTY);
    }
    return UbseCliProcessNumaStatusData(ubse_de_serial, numaInfoSize);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::QueryMemConfig()
{
    UbseSerialization ubse_req_serial(REQUEST_BUFFER_CAPACITY);
    ubse_api_buffer_t ubse_req_buffer{ubse_req_serial.GetBuffer(), static_cast<uint32_t>(ubse_req_serial.GetLength())};
    ubse_api_buffer_t ubse_res_buffer{};
    uint32_t ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_CLI_CONFIG, &ubse_req_buffer, &ubse_res_buffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(GetErrorMessage(ret));
    }
    UbseDeSerialization ubse_de_serial(ubse_res_buffer.buffer, ubse_res_buffer.length);
    size_t node_size;
    ubse_de_serial >> array_len_capture(node_size);
    if (!ubse_de_serial.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    if (node_size == 0) {
        return UbseCliStringPromptReply(NODE_BORROW_EMPTY);
    }

    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_2, NODE_LENGTH);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "isLender");
    variable_cell_builder.UbseCliAddBottomlineSeparate();

    for (size_t i = 0; i < node_size; i++) {
        row = variable_cell_builder.UbseCliAddRow();
        std::string ubs_node{};
        bool isLender{};
        ubse_de_serial >> ubs_node >> isLender;
        if (!ubse_de_serial.Check()) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, ubs_node);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, isLender ? "true" : "false");
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

bool CheckBorrowDetailType(const std::string& type)
{
    return type == "fd" || type == "numa" || type == "share";
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliMemQueryFunc(
    const std::map<std::string, std::string>& params)
{
    auto it_kind = params.find(DISPLAY_MEM_T_OPTION);
    auto it_borrow_type = params.find(DISPLAY_MEM_BT_OPTION);
    auto it_name = params.find(DISPLAY_MEM_N_OPTION);
    if (it_kind == params.end()) {
        return UbseCliStringPromptReply(DISPLAY_MEM_TYPE_OPTION_REQUIRED);
    }
    std::string kind = it_kind->second;
    if (kind == "borrow_detail") {
        UbseCliMemDisplayBorrowDetail::Filter fliter;
        std::string borrow_type{};
        if (it_borrow_type != params.end()) {
            if (!CheckBorrowDetailType(it_borrow_type->second)) {
                return UbseCliStringPromptReply(DISPLAY_MEM_BORROW_TYPE_PARAM_INVALID);
            }
            fliter.type = it_borrow_type->second;
        }
        if (it_name != params.end()) {
            if (!CheckName(it_name->second)) {
                return UbseCliStringPromptReply(DISPLAY_MEM_NAME_PARAM_INVALID);
            }
            fliter.name = it_name->second;
        }
        UbseCliMemDisplayBorrowDetail query;
        return query.UbseCliQueryBorrowDetail(fliter);
    } else {
        if (it_name != params.end()) {
            return UbseCliStringPromptReply(DISPLAY_MEM_NAME_OPTION_UNSUPPORT);
        }
        if (it_borrow_type != params.end()) {
            return UbseCliStringPromptReply(DISPLAY_MEM_BORROW_TYPE_OPTION_UNSUPPORT);
        }
        if (kind == "node_borrow") {
            return UbseCliQueryNodeBorrowInfo();
        } else if (kind == "node_lend") {
            return UbseCliQueryNodeLendInfo();
        } else if (kind == "numa_status") {
            return UbseCliQueryNumaStatus();
        } else if (kind == "config") {
            return QueryMemConfig();
        } else if (kind == "pidInfo") {
            UbseCliMemPid memPid{};
            return memPid.UbseCliPrintPidInfo();
        }
    }
    return UbseCliStringPromptReply(DISPLAY_MEM_TYPE_PARAM_INVALID);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliCheckMemoryStatusFunc(
    [[maybe_unused]] const std::map<std::string, std::string>& params)
{
    UbseSerialization ubse_req_serial(UBSE_CLI_NUM_8);
    ubse_api_buffer_t ubse_req_buffer{ubse_req_serial.GetBuffer(), static_cast<uint32_t>(ubse_req_serial.GetLength())};
    ubse_api_buffer_t ubse_res_buffer{};
    uint32_t ret = ubse_invoke_call(UBSE_MEM, UBSE_MEM_CLI_CHECK_STATUS, &ubse_req_buffer, &ubse_res_buffer);
    UbseCliBufferGuard ubseCliBufferGuard(ubse_res_buffer);
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(GetErrorMessage(ret));
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

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliMemoryStatusData(UbseDeSerialization& ubse_de_serial,
                                                                                size_t size)
{
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_3, NODE_LENGTH);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "status");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "detail");
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    for (size_t i = 0; i < size; i++) {
        row = variable_cell_builder.UbseCliAddRow();
        std::string node{};
        std::string status{};
        std::string detail{};
        ubse_de_serial >> node >> status >> detail;
        if (!ubse_de_serial.Check()) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, node);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, status);
        variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, detail);
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

UbseCliCommandInfo UbseCliRegMemModule::UbseCliQueryMem()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("display")
        .UbseCliSetType("memory")
        .UbseCliAddOption("t", DISPLAY_MEM_T_OPTION, DISPLAY_MEM_TYPE_OPTION_TIP)
        .UbseCliAddOption("bt", DISPLAY_MEM_BT_OPTION, DISPLAY_MEM_BORROW_TYPE_OPTION_TIP)
        .UbseCliAddOption("n", DISPLAY_MEM_N_OPTION, DISPLAY_MEM_NAME_OPTION_TIP)
        .UbseCliSetFunc(UbseCliMemQueryFunc);
    return builder.UbseCliBuild();
}

UbseCliCommandInfo UbseCliRegMemModule::UbseCliCheckMemoryStatus()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("check").UbseCliSetType("memory").UbseCliSetFunc(UbseCliCheckMemoryStatusFunc);
    return builder.UbseCliBuild();
}

bool LinkIsMatch(const std::string& str)
{
    const std::regex pattern(R"(^\d+/\d+/\d+-\d+/\d+/\d+$)");
    return std::regex_match(str, pattern);
}

bool SizeIsMatch(const std::string& str, size_t& size)
{
    std::regex pattern(R"(^(\d+)(G|M))");
    std::smatch match;
    if (!std::regex_match(str, match, pattern)) {
        return false;
    }
    std::string num = match[1];
    std::string unit = match[2];
    uint64_t tmpSize{0};
    ubse::utils::ConvertStrToUint64(match[1], tmpSize);
    uint64_t multiplier = 1ULL;
    if (unit == "M") {
        if (tmpSize < 4ULL) {
            return false;
        }
        multiplier = 1024ULL * 1024ULL;
    } else {
        multiplier = 1024ULL * 1024ULL * 1024ULL;
    }
    if (tmpSize > std::numeric_limits<size_t>::max() / multiplier) {
        return false; // 溢出
    }

    size = tmpSize * multiplier;
    return true;
}

bool CheckName(const std::string& name)
{
    if (name.length() > MAX_NAME_LENGTH || name.empty()) {
        return false;
    }
    for (char c : name) {
        if (!isdigit(c) && !isalpha(c) && c != '_' && c != '-' && c != '.' && c != ':') {
            return false;
        }
    }
    return true;
}

static std::string FormatMemoryInfoReply(const std::string& name, int64_t numaId, const std::string& importNode,
                                         const std::string& exportNode)
{
    std::ostringstream oss;
    oss << "name:" << name << '\n'
        << "numaId:" << numaId << '\n'
        << "import-node:" << importNode << '\n'
        << "export-node:" << exportNode << '\n';
    return oss.str();
}

static std::string MakeInternalErrorString(uint32_t errorCode)
{
    return GetErrorMessage(errorCode);
}

struct ParsedResponse {
    bool success;
    std::string name;
    UbseMemStage stage;
    int64_t numaId;
    std::string importNode;
    std::string exportNode;
    std::string errorMsg;
};

ParsedResponse ParseResponseBuffer(const ubse_api_buffer_t& responseBuffer)
{
    ParsedResponse result{};
    UbseDeSerialization deserial{responseBuffer.buffer, responseBuffer.length};
    std::string name;
    std::string state;
    try {
        deserial >> name >> state;
        if (!deserial.Check()) {
            result.success = false;
            result.errorMsg = DE_SERIALIZATION_ERROR;
            return result;
        }
        auto stageInt = std::stoi(state);
        result.stage = static_cast<UbseMemStage>(stageInt);
        result.name = std::move(name);
    } catch (const std::invalid_argument& e) {
        result.success = false;
        result.errorMsg = "ERROR: Internal error.";
        return result;
    } catch (const std::out_of_range& e) {
        result.success = false;
        result.errorMsg = "ERROR: Internal error.";
        return result;
    }
    if (result.stage == UbseMemStage::UBSE_EXIST || result.stage == UbseMemStage::UBSE_ERR_ONLY_IMPORT) {
        try {
            deserial >> result.numaId >> result.exportNode >> result.importNode;
            if (!deserial.Check()) {
                result.success = false;
                result.errorMsg = DE_SERIALIZATION_ERROR;
                return result;
            }
        } catch (...) {
            result.success = false;
            result.errorMsg = "ERROR: Failed to parse NUMA info.";
            return result;
        }
    }
    result.success = true;
    return result;
}

// 处理 IPC 超时后的轮询查询逻辑
std::shared_ptr<UbseCliResultEcho> HandleTimeoutRetry(const std::string& name)
{
    while (true) {
        UbseSerialization timeOutSerial;
        timeOutSerial << name;
        if (!timeOutSerial.Check()) {
            return UbseCliRegModule::UbseCliStringPromptReply(SERIALIZATION_ERROR);
        }
        ubse_api_buffer_t requestBuffer{timeOutSerial.GetBuffer(), static_cast<uint32_t>(timeOutSerial.GetLength())};
        ubse_api_buffer_t responseBuffer;
        uint32_t ret =
            ubse_invoke_call(MEM_MODULE_CODE, MEMORY_NUMA_STATE_QUERY_OP_CODE, &requestBuffer, &responseBuffer);
        UbseCliBufferGuard ubseCliBufferGuard(responseBuffer);
        if (ret == UBSE_IPC_ERROR_QUERY_NUMA_NOT_EXIST) {
            return UbseCliRegModule::UbseCliStringPromptReply("Delete successfully");
        }
        if (ret != UBSE_OK) {
            return UbseCliRegModule::UbseCliStringPromptReply(MakeInternalErrorString(ret));
        }
        auto parsed = ParseResponseBuffer(responseBuffer);
        if (!parsed.success) {
            return UbseCliRegModule::UbseCliStringPromptReply(parsed.errorMsg);
        }
        switch (parsed.stage) {
            case UbseMemStage::UBSE_EXIST:
                return UbseCliRegModule::UbseCliStringPromptReply(
                    FormatMemoryInfoReply(parsed.name, parsed.numaId, parsed.importNode, parsed.exportNode));
            case UbseMemStage::UBSE_ERR_ONLY_IMPORT:
                return UbseCliRegModule::UbseCliStringPromptReply(
                    FormatMemoryInfoReply(parsed.name, parsed.numaId, parsed.importNode, parsed.exportNode) +
                    "\nbut export node is fault");
            case UbseMemStage::UBSE_CREATING:
            case UbseMemStage::UBSE_DELETING:
                sleep(RETRY_WAIT_TIME);
                break;
            case UbseMemStage::UBSE_ERR_WAIT_UNEXPORT:
            case UbseMemStage::UBSE_NOT_EXIST:
                return UbseCliRegModule::UbseCliStringPromptReply("Delete successfully");
            default:
                return UbseCliRegModule::UbseCliStringPromptReply(MakeInternalErrorString(ret));
        }
    }
}

bool ParseRegionString(const std::string& regionStr, std::vector<uint32_t>& regions)
{
    if (regionStr.empty() || regionStr.front() == ',' || regionStr.back() == ',') {
        return false;
    }

    regions.clear();
    std::stringstream ss(regionStr);
    std::string item;

    while (std::getline(ss, item, ',')) {
        if (item.empty()) {
            return false;
        }

        try {
            size_t pos = 0;
            unsigned long value = std::stoul(item, &pos);
            // 检查是否完整解析
            if (pos != item.length()) {
                return false;
            }
            if (value > std::numeric_limits<uint32_t>::max()) {
                return false;
            }
            regions.push_back(static_cast<uint32_t>(value));
        } catch (...) {
            return false; // 不是有效数字
        }
    }

    return !regions.empty();
}

static std::set<std::string> GetAllowedParams(const std::string& type)
{
    std::set<std::string> allowed = {CREATE_MEM_T_OPTION, CREATE_MEM_N_OPTION, CREATE_MEM_S_OPTION};

    if (type == "numa") {
        allowed.insert(CREATE_MEM_L_OPTION);
    } else if (type == "share") {
        allowed.insert(CREATE_MEM_R_OPTION);
    }

    return allowed;
}

static bool ValidateParamsWhitelist(const std::map<std::string, std::string>& params,
                                    const std::set<std::string>& allowed, std::string& errorMsg)
{
    for (const auto& [key, value] : params) {
        if (allowed.find(key) != allowed.end()) {
            continue;
        } else if (key == CREATE_MEM_L_OPTION) {
            errorMsg = CREATE_MEM_LINK_OPTION_UNSUPPORT;
            return false;
        } else if (key == CREATE_MEM_R_OPTION) {
            errorMsg = CREATE_MEM_REGION_OPTION_UNSUPPORT;
            return false;
        }
    }
    return true;
}

static bool ValidateCommonParams(const std::map<std::string, std::string>& params, std::string& name, size_t& size,
                                 std::string& errorMsg)
{
    auto itName = params.find(CREATE_MEM_N_OPTION);
    if (itName == params.end()) {
        errorMsg = CREATE_MEM_NAME_OPTION_REQUIRED;
        return false;
    }
    if (!CheckName(itName->second)) {
        errorMsg = CREATE_MEM_NAME_PARAM_INVALID;
        return false;
    }
    name = itName->second;

    auto itSize = params.find(CREATE_MEM_S_OPTION);
    if (itSize == params.end()) {
        errorMsg = CREATE_MEM_SIZE_OPTION_REQUIRED;
        return false;
    }

    if (itSize == params.end() || !SizeIsMatch(itSize->second, size)) {
        errorMsg = CREATE_MEM_SIZE_PARAM_INVALID;
        return false;
    }

    return true;
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::CreateMemoryFunc(
    const std::map<std::string, std::string>& params)
{
    if (!UbseCliRegModule::DisableTimeoutTimer()) {
        return UbseCliStringPromptReply(SET_TIMER_ERROR);
    }
    auto it = params.find(CREATE_MEM_T_OPTION);
    if (it == params.end()) {
        return UbseCliStringPromptReply(CREATE_MEM_TYPE_OPTION_REQUIRED);
    }
    const std::string& type = it->second;
    if (type != "numa" && type != "share" && type != "fd") {
        return UbseCliStringPromptReply(CREATE_MEM_TYPE_PARAM_INVALID);
    }
    // 验证参数白名单
    auto allowedParams = GetAllowedParams(type);
    std::string errorMsg;
    if (!ValidateParamsWhitelist(params, allowedParams, errorMsg)) {
        return UbseCliStringPromptReply(errorMsg);
    }
    // 验证公共参数
    std::string name;
    size_t size{};
    if (!ValidateCommonParams(params, name, size, errorMsg)) {
        return UbseCliStringPromptReply(errorMsg);
    }
    // 根据类型创建
    UbseCliMemCreate memCreate{};
    if (type == "numa") {
        // NUMA 类型：可以有 link 参数
        auto itLink = params.find(CREATE_MEM_L_OPTION);
        if (itLink != params.end() && !LinkIsMatch(itLink->second)) {
            return UbseCliStringPromptReply(CREATE_MEM_LINK_PARAM_INVALID);
        }
        auto linkValue = (itLink != params.end()) ? itLink->second : "";
        return memCreate.UbseCliCreateNumaMem(name, size, linkValue);
    } else if (type == "share") {
        // SHARE 类型：可以有 region 参数
        auto itRegion = params.find(CREATE_MEM_R_OPTION);
        std::vector<uint32_t> region{};
        if (itRegion != params.end() && !ParseRegionString(itRegion->second, region)) {
            return UbseCliStringPromptReply(CREATE_MEM_REGION_PARAM_INVALID);
        }
        return memCreate.UbseCliCreateShareMem(name, size, region);
    } else {
        // type == "fd"
        return memCreate.UbseCliCreateFdMem(name, size);
    }
}

UbseCliCommandInfo UbseCliRegMemModule::ChangeMemory()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("change")
        .UbseCliSetType("memory")
        .UbseCliAddOption("p", PID_OPTION, PID_OPTION_TIP)
        .UbseCliAddOption("e", EVICT_THRESHOLD_OPTION, THRESHOLD_OPTION_TIP)
        .UbseCliAddOption("t", TARGET_EVICT_THRESHOLD_OPTION, THRESHOLD_OPTION_TIP)
        .UbseCliAddOption("r", RECLAIM_THRESHOLD_OPTION, THRESHOLD_OPTION_TIP)
        .UbseCliAddOption("s", SIZE_OPTION, SIZE_OPTION_TIP)
        .UbseCliAddOption("sn", SRC_NUMAID_OPTION, SRC_NUMAID_OPTION_TIP)
        .UbseCliSetFunc(PidSetThresholdFunc);
    return builder.UbseCliBuild();
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::PidUnSetFunc(const std::map<std::string, std::string>& params)
{
    if (!UbseCliRegModule::DisableTimeoutTimer()) {
        return UbseCliStringPromptReply(SET_TIMER_ERROR);
    }
    auto pid = params.find(PID_OPTION);
    if (pid == params.end()) {
        return UbseCliStringPromptReply("Invalid PID");
    }
    pid_t tmpPid = 0;
    if (utils::ConvertStrToInt(pid->second, tmpPid) != UBSE_OK) {
        return UbseCliStringPromptReply("Invalid PID");
    }
    UbseCliMemPid memPid{};
    return memPid.UbseCliUnsetPid(tmpPid);
}

UbseCliCommandInfo UbseCliRegMemModule::RemoveMemory()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("remove")
        .UbseCliSetType("memory")
        .UbseCliAddOption("p", PID_OPTION, PID_OPTION_TIP)
        .UbseCliSetFunc(PidUnSetFunc);
    return builder.UbseCliBuild();
}

bool CheckDeleteType(const std::string& type)
{
    return type == "fd" || type == "numa" || type == "addr" || type == "share";
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::DeleteMemoryFunc(
    const std::map<std::string, std::string>& params)
{
    if (!UbseCliRegModule::DisableTimeoutTimer()) {
        return UbseCliStringPromptReply(SET_TIMER_ERROR);
    }
    auto name = params.find(DELETE_MEM_N_OPTION);
    if (name == params.end()) {
        return UbseCliStringPromptReply(DELETE_MEM_NAME_OPTION_REQUIRED);
    }
    if (!CheckName(name->second)) {
        return UbseCliStringPromptReply(DELETE_MEM_NAME_PARAM_INVALID);
    }
    std::string deleteType = "numa"; // 默认使用numa
    auto type = params.find(DELETE_MEM_T_OPTION);
    if (type != params.end()) {
        if (!CheckDeleteType(type->second)) {
            return UbseCliStringPromptReply(DELETE_MEM_TYPE_PARAM_INVALID);
        }
        deleteType = type->second;
    }
    UbseSerialization serial;
    serial << name->second << deleteType;
    if (!serial.Check()) {
        return UbseCliStringPromptReply(SERIALIZATION_ERROR);
    }
    ubse_api_buffer_t reqBuffer{serial.GetBuffer(), static_cast<uint32_t>(serial.GetLength())};
    ubse_api_buffer_t resBuffer{};
    UbseCliWaitIndicator waitIndicator("Deleting memory");
    uint32_t ret = ubse_invoke_call(MEM_MODULE_CODE, MEMORY_DELETE_OP_CODE, &reqBuffer, &resBuffer);
    UbseCliBufferGuard ubseCliBufferGuard(resBuffer);
    // 处理超时错误
    if (ret == UBSE_ERR_TIMED_OUT) {
        return HandleTimeoutRetry(name->second);
    }
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(GetErrorMessage(ret));
    }
    UbseDeSerialization deserial{resBuffer.buffer, resBuffer.length};
    // 成功回显
    uint32_t errorCode{0};
    std::string errMsg;
    deserial >> errorCode >> errMsg;
    if (deserial.Check() && errorCode == MEM_SUCCESS_CODE) {
        return UbseCliStringPromptReply("Delete successfully");
    }
    // 失败回显
    if (deserial.Check() && errorCode == UBSE_ERR_NOT_SUPPORTED) {
        return UbseCliStringPromptReply(GetErrorMessage(errorCode));
    }
    return UbseCliStringPromptReply("ERROR: " + errMsg);
}

UbseCliCommandInfo UbseCliRegMemModule::CreateMemory()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("create")
        .UbseCliSetType("memory")
        .UbseCliAddOption("t", CREATE_MEM_T_OPTION, CREATE_MEM_TYPE_OPTION_TIP)
        .UbseCliAddOption("l", CREATE_MEM_L_OPTION, CREATE_MEM_LINK_OPTION_TIP)
        .UbseCliAddOption("s", CREATE_MEM_S_OPTION, CREATE_MEM_SIZE_OPTION_TIP)
        .UbseCliAddOption("n", CREATE_MEM_N_OPTION, CREATE_MEM_NAME_OPTION_TIP)
        .UbseCliAddOption("r", CREATE_MEM_R_OPTION, CREATE_MEM_REGION_OPTION_TIP)
        .UbseCliSetFunc(CreateMemoryFunc);
    return builder.UbseCliBuild();
}

UbseCliCommandInfo UbseCliRegMemModule::DeleteMemory()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("delete")
        .UbseCliSetType("memory")
        .UbseCliAddOption("n", DELETE_MEM_N_OPTION, DELETE_MEM_NAME_OPTION_TIP)
        .UbseCliAddOption("t", DELETE_MEM_T_OPTION, DELETE_MEM_TYPE_OPTION_TIP)
        .UbseCliSetFunc(DeleteMemoryFunc);
    return builder.UbseCliBuild();
}

void UbseCliRegMemModule::UbseCliSignUp()
{
    this->cmd_.emplace_back(UbseCliQueryMem());
    this->cmd_.emplace_back(UbseCliCheckMemoryStatus());
    this->cmd_.emplace_back(CreateMemory());
    this->cmd_.emplace_back(DeleteMemory());
    this->cmd_.emplace_back(ShmMemoryAttach());
    this->cmd_.emplace_back(ShmMemoryDetach());
    this->cmd_.emplace_back(ChangeMemory());
    this->cmd_.emplace_back(RemoveMemory());
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::ShmMemoryAttachFunc(
    const std::map<std::string, std::string>& params)
{
    if (!UbseCliRegModule::DisableTimeoutTimer()) {
        return UbseCliStringPromptReply(SET_TIMER_ERROR);
    }
    auto itName = params.find(ATTACH_MEM_N_OPTION);
    if (itName == params.end()) {
        return UbseCliStringPromptReply(ATTACH_MEM_NAME_OPTION_REQUIRED);
    }
    if (!CheckName(itName->second)) {
        return UbseCliStringPromptReply(ATTACH_MEM_NAME_PARAM_INVALID);
    }
    UbseCliMemAttach memAttach{};
    return memAttach.UbseCliAttachMem(itName->second);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::ShmMemoryDetachFunc(
    const std::map<std::string, std::string>& params)
{
    if (!UbseCliRegModule::DisableTimeoutTimer()) {
        return UbseCliStringPromptReply(SET_TIMER_ERROR);
    }
    auto itName = params.find(DETACH_MEM_N_OPTION);
    if (itName == params.end()) {
        return UbseCliStringPromptReply(DETACH_MEM_NAME_OPTION_REQUIRED);
    }
    if (!CheckName(itName->second)) {
        return UbseCliStringPromptReply(DETACH_MEM_NAME_PARAM_INVALID);
    }
    UbseCliMemDetach memDetach{};
    return memDetach.UbseCliDetachMem(itName->second);
}

UbseCliCommandInfo UbseCliRegMemModule::ShmMemoryAttach()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("attach")
        .UbseCliSetType("memory")
        .UbseCliAddOption("n", ATTACH_MEM_N_OPTION, ATTACH_MEM_NAME_OPTION_TIP)
        .UbseCliSetFunc(ShmMemoryAttachFunc);
    return builder.UbseCliBuild();
}

UbseCliCommandInfo UbseCliRegMemModule::ShmMemoryDetach()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("detach")
        .UbseCliSetType("memory")
        .UbseCliAddOption("n", DETACH_MEM_N_OPTION, DETACH_MEM_NAME_OPTION_TIP)
        .UbseCliSetFunc(ShmMemoryDetachFunc);
    return builder.UbseCliBuild();
}

// Threshold limits
static constexpr int THRESHOLD_PERCENT_MAX = 100;
static constexpr int PID_VALUE_MAX = 4194304;
static constexpr int MIN_EVICT_RECLAIM_DELTA = 5;

// Size conversion constants
static constexpr uint64_t BYTES_PER_KIB = 1024ULL;
static constexpr uint64_t BYTES_PER_MIB = BYTES_PER_KIB * BYTES_PER_KIB;
static constexpr uint64_t BYTES_PER_GIB = BYTES_PER_KIB * BYTES_PER_KIB * BYTES_PER_KIB;
static constexpr int DECIMAL_BASE = 10;
static constexpr int DECIMAL_DIGITS_GROUP = 3;

bool IsNonNegativeInteger(const std::string& s)
{
    if (s.empty()) {
        return false;
    }
    if (s[0] == '0' && s.size() > 1) {
        return false; // 禁止前导零（如 "01"）
    }
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
};

bool CheckThreshold(const std::string& threshold)
{
    // 检查 threshold 是否为有效非负整数
    // 将 threshold 转为整数（使用 strtol 避免异常）
    if (!IsNonNegativeInteger(threshold)) {
        return false;
    }
    int val = 0;
    if (utils::ConvertStrToInt(threshold, val) != UBSE_OK) {
        return false;
    }
    if (val <= 0 || val > THRESHOLD_PERCENT_MAX) {
        return false;
    }
    return true;
}

bool SetPidThresholdParamCheck(const std::string& pid, const std::string& evictThreshold,
                               const std::string& targetEvictThreshold, const std::string& reclaimThreshold)
{
    if (!IsNonNegativeInteger(pid)) {
        return false;
    }
    int pidVal = 0;
    if (utils::ConvertStrToInt(pid, pidVal) != UBSE_OK) {
        return false;
    }
    if (pidVal <= 0 || pidVal > PID_VALUE_MAX) {
        return false;
    }
    return CheckThreshold(evictThreshold) && CheckThreshold(targetEvictThreshold) && CheckThreshold(reclaimThreshold);
}

static bool CheckEvictReclaimDelta(const std::string& evictThreshold, const std::string& reclaimThreshold)
{
    int evict = 0;
    int reclaim = 0;
    if (utils::ConvertStrToInt(evictThreshold, evict) != UBSE_OK ||
        utils::ConvertStrToInt(reclaimThreshold, reclaim) != UBSE_OK) {
        return false;
    }
    return (evict - reclaim) >= MIN_EVICT_RECLAIM_DELTA;
}

static bool ConvertIntegerPart(uint64_t intPart, uint64_t unitMultiplier, uint64_t& size)
{
    if (unitMultiplier == 0) {
        return false;
    }
    if (intPart > SIZE_MAX / unitMultiplier) {
        return false;
    }
    size = intPart * unitMultiplier;
    return true;
}

static bool ConvertDecimalPart(uint64_t intPart, const std::sub_match<std::string::const_iterator>& decMatch,
                               uint64_t unitMultiplier, uint64_t& size)
{
    if (unitMultiplier == 0) {
        return false;
    }
    uint64_t decPart{0};
    if (ubse::utils::ConvertStrToUint64(decMatch, decPart) != UBSE_OK) {
        return false;
    }
    auto decPlaces = static_cast<int>(decMatch.length());

    uint64_t power10 = 1;
    for (int i = 0; i < decPlaces; ++i) {
        power10 *= DECIMAL_BASE;
    }

    if (intPart > SIZE_MAX / unitMultiplier) {
        return false;
    }
    uint64_t intBytes = intPart * unitMultiplier;

    if (decPart > SIZE_MAX / unitMultiplier) {
        return false;
    }
    uint64_t decBytes = decPart * unitMultiplier / power10;

    if (intBytes > SIZE_MAX - decBytes) {
        return false;
    }
    size = intBytes + decBytes;
    return true;
}

bool SizeConversion(const std::string& str, uint64_t& size)
{
    // 支持整数 (128M, 1G) 和小数 (1.5G, 0.25M)，小数最多2位
    std::regex pattern(R"(^(\d+)(\.(\d{1,2}))?([BKMG])$)");
    std::smatch match;
    if (!std::regex_match(str, match, pattern)) {
        return false;
    }

    uint64_t intPart{0};
    if (ubse::utils::ConvertStrToUint64(match[1], intPart) != UBSE_OK) {
        return false;
    }
    std::string unit = match[4];

    uint64_t unitMultiplier = 0;
    if (unit == "B") {
        unitMultiplier = 1;
    } else if (unit == "K") {
        unitMultiplier = BYTES_PER_KIB;
    } else if (unit == "M") {
        unitMultiplier = BYTES_PER_MIB;
    } else if (unit == "G") {
        unitMultiplier = BYTES_PER_GIB;
    } else {
        return false;
    }

    if (!match[DECIMAL_DIGITS_GROUP].matched) {
        return ConvertIntegerPart(intPart, unitMultiplier, size);
    }
    return ConvertDecimalPart(intPart, match[DECIMAL_DIGITS_GROUP], unitMultiplier, size);
}

process_mem::def::ProcessMemPidInfo SetPidManagerInfo(const std::string& pidStr, const std::string& evictThresholdStr,
                                                      const std::string& targetEvictThresholdStr,
                                                      const std::string& reclaimThresholdStr, uint64_t size)
{
    process_mem::def::ProcessMemPidInfo tmpInfo{};
    tmpInfo.configInfo.expectedMemoryUsage = size;
    if (utils::ConvertStrToInt(pidStr, tmpInfo.configInfo.pid) != UBSE_OK ||
        utils::ConvertStrToInt(evictThresholdStr, tmpInfo.configInfo.evictThreshold) != UBSE_OK ||
        utils::ConvertStrToInt(targetEvictThresholdStr, tmpInfo.configInfo.targetEvictThreshold) != UBSE_OK ||
        utils::ConvertStrToInt(reclaimThresholdStr, tmpInfo.configInfo.reclaimThreshold) != UBSE_OK) {
        tmpInfo.configInfo.pid = -1;
    }
    return tmpInfo;
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::PidSetThresholdFunc(
    const std::map<std::string, std::string>& params)
{
    if (!UbseCliRegModule::DisableTimeoutTimer()) {
        return UbseCliStringPromptReply(SET_TIMER_ERROR);
    }
    auto pid = params.find(PID_OPTION);
    if (pid == params.end()) {
        return UbseCliStringPromptReply("Invalid PID, the length must be lower than 10");
    }
    auto evictThreshold = params.find(EVICT_THRESHOLD_OPTION);
    auto targetEvictThreshold = params.find(TARGET_EVICT_THRESHOLD_OPTION);
    auto reclaimThreshold = params.find(RECLAIM_THRESHOLD_OPTION);
    auto srcNumaIdParam = params.find(SRC_NUMAID_OPTION);
    if (evictThreshold == params.end() || targetEvictThreshold == params.end() || reclaimThreshold == params.end()) {
        return UbseCliStringPromptReply("Invalid threshold");
    }

    if (!SetPidThresholdParamCheck(pid->second, evictThreshold->second, targetEvictThreshold->second,
                                   reclaimThreshold->second)) {
        return UbseCliStringPromptReply("Invalid threshold");
    }

    if (!CheckEvictReclaimDelta(evictThreshold->second, reclaimThreshold->second)) {
        return UbseCliStringPromptReply(
            "evict-thresh must be at least 5 higher than reclaim-thresh to avoid oscillation");
    }

    auto sizeParam = params.find(SIZE_OPTION);
    uint64_t size{};
    if (sizeParam == params.end() || !SizeConversion(sizeParam->second, size)) {
        return UbseCliStringPromptReply(INVALID_SIZE_OPTION_TIP);
    }

    auto pidInfo = SetPidManagerInfo(pid->second, evictThreshold->second, targetEvictThreshold->second,
                                     reclaimThreshold->second, size);
    if (pidInfo.configInfo.pid == -1) {
        return UbseCliStringPromptReply("Internal error");
    }

    if (srcNumaIdParam != params.end()) {
        uint64_t srcNumaId;
        if (utils::ConvertStrToUint64(srcNumaIdParam->second, srcNumaId) != UBSE_OK) {
            return UbseCliStringPromptReply("Invalid --src-numa value");
        }
        pidInfo.configInfo.srcNumaId = srcNumaId;
    }
    UbseCliMemPid memPid{};
    return memPid.UbseCliSetPidThreshold(pidInfo);
}

} // namespace ubse::cli::reg
