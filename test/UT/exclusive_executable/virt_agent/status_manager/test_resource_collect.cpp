/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#include "test_resource_collect.h"

#include <ubse_storage.h>
#include <mockcpp/mockcpp.hpp>

#include "global_borrow_map_message.h"
#include "migrate_state_storage.h"
#include "resource_collect.h"
#include "resource_query.h"
#include "vm_configuration.h"
#include "vm_info.h"

using namespace vm;
using namespace ubse::storage;
namespace ubse::vm::ut {
void TestResourceCollect::SetUp()
{
    Test::SetUp();
}

void TestResourceCollect::TearDown()
{
    Test::TearDown();
}

TEST_F(TestResourceCollect, VmResourceCollectInfoHandleEmpty)
{
    std::vector<HostVmDomainInfo> vmDomainInfoCollectList{};
    std::vector<HostNumaCpuInfo> numaInfoCollectList{};
    auto& resource = ResourceCollect::GetInstance();
    EXPECT_EQ(resource.VmResourceCollectInfoHandle(vmDomainInfoCollectList, numaInfoCollectList),
              VM_MASTER_EMPTY_VECTOR_ERROR);
}

TEST_F(TestResourceCollect, VmResourceCollectInfoHandle)
{
    std::vector<HostVmDomainInfo> vmDomainInfoCollectList{};
    std::vector<HostNumaCpuInfo> numaInfoCollectList{};
    HostVmDomainInfo hostVmDomainInfo{};
    HostNumaCpuInfo hostNumaCpuInfo{};
    hostVmDomainInfo.vmDomainInfos.push_back(::vm::VmDomainInfo{});
    hostNumaCpuInfo.numaCpuInfos.push_back(NumaCpuInfo{});
    hostNumaCpuInfo.numaCpuInfos.push_back(NumaCpuInfo{});
    vmDomainInfoCollectList.push_back(hostVmDomainInfo);
    numaInfoCollectList.push_back(hostNumaCpuInfo);
    auto& resource = ResourceCollect::GetInstance();
    EXPECT_EQ(resource.VmResourceCollectInfoHandle(vmDomainInfoCollectList, numaInfoCollectList), VM_OK);
}

TEST_F(TestResourceCollect, InitGlobalBorrowMap_Failed)
{
    MOCKER(UbseStorageQueryData).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(ResourceCollect::InitGlobalBorrowMap(), VM_ERROR);
    MOCKER(UbseStorageQueryData).reset();
}

uint32_t MockUbseStorageQueryData(const std::string& keyPrefix, const std::string& key, void* ctx,
                                  UbseStorageDealDataFunc func)
{
    BorrowIdStatus borrowIdStatus{.borrowId = "123", .presentNumaId = 0};
    std::unordered_map<std::string, BorrowIdStatus> globalBorrowMap{};
    globalBorrowMap.emplace("123", borrowIdStatus);
    uint64_t index = 1;
    GlobalBorrowMapMessage message{globalBorrowMap, index};
    auto ret = message.Serialize();
    if (ret != VM_OK) {
        return VM_ERROR;
    }
    UbseByteBuffer buff{message.SerializedData(), message.SerializedDataSize()};

    ResourceCollect::QueryHandler(keyPrefix, key, buff, ctx);
    return VM_OK;
}
TEST_F(TestResourceCollect, InitGlobalBorrowMap_Success)
{
    MOCKER(UbseStorageQueryData).stubs().will(invoke(MockUbseStorageQueryData));
    EXPECT_EQ(ResourceCollect::InitGlobalBorrowMap(), VM_OK);
    MOCKER(UbseStorageQueryData).reset();
}

TEST_F(TestResourceCollect, GetVMInfo)
{
    GTEST_SKIP();
    VMNodeLocInfo vmNodeLocInfo{.hostName = "adc"};
    std::unordered_map<std::string, VMBasicInfo> result;
    auto& resource = ResourceCollect::GetInstance();
    resource.ClearGlobalNumaInfo();
    result = resource.GetVMInfo(vmNodeLocInfo);
    EXPECT_TRUE(result.empty());
}

TEST_F(TestResourceCollect, UpdateGlobalNumaInfoMapAndGlobalNumaVMInfoMap)
{
    HostVmDomainInfo hostVmDomainInfo{};
    HostNumaCpuInfo hostNumaCpuInfo{};
    MOCKER(ResourceQuery::GetLocalHostVmCollectData).stubs().will(returnValue(VM_ERROR)).then(returnValue(VM_OK));
    EXPECT_EQ(
        ResourceCollect::GetInstance().UpdateGlobalNumaInfoMapAndGlobalNumaVMInfoMap(hostVmDomainInfo, hostNumaCpuInfo),
        VM_ERROR);
    EXPECT_EQ(
        ResourceCollect::GetInstance().UpdateGlobalNumaInfoMapAndGlobalNumaVMInfoMap(hostVmDomainInfo, hostNumaCpuInfo),
        VM_OK);
}

TEST_F(TestResourceCollect, ResourceCollectInitFail)
{
    MOCKER(&ResourceCollect::UpdateGlobalNumaInfoMapAndGlobalNumaVMInfoMap).stubs().will(returnValue(VM_ERROR));
    auto ret = ResourceCollect::Init();
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestResourceCollect, ResourceCollectUpdateVMStatus)
{
    NumaMemInfoMap numaMemInfoMap{};
    std::string uuid{"id"};
    pid_t pid = 123;
    VmMigrateStatus vmMigrateStatus{MIGRATEABLE};
    MOCKER(&VmConfiguration::GetNodeId).stubs().will(returnValue(std::string("1")));
    MOCKER(MigrateStateStorage::SaveMigrateState).stubs().will(returnValue(VM_ERROR));
    MOCKER(MigrateStateStorage::DelMigrateState).stubs().will(returnValue(VM_ERROR));
    auto ret = ResourceCollect::GetInstance().UpdateVMStatus(numaMemInfoMap, uuid, pid, vmMigrateStatus);
    EXPECT_EQ(ret, VM_ERROR);
    vmMigrateStatus = MIGRATING;
    ret = ResourceCollect::GetInstance().UpdateVMStatus(numaMemInfoMap, uuid, pid, vmMigrateStatus);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestResourceCollect, TestSyncGlobalBorrowMap_MemcpyFailed)
{
    std::vector<UbseNumaMemoryImportDebtInfo> debtInfos{};
    debtInfos.push_back({"borrowName1", "1", {}, 1024, {}});
    MOCKER(memcpy_s).stubs().will(returnValue(1));
    auto ret = ResourceCollect::SyncGlobalBorrowMap(debtInfos);
    EXPECT_EQ(ret, VM_ERROR_NOMEM);
    MOCKER(memcpy_s).reset();
}

TEST_F(TestResourceCollect, TestSyncGlobalBorrowMap_InvalidParam)
{
    std::vector<UbseNumaMemoryImportDebtInfo> debtInfos;
    debtInfos.push_back({"borrowName1", "1", {}, 1024, {}});
    auto ret = ResourceCollect::SyncGlobalBorrowMap(debtInfos);
    EXPECT_EQ(ret, VM_INVALID_PARAM_ERROR);
}

TEST_F(TestResourceCollect, TestSyncGlobalBorrowMap_Success)
{
    std::vector<UbseNumaMemoryImportDebtInfo> debtInfos;
    debtInfos.push_back({"borrowName2", "1", {1}, 1024, {}});
    debtInfos.push_back({"borrowName3-rep", "1", {1}, 1024, {}});
    auto ret = ResourceCollect::SyncGlobalBorrowMap(debtInfos);
    EXPECT_EQ(ret, VM_OK);
}
} // namespace ubse::vm::ut
