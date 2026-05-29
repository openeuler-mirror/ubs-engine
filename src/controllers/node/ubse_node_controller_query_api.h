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

#ifndef UBSE_NODE_CONTROLLER_QUERY_API_H
#define UBSE_NODE_CONTROLLER_QUERY_API_H
#include <cstdint>
#include <vector>
#include "ubse_mem_account.h"
#include "ubse_node_controller_def.h"

namespace ubse::nodeController {
using ubse::mem::account::UbseNumaNodeInfo;
/**
 * @brief 查询所有CPU类型节点的拓扑信息
 *
 * @param cpu_links [OUT] cpu连接信息数组, 调用方需要使用free接口主动释放内存
 * @param cpu_link_cnt [OUT] cpu连接信息个数
 * @return UBSE_OK:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBSE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBSE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBSE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBSE_ERR_INTERNAL:UBSE服务端内部错误
 */
void UbseNodeCpuTopoList(std::vector<def::UbseCpuLink>& linkList);

/**
 * @brief 查询全量节点信息
 *
 * @param node_list [OUT] 节点信息数组, 调用方需要使用free主动释放内存
 * @param cnt [OUT] 节点信息个数
 * @return UBSE_OK:操作成功;
 * UBS_ERR_NULL_POINTER:空指针;
 * UBSE_ERR_CONNECTION_FAILED:连接UBSE服务端失败;
 * UBSE_ERR_AUTH_FAILED:UBSE服务端鉴权不通过;
 * UBSE_ERR_TIMEOUT:UBSE服务端处理超时;
 * UBSE_ERR_INTERNAL:UBSE服务端内部错误
 */
void UbseNodeList(std::vector<def::UbseNode>& nodeList);

void UbseNodeGet(def::UbseNode& node);
/**
 * 主节点侧根据节点id查询节点信息
 * @param nodeId [IN]节点id
 * @param node [OUT]对应节点信息
 */
void UbseNodeGetByNodeIdInMaster(const std::string& nodeId, def::UbseNode& node);
void UbseNodeGetByNodeId(const std::string& nodeId, def::UbseNode& node);

uint32_t UbseNodeNumaMemGet(const std::string& nodeId, std::vector<UbseNumaNodeInfo>& nodeNumaMemList);

size_t UbseGetUnitSize();
} // namespace ubse::nodeController
#endif
