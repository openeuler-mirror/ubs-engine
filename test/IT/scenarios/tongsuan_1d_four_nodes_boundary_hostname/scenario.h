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

#ifndef TONGSUAN_1D_FOUR_NODES_BOUNDARY_HOSTNAME_SCENARIO_H
#define TONGSUAN_1D_FOUR_NODES_BOUNDARY_HOSTNAME_SCENARIO_H

#include "it_scenario_fixture.h"

#include <string>

namespace {
// IT 环境下四节点同机运行，节点进程经预加载库 gethostname 覆写后上报指定主机名：
// 节点"1"→"a"、节点"2"→63个'a'、节点"3"→"b"、节点"4"→63个'b'。
// 其中 "a" 是 63个'a' 的前缀、"b" 是 63个'b' 的前缀，且 63 字符为 HOST_NAME_MAX-1 的
// 主机名长度上限，用于验证 group/provider 配置按主机名全名精确匹配（非前缀匹配、支持最长主机名）。
// group 配置以 "," 分隔组内全部主机名，四节点同组；
// provider 仅配置节点3（b）与节点4（63个'b'）。
std::string GetHostnameA63()
{
    return std::string(63, 'a');
}

std::string GetHostnameB63()
{
    return std::string(63, 'b');
}

std::string GetGroupConfig()
{
    return "a," + GetHostnameA63() + ",b," + GetHostnameB63();
}

std::string GetProviderConfig()
{
    return "b," + GetHostnameB63();
}
} // namespace

/**
 * @brief 四节点边界主机名场景。
 *
 * 节点"1"~"4"分别上报主机名 a / 63个'a' / b / 63个'b'，均配置
 * [ubse.memory] group=全部主机名、provider=节点3,节点4。
 * 用于验证：节点1通过 SDK 接口（with_lender）指定节点3创建 FD 内存成功且借出节点为节点3；
 * 节点2通过 SDK 接口（with_lender）指定节点4创建 FD 内存成功且借出节点为节点4。
 */
IT_DEFINE_SCENARIO(Tongsuan1dFourNodesBoundaryHostnameScenario,
                   MakeBuilder()
                       .Tongsuan()
                       .Nodes({ubse::it::infra::NodeSpec{"1", "127.0.0.2", 8082, 1},
                               ubse::it::infra::NodeSpec{"2", "127.0.0.3", 8083, 2},
                               ubse::it::infra::NodeSpec{"3", "127.0.0.4", 8084, 3},
                               ubse::it::infra::NodeSpec{"4", "127.0.0.5", 8085, 4}})
                       .WithNodeHostname("1", "a")
                       .WithNodeHostname("2", GetHostnameA63())
                       .WithNodeHostname("3", "b")
                       .WithNodeHostname("4", GetHostnameB63())
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

#endif // TONGSUAN_1D_FOUR_NODES_BOUNDARY_HOSTNAME_SCENARIO_H
