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

#ifndef UBSE_CONTROLLER_DEF_H
#define UBSE_CONTROLLER_DEF_H

#include <string>

#include "ubs_engine.h"
#include "ubs_engine_topo.h"
namespace ubse::nodeController::def {
struct UbseNode {
    uint32_t slotId;
    uint32_t socketId[UBS_TOPO_SOCKET_NUM];
    uint32_t numaIds[UBS_TOPO_SOCKET_NUM][UBS_TOPO_NUMA_NUM];
    ubs_topo_ip_address_t ips[UBS_TOPO_IPADDR_NUM];
    std::string hostName;
};

struct UbseCpuLink {
    uint32_t slotId;
    uint32_t socketId;
    uint32_t portId;
    uint32_t peerSlotId;
    uint32_t peerSocketId;
    uint32_t peerPortId;
};
} // namespace ubse::nodeController::def
#endif
