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

// 节点OOM故障：注入大页OOM事件，验证RAS回调触发虚机借用内存进行逃逸
// 事件流经路径：xalarm FIFO → syssentry → RAS handler链 → mock_plugin OomHandle → UbseMemNumaCreate
// 通过xalarm_stub ack文件获取RAS处理结果，再通过SDK验证借用达到UBSE_EXIST状态
// @param cluster  集群实例
// @param nodeId   接收OOM事件的目标节点
// @param nids     受影响的NUMA节点ID列表
// @param timeout  OOM处理超时（秒），复用为xalarm事件注入参数
void RunVmOomEscapeBorrowTest(ubse::it::infra::ItCluster& cluster, const std::string& nodeId,
                              const std::vector<int>& nids, int timeout);

} // namespace ubse::it::tests::fault

#endif // IT_FAULT_CASES_H
