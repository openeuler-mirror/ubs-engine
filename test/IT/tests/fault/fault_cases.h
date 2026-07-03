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

#ifndef IT_FAULT_CASES_H
#define IT_FAULT_CASES_H

#include <cstdint>
#include <string>
#include <vector>

#include "it_cluster.h"

namespace ubse::it::tests::fault {

// 向指定节点的xalarm FIFO注入OOM事件
// 事件流经路径：syssentry → RAS处理器链 → mock_plugin OomHandle
// @param cluster  集群实例
// @param nodeId   接收OOM事件的目标节点
// @param nids     受影响的NUMA节点ID列表
// @param timeout  OOM超时参数
void RunOomEventTest(ubse::it::infra::ItCluster& cluster, const std::string& nodeId, const std::vector<int>& nids,
                     int timeout);

} // namespace ubse::it::tests::fault

#endif // IT_FAULT_CASES_H
