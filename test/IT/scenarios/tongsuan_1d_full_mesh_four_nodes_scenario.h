/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef TONGSUAN_1D_FULL_MESH_FOUR_NODES_SCENARIO_H
#define TONGSUAN_1D_FULL_MESH_FOUR_NODES_SCENARIO_H

#include <memory>
#include <string>

#include "it_cluster.h"
#include "it_scenario_fixture.h"

namespace ubse::it::infra {

/**
 * @brief 通算1D FullMatch 四节点场景
 *
 * SetUpTestSuite 启动四节点集群并等待选举收敛，TearDownTestSuite 停止。
 * 集群包含 1 master + 1 standby + 2 agent。
 * 所有 TEST_F(Tongsuan1dFullMeshFourNodesScenario, ...) 共享同一集群实例。
 */
class Tongsuan1dFullMeshFourNodesScenario : public ItScenarioFixture {
public:
    static void SetUpTestSuite();
    static void TearDownTestSuite();

    static ItCluster& Cluster()
    {
        return *cluster_;
    }

private:
    static std::unique_ptr<ItCluster> cluster_;
    static std::string workDir_;
};

} // namespace ubse::it::infra

#endif // TONGSUAN_1D_FULL_MESH_FOUR_NODES_SCENARIO_H
