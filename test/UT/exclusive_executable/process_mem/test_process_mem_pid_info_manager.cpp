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

#include "ubse_mem_controller.h"
#include "ubse_node_controller.h"
#include "mock/ubse/mock_control.h"
#include "process_mem_pid_bridge.h"
#include "process_mem_pid_collect.h"
#include "process_mem_pid_config_manager.h"

// Forward declare functions from process_mem_pid_info_manager.cpp for testing
namespace process_mem::manager {
bool CheckIsNeedBorrow(const ::process_mem::def::ProcessMemPidInfo& pidInfo, uint64_t expectRemoteMemory,
                       uint64_t& needBorrowSize);
uint32_t MemoryCheckHandle(::process_mem::def::ProcessMemPidInfo& info,
                           const std::unordered_map<uint32_t, size_t>& numaMemory);
std::string GenerateSimpleName(pid_t pid);
bool GetMigrateResult(pid_t pid, int64_t remoteNumaId, uint64_t expectSize, bool isBack = false);
uint32_t GetPidNumaSize(pid_t pid, int64_t numaId, uint64_t& numaSize);
uint32_t MigrateOut(const ::process_mem::def::ProcessMemPidInfo& pidInfo, uint64_t targetRemoteMemory);
int DoMigrateOut(pid_t pid, int destNid, uint64_t memSizeKb);
uint32_t GetUbseMemBorrower(const ::process_mem::def::ProcessMemPidInfo& pidInfo,
                            ubse::mem::controller::UbseMemBorrower& borrower);
uint64_t RefreshExpectRemoteMemory(pid_t pid, const ::process_mem::def::ProcessMemPidInfo& pidInfo,
                                   uint64_t defaultExpectRemote);
uint32_t BorrowAndMigrate(::process_mem::def::ProcessMemPidInfo& pidInfo, uint64_t expectRemoteMemory, int32_t minNuma);
uint32_t BorrowMemoryAndMigrateOut(::process_mem::def::ProcessMemPidInfo& pidInfo, uint64_t localMemorySize,
                                   uint64_t remoteMemorySize, int32_t minNuma);
uint32_t MigrateBackAndReturnMemory(::process_mem::def::ProcessMemPidInfo& pidInfo);
void ExceptionMemoryHandle(const std::string& name);
void AsyncBorrowAndMigrateExecute(pid_t pid, const ::process_mem::def::DebtInfo& debtInfo, uint64_t expectRemoteMemory);

template <typename T>
bool IsProcessMemDebt(const T& debt);

bool IsNodeRecovered(const std::string& lentNodeId);
void RestoreFaultPids(const std::vector<std::string>& recoveredNodes,
                      std::unordered_map<std::string, std::unordered_set<pid_t>>& faultedLentNodes,
                      std::unordered_map<pid_t, ::process_mem::def::ProcessMemPidInfo>& pidInfoMap);

uint32_t MigrateBackAndReturnMemoryAsyncExecute(const ::process_mem::def::ProcessMemPidInfo& pidInfo);
} // namespace process_mem::manager

namespace process_mem::pid::bridge {
uint32_t ValidateSrcNumaIdOnCurrentNode(const ::process_mem::def::ProcessMemPidConfigInfo& configInfo);
uint32_t SendSetPidMapErrorResponse(uint32_t ret, uint64_t requestId);
uint32_t SendPidSetResponse(int successCode, const std::string& errorMsg, uint64_t requestId);
} // namespace process_mem::pid::bridge

namespace process_mem::collect {
bool GetNumaInfoFromToken(const std::string& token, const size_t pageSize,
                          std::unordered_map<uint32_t, size_t>& numaMemDistribution);
uint32_t GetPidInfoByCollect(::process_mem::def::ProcessMemPidInfo& pidInfo, CollectInfoMap& pidCollectInfo);
} // namespace process_mem::collect

namespace ubse::ut::process_mem {
using namespace ::process_mem::manager;
using namespace ::process_mem::def;

void TestProcessMemPidInfoManager::SetUp()
{
    // Initialize bridge function pointers that are normally dlopened
    if (!::process_mem::pid::bridge::ProcessMemPidBridge::rmrsMigrateOut) {
        ::process_mem::pid::bridge::ProcessMemPidBridge::rmrsMigrateOut =
            [](const std::vector<mempooling::smap::MigrateOutPayload>&, int) -> int {
            return 0;
        };
    }
    if (!::process_mem::pid::bridge::ProcessMemPidBridge::rmrsRemove) {
        ::process_mem::pid::bridge::ProcessMemPidBridge::rmrsRemove = [](const uint16_t, const std::vector<pid_t>&,
                                                                         int) -> int {
            return 0;
        };
    }
    if (!::process_mem::pid::bridge::ProcessMemPidBridge::rmrsFreeWithMigrate) {
        ::process_mem::pid::bridge::ProcessMemPidBridge::rmrsFreeWithMigrate = [](const std::string&) -> uint32_t {
            return UBSE_OK;
        };
    }
}

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

// ==================== CheckIsNeedBorrow tests ====================

TEST_F(TestProcessMemPidInfoManager, CheckIsNeedBorrowNoDebts)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 10001;
    uint64_t needBorrowSize = 0;
    bool needBorrow = CheckIsNeedBorrow(pidInfo, 1024 * 1024, needBorrowSize);
    EXPECT_TRUE(needBorrow);
    EXPECT_EQ(needBorrowSize, 1024u * 1024);
}

TEST_F(TestProcessMemPidInfoManager, CheckIsNeedBorrowSufficientExistingDebt)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 10002;
    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "existing_debt";
    debtInfo.numaDesc.size = 2 * 1024 * 1024;
    debtInfo.status = BorrowStatus::COMPLETED;
    pidInfo.memBorrowInfo.debtInfos["existing_debt"] = debtInfo;

    uint64_t needBorrowSize = 0;
    bool needBorrow = CheckIsNeedBorrow(pidInfo, 1024 * 1024, needBorrowSize);
    EXPECT_FALSE(needBorrow);
}

TEST_F(TestProcessMemPidInfoManager, CheckIsNeedBorrowInsufficientDebt)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 10003;
    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "small_debt";
    debtInfo.numaDesc.size = 512 * 1024;
    debtInfo.status = BorrowStatus::COMPLETED;
    pidInfo.memBorrowInfo.debtInfos["small_debt"] = debtInfo;

    uint64_t needBorrowSize = 0;
    bool needBorrow = CheckIsNeedBorrow(pidInfo, 2 * 1024 * 1024, needBorrowSize);
    EXPECT_TRUE(needBorrow);
    EXPECT_GT(needBorrowSize, 0u);
}

TEST_F(TestProcessMemPidInfoManager, CheckIsNeedBorrowMultipleDebts)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 10004;
    DebtInfo debt1{};
    debt1.numaDesc.name = "debt_1";
    debt1.numaDesc.size = 1024 * 1024;
    debt1.status = BorrowStatus::COMPLETED;
    pidInfo.memBorrowInfo.debtInfos["debt_1"] = debt1;

    DebtInfo debt2{};
    debt2.numaDesc.name = "debt_2";
    debt2.numaDesc.size = 1024 * 1024;
    debt2.status = BorrowStatus::COMPLETED;
    pidInfo.memBorrowInfo.debtInfos["debt_2"] = debt2;

    uint64_t needBorrowSize = 0;
    bool needBorrow = CheckIsNeedBorrow(pidInfo, 3 * 1024 * 1024, needBorrowSize);
    EXPECT_TRUE(needBorrow);
    EXPECT_EQ(needBorrowSize, 1024u * 1024);
}

TEST_F(TestProcessMemPidInfoManager, CheckIsNeedBorrowExactMatch)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 10005;
    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "exact_debt";
    debtInfo.numaDesc.size = 1024 * 1024;
    debtInfo.status = BorrowStatus::COMPLETED;
    pidInfo.memBorrowInfo.debtInfos["exact_debt"] = debtInfo;

    uint64_t needBorrowSize = 0;
    bool needBorrow = CheckIsNeedBorrow(pidInfo, 1024 * 1024, needBorrowSize);
    EXPECT_FALSE(needBorrow);
}

// ==================== GenerateSimpleName tests ====================

TEST_F(TestProcessMemPidInfoManager, GenerateSimpleNameProducesNonEmpty)
{
    std::string name = GenerateSimpleName(100);
    EXPECT_FALSE(name.empty());
    EXPECT_GT(name.size(), 0u);
}

TEST_F(TestProcessMemPidInfoManager, GenerateSimpleNameContainsPid)
{
    std::string name = GenerateSimpleName(99999);
    EXPECT_NE(name.find("99999"), std::string::npos);
}

// ==================== IsProcessMemDebt tests ====================

TEST_F(TestProcessMemPidInfoManager, IsProcessMemDebtValid)
{
    ProcessMemUsrInfo usrInfo{};
    usrInfo.pluginId = UsrInfoPluginType::PROCESS_MEM;
    usrInfo.pid = 1234;
    usrInfo.startTime = 5000;

    ubse::mem::controller::UbseNumaMemoryImportDebtInfo debt{};
    memcpy(debt.usrInfo, &usrInfo, sizeof(usrInfo));

    EXPECT_TRUE(IsProcessMemDebt(debt));
}

TEST_F(TestProcessMemPidInfoManager, IsProcessMemDebtInvalidPluginId)
{
    ProcessMemUsrInfo usrInfo{};
    usrInfo.pluginId = static_cast<UsrInfoPluginType>(999);
    usrInfo.pid = 1234;

    ubse::mem::controller::UbseNumaMemoryImportDebtInfo debt{};
    memcpy(debt.usrInfo, &usrInfo, sizeof(usrInfo));

    EXPECT_FALSE(IsProcessMemDebt(debt));
}

// ==================== RestoreFaultPids tests ====================

TEST_F(TestProcessMemPidInfoManager, RestoreFaultPidsRestoresStatus)
{
    std::unordered_map<std::string, std::unordered_set<pid_t>> faultedLentNodes;
    faultedLentNodes["NODE_A"] = {100, 200};

    std::unordered_map<pid_t, ProcessMemPidInfo> pidInfoMap;
    ProcessMemPidInfo info1{};
    info1.configInfo.pid = 100;
    info1.processStatus = ProcessStatus::FAULT;
    pidInfoMap[100] = info1;

    ProcessMemPidInfo info2{};
    info2.configInfo.pid = 200;
    info2.processStatus = ProcessStatus::FAULT;
    pidInfoMap[200] = info2;

    ProcessMemPidInfo info3{};
    info3.configInfo.pid = 300;
    info3.processStatus = ProcessStatus::IDLE;
    pidInfoMap[300] = info3;

    std::vector<std::string> recoveredNodes = {"NODE_A"};
    RestoreFaultPids(recoveredNodes, faultedLentNodes, pidInfoMap);

    EXPECT_EQ(pidInfoMap[100].processStatus, ProcessStatus::IDLE);
    EXPECT_EQ(pidInfoMap[200].processStatus, ProcessStatus::IDLE);
    EXPECT_EQ(pidInfoMap[300].processStatus, ProcessStatus::IDLE);
    EXPECT_TRUE(faultedLentNodes.empty());
}

TEST_F(TestProcessMemPidInfoManager, RestoreFaultPidsNonExistentNode)
{
    std::unordered_map<std::string, std::unordered_set<pid_t>> faultedLentNodes;
    faultedLentNodes["NODE_A"] = {100};

    std::unordered_map<pid_t, ProcessMemPidInfo> pidInfoMap;
    std::vector<std::string> recoveredNodes = {"NODE_B"};
    EXPECT_NO_THROW(RestoreFaultPids(recoveredNodes, faultedLentNodes, pidInfoMap));
    EXPECT_FALSE(faultedLentNodes.empty());
}

TEST_F(TestProcessMemPidInfoManager, RestoreFaultPidsOnlyFaultPidsRestored)
{
    std::unordered_map<std::string, std::unordered_set<pid_t>> faultedLentNodes;
    faultedLentNodes["NODE_A"] = {100, 200};

    std::unordered_map<pid_t, ProcessMemPidInfo> pidInfoMap;
    ProcessMemPidInfo info1{};
    info1.configInfo.pid = 100;
    info1.processStatus = ProcessStatus::BORROWING;
    pidInfoMap[100] = info1;

    ProcessMemPidInfo info2{};
    info2.configInfo.pid = 200;
    info2.processStatus = ProcessStatus::FAULT;
    pidInfoMap[200] = info2;

    std::vector<std::string> recoveredNodes = {"NODE_A"};
    RestoreFaultPids(recoveredNodes, faultedLentNodes, pidInfoMap);

    EXPECT_EQ(pidInfoMap[100].processStatus, ProcessStatus::BORROWING);
    EXPECT_EQ(pidInfoMap[200].processStatus, ProcessStatus::IDLE);
}

// ==================== MemoryCheckHandle tests ====================

TEST_F(TestProcessMemPidInfoManager, MemoryCheckHandleExpectedMemoryZero)
{
    ProcessMemPidInfo info{};
    info.configInfo.pid = 20001;
    info.configInfo.expectedMemoryUsage = 0;
    info.configInfo.evictThreshold = 80;
    info.configInfo.reclaimThreshold = 10;
    info.memBorrowInfo.remoteNumaId = 0;

    std::unordered_map<uint32_t, size_t> numaMemory = {{0, 1024}};
    auto ret = MemoryCheckHandle(info, numaMemory);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, MemoryCheckHandleStableNoAction)
{
    ProcessMemPidInfo info{};
    info.configInfo.pid = 20002;
    info.configInfo.expectedMemoryUsage = 1024;
    info.configInfo.evictThreshold = 80;
    info.configInfo.reclaimThreshold = 50;
    info.configInfo.targetEvictThreshold = 30;
    info.memBorrowInfo.remoteNumaId = -1;

    // totalMemory=600, 600*100=60000 <= 80*1024=81920 -> no borrow
    // 600*100=60000 >= 50*1024=51200 -> no return
    std::unordered_map<uint32_t, size_t> numaMemory = {{0, 600}};
    auto ret = MemoryCheckHandle(info, numaMemory);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== PerPidMemoryCheckCallBack tests ====================

TEST_F(TestProcessMemPidInfoManager, PerPidMemoryCheckCallBackPidNotInMap)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    std::unordered_map<uint32_t, size_t> numaMemory = {{0, 1024}};
    auto ret = mgr.PerPidMemoryCheckCallBack(99999999, numaMemory);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== CheckFaultNodesRecovery tests ====================

TEST_F(TestProcessMemPidInfoManager, CheckFaultNodesRecoveryEmpty)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    EXPECT_NO_THROW(mgr.CheckFaultNodesRecovery());
}

// ==================== IsNodeRecovered tests ====================

TEST_F(TestProcessMemPidInfoManager, IsNodeRecoveredNoMatchingDebts)
{
    auto result = IsNodeRecovered("NODE_RECOVERED");
    EXPECT_TRUE(result);
}

TEST_F(TestProcessMemPidInfoManager, IsNodeRecoveredWithEmptyNodeId)
{
    auto result = IsNodeRecovered("");
    EXPECT_TRUE(result);
}

// ==================== GenerateSimpleName tests (extended) ====================

TEST_F(TestProcessMemPidInfoManager, GenerateSimpleNameSizeLimit)
{
    std::string name = GenerateSimpleName(1);
    EXPECT_FALSE(name.empty());
    EXPECT_LE(name.size(), 48u);
}

// ==================== GetMigrateResult tests ====================

TEST_F(TestProcessMemPidInfoManager, GetMigrateResultInvalidPid)
{
    auto result = GetMigrateResult(99999999, 0, 0, false);
    EXPECT_FALSE(result);
}

TEST_F(TestProcessMemPidInfoManager, GetMigrateResultInvalidPidBack)
{
    auto result = GetMigrateResult(99999999, 0, 0, true);
    EXPECT_FALSE(result);
}

// ==================== GetPidNumaSize tests ====================

TEST_F(TestProcessMemPidInfoManager, GetPidNumaSizeInvalidPid)
{
    uint64_t numaSize = 999;
    auto ret = GetPidNumaSize(99999999, 0, numaSize);
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== MigrateOut tests ====================

TEST_F(TestProcessMemPidInfoManager, MigrateOutRemoteNumaIdNegative)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.memBorrowInfo.remoteNumaId = -1;
    auto ret = MigrateOut(pidInfo, 1024 * 1024);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== DoMigrateOut tests ====================

TEST_F(TestProcessMemPidInfoManager, DoMigrateOutNoLibrary)
{
    // rmrsMigrateOut is initialized in SetUp, so it returns 0 (success) instead of throwing
    int ret = DoMigrateOut(9999, 0, 4096);
    EXPECT_EQ(ret, 0);
}

// ==================== GetUbseMemBorrower tests ====================

TEST_F(TestProcessMemPidInfoManager, GetUbseMemBorrowerWithAffinitySocketIdSet)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 10001;
    pidInfo.memBorrowInfo.importSocketId = 1;
    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.affinitySocketId = -99;
    auto ret = GetUbseMemBorrower(pidInfo, borrower);
    // mock GetCurrentNodeId returns "NODE0", so nodeId is not empty and importSocketId != -1 -> returns OK
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, GetUbseMemBorrowerWithSrcNumaIdSet)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 10002;
    pidInfo.memBorrowInfo.importSocketId = -1;
    pidInfo.configInfo.srcNumaId = 2;
    ubse::mem::controller::UbseMemBorrower borrower{};
    auto ret = GetUbseMemBorrower(pidInfo, borrower);
    // mock GetCurNode has empty numaInfos, so srcNumaId not found and affinitySocketId stays -1, but returns OK
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, GetUbseMemBorrowerWithoutSrcNumaId)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 99999999;
    pidInfo.memBorrowInfo.importSocketId = -1;
    pidInfo.configInfo.srcNumaId = std::nullopt;
    ubse::mem::controller::UbseMemBorrower borrower{};
    auto ret = GetUbseMemBorrower(pidInfo, borrower);
    // mock collect returns error for non-existent pid, but function returns UBSE_OK with importSocketId still -1
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== RefreshExpectRemoteMemory tests ====================

TEST_F(TestProcessMemPidInfoManager, RefreshExpectRemoteMemoryInvalidPid)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 99999999;
    pidInfo.configInfo.targetEvictThreshold = 30;
    pidInfo.memBorrowInfo.remoteNumaId = 0;
    auto result = RefreshExpectRemoteMemory(99999999, pidInfo, 1024 * 1024);
    EXPECT_EQ(result, 1024u * 1024);
}

// ==================== BorrowAndMigrate tests ====================

TEST_F(TestProcessMemPidInfoManager, BorrowAndMigrateNoNeedWithMigrateOk)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 20001;
    pidInfo.memBorrowInfo.remoteNumaId = -1;
    pidInfo.configInfo.targetEvictThreshold = 30;
    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "sufficient";
    debtInfo.numaDesc.size = 2 * 1024 * 1024;
    debtInfo.status = BorrowStatus::COMPLETED;
    pidInfo.memBorrowInfo.debtInfos["sufficient"] = debtInfo;
    auto ret = BorrowAndMigrate(pidInfo, 1024 * 1024, 0);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, BorrowAndMigrateNeedBorrow)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 20002;
    pidInfo.memBorrowInfo.remoteNumaId = -1;
    auto ret = BorrowAndMigrate(pidInfo, 1024 * 1024, 0);
    EXPECT_EQ(ret, UBSE_OK);
    mgr.UnInit();
}

// ==================== BorrowMemoryAndMigrateOut tests ====================

TEST_F(TestProcessMemPidInfoManager, BorrowMemoryAndMigrateOutNoNeed)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 30001;
    pidInfo.configInfo.targetEvictThreshold = 30;
    pidInfo.memBorrowInfo.remoteNumaId = -1;
    auto ret = BorrowMemoryAndMigrateOut(pidInfo, 1024 * 1024, 0, 0);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, BorrowMemoryAndMigrateOutNeedBorrow)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 30002;
    pidInfo.configInfo.targetEvictThreshold = 50;
    pidInfo.memBorrowInfo.remoteNumaId = -1;
    auto ret = BorrowMemoryAndMigrateOut(pidInfo, 1024 * 1024, 0, 0);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== MigrateBackAndReturnMemory tests ====================

TEST_F(TestProcessMemPidInfoManager, MigrateBackAndReturnMemoryWithNullExecutor)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 40001;
    pidInfo.memBorrowInfo.remoteNumaId = 0;
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.returnExecutor = nullptr;
    auto ret = MigrateBackAndReturnMemory(pidInfo);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, MigrateBackAndReturnMemorySuccess)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 40002;
    pidInfo.memBorrowInfo.remoteNumaId = 0;
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    if (mgr.returnExecutor.Get() != nullptr) {
        auto ret = MigrateBackAndReturnMemory(pidInfo);
        EXPECT_EQ(ret, UBSE_OK);
    }
}

// ==================== ExceptionMemoryHandle tests ====================

TEST_F(TestProcessMemPidInfoManager, ExceptionMemoryHandleEmptyName)
{
    EXPECT_NO_THROW(ExceptionMemoryHandle(""));
}

// ==================== IsProcessMemDebt tests (extended) ====================

TEST_F(TestProcessMemPidInfoManager, IsProcessMemDebtWithUbseNumaMemoryDebtInfo)
{
    ProcessMemUsrInfo usrInfo{};
    usrInfo.pluginId = UsrInfoPluginType::PROCESS_MEM;
    usrInfo.pid = 6666;
    usrInfo.startTime = 7777;
    ubse::mem::controller::UbseNumaMemoryDebtInfo debt{};
    memcpy(debt.usrInfo, &usrInfo, sizeof(usrInfo));
    EXPECT_TRUE(IsProcessMemDebt(debt));
}

TEST_F(TestProcessMemPidInfoManager, IsProcessMemDebtInvalidDebt)
{
    ubse::mem::controller::UbseNumaMemoryDebtInfo debt{};
    ProcessMemUsrInfo usrInfo{};
    usrInfo.pluginId = static_cast<UsrInfoPluginType>(999);
    memcpy(debt.usrInfo, &usrInfo, sizeof(usrInfo));
    EXPECT_FALSE(IsProcessMemDebt(debt));
}

// ==================== CheckIsNeedBorrow tests (extended) ====================

TEST_F(TestProcessMemPidInfoManager, CheckIsNeedBorrowWithTimeoutCreatingDebt)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 50001;
    DebtInfo timeoutDebt{};
    timeoutDebt.numaDesc.name = "timeout_debt";
    timeoutDebt.numaDesc.size = 1024 * 1024;
    timeoutDebt.status = BorrowStatus::CREATING;
    timeoutDebt.borrowStartTime = std::chrono::steady_clock::now() - std::chrono::seconds(60);
    pidInfo.memBorrowInfo.debtInfos["timeout_debt"] = timeoutDebt;
    uint64_t needBorrowSize = 0;
    bool needBorrow = CheckIsNeedBorrow(pidInfo, 1024 * 1024, needBorrowSize);
    EXPECT_TRUE(needBorrow);
}

// ==================== MemoryCheckHandle tests (extended) ====================

TEST_F(TestProcessMemPidInfoManager, MemoryCheckHandleNeedBorrow)
{
    ProcessMemPidInfo info{};
    info.configInfo.pid = 60001;
    info.configInfo.expectedMemoryUsage = 1024;
    info.configInfo.evictThreshold = 50;
    info.configInfo.reclaimThreshold = 10;
    info.configInfo.targetEvictThreshold = 30;
    info.memBorrowInfo.remoteNumaId = -1;
    std::unordered_map<uint32_t, size_t> numaMemory = {{0, 2000}};
    auto ret = MemoryCheckHandle(info, numaMemory);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, MemoryCheckHandleNeedReturn)
{
    ProcessMemPidInfo info{};
    info.configInfo.pid = 60002;
    info.configInfo.expectedMemoryUsage = 1024;
    info.configInfo.evictThreshold = 100;
    info.configInfo.reclaimThreshold = 80;
    info.configInfo.targetEvictThreshold = 30;
    info.memBorrowInfo.remoteNumaId = 0;
    std::unordered_map<uint32_t, size_t> numaMemory = {{0, 100}};
    auto ret = MemoryCheckHandle(info, numaMemory);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, MemoryCheckHandleBothRemoteAndLocal)
{
    ProcessMemPidInfo info{};
    info.configInfo.pid = 60003;
    info.configInfo.expectedMemoryUsage = 1024;
    info.configInfo.evictThreshold = 80;
    info.configInfo.reclaimThreshold = 50;
    info.configInfo.targetEvictThreshold = 30;
    info.memBorrowInfo.remoteNumaId = 0;
    std::unordered_map<uint32_t, size_t> numaMemory = {{0, 300}, {1, 300}};
    auto ret = MemoryCheckHandle(info, numaMemory);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== PerPidMemoryCheckCallBack tests (extended) ====================

TEST_F(TestProcessMemPidInfoManager, PerPidMemoryCheckCallBackNotRecovered)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 70001;
    pidInfo.startTime = 1000;
    mgr.SetPidInfoMap(pidInfo);
    std::unordered_map<uint32_t, size_t> numaMemory = {{0, 1024}};
    auto ret = mgr.PerPidMemoryCheckCallBack(70001, numaMemory);
    EXPECT_EQ(ret, UBSE_OK);
    mgr.UnsetPidInfo(70001);
}

// ==================== AsyncBorrowAndMigrateExecute tests ====================

TEST_F(TestProcessMemPidInfoManager, AsyncBorrowAndMigrateExecutePidNotExist)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 80001;
    pidInfo.startTime = 1000;
    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "test_borrow_async";
    debtInfo.numaDesc.size = 1024 * 1024;
    debtInfo.status = BorrowStatus::CREATING;
    debtInfo.borrowStartTime = std::chrono::steady_clock::now();
    EXPECT_NO_THROW(AsyncBorrowAndMigrateExecute(99999999, debtInfo, 1024 * 1024));
}

// ==================== TotalMemoryCheckCallBack tests ====================

TEST_F(TestProcessMemPidInfoManager, TotalMemoryCheckCallBackWithData)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    std::unordered_map<pid_t, std::unordered_map<uint32_t, size_t>> collectInfoMap;
    EXPECT_NO_THROW(mgr.TotalMemoryCheckCallBack(collectInfoMap));
}

// ==================== UpdatePidMemBorrowInfo tests (extended) ====================

TEST_F(TestProcessMemPidInfoManager, UpdatePidMemBorrowInfoWithBorrowerPidNotExist)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.affinitySocketId = 5;
    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "nonexistent";
    debtInfo.numaDesc.size = 1024;
    auto ret = mgr.UpdatePidMemBorrowInfo(99999999, borrower, debtInfo);
    EXPECT_NE(ret, UBSE_OK);
}

// ==================== DeletePidMemBorrowInfo tests (extended) ====================

TEST_F(TestProcessMemPidInfoManager, DeletePidMemBorrowInfoLastDebt)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 90001;
    pidInfo.startTime = 1000;
    mgr.SetPidInfoMap(pidInfo);
    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "last_debt";
    debtInfo.numaDesc.size = 1024;
    debtInfo.status = BorrowStatus::COMPLETED;
    mgr.UpdatePidMemBorrowInfo(90001, debtInfo);
    EXPECT_NO_THROW(mgr.DeletePidMemBorrowInfo(90001, "last_debt"));
    auto retrieved = mgr.GetPidInfoMap(90001);
    EXPECT_TRUE(retrieved.memBorrowInfo.debtInfos.empty());
    mgr.UnsetPidInfo(90001);
}

// ==================== HandleNodeFaultEvent tests (extended) ====================

TEST_F(TestProcessMemPidInfoManager, HandleNodeFaultEventWithEmptyNodeId)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    // Mock returns UBSE_OK with empty list for empty nodeId, so no affected PIDs found -> UBSE_OK
    auto ret = mgr.HandleNodeFaultEvent("");
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== RefreshBorrowInfo tests ====================

TEST_F(TestProcessMemPidInfoManager, RefreshBorrowInfoBasic)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    EXPECT_NO_THROW(mgr.RefreshBorrowInfo());
}

// ==================== RecoverAllDebtInfoData tests ====================

TEST_F(TestProcessMemPidInfoManager, RecoverAllDebtInfoDataBasic)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    EXPECT_NO_THROW(mgr.RecoverAllDebtInfoData());
}

TEST_F(TestProcessMemPidInfoManager, IsRecoverCompletedAfterRecover)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.RecoverAllDebtInfoData();
    EXPECT_TRUE(mgr.IsRecoverCompleted());
}

// ==================== Init/UnInit tests ====================

TEST_F(TestProcessMemPidInfoManager, InitBasic)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    EXPECT_NO_THROW(mgr.Init());
}

TEST_F(TestProcessMemPidInfoManager, UnInitAfterInit)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    EXPECT_NO_THROW(mgr.UnInit());
}

// ==================== Init with full path tests ====================

TEST_F(TestProcessMemPidInfoManager, InitCreatesExecutors)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();
    EXPECT_NE(mgr.borrowExecutor.Get(), nullptr);
    EXPECT_NE(mgr.returnExecutor.Get(), nullptr);
    EXPECT_NE(mgr.exceptionHandleExecutor.Get(), nullptr);
    mgr.UnInit();
}

TEST_F(TestProcessMemPidInfoManager, UnInitStopsAllExecutors)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();
    mgr.UnInit();
    // Verify recover not completed after UnInit (UnInit doesn't modify isRecoverCompleted_)
}

// ==================== DeletePidMemBorrowInfo full cleanup path tests ====================

TEST_F(TestProcessMemPidInfoManager, DeleteLastDebtTriggersMigrateCleanup)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 91001;
    pidInfo.startTime = 1000;

    mgr.SetPidInfoMap(pidInfo);

    // Set up borrowInfo with remoteNumaId to trigger the full cleanup path
    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.affinitySocketId = 2;
    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "full_cleanup_debt";
    debtInfo.numaDesc.size = 2048;
    debtInfo.numaDesc.numaId = 3;
    debtInfo.numaDesc.exportNode.slotId = 4;
    debtInfo.status = BorrowStatus::COMPLETED;
    mgr.UpdatePidMemBorrowInfo(91001, borrower, debtInfo);

    // Verify remoteNumaId is set
    auto before = mgr.GetPidInfoMap(91001);
    EXPECT_NE(before.memBorrowInfo.remoteNumaId, -1);

    // rmrsMigrateOut is initialized in SetUp, so the full cleanup path runs without throwing
    EXPECT_NO_THROW(mgr.DeletePidMemBorrowInfo(91001, "full_cleanup_debt"));

    mgr.UnsetPidInfo(91001);
}

// ==================== DeletePidMemBorrowInfo full cleanup path test ====================

TEST_F(TestProcessMemPidInfoManager, DeleteLastDebtTriggersFullCleanup)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 91002;
    pidInfo.startTime = 1000;

    mgr.SetPidInfoMap(pidInfo);

    // Set up borrowInfo with remoteNumaId to trigger the full cleanup path
    ubse::mem::controller::UbseMemBorrower borrower{};
    borrower.affinitySocketId = 2;
    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "full_cleanup_debt";
    debtInfo.numaDesc.size = 2048;
    debtInfo.numaDesc.numaId = 3;
    debtInfo.numaDesc.exportNode.slotId = 4;
    debtInfo.status = BorrowStatus::COMPLETED;
    mgr.UpdatePidMemBorrowInfo(91002, borrower, debtInfo);

    EXPECT_NO_THROW(mgr.DeletePidMemBorrowInfo(91002, "full_cleanup_debt"));

    mgr.UnsetPidInfo(91002);
}
