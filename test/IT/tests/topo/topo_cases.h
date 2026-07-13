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

// 拓扑查询测试：验证display topo -t cpu返回完整拓扑信息，错误参数返回失败
void RunQueryNodeTopo001(ubse::it::infra::ItCluster& cluster, const std::string& nodeId);

// LCNE vs SDK TopoNodeList：对比每个slot的socketId集合
void RunLcneVsSdkTopoNodeList001(ubse::it::infra::ItCluster& cluster);

// LCNE vs SDK TopoNodeLocalGet：本地节点socketId与LCNE对应slot一致
void RunLcneVsSdkTopoNodeLocalGet001(ubse::it::infra::ItCluster& cluster);

// LCNE vs SDK TopoLinkList：对比链路连接
void RunLcneVsSdkTopoLinkList001(ubse::it::infra::ItCluster& cluster);

// LCNE vs CLI display topo -t cpu：对比链路连接
void RunLcneVsCliTopoCpu001(ubse::it::infra::ItCluster& cluster);

// LCNE logic-entities vs CLI display cluster：对比每个节点的GUID
void RunLcneVsCliClusterGuid001(ubse::it::infra::ItCluster& cluster);

} // namespace ubse::it::tests::topo

#endif // IT_TOPO_CASES_H
