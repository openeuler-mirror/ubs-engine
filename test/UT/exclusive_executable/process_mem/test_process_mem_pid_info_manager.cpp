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

#include "test_process_mem_pid_info_manager.h"

namespace ubse::ut::process_mem {
using namespace ::process_mem::manager;
using namespace ::process_mem::def;

void TestProcessMemPidInfoManager::SetUp() {}

void TestProcessMemPidInfoManager::TearDown() {}

TEST_F(TestProcessMemPidInfoManager, SetPidInfoMapSuccess)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 1234;
    pidInfo.startTime = 1000;
    pidInfo.configInfo.evictThreshold = 80;

    auto ret = mgr.SetPidInfoMap(pidInfo);
    EXPECT_EQ(ret, UBSE_OK);

    auto retrieved = mgr.GetPidInfoMap(1234);
    EXPECT_EQ(retrieved.configInfo.pid, 1234);
    EXPECT_EQ(retrieved.startTime, 1000);

    mgr.UnsetPidInfo(1234);
}

TEST_F(TestProcessMemPidInfoManager, SetPidInfoMapZeroStartTime)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 2345;
    pidInfo.startTime = 0;

    auto ret = mgr.SetPidInfoMap(pidInfo);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);
}

TEST_F(TestProcessMemPidInfoManager, SetPidInfoMapStartTimeMismatch)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo1{};
    pidInfo1.configInfo.pid = 3456;
    pidInfo1.startTime = 1000;
    pidInfo1.configInfo.evictThreshold = 80;

    auto ret = mgr.SetPidInfoMap(pidInfo1);
    EXPECT_EQ(ret, UBSE_OK);

    ProcessMemPidInfo pidInfo2{};
    pidInfo2.configInfo.pid = 3456;
    pidInfo2.startTime = 2000;
    pidInfo2.configInfo.evictThreshold = 90;

    ret = mgr.SetPidInfoMap(pidInfo2);
    EXPECT_EQ(ret, UBSE_ERR_INVALID_ARG);

    mgr.UnsetPidInfo(3456);
}

TEST_F(TestProcessMemPidInfoManager, SetPidInfoMapModifySrcNumaWithDebt)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 4567;
    pidInfo.startTime = 1000;
    pidInfo.configInfo.srcNumaId = 0;

    auto ret = mgr.SetPidInfoMap(pidInfo);
    EXPECT_EQ(ret, UBSE_OK);

    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "test_debt";
    debtInfo.numaDesc.size = 1024;
    debtInfo.status = BorrowStatus::COMPLETED;
    mgr.UpdatePidMemBorrowInfo(4567, debtInfo);

    ProcessMemPidInfo pidInfoUpdated{};
    pidInfoUpdated.configInfo.pid = 4567;
    pidInfoUpdated.startTime = 1000;
    pidInfoUpdated.configInfo.srcNumaId = 1;

    ret = mgr.SetPidInfoMap(pidInfoUpdated);
    EXPECT_EQ(ret, UBSE_ERR_RESOURCE_BUSY);

    mgr.DeletePidMemBorrowInfo(4567, "test_debt");
    mgr.UnsetPidInfo(4567);
}

TEST_F(TestProcessMemPidInfoManager, SetPidInfoMapUpdateConfigSameStartTime)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo1{};
    pidInfo1.configInfo.pid = 5555;
    pidInfo1.startTime = 1000;
    pidInfo1.configInfo.evictThreshold = 80;

    auto ret = mgr.SetPidInfoMap(pidInfo1);
    EXPECT_EQ(ret, UBSE_OK);

    ProcessMemPidInfo pidInfo2{};
    pidInfo2.configInfo.pid = 5555;
    pidInfo2.startTime = 1000;
    pidInfo2.configInfo.evictThreshold = 90;

    ret = mgr.SetPidInfoMap(pidInfo2);
    EXPECT_EQ(ret, UBSE_OK);

    auto retrieved = mgr.GetPidInfoMap(5555);
    EXPECT_EQ(retrieved.configInfo.evictThreshold, 90);

    mgr.UnsetPidInfo(5555);
}

TEST_F(TestProcessMemPidInfoManager, GetPidInfoMapNotExist)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    auto result = mgr.GetPidInfoMap(99999999);
    EXPECT_EQ(result.configInfo.pid, -1);
}

TEST_F(TestProcessMemPidInfoManager, GetAllPidInfoEmpty)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    std::vector<ProcessMemPidInfo> pidInfos;
    mgr.GetAllPidInfo(pidInfos);
}

TEST_F(TestProcessMemPidInfoManager, GetAllPidInfoWithEntries)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo1{};
    pidInfo1.configInfo.pid = 6001;
    pidInfo1.startTime = 1000;
    pidInfo1.configInfo.evictThreshold = 80;

    ProcessMemPidInfo pidInfo2{};
    pidInfo2.configInfo.pid = 6002;
    pidInfo2.startTime = 2000;
    pidInfo2.configInfo.evictThreshold = 90;

    mgr.SetPidInfoMap(pidInfo1);
    mgr.SetPidInfoMap(pidInfo2);

    std::vector<ProcessMemPidInfo> pidInfos;
    mgr.GetAllPidInfo(pidInfos);
    EXPECT_GE(pidInfos.size(), 2u);

    mgr.UnsetPidInfo(6001);
    mgr.UnsetPidInfo(6002);
}

TEST_F(TestProcessMemPidInfoManager, UnsetPidInfoSuccess)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 7777;
    pidInfo.startTime = 1000;

    mgr.SetPidInfoMap(pidInfo);
    auto ret = mgr.UnsetPidInfo(7777);
    EXPECT_EQ(ret, UBSE_OK);

    auto result = mgr.GetPidInfoMap(7777);
    EXPECT_EQ(result.configInfo.pid, -1);
}

TEST_F(TestProcessMemPidInfoManager, UnsetPidInfoNotExist)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    auto ret = mgr.UnsetPidInfo(99999999);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);
}

TEST_F(TestProcessMemPidInfoManager, UpdatePidMemBorrowInfoSuccess)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 8888;
    pidInfo.startTime = 1000;

    mgr.SetPidInfoMap(pidInfo);

    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "test_borrow";
    debtInfo.numaDesc.size = 2048;
    debtInfo.status = BorrowStatus::COMPLETED;

    auto ret = mgr.UpdatePidMemBorrowInfo(8888, debtInfo);
    EXPECT_EQ(ret, UBSE_OK);

    auto retrieved = mgr.GetPidInfoMap(8888);
    EXPECT_TRUE(retrieved.memBorrowInfo.debtInfos.count("test_borrow") > 0);
    EXPECT_EQ(retrieved.memBorrowInfo.debtInfos["test_borrow"].numaDesc.size, 2048u);

    mgr.UnsetPidInfo(8888);
}

TEST_F(TestProcessMemPidInfoManager, UpdatePidMemBorrowInfoNotExist)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "test_borrow";

    auto ret = mgr.UpdatePidMemBorrowInfo(99999999, debtInfo);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestProcessMemPidInfoManager, UpdatePidMemBorrowInfoWithBorrower)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 8889;
    pidInfo.startTime = 1000;

    mgr.SetPidInfoMap(pidInfo);

    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.affinitySocketId = 1;

    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "test_borrow2";
    debtInfo.numaDesc.size = 4096;
    debtInfo.numaDesc.numaId = 2;
    debtInfo.numaDesc.exportNode.slotId = 3;
    debtInfo.status = BorrowStatus::COMPLETED;

    auto ret = mgr.UpdatePidMemBorrowInfo(8889, borrower, debtInfo);
    EXPECT_EQ(ret, UBSE_OK);

    auto retrieved = mgr.GetPidInfoMap(8889);
    EXPECT_EQ(retrieved.memBorrowInfo.importSocketId, 1);
    EXPECT_EQ(retrieved.memBorrowInfo.remoteNumaId, 2);
    EXPECT_EQ(retrieved.memBorrowInfo.exportSlotId, 3);
    EXPECT_TRUE(retrieved.memBorrowInfo.debtInfos.count("test_borrow2") > 0);

    mgr.UnsetPidInfo(8889);
}

TEST_F(TestProcessMemPidInfoManager, DeletePidMemBorrowInfoSuccess)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 9990;
    pidInfo.startTime = 1000;

    mgr.SetPidInfoMap(pidInfo);

    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "debt_to_delete";
    debtInfo.numaDesc.size = 1024;
    debtInfo.status = BorrowStatus::COMPLETED;

    mgr.UpdatePidMemBorrowInfo(9990, debtInfo);

    auto retrieved = mgr.GetPidInfoMap(9990);
    EXPECT_TRUE(retrieved.memBorrowInfo.debtInfos.count("debt_to_delete") > 0);

    mgr.DeletePidMemBorrowInfo(9990, "debt_to_delete");

    retrieved = mgr.GetPidInfoMap(9990);
    EXPECT_TRUE(retrieved.memBorrowInfo.debtInfos.count("debt_to_delete") == 0);

    mgr.UnsetPidInfo(9990);
}

TEST_F(TestProcessMemPidInfoManager, DeletePidMemBorrowInfoPidNotExist)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    EXPECT_NO_THROW(mgr.DeletePidMemBorrowInfo(99999999, "nonexistent_debt"));
}

TEST_F(TestProcessMemPidInfoManager, ResetPidMemBorrowInfoSuccess)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 9991;
    pidInfo.startTime = 1000;

    mgr.SetPidInfoMap(pidInfo);

    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "debt_to_reset";
    debtInfo.numaDesc.size = 1024;
    debtInfo.status = BorrowStatus::COMPLETED;

    mgr.UpdatePidMemBorrowInfo(9991, debtInfo);

    mgr.ResetPidMemBorrowInfo(9991);

    auto retrieved = mgr.GetPidInfoMap(9991);
    EXPECT_TRUE(retrieved.memBorrowInfo.debtInfos.empty());
    EXPECT_EQ(retrieved.memBorrowInfo.remoteNumaId, -1);

    mgr.UnsetPidInfo(9991);
}

TEST_F(TestProcessMemPidInfoManager, ResetPidMemBorrowInfoNotExist)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    EXPECT_NO_THROW(mgr.ResetPidMemBorrowInfo(99999999));
}

TEST_F(TestProcessMemPidInfoManager, IsRecoverCompletedDefault)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    EXPECT_FALSE(mgr.IsRecoverCompleted());
}

TEST_F(TestProcessMemPidInfoManager, HandleNodeFaultEvent)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    EXPECT_NO_THROW(mgr.HandleNodeFaultEvent("NODE_FAULT_1"));
}
} // namespace ubse::ut::process_mem
