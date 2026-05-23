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

#include "ubse_cli_mem_struct.h"
#include <sstream>

#include "ubse_cli_constant.h"
#include "ubse_error.h"
#include "src/sdk/c/include/ubs_error.h"

namespace ubse::cli::reg {
using namespace framework;

template <typename T>
std::string JoinVector(const std::vector<T>& vec, const std::string& delimiter = ",")
{
    if (vec.empty()) {
        return "-";
    }
    std::ostringstream oss;
    for (size_t i = 0; i < vec.size(); ++i) {
        oss << (i == 0 ? "" : delimiter) << vec[i];
    }
    return oss.str();
}

// UbseNumaInfo
bool UbseNumaInfo::Deserialize(serial::UbseDeSerialization& stream)
{
    stream >> name >> numaId >> importSlotId >> exportSlotId >> size >> ubse::serial::enum_v(state);
    return stream.Check();
}
std::string UbseNumaInfo::GetStringResult() const
{
    std::ostringstream oss;
    oss << "name:" << name << '\n'
        << "size:" << std::to_string(size / MB) << "MB" << '\n'
        << "numa-id:" << numaId << '\n'
        << "import-node:" << importSlotId << '\n'
        << "export-node:" << exportSlotId << '\n';
    return oss.str();
}

// UbseFdInfo
bool UbseFdInfo::Deserialize(serial::UbseDeSerialization& stream)
{
    stream >> name >> memIds >> importSlotId >> exportSlotId >> size >> ubse::serial::enum_v(state);
    return stream.Check();
}
std::string UbseFdInfo::GetStringResult() const
{
    std::ostringstream oss;
    oss << "name:" << name << '\n' << "size:" << (size / MB) << "MB" << '\n';

    oss << "mem-ids:" << JoinVector(memIds) << '\n';

    oss << "import-node:" << importSlotId << '\n' << "export-node:" << exportSlotId << '\n';
    return oss.str();
}

// UbseCliMemOperationResp
bool UbseCliMemOperationResp::Deserialize(serial::UbseDeSerialization& stream)
{
    stream >> name >> requestNodeId >> errorCode >> errMsg >> realSize >> memIdList >> remoteNumaId >> requestId;
    return stream.Check();
}

bool UbseMemShmInfo::Deserialize(serial::UbseDeSerialization& deserialization)
{
    // 解析基本信息
    deserialization >> name >> size >> exportNode >> ubse::serial::enum_v(exportState) >> importNode >>
        ubse::serial::enum_v(importState);

    size_t memIdsSize = 0;
    deserialization >> ubse::serial::array_len_capture(memIdsSize);
    if (!deserialization.Check()) {
        return false;
    }
    for (size_t j = 0; j < memIdsSize; ++j) {
        uint64_t memId;
        deserialization >> memId;
        if (!deserialization.Check()) {
            return false;
        }
        memIds.push_back(memId);
    }
    size_t regionSize = 0;
    deserialization >> ubse::serial::array_len_capture(regionSize);
    if (!deserialization.Check()) {
        return false;
    }
    for (size_t j = 0; j < regionSize; ++j) {
        uint32_t slotId;
        deserialization >> slotId;
        if (!deserialization.Check()) {
            return false;
        }
        region.push_back(slotId);
    }
    return deserialization.Check();
}

std::string UbseMemShmInfo::GetStringResult(bool isAttach) const
{
    std::ostringstream oss;

    // 1. 基础信息
    oss << "name:" << name << "\n"
        << "size:" << (size / MB) << "MB\n";

    // 2. 节点与特定 ID 信息
    if (isAttach) {
        oss << "mem-ids:" << JoinVector(memIds) << "\n"
            << "import-node:" << importNode << "\n";
    }

    oss << "export-node:" << exportNode << "\n"
        << "region:" << JoinVector(region) << "\n";

    return oss.str();
}

std::string GetErrorMessage(uint32_t errorCode)
{
    switch (errorCode) {
        case UBSE_ERR_IPC_CONNECTION_FAILED:
            return systemd::error::CONNECTION_FAILED;
        case UBSE_IPC_ERROR_RECV_FAILED:
            return systemd::error::RECV_FAILED;
        case UBS_ENGINE_ERR_EXISTED:
            return "ERROR: Memory already exists.";
        case UBS_ENGINE_ERR_NOT_EXIST:
            return "ERROR: Memory does not exist.";
        case UBS_ENGINE_ERR_ALLOCATE:
            return "ERROR: Failed to allocate memory.";
        case UBS_ENGINE_ERR_SHM_NO_ATTACH:
            return "ERROR: No shared memory attached.";
        case UBSE_ERR_NOT_SUPPORTED:
            return "ERROR: Memory feature is not supported on this node.";
        default:
            return "ERROR: Internal error with error code " + std::to_string(errorCode) + ".";
    }
}

// UbseBorrowDetailInfo
std::string UbseBorrowDetailInfo::GetBorrowNodeDisplay(
    const std::unordered_map<std::string, std::string>& node_id_with_hostname) const
{
    if ((type == "share" && status == "single") || (type == "share" && borrowNode.empty())) {
        return "";
    }
    auto it = node_id_with_hostname.find(borrowNode);
    if (it == node_id_with_hostname.end()) {
        return std::string("-(" + borrowNode + ")");
    }
    return std::string(it->second + "(" + borrowNode + ")");
}

std::string UbseBorrowDetailInfo::GetLendNodeDisplay(
    const std::unordered_map<std::string, std::string>& node_id_with_hostname) const
{
    auto it = node_id_with_hostname.find(lendNode);
    if (it == node_id_with_hostname.end()) {
        return std::string("-(" + lendNode + ")");
    }
    return std::string(it->second + "(" + lendNode + ")");
}

std::string UbseBorrowDetailInfo::GetStatusDisplay() const
{
    if (type == "share" && status == "single") {
        return "done";
    } else if (type != "share" && status == "single") {
        return "fault";
    } else {
        return status;
    }
}

framework::UbseCliVariableCellInfo UbseBorrowDetailInfo::GetVariableCellInfo(
    std::unordered_map<std::string, UbseBorrowDetailInfo>& node_borrow_detail,
    std::unordered_map<std::string, std::string>& node_id_with_hostname)
{
    UbseCliResBuilder variable_cell_builder(UBSE_CLI_NUM_8, UBSE_CLI_NUM_3 * UBSE_CLI_NUM_10);
    size_t row = variable_cell_builder.UbseCliAddRow();
    variable_cell_builder.UbseCliAddlineSeparate(row);
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, "name");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, "type");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3, "borrow_node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4, "lend_node");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_5, "lend_numa");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, "lend_size");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, "status");
    variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_8, "handle");
    variable_cell_builder.UbseCliAddBottomlineSeparate();

    for (const auto& [key, obj] : node_borrow_detail) {
        for (const auto& item : obj.numaLendInfos) {
            row = variable_cell_builder.UbseCliAddRow();
            variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_1, obj.name);
            variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_2, obj.type);
            variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_3,
                                                     obj.GetBorrowNodeDisplay(node_id_with_hostname));
            variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_4,
                                                     obj.GetLendNodeDisplay(node_id_with_hostname));
            variable_cell_builder.UbseCliSetCellData(
                row, UBSE_CLI_NUM_5, std::to_string(item.numaId) + "(" + std::to_string(item.socketId) + ")");
            variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_6, std::to_string(item.size));
            variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_7, obj.GetStatusDisplay());
            variable_cell_builder.UbseCliSetCellData(row, UBSE_CLI_NUM_8, obj.handle.empty() ? "-" : obj.handle);
        }
    }
    variable_cell_builder.UbseCliAddBottomlineSeparate();
    return std::move(variable_cell_builder.UbseCliVariableCellBuild());
}
} // namespace ubse::cli::reg
