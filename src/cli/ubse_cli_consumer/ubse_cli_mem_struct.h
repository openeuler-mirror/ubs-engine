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

#ifndef UBSE_CLI_MEM_RELATION_STRUCT_H
#define UBSE_CLI_MEM_RELATION_STRUCT_H
#include <string>
#include "ubse_cli_res_builder.h"
#include "ubse_mem_controller.h"
#include "ubse_mem_debt_info_partial_fetch_req.h"
#include "ubse_mem_debt_info_partial_fetch_res.h"
#include "ubse_serial_util.h"

namespace ubse::cli::reg {
constexpr size_t MB = 1024 * 1024;

struct UbseNumaInfo {
    std::string name;
    int64_t numaId;
    uint32_t importSlotId;
    uint32_t exportSlotId;
    uint64_t size;
    ubse::mem::controller::UbseMemStage state;
    bool Deserialize(ubse::serial::UbseDeSerialization& stream);
    [[nodiscard]] std::string GetStringResult() const;
};

struct UbseFdInfo {
    std::string name;
    std::vector<uint64_t> memIds;
    uint32_t importSlotId;
    uint32_t exportSlotId;
    uint64_t size;
    ubse::mem::controller::UbseMemStage state;
    bool Deserialize(ubse::serial::UbseDeSerialization& stream);
    [[nodiscard]] std::string GetStringResult() const;
};

struct UbseCliMemOperationResp {
    std::string name{};
    std::string requestNodeId{}; // 发起去借用的节点ID
    uint32_t errorCode{0};       // 操作错误码
    std::string errMsg{};        // 错误描述信息
    std::string realSize{};      // request size向上取整
    std::vector<uint64_t> memIdList{};
    int64_t remoteNumaId{-1}; // 远端Numa在本地呈现的NumaId
    uint64_t requestId{};
    bool Deserialize(ubse::serial::UbseDeSerialization& stream);
};

// 操作类型枚举
enum class UbseCliShmOperation {
    CREATE,
    DELETE,
    ATTACH,
    DETACH
};

struct UbseMemShmInfo {
    std::string name;
    uint64_t size;
    uint32_t exportNode;
    uint32_t importNode;
    std::vector<uint64_t> memIds;
    ubse::mem::controller::UbseMemStage exportState;
    ubse::mem::controller::UbseMemStage importState;
    std::vector<uint32_t> region;

    bool Deserialize(ubse::serial::UbseDeSerialization& deserialization);

    std::string GetStringResult(bool isAttach = false) const;
};

struct UbseBorrowDetailInfo {
    std::string name{};
    std::string type{};
    std::string borrowNode{};
    std::string lendNode{};
    std::vector<ubse::mem::controller::message::numaLendInfo> numaLendInfos;
    std::string status{};
    std::string handle{};
    [[nodiscard]] static framework::UbseCliVariableCellInfo GetVariableCellInfo(
        std::unordered_map<std::string, UbseBorrowDetailInfo>& node_borrow_detail,
        std::unordered_map<std::string, std::string>& node_id_with_hostname);
    [[nodiscard]] std::string GetBorrowNodeDisplay(
        const std::unordered_map<std::string, std::string>& node_id_with_hostname) const;
    [[nodiscard]] std::string GetLendNodeDisplay(
        const std::unordered_map<std::string, std::string>& node_id_with_hostname) const;
    [[nodiscard]] std::string GetStatusDisplay() const;
};

struct UbseShmAttachRecord {
    bool hasAttached{false};
    UbseBorrowDetailInfo ubseOneBorrowDetailInfo{};
};

std::string GetErrorMessage(uint32_t errorCode);
} // namespace ubse::cli::reg
#endif // UBSE_CLI_MEM_RELATION_STRUCT_H
