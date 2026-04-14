/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "test_container_service.h"

#include <mockcpp/GlobalMockObject.h>
#include <mockcpp/mokc.h>
#include "container_service.h"
#include "os_helper.h"
#include "ubse_ut_dir.h"
#include "vm_mem_adapter.h"

using namespace vm;
using namespace vm::overcommit;
namespace ubse::vm::ut {
TestContainerService::TestContainerService() = default;

void TestContainerService::SetUp()
{
    Test::SetUp();
}

void TestContainerService::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestContainerService, MemBorrow)
{
    NodeLocInfo nodeLocInfo{"0", 0, 0};
    std::vector<uint64_t> borrowSizes{100, 200};
    WaterMark waterMark{80, 60};
    UserInfo userInfo{0, "name"};
    MemBorrowExecuteResult borrowResult{};
    MemBorrowExecuteResult mockBborrowResult{{"0"}, {0}};
    ContainerService containerService{};
    MOCKER(&VmMemManager::RunPreflight).stubs().will(returnValue(true));
    MOCKER(&VmMemManager::RunMemBorrow).stubs().with(outBound(mockBborrowResult)).will(returnValue(VM_OK));
    auto ret = containerService.MemBorrow(nodeLocInfo, borrowSizes, waterMark, userInfo, borrowResult);
    EXPECT_EQ(ret, VM_OK);
    EXPECT_EQ(borrowResult.borrowIds, mockBborrowResult.borrowIds);
    EXPECT_EQ(borrowResult.presentNumaIds, mockBborrowResult.presentNumaIds);
}

TEST_F(TestContainerService, MemBorrowFail)
{
    NodeLocInfo nodeLocInfo{};
    std::vector<uint64_t> borrowSizes{};
    WaterMark waterMark{};
    UserInfo userInfo{};
    MemBorrowExecuteResult borrowResult{};
    ContainerService containerService{};
    MOCKER(&VmMemManager::RunPreflight).stubs().will(returnValue(false));
    auto ret = containerService.MemBorrow(nodeLocInfo, borrowSizes, waterMark, userInfo, borrowResult);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestContainerService, MemMigrate)
{
    NodeLocInfo nodeLocInfo{"0", 0, 0};
    std::unordered_set<std::string> borrowIdSet{"0", "1"};
    std::vector<VMPresetParam> vmPresetParamList{{0, 0}, {1, 50}};
    std::unordered_map<std::string, uint16_t> borrowRemoteMaps{{"0", 100}, {"1", 100}};
    ContainerService containerService{};
    MOCKER(VmMemAdapter::GetMemoryRemoteNumaIds)
        .stubs()
        .with(_, outBound(borrowRemoteMaps))
        .will(returnValue(VM_OK));
    MOCKER(&VmMemManager::RunPreflight).stubs().will(returnValue(true));
    MOCKER(&VmMemManager::OutMemMigrate).stubs().will(returnValue(VM_OK));
    auto ret = containerService.MemMigrate(nodeLocInfo, borrowIdSet, vmPresetParamList);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestContainerService, MemMigrateFail)
{
    NodeLocInfo nodeLocInfo{"0", 0, 0};
    std::unordered_set<std::string> borrowIdSet{"0", "1"};
    std::vector<VMPresetParam> vmPresetParamList{{0, 0}, {1, 50}};
    std::unordered_map<std::string, uint16_t> borrowRemoteMaps{{"0", 100}, {"1", 100}};
    ContainerService containerService{};
    MOCKER(VmMemAdapter::GetMemoryRemoteNumaIds)
        .stubs()
        .with(_, outBound(borrowRemoteMaps))
        .will(returnValue(VM_OK));
    MOCKER(&VmMemManager::RunPreflight).stubs().will(returnValue(false));
    auto ret = containerService.MemMigrate(nodeLocInfo, borrowIdSet, vmPresetParamList);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestContainerService, MemReturn)
{
    NodeLocInfo nodeLocInfo{"0", 0, 0};
    std::vector<std::string> borrowIds{"0", "1"};
    std::vector<pid_t> pids{0, 1};
    ContainerService containerService{};
    MOCKER(&VmMemManager::RunPreflight).stubs().will(returnValue(false));
    auto ret = containerService.MemReturn(nodeLocInfo, borrowIds, pids);
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(&VmMemManager::RunPreflight).reset();
    MOCKER(&VmMemManager::RunPreflight).stubs().will(returnValue(true));
    MOCKER(&VmMemManager::OutMemReturn).stubs().will(returnValue(VM_OK));
    ret = containerService.MemReturn(nodeLocInfo, borrowIds, pids);
    EXPECT_EQ(ret, VM_OK);
}
} // namespace ubse::vm::ut