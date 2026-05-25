/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "mp_smap_module.h"
#include "over_commit_msg.h"
#include "over_commit_msg_handler.h"

#include "collect_util.h"
#define private public
#include "vm_mem_migrate_strategy.h"
#undef private

#define MOCKER_CPP(api, TT) MOCKCPP_NS::mockAPI<>::get(#api, "", api)

namespace mempooling::outinterface {
using namespace ubse::log;
using namespace mempooling::over_commit;
using namespace mempooling::smap;

bool containsPid(const std::vector<pid_t>& pids, pid_t targetPid);
void CalculateVmDemandAndQuota(const std::unordered_map<pid_t, VMInfo>& vmInfos, uint8_t ratio,
                               std::map<pid_t, uint64_t>& vmDemand, std::map<pid_t, uint64_t>& vmQuota);

class MockVMMemMigrateStrategy : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override
    {
        GlobalMockObject::verify();
    }
};

class StrategyProxy {
public:
    static MpResult ExecuteProxy(const std::unordered_map<pid_t, VMInfo>& vmInfos, std::vector<RemoteNUMA>& remoteNUMAs,
                                 std::vector<VMResult>& vmResults)
    {
        VMMemMigrateStrategy strategy;
        return strategy.Execute(vmInfos, remoteNUMAs, vmResults);
    }
};

MpResult MockStrategyExecuteSuccess(std::unordered_map<pid_t, VMInfo>& vmInfos, std::vector<RemoteNUMA>& remoteNUMAs,
                                    std::vector<VMResult>& vmResults)
{
    RemoteNUMA remoteNuma(0, 1024);
    remoteNUMAs.push_back(remoteNuma);
    return MEM_POOLING_OK;
}

MpSceneType MockGetMpSceneType()
{
    return MpSceneType::VIRTUAL_SCENE;
}

std::vector<VmDomainInfo> MockGetVmDomainInfos()
{
    VmDomainInfo info;
    info.metaData.pid = 8;
    VmDomainNumaInfo vmNumaInfo1 = {1, 2048, 2, 0, 1};
    info.numaInfo[1] = vmNumaInfo1;
    VmDomainNumaInfo vmNumaInfo2 = {0, 2048, 0, -1, 0};
    info.numaInfo[0] = vmNumaInfo2;
    info.metaData.maxMem = 24;
    std::vector<VmDomainInfo> vmDomainInfos;
    vmDomainInfos.push_back(info);
    return vmDomainInfos;
}

// 测试 execute 函数
void FillParam1(std::unordered_map<pid_t, VMInfo>& vmInfos)
{
    VMInfo vm8;
    // 和remoteNUMA 1已借用4，本次再借用8
    vm8.totalRemoteUsedMem = 4;
    vm8.totalLocalUsedMem = 20;
    vm8.ratio = 50;
    vm8.remoteNumaId = 1;
    vm8.pid = 8;
    vmInfos[vm8.pid] = vm8;

    // 已借用NUMA2，可再借用14，但由于NUMA2只有5，故借用5
    VMInfo vm6;
    vm6.totalRemoteUsedMem = 2;
    vm6.totalLocalUsedMem = 30;
    vm6.ratio = 50;
    vm6.remoteNumaId = 2;
    vm6.pid = 6;
    vmInfos[vm6.pid] = vm6;

    // 无借用，本次借用15，因NUMA 2只借来5已被vm 6使用，故vm5从NUMA1借用（但NUMA1只剩12，所以只借用12）
    VMInfo vm5;
    vm5.totalRemoteUsedMem = 0;
    vm5.totalLocalUsedMem = 30;
    vm5.ratio = 50;
    vm5.remoteNumaId = 0;
    vm5.pid = 5;
    vmInfos[vm5.pid] = vm5;

    // 无借用，本次借用10，无可借用numa，不计入vm result
    VMInfo vm4;
    vm4.totalRemoteUsedMem = 0;
    vm4.totalLocalUsedMem = 20;
    vm4.ratio = 50;
    vm4.remoteNumaId = 0;
    vm4.pid = 4;
    vmInfos[vm4.pid] = vm4;

    VMInfo vm7;
    // 已借用NUMA3，无法在本次借用NUMA 1 & 2
    vm7.totalRemoteUsedMem = 5;
    vm7.remoteNumaId = 3;
    vm7.pid = 7;
    vmInfos[vm7.pid] = vm7;
    return;
}

TEST(VMMemMigrateStrategyTest, Execute)
{
    std::unordered_map<pid_t, VMInfo> vmInfos;
    std::vector<RemoteNUMA> remoteNUMAs;
    std::vector<VMResult> vmResults;
    FillParam1(vmInfos);
    // Numa1借来100，满足虚机8&5需求
    RemoteNUMA numa1;
    numa1.remoteNumaId = 1;
    numa1.borrowSize = 20;
    remoteNUMAs.push_back(numa1);

    // Numa2只借来5，小于虚机6需求
    RemoteNUMA numa2;
    numa2.remoteNumaId = 2;
    numa2.borrowSize = 5;
    remoteNUMAs.push_back(numa2);

    // Numa3无可借用内存，小于虚机7需求
    RemoteNUMA numa3;
    numa2.remoteNumaId = 3;
    numa2.borrowSize = 0;
    remoteNUMAs.push_back(numa3);

    // 调用 execute 函数
    VMMemMigrateStrategy strategy;
    uint32_t result = strategy.Execute(vmInfos, remoteNUMAs, vmResults);
    ASSERT_EQ(result, 0);
}

// 测试 execute 函数的极端情况
void FillParam2(std::unordered_map<pid_t, VMInfo>& vmInfos)
{
    VMInfo vm8;
    // 和remote NUMA 1已借用4，本次再借用8
    vm8.totalRemoteUsedMem = 4;
    vm8.totalLocalUsedMem = 20;
    vm8.ratio = 50;
    vm8.remoteNumaId = 1;
    vm8.pid = 8;
    vmInfos[vm8.pid] = vm8;

    // 已借用NUMA2，可再借用14，
    VMInfo vm6;
    vm6.totalRemoteUsedMem = 2;
    vm6.totalLocalUsedMem = 30;
    vm6.ratio = 50;
    vm6.remoteNumaId = 2;
    vm6.pid = 6;
    vmInfos[vm6.pid] = vm6;

    // 无借用，本次借用15.
    VMInfo vm5;
    vm5.totalRemoteUsedMem = 0;
    vm5.totalLocalUsedMem = 30;
    vm5.ratio = 50;
    vm5.remoteNumaId = 0;
    vm5.pid = 5;
    vmInfos[vm5.pid] = vm5;

    // 无借用，本次借用10，无可借用numa，不计入vm result
    VMInfo vm4;
    vm4.totalRemoteUsedMem = 0;
    vm4.totalLocalUsedMem = 20;
    vm4.ratio = 50;
    vm4.remoteNumaId = 0;
    vm4.pid = 4;
    vmInfos[vm4.pid] = vm4;
}
TEST(VMMemMigrateStrategyTest, ExecuteNoRemoteMem)
{
    std::unordered_map<pid_t, VMInfo> vmInfos;
    std::vector<RemoteNUMA> remoteNUMAs;
    std::vector<VMResult> vmResults;

    FillParam2(vmInfos);
    // Numa1借来100，满足虚机8&5需求
    RemoteNUMA numa1;
    numa1.remoteNumaId = 1;
    numa1.borrowSize = 0;
    remoteNUMAs.push_back(numa1);

    // Numa2只借来5，小于虚机6需求
    RemoteNUMA numa2;
    numa2.remoteNumaId = 2;
    numa2.borrowSize = 0;
    remoteNUMAs.push_back(numa2);

    // Numa3无可借用内存，小于虚机7需求
    RemoteNUMA numa3;
    numa2.remoteNumaId = 3;
    numa2.borrowSize = 0;
    remoteNUMAs.push_back(numa3);

    // 调用execute函数
    VMMemMigrateStrategy strategy;
    uint32_t result = strategy.Execute(vmInfos, remoteNUMAs, vmResults);
    ASSERT_EQ(vmResults.size(), 0);
}

TEST(VMMemMigrateStrategyTest, ExecuteRebalanceERRWhenCollectBorrowRecords)
{
    // 调用 execute 函数
    MOCKER_CPP(&BorrowRecordHelper::CollectBorrowRecords,
               MpResult (*)(BorrowRecordHelper*, const std::string, std::vector<BorrowRecord>&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));
    VMMemMigrateStrategy strategy;
    std::vector<pid_t> pids;
    auto ret = strategy.Rebalance("1", 0, pids, 100);
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

TEST(MockVMMemMigrateStrategy, TestVMStatWithEmptyInputs)
{
    std::vector<std::pair<pid_t, uint64_t>> demand;
    std::map<pid_t, int16_t> location;
    std::map<pid_t, uint64_t> quotaRef;
    VMStat(demand, location, quotaRef);
}

TEST(MockVMMemMigrateStrategy, FillNumaInfo_Succeed)
{
    JSON_MAP data = {{"MemTotal", "1024"},
                     {"MemFree", "512"},
                     {"HugePages_Total", "10"},
                     {"HugePages_Free", "5"},
                     {"SocketId", "1"}};
    mempooling::outinterface::NumaMetaData info;
    auto ret = FillNumaInfo(info, data);
    ASSERT_EQ(ret, MEM_POOLING_OK);
}

TEST(MockVMMemMigrateStrategy, FillNumaInfo_Test1)
{
    JSON_MAP data = {{"MemTotal", "1024"}, {"MemFree", "512"}, {"HugePages_Total", "10"}, {"HugePages_Free", "5"}};
    mempooling::outinterface::NumaMetaData info;
    auto ret = FillNumaInfo(info, data);
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

TEST(MockVMMemMigrateStrategy, FillNumaInfo_Test2)
{
    JSON_MAP data = {{"MemTotal", "1024"}, {"MemFree", "512"}, {"HugePages_Total", "10"}, {"SocketId", "1"}};
    mempooling::outinterface::NumaMetaData info;
    auto ret = FillNumaInfo(info, data);
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

TEST(MockVMMemMigrateStrategy, FillNumaInfo_Test3)
{
    JSON_MAP data = {{"MemTotal", "1024"}, {"MemFree", "512"}, {"HugePages_Free", "5"}, {"SocketId", "1"}};
    mempooling::outinterface::NumaMetaData info;
    auto ret = FillNumaInfo(info, data);
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

TEST(MockVMMemMigrateStrategy, FillNumaInfo_Test4)
{
    JSON_MAP data = {{"MemTotal", "1024"}, {"HugePages_Total", "10"}, {"HugePages_Free", "5"}, {"SocketId", "1"}};
    mempooling::outinterface::NumaMetaData info;
    auto ret = FillNumaInfo(info, data);
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

TEST(MockVMMemMigrateStrategy, FillNumaInfo_Test5)
{
    JSON_MAP data = {{"MemFree", "512"}, {"HugePages_Total", "10"}, {"HugePages_Free", "5"}, {"SocketId", "1"}};
    mempooling::outinterface::NumaMetaData info;
    auto ret = FillNumaInfo(info, data);
    ASSERT_EQ(ret, MEM_POOLING_ERROR);
}

TEST(MockVMMemMigrateStrategy, CalBestRemainValueTest1)
{
    std::map<uint16_t, uint64_t> resourceCapacity = {{0, 100}, {1, 200}};
    std::map<pid_t, uint64_t> quota = {{10, 80}, {11, 100}};
    std::vector<std::pair<pid_t, uint64_t>> demand = {{10, 50}, {11, 30}};
    std::map<pid_t, int16_t> location = {{10, 0}, {11, 1}};
    VMStat vmStat(demand, location, quota);
    std::map<pid_t, bool> assigned;

    auto ret = CalBestRemainValue(resourceCapacity, vmStat, assigned);
    EXPECT_EQ(ret, 200u);
}

TEST(MockVMMemMigrateStrategy, CalBestRemainValueTest2)
{
    std::map<uint16_t, uint64_t> resourceCapacity = {{0, 50}};
    std::map<pid_t, uint64_t> quota = {{20, 200}};
    std::vector<std::pair<pid_t, uint64_t>> demand = {{20, 10}};
    std::map<pid_t, int16_t> location = {{20, 0}};
    VMStat vmStat(demand, location, quota);
    std::map<pid_t, bool> assigned;

    auto ret = CalBestRemainValue(resourceCapacity, vmStat, assigned);
    EXPECT_EQ(ret, 0u);
}

TEST(MockVMMemMigrateStrategy, CalBestRemainValueTest3)
{
    std::map<uint16_t, uint64_t> resourceCapacity = {{0, 100}};
    std::map<pid_t, uint64_t> quota = {{30, 20}};
    std::vector<std::pair<pid_t, uint64_t>> demand = {{30, 99}};
    std::map<pid_t, int16_t> location = {{30, -1}}; // should skip
    VMStat vmStat(demand, location, quota);
    std::map<pid_t, bool> assigned;

    auto ret = CalBestRemainValue(resourceCapacity, vmStat, assigned);
    EXPECT_EQ(ret, 80u);
}

TEST(MockVMMemMigrateStrategy, CalBestRemainValueTest4)
{
    std::map<uint16_t, uint64_t> resourceCapacity = {{0, 100}};
    std::map<pid_t, uint64_t> quota = {{40, 20}};
    std::vector<std::pair<pid_t, uint64_t>> demand = {{40, 50}};
    std::map<pid_t, int16_t> location = {{40, 0}};
    VMStat vmStat(demand, location, quota);
    std::map<pid_t, bool> assigned = {{40, true}}; // assigned

    auto ret = CalBestRemainValue(resourceCapacity, vmStat, assigned);
    EXPECT_EQ(ret, 100u);
}

TEST(MockVMMemMigrateStrategy, CalBestRemainValueTest5)
{
    std::map<uint16_t, uint64_t> resourceCapacity = {{0, 100}};
    std::map<pid_t, uint64_t> quota = {{50, 60}};
    std::vector<std::pair<pid_t, uint64_t>> demand = {{50, 30}};
    std::map<pid_t, int16_t> location = {{50, 0}};
    VMStat vmStat(demand, location, quota);
    std::map<pid_t, bool> assigned = {{50, true}}; // only skip in quota

    auto ret = CalBestRemainValue(resourceCapacity, vmStat, assigned);
    EXPECT_EQ(ret, 100u);
}

static uint32_t MockRmrsPidNumaInfoCollectReturn(const turbo::rmrs::PidNumaInfoCollectParam& pidNumaInfoCollectParam,
                                                 turbo::rmrs::PidNumaInfoCollectResult& pidNumaInfoCollectResult)
{
    return 0;
}

TEST(MockVMMemMigrateStrategy, UpdateContainerInfoInnode_Success)
{
    std::string srcNid = "0";
    std::vector<pid_t> pids{};
    std::unordered_map<pid_t, VMInfo> vmInfos{};

    MempoolingMessage::rmrsPidNumaInfoCollect = &MockRmrsPidNumaInfoCollectReturn;

    uint32_t ret = UpdateContainerInfoInnode(srcNid, pids, vmInfos);
    EXPECT_EQ(ret, MEM_POOLING_OK);
    GlobalMockObject::verify();
}

TEST(MockVMMemMigrateStrategy, UpdateContainerInfoInnode_Failed)
{
    std::string srcNid = "0";
    std::vector<pid_t> pids{};
    std::unordered_map<pid_t, VMInfo> vmInfos{};

    MOCKER_CPP(&PidNumaInfoCollectRecvHandler, MpResult (*)(UbseByteBuffer, UbseByteBuffer&))
        .stubs()
        .will(returnValue(MEM_POOLING_ERROR));

    uint32_t ret = UpdateContainerInfoInnode(srcNid, pids, vmInfos);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST(MockVMMemMigrateStrategy, containsPid_Found)
{
    std::vector<pid_t> pids{1, 2};
    pid_t pidInPids = 1;
    EXPECT_TRUE(containsPid(pids, pidInPids));
}

TEST(MockVMMemMigrateStrategy, containsPid_NotFound)
{
    std::vector<pid_t> pids{1, 2};
    pid_t pidNotInPids = 3;
    EXPECT_FALSE(containsPid(pids, pidNotInPids));
}

int MockSmapQueryProcessConfigReturnOK(int numaId, struct ProcessPayload* processPayload, int pid, int* retLen)
{
    return MEM_POOLING_OK;
}

int MockSmapQueryProcessConfigReturnERROR(int numaId, struct ProcessPayload* processPayload, int pid, int* retLen)
{
    return MEM_POOLING_ERROR;
}

TEST(MockVMMemMigrateStrategy, CollectProcessInformation_SmapGetterNull)
{
    std::set<uint16_t> remoteNuma{0};
    std::vector<pid_t> pids{0};
    std::map<pid_t, int16_t> currentVmLocation{};
    std::unordered_map<pid_t, VMInfo> vmInfos{};
    uint8_t ratio{};
    VMMemMigrateStrategy strategy;

    MOCKER(&mempooling::smap::SmapModule::GetSmapGetRemoteProcessesFunc)
        .stubs()
        .will(returnValue(static_cast<SmapGetRemotePidsFunc>(nullptr)));

    uint32_t ret = strategy.CollectProcessInformation(remoteNuma, pids, currentVmLocation, vmInfos, ratio);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);

    GlobalMockObject::verify();
}

TEST(MockVMMemMigrateStrategy, CollectProcessInformation_SmapOK)
{
    std::set<uint16_t> remoteNuma{0};
    std::vector<pid_t> pids{0};
    std::map<pid_t, int16_t> currentVmLocation{};
    std::unordered_map<pid_t, VMInfo> vmInfos{};
    uint8_t ratio{};
    VMMemMigrateStrategy strategy;

    MOCKER(&mempooling::smap::SmapModule::GetSmapGetRemoteProcessesFunc)
        .stubs()
        .will(returnValue(static_cast<SmapGetRemotePidsFunc>(&MockSmapQueryProcessConfigReturnOK)));

    uint32_t ret = strategy.CollectProcessInformation(remoteNuma, pids, currentVmLocation, vmInfos, ratio);
    EXPECT_EQ(ret, MEM_POOLING_OK);

    GlobalMockObject::verify();
}

TEST(MockVMMemMigrateStrategy, CollectProcessInformation_SmapError)
{
    std::set<uint16_t> remoteNuma{0};
    std::vector<pid_t> pids{0};
    std::map<pid_t, int16_t> currentVmLocation{};
    std::unordered_map<pid_t, VMInfo> vmInfos{};
    uint8_t ratio{};
    VMMemMigrateStrategy strategy;

    MOCKER(&mempooling::smap::SmapModule::GetSmapGetRemoteProcessesFunc)
        .stubs()
        .will(returnValue(static_cast<SmapGetRemotePidsFunc>(&MockSmapQueryProcessConfigReturnERROR)));

    uint32_t ret = strategy.CollectProcessInformation(remoteNuma, pids, currentVmLocation, vmInfos, ratio);
    EXPECT_EQ(ret, MEM_POOLING_ERROR);
}

TEST(MockVMMemMigrateStrategy, CalculateVmDemandAndQuotaTest)
{
    std::unordered_map<pid_t, VMInfo> vmInfos{};
    uint8_t ratio{};
    std::map<pid_t, uint64_t> vmDemand{};
    std::map<pid_t, uint64_t> vmQuota{};
    CalculateVmDemandAndQuota(vmInfos, ratio, vmDemand, vmQuota);
}
} // namespace mempooling::outinterface
