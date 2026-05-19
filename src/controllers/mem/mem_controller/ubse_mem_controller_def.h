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

#ifndef UBSE_MEM_CONTROLLER_DEF_H
#define UBSE_MEM_CONTROLLER_DEF_H
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>
#include "ubse_mem_controller.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller_def.h"
#include "ubs_engine_mem.h"
namespace ubse::mem::def {
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::nodeController::def;
struct UbseMemFdDesc {
    std::string name;
    std::vector<uint64_t> memIds;
    uint64_t totalMemSize;
    size_t unitSize;
    UbseNode exportNode;
    UbseNode importNode;
    ubse::mem::controller::UbseMemStage state;
};
struct UbseMemShmMemStatusDesc {
    std::vector<uint64_t> memIds;
    std::vector<UbMemFaultType> faultTypes;
};
struct UbseMemNumaDesc {
    std::string name;
    int64_t numaId;
    ubse::nodeController::def::UbseNode importNode;
    ubse::nodeController::def::UbseNode exportNode;
    uint64_t size;
    ubse::mem::controller::UbseMemStage state;
    uint8_t usrInfo[UBSE_MAX_USR_INFO_LEN];
};
struct UbseMemShmImportDesc {
    std::vector<uint64_t> memIds;
    UbseNode importNode;
    ubse::mem::controller::UbseMemStage state;
};
struct UbseMemShmDesc {
    std::string name;
    uint64_t totalMemSize;
    size_t unitSize;
    UbseNode exportNode;
    uint8_t userInfo[UBSE_MAX_USR_INFO_LEN];
    std::vector<UbseMemShmImportDesc> importDesc;
    ubse::mem::controller::UbseMemStage state;
    std::vector<uint32_t> region;
};
struct UbseMemShmRegion {
    uint32_t nodeCnt{};                     // 节点数
    uint32_t slotIds[UBS_MEM_MAX_SLOT_NUM]; // 节点id列表
};
struct UbseMemShmDispatcher {
    std::string name;
    uint64_t size;
    uint8_t usrInfo[UBS_MEM_MAX_USR_INFO_LEN];
    uint64_t flag;
    adapter_plugins::mmi::UbseUdsInfo udsInfo;
    UbseMemShmRegion shmRegion;
    UbseMemShmRegion provider;
    uint32_t affinitySocketId;
};
struct UserInfo {
    adapter_plugins::mmi::UbseUdsInfo udsInfo;
    int flag;
    mode_t mode;
};

struct UbseMemDebtQueryRequest {
    std::string name{};         // 账本名称, list接口作为name前缀，为空表示查全量
    std::string importNodeId{}; // 导入节点, 为空表示查全量
    std::string exportNodeId{}; // 导出节点, 为空表示查全量
    UbseUdsInfo udsInfo{};      // 调用用户信息, 用于权限校验
};
struct UbseMemIdQueryRequest {
    std::string name{};         // 账本名称
    std::string importNodeId{}; // 导入节点
    uint64_t importMemId{};     // 导入内存id
    uint32_t borrowType{};      // 借用类型
    UbseUdsInfo udsInfo{};      // 调用用户信息, 用于权限校验
};

struct UbseNodeBorrowInfo {
    uint32_t borrowSlotId;
    std::string borrowHostname;
    uint32_t lendSlotId;
    std::string lendHostname;
    uint64_t size; // 总借用量, 单位为MB
};

struct LedgerResp {
    uint32_t ret = 0;
    NodeMemDebtInfoMap debtInfoMap;
};

struct UbseExportMemDesc {
    uint32_t exportSlotId;
    uint64_t exportMemId;
};

struct ShareHandleInfo {
    std::string name;
    std::unordered_set<uint64_t> memIds;
    UbseUdsInfo udsInfo{};
};
using ShareHandleInfoVec = std::vector<ShareHandleInfo>;

using FdHandleInfo = ShareHandleInfo;
using FdHandleInfoVec = std::vector<FdHandleInfo>;

struct NumaHandleInfo {
    std::string name;
    std::unordered_set<int64_t> numaIds;
    UbseUdsInfo udsInfo{};
};
using NumaHandleInfoVec = std::vector<NumaHandleInfo>;

struct DebtHandleInfos {
    ShareHandleInfoVec &shareVec;
    NumaHandleInfoVec &numaVec;
    FdHandleInfoVec &fdVec;
};
} // namespace ubse::mem::def

#endif
