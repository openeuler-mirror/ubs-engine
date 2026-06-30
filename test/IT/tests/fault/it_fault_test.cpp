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

#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <thread>

#include "it_assertion.h"
#include "it_wait_helper.h"
#include "it_xalarm_helper.h"
#include "tongsuan_1d_full_mesh_four_nodes_scenario.h"

using ubse::it::infra::ItXalarmHelper;
using ubse::it::infra::Tongsuan1dFullMeshFourNodesScenario;
/**
 * @brief Verify OOM event (alarmId=1005) reaches RAS handler on node 2.
 *
 * Inject an OOM event to node 2's xalarm FIFO with NUMA nodes [0, 1].
 * The daemon should log "Alarm from sysSentry" and invoke the OOM handler chain.
 */
TEST_F(Tongsuan1dFullMeshFourNodesScenario, InjectOomEventToNode2)
{
    auto& cluster = Cluster();
    auto fifoPath = cluster.GetNodeProcess("2").GetXalarmFifoPath();

    auto ret = ItXalarmHelper::InjectOomEvent(fifoPath, 99999, {0, 1}, 1, 30, 2);
    EXPECT_IT_OK(ret);

    std::this_thread::sleep_for(std::chrono::seconds(5));
}