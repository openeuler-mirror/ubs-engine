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

#include "test_scheduler_account_manager.h"

#include <mockcpp/mockcpp.hpp>

#include "ubse_mem_scheduler_account_manager.h"
#include "ubse_mem_scheduler_node_manager.h"

namespace ubse::mem::scheduler::ut {

using namespace ubse::common::def;
using namespace ubse::adapter_plugins::mmi;

void TestSchedulerAccountManager::SetUp()
{
    Test::SetUp();
}

void TestSchedulerAccountManager::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

namespace {

UbseMemDebtNumaInfo MakeLoc(const std::string& nodeId, int socketId, int64_t numaId, uint64_t size = 128)
{
    UbseMemDebtNumaInfo loc{};
    loc.nodeId = nodeId;
    loc.socketId = socketId;
    loc.numaId = numaId;
    loc.size = size;
    return loc;
}

} // namespace

TEST_F(TestSchedulerAccountManager, UpdateSchedulerAccountNew)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    accMgr.SetNodeManager(&nodeMgr);

    UbseMemAlgoResult result;
    result.importNumaInfos = {MakeLoc("1", 36, 0)};
    result.exportNumaInfos = {MakeLoc("2", 36, 0)};
    result.blockSize = 128;

    auto ret = accMgr.UpdateSchedulerAccount("test1", result, UBSE_MEM_SCHEDULING, BorrowedType::FD);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestSchedulerAccountManager, UpdateSchedulerAccountExisting)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    accMgr.SetNodeManager(&nodeMgr);

    UbseMemAlgoResult result;
    result.importNumaInfos = {MakeLoc("1", 36, 0)};
    result.exportNumaInfos = {MakeLoc("2", 36, 0)};
    result.blockSize = 128;

    auto ret1 = accMgr.UpdateSchedulerAccount("test1", result, UBSE_MEM_SCHEDULING, BorrowedType::FD);
    EXPECT_EQ(ret1, UBSE_OK);

    auto ret2 = accMgr.UpdateSchedulerAccount("test1", result, UBSE_MEM_IMPORT_SUCCESS, BorrowedType::FD);
    EXPECT_EQ(ret2, UBSE_OK);
}

TEST_F(TestSchedulerAccountManager, UpdateSchedulerAccountEmptyImportReturnsError)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    accMgr.SetNodeManager(&nodeMgr);

    UbseMemAlgoResult result;
    result.exportNumaInfos = {MakeLoc("2", 36, 0)};
    result.blockSize = 128;

    auto ret = accMgr.UpdateSchedulerAccount("test1", result, UBSE_MEM_SCHEDULING, BorrowedType::FD);
    EXPECT_EQ(ret, UBSE_ERROR_INVAL);
}

TEST_F(TestSchedulerAccountManager, HasBorrowedFromAfterUpdate)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    accMgr.SetNodeManager(&nodeMgr);

    UbseMemAlgoResult result;
    result.importNumaInfos = {MakeLoc("1", 36, 0)};
    result.exportNumaInfos = {MakeLoc("2", 36, 0)};
    result.blockSize = 128;

    accMgr.UpdateSchedulerAccount("test1", result, UBSE_MEM_SCHEDULING, BorrowedType::FD);

    EXPECT_TRUE(accMgr.HasBorrowedFrom("1", "2"));
    EXPECT_FALSE(accMgr.HasBorrowedFrom("1", "3"));
    EXPECT_FALSE(accMgr.HasBorrowedFrom("3", "2"));
}

TEST_F(TestSchedulerAccountManager, ClearRemovesAllAccounts)
{
    SchedulerNodeManager nodeMgr;
    SchedulerAccountManager accMgr;
    accMgr.SetNodeManager(&nodeMgr);

    UbseMemAlgoResult result;
    result.importNumaInfos = {MakeLoc("1", 36, 0)};
    result.exportNumaInfos = {MakeLoc("2", 36, 0)};
    result.blockSize = 128;

    accMgr.UpdateSchedulerAccount("test1", result, UBSE_MEM_SCHEDULING, BorrowedType::FD);
    EXPECT_TRUE(accMgr.HasBorrowedFrom("1", "2"));

    accMgr.Clear();
    EXPECT_FALSE(accMgr.HasBorrowedFrom("1", "2"));
}

} // namespace ubse::mem::scheduler::ut
