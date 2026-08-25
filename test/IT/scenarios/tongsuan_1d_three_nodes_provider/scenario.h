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

#ifndef TONGSUAN_1D_THREE_NODES_PROVIDER_SCENARIO_H
#define TONGSUAN_1D_THREE_NODES_PROVIDER_SCENARIO_H

#include "it_scenario_fixture.h"

#include <unistd.h>

#include <string>

namespace {
// IT 环境下三节点同机运行，节点进程经预加载库 gethostname 覆写后上报唯一主机名
// it-node-<slotId>（节点"1"/"2"/"3"分别上报 it-node-1~it-node-3）。
// group 配置以 "," 分隔组内主机名，三节点同组；provider 仅配置节点2，作为唯一的可借出节点。
std::string GetItHostname(const std::string& nodeId)
{
    return "it-node-" + nodeId;
}

std::string GetGroupConfig()
{
    return GetItHostname("1") + "," + GetItHostname("2") + "," + GetItHostname("3");
}

std::string GetProviderConfig()
{
    return GetItHostname("2");
}
} // namespace

/**
 * @brief 三节点 provider 配置场景。
 *
 * 节点"1"~"3"均配置 [ubse.memory] group=节点1,节点2,节点3、provider=节点2。
 * 用于验证：节点1通过 SDK 接口（with_lender）指定借出节点创建 FD/NUMA 内存时，
 * 指定 provider 节点2（同组且为provider）创建成功且借出节点为节点2；
 * 指定非 provider 节点3（同组但非provider）创建失败，返回错误码 UBS_ENGINE_ERR_ALLOCATE。
 */
IT_DEFINE_SCENARIO(Tongsuan1dThreeNodesProviderScenario,
                   MakeBuilder()
                       .Tongsuan()
                       .Nodes({ubse::it::infra::NodeSpec{"1", "127.0.0.2", 8082, 1},
                               ubse::it::infra::NodeSpec{"2", "127.0.0.3", 8083, 2},
                               ubse::it::infra::NodeSpec{"3", "127.0.0.4", 8084, 3}})
                       .WithNodeConfig("1", "ubse.memory", "group", GetGroupConfig())
                       .WithNodeConfig("1", "ubse.memory", "provider", GetProviderConfig())
                       .WithNodeConfig("2", "ubse.memory", "group", GetGroupConfig())
                       .WithNodeConfig("2", "ubse.memory", "provider", GetProviderConfig())
                       .WithNodeConfig("3", "ubse.memory", "group", GetGroupConfig())
                       .WithNodeConfig("3", "ubse.memory", "provider", GetProviderConfig())
                       .WithNodeConfig("1", "ubse.log", "log.level", "DEBUG")
                       .WithNodeConfig("2", "ubse.log", "log.level", "DEBUG")
                       .WithNodeConfig("3", "ubse.log", "log.level", "DEBUG")
                       .Start(cluster_))

#endif // TONGSUAN_1D_THREE_NODES_PROVIDER_SCENARIO_H
