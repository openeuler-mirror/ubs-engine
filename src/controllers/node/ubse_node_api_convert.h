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

#ifndef UBSE_NODE_API_CONVERT_H
#define UBSE_NODE_API_CONVERT_H

#include <vector>

#include "ubs_engine.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_def.h"

namespace ubse::node::api {
using namespace ubse::nodeController::def;

const size_t UBSE_NODE_SIZE =
    sizeof(uint32_t) + sizeof(uint32_t) * UBS_TOPO_SOCKET_NUM + HOST_NAME_MAX; // ubse_node_t size

const size_t UBSE_CPU_LINK_SIZE = 4 * sizeof(uint32_t); // ubse_cpu_link size

uint64_t HtonllCustom(uint64_t host_value);

uint64_t NtohllCustom(uint64_t net_value);

uint32_t UbseNodePack(const UbseNode &ubseNode, uint8_t *buffer);

uint32_t UbseNodeListPack(const std::vector<UbseNode> &ubseNodeList, uint8_t *buffer);

uint32_t UbseCpuLinkListPack(const std::vector<UbseCpuLink> &ubseCpuLinkList, uint8_t *buffer);
} // namespace ubse::node::api

#endif // UBSE_NODE_API_CONVERT_H
