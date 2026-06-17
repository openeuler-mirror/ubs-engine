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

#include <memory>
#include <string>
#include <vector>

#include "it_assertion.h"
#include "it_cluster.h"
#include "it_console_log.h"
#include "it_test_fixture.h"
#include "it_wait_helper.h"
#include "ubse_common_def.h"
#include "ubs_engine_mem.h"

class ItNumaBorrowTest : public ubse::it::infra::ItTestFixture {
protected:
    static void SetUpTestSuite()
    {
        ItTestFixture::SetUpTestSuite();

        nodeConfigs_ = {{lenderNodeId_, "127.0.0.2", 8082, lenderSlotId_},
                        {borrowerNodeId_, "127.0.0.3", 8083, borrowerSlotId_}};

        cluster_ = std::make_unique<ubse::it::infra::ItCluster>(
            binaryPath_.string(), workDir_, nodeConfigs_, stubLibDir_.string());

        IT_LOG_INFO << "Starting two-node cluster for NUMA borrow tests...";
        auto ret = cluster_->StartClusterParallel(30000);
        ASSERT_IT_OK(ret);

        std::string masterNodeId;
        ret = cluster_->GetMasterNodeId(masterNodeId);
        ASSERT_IT_OK(ret);
        IT_LOG_INFO << "Master node: " << masterNodeId;
        ASSERT_EQ(masterNodeId, lenderNodeId_) << "Two-node NUMA borrow test expects node 1 to be the lender";
        IT_LOG_INFO << "Borrower node: " << borrowerNodeId_ << ", Lender node: " << lenderNodeId_;
    }

    static void TearDownTestSuite()
    {
        if (cluster_) {
            IT_LOG_INFO << "Stopping cluster...";
            auto ret = cluster_->StopCluster();
            EXPECT_IT_OK(ret);
            cluster_.reset();
        }
    }

    static std::unique_ptr<ubse::it::infra::ItCluster> cluster_;
    static std::vector<ubse::it::infra::NodeConfig> nodeConfigs_;

    static constexpr const char* lenderNodeId_ = "1";
    static constexpr const char* borrowerNodeId_ = "2";
    static constexpr uint32_t lenderSlotId_ = 1;
    static constexpr uint32_t borrowerSlotId_ = 2;
};

inline std::unique_ptr<ubse::it::infra::ItCluster> ItNumaBorrowTest::cluster_;
inline std::vector<ubse::it::infra::NodeConfig> ItNumaBorrowTest::nodeConfigs_;

TEST_F(ItNumaBorrowTest, TwoNodeNumaNormalBorrow)
{
    auto& borrowerClient = cluster_->GetSdkClient(borrowerNodeId_);
    IT_LOG_INFO << "Borrower SDK client initialized on node " << borrowerNodeId_;

    constexpr uint64_t borrowSize = UBS_MEM_MIN_SIZE;
    const char* borrowName = "it_numa_normal_borrow";
    ubs_mem_numa_desc_t numaDesc{};
    IT_LOG_INFO << "Creating NUMA borrow: name=" << borrowName << ", size=" << borrowSize
                << ", distance=MEM_DISTANCE_L0";
    int32_t sdkRet = borrowerClient.MemNumaCreate(borrowName, borrowSize, MEM_DISTANCE_L0, &numaDesc);
    ASSERT_IT_OK(sdkRet);
    IT_LOG_INFO << "NUMA borrow created, initial stage=" << numaDesc.mem_stage;

    IT_LOG_INFO << "Waiting for borrow to reach UBSE_EXIST state...";
    auto waitRet = ubse::it::infra::ItWaitHelper::WaitForCondition(
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
    EXPECT_EQ(verifyDesc.import_node.slot_id, borrowerSlotId_);
    EXPECT_EQ(verifyDesc.export_node.slot_id, lenderSlotId_);
    IT_LOG_INFO << "Borrow verified: stage=" << verifyDesc.mem_stage << ", size=" << verifyDesc.size
                << ", numaid=" << verifyDesc.numaid << ", importSlot=" << verifyDesc.import_node.slot_id
                << ", exportSlot=" << verifyDesc.export_node.slot_id;

    IT_LOG_INFO << "Deleting NUMA borrow: " << borrowName;
    sdkRet = borrowerClient.MemNumaDelete(borrowName);
    EXPECT_IT_OK(sdkRet);
}