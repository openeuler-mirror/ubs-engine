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

#ifndef ZHISUAN_NPU_SINGLE_NODE_SCENARIO_H
#define ZHISUAN_NPU_SINGLE_NODE_SCENARIO_H

#include <memory>
#include <string>

#include "it_cluster.h"
#include "it_scenario_fixture.h"

namespace ubse::it::infra {

/**
 * @brief 智算NPU单节点场景
 *
 * SCENE_TYPE=ai, 单节点集群, 不等待选举收敛。
 * SetUpTestSuite 启动集群，TearDownTestSuite 停止。
 * 所有 TEST_F(ZhisuanNpuSingleNodeScenario, ...) 共享同一集群实例。
 */
class ZhisuanNpuSingleNodeScenario : public ItScenarioFixture {
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

#endif // ZHISUAN_NPU_SINGLE_NODE_SCENARIO_H
