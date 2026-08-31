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

#include "test_process_mem_pid_borrow_reuse.h"

namespace ubse::ut::process_mem {
using namespace ::process_mem::decision;
using namespace ::process_mem::manager;
using namespace ::process_mem::def;
using namespace ::process_mem::pid::bridge;

namespace {
constexpr uint64_t BYTES_PER_GB = 1073741824;
constexpr uint64_t GB3_5 = 3758096384;
} // namespace

void TestProcessMemPidBorrowReuse::SetUp()
{
    ubse::mem::controller::MockResetAllErrors();
    ubse::nodeController::MockSetCurrentNodeId("NODE0");

    ubse::smap::MockResetMigrateState();
    ProcessMemPidBridge::rmrsMigrateOut = ubse::smap::MockRmrsMigrateOut;

    auto& decision = ProcessMemPidDecision::GetInstance();
    if (decision.returnExecutor_ == nullptr || !decision.returnExecutor_->IsStart()) {
        decision.returnExecutor_ = ubse::task_executor::UbseTaskExecutor::Create("ProcMemReuseReturn", 2, 64);
        decision.returnExecutor_->Start();
    }
}

void TestProcessMemPidBorrowReuse::TearDown()
{
    ProcessMemPidBridge::rmrsMigrateOut = {};
    ubse::smap::MockResetMigrateState();

    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();
    for (const auto& [pid, entry] : snapshot) {
        infoMgr.RemoveManagedPidEntry(pid);
    }
    ubse::nodeController::MockResetCurrentNodeId();
}

void TestProcessMemPidBorrowReuse::AddPidWithBorrow(pid_t pid, const def::BorrowState& borrow,
                                                    def::ProcessStatus status)
{
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    infoMgr.AddNameSourceToManagedPid(pid, "reuse_proc", 32 * BYTES_PER_GB, 0.5);
    infoMgr.UpdateManagedPidBorrowState(pid, borrow, status);
}

TEST_F(TestProcessMemPidBorrowReuse, PartialReuseMigrateFailAbortsNewDebtOnly)
{
    const pid_t pid = 15964;
    constexpr uint64_t need = 2 * BYTES_PER_GB;
    constexpr uint64_t reuse = BYTES_PER_GB / 2;
    constexpr uint64_t remain = need - reuse;

    def::BorrowSlot slot;
    slot.capacity = 4 * BYTES_PER_GB;
    slot.migratedBytes = GB3_5;
    slot.debtId = "d1";
    slot.remoteNumaId = 3;
    slot.status = def::BorrowSlotStatus::COMPLETED;
    def::BorrowState borrow;
    borrow.currentRemote = GB3_5;
    borrow.slots.push_back(slot);
    AddPidWithBorrow(pid, borrow, def::ProcessStatus::BORROWED);

    ubse::mem::controller::MockSetNumaCreateDesc(5, 9, 0);
    ubse::smap::MockSetMigrateFail(0, 3);

    auto debtId = ProcessMemPidDecision::GetInstance().RecordPendingBorrow(pid, need, 0, 1);
    ASSERT_FALSE(debtId.empty());
    ProcessMemPidDecision::GetInstance().AsyncBorrowAndMigrate(debtId, pid, need, 0, 1);

    // 部分复用: 槽 d1 空闲 0.5GB 被复用 bump 到 capacity, 剩余 1.5GB 走新建债务, 单次全量下发
    ASSERT_EQ(ubse::smap::MockGetMigrateCallCount(), 1u);
    auto calls = ubse::smap::MockGetMigrateCalls();
    ASSERT_EQ(calls.size(), 2u);
    EXPECT_EQ(calls[0].first, 3);
    EXPECT_EQ(calls[0].second, 4 * BYTES_PER_GB / 1024);
    EXPECT_EQ(calls[1].first, 5);
    EXPECT_EQ(calls[1].second, remain / 1024);
    EXPECT_EQ(ubse::smap::MockGetMigrateTargetKb(pid, 3), 0u);
    EXPECT_EQ(ubse::smap::MockGetMigrateTargetKb(pid, 5), 0u);

    // 下发失败仅作废新债务: 复用 bump 是独立持久意图, 留在账本由对账全量对齐 smap 自愈
    EXPECT_EQ(ubse::mem::controller::MockGetNumaDeleteCallCount(), 1u);
    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaDeleteName(), debtId);
    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    auto it = snapshot.find(pid);
    ASSERT_NE(it, snapshot.end());
    ASSERT_EQ(it->second.borrow.slots.size(), 1u);
    EXPECT_EQ(it->second.borrow.slots[0].migratedBytes, 4 * BYTES_PER_GB);
    EXPECT_EQ(it->second.borrow.currentRemote, 4 * BYTES_PER_GB);
    EXPECT_EQ(it->second.processStatus, def::ProcessStatus::BORROWED);
}

TEST_F(TestProcessMemPidBorrowReuse, VanishedSlotAfterCreateReturnsUbseDebt)
{
    const pid_t pid = 15964;
    constexpr uint64_t need = 1 * BYTES_PER_GB;
    auto& decision = ProcessMemPidDecision::GetInstance();
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    decision.timeoutPer128MbMs_ = 1000;

    infoMgr.AddNameSourceToManagedPid(pid, "reuse_proc", 32 * BYTES_PER_GB, 0.5);

    auto debtId = decision.RecordPendingBorrow(pid, need, 0, 1);
    ASSERT_FALSE(debtId.empty());

    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();
    auto borrow = snapshot.at(pid).borrow;
    ASSERT_EQ(borrow.slots.size(), 1u);
    borrow.slots[0].borrowTime = std::chrono::steady_clock::now() - std::chrono::seconds(60);
    infoMgr.UpdateManagedPidBorrowState(pid, borrow, def::ProcessStatus::BORROWING);
    decision.CheckTimeouts(2);
    ASSERT_EQ(infoMgr.GetManagedPidCacheSnapshot().at(pid).borrow.slots.size(), 0u);
    EXPECT_EQ(ubse::mem::controller::MockGetNumaDeleteCallCount(), 1u);

    ubse::mem::controller::MockSetNumaCreateDesc(5, 9, need);
    decision.AsyncBorrowAndMigrate(debtId, pid, need, 0, 1);
    EXPECT_EQ(ubse::mem::controller::MockGetNumaDeleteCallCount(), 2u);
    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaDeleteName(), debtId);
    snapshot = infoMgr.GetManagedPidCacheSnapshot();
    EXPECT_EQ(snapshot.at(pid).borrow.slots.size(), 0u);
}
} // namespace ubse::ut::process_mem
