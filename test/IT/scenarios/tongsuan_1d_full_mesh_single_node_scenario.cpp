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

#include "tongsuan_1d_full_mesh_single_node_scenario.h"

#include "it_assertion.h"
#include "it_console_log.h"

namespace ubse::it::infra {

std::unique_ptr<ItCluster> Tongsuan1dFullMeshSingleNodeNormalConfigScenario::cluster_;
std::string Tongsuan1dFullMeshSingleNodeNormalConfigScenario::workDir_;

void Tongsuan1dFullMeshSingleNodeNormalConfigScenario::SetUpTestSuite()
{
    IT_LOG_INFO << "Tongsuan1dFullmatchSingleNodeScenario: starting single-node cluster...";
    auto ret = MakeBuilder().SingleNode().Start(cluster_);
    ASSERT_IT_OK(ret);
    IT_LOG_INFO << "Tongsuan1dFullmatchSingleNodeScenario: cluster started";
}

void Tongsuan1dFullMeshSingleNodeNormalConfigScenario::TearDownTestSuite()
{
    if (cluster_) {
        IT_LOG_INFO << "Tongsuan1dFullmatchSingleNodeScenario: stopping cluster...";
        cluster_->StopCluster();
        cluster_.reset();
    }
    CleanupWorkDir(workDir_);
}

} // namespace ubse::it::infra
