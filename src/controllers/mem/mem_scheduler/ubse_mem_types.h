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

#ifndef UBSE_MEM_SCHEDULER_TYPES_H
#define UBSE_MEM_SCHEDULER_TYPES_H

#include "ubse_common_def.h"
#include "ubse_mmi_def.h"
#include "ubse_node_controller.h"

namespace ubse::mem::scheduler {
using UbseNodeInfo = ubse::nodeController::UbseNodeInfo;
using UbseNumaInfo = ubse::nodeController::UbseNumaInfo;
using UbseAllocator = ubse::nodeController::UbseAllocator;
using UbseNodeClusterState = ubse::nodeController::UbseNodeClusterState;
using UbseResult = ubse::common::def::UbseResult;
using ubse::common::def::MAX_PERCENT;
using PhysicalLink = ubse::nodeController::PhysicalLink;
using NodeId = std::string;
using SocketId = uint32_t;
using ChipId = uint32_t;
using PortId = uint32_t;
using NumaId = uint32_t;

constexpr uint64_t ONE_M = 1024 * 1024;

/**
     * @brief numa信息结构体
     *
     */
struct SocketInfo {
    SocketId socketId{};
    std::vector<NumaId> numaInfos;
};

struct NodeInfo {
    NodeId nodeId;
    std::vector<SocketInfo> socketInfos;
};

} // namespace ubse::mem::scheduler
#endif // UBSE_MEM_SCHEDULER_TYPES_H
