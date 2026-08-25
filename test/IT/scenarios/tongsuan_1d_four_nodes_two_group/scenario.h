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

#ifndef TONGSUAN_1D_FOUR_NODES_TWO_GROUP_SCENARIO_H
#define TONGSUAN_1D_FOUR_NODES_TWO_GROUP_SCENARIO_H

#include "it_scenario_fixture.h"

#include <unistd.h>

#include <string>

namespace {
// IT 环境下四节点同机运行，节点进程经预加载库 gethostname 覆写后上报唯一主机名
// it-node-<slotId>（节点"1"/"2"/"3"/"4"分别上报 it-node-1~it-node-4）。
// group 配置以 ";" 分隔多个组，组内以 "," 分隔主机名；节点1/节点2 同组，
// 节点3/节点4 同组。provider 仅配置节点2，作为唯一的可借出节点。
std::string GetItHostname(const std::string& nodeId)
{
    return "it-node-" + nodeId;
}

std::string GetGroupConfig()
{
    return GetItHostname("1") + "," + GetItHostname("2") + ";" + GetItHostname("3") + "," + GetItHostname("4");
}

std::string GetProviderConfig()
{
    return GetItHostname("2");
}
} // namespace

/**
 * @brief 四节点双组 group/provider 配置场景。
 *
 * 节点"1"~"4"均配置 [ubse.memory] group=节点1,节点2;节点3,节点4、provider=节点2。
 * 用于验证：节点1（与 provider 节点2 同组）借用 FD/NUMA 内存成功且借出节点为节点2；
 * 节点3/节点4（与 provider 节点2 不同组）借用 FD/NUMA 内存失败，且返回错误码一致。
 */
IT_DEFINE_SCENARIO(Tongsuan1dFourNodesTwoGroupScenario,
                   MakeBuilder()
                       .Tongsuan()
                       .FourNode()
                       .WithNodeConfig("1", "ubse.memory", "group", GetGroupConfig())
                       .WithNodeConfig("1", "ubse.memory", "provider", GetProviderConfig())
                       .WithNodeConfig("2", "ubse.memory", "group", GetGroupConfig())
                       .WithNodeConfig("2", "ubse.memory", "provider", GetProviderConfig())
                       .WithNodeConfig("3", "ubse.memory", "group", GetGroupConfig())
                       .WithNodeConfig("3", "ubse.memory", "provider", GetProviderConfig())
                       .WithNodeConfig("4", "ubse.memory", "group", GetGroupConfig())
                       .WithNodeConfig("4", "ubse.memory", "provider", GetProviderConfig())
                       .WithNodeConfig("1", "ubse.log", "log.level", "DEBUG")
                       .WithNodeConfig("2", "ubse.log", "log.level", "DEBUG")
                       .WithNodeConfig("3", "ubse.log", "log.level", "DEBUG")
                       .WithNodeConfig("4", "ubse.log", "log.level", "DEBUG")
                       .Start(cluster_))

#endif // TONGSUAN_1D_FOUR_NODES_TWO_GROUP_SCENARIO_H
