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

#include <string>

#include "ubse_common_def.h"
#include "it_assertion.h"
#include "it_console_log.h"
#include "it_wait_helper.h"
#include "tongsuan_1d_full_mesh_two_nodes_scenario.h"
#include "ubs_engine_mem.h"

using ubse::it::infra::ItCluster;
using ubse::it::infra::ItWaitHelper;
using ubse::it::infra::Tongsuan1dFullMeshTwoNodesScenario;

namespace {

constexpr const char* lenderNodeId = "1";
constexpr const char* borrowerNodeId = "2";
constexpr uint32_t lenderSlotId = 1;
constexpr uint32_t borrowerSlotId = 2;

} // namespace

TEST_F(Tongsuan1dFullMeshTwoNodesScenario, TwoNodeNumaNormalBorrow)
{
    auto& borrowerClient = Cluster().GetSdkClient(borrowerNodeId);
    IT_LOG_INFO << "Borrower SDK client initialized on node " << borrowerNodeId;

    constexpr uint64_t borrowSize = UBS_MEM_MIN_SIZE;
    const char* borrowName = "it_numa_normal_borrow";
    ubs_mem_numa_desc_t numaDesc{};
    IT_LOG_INFO << "Creating NUMA borrow: name=" << borrowName << ", size=" << borrowSize
                << ", distance=MEM_DISTANCE_L0";
    int32_t sdkRet = borrowerClient.MemNumaCreate(borrowName, borrowSize, MEM_DISTANCE_L0, &numaDesc);
    ASSERT_IT_OK(sdkRet);
    IT_LOG_INFO << "NUMA borrow created, initial stage=" << numaDesc.mem_stage;

    IT_LOG_INFO << "Waiting for borrow to reach UBSE_EXIST state...";
    auto waitRet = ItWaitHelper::WaitForCondition(
        [&]() {
            ubs_mem_numa_desc_t desc{};
            int32_t getRet = borrowerClient.MemNumaGet(borrowName, &desc);
            if (getRet != UBS_SUCCESS) {
                IT_LOG_WARN << "MemNumaGet returned: " << getRet;
                return false;
            }
            IT_LOG_INFO << "Borrow stage: " << desc.mem_stage;
            return desc.mem_stage == UBSE_EXIST;
        },
        15000, 200);
    EXPECT_IT_OK(waitRet);

    ubs_mem_numa_desc_t verifyDesc{};
    sdkRet = borrowerClient.MemNumaGet(borrowName, &verifyDesc);
    EXPECT_IT_OK(sdkRet);
    EXPECT_EQ(verifyDesc.mem_stage, UBSE_EXIST);
    EXPECT_EQ(verifyDesc.size, borrowSize);
    EXPECT_GE(verifyDesc.numaid, 0);
    EXPECT_EQ(verifyDesc.import_node.slot_id, borrowerSlotId);
    EXPECT_EQ(verifyDesc.export_node.slot_id, lenderSlotId);
    IT_LOG_INFO << "Borrow verified: stage=" << verifyDesc.mem_stage << ", size=" << verifyDesc.size
                << ", numaid=" << verifyDesc.numaid << ", importSlot=" << verifyDesc.import_node.slot_id
                << ", exportSlot=" << verifyDesc.export_node.slot_id;

    IT_LOG_INFO << "Deleting NUMA borrow: " << borrowName;
    sdkRet = borrowerClient.MemNumaDelete(borrowName);
    EXPECT_IT_OK(sdkRet);
}
