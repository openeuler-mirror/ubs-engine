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
#include <regex>

#include "ubse_cli_buffer_guard.h"
#include "ubse_cli_mem_attach.h"
#include "ubse_cli_mem_create.h"
#include "ubse_cli_mem_detach.h"
#include "ubse_cli_mem_pid.h"
#include "ubse_cli_mem_query.h"
#include "ubse_error.h"
#include "ubse_ipc_common.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_serial_util.h"
#include "ubse_str_util.h"

UBSE_DEFINE_THIS_MODULE("ubse_cli");

namespace ubse::cli::reg {
UBSE_CLI_REGISTER_MODULE("CLI_MEM_MODULE", UbseCliRegMemModule);
using namespace ubse::cli::framework;
using namespace ubse::serial;
using namespace ubse::mem::controller;

constexpr const char* PUBLIC_NAME_OPTION = "name";
constexpr const char* PUBLIC_NAME_OPTION_TIP =
    "Input a unique name. The name must not exceed 47 characters "
    "and can only include English letters, numbers, dots, colons, underscores, and hyphens.";
constexpr const char* PUBLIC_NAME_OPTION_REQUIRED =
    "ERROR: The request option -n or --name is required, and the supported name must not exceed 47 characters and can "
    "only include English letters, numbers, dots, colons, underscores, and hyphens.";
constexpr const char* PUBLIC_NAME_PARAM_INVALID =
    "ERROR: Invalid name. The name must not exceed 47 characters and can only include English letters, numbers, dots, "
    "colons, underscores, and hyphens.";

constexpr const char* DISPLAY_MEM_T_OPTION = "type";
constexpr const char* DISPLAY_MEM_BT_OPTION = "borrow-type";
constexpr const char* DISPLAY_MEM_N_OPTION = "name";
constexpr const char* DISPLAY_MEM_TYPE_OPTION_TIP =
    "Query the memory information of a specified option. The option is as follows: node_borrow, borrow_detail, "
    "node_lend, numa_status, config.";
constexpr const char* DISPLAY_MEM_BORROW_TYPE_OPTION_TIP =
    "Input the borrow-type to filter memory account. The option is as follows: "
    "fd, numa, share. Supported only when the type parameter is borrow_detail.";
constexpr const char* DISPLAY_MEM_NAME_OPTION_TIP =
    "Input a unique name to filter memory account. The name must not exceed 47 characters and can only include English "
    "letters, numbers, dots, colons, underscores, and hyphens. Supported only when the type parameter is "
    "borrow_detail.";
constexpr const char* DISPLAY_MEM_TYPE_OPTION_REQUIRED =
    "ERROR: The request option -t or --type is required, and the supported param is as follows: node_borrow, "
    "borrow_detail, node_lend, numa_status, config.";
constexpr const char* DISPLAY_MEM_TYPE_PARAM_INVALID =
    "ERROR: Invalid type. The supported param is as follows: node_borrow, "
    "borrow_detail, node_lend, numa_status, config.";
constexpr const char* DISPLAY_MEM_BORROW_TYPE_PARAM_INVALID =
    "ERROR: Invalid borrow-type. The supported param is as follows: numa, fd, share.";
constexpr const char* DISPLAY_MEM_NAME_PARAM_INVALID = PUBLIC_NAME_PARAM_INVALID;

constexpr const char* PID_OPTION = "pid";
constexpr const char* PID_OPTION_TIP = "PID of the target process. Range: 1-4194304";
constexpr const char* NAME_OPTION = "name";
constexpr const char* NAME_OPTION_TIP = "Process name (comm). Mutually exclusive with --pid. Max 15 chars, "
                                        "letters/numbers/dots/colons/underscores/hyphens.";

constexpr const char* SIZE_OPTION = "size";
constexpr const char* REMOTE_RATIO_OPTION = "remote-ratio";
constexpr const char* SIZE_OPTION_TIP =
    "Size of the process memory. The range is from 128M to 32G, support up to 2 "
    "decimal places (e.g., 8G, 512M, 1.5G). Units: B/K/M/G (binary); default unit is "
    "MiB when omitted.";
constexpr const char* REMOTE_RATIO_OPTION_TIP = "Max remote memory ratio. Range: 0.0-1.0";
constexpr const char* SIZE_INVALID_FORMAT = "ERROR: Invalid size format.";
constexpr const char* REMOTE_RATIO_INVALID_FORMAT = "ERROR: Invalid remote-ratio, range 0.0-1.0";

constexpr const char* DISPLAY_MEM_NAME_OPTION_UNSUPPORT =
    "ERROR: The -n or --name option only supports when the -t or --type parameter is borrow_detail.";
constexpr const char* DISPLAY_MEM_BORROW_TYPE_OPTION_UNSUPPORT =
    "ERROR: The -bt or --borrow-type option only supports when the -t or --type parameter is borrow_detail.";
constexpr const char* DISPLAY_MEM_ALL_OPTION = "all";
constexpr const char* DISPLAY_MEM_ALL_OPTION_TIP =
    "Display all hugepage types supported by the current environment in addition to basic NUMA info. "
    "Supported only when the type parameter is numa_status.";
constexpr const char* DISPLAY_MEM_ALL_OPTION_UNSUPPORT =
    "ERROR: The -a or --all option only supports when the -t or --type parameter is numa_status.";

constexpr const char* CREATE_MEM_T_OPTION = "type";
constexpr const char* CREATE_MEM_L_OPTION = "link-id";
constexpr const char* CREATE_MEM_S_OPTION = "size";
constexpr const char* CREATE_MEM_N_OPTION = PUBLIC_NAME_OPTION;
constexpr const char* CREATE_MEM_R_OPTION = "region";
constexpr const char* CREATE_MEM_TYPE_OPTION_TIP = "Specify the type. The option is as follows: numa, fd, share.";
constexpr const char* CREATE_MEM_LINK_OPTION_TIP =
    "Specify the link-id. The format is: nodeID/socketID/portID-nodeID/socketID/portID (e.g., 1/36/0-2/36/0). "
    "Supported "
    "only when the type parameter is numa.";
constexpr const char* CREATE_MEM_SIZE_OPTION_TIP = "Specify the size. The minimum allowed size is 4M. (e.g., 128M,1G).";
constexpr const char* CREATE_MEM_NAME_OPTION_TIP = PUBLIC_NAME_OPTION_TIP;
constexpr const char* CREATE_MEM_REGION_OPTION_TIP = "Specify the shared region node IDs. The format is: node1,node2 "
                                                     "(e.g., 1,2). Supported only when the type parameter is share.";
constexpr const char* CREATE_MEM_NAME_OPTION_REQUIRED = PUBLIC_NAME_OPTION_REQUIRED;
constexpr const char* CREATE_MEM_NAME_PARAM_INVALID = PUBLIC_NAME_PARAM_INVALID;
constexpr const char* CREATE_MEM_TYPE_OPTION_REQUIRED =
    "ERROR: The request option -t or --type is required, and the supported param is as follows: numa, fd, share.";
constexpr const char* CREATE_MEM_TYPE_PARAM_INVALID =
    "ERROR: Invalid type. The supported param is as follows: numa, fd, share.";
constexpr const char* CREATE_MEM_SIZE_OPTION_REQUIRED =
    "ERROR: The request option -s or --size is required, and the minimum allowed size is 4M. (e.g., 128M,1G)";
constexpr const char* CREATE_MEM_SIZE_PARAM_INVALID =
    "ERROR: Invalid size. The minimum allowed size is 4M. (e.g., 128M,1G)";
constexpr const char* CREATE_MEM_LINK_PARAM_INVALID =
    "ERROR: Invalid link. The link must be hyphen-separated nodeID/socketID/portID pairs (e.g., 1/36/0-2/36/0).";
constexpr const char* CREATE_MEM_REGION_PARAM_INVALID =
    "ERROR: Invalid region. The region must be comma-separated numeric node IDs (e.g., 1,2,3).";
constexpr const char* CREATE_MEM_LINK_OPTION_UNSUPPORT =
    "ERROR: The -l or --link option only supports when the -t or --type parameter is numa.";
constexpr const char* CREATE_MEM_REGION_OPTION_UNSUPPORT =
    "ERROR: The -r or --region option only supports when the -t or --type parameter is share.";

constexpr const char* DELETE_MEM_N_OPTION = PUBLIC_NAME_OPTION;
constexpr const char* DELETE_MEM_T_OPTION = "type";
constexpr const char* DELETE_MEM_NAME_OPTION_TIP = PUBLIC_NAME_OPTION_TIP;
constexpr const char* DELETE_MEM_TYPE_OPTION_TIP =
    "Input the type to delete memory. The default value is numa. The option is as follows: fd, numa, share, addr.";
constexpr const char* DELETE_MEM_NAME_OPTION_REQUIRED = PUBLIC_NAME_OPTION_REQUIRED;
constexpr const char* DELETE_MEM_NAME_PARAM_INVALID = PUBLIC_NAME_PARAM_INVALID;
constexpr const char* DELETE_MEM_TYPE_PARAM_INVALID =
    "ERROR: Invalid type. The supported param is as follows: numa, fd, share, addr.";

constexpr const char* ATTACH_MEM_N_OPTION = PUBLIC_NAME_OPTION;
constexpr const char* ATTACH_MEM_NAME_OPTION_TIP = PUBLIC_NAME_OPTION_TIP;
constexpr const char* ATTACH_MEM_NAME_OPTION_REQUIRED = PUBLIC_NAME_OPTION_REQUIRED;
constexpr const char* ATTACH_MEM_NAME_PARAM_INVALID = PUBLIC_NAME_PARAM_INVALID;

constexpr const char* DETACH_MEM_N_OPTION = PUBLIC_NAME_OPTION;
constexpr const char* DETACH_MEM_NAME_OPTION_TIP = PUBLIC_NAME_OPTION_TIP;
constexpr const char* DETACH_MEM_NAME_OPTION_REQUIRED = PUBLIC_NAME_OPTION_REQUIRED;
constexpr const char* DETACH_MEM_NAME_PARAM_INVALID = PUBLIC_NAME_PARAM_INVALID;

constexpr const char* NODE_BORROW_EMPTY = "INFO: The node borrow information is empty.";
constexpr const char* BORROW_DETAIL_EMPTY = "INFO: The borrow detail information is empty.";

constexpr const char* SERIALIZATION_ERROR = "ERROR: Serialization failed.";
constexpr const char* DE_SERIALIZATION_ERROR = "ERROR: Deserialization failed.";
constexpr const char* MEMORY_EMPTY_ERROR = "ERROR: Failed to obtain memory information";
constexpr const char* SET_TIMER_ERROR = "ERROR: Set timer failed. ";

static const uint16_t MEM_MODULE_CODE = UBSE_MEM;
static const uint16_t MEM_NODE_BORROW_OP_CODE = UBSE_MEM_CLI_NODE_BORROW;
static const uint16_t MEMORY_DELETE_OP_CODE = UBSE_MEM_CLI_DELETE_MEMORY;
static const uint16_t MEMORY_NUMA_STATE_QUERY_OP_CODE = UBSE_MEM_CLI_NUMA_STATE_QUERY;
const int8_t MEM_SUCCESS_CODE = UBSE_OK;
const int MAX_NAME_LENGTH = 47;
const uint32_t REQUEST_BUFFER_CAPACITY = 8;
const int8_t RETRY_WAIT_TIME = 10;
constexpr size_t NODE_LENGTH = 80;

std::string FormatHostnameSlot(const std::string& hostname, uint32_t slotId)
{
    return (hostname.empty() ? "-" : hostname) + "(" + std::to_string(slotId) + ")";
}

std::string FormatNameParamError(int maxLength)
{
    return "ERROR: Invalid name. The name must not exceed " + std::to_string(maxLength) +
           " characters and can only include English letters, numbers, dots, colons, underscores, and hyphens.";
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliQueryNodeBorrowInfo()
{
    UbseSerialization ubse_req_serial(REQUEST_BUFFER_CAPACITY);
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
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "borrow_node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "lend_node");
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
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "lend_node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "borrow_node");
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
    UbseDeSerialization& deSerialization, size_t numaInfoSize, bool showAll)
{
    if (!showAll) {
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
    return UbseCliProcessNumaStatusDataWithHugepages(deSerialization, numaInfoSize);
}

static void FillNumaStatusHeader(UbseCliResBuilder& builder, size_t row, bool show2MColumns,
                                 const std::string& largeTotalCol, const std::string& largeFreeCol)
{
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "node");
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "numa");
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "total");
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "used");
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, "free");
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, "used_percent");
    if (show2MColumns) {
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, "2M_total");
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_8, "2M_free");
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_9, largeTotalCol);
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_10, largeFreeCol);
    } else {
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, largeTotalCol);
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_8, largeFreeCol);
    }
}

static uint32_t FillNumaStatusRow(UbseCliResBuilder& builder, size_t row, bool show2MColumns,
                                  UbseDeSerialization& deSerialization)
{
    UbseNumaStatusInfo numaInfo{};
    std::string nr2M;
    std::string free2M;
    std::string nrLarge;
    std::string freeLarge;
    deSerialization >> numaInfo.node >> numaInfo.numa >> numaInfo.total >> numaInfo.used >> numaInfo.freeSize >>
        numaInfo.used_percent >> nr2M >> free2M >> nrLarge >> freeLarge;
    if (!deSerialization.Check()) {
        return UBSE_ERROR;
    }
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, numaInfo.node);
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, numaInfo.numa);
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, numaInfo.total);
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, numaInfo.used);
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, numaInfo.freeSize);
    builder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, numaInfo.used_percent);
    if (show2MColumns) {
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, nr2M);
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_8, free2M);
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_9, nrLarge);
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_10, freeLarge);
    } else {
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, nrLarge);
        builder.UbseCliSetCellData(row, UBSE_CLI_NUM_8, freeLarge);
    }
    return UBSE_OK;
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliProcessNumaStatusDataWithHugepages(
    UbseDeSerialization& deSerialization, size_t numaInfoSize)
{
    std::string pageSizeType;
    deSerialization >> pageSizeType;
    if (!deSerialization.Check()) {
        return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
    }
    std::string largeTotalCol = pageSizeType + "_total";
    std::string largeFreeCol = pageSizeType + "_free";
    bool show2MColumns = (pageSizeType != "512M");
    size_t colCount = show2MColumns ? UBSE_CLI_NUM_10 : UBSE_CLI_NUM_8;
    UbseCliResBuilder variable_cell_builder(colCount, NODE_LENGTH);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    FillNumaStatusHeader(variable_cell_builder, row, show2MColumns, largeTotalCol, largeFreeCol);
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    for (size_t i = 0; i < numaInfoSize; i++) {
        row = variable_cell_builder.UbseCliAddRow();
        if (FillNumaStatusRow(variable_cell_builder, row, show2MColumns, deSerialization) != UBSE_OK) {
            return UbseCliStringPromptReply(DE_SERIALIZATION_ERROR);
        }
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return UbseCliVariableCelReply(variable_cell_builder.UbseCliVariableCellBuild());
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::UbseCliQueryNumaStatus(bool showAll)
{
    UbseSerialization ubse_req_serial(REQUEST_BUFFER_CAPACITY);
    if (showAll) {
        uint8_t flag = 1;
        ubse_req_serial << flag;
        if (!ubse_req_serial.Check()) {
            return UbseCliStringPromptReply(SERIALIZATION_ERROR);
        }
    }
    ubse_api_buffer_t ubse_req_buffer{ubse_req_serial.GetBuffer(), static_cast<uint32_t>(ubse_req_serial.GetLength())};
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
    return UbseCliProcessNumaStatusData(ubse_de_serial, numaInfoSize, showAll);
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
        auto it_all = params.find(DISPLAY_MEM_ALL_OPTION);
        if (it_all != params.end()) {
            return UbseCliStringPromptReply(DISPLAY_MEM_ALL_OPTION_UNSUPPORT);
        }
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
        auto it_all = params.find(DISPLAY_MEM_ALL_OPTION);
        if (it_all != params.end() && kind != "numa_status") {
            return UbseCliStringPromptReply(DISPLAY_MEM_ALL_OPTION_UNSUPPORT);
        }
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
            return UbseCliQueryNumaStatus(it_all != params.end());
        } else if (kind == "config") {
            return QueryMemConfig();
        }
    }
    return UbseCliStringPromptReply(DISPLAY_MEM_TYPE_PARAM_INVALID);
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::DisplayProcessMemFunc(
    const std::map<std::string, std::string>& params)
{
    auto it = params.find("type");
    if (it == params.end()) {
        return UbseCliStringPromptReply("ERROR: The request option -t or --type is required, and the supported params "
                                        "are as follows: config, proc_detail.");
    }
    if (it->second != "config" && it->second != "proc_detail") {
        return UbseCliStringPromptReply(
            "ERROR: Invalid type. The supported params are as follows: config, proc_detail.");
    }
    UbseCliMemPid memPid{};
    if (it->second == "proc_detail") {
        return memPid.UbseCliDisplayProcessMemDetail();
    }
    return memPid.UbseCliDisplayProcessMemConfig();
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
        .UbseCliAddFlagOption("a", DISPLAY_MEM_ALL_OPTION, DISPLAY_MEM_ALL_OPTION_TIP)
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
        return false;
    }

    size = tmpSize * multiplier;
    return true;
}

bool CheckName(const std::string& name)
{
    return CheckName(name, MAX_NAME_LENGTH);
}

bool CheckName(const std::string& name, int maxLength)
{
    if (name.length() > static_cast<size_t>(maxLength) || name.empty()) {
        return false;
    }
    for (char c : name) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (!isdigit(uc) && !isalpha(uc) && c != '_' && c != '-' && c != '.' && c != ':') {
            return false;
        }
    }
    return true;
}

constexpr int MAX_PROC_NAME_LENGTH = 15;

bool CheckProcName(const std::string& name)
{
    return CheckName(name, MAX_PROC_NAME_LENGTH);
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
            if (pos != item.length()) {
                return false;
            }
            if (value > std::numeric_limits<uint32_t>::max()) {
                return false;
            }
            regions.push_back(static_cast<uint32_t>(value));
        } catch (...) {
            return false;
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
    auto allowedParams = GetAllowedParams(type);
    std::string errorMsg;
    if (!ValidateParamsWhitelist(params, allowedParams, errorMsg)) {
        return UbseCliStringPromptReply(errorMsg);
    }
    std::string name;
    size_t size{};
    if (!ValidateCommonParams(params, name, size, errorMsg)) {
        return UbseCliStringPromptReply(errorMsg);
    }
    UbseCliMemCreate memCreate{};
    if (type == "numa") {
        auto itLink = params.find(CREATE_MEM_L_OPTION);
        if (itLink != params.end() && !LinkIsMatch(itLink->second)) {
            return UbseCliStringPromptReply(CREATE_MEM_LINK_PARAM_INVALID);
        }
        auto linkValue = (itLink != params.end()) ? itLink->second : "";
        return memCreate.UbseCliCreateNumaMem(name, size, linkValue);
    } else if (type == "share") {
        auto itRegion = params.find(CREATE_MEM_R_OPTION);
        std::vector<uint32_t> region{};
        if (itRegion != params.end() && !ParseRegionString(itRegion->second, region)) {
            return UbseCliStringPromptReply(CREATE_MEM_REGION_PARAM_INVALID);
        }
        return memCreate.UbseCliCreateShareMem(name, size, region);
    } else {
        return memCreate.UbseCliCreateFdMem(name, size);
    }
}

UbseCliCommandInfo UbseCliRegMemModule::DisplayProcessMem()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("display")
        .UbseCliSetType("process-mem")
        .UbseCliAddOption("t", "type",
                          "Query the process memory configuration. The option is as follows: config, "
                          "proc_detail.")
        .UbseCliSetFunc(DisplayProcessMemFunc);
    return builder.UbseCliBuild();
}

UbseCliCommandInfo UbseCliRegMemModule::ChangeMemory()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("change")
        .UbseCliSetType("process-mem")
        .UbseCliAddOption("p", PID_OPTION, PID_OPTION_TIP)
        .UbseCliAddOption("n", NAME_OPTION, NAME_OPTION_TIP)
        .UbseCliAddOption("s", SIZE_OPTION, SIZE_OPTION_TIP)
        .UbseCliAddOption("r", REMOTE_RATIO_OPTION, REMOTE_RATIO_OPTION_TIP)
        .UbseCliSetFunc(ChangeProcessMemFunc);
    return builder.UbseCliBuild();
}

static constexpr int PID_VALUE_MAX = 4194304;

bool IsValidIntegerString(const std::string& s)
{
    if (s.empty()) {
        return false;
    }
    if (s[0] == '0' && s.size() > 1) {
        return false;
    }
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

static std::string ValidatePid(const std::string& pidStr)
{
    if (!IsValidIntegerString(pidStr)) {
        return "--pid must be a positive integer, range 1~" + std::to_string(PID_VALUE_MAX);
    }
    int pidVal = 0;
    if (utils::ConvertStrToInt(pidStr, pidVal) != UBSE_OK || pidVal <= 0 || pidVal > PID_VALUE_MAX) {
        return "--pid must be a positive integer, range 1~" + std::to_string(PID_VALUE_MAX);
    }
    return "";
}

static std::shared_ptr<UbseCliResultEcho> DoRemoveByPid(const std::string& pidStr)
{
    UbseCliMemPid memPid{};
    auto removeResult = memPid.UbseCliRemoveProcessMem(true, pidStr);
    return removeResult;
}

static std::shared_ptr<UbseCliResultEcho> DoRemoveByName(const std::string& name)
{
    UbseCliMemPid memPid{};
    auto removeResult = memPid.UbseCliRemoveProcessMem(false, name);
    return removeResult;
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::RemoveProcessMemFunc(
    const std::map<std::string, std::string>& params)
{
    if (!UbseCliRegModule::DisableTimeoutTimer()) {
        return UbseCliStringPromptReply(SET_TIMER_ERROR);
    }
    auto pidIt = params.find(PID_OPTION);
    auto nameIt = params.find(NAME_OPTION);
    if (pidIt == params.end() && nameIt == params.end()) {
        return UbseCliStringPromptReply("ERROR: The request option -p/--pid or -n/--name is required.");
    }
    if (pidIt != params.end() && nameIt != params.end()) {
        return UbseCliStringPromptReply("ERROR: --pid and --name are mutually exclusive.");
    }
    if (nameIt != params.end()) {
        if (!CheckProcName(nameIt->second)) {
            return UbseCliStringPromptReply(FormatNameParamError(MAX_PROC_NAME_LENGTH));
        }
        return DoRemoveByName(nameIt->second);
    }
    auto errMsg = ValidatePid(pidIt->second);
    if (!errMsg.empty()) {
        return UbseCliStringPromptReply("ERROR: " + errMsg);
    }
    return DoRemoveByPid(pidIt->second);
}

UbseCliCommandInfo UbseCliRegMemModule::RemoveMemory()
{
    UbseCliRegBuilder builder;
    builder.UbseCliSetCommand("remove")
        .UbseCliSetType("process-mem")
        .UbseCliAddOption("p", PID_OPTION, PID_OPTION_TIP)
        .UbseCliAddOption("n", NAME_OPTION, NAME_OPTION_TIP)
        .UbseCliSetFunc(RemoveProcessMemFunc);
    return builder.UbseCliBuild();
}

bool CheckDeleteType(const std::string& type)
{
    return type == "fd" || type == "numa" || type == "addr" || type == "share";
}

void EnsureDotAtEnd(std::string& str)
{
    if (!str.empty() && str.back() != '.') {
        str += '.';
    }
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
    std::string deleteType = "numa";
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
    if (ret == UBSE_ERR_TIMED_OUT) {
        return HandleTimeoutRetry(name->second);
    }
    if (ret != UBSE_OK) {
        return UbseCliStringPromptReply(GetErrorMessage(ret));
    }
    UbseDeSerialization deserial{resBuffer.buffer, resBuffer.length};
    uint32_t errorCode{0};
    std::string errMsg;
    deserial >> errorCode >> errMsg;
    if (deserial.Check() && errorCode == MEM_SUCCESS_CODE) {
        return UbseCliStringPromptReply("Delete successfully");
    }
    if (deserial.Check() && errorCode == UBSE_ERR_NOT_SUPPORTED) {
        return UbseCliStringPromptReply(GetErrorMessage(errorCode));
    }
    EnsureDotAtEnd(errMsg);
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
    this->cmd_.emplace_back(DisplayProcessMem());
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

static constexpr uint64_t PROCESS_MEM_SIZE_MIN = 128ULL * 1024 * 1024;
static constexpr uint64_t PROCESS_MEM_SIZE_MAX = 32ULL * 1024 * 1024 * 1024;

static constexpr uint64_t BYTES_PER_KIB = 1024ULL;
static constexpr uint64_t BYTES_PER_MIB = BYTES_PER_KIB * BYTES_PER_KIB;
static constexpr uint64_t BYTES_PER_GIB = BYTES_PER_KIB * BYTES_PER_KIB * BYTES_PER_KIB;
static constexpr int DECIMAL_BASE = 10;
static constexpr int DECIMAL_DIGITS_GROUP = 3;

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

static std::string ConvertSizeToBytes(const std::string& valStr, uint64_t& outBytes)
{
    if (SizeConversion(valStr, outBytes)) {
        return "";
    }
    std::regex pattern(R"(^(\d+)(\.(\d{1,2}))?$)");
    std::smatch match;
    if (!std::regex_match(valStr, match, pattern)) {
        return SIZE_INVALID_FORMAT;
    }
    uint64_t intPart{0};
    if (ubse::utils::ConvertStrToUint64(match[1], intPart) != UBSE_OK) {
        return SIZE_INVALID_FORMAT;
    }
    if (match[DECIMAL_DIGITS_GROUP].matched) {
        if (!ConvertDecimalPart(intPart, match[DECIMAL_DIGITS_GROUP], BYTES_PER_MIB, outBytes)) {
            return SIZE_INVALID_FORMAT;
        }
    } else {
        if (!ConvertIntegerPart(intPart, BYTES_PER_MIB, outBytes)) {
            return SIZE_INVALID_FORMAT;
        }
    }
    return "";
}

static std::string ValidateRemoteRatio(const std::string& valStr)
{
    if (valStr.empty()) {
        return REMOTE_RATIO_INVALID_FORMAT;
    }
    std::regex pattern(R"(^\d+(\.\d+)?$)");
    if (!std::regex_match(valStr, pattern)) {
        return REMOTE_RATIO_INVALID_FORMAT;
    }
    try {
        double ratio = std::stod(valStr);
        if (ratio < 0.0 || ratio > 1.0) {
            return REMOTE_RATIO_INVALID_FORMAT;
        }
    } catch (...) {
        return REMOTE_RATIO_INVALID_FORMAT;
    }
    return "";
}

static std::string ValidateChangePidName(const std::map<std::string, std::string>& params,
                                         process_mem::def::ProcessMemNewConfigInfo& newConfig)
{
    auto pidIt = params.find(PID_OPTION);
    auto nameIt = params.find(NAME_OPTION);
    if (pidIt == params.end() && nameIt == params.end()) {
        return "ERROR: -p/--pid or -n/--name is required.";
    }
    if (pidIt != params.end() && nameIt != params.end()) {
        return "ERROR: --pid and --name are mutually exclusive.";
    }
    if (pidIt != params.end()) {
        auto errMsg = ValidatePid(pidIt->second);
        if (!errMsg.empty()) {
            return "ERROR: " + errMsg;
        }
        newConfig.isPid = true;
        newConfig.identifier = pidIt->second;
    } else {
        if (!CheckProcName(nameIt->second)) {
            return FormatNameParamError(MAX_PROC_NAME_LENGTH);
        }
        newConfig.isPid = false;
        newConfig.identifier = nameIt->second;
    }
    return "";
}

static std::string ValidateChangeSize(const std::map<std::string, std::string>& params,
                                      process_mem::def::ProcessMemNewConfigInfo& newConfig)
{
    auto sizeIt = params.find(SIZE_OPTION);
    if (sizeIt == params.end()) {
        return "ERROR: -s/--size is required.";
    }
    uint64_t sizeBytes = 0;
    auto sizeErr = ConvertSizeToBytes(sizeIt->second, sizeBytes);
    if (!sizeErr.empty()) {
        return sizeErr;
    }
    if (sizeBytes < PROCESS_MEM_SIZE_MIN || sizeBytes > PROCESS_MEM_SIZE_MAX) {
        UBSE_LOG_ERROR << "size out of range: value=" << sizeBytes << " (valid: " << PROCESS_MEM_SIZE_MIN << "-"
                       << PROCESS_MEM_SIZE_MAX << ")";
        return "ERROR: -s/--size out of range, supported range is 128M~32G";
    }
    newConfig.maxMemory = sizeBytes;
    return "";
}

static std::string ValidateChangeRemoteRatio(const std::map<std::string, std::string>& params,
                                             process_mem::def::ProcessMemNewConfigInfo& newConfig)
{
    auto remoteRatioIt = params.find(REMOTE_RATIO_OPTION);
    if (remoteRatioIt == params.end()) {
        return "ERROR: -r/--remote-ratio is required.";
    }
    auto ratioErr = ValidateRemoteRatio(remoteRatioIt->second);
    if (!ratioErr.empty()) {
        return ratioErr;
    }
    newConfig.remoteRatio = std::stod(remoteRatioIt->second);
    return "";
}

std::shared_ptr<UbseCliResultEcho> UbseCliRegMemModule::ChangeProcessMemFunc(
    const std::map<std::string, std::string>& params)
{
    if (!UbseCliRegModule::DisableTimeoutTimer()) {
        return UbseCliStringPromptReply(SET_TIMER_ERROR);
    }

    process_mem::def::ProcessMemNewConfigInfo newConfig{};
    auto errMsg = ValidateChangePidName(params, newConfig);
    if (!errMsg.empty()) {
        return UbseCliStringPromptReply(errMsg);
    }
    errMsg = ValidateChangeSize(params, newConfig);
    if (!errMsg.empty()) {
        return UbseCliStringPromptReply(errMsg);
    }
    errMsg = ValidateChangeRemoteRatio(params, newConfig);
    if (!errMsg.empty()) {
        return UbseCliStringPromptReply(errMsg);
    }

    UbseCliMemPid memPid{};
    return memPid.UbseCliSetProcessMem(newConfig);
}

} // namespace ubse::cli::reg
