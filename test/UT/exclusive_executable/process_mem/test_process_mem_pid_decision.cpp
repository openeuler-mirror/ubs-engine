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
#include "test_process_mem_pid_decision.h"

#include <unistd.h>
#include <cstring>
#include <set>

#include "ubse_error.h"
#include "ubse_serial_util.h"
#include "process_mem_pid_collect.h"
#include "process_mem_pid_config_manager.h"

namespace ubse::ut::process_mem {
using namespace ::process_mem::decision;
using namespace ::process_mem::manager;
using namespace ::process_mem::def;
using namespace ::process_mem::pid::bridge;
using namespace ::process_mem::collect;

namespace {
constexpr uint64_t GB = 1073741824ULL;
constexpr uint64_t THRESHOLD_BYTES = 50 * GB;
constexpr uint64_t EMERGENCY_BYTES = 5 * GB;

constexpr uint32_t KB_PER_GB = 1024 * 1024;

std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo> MakeLentDebts(const std::vector<std::string>& borrowers)
{
    std::vector<ubse::mem::controller::UbseNumaMemoryDebtInfo> debts;
    for (const auto& borrower : borrowers) {
        ubse::mem::controller::UbseNumaMemoryDebtInfo debt;
        debt.name = "debt-" + borrower;
        debt.borrowNodeId = borrower;
        debt.lentNodeId = "NODE0";
        debt.size = 1 * GB;
        debt.remoteNumaId = 5;
        debts.push_back(debt);
    }
    return debts;
}

ubse::mem::controller::UbseNumaMemoryDebtInfo MakeDebt(const std::string& name, const std::string& lender,
                                                       int64_t remoteNuma, int32_t pid)
{
    ubse::mem::controller::UbseNumaMemoryDebtInfo debt;
    debt.name = name;
    debt.borrowNodeId = "NODE0";
    debt.lentNodeId = lender;
    debt.size = 1 * GB;
    debt.remoteNumaId = remoteNuma;
    ProcessMemUsrInfo usr{};
    usr.pid = pid;
    usr.srcNuma = 1;
    memset(debt.usrInfo, 0, sizeof(debt.usrInfo));
    memcpy(debt.usrInfo, &usr, sizeof(usr));
    return debt;
}

BorrowState MakeBorrow(uint64_t amountGb, const std::string& debtId)
{
    BorrowState borrow;
    borrow.currentRemote = amountGb * GB;
    BorrowSlot slot;
    slot.debtId = debtId;
    slot.migratedBytes = amountGb * GB;
    slot.capacity = amountGb * GB;
    slot.status = BorrowSlotStatus::COMPLETED;
    slot.remoteNumaId = 5;
    borrow.slots.push_back(slot);
    return borrow;
}

void AddManagedPid(pid_t pid, uint64_t maxGb, double ratio, uint64_t vmRssGb, const BorrowState& borrow = {})
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.AddNameSourceToManagedPid(pid, "proc" + std::to_string(pid), maxGb * GB, ratio);
    PidCollectInfoMap collectMap;
    collectMap.entries[pid].vmRssKb = vmRssGb * 1024 * 1024;
    mgr.UpdateManagedPidVmRssBatch(collectMap);
    if (!borrow.slots.empty()) {
        mgr.UpdateManagedPidBorrowState(pid, borrow, ProcessStatus::BORROWED);
    }
}

// nodeFree 口径统一为本地 NUMA 空闲总和后，mock 直接驱动 localNumaFreeKbReader
void MockNodeFreeBytes(uint64_t bytes)
{
    ProcessMemPidDecision::localNumaFreeKbReader = [bytes]() {
        return bytes / 1024;
    };
}

void MockNodeFree(uint64_t freeGb)
{
    MockNodeFreeBytes(freeGb * GB);
}

std::vector<ReturnRequestItem> DecodeItems(const std::string& payload)
{
    ubse::serial::UbseDeSerialization deserializer{reinterpret_cast<const ubse::serial::base_ptr_type*>(payload.data()),
                                                   payload.size()};
    ubse::serial::common_len count = 0;
    deserializer >> ubse::serial::array_len_capture(count);
    std::vector<ReturnRequestItem> items;
    for (ubse::serial::common_len i = 0; i < count; ++i) {
        ReturnRequestItem item;
        deserializer >> item.name >> item.size;
        items.push_back(item);
    }
    return items;
}
} // namespace

void TestProcessMemPidDecision::SetUp()
{
    ubse::mem::controller::MockResetAllErrors();
    ubse::com::MockResetRpcState();
    ubse::nodeController::MockSetCurrentNodeId("NODE0");

    ubse::smap::MockResetMigrateState();
    ProcessMemPidBridge::rmrsMigrateOut = ubse::smap::MockRmrsMigrateOut;
    ProcessMemPidBridge::rmrsFreeWithMigrate = [](const std::string&) {
        return 0;
    };

    auto& decision = ProcessMemPidDecision::GetInstance();
    decision.stopping_ = false;
    decision.freeMemoryThresholdBytes_ = THRESHOLD_BYTES;
    decision.emergencyThresholdBytes_ = EMERGENCY_BYTES;
    decision.fastPollIntervalMs_ = 200;
    decision.observeCycles_ = 6;
    decision.returnRetryIntervalMs_ = 1;
    decision.oomFastWindowCount_ = 0;
    decision.oomFastWindowMin_ = 0;
    decision.lastEmergencyBroadcast_ = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    if (decision.borrowExecutor_ == nullptr || !decision.borrowExecutor_->IsStart()) {
        decision.borrowExecutor_ = ubse::task_executor::UbseTaskExecutor::Create("ProcMemDecisionTest", 2, 64);
        decision.borrowExecutor_->Start();
    }
    if (decision.returnExecutor_ == nullptr || !decision.returnExecutor_->IsStart()) {
        decision.returnExecutor_ = ubse::task_executor::UbseTaskExecutor::Create("ProcMemReturnTest", 2, 64);
        decision.returnExecutor_->Start();
    }
    // UT 无远端 numa sysfs: mock 实际占用量 = 已完成 slot 之和, 使 pending 语义等价于旧的在途借用口径
    ProcessMemPidDecision::remoteNumaUsedKbReader = []() -> std::optional<uint64_t> {
        uint64_t actualBytes = 0;
        auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
        for (const auto& [pid, entry] : snapshot) {
            for (const auto& s : entry.borrow.slots) {
                if (s.status == BorrowSlotStatus::COMPLETED) {
                    actualBytes += s.migratedBytes;
                }
            }
        }
        return actualBytes / 1024;
    };
}

void TestProcessMemPidDecision::TearDown()
{
    ProcessMemPidBridge::rmrsMigrateOut = {};
    ubse::smap::MockResetMigrateState();
    ProcessMemPidBridge::rmrsRemove = {};
    ProcessMemPidBridge::rmrsFreeWithMigrate = {};
    ProcessMemPidBridge::rmrsRemoteToRemote = {};
    ProcessMemPidBridge::rmrsProcessConfigQuery = {};
    ProcessMemPidDecision::localNumaFreeKbReader = nullptr;
    ProcessMemPidDecision::remoteNumaUsedKbReader = nullptr;
    ProcessMemPidCollect::GetInstance().SetCollectVmRssOverride(nullptr);

    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    for (const auto& [pid, entry] : snapshot) {
        mgr.RemoveManagedPidEntry(pid);
    }
    ubse::mem::controller::MockResetAllErrors();
    ubse::task_executor::MockSetExecutorAsync(false);
    ubse::com::MockResetRpcState();
}

TEST_F(TestProcessMemPidDecision, PassiveReturnNoBroadcastWhenFreeAboveThreshold)
{
    MockNodeFree(60);
    ubse::mem::controller::MockSetDebtInfos(MakeLentDebts({"NODE1", "NODE2"}));

    ProcessMemPidDecision::GetInstance().OnDecisionTimer();

    EXPECT_TRUE(ubse::com::MockGetRpcSendRecords().empty());
}

TEST_F(TestProcessMemPidDecision, PassiveReturnStopsWhenFreeRecovered)
{
    MockNodeFree(10);
    ubse::mem::controller::MockSetDebtInfos(MakeLentDebts({"NODE1", "NODE2"}));

    auto& decision = ProcessMemPidDecision::GetInstance();
    decision.OnDecisionTimer();
    EXPECT_EQ(ubse::com::MockGetRpcSendRecords().size(), 2u);

    MockNodeFree(60);
    decision.OnDecisionTimer();
    EXPECT_EQ(ubse::com::MockGetRpcSendRecords().size(), 2u);
}

TEST_F(TestProcessMemPidDecision, BorrowerDirectReturn)
{
    MockNodeFree(60);
    ubse::mem::controller::MockSetDebtInfos({MakeDebt("debt-x", "NODE0", 5, 1001)});
    AddManagedPid(1001, 10, 0.5, 4, MakeBorrow(1, "debt-x"));

    std::vector<std::string> freedDebts;
    ProcessMemPidBridge::rmrsFreeWithMigrate = [&freedDebts](const std::string& name) {
        freedDebts.push_back(name);
        return 0;
    };

    ReturnRequestItem item{"debt-x", 1 * GB};
    EXPECT_EQ(ProcessMemPidDecision::GetInstance().HandleReturnRequest({item}), UBSE_OK);

    ASSERT_EQ(freedDebts.size(), 1u);
    EXPECT_EQ(freedDebts[0], "debt-x");

    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    auto it = snapshot.find(1001);
    ASSERT_NE(it, snapshot.end());
    EXPECT_EQ(it->second.borrow.currentRemote, 0u);
    EXPECT_TRUE(it->second.borrow.slots.empty());
    EXPECT_EQ(it->second.processStatus, ProcessStatus::IDLE);
}

TEST_F(TestProcessMemPidDecision, TimeoutReturnEnqueuedAndSlotRemoved)
{
    const pid_t pid = 1001;
    const std::string debtId = "debt-timeout";
    auto& decision = ProcessMemPidDecision::GetInstance();
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    decision.timeoutPer128MbMs_ = 1000;

    BorrowState borrow;
    BorrowSlot slot;
    slot.debtId = debtId;
    slot.capacity = 1 * GB;
    slot.migratedBytes = 1 * GB;
    slot.remoteNumaId = 3;
    slot.status = BorrowSlotStatus::BORROWING;
    borrow.slots.push_back(slot);
    AddManagedPid(pid, 10, 0.5, 4, borrow);

    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();
    auto stale = snapshot.at(pid).borrow;
    stale.slots[0].borrowTime = std::chrono::steady_clock::now() - std::chrono::seconds(60);
    infoMgr.UpdateManagedPidBorrowState(pid, stale, ProcessStatus::BORROWING);

    ubse::task_executor::MockSetExecutorAsync(true);
    ubse::mem::controller::MockBlockNumaDelete();
    decision.CheckTimeouts(2);

    ASSERT_TRUE(ubse::mem::controller::MockWaitNumaDeleteEntered(5000));
    snapshot = infoMgr.GetManagedPidCacheSnapshot();
    EXPECT_TRUE(snapshot.at(pid).borrow.slots.empty());
    EXPECT_EQ(snapshot.at(pid).borrow.currentRemote, 0u);
    EXPECT_EQ(ubse::mem::controller::MockGetNumaDeleteCallCount(), 1u);
    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaDeleteName(), debtId);

    ubse::mem::controller::MockReleaseNumaDelete();
    ubse::task_executor::MockWaitExecutorIdle();
    EXPECT_EQ(ubse::mem::controller::MockGetNumaDeleteCallCount(), 1u);

    ubse::task_executor::MockSetExecutorAsync(false);
}

TEST_F(TestProcessMemPidDecision, OomDetectorAlwaysFastPoll)
{
    ProcessMemPidDecision::localNumaFreeKbReader = []() {
        return 200 * KB_PER_GB;
    };

    auto& decision = ProcessMemPidDecision::GetInstance();
    EXPECT_EQ(decision.OomPollOnce(), 200u);
    EXPECT_EQ(decision.oomFastWindowCount_, 1u);
    EXPECT_EQ(decision.oomFastWindowMin_, 200 * GB);
}

TEST_F(TestProcessMemPidDecision, OomDetectorFastPoll)
{
    ProcessMemPidDecision::localNumaFreeKbReader = []() {
        return 100 * KB_PER_GB;
    };
    MockNodeFree(10);

    auto& decision = ProcessMemPidDecision::GetInstance();
    EXPECT_EQ(decision.OomPollOnce(), 200u);
    EXPECT_EQ(decision.oomFastWindowCount_, 1u);
    EXPECT_EQ(decision.oomFastWindowMin_, 10 * GB);
}

TEST_F(TestProcessMemPidDecision, OomEmergencyBorrow)
{
    ProcessMemPidDecision::localNumaFreeKbReader = []() {
        return 3 * KB_PER_GB;
    };
    AddManagedPid(1001, 10, 0.5, 4);

    int migrateCalls = 0;
    ProcessMemPidBridge::rmrsMigrateOut = [&migrateCalls](const std::vector<mempooling::smap::MigrateOutPayload>&,
                                                          int) {
        ++migrateCalls;
        return 0;
    };

    ProcessMemPidDecision::GetInstance().OomPollOnce();

    EXPECT_EQ(migrateCalls, 1);
    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    auto it = snapshot.find(1001);
    ASSERT_NE(it, snapshot.end());
    EXPECT_EQ(it->second.borrow.currentRemote, 2 * GB);
    EXPECT_EQ(it->second.borrow.slots.size(), 1u);
}

TEST_F(TestProcessMemPidDecision, OomEmergencyAllowsExistingDebt)
{
    ProcessMemPidDecision::localNumaFreeKbReader = []() {
        return 3 * KB_PER_GB;
    };
    AddManagedPid(1001, 10, 0.5, 4, MakeBorrow(1, "debt-x"));

    int migrateCalls = 0;
    ProcessMemPidBridge::rmrsMigrateOut = [&migrateCalls](const std::vector<mempooling::smap::MigrateOutPayload>&,
                                                          int) {
        ++migrateCalls;
        return 0;
    };

    ProcessMemPidDecision::GetInstance().OomPollOnce();

    EXPECT_EQ(migrateCalls, 1);
    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    auto it = snapshot.find(1001);
    ASSERT_NE(it, snapshot.end());
    EXPECT_EQ(it->second.borrow.currentRemote, 2 * GB);
    EXPECT_EQ(it->second.borrow.slots.size(), 2u);
}

TEST_F(TestProcessMemPidDecision, OomLenderEmergencyNotify)
{
    ProcessMemPidDecision::localNumaFreeKbReader = []() {
        return 3 * KB_PER_GB;
    };
    ubse::mem::controller::MockSetDebtInfos(MakeLentDebts({"NODE1", "NODE2"}));

    ProcessMemPidDecision::GetInstance().OomPollOnce();

    EXPECT_EQ(ubse::com::MockGetRpcSendRecords().size(), 2u);
}

TEST_F(TestProcessMemPidDecision, OomActiveReturnTrigger)
{
    ProcessMemPidDecision::localNumaFreeKbReader = []() {
        return 100 * KB_PER_GB;
    };
    MockNodeFree(60);
    AddManagedPid(1001, 10, 0.5, 4, MakeBorrow(2, "debt-a"));
    AddManagedPid(1002, 10, 0.5, 4, MakeBorrow(1, "debt-b"));

    auto& decision = ProcessMemPidDecision::GetInstance();
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(decision.OomPollOnce(), 200u);
    }
    EXPECT_EQ(decision.oomFastWindowCount_, 5u);

    decision.OomPollOnce();

    EXPECT_EQ(ubse::mem::controller::MockGetNumaDeleteCallCount(), 2);
    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaDeleteName(), "debt-b");
    EXPECT_EQ(decision.oomFastWindowCount_, 0u);

    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    for (pid_t pid : {1001, 1002}) {
        auto it = snapshot.find(pid);
        ASSERT_NE(it, snapshot.end());
        EXPECT_EQ(it->second.borrow.currentRemote, 0u);
        EXPECT_TRUE(it->second.borrow.slots.empty());
        EXPECT_EQ(it->second.processStatus, ProcessStatus::IDLE);
    }
}

TEST_F(TestProcessMemPidDecision, OomActiveCounterReset)
{
    ProcessMemPidDecision::localNumaFreeKbReader = []() {
        return 100 * KB_PER_GB;
    };
    MockNodeFree(60);
    AddManagedPid(1001, 10, 0.5, 4, MakeBorrow(1, "debt-x"));

    auto& decision = ProcessMemPidDecision::GetInstance();
    for (int i = 0; i < 3; ++i) {
        decision.OomPollOnce();
    }
    MockNodeFree(30);
    decision.OomPollOnce();
    EXPECT_EQ(decision.oomFastWindowMin_, 30 * GB);

    for (int i = 0; i < 2; ++i) {
        decision.OomPollOnce();
    }
    EXPECT_EQ(decision.oomFastWindowCount_, 0u);
    EXPECT_EQ(ubse::mem::controller::MockGetNumaDeleteCallCount(), 0);
}

TEST_F(TestProcessMemPidDecision, OomActiveReturnSkipOversizeDebt)
{
    ProcessMemPidDecision::localNumaFreeKbReader = []() {
        return 100 * KB_PER_GB;
    };
    MockNodeFree(55);
    AddManagedPid(1001, 10, 0.5, 4, MakeBorrow(8, "debt-big"));
    AddManagedPid(1002, 10, 0.5, 4, MakeBorrow(3, "debt-ok"));

    auto& decision = ProcessMemPidDecision::GetInstance();
    for (int i = 0; i < 6; ++i) {
        decision.OomPollOnce();
    }

    EXPECT_EQ(ubse::mem::controller::MockGetNumaDeleteCallCount(), 1);
    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaDeleteName(), "debt-ok");

    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    auto big = snapshot.find(1001);
    ASSERT_NE(big, snapshot.end());
    EXPECT_EQ(big->second.borrow.currentRemote, 8 * GB);
    auto ok = snapshot.find(1002);
    ASSERT_NE(ok, snapshot.end());
    EXPECT_EQ(ok->second.borrow.currentRemote, 0u);
}

TEST_F(TestProcessMemPidDecision, OomActiveReturnDeleteFailRetriesUntilSuccess)
{
    ProcessMemPidDecision::localNumaFreeKbReader = []() {
        return 100 * KB_PER_GB;
    };
    MockNodeFree(60);
    AddManagedPid(1001, 10, 0.5, 4, MakeBorrow(2, "debt-a"));
    ubse::mem::controller::MockSetNumaDeleteErrorOnce(UBSE_ERROR);

    auto& decision = ProcessMemPidDecision::GetInstance();
    for (int i = 0; i < 6; ++i) {
        decision.OomPollOnce();
    }

    EXPECT_EQ(ubse::mem::controller::MockGetNumaDeleteCallCount(), 2);
    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    auto it = snapshot.find(1001);
    ASSERT_NE(it, snapshot.end());
    EXPECT_EQ(it->second.borrow.currentRemote, 0u);
    EXPECT_TRUE(it->second.borrow.slots.empty());
    EXPECT_EQ(it->second.processStatus, ProcessStatus::IDLE);

    for (int i = 0; i < 6; ++i) {
        decision.OomPollOnce();
    }
    EXPECT_EQ(ubse::mem::controller::MockGetNumaDeleteCallCount(), 2);
}

TEST_F(TestProcessMemPidDecision, OomActiveReturnNotExistCleansLedger)
{
    ProcessMemPidDecision::localNumaFreeKbReader = []() {
        return 100 * KB_PER_GB;
    };
    MockNodeFree(60);
    AddManagedPid(1001, 10, 0.5, 4, MakeBorrow(2, "debt-a"));
    ubse::mem::controller::MockSetNumaDeleteError(UBSE_ERR_NOT_EXIST);

    auto& decision = ProcessMemPidDecision::GetInstance();
    for (int i = 0; i < 6; ++i) {
        decision.OomPollOnce();
    }
    EXPECT_EQ(ubse::mem::controller::MockGetNumaDeleteCallCount(), 1);

    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    auto it = snapshot.find(1001);
    ASSERT_NE(it, snapshot.end());
    EXPECT_EQ(it->second.borrow.currentRemote, 0u);
    EXPECT_TRUE(it->second.borrow.slots.empty());
    EXPECT_EQ(it->second.processStatus, ProcessStatus::IDLE);
}

TEST_F(TestProcessMemPidDecision, OomRearrangeMigratedBigFirst)
{
    const pid_t pid = 15980;
    BorrowState borrow;
    BorrowSlot big;
    big.debtId = "debt-big";
    big.capacity = 8 * GB;
    big.migratedBytes = 1 * GB;
    big.remoteNumaId = 5;
    big.status = BorrowSlotStatus::COMPLETED;
    BorrowSlot mid;
    mid.debtId = "debt-mid";
    mid.capacity = 4 * GB;
    mid.migratedBytes = 4 * GB;
    mid.remoteNumaId = 5;
    mid.status = BorrowSlotStatus::COMPLETED;
    BorrowSlot sml;
    sml.debtId = "debt-sml";
    sml.capacity = 2 * GB;
    sml.migratedBytes = 2 * GB;
    sml.remoteNumaId = 5;
    sml.status = BorrowSlotStatus::COMPLETED;
    BorrowSlot numa9;
    numa9.debtId = "debt-n9";
    numa9.capacity = 5 * GB;
    numa9.migratedBytes = 3 * GB;
    numa9.remoteNumaId = 9;
    numa9.status = BorrowSlotStatus::COMPLETED;
    BorrowSlot inFlight;
    inFlight.debtId = "debt-inflight";
    inFlight.capacity = 3 * GB;
    inFlight.migratedBytes = 2 * GB;
    inFlight.remoteNumaId = 5;
    inFlight.status = BorrowSlotStatus::BORROWING;
    BorrowSlot returning;
    returning.debtId = "debt-ret";
    returning.capacity = 6 * GB;
    returning.migratedBytes = 6 * GB;
    returning.remoteNumaId = 5;
    returning.status = BorrowSlotStatus::RETURNING;
    borrow.slots = {big, mid, sml, numa9, inFlight, returning};
    borrow.currentRemote = 16 * GB;
    AddManagedPid(pid, 32, 0.5, 16, borrow);

    ProcessMemPidDecision::GetInstance().OomRearrangeMigrated();

    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    auto it = snapshot.find(pid);
    ASSERT_NE(it, snapshot.end());
    const auto& slots = it->second.borrow.slots;
    ASSERT_EQ(slots.size(), 6u);
    auto mig = [&slots](const std::string& debtId) -> uint64_t {
        for (const auto& s : slots) {
            if (s.debtId == debtId) {
                return s.migratedBytes;
            }
        }
        return ~0ULL;
    };
    EXPECT_EQ(mig("debt-big"), 7 * GB);
    EXPECT_EQ(mig("debt-mid"), 0u);
    EXPECT_EQ(mig("debt-sml"), 0u);
    EXPECT_EQ(mig("debt-n9"), 3 * GB);
    EXPECT_EQ(mig("debt-inflight"), 2 * GB);
    EXPECT_EQ(mig("debt-ret"), 6 * GB);
    EXPECT_EQ(it->second.borrow.currentRemote, 16 * GB);
    EXPECT_EQ(it->second.processStatus, ProcessStatus::BORROWED);
}

TEST_F(TestProcessMemPidDecision, OomActiveReturnUsesMigratedBytes)
{
    ProcessMemPidDecision::localNumaFreeKbReader = []() {
        return 100 * KB_PER_GB;
    };
    MockNodeFree(55);
    const pid_t pid = 1003;
    BorrowState borrow;
    BorrowSlot big;
    big.debtId = "debt-big";
    big.capacity = 8 * GB;
    big.migratedBytes = 8 * GB;
    big.remoteNumaId = 5;
    big.status = BorrowSlotStatus::COMPLETED;
    BorrowSlot mid;
    mid.debtId = "debt-mid";
    mid.capacity = 4 * GB;
    mid.migratedBytes = 2 * GB;
    mid.remoteNumaId = 5;
    mid.status = BorrowSlotStatus::COMPLETED;
    BorrowSlot empty;
    empty.debtId = "debt-empty";
    empty.capacity = 2 * GB;
    empty.migratedBytes = 0;
    empty.remoteNumaId = 5;
    empty.status = BorrowSlotStatus::COMPLETED;
    borrow.slots = {big, mid, empty};
    borrow.currentRemote = 10 * GB;
    AddManagedPid(pid, 32, 0.5, 16, borrow);

    auto& decision = ProcessMemPidDecision::GetInstance();
    for (int i = 0; i < 6; ++i) {
        decision.OomPollOnce();
    }

    EXPECT_EQ(ubse::mem::controller::MockGetNumaDeleteCallCount(), 2);
    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaDeleteName(), "debt-empty");
    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    auto it = snapshot.find(pid);
    ASSERT_NE(it, snapshot.end());
    EXPECT_EQ(it->second.borrow.currentRemote, 8 * GB);
    ASSERT_EQ(it->second.borrow.slots.size(), 1u);
    EXPECT_EQ(it->second.borrow.slots[0].debtId, "debt-big");
    EXPECT_EQ(it->second.borrow.slots[0].migratedBytes, 8 * GB);
}

ubse::mem::controller::UbseNumaMemoryImportDebtInfo MakeImportDebt(const std::string& name, uint64_t size,
                                                                   int64_t remoteNuma, int32_t pid, int64_t startTime)
{
    ubse::mem::controller::UbseNumaMemoryImportDebtInfo debt;
    debt.name = name;
    debt.size = size;
    debt.remoteNumaId = remoteNuma;
    memset(debt.usrInfo, 0, sizeof(debt.usrInfo));
    ProcessMemUsrInfo usr{};
    usr.pluginId = UsrInfoPluginType::PROCESS_MEM;
    usr.pid = pid;
    usr.startTime = startTime;
    usr.srcNuma = 1;
    memcpy(debt.usrInfo, &usr, sizeof(usr));
    return debt;
}

TEST_F(TestProcessMemPidDecision, RecoverBorrowOrphanReleasedWhenPidMissing)
{
    ubse::mem::controller::MockSetImportDebtInfos({MakeImportDebt("debt-orphan", 1 * GB, 5, 1001, 123456)});

    EXPECT_EQ(ProcessMemPidDecision::GetInstance().ReconcileLedgerWithCache(), UBSE_OK);
    ASSERT_TRUE(ubse::mem::controller::MockWaitNumaDeleteEntered(5000));

    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaDeleteName(), "debt-orphan");
}

TEST_F(TestProcessMemPidDecision, RecoverBorrowOrphanRetriedUntilSuccess)
{
    ProcessMemPidDecision::GetInstance().returnRetryIntervalMs_ = 1;
    ubse::mem::controller::MockSetNumaDeleteErrorOnce(UBSE_ERR_TIMEOUT);
    ubse::mem::controller::MockSetImportDebtInfos({MakeImportDebt("debt-retry", 1 * GB, 5, 1003, 123456)});

    EXPECT_EQ(ProcessMemPidDecision::GetInstance().ReconcileLedgerWithCache(), UBSE_OK);
    // 孤儿归还异步执行, 首次失败后 1ms 重试, 轮询等待第二次删除调用
    for (int i = 0; i < 500 && ubse::mem::controller::MockGetNumaDeleteCallCount() < 2; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_GE(ubse::mem::controller::MockGetNumaDeleteCallCount(), 2u);
    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaDeleteName(), "debt-retry");
}

TEST_F(TestProcessMemPidDecision, RecoverBorrowOrphanNotExistStopsRetry)
{
    ProcessMemPidDecision::GetInstance().returnRetryIntervalMs_ = 1;
    ubse::mem::controller::MockSetNumaDeleteErrorOnce(UBSE_ERR_NOT_EXIST);
    ubse::mem::controller::MockSetImportDebtInfos({MakeImportDebt("debt-gone", 1 * GB, 5, 1004, 123456)});

    EXPECT_EQ(ProcessMemPidDecision::GetInstance().ReconcileLedgerWithCache(), UBSE_OK);
    ASSERT_TRUE(ubse::mem::controller::MockWaitNumaDeleteEntered(5000));
    // NOT_EXIST 视为完成不重试, 短暂等待异步线程收尾后断言次数
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(ubse::mem::controller::MockGetNumaDeleteCallCount(), 1u);
    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaDeleteName(), "debt-gone");
}

TEST_F(TestProcessMemPidDecision, RecoverBorrowOrphanReleasedOnStartTimeMismatch)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    pid_t pid = getpid();
    AddManagedPid(pid, 10, 0.5, 2);
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(pid);
    ASSERT_NE(startTime, 0u);

    ubse::mem::controller::MockSetImportDebtInfos(
        {MakeImportDebt("debt-stale", 1 * GB, 5, pid, static_cast<int64_t>(startTime) + 1)});

    EXPECT_EQ(ProcessMemPidDecision::GetInstance().ReconcileLedgerWithCache(), UBSE_OK);
    ASSERT_TRUE(ubse::mem::controller::MockWaitNumaDeleteEntered(5000));

    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaDeleteName(), "debt-stale");
    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    EXPECT_TRUE(snapshot.at(pid).borrow.slots.empty());
}

TEST_F(TestProcessMemPidDecision, RecoverBorrowSkipsNonProcessMemUsrInfo)
{
    ubse::mem::controller::UbseNumaMemoryImportDebtInfo other;
    other.name = "debt-other";
    other.size = 1 * GB;
    other.remoteNumaId = 5;
    memset(other.usrInfo, 0, sizeof(other.usrInfo));

    ubse::mem::controller::UbseNumaMemoryImportDebtInfo badPid;
    badPid.name = "debt-badpid";
    badPid.size = 1 * GB;
    badPid.remoteNumaId = 5;
    memset(badPid.usrInfo, 0, sizeof(badPid.usrInfo));
    ProcessMemUsrInfo usr{};
    usr.pluginId = UsrInfoPluginType::PROCESS_MEM;
    usr.pid = -1;
    memcpy(badPid.usrInfo, &usr, sizeof(usr));

    ubse::mem::controller::MockSetImportDebtInfos({other, badPid});

    EXPECT_EQ(ProcessMemPidDecision::GetInstance().ReconcileLedgerWithCache(), UBSE_OK);

    EXPECT_TRUE(ubse::mem::controller::MockGetLastNumaDeleteName().empty());
}

TEST_F(TestProcessMemPidDecision, RecoverSmapConfigUnavailable)
{
    ProcessMemPidBridge::rmrsProcessConfigQuery = {};
    EXPECT_EQ(ProcessMemPidDecision::GetInstance().RecoverSmapProcessConfig(), 0u);
}

TEST_F(TestProcessMemPidDecision, RecoverSmapConfigReportsDirtyStates)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    pid_t pid = getpid();
    AddManagedPid(pid, 10, 0.5, 2, MakeBorrow(1, "debt-x"));
    AddManagedPid(2001, 10, 0.5, 2);
    ubse::mem::controller::MockSetImportDebtInfos({MakeImportDebt("debt-numa5", 1 * GB, 5, 1000, 0)});

    std::set<int> queriedNumas;
    ProcessMemPidBridge::rmrsProcessConfigQuery =
        [&queriedNumas, pid](int nid, mempooling::smap::ProcessPayload* payloads, int capacity, int* realLen) {
            queriedNumas.insert(nid);
            if (nid != 5) {
                *realLen = 0;
                return 0;
            }
            mempooling::smap::ProcessPayload p1{};
            p1.pid = pid;
            p1.scanType = 1;
            p1.memSize = 1 * 1024 * 1024;
            mempooling::smap::ProcessPayload p2{};
            p2.pid = 2002;
            p2.scanType = 1;
            p2.memSize = 2 * 1024 * 1024;
            mempooling::smap::ProcessPayload p3{};
            p3.pid = 2001;
            p3.scanType = 1;
            p3.memSize = 1 * 1024 * 1024;
            *realLen = 3;
            payloads[0] = p1;
            payloads[1] = p2;
            payloads[2] = p3;
            return 0;
        };

    EXPECT_EQ(ProcessMemPidDecision::GetInstance().RecoverSmapProcessConfig(), 2u);
    EXPECT_EQ(queriedNumas, std::set<int>({5}));
}

TEST_F(TestProcessMemPidDecision, RecoverSmapConfigQueryAllFail)
{
    ubse::mem::controller::MockSetImportDebtInfos({MakeImportDebt("debt-numa5", 1 * GB, 5, 1000, 0)});
    ProcessMemPidBridge::rmrsProcessConfigQuery = [](int, mempooling::smap::ProcessPayload*, int, int*) {
        return -1;
    };
    EXPECT_EQ(ProcessMemPidDecision::GetInstance().RecoverSmapProcessConfig(), 0u);
}

TEST_F(TestProcessMemPidDecision, RecoverSmapConfigMultiRemoteNumaAccumulates)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    pid_t pid = getpid();
    AddManagedPid(pid, 10, 0.5, 2, MakeBorrow(1, "debt-x"));
    ubse::mem::controller::MockSetImportDebtInfos(
        {MakeImportDebt("debt-numa5", 1 * GB, 5, 1000, 0), MakeImportDebt("debt-numa7", 1 * GB, 7, 1000, 0)});
    ProcessMemPidBridge::rmrsProcessConfigQuery = [pid](int nid, mempooling::smap::ProcessPayload* payloads, int,
                                                        int* realLen) {
        if (nid != 5 && nid != 7) {
            *realLen = 0;
            return 0;
        }
        mempooling::smap::ProcessPayload p{};
        p.pid = pid;
        p.scanType = 1;
        p.memSize = 1 * 1024 * 1024;
        *realLen = 1;
        payloads[0] = p;
        return 0;
    };

    EXPECT_EQ(ProcessMemPidDecision::GetInstance().RecoverSmapProcessConfig(), 1u);
    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    const auto& borrow = snapshot.at(pid).borrow;
    ASSERT_EQ(borrow.slots.size(), 1u);
    EXPECT_EQ(borrow.slots[0].migratedBytes, 1 * GB);
    // currentRemote/remoteNumaMigrated 为账本 COMPLETED 槽权威投影, smap 多出的 numa7 无槽不认领
    EXPECT_EQ(borrow.currentRemote, 1 * GB);
    EXPECT_EQ(borrow.remoteNumaMigrated.size(), 1u);
    EXPECT_EQ(borrow.remoteNumaMigrated.at(5), 1 * GB);
    EXPECT_EQ(borrow.remoteNumaMigrated.count(7), 0u);
}

TEST_F(TestProcessMemPidDecision, RecoverSmapConfigFillsAcrossMultipleSlots)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    pid_t pid = getpid();
    BorrowState borrow;
    BorrowSlot s5;
    s5.debtId = "debt-a";
    s5.migratedBytes = 1 * GB;
    s5.capacity = 1 * GB;
    s5.status = BorrowSlotStatus::COMPLETED;
    s5.remoteNumaId = 5;
    BorrowSlot s7 = s5;
    s7.debtId = "debt-b";
    s7.remoteNumaId = 7;
    borrow.slots = {s5, s7};
    borrow.currentRemote = 2 * GB;
    AddManagedPid(pid, 10, 0.5, 2, borrow);
    ubse::mem::controller::MockSetImportDebtInfos(
        {MakeImportDebt("debt-numa5", 1 * GB, 5, 1000, 0), MakeImportDebt("debt-numa7", 1 * GB, 7, 1000, 0)});

    ProcessMemPidBridge::rmrsProcessConfigQuery = [pid](int nid, mempooling::smap::ProcessPayload* payloads, int,
                                                        int* realLen) {
        if (nid != 5 && nid != 7) {
            *realLen = 0;
            return 0;
        }
        mempooling::smap::ProcessPayload p{};
        p.pid = pid;
        p.scanType = 1;
        p.memSize = 1 * 1024 * 1024;
        *realLen = 1;
        payloads[0] = p;
        return 0;
    };

    EXPECT_EQ(ProcessMemPidDecision::GetInstance().RecoverSmapProcessConfig(), 0u);
    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    const auto& borrowAfter = snapshot.at(pid).borrow;
    ASSERT_EQ(borrowAfter.slots.size(), 2u);
    EXPECT_EQ(borrowAfter.slots[0].migratedBytes, 1 * GB);
    EXPECT_EQ(borrowAfter.slots[1].migratedBytes, 1 * GB);
    EXPECT_EQ(borrowAfter.currentRemote, 2 * GB);
    EXPECT_EQ(borrowAfter.remoteNumaMigrated.at(5), 1 * GB);
    EXPECT_EQ(borrowAfter.remoteNumaMigrated.at(7), 1 * GB);
}

TEST_F(TestProcessMemPidDecision, RecoverSmapConfigFillsSlotsByCapacityInOrder)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    pid_t pid = getpid();
    BorrowState borrow;
    BorrowSlot s1;
    s1.debtId = "debt-big";
    s1.migratedBytes = 2 * GB;
    s1.capacity = 2 * GB;
    s1.status = BorrowSlotStatus::COMPLETED;
    s1.remoteNumaId = 5;
    BorrowSlot s2 = s1;
    s2.debtId = "debt-small";
    s2.migratedBytes = 1 * GB;
    s2.capacity = 1 * GB;
    borrow.slots = {s1, s2};
    borrow.currentRemote = 3 * GB;
    AddManagedPid(pid, 10, 0.5, 2, borrow);
    ubse::mem::controller::MockSetImportDebtInfos({MakeImportDebt("debt-numa5", 1 * GB, 5, 1000, 0)});

    ProcessMemPidBridge::rmrsProcessConfigQuery = [pid](int nid, mempooling::smap::ProcessPayload* payloads, int,
                                                        int* realLen) {
        if (nid != 5) {
            *realLen = 0;
            return 0;
        }
        mempooling::smap::ProcessPayload p{};
        p.pid = pid;
        p.scanType = 1;
        p.memSize = 1536 * 1024;
        *realLen = 1;
        payloads[0] = p;
        return 0;
    };

    EXPECT_EQ(ProcessMemPidDecision::GetInstance().RecoverSmapProcessConfig(), 0u);
    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    const auto& borrowAfter = snapshot.at(pid).borrow;
    ASSERT_EQ(borrowAfter.slots.size(), 2u);
    // COMPLETED 槽 migratedBytes 为账本权威值, 实测不覆盖
    EXPECT_EQ(borrowAfter.slots[0].migratedBytes, 2 * GB);
    EXPECT_EQ(borrowAfter.slots[1].migratedBytes, 1 * GB);
    EXPECT_EQ(borrowAfter.currentRemote, 3 * GB);
}

TEST_F(TestProcessMemPidDecision, RecoverSmapConfigFillsByCapacityDescIgnoreArrayOrder)
{
    pid_t pid = getpid();
    BorrowState borrow;
    BorrowSlot smallFirst;
    smallFirst.debtId = "debt-small";
    smallFirst.migratedBytes = 1 * GB;
    smallFirst.capacity = 1 * GB;
    smallFirst.status = BorrowSlotStatus::COMPLETED;
    smallFirst.remoteNumaId = 5;
    BorrowSlot bigSecond;
    bigSecond.debtId = "debt-big";
    bigSecond.migratedBytes = 2 * GB;
    bigSecond.capacity = 2 * GB;
    bigSecond.status = BorrowSlotStatus::COMPLETED;
    bigSecond.remoteNumaId = 5;
    borrow.slots = {smallFirst, bigSecond};
    borrow.currentRemote = 3 * GB;
    AddManagedPid(pid, 10, 0.5, 2, borrow);
    ubse::mem::controller::MockSetImportDebtInfos({MakeImportDebt("debt-numa5", 1 * GB, 5, 1000, 0)});

    ProcessMemPidBridge::rmrsProcessConfigQuery = [pid](int nid, mempooling::smap::ProcessPayload* payloads, int,
                                                        int* realLen) {
        if (nid != 5) {
            *realLen = 0;
            return 0;
        }
        mempooling::smap::ProcessPayload p{};
        p.pid = pid;
        p.scanType = 1;
        p.memSize = 1536 * 1024;
        *realLen = 1;
        payloads[0] = p;
        return 0;
    };

    EXPECT_EQ(ProcessMemPidDecision::GetInstance().RecoverSmapProcessConfig(), 0u);
    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    const auto& borrowAfter = snapshot.at(pid).borrow;
    ASSERT_EQ(borrowAfter.slots.size(), 2u);
    // COMPLETED 槽 migratedBytes 为账本权威值, 实测不覆盖
    EXPECT_EQ(borrowAfter.slots[0].migratedBytes, 1 * GB);
    EXPECT_EQ(borrowAfter.slots[1].migratedBytes, 2 * GB);
    EXPECT_EQ(borrowAfter.currentRemote, 3 * GB);
}

TEST_F(TestProcessMemPidDecision, RecoverSmapConfigFillsRemainderWhenCapacityExceeded)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    pid_t pid = getpid();
    BorrowState borrow;
    BorrowSlot slot;
    slot.debtId = "debt-2g";
    slot.capacity = 2 * GB;
    slot.migratedBytes = slot.capacity;
    slot.status = BorrowSlotStatus::COMPLETED;
    slot.remoteNumaId = 5;
    borrow.slots = {slot};
    borrow.currentRemote = slot.capacity;
    AddManagedPid(pid, 10, 0.5, 2, borrow);
    ubse::mem::controller::MockSetImportDebtInfos({MakeImportDebt("debt-numa5", 2 * GB, 5, 1000, 0)});

    ProcessMemPidBridge::rmrsProcessConfigQuery = [pid](int nid, mempooling::smap::ProcessPayload* payloads, int,
                                                        int* realLen) {
        if (nid != 5) {
            *realLen = 0;
            return 0;
        }
        mempooling::smap::ProcessPayload p{};
        p.pid = pid;
        p.scanType = 1;
        p.memSize = 1536 * 1024;
        *realLen = 1;
        payloads[0] = p;
        return 0;
    };

    EXPECT_EQ(ProcessMemPidDecision::GetInstance().RecoverSmapProcessConfig(), 0u);
    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    const auto& borrowAfter = snapshot.at(pid).borrow;
    ASSERT_EQ(borrowAfter.slots.size(), 1u);
    // COMPLETED 槽 migratedBytes/remoteNumaMigrated 为账本权威值, 实测不覆盖
    EXPECT_EQ(borrowAfter.slots[0].migratedBytes, 2 * GB);
    EXPECT_EQ(borrowAfter.currentRemote, 2 * GB);
    EXPECT_EQ(borrowAfter.remoteNumaMigrated.at(5), 2 * GB);
}

TEST_F(TestProcessMemPidDecision, RecoverSmapConfigIgnoresHamProcesses)
{
    ubse::mem::controller::MockSetImportDebtInfos({MakeImportDebt("debt-numa5", 1 * GB, 5, 1000, 0)});
    ProcessMemPidBridge::rmrsProcessConfigQuery = [](int nid, mempooling::smap::ProcessPayload* payloads, int,
                                                     int* realLen) {
        if (nid != 5) {
            *realLen = 0;
            return 0;
        }
        mempooling::smap::ProcessPayload p{};
        p.pid = 3001;
        p.scanType = 0;
        p.memSize = 4 * 1024 * 1024;
        *realLen = 1;
        payloads[0] = p;
        return 0;
    };

    EXPECT_EQ(ProcessMemPidDecision::GetInstance().RecoverSmapProcessConfig(), 0u);
}

TEST_F(TestProcessMemPidDecision, MultiCycleBorrowingLedgerVisibleAndNoDuplicateBorrow)
{
    const pid_t pid = 15970;
    constexpr uint64_t maxGb = 32;
    constexpr double ratio = 0.5;
    constexpr uint64_t vmRssGb = 20;
    AddManagedPid(pid, maxGb, ratio, vmRssGb);

    ProcessMemPidBridge::rmrsMigrateOut = [](const std::vector<mempooling::smap::MigrateOutPayload>&, int) {
        return 0;
    };

    auto& decision = ProcessMemPidDecision::GetInstance();
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();

    ubse::task_executor::MockSetExecutorAsync(true);
    ubse::mem::controller::MockBlockNumaCreate();

    MockNodeFree(10);
    std::thread round1Thread([&decision]() { decision.OnDecisionTimer(); });
    round1Thread.join();
    ASSERT_TRUE(ubse::mem::controller::MockWaitNumaCreateEntered(5000));

    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(pid);
    ASSERT_NE(it, snapshot.end());
    ASSERT_EQ(it->second.borrow.slots.size(), 1u);
    EXPECT_EQ(it->second.borrow.slots[0].migratedBytes, 10 * GB);
    EXPECT_FALSE(it->second.borrow.slots[0].debtId.empty());
    EXPECT_EQ(it->second.borrow.slots[0].status, BorrowSlotStatus::BORROWING);
    EXPECT_EQ(it->second.processStatus, ProcessStatus::BORROWING);
    std::string firstDebtId = it->second.borrow.slots[0].debtId;
    EXPECT_EQ(decision.GetPendingMigrateTotal(), 10 * GB);

    std::thread round2Thread([&decision]() { decision.OnDecisionTimer(); });
    round2Thread.join();
    snapshot = infoMgr.GetManagedPidCacheSnapshot();
    it = snapshot.find(pid);
    ASSERT_NE(it, snapshot.end());
    ASSERT_EQ(it->second.borrow.slots.size(), 1u);
    EXPECT_EQ(it->second.borrow.slots[0].debtId, firstDebtId);
    EXPECT_EQ(it->second.borrow.slots[0].status, BorrowSlotStatus::BORROWING);
    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaCreateName(), firstDebtId);
    EXPECT_EQ(decision.GetPendingMigrateTotal(), 10 * GB);

    ubse::mem::controller::MockReleaseNumaCreate();
    ubse::task_executor::MockWaitExecutorIdle();
    snapshot = infoMgr.GetManagedPidCacheSnapshot();
    it = snapshot.find(pid);
    ASSERT_NE(it, snapshot.end());
    ASSERT_EQ(it->second.borrow.slots.size(), 1u);
    EXPECT_EQ(it->second.borrow.slots[0].debtId, firstDebtId);
    EXPECT_EQ(it->second.borrow.slots[0].migratedBytes, 10 * GB);
    EXPECT_EQ(it->second.borrow.slots[0].capacity, 10 * GB);
    EXPECT_EQ(it->second.borrow.slots[0].remoteNumaId, 3);
    EXPECT_EQ(it->second.borrow.slots[0].status, BorrowSlotStatus::COMPLETED);
    EXPECT_EQ(it->second.borrow.currentRemote, 10 * GB);
    EXPECT_EQ(it->second.processStatus, ProcessStatus::BORROWED);
    EXPECT_EQ(decision.GetPendingMigrateTotal(), 0u);

    std::thread round3Thread([&decision]() { decision.OnDecisionTimer(); });
    round3Thread.join();
    snapshot = infoMgr.GetManagedPidCacheSnapshot();
    it = snapshot.find(pid);
    ASSERT_NE(it, snapshot.end());
    ASSERT_EQ(it->second.borrow.slots.size(), 1u);
    EXPECT_EQ(it->second.borrow.slots[0].debtId, firstDebtId);
    EXPECT_EQ(it->second.borrow.slots[0].status, BorrowSlotStatus::COMPLETED);
    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaCreateName(), firstDebtId);
    EXPECT_EQ(it->second.borrow.currentRemote, 10 * GB);

    ubse::task_executor::MockSetExecutorAsync(false);
}

TEST_F(TestProcessMemPidDecision, NodeFreeChangeConsidersInFlightBorrow)
{
    const pid_t pid = 15970;
    constexpr uint64_t maxGb = 32;
    constexpr double ratio = 0.5;
    AddManagedPid(pid, maxGb, ratio, 40);

    auto& decision = ProcessMemPidDecision::GetInstance();
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();

    ubse::task_executor::MockSetExecutorAsync(true);
    ubse::mem::controller::MockBlockNumaCreate();

    MockNodeFree(40);
    std::thread round1Thread([&decision]() { decision.OnDecisionTimer(); });
    round1Thread.join();
    ASSERT_TRUE(ubse::mem::controller::MockWaitNumaCreateEntered(5000));
    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pid).borrow.slots.size(), 1u);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].migratedBytes, 10 * GB);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].status, BorrowSlotStatus::BORROWING);
    std::string firstDebtId = snapshot.at(pid).borrow.slots[0].debtId;
    EXPECT_EQ(decision.GetPendingMigrateTotal(), 10 * GB);

    MockNodeFree(35);
    std::thread round2Thread([&decision]() { decision.OnDecisionTimer(); });
    round2Thread.join();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        auto snap = infoMgr.GetManagedPidCacheSnapshot();
        if (snap.at(pid).borrow.slots.size() >= 2 &&
            ubse::mem::controller::MockGetLastNumaCreateName() != firstDebtId) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    snapshot = infoMgr.GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pid).borrow.slots.size(), 2u);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].migratedBytes, 10 * GB);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].status, BorrowSlotStatus::BORROWING);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[1].migratedBytes, 5 * GB);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[1].status, BorrowSlotStatus::BORROWING);
    EXPECT_FALSE(snapshot.at(pid).borrow.slots[1].debtId.empty());
    EXPECT_EQ(decision.GetPendingMigrateTotal(), 15 * GB);

    ubse::mem::controller::MockReleaseNumaCreate();
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        auto snap = infoMgr.GetManagedPidCacheSnapshot();
        if (snap.at(pid).borrow.slots.size() >= 2 &&
            snap.at(pid).borrow.slots[0].status == BorrowSlotStatus::COMPLETED &&
            snap.at(pid).borrow.slots[1].status == BorrowSlotStatus::COMPLETED) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    snapshot = infoMgr.GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pid).borrow.slots.size(), 2u);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].status, BorrowSlotStatus::COMPLETED);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[1].status, BorrowSlotStatus::COMPLETED);
    EXPECT_EQ(snapshot.at(pid).borrow.currentRemote, 15 * GB);
    EXPECT_EQ(decision.GetPendingMigrateTotal(), 0u);

    MockNodeFree(60);
    std::thread round3Thread([&decision]() { decision.OnDecisionTimer(); });
    round3Thread.join();
    ubse::task_executor::MockWaitExecutorIdle();
    snapshot = infoMgr.GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pid).borrow.slots.size(), 2u);
    EXPECT_EQ(snapshot.at(pid).borrow.currentRemote, 15 * GB);
    EXPECT_EQ(decision.GetPendingMigrateTotal(), 0u);

    ubse::task_executor::MockSetExecutorAsync(false);
}

TEST_F(TestProcessMemPidDecision, BorrowAmountRoundedUpToBlockSize)
{
    const pid_t pid = 15970;
    constexpr uint64_t maxGb = 32;
    constexpr double ratio = 0.5;
    AddManagedPid(pid, maxGb, ratio, 40);

    // mock blockSize=256MB: 缺口 4.625GB 非对齐, 向上取整到 4.75GB, shortage 允许多减
    constexpr uint64_t shortageBytes = 4 * GB + GB / 2 + GB / 8;
    constexpr uint64_t expectedBytes = 4 * GB + GB / 2 + GB / 4;
    ubse::mem::controller::MockSetNumaCreateDesc(3, 7, expectedBytes);
    MockNodeFreeBytes(THRESHOLD_BYTES - shortageBytes);

    auto& decision = ProcessMemPidDecision::GetInstance();
    std::thread roundThread([&decision]() { decision.OnDecisionTimer(); });
    roundThread.join();
    ubse::task_executor::MockWaitExecutorIdle();

    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pid).borrow.slots.size(), 1u);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].migratedBytes, expectedBytes);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].capacity, expectedBytes);
    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaCreateSize(), expectedBytes);
}

TEST_F(TestProcessMemPidDecision, NextCycleBorrowsFullAmountNoReuse)
{
    const pid_t pid = 15970;
    constexpr uint64_t maxGb = 32;
    constexpr double ratio = 0.5;
    AddManagedPid(pid, maxGb, ratio, 20);

    auto& decision = ProcessMemPidDecision::GetInstance();
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    ubse::task_executor::MockSetExecutorAsync(true);

    constexpr uint64_t gb2_5 = 2 * GB + GB / 2;
    ubse::mem::controller::MockSetNumaCreateDesc(3, 7, 3 * GB);
    MockNodeFreeBytes(THRESHOLD_BYTES - gb2_5);
    std::thread round1Thread([&decision]() { decision.OnDecisionTimer(); });
    round1Thread.join();
    ubse::task_executor::MockWaitExecutorIdle();
    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pid).borrow.slots.size(), 1u);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].capacity, 3 * GB);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].migratedBytes, gb2_5);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].remoteNumaId, 3);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].status, BorrowSlotStatus::COMPLETED);
    EXPECT_EQ(snapshot.at(pid).borrow.currentRemote, gb2_5);
    ASSERT_EQ(ubse::smap::MockGetMigrateCallCount(), 1u);
    EXPECT_EQ(ubse::smap::MockGetMigrateTargetKb(pid, 3), gb2_5 / 1024);
    std::string firstDebtId = snapshot.at(pid).borrow.slots[0].debtId;

    ubse::mem::controller::MockSetNumaCreateDesc(9, 11, 0);
    ubse::mem::controller::MockBlockNumaCreate();
    MockNodeFreeBytes(THRESHOLD_BYTES - 4 * GB - GB / 2);
    std::thread round2Thread([&decision]() { decision.OnDecisionTimer(); });
    round2Thread.join();
    ASSERT_TRUE(ubse::mem::controller::MockWaitNumaCreateEntered(5000));

    // 空闲容量 0.5GB 不足以覆盖 4.5GB 借用需求, 不部分复用, 全额新建债务
    ASSERT_EQ(ubse::smap::MockGetMigrateCallCount(), 1u);
    EXPECT_EQ(ubse::smap::MockGetMigrateTargetKb(pid, 3), gb2_5 / 1024);
    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaCreateSize(), 4 * GB + GB / 2);
    snapshot = infoMgr.GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pid).borrow.slots.size(), 2u);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].status, BorrowSlotStatus::COMPLETED);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].capacity, 3 * GB);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].migratedBytes, gb2_5);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[1].status, BorrowSlotStatus::BORROWING);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[1].migratedBytes, 4 * GB + GB / 2);
    EXPECT_FALSE(snapshot.at(pid).borrow.slots[1].debtId.empty());
    EXPECT_NE(snapshot.at(pid).borrow.slots[1].debtId, firstDebtId);
    EXPECT_EQ(snapshot.at(pid).processStatus, ProcessStatus::BORROWING);
    EXPECT_EQ(decision.GetPendingMigrateTotal(), 4 * GB + GB / 2);

    ubse::mem::controller::MockReleaseNumaCreate();
    ubse::task_executor::MockWaitExecutorIdle();
    ASSERT_EQ(ubse::smap::MockGetMigrateCallCount(), 2u);
    EXPECT_EQ(ubse::smap::MockGetMigrateTargetKb(pid, 3), gb2_5 / 1024);
    EXPECT_EQ(ubse::smap::MockGetMigrateTargetKb(pid, 9), (4 * GB + GB / 2) / 1024);
    snapshot = infoMgr.GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pid).borrow.slots.size(), 2u);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].migratedBytes, gb2_5);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].status, BorrowSlotStatus::COMPLETED);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[1].capacity, 4 * GB + GB / 2);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[1].migratedBytes, 4 * GB + GB / 2);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[1].remoteNumaId, 9);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[1].status, BorrowSlotStatus::COMPLETED);
    EXPECT_EQ(snapshot.at(pid).borrow.currentRemote, gb2_5 + 4 * GB + GB / 2);
    EXPECT_EQ(decision.GetPendingMigrateTotal(), 0u);
    EXPECT_EQ(snapshot.at(pid).processStatus, ProcessStatus::BORROWED);

    ubse::task_executor::MockSetExecutorAsync(false);
}

TEST_F(TestProcessMemPidDecision, ReturningSlotExcludedFromMigrateTargets)
{
    const pid_t pid = 15970;
    constexpr uint64_t maxGb = 32;
    constexpr double ratio = 0.5;
    constexpr uint64_t gb2_5 = 2 * GB + GB / 2;
    AddManagedPid(pid, maxGb, ratio, 20);

    BorrowState borrow;
    BorrowSlot slot;
    slot.debtId = "debt-returning";
    slot.capacity = 3 * GB;
    slot.migratedBytes = gb2_5;
    slot.remoteNumaId = 3;
    slot.status = BorrowSlotStatus::RETURNING;
    borrow.currentRemote = gb2_5;
    borrow.slots.push_back(slot);
    AddManagedPid(pid, maxGb, ratio, 20, borrow);

    ubse::mem::controller::MockSetNumaCreateDesc(3, 7, 0);
    MockNodeFreeBytes(THRESHOLD_BYTES - gb2_5);

    ProcessMemPidDecision::GetInstance().OnDecisionTimer();

    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaCreateSize(), gb2_5);
    EXPECT_EQ(ubse::smap::MockGetMigrateTargetKb(pid, 3), gb2_5 / 1024);

    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pid).borrow.slots.size(), 2u);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].migratedBytes, gb2_5);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].status, BorrowSlotStatus::RETURNING);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[1].migratedBytes, gb2_5);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[1].status, BorrowSlotStatus::COMPLETED);
}

TEST_F(TestProcessMemPidDecision, BorrowingSlotExcludedFromMigrateTargets)
{
    const pid_t pid = 15970;
    constexpr uint64_t maxGb = 32;
    constexpr double ratio = 0.5;
    constexpr uint64_t gb2_5 = 2 * GB + GB / 2;
    BorrowState inFlight;
    BorrowSlot slot;
    slot.debtId = "debt-inflight";
    slot.migratedBytes = gb2_5;
    slot.status = BorrowSlotStatus::BORROWING;
    slot.borrowTime = std::chrono::steady_clock::now();
    inFlight.slots.push_back(slot);
    AddManagedPid(pid, maxGb, ratio, 20, inFlight);

    auto& decision = ProcessMemPidDecision::GetInstance();
    auto& infoMgr = ProcessMemPidInfoManager::GetInstance();
    ubse::task_executor::MockSetExecutorAsync(true);

    constexpr uint64_t gb2 = 2 * GB;
    ubse::mem::controller::MockSetNumaCreateDesc(9, 11, 0);
    MockNodeFreeBytes(THRESHOLD_BYTES - 4 * GB - GB / 2);
    std::thread roundThread([&decision]() { decision.OnDecisionTimer(); });
    roundThread.join();
    ubse::task_executor::MockWaitExecutorIdle();
    EXPECT_EQ(ubse::mem::controller::MockGetLastNumaCreateSize(), gb2);
    auto snapshot = infoMgr.GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pid).borrow.slots.size(), 2u);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].capacity, 0u);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].migratedBytes, gb2_5);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].remoteNumaId, -1);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].status, BorrowSlotStatus::BORROWING);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[1].capacity, gb2);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[1].migratedBytes, gb2);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[1].remoteNumaId, 9);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[1].status, BorrowSlotStatus::COMPLETED);
    EXPECT_EQ(snapshot.at(pid).borrow.currentRemote, gb2);
    EXPECT_EQ(ubse::smap::MockGetMigrateTargetKb(pid, 3), 0u);
    EXPECT_EQ(ubse::smap::MockGetMigrateTargetKb(pid, 9), gb2 / 1024);
    auto calls = ubse::smap::MockGetMigrateCalls();
    ASSERT_EQ(calls.size(), 1u);
    EXPECT_EQ(calls[0].first, 9);
    EXPECT_EQ(calls[0].second, gb2 / 1024);
    EXPECT_EQ(decision.GetPendingMigrateTotal(), gb2_5);
    EXPECT_EQ(snapshot.at(pid).processStatus, ProcessStatus::BORROWING);

    ubse::task_executor::MockSetExecutorAsync(false);
}

TEST_F(TestProcessMemPidDecision, BorrowingPidEligibleWithChangingNodeFree)
{
    const pid_t pidA = 15970;
    constexpr uint64_t maxGb = 32;
    constexpr double ratio = 0.5;
    AddManagedPid(pidA, maxGb, ratio, 40);

    ProcessMemPidBridge::rmrsMigrateOut = [](const std::vector<mempooling::smap::MigrateOutPayload>&, int) {
        return 0;
    };

    auto& decision = ProcessMemPidDecision::GetInstance();

    ubse::mem::controller::MockBlockNumaCreate();
    MockNodeFree(40);
    std::thread decisionThread([&decision]() { decision.OnDecisionTimer(); });
    ASSERT_TRUE(ubse::mem::controller::MockWaitNumaCreateEntered(5000));

    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pidA).borrow.slots.size(), 1u);
    EXPECT_EQ(snapshot.at(pidA).borrow.slots[0].migratedBytes, 10 * GB);
    EXPECT_EQ(snapshot.at(pidA).borrow.slots[0].status, BorrowSlotStatus::BORROWING);
    EXPECT_EQ(snapshot.at(pidA).processStatus, ProcessStatus::BORROWING);

    std::thread round2Thread([&decision]() { decision.OnDecisionTimer(); });
    round2Thread.join();
    snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pidA).borrow.slots.size(), 1u);
    EXPECT_EQ(snapshot.at(pidA).borrow.slots[0].status, BorrowSlotStatus::BORROWING);
    EXPECT_EQ(decision.GetPendingMigrateTotal(), 10 * GB);

    auto candidates = decision.BuildCandidates(2);
    ASSERT_EQ(candidates.size(), 1u);
    EXPECT_EQ(candidates[0].pid, pidA);
    EXPECT_TRUE(candidates[0].hasActiveBorrow);
    EXPECT_EQ(candidates[0].canMigrate, 10 * GB);

    ubse::mem::controller::MockReleaseNumaCreate();
    decisionThread.join();
    snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pidA).borrow.slots.size(), 1u);
    EXPECT_EQ(snapshot.at(pidA).borrow.slots[0].status, BorrowSlotStatus::COMPLETED);
    EXPECT_EQ(snapshot.at(pidA).borrow.currentRemote, 10 * GB);

    MockNodeFree(45);
    std::thread round3Thread([&decision]() { decision.OnDecisionTimer(); });
    round3Thread.join();
    snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pidA).borrow.slots.size(), 2u);
    EXPECT_EQ(snapshot.at(pidA).borrow.slots[1].migratedBytes, 5 * GB);
    EXPECT_EQ(snapshot.at(pidA).borrow.slots[1].status, BorrowSlotStatus::COMPLETED);
    EXPECT_EQ(snapshot.at(pidA).borrow.currentRemote, 15 * GB);

    MockNodeFree(40);
    std::thread round4Thread([&decision]() { decision.OnDecisionTimer(); });
    round4Thread.join();
    snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pidA).borrow.slots.size(), 3u);
    EXPECT_EQ(snapshot.at(pidA).borrow.slots[2].migratedBytes, 5 * GB);
    EXPECT_EQ(snapshot.at(pidA).borrow.slots[2].status, BorrowSlotStatus::COMPLETED);
    EXPECT_EQ(snapshot.at(pidA).borrow.currentRemote, 20 * GB);

    std::thread round5Thread([&decision]() { decision.OnDecisionTimer(); });
    round5Thread.join();
    snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pidA).borrow.slots.size(), 3u);
    EXPECT_EQ(snapshot.at(pidA).borrow.currentRemote, 20 * GB);
}

TEST_F(TestProcessMemPidDecision, ShortageDeductsPendingBorrow)
{
    const pid_t pidA = 15970;
    const pid_t pidB = 15971;
    const pid_t pidC = 15972;
    AddManagedPid(pidA, 32, 0.5, 40);
    BorrowState borrowA;
    BorrowSlot inFlight;
    inFlight.debtId = "debt-a";
    inFlight.migratedBytes = 15 * GB;
    inFlight.capacity = 15 * GB;
    inFlight.status = BorrowSlotStatus::BORROWING;
    inFlight.borrowTime = std::chrono::steady_clock::now() + std::chrono::hours(1);
    borrowA.slots = {inFlight};
    ProcessMemPidInfoManager::GetInstance().UpdateManagedPidBorrowState(pidA, borrowA, ProcessStatus::BORROWING);
    AddManagedPid(pidB, 32, 0.5, 30);
    AddManagedPid(pidC, 32, 0.5, 30);

    ProcessMemPidBridge::rmrsMigrateOut = [](const std::vector<mempooling::smap::MigrateOutPayload>&, int) {
        return 0;
    };
    MockNodeFree(10);

    ProcessMemPidDecision::GetInstance().OnDecisionTimer();

    auto snapshot = ProcessMemPidInfoManager::GetInstance().GetManagedPidCacheSnapshot();
    ASSERT_EQ(snapshot.at(pidB).borrow.slots.size(), 1u);
    EXPECT_EQ(snapshot.at(pidB).borrow.slots[0].status, BorrowSlotStatus::COMPLETED);
    ASSERT_EQ(snapshot.at(pidC).borrow.slots.size(), 1u);
    EXPECT_EQ(snapshot.at(pidC).borrow.slots[0].status, BorrowSlotStatus::COMPLETED);
    std::multiset<uint64_t> migrated = {snapshot.at(pidB).borrow.slots[0].migratedBytes,
                                        snapshot.at(pidC).borrow.slots[0].migratedBytes};
    EXPECT_EQ(migrated, (std::multiset<uint64_t>{15 * GB, 10 * GB}));
    ASSERT_EQ(snapshot.at(pidA).borrow.slots.size(), 1u);
    EXPECT_EQ(snapshot.at(pidA).borrow.slots[0].debtId, "debt-a");
    EXPECT_EQ(snapshot.at(pidA).borrow.slots[0].status, BorrowSlotStatus::BORROWING);
    EXPECT_EQ(ProcessMemPidDecision::GetInstance().GetPendingMigrateTotal(), 15 * GB);
}

TEST_F(TestProcessMemPidDecision, IsReturnDoneTreatsUnimportSuccessAsDone)
{
    // unimport 成功仅 unexport 失败: 内存已实际归还借出方, 债务已解除, 均视为归还完成
    EXPECT_TRUE(IsReturnDone(UBSE_OK));
    EXPECT_TRUE(IsReturnDone(UBSE_ERR_NOT_EXIST));
    EXPECT_TRUE(IsReturnDone(UBSE_ERR_UNIMPORT_SUCCESS));
    EXPECT_FALSE(IsReturnDone(UBSE_ERROR));
}
} // namespace ubse::ut::process_mem
