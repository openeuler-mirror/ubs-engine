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

#ifndef IT_TOPO_CASES_H
#define IT_TOPO_CASES_H

#include "it_cluster.h"

namespace ubse::it::tests::topo {

// ==================== P0 用例 ====================

// P0-NodeList-Ok-01: 单节点查询 + LCNE 比对
void RunP0NodeListOk01(ubse::it::infra::ItCluster& cluster);

// P0-NodeList-NullPtr-01: 空指针, 返回 NULL_POINTER
void RunP0NodeListNullPtr01(ubse::it::infra::ItCluster& cluster);

// P0-NodeList-Ok-02: 多节点查询 + LCNE 比对
void RunP0NodeListOk02(ubse::it::infra::ItCluster& cluster);

// P0-LocalGet-Ok-01: 查询本节点 + LCNE 比对
void RunP0LocalGetOk01(ubse::it::infra::ItCluster& cluster);

// P0-LocalGet-NullPtr-01: 空指针, 返回 NULL_POINTER
void RunP0LocalGetNullPtr01(ubse::it::infra::ItCluster& cluster);

// P0-LinkList-Ok-01: 双节点链路查询 + LCNE 比对
void RunP0LinkListOk01(ubse::it::infra::ItCluster& cluster);

// P0-LinkList-Fld-01: 字段校验, socket_id/port_id 等非空
void RunP0LinkListFld01(ubse::it::infra::ItCluster& cluster);

// P0-LinkList-NullPtr-01: 空指针, 返回 NULL_POINTER
void RunP0LinkListNullPtr01(ubse::it::infra::ItCluster& cluster);

// ==================== CLI 用例 ====================

// CLI拓扑查询：验证display topo -t cpu返回完整信息，错误参数返回失败
void RunP0CliTopoCpuOk01(ubse::it::infra::ItCluster& cluster, const std::string& nodeId);

// LCNE vs CLI display topo -t cpu：对比链路连接
void RunP1CliTopoCpuCrossConsist01(ubse::it::infra::ItCluster& cluster);

// LCNE logic-entities vs CLI display cluster：对比每个节点的GUID
void RunP0CliClusterOk01(ubse::it::infra::ItCluster& cluster);

// P0-CliNode-Ok-01: CLI display node 查询本节点，验证 nodeId/role/state 非空
void RunP0CliNodeOk01(ubse::it::infra::ItCluster& cluster);

// P0-CliNode-BadParam-01: CLI display node -n 999 不存在的 nodeId 报错
void RunP0CliNodeBadParam01(ubse::it::infra::ItCluster& cluster);

} // namespace ubse::it::tests::topo

#endif // IT_TOPO_CASES_H
