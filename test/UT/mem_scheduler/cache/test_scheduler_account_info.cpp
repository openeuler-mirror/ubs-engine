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

#include "test_scheduler_account_info.h"

#include "scheduler_cache/ubse_mem_scheduler_account_info.h"

namespace ubse::mem::scheduler::ut {

using namespace ubse::mem::scheduler;
using namespace ubse::adapter_plugins::mmi;

namespace {

UbseMemDebtNumaInfo MakeLoc(const std::string& nodeId, uint32_t socketId, uint32_t numaId, uint64_t size = 128)
{
    UbseMemDebtNumaInfo loc{};
    loc.nodeId = nodeId;
    loc.socketId = static_cast<int32_t>(socketId);
    loc.numaId = static_cast<int32_t>(numaId);
    loc.size = size;
    return loc;
}

} // namespace

// ==================== Construction ====================

TEST_F(TestSchedulerAccountInfo, ConstructSuccess)
{
    auto importLocs = {MakeLoc("1", 36, 0)};
    auto exportLocs = {MakeLoc("2", 36, 0)};
    SchedulerAccountInfo account(importLocs, exportLocs, BorrowedType::FD, 128);
    EXPECT_EQ(account.GetImportNodeId(), "1");
    EXPECT_EQ(account.GetExportNodeId(), "2");
    EXPECT_EQ(account.GetAccountState(), AccountState::BOTH_NOT_EXIST);
}

TEST_F(TestSchedulerAccountInfo, ConstructBlockSizeZeroThrows)
{
    auto locs = {MakeLoc("2", 36, 0)};
    EXPECT_THROW(SchedulerAccountInfo({locs}, locs, BorrowedType::FD, 0), std::invalid_argument);
}

TEST_F(TestSchedulerAccountInfo, ConstructExportEmptyThrows)
{
    auto importLocs = {MakeLoc("1", 36, 0)};
    EXPECT_THROW(SchedulerAccountInfo(importLocs, {}, BorrowedType::FD, 128), std::invalid_argument);
}

TEST_F(TestSchedulerAccountInfo, ConstructImportEmptyNonShmThrows)
{
    auto exportLocs = {MakeLoc("2", 36, 0)};
    EXPECT_THROW(SchedulerAccountInfo({}, exportLocs, BorrowedType::FD, 128), std::invalid_argument);
}

TEST_F(TestSchedulerAccountInfo, ConstructImportEmptyShmOk)
{
    auto exportLocs = {MakeLoc("2", 36, 0)};
    SchedulerAccountInfo account({}, exportLocs, BorrowedType::SHM, 128);
    EXPECT_TRUE(account.GetImportNodeId().empty());
    EXPECT_EQ(account.GetExportNodeId(), "2");
}

// ==================== BOTH_NOT_EXIST → * ====================

TEST_F(TestSchedulerAccountInfo, BothNotExist_Scheduling)
{
    auto importLocs = {MakeLoc("1", 36, 0)};
    auto exportLocs = {MakeLoc("2", 36, 0)};
    SchedulerAccountInfo account(importLocs, exportLocs, BorrowedType::FD, 128);
    account.UpdateAccountState(UBSE_MEM_SCHEDULING, nullptr);
    EXPECT_EQ(account.GetAccountState(), AccountState::IMPORT_EXPORT_EXIST);
}

TEST_F(TestSchedulerAccountInfo, BothNotExist_ImportSuccess)
{
    auto importLocs = {MakeLoc("1", 36, 0)};
    auto exportLocs = {MakeLoc("2", 36, 0)};
    SchedulerAccountInfo account(importLocs, exportLocs, BorrowedType::FD, 128);
    account.UpdateAccountState(UBSE_MEM_IMPORT_SUCCESS, nullptr);
    EXPECT_EQ(account.GetAccountState(), AccountState::ONLY_IMPORT_EXIST);
}

TEST_F(TestSchedulerAccountInfo, BothNotExist_ExportSuccess)
{
    auto importLocs = {MakeLoc("1", 36, 0)};
    auto exportLocs = {MakeLoc("2", 36, 0)};
    SchedulerAccountInfo account(importLocs, exportLocs, BorrowedType::FD, 128);
    account.UpdateAccountState(UBSE_MEM_EXPORT_SUCCESS, nullptr);
    EXPECT_EQ(account.GetAccountState(), AccountState::ONLY_EXPORT_EXIST);
}

// ==================== ONLY_EXPORT_EXIST → * ====================

TEST_F(TestSchedulerAccountInfo, OnlyExportExist_ImportSuccess)
{
    auto importLocs = {MakeLoc("1", 36, 0)};
    auto exportLocs = {MakeLoc("2", 36, 0)};
    SchedulerAccountInfo account(importLocs, exportLocs, BorrowedType::FD, 128);
    account.UpdateAccountState(UBSE_MEM_EXPORT_SUCCESS, nullptr);
    ASSERT_EQ(account.GetAccountState(), AccountState::ONLY_EXPORT_EXIST);

    account.UpdateAccountState(UBSE_MEM_IMPORT_SUCCESS, nullptr);
    EXPECT_EQ(account.GetAccountState(), AccountState::IMPORT_EXPORT_EXIST);
}

TEST_F(TestSchedulerAccountInfo, OnlyExportExist_ExportDestroyed)
{
    auto importLocs = {MakeLoc("1", 36, 0)};
    auto exportLocs = {MakeLoc("2", 36, 0)};
    SchedulerAccountInfo account(importLocs, exportLocs, BorrowedType::FD, 128);
    account.UpdateAccountState(UBSE_MEM_EXPORT_SUCCESS, nullptr);
    ASSERT_EQ(account.GetAccountState(), AccountState::ONLY_EXPORT_EXIST);

    account.UpdateAccountState(UBSE_MEM_EXPORT_DESTROYED, nullptr);
    EXPECT_EQ(account.GetAccountState(), AccountState::BOTH_NOT_EXIST);
}

// ==================== ONLY_IMPORT_EXIST → * ====================

TEST_F(TestSchedulerAccountInfo, OnlyImportExist_ExportSuccess)
{
    auto importLocs = {MakeLoc("1", 36, 0)};
    auto exportLocs = {MakeLoc("2", 36, 0)};
    SchedulerAccountInfo account(importLocs, exportLocs, BorrowedType::FD, 128);
    account.UpdateAccountState(UBSE_MEM_IMPORT_SUCCESS, nullptr);
    ASSERT_EQ(account.GetAccountState(), AccountState::ONLY_IMPORT_EXIST);

    account.UpdateAccountState(UBSE_MEM_EXPORT_SUCCESS, nullptr);
    EXPECT_EQ(account.GetAccountState(), AccountState::IMPORT_EXPORT_EXIST);
}

TEST_F(TestSchedulerAccountInfo, OnlyImportExist_ImportDestroyed)
{
    auto importLocs = {MakeLoc("1", 36, 0)};
    auto exportLocs = {MakeLoc("2", 36, 0)};
    SchedulerAccountInfo account(importLocs, exportLocs, BorrowedType::FD, 128);
    account.UpdateAccountState(UBSE_MEM_IMPORT_SUCCESS, nullptr);
    ASSERT_EQ(account.GetAccountState(), AccountState::ONLY_IMPORT_EXIST);

    account.UpdateAccountState(UBSE_MEM_IMPORT_DESTROYED, nullptr);
    EXPECT_EQ(account.GetAccountState(), AccountState::BOTH_NOT_EXIST);
}

// ==================== SHM paths ====================

TEST_F(TestSchedulerAccountInfo, ShmBothNotExist_Scheduling)
{
    auto exportLocs = {MakeLoc("2", 36, 0)};
    SchedulerAccountInfo account({}, exportLocs, BorrowedType::SHM, 128);
    account.UpdateAccountState(UBSE_MEM_SCHEDULING, nullptr);
    EXPECT_EQ(account.GetAccountState(), AccountState::ONLY_EXPORT_EXIST);
}

TEST_F(TestSchedulerAccountInfo, BothExist_StateFailed)
{
    auto importLocs = {MakeLoc("1", 36, 0)};
    auto exportLocs = {MakeLoc("2", 36, 0)};
    SchedulerAccountInfo account(importLocs, exportLocs, BorrowedType::FD, 128);
    account.UpdateAccountState(UBSE_MEM_SCHEDULING, nullptr);
    ASSERT_EQ(account.GetAccountState(), AccountState::IMPORT_EXPORT_EXIST);

    account.UpdateAccountState(UBSE_MEM_STATE_FAILED, nullptr);
    EXPECT_EQ(account.GetAccountState(), AccountState::BOTH_NOT_EXIST);
}

} // namespace ubse::mem::scheduler::ut
