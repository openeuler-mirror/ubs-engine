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

#ifndef TONGSUAN_1D_FULLMATCH_SINGLE_NODE_SCENARIO_H
#define TONGSUAN_1D_FULLMATCH_SINGLE_NODE_SCENARIO_H

#include <memory>
#include <string>

#include "it_cluster.h"
#include "it_scenario_fixture.h"

namespace ubse::it::infra {

/**
 * @brief 通算1D FullMatch 单节点场景
 *
 * SetUpTestSuite 启动单节点集群，TearDownTestSuite 停止。
 * 所有 TEST_F(Tongsuan1dFullMeshSingleNodeScenario, ...) 共享同一集群实例。
 */
class Tongsuan1dFullMeshSingleNodeNormalConfigScenario : public ItScenarioFixture {
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

#endif // TONGSUAN_1D_FULLMATCH_SINGLE_NODE_SCENARIO_H
