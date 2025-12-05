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

#ifndef UBSE_NODE_CONTROLLER_MSG_H
#define UBSE_NODE_CONTROLLER_MSG_H

#include "ubse_com_module.h"
#include "ubse_common_def.h"
#include "ubse_node_controller.h"

namespace ubse::nodeController {
using namespace ubse::common::def;
using namespace ubse::com;

/**
 * 注册消息处理handler；当前注册: 1. agent向主节点查询全量节点列表. 2. 主节点向agent查询节点拓扑信息
 */
void RegNodeControllerHandler();

UbseResult ReportUbseNodeInfo(const std::string &nodeId, UbseNodeInfo info);

/**
 * 同步消息，主节点采集节点拓扑
 * @param nodeId [in] 采集节点id
 * @param info [out] agent节点拓扑
 * @return
 */
UbseResult CollectRemoteNodeInfo(const std::string &nodeId, UbseNodeInfo &info);

/**
 * 同步消息，agent节点向主节点请求全量节点列表
 * @param nodeId [in] 主节点id
 * @param infos [out] 全量节点列表
 * @return
 */
UbseResult GetAllNodeInfoFromRemote(const std::string &nodeId, std::vector<UbseNodeInfo> &infos);

/**
 * 同步消息，agent节点向主节点请求全量链路信息
 * @param nodeId [in] 主节点id
 * @param devDirConnectInfoRemote [out] 响应
 * @return
 */
UbseResult UbseGetDirectConnectInfoFromRemote(const std::string &nodeId,
    std::unordered_map<std::string, PhysicalLink> &devDirConnectInfoRemote);

/**
 * agent侧处理master节点周期采集节点拓扑 handler
 * @param req [in] 请求入参
 * @param resp [out] 响应
 * @return
 */
UbseResult CollectNodeInfoHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

/**
 * master侧处理agent查询全量节点列表 handler
 * @param req [in] 请求入参
 * @param resp [out] 响应
 * @return
 */
UbseResult GetAllNodeInfoFromRemoteHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

/**
 * master侧处理agent查询全量链路信息 handler
 * @param req [in] 请求入参
 * @param resp [out] 响应
 * @return
 */
UbseResult UbseGetDirectConnectInfoFromRemoteHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);

UbseResult ReportTopologyHandler(const UbseByteBuffer &req, UbseByteBuffer &resp);
} // namespace ubse::nodeController

#endif // UBSE_NODE_CONTROLLER_MSG_H
