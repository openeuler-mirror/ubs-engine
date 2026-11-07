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

#include "test_scheduler_fixture.h"

#include "ubse_init_ledger_state.h"

namespace ubse::mem::scheduler::ut {

class TestSchedulerInitLedger : public TestSchedulerEndToEnd {
protected:
    void SetUp() override
    {
        TestSchedulerEndToEnd::SetUp();
        auto& ledger = controller::UbseInitLedgerState::GetInstance();
        for (const auto* nodeId : {"1", "2"}) {
            previousLedgerStates[nodeId] = ledger.IsInitLedgerDone(nodeId);
            ledger.SetInitLedgerDone(nodeId, false);
        }
        nodeMap = CreateNodeMap(2);
        nodeMap.at("1").isLender = false;
        AddFullMeshPeers(nodeMap);
        for (const auto& [_, node] : nodeMap) {
            ASSERT_EQ(SchedulerImpl::GetInstance().NodeObjChangeHandler(node), UBSE_OK);
        }
        SetupMockNodeMap(nodeMap);
    }

    void TearDown() override
    {
        for (const auto& [nodeId, done] : previousLedgerStates) {
            controller::UbseInitLedgerState::GetInstance().SetInitLedgerDone(nodeId, done);
        }
        TestSchedulerEndToEnd::TearDown();
    }

    void SetLenderState(UbseNodeClusterState state, bool initLedgerDone)
    {
        controller::UbseInitLedgerState::GetInstance().SetInitLedgerDone("2", initLedgerDone);
        nodeMap.at("2").clusterState = state;
        ASSERT_EQ(SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap.at("2")), UBSE_OK);
        SetupMockNodeMap(nodeMap);
    }

    UbseMemFdBorrowImportObj MakeFdObj(uint64_t size = BYTE_128MB)
    {
        UbseMemFdBorrowImportObj obj{};
        obj.req.name = "fd-init-ledger";
        obj.req.requestNodeId = "1";
        obj.req.importNodeId = "1";
        obj.req.size = size;
        obj.status.state = UBSE_MEM_SCHEDULING;
        return obj;
    }

    template <typename MemObj>
    void CheckDecision(MemObj& obj, UbseResult expectedResult)
    {
        ASSERT_EQ(SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj), expectedResult);
        if (expectedResult == UBSE_OK) {
            ASSERT_FALSE(obj.algoResult.exportNumaInfos.empty());
            for (const auto& numa : obj.algoResult.exportNumaInfos) {
                EXPECT_EQ(numa.nodeId, "2");
                EXPECT_GT(numa.size, 0U);
            }
        } else {
            EXPECT_TRUE(obj.algoResult.exportNumaInfos.empty());
        }
    }

    std::unordered_map<std::string, UbseNodeInfo> nodeMap;
    std::unordered_map<std::string, bool> previousLedgerStates;
};

struct InitLedgerNodeStateCase {
    UbseNodeClusterState state;
    bool initLedgerDone;
    UbseResult expectedResult;
};

class TestSchedulerInitLedgerNodeState
    : public TestSchedulerInitLedger
    , public testing::WithParamInterface<InitLedgerNodeStateCase> {
protected:
    void SetUp() override
    {
        TestSchedulerInitLedger::SetUp();
        SetLenderState(GetParam().state, GetParam().initLedgerDone);
    }
};

TEST_P(TestSchedulerInitLedgerNodeState, FdBorrow)
{
    auto obj = MakeFdObj();
    CheckDecision(obj, GetParam().expectedResult);
}

TEST_P(TestSchedulerInitLedgerNodeState, NumaBorrow)
{
    UbseMemNumaBorrowImportObj obj{};
    obj.req.name = "numa-init-ledger";
    obj.req.requestNodeId = "1";
    obj.req.importNodeId = "1";
    obj.req.size = BYTE_128MB;
    obj.req.srcSocket = 36;
    obj.req.highWatermark = 100;
    obj.status.state = UBSE_MEM_SCHEDULING;
    CheckDecision(obj, GetParam().expectedResult);
}

TEST_P(TestSchedulerInitLedgerNodeState, ShareBorrow)
{
    UbseMemShareBorrowExportObj obj{};
    obj.req.name = "share-init-ledger";
    obj.req.requestNodeId = "1";
    obj.req.size = BYTE_128MB;
    for (const auto* nodeId : {"1", "2"}) {
        adapter_plugins::mmi::UbseNodeInfo node{};
        node.nodeId = nodeId;
        obj.req.shmRegion.nodelist.push_back(node);
    }
    obj.status.state = UBSE_MEM_SCHEDULING;
    CheckDecision(obj, GetParam().expectedResult);
}

TEST_P(TestSchedulerInitLedgerNodeState, AddrBorrow)
{
    UbseMemAddrBorrowImportObj obj{};
    obj.req.name = "addr-init-ledger";
    obj.req.requestNodeId = "1";
    obj.req.importNodeId = "1";
    obj.req.exportNodeId = "2";
    obj.req.dstSocket = 36;
    obj.req.dstNuma = 0;
    obj.req.exportAddrList = {{.addr = 0, .size = BYTE_128MB}};
    obj.status.state = UBSE_MEM_SCHEDULING;
    CheckDecision(obj, GetParam().expectedResult);
}

INSTANTIATE_TEST_SUITE_P(
    InitLedger, TestSchedulerInitLedgerNodeState,
    testing::Values(
        InitLedgerNodeStateCase{UbseNodeClusterState::UBSE_NODE_WORKING, false, UBSE_OK},
        InitLedgerNodeStateCase{UbseNodeClusterState::UBSE_NODE_SMOOTHING, true, UBSE_OK},
        InitLedgerNodeStateCase{UbseNodeClusterState::UBSE_NODE_SMOOTHING, false, UBSE_SCHEDULER_ERROR_NODE_RECONCILE},
        InitLedgerNodeStateCase{UbseNodeClusterState::UBSE_NODE_INIT, true, UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND},
        InitLedgerNodeStateCase{UbseNodeClusterState::UBSE_NODE_UNKNOWN, true, UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND},
        InitLedgerNodeStateCase{UbseNodeClusterState::UBSE_NODE_FAULT, true, UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND},
        InitLedgerNodeStateCase{UbseNodeClusterState::UBSE_NODE_PRE_BMC, true, UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND}));

TEST_F(TestSchedulerInitLedger, PeriodicLedgerWithoutCapacityKeepsNoNodeError)
{
    SetLenderState(UbseNodeClusterState::UBSE_NODE_SMOOTHING, true);
    for (auto& [_, numa] : nodeMap.at("2").numaInfos) {
        numa.freeSize = 0;
        numa.mempool_available_cleared = 0;
        numa.mempool_available_uncleared = 0;
    }
    SetupMockNodeMap(nodeMap);

    auto obj = MakeFdObj();
    CheckDecision(obj, UBSE_SCHEDULER_ERROR_NO_NODE_CAN_LEND);
}

TEST_F(TestSchedulerInitLedger, PeriodicLedgerWithSmallerRequestFeasibleKeepsSizeError)
{
    SetLenderState(UbseNodeClusterState::UBSE_NODE_SMOOTHING, true);
    auto obj = MakeFdObj(16 * BYTE_1GB);
    CheckDecision(obj, UBSE_SCHEDULER_ERROR_SIZE_EXCEED_LEND);
}

TEST_F(TestSchedulerInitLedger, InitLedgerCompletionAndResetTakeEffectWithoutNodeStateChange)
{
    SetLenderState(UbseNodeClusterState::UBSE_NODE_SMOOTHING, false);
    auto waitingObj = MakeFdObj();
    CheckDecision(waitingObj, UBSE_SCHEDULER_ERROR_NODE_RECONCILE);

    controller::UbseInitLedgerState::GetInstance().SetInitLedgerDone("2", true);
    auto readyObj = MakeFdObj();
    CheckDecision(readyObj, UBSE_OK);

    controller::UbseInitLedgerState::GetInstance().SetInitLedgerDone("2", false);
    auto resetObj = MakeFdObj();
    resetObj.req.name = "fd-after-ledger-reset";
    CheckDecision(resetObj, UBSE_SCHEDULER_ERROR_NODE_RECONCILE);
}

TEST_F(TestSchedulerInitLedger, MixedLedgerStatesAllowReadyLenderAndRetryOnFailure)
{
    SetLenderState(UbseNodeClusterState::UBSE_NODE_SMOOTHING, true);
    nodeMap.at("1").clusterState = UbseNodeClusterState::UBSE_NODE_SMOOTHING;
    ASSERT_EQ(SchedulerImpl::GetInstance().NodeObjChangeHandler(nodeMap.at("1")), UBSE_OK);
    SetupMockNodeMap(nodeMap);

    auto readyObj = MakeFdObj();
    CheckDecision(readyObj, UBSE_OK);

    auto tooLargeObj = MakeFdObj(16 * BYTE_1GB);
    tooLargeObj.req.name = "fd-mixed-ledger-too-large";
    CheckDecision(tooLargeObj, UBSE_SCHEDULER_ERROR_NODE_RECONCILE);
}

} // namespace ubse::mem::scheduler::ut
