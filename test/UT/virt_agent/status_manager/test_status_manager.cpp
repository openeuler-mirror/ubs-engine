/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "test_status_manager.h"

#include <mockcpp/mokc.h>

#include "status_manager.h"
#include "vm_configuration.h"
#include "vm_task_counter.h"
#include "resource_collect.h"

using namespace vm;
using namespace vm::mempooling;

namespace ubse::ut::vm {
int callCount = 0;
// 设置测试环境
void TestStatusManager::SetUp()
{
    Test::SetUp();
}

// 拆卸测试环境
void TestStatusManager::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

UBSRMRSMemBorrowFunc MockUBSRMRSMemBorrow()
{
    return [](const SrcMemoryBorrowParam &srcParam, const std::vector<uint64_t> &borrowSizes,
              const WaterMark &waterMark, MemBorrowExecuteResult &result) {
        result = {{"testBorrowId"}, {1}};
        return VM_OK;
    };
}

UBSRMRSMemMigrateFunc MockUBSRMRSMemMigrate()
{
    return [](const SrcMemoryBorrowParam &, const std::vector<VMPresetParam> &, const MemBorrowExecuteResult &) {
        return VM_OK;
    };
}

UBSRMRSMemReturnFunc MockUBSRMRSMemReturn()
{
    return [](const SrcMemoryBorrowParam &srcParam, const std::vector<std::string> &borrowIds,
              const std::vector<pid_t> &pids) {
                  callCount++;
                  return VM_OK;
              };
}

TEST_F(TestStatusManager, LoadGlobalBorrowMapSuccess)
{
    MOCKER(&ResourceCollect::InitGlobalBorrowMap).stubs().will(returnValue(VM_OK));
    GlobalBorrowMap globalBorrowMap_ = {
        {"id1", {"id1", 4, MemMigrateStatus::READY_TO_MIGRATE, {"compute01", "2", 2, 2}}}, {"id2", {}}};
    MOCKER(MempoolingModule::UBSRMRSMemReturn).stubs().will(invoke(MockUBSRMRSMemReturn));
    StatusManager::LoadGlobalBorrowMap();
    MOCKER(&ResourceCollect::InitGlobalBorrowMap).reset();
    MOCKER(MempoolingModule::UBSRMRSMemReturn).reset();
}

TEST_F(TestStatusManager, LoadGlobalBorrowMapExcludeReady)
{
    MOCKER(&ResourceCollect::InitGlobalBorrowMap).stubs().will(returnValue(VM_OK));
    GlobalBorrowMap globalBorrowMap_ = {
        {"id1", {"id1", 4, {MemMigrateStatus::READY_TO_MIGRATE}, {"compute01", "2", 2, 2}}},
        {"id2", {"id2", 4, {MemMigrateStatus::MIGRATE_SUCCESS}, {"compute01", "2", 2, 2}}}};
    ResourceCollect::globalBorrowMap_ = globalBorrowMap_;
    callCount = 0;
    MOCKER(MempoolingModule::UBSRMRSMemReturn).stubs().will(invoke(MockUBSRMRSMemReturn));
    StatusManager::LoadGlobalBorrowMap();
    EXPECT_EQ(1, callCount);
    MOCKER(&ResourceCollect::InitGlobalBorrowMap).reset();
    MOCKER(MempoolingModule::UBSRMRSMemReturn).reset();
    globalBorrowMap_.clear();
    ResourceCollect::globalBorrowMap_ = globalBorrowMap_;
}

TEST_F(TestStatusManager, LoadGlobalBorrowMapFailed)
{
    GlobalBorrowMap globalBorrowMap_ = {
        {"id1", {"id1", 4, MemMigrateStatus::READY_TO_MIGRATE, {"compute01", "2", 2, 2}}}, {"id2", {}}};
    // 初始化InitGlobalBorrowMap失败
    MOCKER(&ResourceCollect::InitGlobalBorrowMap).stubs().will(returnValue(VM_ERROR));
    StatusManager::LoadGlobalBorrowMap();
    MOCKER(&ResourceCollect::InitGlobalBorrowMap).reset();

    // 内存归还失败
    MOCKER(&ResourceCollect::InitGlobalBorrowMap).stubs().will(returnValue(VM_OK));
    MOCKER(MempoolingModule::UBSRMRSMemReturn).stubs().will(returnValue(nullptr));
    StatusManager::LoadGlobalBorrowMap();
    MOCKER(&ResourceCollect::InitGlobalBorrowMap).reset();
    MOCKER(MempoolingModule::UBSRMRSMemReturn).reset();
}

TEST_F(TestStatusManager, EscapeStrategyHandleBorrow)
{
    VMNodeLocInfo nodeLoc = {"host1", "host-id-1", 0, 0};
    EscapeAction escapeAction;
    escapeAction.actionType = EscapeActionType::BORROW;
    escapeAction.curNodeLoc = nodeLoc;
    escapeAction.borrowSizes = {1024, 2048};

    StatusManager::GetInstance().EscapeStrategyHandle(escapeAction);
}

TEST_F(TestStatusManager, TestEscapeStrategyHandleReturn)
{
    VMNodeLocInfo nodeLoc = {"host1", "host-id-1", 0, 0};
    EscapeAction escapeAction;
    escapeAction.actionType = EscapeActionType::RETURN;
    escapeAction.curNodeLoc = nodeLoc;
    escapeAction.returnMemNames = {"123", "456"};
    escapeAction.borrowSizes = {1024, 2048};

    MOCKER(&StatusManager::MemoryReturnOperation).stubs();
    StatusManager::GetInstance().EscapeStrategyHandle(escapeAction);
    MOCKER(&StatusManager::MemoryReturnOperation).reset();
    // escapeAction.returnMemNames为空
    escapeAction.returnMemNames = {};
    MOCKER(&StatusManager::MemoryReturnOperation).stubs();
    StatusManager::GetInstance().EscapeStrategyHandle(escapeAction);
    MOCKER(&StatusManager::MemoryReturnOperation).reset();
}

TEST_F(TestStatusManager, TestEscapeStrategyHandleNope)
{
    VMNodeLocInfo nodeLoc = {"host1", "host-id-1", 0, 0};
    EscapeAction escapeAction;
    escapeAction.actionType = EscapeActionType::NOPE;
    escapeAction.curNodeLoc = nodeLoc;
    escapeAction.returnMemNames = {"123", "456"};
    escapeAction.borrowSizes = {1024, 2048};

    StatusManager::GetInstance().EscapeStrategyHandle(escapeAction);
}

TEST_F(TestStatusManager, TestEscapeStrategyHandleDefault)
{
    VMNodeLocInfo nodeLoc = {"host1", "host-id-1", 0, 0};
    EscapeAction escapeAction;
    escapeAction.curNodeLoc = nodeLoc;
    escapeAction.returnMemNames = {"123", "456"};
    escapeAction.borrowSizes = {1024, 2048};

    StatusManager::GetInstance().EscapeStrategyHandle(escapeAction);
}

TEST_F(TestStatusManager, TestMemoryBorrowOperationBorrowFail)
{
    VMNodeLocInfo originNode = {"host1", "host-id-1", 0, 0};
    std::vector<pid_t> pids = {1, 2, 3};
    std::vector<uint64_t> borrowSizes = {1024, 2048};
    VmConfiguration::GetInstance().SetDefaultWaterConf();

    MOCKER(MempoolingModule::UBSRMRSMemBorrow).stubs().will(returnValue(static_cast<UBSRMRSMemBorrowFunc>(nullptr)));
    MOCKER(StatusManager::CleanEmptyBorrowRes).expects(never());
    MOCKER(StatusManager::GenerateBorrowIdStatuses).expects(never());
    StatusManager::GetInstance().MemoryBorrowOperation(originNode, pids, borrowSizes);
    MOCKER(MempoolingModule::UBSRMRSMemBorrow).reset();
}

TEST_F(TestStatusManager, TestMemoryBorrowOperationBorrowSuccess)
{
    VMNodeLocInfo originNode = {"host1", "host-id-1", 0, 0};
    std::vector<pid_t> pids = {1, 2, 3};
    std::vector<uint64_t> borrowSizes = {1024, 2048};
    VmConfiguration::GetInstance().SetDefaultWaterConf();

    MOCKER(MempoolingModule::UBSRMRSMemBorrow).stubs().will(invoke(MockUBSRMRSMemBorrow));
    MOCKER(MempoolingModule::UBSRMRSMemMigrate).stubs().will(invoke(MockUBSRMRSMemMigrate));
    MOCKER(StatusManager::MigrateSuccessBorrowId).expects(once());
    StatusManager::GetInstance().MemoryBorrowOperation(originNode, pids, borrowSizes);
    MOCKER(MempoolingModule::UBSRMRSMemBorrow).reset();
    MOCKER(MempoolingModule::UBSRMRSMemMigrate).reset();
}

TEST_F(TestStatusManager, TestMemoryBorrowOperationMigrateFail)
{
    VMNodeLocInfo originNode = {"host1", "host-id-1", 0, 0};
    std::vector<pid_t> pids = {1, 2, 3};
    std::vector<uint64_t> borrowSizes = {1024, 2048};
    VmConfiguration::GetInstance().SetDefaultWaterConf();

    MOCKER(MempoolingModule::UBSRMRSMemBorrow).stubs().will(invoke(MockUBSRMRSMemBorrow));
    MOCKER(MempoolingModule::UBSRMRSMemMigrate).stubs().will(returnValue(static_cast<UBSRMRSMemMigrateFunc>(nullptr)));
    MOCKER(StatusManager::MigrateSuccessBorrowId).expects(never());
    StatusManager::GetInstance().MemoryBorrowOperation(originNode, pids, borrowSizes);
    MOCKER(MempoolingModule::UBSRMRSMemBorrow).reset();
    MOCKER(MempoolingModule::UBSRMRSMemMigrate).reset();
}

TEST_F(TestStatusManager, TestMemoryReturnOperationSuccess)
{
    VMNodeLocInfo nodeLoc = {"host1", "host-id-1", 0, 0};
    EscapeAction escapeAction;
    escapeAction.curNodeLoc = nodeLoc;
    escapeAction.returnMemNames = {"123", "456"};
    std::vector<pid_t> pids = {1, 2, 3};

    MOCKER(MempoolingModule::UBSRMRSMemReturn).stubs().will(invoke(MockUBSRMRSMemReturn));
    MOCKER(&ResourceCollect::GetPidsOnNuma).stubs().will(returnValue(pids));
    MOCKER(ResourceCollect::GetInstance().DeleteGlobalBorrowMap).expects(once());
    StatusManager::GetInstance().MemoryReturnOperation(escapeAction);
    MOCKER(&ResourceCollect::GetPidsOnNuma).reset();
    MOCKER(MempoolingModule::UBSRMRSMemReturn).reset();
}

TEST_F(TestStatusManager, TestMemoryReturnOperationFailed)
{
    VMNodeLocInfo nodeLoc = {"host1", "host-id-1", 0, 0};
    EscapeAction escapeAction;
    escapeAction.curNodeLoc = nodeLoc;
    escapeAction.returnMemNames = {"123", "456"};
    std::vector<pid_t> pids = {1, 2, 3};

    MOCKER(MempoolingModule::UBSRMRSMemReturn).stubs().will(returnValue(static_cast<UBSRMRSMemReturnFunc>(nullptr)));
    MOCKER(&ResourceCollect::GetPidsOnNuma).stubs().will(returnValue(pids));
    MOCKER(ResourceCollect::GetInstance().DeleteGlobalBorrowMap).expects(never());
    MOCKER(VmTaskCounter::CompleteTask).expects(never());
    StatusManager::GetInstance().MemoryReturnOperation(escapeAction);
    MOCKER(&ResourceCollect::GetPidsOnNuma).reset();
    MOCKER(MempoolingModule::UBSRMRSMemReturn).reset();
}

TEST_F(TestStatusManager, BorrowFilterSetTest)
{
    VMNodeLocInfo nodeInfo = {"compute01", "host1", 0, 1};
    StatusManager::AddTaskFilterSet(nodeInfo);
    EXPECT_TRUE(StatusManager::StillInTask(nodeInfo.hostId, nodeInfo.socketId, nodeInfo.numaId));
    StatusManager::RemoveTaskFilterSet(nodeInfo);
    EXPECT_FALSE(StatusManager::StillInTask(nodeInfo.hostId, nodeInfo.socketId, nodeInfo.numaId));
}

TEST_F(TestStatusManager, MigrateByBorrowIdStatusSuccess)
{
    const SrcMemoryBorrowParam srcMemoryBorrowParam{
        .srcNid = "host1",
        .srcSocketId = 0,
        .srcNumaId = 1,
    };
    const VMNodeLocInfo nodeLocInfo{
        .hostName = "host1",
        .hostId = "host1",
        .socketId = 0,
        .numaId = 1,
    };
    const BorrowIdStatus borrowIdStatus{
        .borrowId = "host1",
        .presentNumaId = 4,
        .memMigrateStatus = MemMigrateStatus::READY_TO_MIGRATE,
        .nodeLocInfo = nodeLocInfo,
    };
    std::vector BorrowIdStatuses{borrowIdStatus};
    MOCKER_CPP(&ResourceCollect::GetPidsOnNuma,
               std::vector<pid_t>(ResourceCollect::*)(const VMNodeLocInfo &, const std::string &))
        .stubs()
        .will(returnValue(std::vector{1}));
    MOCKER(MempoolingModule::UBSRMRSMemMigrate).stubs().will(invoke(MockUBSRMRSMemMigrate));
    const auto ret = StatusManager::MigrateByBorrowIdStatus(srcMemoryBorrowParam, BorrowIdStatuses);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestStatusManager, MigrateByBorrowIdStatusFail1)
{
    const SrcMemoryBorrowParam srcMemoryBorrowParam{
        .srcNid = "host1",
        .srcSocketId = 0,
        .srcNumaId = 1,
    };
    const VMNodeLocInfo nodeLocInfo{.hostName = "host1", .hostId = "host1", .socketId = 0, .numaId = 1};
    const BorrowIdStatus borrowIdStatus{.borrowId = "host1",
                                        .presentNumaId = 4,
                                        .memMigrateStatus = MemMigrateStatus::READY_TO_MIGRATE,
                                        .nodeLocInfo = nodeLocInfo};
    std::vector BorrowIdStatuses{borrowIdStatus};
    MOCKER_CPP(&ResourceCollect::GetPidsOnNuma,
               std::vector<pid_t>(ResourceCollect::*)(const VMNodeLocInfo &, const std::string &))
        .stubs()
        .will(returnValue(std::vector<pid_t>{}));
    const auto ret = StatusManager::MigrateByBorrowIdStatus(srcMemoryBorrowParam, BorrowIdStatuses);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestStatusManager, MigrateByBorrowIdStatusFail2)
{
    const SrcMemoryBorrowParam srcMemoryBorrowParam{
        .srcNid = "host1",
        .srcSocketId = 0,
        .srcNumaId = 1,
    };
    const VMNodeLocInfo nodeLocInfo{.hostName = "host1", .hostId = "host1", .socketId = 0, .numaId = 1};
    const BorrowIdStatus borrowIdStatus{.borrowId = "host1",
                                        .presentNumaId = 4,
                                        .memMigrateStatus = MemMigrateStatus::READY_TO_MIGRATE,
                                        .nodeLocInfo = nodeLocInfo};
    std::vector BorrowIdStatuses{borrowIdStatus};
    MOCKER_CPP(&ResourceCollect::GetPidsOnNuma,
               std::vector<pid_t>(ResourceCollect::*)(const VMNodeLocInfo &, const std::string &))
        .stubs()
        .will(returnValue(std::vector{1}));
    MOCKER(MempoolingModule::UBSRMRSMemMigrate).stubs().will(returnValue(static_cast<UBSRMRSMemMigrateFunc>(nullptr)));
    const auto ret = StatusManager::MigrateByBorrowIdStatus(srcMemoryBorrowParam, BorrowIdStatuses);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestStatusManager, ReturnByBorrowIdStatusSuccess)
{
    const SrcMemoryBorrowParam srcMemoryBorrowParam{
        .srcNid = "host1",
        .srcSocketId = 0,
        .srcNumaId = 1,
    };
    const VMNodeLocInfo nodeLocInfo{.hostName = "host1", .hostId = "host1", .socketId = 0, .numaId = 1};
    const BorrowIdStatus borrowIdStatus{.borrowId = "host1",
                                        .presentNumaId = 4,
                                        .memMigrateStatus = MemMigrateStatus::READY_TO_MIGRATE,
                                        .nodeLocInfo = nodeLocInfo};
    std::vector BorrowIdStatuses{borrowIdStatus};
    MOCKER_CPP(&ResourceCollect::GetPidsOnNuma,
               std::vector<pid_t>(ResourceCollect::*)(const VMNodeLocInfo &, const std::string &))
        .stubs()
        .will(returnValue(std::vector{1}));
    MOCKER(MempoolingModule::UBSRMRSMemReturn).stubs().will(invoke(MockUBSRMRSMemReturn));
    const auto ret = StatusManager::ReturnByBorrowIdStatus(srcMemoryBorrowParam, BorrowIdStatuses);
    EXPECT_EQ(ret, VM_OK);
}

TEST_F(TestStatusManager, ReturnByBorrowIdStatusFail1)
{
    const SrcMemoryBorrowParam srcMemoryBorrowParam{
        .srcNid = "host1",
        .srcSocketId = 0,
        .srcNumaId = 1,
    };
    const VMNodeLocInfo nodeLocInfo{.hostName = "host1", .hostId = "host1", .socketId = 0, .numaId = 1};
    const BorrowIdStatus borrowIdStatus{.borrowId = "host1",
                                        .presentNumaId = 4,
                                        .memMigrateStatus = MemMigrateStatus::READY_TO_MIGRATE,
                                        .nodeLocInfo = nodeLocInfo};
    std::vector BorrowIdStatuses{borrowIdStatus};
    MOCKER_CPP(&ResourceCollect::GetPidsOnNuma,
               std::vector<pid_t>(ResourceCollect::*)(const VMNodeLocInfo &, const std::string &))
        .stubs()
        .will(returnValue(std::vector<pid_t>{}));
    const auto ret = StatusManager::ReturnByBorrowIdStatus(srcMemoryBorrowParam, BorrowIdStatuses);
    EXPECT_EQ(ret, VM_ERROR);
}

TEST_F(TestStatusManager, ReturnByBorrowIdStatusFail2)
{
    const SrcMemoryBorrowParam srcMemoryBorrowParam{
        .srcNid = "host1",
        .srcSocketId = 0,
        .srcNumaId = 1,
    };
    const VMNodeLocInfo nodeLocInfo{.hostName = "host1", .hostId = "host1", .socketId = 0, .numaId = 1};
    const BorrowIdStatus borrowIdStatus{.borrowId = "host1",
                                        .presentNumaId = 4,
                                        .memMigrateStatus = MemMigrateStatus::READY_TO_MIGRATE,
                                        .nodeLocInfo = nodeLocInfo};
    std::vector BorrowIdStatuses{borrowIdStatus};
    MOCKER_CPP(&ResourceCollect::GetPidsOnNuma,
               std::vector<pid_t>(ResourceCollect::*)(const VMNodeLocInfo &, const std::string &))
        .stubs()
        .will(returnValue(std::vector{1}));
    MOCKER(MempoolingModule::UBSRMRSMemReturn).stubs().will(returnValue(static_cast<UBSRMRSMemReturnFunc>(nullptr)));
    const auto ret = StatusManager::ReturnByBorrowIdStatus(srcMemoryBorrowParam, BorrowIdStatuses);
    EXPECT_EQ(ret, VM_ERROR);
}
} // namespace ubse::ut::vm