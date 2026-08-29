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

#ifndef TONGSUAN_1D_TWO_NODES_GROUP_ALL_SCENARIO_H
#define TONGSUAN_1D_TWO_NODES_GROUP_ALL_SCENARIO_H

#include "it_scenario_fixture.h"

#include <unistd.h>

#include <string>

namespace {
// IT 环境下两节点同机运行，节点进程经预加载库 gethostname 覆写后上报唯一主机名
// it-node-<slotId>（节点"1"/"2"分别上报 it-node-1、it-node-2）。
// group 配置须列出组内全部主机名（it-node-1,it-node-2）使两节点同组，可互为借出节点。
// provider 不配置，使用默认行为（未配置时所有节点皆可借出）。
std::string GetItHostname(const std::string& nodeId)
{
    return "it-node-" + nodeId;
}

std::string GetItAllHostnames()
{
    return GetItHostname("1") + "," + GetItHostname("2");
}
} // namespace

/**
 * @brief 双节点 group 覆盖集群所有节点配置场景。
 *
 * 节点"1"/"2"均配置 [ubse.memory] group=<主机名>（代表集群所有节点），不配置 provider。
 * 用于验证 group 覆盖全部节点时，节点1借用 FD 内存与 NUMA 内存均成功。
 */
IT_DEFINE_SCENARIO(Tongsuan1dTwoNodesGroupAllScenario,
                   MakeBuilder()
                       .Tongsuan()
                       .TwoNode()
                       .WithNodeConfig("1", "ubse.memory", "group", GetItAllHostnames())
                       .WithNodeConfig("2", "ubse.memory", "group", GetItAllHostnames())
                       .WithNodeConfig("1", "ubse.log", "log.level", "DEBUG")
                       .WithNodeConfig("2", "ubse.log", "log.level", "DEBUG")
                       .Start(cluster_))

#endif // TONGSUAN_1D_TWO_NODES_GROUP_ALL_SCENARIO_H
