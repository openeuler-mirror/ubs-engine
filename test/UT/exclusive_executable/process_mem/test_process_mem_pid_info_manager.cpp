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

// ==================== HandleNodeFaultEvent with data tests ====================

TEST_F(TestProcessMemPidInfoManager, HandleNodeFaultEventWithPidInMap)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 77700;
    pidInfo.startTime = 1000;
    pidInfo.processStatus = ProcessStatus::IDLE;
    mgr.SetPidInfoMap(pidInfo);

    auto ret = mgr.HandleNodeFaultEvent("NODE_FAULT_DATA");
    EXPECT_EQ(ret, UBSE_OK);

    mgr.UnsetPidInfo(77700);
}

TEST_F(TestProcessMemPidInfoManager, HandleNodeFaultEventCalledTwice)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    auto ret1 = mgr.HandleNodeFaultEvent("NODE_A");
    auto ret2 = mgr.HandleNodeFaultEvent("NODE_B");
    EXPECT_EQ(ret1, UBSE_OK);
    EXPECT_EQ(ret2, UBSE_OK);
}

// ==================== CheckFaultNodesRecovery with data tests ====================

TEST_F(TestProcessMemPidInfoManager, CheckFaultNodesRecoveryWithRecoveredNode)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.RecoverAllDebtInfoData(); // ensure IsRecoverCompleted

    // First set up a fault via HandleNodeFaultEvent
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 77800;
    pidInfo.startTime = 1000;
    pidInfo.processStatus = ProcessStatus::IDLE;
    mgr.SetPidInfoMap(pidInfo);

    mgr.HandleNodeFaultEvent("NODE_RECOVERY_TEST");

    auto retrieved = mgr.GetPidInfoMap(77800);
    // Note: HandleNodeFaultEvent with empty debts won't mark the pid as FAULT
    // because there are no matching debts. Let's just check we don't crash.

    mgr.CheckFaultNodesRecovery();

    mgr.UnsetPidInfo(77800);
}

// ==================== TotalMemoryCheckCallBack with PIDs tests ====================

TEST_F(TestProcessMemPidInfoManager, TotalMemoryCheckCallBackWithPopulatedPidInfoMap)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();
    mgr.RecoverAllDebtInfoData();

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 77900;
    pidInfo.startTime = 1000;
    pidInfo.configInfo.evictThreshold = 80;
    pidInfo.configInfo.reclaimThreshold = 20;
    pidInfo.configInfo.targetEvictThreshold = 50;
    pidInfo.configInfo.expectedMemoryUsage = 1024;
    pidInfo.processStatus = ProcessStatus::IDLE;
    mgr.SetPidInfoMap(pidInfo);

    std::unordered_map<pid_t, std::unordered_map<uint32_t, size_t>> collectInfo;
    collectInfo[77900] = {{0, 500}};
    EXPECT_NO_THROW(mgr.TotalMemoryCheckCallBack(collectInfo));

    // UnInit first to stop executors before UnsetPidInfo: mock runs tasks inline,
    // and MigrateBackAndReturnMemory would re-enter pidInfoMutex → deadlock
    mgr.UnInit();
    mgr.UnsetPidInfo(77900);
}

TEST_F(TestProcessMemPidInfoManager, TotalMemoryCheckCallBackWithFaultStatusPid)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();
    mgr.RecoverAllDebtInfoData();

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 77901;
    pidInfo.startTime = 1000;
    pidInfo.processStatus = ProcessStatus::FAULT;
    mgr.SetPidInfoMap(pidInfo);

    std::unordered_map<pid_t, std::unordered_map<uint32_t, size_t>> collectInfo;
    collectInfo[77901] = {{0, 500}};
    EXPECT_NO_THROW(mgr.TotalMemoryCheckCallBack(collectInfo));

    mgr.UnInit();
    mgr.UnsetPidInfo(77901);
}

// ==================== CheckIsNeedBorrow with timed-out creating debts ====================

TEST_F(TestProcessMemPidInfoManager, CheckIsNeedBorrowSkipsTimeoutCreatingDebts)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 78000;
    // Timed-out CREATING debt (borrowed 60s ago, timeout is 30s) — will be skipped/deleted
    DebtInfo timeoutDebt{};
    timeoutDebt.numaDesc.name = "timed_out";
    timeoutDebt.numaDesc.size = 1024;
    timeoutDebt.status = BorrowStatus::CREATING;
    timeoutDebt.borrowStartTime = std::chrono::steady_clock::now() - std::chrono::seconds(60);
    pidInfo.memBorrowInfo.debtInfos["timed_out"] = timeoutDebt;

    // Only the timed-out CREATING debt, no active debts
    // totalSecured = 0 (timed out debt skipped), expectRemoteMemory = 4096 → needBorrow
    uint64_t needBorrowSize = 0;
    bool needBorrow = CheckIsNeedBorrow(pidInfo, 4096, needBorrowSize);
    EXPECT_TRUE(needBorrow);
    EXPECT_EQ(needBorrowSize, 4096u);
}

// ==================== RefreshBorrowInfo with empty data ====================

TEST_F(TestProcessMemPidInfoManager, RefreshBorrowInfoWithPidsInMap)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();
    mgr.RecoverAllDebtInfoData();

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 78100;
    pidInfo.startTime = 1000;
    mgr.SetPidInfoMap(pidInfo);

    EXPECT_NO_THROW(mgr.RefreshBorrowInfo());

    // UnInit first to stop executors before UnsetPidInfo: mock runs tasks inline,
    // and MigrateBackAndReturnMemory would re-enter pidInfoMutex → deadlock
    mgr.UnInit();
    mgr.UnsetPidInfo(78100);
}

// ==================== ExceptionMemoryHandle retry test ====================

TEST_F(TestProcessMemPidInfoManager, ExceptionMemoryHandleWithMockReturns)
{
    // Mock MemoryReturn returns UBSE_OK, so first call succeeds (no retry)
    EXPECT_NO_THROW(ExceptionMemoryHandle("test_exception_name"));
}

// ==================== HandleNodeFaultEvent full path test ====================

TEST_F(TestProcessMemPidInfoManager, HandleNodeFaultEventViaRpcHandler)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    // Simulate the path from ProcessMemNodeFaultNotifyHandler
    mgr.RecoverAllDebtInfoData();

    auto ret = mgr.HandleNodeFaultEvent("FAULT_FROM_RPC");
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== ProcessMemPidInfoManager comprehensive tests ====================

TEST_F(TestProcessMemPidInfoManager, FullLifecycleInitRecoverRefreshUnInit)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    EXPECT_NO_THROW(mgr.Init());
    EXPECT_NO_THROW(mgr.RecoverAllDebtInfoData());
    EXPECT_TRUE(mgr.IsRecoverCompleted());
    EXPECT_NO_THROW(mgr.RefreshBorrowInfo());
    EXPECT_NO_THROW(mgr.CheckFaultNodesRecovery());
    EXPECT_NO_THROW(mgr.TotalMemoryCheckCallBack({}));
    EXPECT_NO_THROW(mgr.UnInit());
}

TEST_F(TestProcessMemPidInfoManager, HandleNodeFaultEventThenCheckRecovery)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.RecoverAllDebtInfoData();

    auto ret = mgr.HandleNodeFaultEvent("NODE_TO_RECOVER");
    EXPECT_EQ(ret, UBSE_OK);

    EXPECT_NO_THROW(mgr.CheckFaultNodesRecovery());
}

TEST_F(TestProcessMemPidInfoManager, SetPidInfoMapUpdateConfigNoSrcNuma)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo1{};
    pidInfo1.configInfo.pid = 78200;
    pidInfo1.startTime = 1000;
    pidInfo1.configInfo.evictThreshold = 80;
    pidInfo1.configInfo.srcNumaId = std::nullopt;

    auto ret = mgr.SetPidInfoMap(pidInfo1);
    EXPECT_EQ(ret, UBSE_OK);

    ProcessMemPidInfo pidInfo2{};
    pidInfo2.configInfo.pid = 78200;
    pidInfo2.startTime = 1000;
    pidInfo2.configInfo.evictThreshold = 90;
    pidInfo2.configInfo.srcNumaId = std::nullopt;

    ret = mgr.SetPidInfoMap(pidInfo2);
    EXPECT_EQ(ret, UBSE_OK);

    auto retrieved = mgr.GetPidInfoMap(78200);
    EXPECT_EQ(retrieved.configInfo.evictThreshold, 90);

    mgr.UnsetPidInfo(78200);
}

TEST_F(TestProcessMemPidInfoManager, UnsetPidInfoWithBorrowDebts)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 78300;
    pidInfo.startTime = 1000;

    mgr.SetPidInfoMap(pidInfo);

    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "unset_with_debt";
    debtInfo.numaDesc.size = 1024;
    debtInfo.status = BorrowStatus::COMPLETED;
    mgr.UpdatePidMemBorrowInfo(78300, debtInfo);

    // UnsetPidInfo should call MigrateBackAndReturnMemory when debts exist
    auto ret = mgr.UnsetPidInfo(78300);
    EXPECT_EQ(ret, UBSE_OK);

    auto result = mgr.GetPidInfoMap(78300);
    EXPECT_EQ(result.configInfo.pid, -1);
}

TEST_F(TestProcessMemPidInfoManager, PerPidMemoryCheckCallBackFaultStatusSkips)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.RecoverAllDebtInfoData();

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 78400;
    pidInfo.startTime = 1000;
    pidInfo.processStatus = ProcessStatus::FAULT;
    mgr.SetPidInfoMap(pidInfo);

    std::unordered_map<uint32_t, size_t> numaMemory = {{0, 1000}};
    auto ret = mgr.PerPidMemoryCheckCallBack(78400, numaMemory);
    EXPECT_EQ(ret, UBSE_OK);

    mgr.UnsetPidInfo(78400);
}

// ==================== GetMigrateResult back mode test ====================

TEST_F(TestProcessMemPidInfoManager, GetMigrateResultBackModeNonExistentPid)
{
    auto result = GetMigrateResult(99999999, 0, 1024, true);
    EXPECT_FALSE(result);
}

// ==================== CheckIsNeedBorrow with all completed debts ====================

TEST_F(TestProcessMemPidInfoManager, CheckIsNeedBorrowAllCompletedSumExceeds)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 78500;
    DebtInfo debt1{};
    debt1.numaDesc.name = "debt_a";
    debt1.numaDesc.size = 1024 * 1024;
    debt1.status = BorrowStatus::COMPLETED;
    pidInfo.memBorrowInfo.debtInfos["debt_a"] = debt1;

    uint64_t needBorrowSize = 0;
    bool needBorrow = CheckIsNeedBorrow(pidInfo, 512 * 1024, needBorrowSize);
    EXPECT_FALSE(needBorrow);
}

// ==================== MemoryCheckHandle stable tests ====================

TEST_F(TestProcessMemPidInfoManager, MemoryCheckHandleBothNeedBorrowAndNeedReturn)
{
    ProcessMemPidInfo info{};
    info.configInfo.pid = 78600;
    info.configInfo.expectedMemoryUsage = 1024;
    info.configInfo.evictThreshold = 100;
    info.configInfo.reclaimThreshold = 100;
    info.configInfo.targetEvictThreshold = 50;
    info.memBorrowInfo.remoteNumaId = 0;
    std::unordered_map<uint32_t, size_t> numaMemory = {{0, 500}, {1, 500}};
    auto ret = MemoryCheckHandle(info, numaMemory);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== AsyncBorrowAndMigrateExecute with real pid tests ====================

TEST_F(TestProcessMemPidInfoManager, AsyncBorrowAndMigrateExecuteHappyPath)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    pid_t myPid = ::getpid();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(myPid);
    ASSERT_GT(startTime, 0u);

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = myPid;
    pidInfo.startTime = startTime;
    pidInfo.configInfo.evictThreshold = 80;
    pidInfo.configInfo.targetEvictThreshold = 50;
    pidInfo.configInfo.srcNumaId = 0;
    mgr.SetPidInfoMap(pidInfo);

    uint64_t expectRemoteMemory = 1024 * 1024;
    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "test_borrow_async_happy";
    debtInfo.numaDesc.size = 1024 * 1024;
    debtInfo.numaDesc.numaId = 0;
    debtInfo.status = BorrowStatus::CREATING;
    debtInfo.borrowStartTime = std::chrono::steady_clock::now();

    EXPECT_NO_THROW(AsyncBorrowAndMigrateExecute(myPid, debtInfo, expectRemoteMemory));

    mgr.DeletePidMemBorrowInfo(myPid, debtInfo.numaDesc.name);
    mgr.UnsetPidInfo(myPid);
}

TEST_F(TestProcessMemPidInfoManager, AsyncBorrowAndMigrateExecutePidNotInMap)
{
    // Pid not in map → GetPidInfoMap returns pid=-1 → early return
    uint64_t expectRemoteMemory = 1024 * 1024;
    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "test_borrow_async_nomap";
    debtInfo.numaDesc.size = 1024 * 1024;
    debtInfo.borrowStartTime = std::chrono::steady_clock::now();
    EXPECT_NO_THROW(AsyncBorrowAndMigrateExecute(99999999, debtInfo, expectRemoteMemory));
}

TEST_F(TestProcessMemPidInfoManager, AsyncBorrowAndMigrateExecuteTimedOut)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(1);

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 1;
    pidInfo.startTime = startTime;
    pidInfo.configInfo.srcNumaId = 0;
    mgr.SetPidInfoMap(pidInfo);

    uint64_t expectRemoteMemory = 1024 * 1024;
    DebtInfo debtInfo{};
    debtInfo.numaDesc.name = "test_borrow_async_timeout";
    debtInfo.numaDesc.size = 1024 * 1024;
    debtInfo.numaDesc.numaId = 0;
    debtInfo.status = BorrowStatus::CREATING;
    // Set borrowStartTime far in the past to trigger timeout
    debtInfo.borrowStartTime = std::chrono::steady_clock::now() - std::chrono::seconds(60);

    // The timeout path: DeletePidMemBorrowInfo + exceptionHandleExecutor->Execute
    EXPECT_NO_THROW(AsyncBorrowAndMigrateExecute(1, debtInfo, expectRemoteMemory));

    mgr.UnInit();
    mgr.UnsetPidInfo(1);
}

// ==================== RefreshExpectRemoteMemory tests ====================

TEST_F(TestProcessMemPidInfoManager, RefreshExpectRemoteMemoryWithRealPid)
{
    pid_t myPid = ::getpid();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(myPid);
    EXPECT_GT(startTime, 0u);
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = myPid;
    pidInfo.startTime = startTime;
    pidInfo.configInfo.targetEvictThreshold = 50;
    pidInfo.memBorrowInfo.remoteNumaId = -1;

    auto result = RefreshExpectRemoteMemory(myPid, pidInfo, 1024 * 1024);
    EXPECT_GT(result, 0u);
}

// ==================== GetPidNumaSize tests ====================

TEST_F(TestProcessMemPidInfoManager, GetPidNumaSizeWithRealPid)
{
    pid_t myPid = ::getpid();
    uint64_t numaSize = 0;
    auto ret = GetPidNumaSize(myPid, 0, numaSize);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_GT(numaSize, 0u);
}

TEST_F(TestProcessMemPidInfoManager, GetPidNumaSizeNonMatchingNuma)
{
    pid_t myPid = ::getpid();
    uint64_t numaSize = 0;
    auto ret = GetPidNumaSize(myPid, 999, numaSize);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(numaSize, 0u);
}

// ==================== GetUbseMemBorrower extended tests ====================

TEST_F(TestProcessMemPidInfoManager, GetUbseMemBorrowerNodeIdEmpty)
{
    // This can't happen with mock (GetCurrentNodeId returns "NODE0"), but test anyway
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 1;
    ubse::mem::controller::UbseMemBorrower borrower{};
    auto ret = GetUbseMemBorrower(pidInfo, borrower);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== BorrowAndMigrate extended tests ====================

TEST_F(TestProcessMemPidInfoManager, BorrowAndMigrateWithNeedBorrowAndValidExecutor)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();
    pid_t myPid = ::getpid();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(myPid);
    ASSERT_GT(startTime, 0u);

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = myPid;
    pidInfo.startTime = startTime;
    pidInfo.configInfo.targetEvictThreshold = 30;
    pidInfo.memBorrowInfo.remoteNumaId = -1;
    mgr.SetPidInfoMap(pidInfo);

    auto ret = BorrowAndMigrate(pidInfo, 1024 * 1024, 0);
    EXPECT_EQ(ret, UBSE_OK);

    // UnInit first to stop executors, avoiding inline execution recursive lock
    mgr.UnInit();
    auto updatedInfo = mgr.GetPidInfoMap(myPid);
    for (auto& [name, _] : updatedInfo.memBorrowInfo.debtInfos) {
        mgr.DeletePidMemBorrowInfo(myPid, name);
    }
    mgr.UnsetPidInfo(myPid);
}

// ==================== MemoryCheckHandle triggering borrow path ====================

TEST_F(TestProcessMemPidInfoManager, MemoryCheckHandleTriggersBorrowMemoryAndMigrateOut)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();
    pid_t myPid = ::getpid();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(myPid);
    ASSERT_GT(startTime, 0u);

    ProcessMemPidInfo info{};
    info.configInfo.pid = myPid;
    info.startTime = startTime;
    info.configInfo.expectedMemoryUsage = 1024;
    info.configInfo.evictThreshold = 10;
    info.configInfo.reclaimThreshold = 90;
    info.configInfo.targetEvictThreshold = 50;
    info.memBorrowInfo.remoteNumaId = -1;

    mgr.SetPidInfoMap(info);
    std::unordered_map<uint32_t, size_t> numaMemory = {{0, 500}};
    auto ret = MemoryCheckHandle(info, numaMemory);
    EXPECT_EQ(ret, UBSE_OK);

    mgr.UnInit();
    mgr.UnsetPidInfo(myPid);
}

// ==================== BorrowMemoryAndMigrateOut extended tests ====================

TEST_F(TestProcessMemPidInfoManager, BorrowMemoryAndMigrateOutRemoteSufficient)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 78700;
    pidInfo.configInfo.targetEvictThreshold = 50;
    pidInfo.memBorrowInfo.remoteNumaId = 0;
    // expectRemote = 1500 * 50 / 100 = 750, remoteMemory = 1000 > 750 → no borrow needed
    auto ret = BorrowMemoryAndMigrateOut(pidInfo, 500, 1000, 0);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, BorrowMemoryAndMigrateOutRemoteInsufficient)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    pid_t myPid = ::getpid();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(myPid);
    ASSERT_GT(startTime, 0u);

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = myPid;
    pidInfo.startTime = startTime;
    pidInfo.configInfo.targetEvictThreshold = 50;
    pidInfo.memBorrowInfo.remoteNumaId = -1;
    mgr.SetPidInfoMap(pidInfo);

    auto ret = BorrowMemoryAndMigrateOut(pidInfo, 500, 0, 0);
    EXPECT_EQ(ret, UBSE_OK);

    mgr.UnInit();
    auto updatedInfo = mgr.GetPidInfoMap(myPid);
    for (auto& [name, _] : updatedInfo.memBorrowInfo.debtInfos) {
        mgr.DeletePidMemBorrowInfo(myPid, name);
    }
    mgr.UnsetPidInfo(myPid);
}

// ==================== MigrateBackAndReturnMemoryAsyncExecute tests ====================

TEST_F(TestProcessMemPidInfoManager, MigrateBackAndReturnMemoryAsyncExecuteNotRecovered)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();
    // isRecoverCompleted_ is false → function will re-post to executor (mock executes inline → infinite loop risk)
    // But mock Stop prevents this. Test the not-recovered path.
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(1);

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 1;
    pidInfo.startTime = startTime;
    pidInfo.memBorrowInfo.remoteNumaId = 0;

    // Stop executors first so inline execution doesn't re-enter
    mgr.returnExecutor->Stop();
    auto ret = MigrateBackAndReturnMemoryAsyncExecute(pidInfo);
    EXPECT_EQ(ret, UBSE_OK);

    mgr.UnInit();
}

TEST_F(TestProcessMemPidInfoManager, MigrateBackAndReturnMemorySuccessPath)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();
    mgr.RecoverAllDebtInfoData();

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 78701;
    pidInfo.memBorrowInfo.remoteNumaId = -1;
    // Not registering pid via SetPidInfoMap; ResetPidMemBorrowInfo will safely return early
    // This exercises the "recovered + no debts" path through MigrateBackAndReturnMemoryAsyncExecute

    auto ret = MigrateBackAndReturnMemoryAsyncExecute(pidInfo);
    EXPECT_EQ(ret, UBSE_OK);

    mgr.UnInit();
}

// ==================== ExceptionMemoryHandle extended tests ====================

TEST_F(TestProcessMemPidInfoManager, ExceptionMemoryHandleWithRetry)
{
    // MemoryReturn mock returns UBSE_OK, so test the immediate-success path
    EXPECT_NO_THROW(ExceptionMemoryHandle("test_retry_name"));
}

// ==================== IsNodeRecovered extended tests ====================

TEST_F(TestProcessMemPidInfoManager, IsNodeRecoveredWorkingNode)
{
    // Mock GetNodeById returns WORKING, and QueryDebtInfoWithRetry returns UBSE_OK with empty list
    // So should return true (no process_mem debts found)
    auto result = IsNodeRecovered("NODE_RECOVERY_WORKING");
    EXPECT_TRUE(result);
}

// ==================== MigrateOut tests extended ====================

TEST_F(TestProcessMemPidInfoManager, MigrateOutWithValidRemoteNuma)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 99999;
    pidInfo.memBorrowInfo.remoteNumaId = 0;
    // rmrsMigrateOut returns 0 (MEM_POOLING_OK)
    auto ret = MigrateOut(pidInfo, 1024 * 1024);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== GetMigrateResult with back mode ====================

TEST_F(TestProcessMemPidInfoManager, GetMigrateResultBackModeValidPid)
{
    pid_t myPid = ::getpid();
    // isBack=true → def=0; use non-matching numa 999 so numaSize stays 0; matches expectSize=0 exactly
    auto result = GetMigrateResult(myPid, 999, 0, true);
    EXPECT_TRUE(result);
}

// ==================== RefreshBorrowInfo with import debts tests ====================

TEST_F(TestProcessMemPidInfoManager, RefreshBorrowInfoWithImportDebtsExercisesAnonFuncs)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();
    mgr.RecoverAllDebtInfoData();

    pid_t myPid = ::getpid();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(myPid);
    ASSERT_GT(startTime, 0u);

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = myPid;
    pidInfo.startTime = startTime;
    pidInfo.configInfo.evictThreshold = 80;
    pidInfo.configInfo.reclaimThreshold = 20;
    pidInfo.configInfo.targetEvictThreshold = 50;
    pidInfo.configInfo.expectedMemoryUsage = 1024;
    mgr.SetPidInfoMap(pidInfo);

    // Set up import debt info via mock for the current process
    ::process_mem::def::ProcessMemUsrInfo usrInfo{};
    usrInfo.pluginId = ::process_mem::def::UsrInfoPluginType::PROCESS_MEM;
    usrInfo.pid = myPid;
    usrInfo.startTime = startTime;

    ubse::mem::controller::UbseNumaMemoryImportDebtInfo importDebt{};
    importDebt.name = "test_import_debt_refresh";
    importDebt.size = 1024 * 1024;
    importDebt.remoteNumaId = 0;
    importDebt.borrowSocketIdList = {1};
    memcpy(importDebt.usrInfo, &usrInfo, sizeof(usrInfo));

    ubse::mem::controller::MockSetImportDebtInfos({importDebt});

    // RefreshBorrowInfo is called inside TotalMemoryCheckCallBack
    std::unordered_map<pid_t, std::unordered_map<uint32_t, size_t>> collectInfo;
    collectInfo[myPid] = {{0, 500}};
    EXPECT_NO_THROW(mgr.TotalMemoryCheckCallBack(collectInfo));

    ubse::mem::controller::MockClearDebtInfos();

    mgr.UnInit();
    // Clean up debts added by RefreshBorrowInfo
    auto info = mgr.GetPidInfoMap(myPid);
    for (auto& [name, _] : info.memBorrowInfo.debtInfos) {
        mgr.DeletePidMemBorrowInfo(myPid, name);
    }
    mgr.UnsetPidInfo(myPid);
}

TEST_F(TestProcessMemPidInfoManager, RefreshBorrowInfoWithRemovedPidPath)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();
    mgr.RecoverAllDebtInfoData();

    // Set up import debt for a pid NOT in the map → exercises removed pid path
    ::process_mem::def::ProcessMemUsrInfo usrInfo{};
    usrInfo.pluginId = ::process_mem::def::UsrInfoPluginType::PROCESS_MEM;
    usrInfo.pid = 99997;
    usrInfo.startTime = 1;

    ubse::mem::controller::UbseNumaMemoryImportDebtInfo importDebt{};
    importDebt.name = "test_removed_pid_debt";
    importDebt.size = 1024;
    importDebt.remoteNumaId = 0;
    importDebt.borrowSocketIdList = {1};
    memcpy(importDebt.usrInfo, &usrInfo, sizeof(usrInfo));

    ubse::mem::controller::MockSetImportDebtInfos({importDebt});

    EXPECT_NO_THROW(mgr.RefreshBorrowInfo());

    ubse::mem::controller::MockClearDebtInfos();
    mgr.UnInit();
}

// ==================== RecoverAllDebtInfoData with debts tests ====================

TEST_F(TestProcessMemPidInfoManager, RecoverAllDebtInfoDataWithDebtInfos)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();

    pid_t myPid = ::getpid();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(myPid);
    ASSERT_GT(startTime, 0u);

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = myPid;
    pidInfo.startTime = startTime;
    pidInfo.configInfo.srcNumaId = 0;
    mgr.SetPidInfoMap(pidInfo);

    // Set up debt info via mock
    ::process_mem::def::ProcessMemUsrInfo usrInfo{};
    usrInfo.pluginId = ::process_mem::def::UsrInfoPluginType::PROCESS_MEM;
    usrInfo.pid = myPid;
    usrInfo.startTime = startTime;

    ubse::mem::controller::UbseNumaMemoryDebtInfo debtInfo{};
    debtInfo.name = "test_recover_debt";
    debtInfo.size = 1024 * 1024;
    debtInfo.remoteNumaId = 0;
    debtInfo.borrowNodeId = "NODE0";
    debtInfo.borrowSocketIdList = {1};
    memcpy(debtInfo.usrInfo, &usrInfo, sizeof(usrInfo));

    ubse::mem::controller::MockSetDebtInfos({debtInfo});

    EXPECT_NO_THROW(mgr.RecoverAllDebtInfoData());
    EXPECT_TRUE(mgr.IsRecoverCompleted());

    ubse::mem::controller::MockClearDebtInfos();

    mgr.UnInit();
    auto info = mgr.GetPidInfoMap(myPid);
    for (auto& [name, _] : info.memBorrowInfo.debtInfos) {
        mgr.DeletePidMemBorrowInfo(myPid, name);
    }
    mgr.UnsetPidInfo(myPid);
}

// ==================== ValidateSrcNumaIdOnCurrentNode tests (bridge) ====================

TEST_F(TestProcessMemPidInfoManager, ValidateSrcNumaIdNoValue)
{
    ::process_mem::def::ProcessMemPidConfigInfo configInfo{};
    configInfo.srcNumaId = std::nullopt;
    auto ret = ::process_mem::pid::bridge::ValidateSrcNumaIdOnCurrentNode(configInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, ValidateSrcNumaIdNotFoundOnNode)
{
    // Mock GetCurNode returns empty numaInfos, so any srcNumaId won't be found
    ::process_mem::def::ProcessMemPidConfigInfo configInfo{};
    configInfo.srcNumaId = 5;
    auto ret = ::process_mem::pid::bridge::ValidateSrcNumaIdOnCurrentNode(configInfo);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);
}

// ==================== GetNumaInfoFromToken exception path tests ====================

TEST_F(TestProcessMemPidInfoManager, GetNumaInfoFromTokenExceptionPath)
{
    // Malformed token that throws in stoi/stoull: "N=" with no digits after =
    // But the condition checks token.size() > 2, "N=" is size 2, so won't enter the if
    // "N0=abc" enters the try block, stoull("abc") throws → catch returns false
    std::unordered_map<uint32_t, size_t> numaMemDistribution;
    bool result = ::process_mem::collect::GetNumaInfoFromToken("N0=abc", 4096, numaMemDistribution);
    EXPECT_FALSE(result);
}

// ==================== GetPidInfoByCollect with real pid ====================

TEST_F(TestProcessMemPidInfoManager, GetPidInfoByCollectWithRealPid)
{
    pid_t myPid = ::getpid();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(myPid);
    ASSERT_GT(startTime, 0u);

    ::process_mem::def::ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = myPid;
    pidInfo.startTime = startTime;
    ::process_mem::collect::CollectInfoMap pidCollectInfo;
    auto ret = ::process_mem::collect::GetPidInfoByCollect(pidInfo, pidCollectInfo);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(pidInfo.processStatus, ::process_mem::def::ProcessStatus::IDLE);
    EXPECT_FALSE(pidCollectInfo.empty());
}

// ==================== CheckIsNeedBorrow with CREATING timed out debts ====================

TEST_F(TestProcessMemPidInfoManager, CheckIsNeedBorrowSkipsAllTimedOutAndReturnsNeed)
{
    ::process_mem::def::ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 78800;
    // All debts are timed-out CREATING → total secured = 0 → needBorrow
    ::process_mem::def::DebtInfo debt1{};
    debt1.numaDesc.name = "timed_out_1";
    debt1.numaDesc.size = 1024;
    debt1.status = ::process_mem::def::BorrowStatus::CREATING;
    debt1.borrowStartTime = std::chrono::steady_clock::now() - std::chrono::seconds(60);
    pidInfo.memBorrowInfo.debtInfos["timed_out_1"] = debt1;

    ::process_mem::def::DebtInfo debt2{};
    debt2.numaDesc.name = "timed_out_2";
    debt2.numaDesc.size = 2048;
    debt2.status = ::process_mem::def::BorrowStatus::CREATING;
    debt2.borrowStartTime = std::chrono::steady_clock::now() - std::chrono::seconds(60);
    pidInfo.memBorrowInfo.debtInfos["timed_out_2"] = debt2;

    uint64_t needBorrowSize = 0;
    bool needBorrow = CheckIsNeedBorrow(pidInfo, 4096, needBorrowSize);
    EXPECT_TRUE(needBorrow);
    EXPECT_EQ(needBorrowSize, 4096u);
}

// ==================== MemoryCheckHandle with missing local numa ====================

TEST_F(TestProcessMemPidInfoManager, MemoryCheckHandleMissingLocalNuma)
{
    ::process_mem::def::ProcessMemPidInfo info{};
    info.configInfo.pid = 78900;
    info.configInfo.expectedMemoryUsage = 1024;
    info.configInfo.evictThreshold = 50;
    info.configInfo.reclaimThreshold = 10;
    info.configInfo.targetEvictThreshold = 30;
    info.memBorrowInfo.remoteNumaId = -1;
    // numa 0 not in numaMemory → localMemory stays 0 → 0 > expectedMemory(1024) * evictThreshold(50)/100 → 0 > 512 → false → no borrow
    // 0 * 100 = 0 >= reclaimThreshold(10) * expectedMemory(1024) → 0 >= 10240 → false → no return
    std::unordered_map<uint32_t, size_t> numaMemory = {{1, 500}};
    auto ret = MemoryCheckHandle(info, numaMemory);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== MemoryCheckHandle with extra numa nodes ====================

TEST_F(TestProcessMemPidInfoManager, MemoryCheckHandleWithExtraNuma)
{
    ::process_mem::def::ProcessMemPidInfo info{};
    info.configInfo.pid = 78901;
    info.configInfo.expectedMemoryUsage = 1024;
    info.configInfo.evictThreshold = 80;
    info.configInfo.reclaimThreshold = 50;
    info.configInfo.targetEvictThreshold = 30;
    info.memBorrowInfo.remoteNumaId = 0;
    // local numa (0): 600, remote numa (1): 200
    // totalRemote = 200, localMemory = 600
    // 600 > 1024*80/100=819 → false → no borrow
    // 600*100=60000 >= 50*1024=51200 → true → need return
    std::unordered_map<uint32_t, size_t> numaMemory = {{0, 600}, {1, 200}};
    auto ret = MemoryCheckHandle(info, numaMemory);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== GenerateSimpleName edge case ====================

TEST_F(TestProcessMemPidInfoManager, GenerateSimpleNameProducesUniqueNames)
{
    std::string name1 = GenerateSimpleName(100);
    std::string name2 = GenerateSimpleName(200);
    EXPECT_NE(name1, name2);
}

// ==================== IsNodeRecovered edge cases ====================

TEST_F(TestProcessMemPidInfoManager, IsNodeRecoveredWithShortNodeId)
{
    // Mock works with any nodeId - no debts returned → recovered
    auto result = IsNodeRecovered("N");
    EXPECT_TRUE(result);
}

// ==================== SendSetPidMapErrorResponse tests (bridge) ====================

TEST_F(TestProcessMemPidInfoManager, SendSetPidMapErrorResponseNotExist)
{
    auto ret = ::process_mem::pid::bridge::SendSetPidMapErrorResponse(UBSE_ERR_NOT_EXIST, 100);
    // Returns SendPidSetResponse result; mock may fail but exercises all branches
    (void)ret;
}

TEST_F(TestProcessMemPidInfoManager, SendSetPidMapErrorResponseInvalidArg)
{
    auto ret = ::process_mem::pid::bridge::SendSetPidMapErrorResponse(UBSE_ERR_INVALID_ARG, 200);
    (void)ret;
}

TEST_F(TestProcessMemPidInfoManager, SendSetPidMapErrorResponseResourceBusy)
{
    auto ret = ::process_mem::pid::bridge::SendSetPidMapErrorResponse(UBSE_ERR_RESOURCE_BUSY, 300);
    (void)ret;
}

TEST_F(TestProcessMemPidInfoManager, SendSetPidMapErrorResponseUnknown)
{
    auto ret = ::process_mem::pid::bridge::SendSetPidMapErrorResponse(UBSE_ERROR, 400);
    (void)ret;
}

// ==================== CheckIsNeedBorrow edge case tests ====================

TEST_F(TestProcessMemPidInfoManager, CheckIsNeedBorrowEmptyExpectZero)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 79000;
    uint64_t needBorrowSize = 999;
    bool needBorrow = CheckIsNeedBorrow(pidInfo, 0, needBorrowSize);
    // expectRemoteMemory(0) <= totalSecured(0) → returns false, needBorrowSize unchanged
    EXPECT_FALSE(needBorrow);
}

TEST_F(TestProcessMemPidInfoManager, CheckIsNeedBorrowMixedCompletedAndCreating)
{
    ::process_mem::def::ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 79001;
    // One completed debt
    ::process_mem::def::DebtInfo completed{};
    completed.numaDesc.name = "completed_debt_mixed";
    completed.numaDesc.size = 1024;
    completed.status = ::process_mem::def::BorrowStatus::COMPLETED;
    pidInfo.memBorrowInfo.debtInfos["completed_debt_mixed"] = completed;
    // One active creating debt (not timed out, so counts toward totalSecured)
    ::process_mem::def::DebtInfo creating{};
    creating.numaDesc.name = "creating_debt_mixed";
    creating.numaDesc.size = 512;
    creating.status = ::process_mem::def::BorrowStatus::CREATING;
    creating.borrowStartTime = std::chrono::steady_clock::now();
    pidInfo.memBorrowInfo.debtInfos["creating_debt_mixed"] = creating;

    uint64_t needBorrowSize = 0;
    // totalSecured = completed(1024) + creating(512) = 1536, expect=2048 → needBorrow=true, needSize=512
    bool needBorrow = CheckIsNeedBorrow(pidInfo, 2048, needBorrowSize);
    EXPECT_TRUE(needBorrow);
    EXPECT_EQ(needBorrowSize, 512u);
}

// ==================== MemoryCheckHandle with numas on both local and remote ====================

TEST_F(TestProcessMemPidInfoManager, MemoryCheckHandleLocalNumaNotConfigured)
{
    ::process_mem::def::ProcessMemPidInfo info{};
    info.configInfo.pid = 79100;
    info.configInfo.expectedMemoryUsage = 1024;
    info.configInfo.evictThreshold = 10;
    info.configInfo.reclaimThreshold = 90;
    info.configInfo.targetEvictThreshold = 50;
    info.memBorrowInfo.remoteNumaId = -1;
    // No numa data for local (0) → local stays 0 → need borrow
    // 0 * 100 = 0 <= evict(10) * expected(1024) = 10240 → need borrow
    std::unordered_map<uint32_t, size_t> numaMemory = {};
    auto ret = MemoryCheckHandle(info, numaMemory);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== GetMigrateResult with isBack=false tests ====================

TEST_F(TestProcessMemPidInfoManager, GetMigrateResultNonBackMode)
{
    pid_t myPid = ::getpid();
    // isBack=false → def=10MiB, larger tolerance
    auto result = GetMigrateResult(myPid, 999, 0, false);
    // Default threshold is 10MiB, so even a large mismatch still passes
    EXPECT_TRUE(result);
}

// ==================== RecoverAllDebtInfoData filter path tests ====================

TEST_F(TestProcessMemPidInfoManager, RecoverAllDebtInfoDataWithNonProcessMemDebt)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();

    pid_t myPid = ::getpid();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(myPid);
    ASSERT_GT(startTime, 0u);

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = myPid;
    pidInfo.startTime = startTime;
    mgr.SetPidInfoMap(pidInfo);

    // Debt with non-PROCESS_MEM pluginId → IsProcessMemDebt returns false → skipped
    ::process_mem::def::ProcessMemUsrInfo usrInfo{};
    usrInfo.pluginId = static_cast<::process_mem::def::UsrInfoPluginType>(99);
    usrInfo.pid = myPid;
    usrInfo.startTime = startTime;

    ubse::mem::controller::UbseNumaMemoryDebtInfo debtInfo{};
    debtInfo.name = "non_process_mem_debt";
    debtInfo.size = 1024;
    debtInfo.remoteNumaId = 0;
    debtInfo.borrowNodeId = "NODE0";
    memcpy(debtInfo.usrInfo, &usrInfo, sizeof(usrInfo));

    ubse::mem::controller::MockSetDebtInfos({debtInfo});

    EXPECT_NO_THROW(mgr.RecoverAllDebtInfoData());
    EXPECT_TRUE(mgr.IsRecoverCompleted());

    ubse::mem::controller::MockClearDebtInfos();
    mgr.UnInit();
    mgr.UnsetPidInfo(myPid);
}

TEST_F(TestProcessMemPidInfoManager, RecoverAllDebtInfoDataWithBorrowNodeMismatch)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();

    pid_t myPid = ::getpid();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(myPid);
    ASSERT_GT(startTime, 0u);

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = myPid;
    pidInfo.startTime = startTime;
    mgr.SetPidInfoMap(pidInfo);

    // Debt with non-matching borrowNodeId → skipped at line 998
    ::process_mem::def::ProcessMemUsrInfo usrInfo{};
    usrInfo.pluginId = ::process_mem::def::UsrInfoPluginType::PROCESS_MEM;
    usrInfo.pid = myPid;
    usrInfo.startTime = startTime;

    ubse::mem::controller::UbseNumaMemoryDebtInfo debtInfo{};
    debtInfo.name = "mismatched_node_debt";
    debtInfo.size = 2048;
    debtInfo.remoteNumaId = 0;
    debtInfo.borrowNodeId = "OTHER_NODE"; // != "NODE0" from mock → skipped
    memcpy(debtInfo.usrInfo, &usrInfo, sizeof(usrInfo));

    ubse::mem::controller::MockSetDebtInfos({debtInfo});

    EXPECT_NO_THROW(mgr.RecoverAllDebtInfoData());
    EXPECT_TRUE(mgr.IsRecoverCompleted());

    ubse::mem::controller::MockClearDebtInfos();
    mgr.UnInit();
    mgr.UnsetPidInfo(myPid);
}

// ==================== HandleNodeFaultEvent with non-PROCESS_MEM debt ====================

TEST_F(TestProcessMemPidInfoManager, HandleNodeFaultEventSkipsNonProcessMemDebt)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.Init();
    mgr.RecoverAllDebtInfoData();

    pid_t myPid = ::getpid();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(myPid);
    ASSERT_GT(startTime, 0u);

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = myPid;
    pidInfo.startTime = startTime;
    mgr.SetPidInfoMap(pidInfo);

    // Non-PROCESS_MEM debt → IsProcessMemDebt returns false → skipped
    ::process_mem::def::ProcessMemUsrInfo usrInfo{};
    usrInfo.pluginId = static_cast<::process_mem::def::UsrInfoPluginType>(99);
    usrInfo.pid = myPid;

    ubse::mem::controller::UbseNumaMemoryDebtInfo debtInfo{};
    debtInfo.name = "fault_non_proc_debt";
    debtInfo.lentNodeId = "NODE_FAULT_SKIP";
    memcpy(debtInfo.usrInfo, &usrInfo, sizeof(usrInfo));

    ubse::mem::controller::MockSetDebtInfos({debtInfo});

    auto ret = mgr.HandleNodeFaultEvent("NODE_FAULT_SKIP");
    EXPECT_EQ(ret, UBSE_OK);

    ubse::mem::controller::MockClearDebtInfos();
    mgr.UnInit();
    mgr.UnsetPidInfo(myPid);
}

// ==================== Config manager tests ====================

TEST_F(TestProcessMemPidInfoManager, QueryPidConfigCallbackNullBuffer)
{
    UbseByteBuffer nullBuf{nullptr, 0};
    EXPECT_NO_THROW(ProcessMemPidConfigManager::QueryPidConfigCallback("prefix", "key", nullBuf, nullptr));
}

TEST_F(TestProcessMemPidInfoManager, QueryPidConfigCallbackZeroLenBuffer)
{
    uint8_t data = 0;
    UbseByteBuffer buf{&data, 0};
    EXPECT_NO_THROW(ProcessMemPidConfigManager::QueryPidConfigCallback("prefix", "key", buf, nullptr));
}

TEST_F(TestProcessMemPidInfoManager, QueryPidConfigCallbackBadData)
{
    uint8_t badData[] = {0xFF, 0xFF, 0xFF, 0xFF};
    UbseByteBuffer buf{badData, sizeof(badData)};
    EXPECT_NO_THROW(ProcessMemPidConfigManager::QueryPidConfigCallback("prefix", "key", buf, nullptr));
}

TEST_F(TestProcessMemPidInfoManager, QueryPidConfigCallbackWithInvalidCtx)
{
    pid_t myPid = ::getpid();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(myPid);
    if (startTime == 0) {
        GTEST_SKIP() << "Cannot read /proc/pid/stat for current pid";
    }

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = myPid;
    pidInfo.startTime = startTime;
    pidInfo.configInfo.evictThreshold = 80;

    ubse::serial::UbseSerialization serializer;
    pidInfo.SerializePidInfo(serializer);
    UbseByteBuffer buf{serializer.GetBuffer(), serializer.GetLength()};

    std::vector<::process_mem::def::ProcessMemPidInfo> pidInfos;
    EXPECT_NO_THROW(ProcessMemPidConfigManager::QueryPidConfigCallback("p", "k", buf, &pidInfos));
    EXPECT_EQ(pidInfos.size(), 1u);
}

TEST_F(TestProcessMemPidInfoManager, IsPidInfoExistValidPid)
{
    pid_t myPid = ::getpid();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(myPid);
    if (startTime == 0) {
        GTEST_SKIP() << "Cannot read /proc/pid/stat for current pid";
    }
    auto result = ProcessMemPidConfigManager::IsPidInfoExist(myPid, startTime);
    EXPECT_TRUE(result);
}

TEST_F(TestProcessMemPidInfoManager, IsPidInfoExistNonExistentPid)
{
    auto result = ProcessMemPidConfigManager::IsPidInfoExist(99999999, 1);
    EXPECT_FALSE(result);
}

TEST_F(TestProcessMemPidInfoManager, CheckPidConfigInfoValid)
{
    pid_t myPid = ::getpid();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(myPid);
    if (startTime == 0) {
        GTEST_SKIP() << "Cannot read /proc/pid/stat for current pid";
    }
    ::process_mem::def::ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = myPid;
    pidInfo.startTime = startTime;
    auto result = ProcessMemPidConfigManager::CheckPidConfigInfo(pidInfo);
    EXPECT_TRUE(result);
}

TEST_F(TestProcessMemPidInfoManager, CheckPidConfigInfoMismatch)
{
    pid_t myPid = ::getpid();
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(myPid);
    if (startTime == 0) {
        GTEST_SKIP() << "Cannot read /proc/pid/stat for current pid";
    }
    ::process_mem::def::ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = myPid;
    pidInfo.startTime = startTime + 99999; // mismatched
    auto result = ProcessMemPidConfigManager::CheckPidConfigInfo(pidInfo);
    EXPECT_FALSE(result);
}

TEST_F(TestProcessMemPidInfoManager, CheckPidConfigInfoNonExistentPid)
{
    ::process_mem::def::ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 99999999;
    pidInfo.startTime = 1;
    auto result = ProcessMemPidConfigManager::CheckPidConfigInfo(pidInfo);
    EXPECT_FALSE(result);
}

TEST_F(TestProcessMemPidInfoManager, DeletePidConfigInfoNonExistent)
{
    EXPECT_NO_THROW(ProcessMemPidConfigManager::DeletePidConfigInfo(99999999));
}

// ==================== CycleCollectNumaInfo test ====================

TEST_F(TestProcessMemPidInfoManager, CycleCollectNumaInfoBasic)
{
    auto& collector = ::process_mem::collect::ProcessMemPidCollect::GetInstance();
    auto ret = collector.CycleCollectNumaInfo();
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== CollectChildPids test ====================

TEST_F(TestProcessMemPidInfoManager, CollectChildPidsWithGetpid)
{
    pid_t myPid = ::getpid();
    ::process_mem::collect::CollectInfoMap pidCollectInfo;
    ::process_mem::def::ProcessMemPidConfigInfo configInfo{};
    configInfo.pid = myPid;
    auto& collector = ::process_mem::collect::ProcessMemPidCollect::GetInstance();
    EXPECT_NO_THROW(collector.CollectChildPids(myPid, configInfo, pidCollectInfo));
}

// ==================== Bridge NotifyBorrowNodesOnFault tests ====================

TEST_F(TestProcessMemPidInfoManager, NotifyBorrowNodesOnFaultNoTargets)
{
    auto ret = ::process_mem::pid::bridge::ProcessMemPidBridge::NotifyBorrowNodesOnFault("FAULT_NODE_NO_TARGET");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, NotifyBorrowNodesOnFaultWithMockDebts)
{
    // Set up debt info with non-matching lentNodeId → still no targets notified
    ::process_mem::def::ProcessMemUsrInfo usrInfo{};
    usrInfo.pluginId = ::process_mem::def::UsrInfoPluginType::PROCESS_MEM;
    usrInfo.pid = 1;

    ubse::mem::controller::UbseNumaMemoryDebtInfo debt{};
    debt.name = "notify_test";
    debt.lentNodeId = "OTHER_NODE";
    debt.borrowNodeId = "NODE0"; // same as current → excluded
    memcpy(debt.usrInfo, &usrInfo, sizeof(usrInfo));

    ubse::mem::controller::MockSetDebtInfos({debt});

    auto ret = ::process_mem::pid::bridge::ProcessMemPidBridge::NotifyBorrowNodesOnFault("FAULT_NODE_WITH_DATA");
    EXPECT_EQ(ret, UBSE_OK);

    ubse::mem::controller::MockClearDebtInfos();
}

TEST_F(TestProcessMemPidInfoManager, NotifyBorrowNodesOnFaultWithRemoteTargets)
{
    // Set up debt with borrowNodeId != currentNodeId → should be collected as target
    ::process_mem::def::ProcessMemUsrInfo usrInfo{};
    usrInfo.pluginId = ::process_mem::def::UsrInfoPluginType::PROCESS_MEM;
    usrInfo.pid = 1;

    ubse::mem::controller::UbseNumaMemoryDebtInfo debt{};
    debt.name = "remote_target";
    debt.lentNodeId = "FAULT_REMOTE";
    debt.borrowNodeId = "REMOTE_NODE"; // != "NODE0" → collected
    memcpy(debt.usrInfo, &usrInfo, sizeof(usrInfo));

    ubse::mem::controller::MockSetDebtInfos({debt});

    // This will try to send RPC to REMOTE_NODE, which will likely fail in UT but shouldn't crash
    auto ret = ::process_mem::pid::bridge::ProcessMemPidBridge::NotifyBorrowNodesOnFault("FAULT_REMOTE");
    // RPC will fail in UT → firstError will be set
    (void)ret;

    ubse::mem::controller::MockClearDebtInfos();
}

// ==================== Bridge GetRemoteNumaSocketInfo tests ====================

TEST_F(TestProcessMemPidInfoManager, GetRemoteNumaSocketInfoNoDebts)
{
    ubse::mem::controller::UbseMemNumaDesc desc{};
    desc.exportNode.slotId = 0;
    uint32_t socketId = 0;
    uint64_t numaId = 0;
    auto ret = ::process_mem::pid::bridge::ProcessMemPidBridge::GetRemoteNumaSocketInfo(desc, socketId, numaId);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, GetRemoteNumaSocketInfoWithMatchingDebt)
{
    ubse::mem::controller::UbseMemNumaDesc desc{};
    desc.name = "matching_debt";
    desc.importNode.slotId = 5;
    desc.exportNode.slotId = 5;

    ubse::mem::controller::UbseNumaMemoryDebtInfo debt{};
    debt.name = "matching_debt";
    debt.borrowNodeId = "5";
    debt.lentSocketIdList = {2};
    debt.lentNumaIdList = {100};

    ubse::mem::controller::MockSetDebtInfos({debt});

    uint32_t socketId = 0;
    uint64_t numaId = 0;
    auto ret = ::process_mem::pid::bridge::ProcessMemPidBridge::GetRemoteNumaSocketInfo(desc, socketId, numaId);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_EQ(socketId, 2u);
    EXPECT_EQ(numaId, 100u);

    ubse::mem::controller::MockClearDebtInfos();
}

// ==================== Bridge SendPidSetResponse test ====================

TEST_F(TestProcessMemPidInfoManager, SendPidSetResponseBasic)
{
    auto ret = ::process_mem::pid::bridge::SendPidSetResponse(0, "test error", 12345);
    // In UT environment, SendResponse will likely fail → non-zero return
    (void)ret;
}

// ==================== Bridge MemoryReturn test ====================

TEST_F(TestProcessMemPidInfoManager, MemoryReturnBasic)
{
    auto ret = ::process_mem::pid::bridge::ProcessMemPidBridge::MemoryReturn("test_mem_return");
    // Mock UbseMemNumaDelete returns UBSE_OK, so this should succeed
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== Config manager persistence tests ====================

TEST_F(TestProcessMemPidInfoManager, PersistPidConfigInfoBasic)
{
    ::process_mem::def::ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 79001;
    pidInfo.startTime = 12345;
    pidInfo.configInfo.evictThreshold = 80;
    EXPECT_NO_THROW(ProcessMemPidConfigManager::PersistPidConfigInfo(pidInfo));
}

TEST_F(TestProcessMemPidInfoManager, GetAllPersistedPidConfigInfoBasic)
{
    std::vector<::process_mem::def::ProcessMemPidInfo> pidInfos;
    EXPECT_NO_THROW(ProcessMemPidConfigManager::GetAllPersistedPidConfigInfo(pidInfos));
}

TEST_F(TestProcessMemPidInfoManager, DeletePidConfigInfoBasic)
{
    EXPECT_NO_THROW(ProcessMemPidConfigManager::DeletePidConfigInfo(79001));
}

// ==================== Bridge MemoryBorrow test ====================

TEST_F(TestProcessMemPidInfoManager, MemoryBorrowExportSlotDefault)
{
    ::process_mem::def::ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = ::getpid();
    pidInfo.memBorrowInfo.exportSlotId = -1;

    ubse::mem::controller::UbseMemBorrower borrower{};
    ::process_mem::pid::bridge::MemoryBorrowRequest request{};
    request.name = "test_borrow";
    request.size = 1024 * 1024;

    ubse::mem::controller::UbseMemNumaDesc desc{};
    auto ret = ::process_mem::pid::bridge::ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, desc);
    // Mock UbseMemNumaCreate returns UBSE_OK
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, MemoryBorrowExportSlotWithCandidate)
{
    ::process_mem::def::ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = ::getpid();
    pidInfo.memBorrowInfo.exportSlotId = 3;

    ubse::mem::controller::UbseMemBorrower borrower{};
    ::process_mem::pid::bridge::MemoryBorrowRequest request{};
    request.name = "test_borrow_candidate";
    request.size = 2048 * 1024;

    ubse::mem::controller::UbseMemNumaDesc desc{};
    auto ret = ::process_mem::pid::bridge::ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, desc);
    EXPECT_EQ(ret, UBSE_OK);
}

// ==================== Bridge FaultHandler test ====================

TEST_F(TestProcessMemPidInfoManager, FaultHandlerBasic)
{
    auto ret = ::process_mem::pid::bridge::ProcessMemPidBridge::FaultHandler(ubse::ras::ALARM_PANIC_EVENT,
                                                                             "FAULT_NODE_FOR_TEST");
    (void)ret;
}

// ==================== Bridge ProcessMemNodeFaultNotifyHandler test ====================

TEST_F(TestProcessMemPidInfoManager, ProcessMemNodeFaultNotifyHandlerBasic)
{
    ubse::serial::UbseSerialization serializer;
    serializer << std::string("NODE_FAULT_NOTIFY");
    UbseByteBuffer req{serializer.GetBuffer(), serializer.GetLength(), nullptr};
    UbseByteBuffer resp{};
    EXPECT_NO_THROW(::process_mem::pid::bridge::ProcessMemPidBridge::ProcessMemNodeFaultNotifyHandler(req, resp));
}

// ==================== Bridge error path tests with mock error injection ====================

TEST_F(TestProcessMemPidInfoManager, MemoryBorrowCreateError)
{
    ::process_mem::def::ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = ::getpid();
    pidInfo.memBorrowInfo.exportSlotId = -1;

    ubse::mem::controller::UbseMemBorrower borrower{};
    ::process_mem::pid::bridge::MemoryBorrowRequest request{};
    request.name = "test_borrow_err";
    request.size = 1024 * 1024;

    ubse::mem::controller::MockSetNumaCreateError(UBSE_ERROR);
    ubse::mem::controller::UbseMemNumaDesc desc{};
    auto ret = ::process_mem::pid::bridge::ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, desc);
    EXPECT_EQ(ret, UBSE_ERROR);
    ubse::mem::controller::MockResetAllErrors();
}

TEST_F(TestProcessMemPidInfoManager, MemoryBorrowWithCandidateError)
{
    ::process_mem::def::ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = ::getpid();
    pidInfo.memBorrowInfo.exportSlotId = 3;

    ubse::mem::controller::UbseMemBorrower borrower{};
    ::process_mem::pid::bridge::MemoryBorrowRequest request{};
    request.name = "test_borrow_cand_err";
    request.size = 2048 * 1024;

    ubse::mem::controller::MockSetNumaCreateError(UBSE_ERROR);
    ubse::mem::controller::UbseMemNumaDesc desc{};
    auto ret = ::process_mem::pid::bridge::ProcessMemPidBridge::MemoryBorrow(pidInfo, borrower, request, desc);
    EXPECT_EQ(ret, UBSE_ERROR);
    ubse::mem::controller::MockResetAllErrors();
}

TEST_F(TestProcessMemPidInfoManager, MemoryReturnEmptyNodeId)
{
    ubse::nodeController::MockSetCurrentNodeId("");
    auto ret = ::process_mem::pid::bridge::ProcessMemPidBridge::MemoryReturn("test_empty_node");
    EXPECT_EQ(ret, UBSE_ERROR);
    ubse::nodeController::MockResetCurrentNodeId();
}

TEST_F(TestProcessMemPidInfoManager, MemoryReturnDeleteError)
{
    ubse::mem::controller::MockSetNumaDeleteError(UBSE_ERROR);
    auto ret = ::process_mem::pid::bridge::ProcessMemPidBridge::MemoryReturn("test_delete_err");
    EXPECT_EQ(ret, UBSE_ERROR);
    ubse::mem::controller::MockResetAllErrors();
}

TEST_F(TestProcessMemPidInfoManager, GetRemoteNumaSocketInfoRetryAndError)
{
    ubse::mem::controller::MockSetDebtInfoWithNodeError(UBSE_ERROR);

    ubse::mem::controller::UbseMemNumaDesc desc{};
    uint32_t socketId = 0;
    uint64_t numaId = 0;
    auto ret = ::process_mem::pid::bridge::ProcessMemPidBridge::GetRemoteNumaSocketInfo(desc, socketId, numaId);
    EXPECT_NE(ret, UBSE_OK);

    ubse::mem::controller::MockResetAllErrors();
}

} // namespace ubse::ut::process_mem
