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

#ifndef UBSE_NODE_CONTROLLER_COLLECTOR_H
#define UBSE_NODE_CONTROLLER_COLLECTOR_H

#include "src/res_plugins/mti/ubse_lcne_module.h"
#include "ubse_logger_module.h"
#include "ubse_node_controller.h"

namespace ubse::nodeController {
using namespace ubse::module;
using namespace ubse::log;
using namespace ubse::mti;
using namespace ubse::context;

/**
 * 采集节点基础信息：包含节点id, eid, hostname, slotId等
 * @param ubseNodeInfo [in,out] 节点对象
 * @return
 */
UbseResult CollectNodeBaseInfo(UbseNodeInfo &ubseNodeInfo);

/**
 * 采集节点拓扑信息，包含节点ipList, numa信息，cpu信息等
 * @param ubseNodeInfo [in,out] 节点对象
 * @return
 */
UbseResult CollectNodeTopology(UbseNodeInfo &ubseNodeInfo);

/**
 * 采集节点ipList
 * @param ubseNodeInfo [in,out] 节点对象
 * @return
 */
UbseResult CollectIpList(UbseNodeInfo &ubseNodeInfo);

/**
 * 采集节点numa信息，通过节点id从遥测模块过滤
 * @param ubseNodeInfo [in,out] 节点对象
 * @param nodeId [in] 当前节点id
 * @return
 */
UbseResult CollectNumaInfo(UbseNodeInfo &ubseNodeInfo, const std::string &nodeId);

/**
 * 采集节点cpu信息，依赖LCNE模块，根据节点id从LCNE模块过滤
 * @param ubseNodeInfo [in,out] 节点对象
 * @param nodeId [in] 当前节点id
 * @return
 */
UbseResult CollectCpuInfo(UbseNodeInfo &ubseNodeInfo, const std::string &nodeId);
} // namespace ubse::nodeController

#endif // UBSE_NODE_CONTROLLER_COLLECTOR_H
