// Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
#include "test_alarm_handler.h"

#include "alarm_handler.h"
#include "ubse_error.h"
#include "ubse_event.h"
#include "mem_handler.h"
#include "escape_algorithm_helper.h"
#include "status_manager.h"
#include "resource_collect.h"

using namespace vm;
namespace ubse::vm::ut {
TestAlarmHandler::TestAlarmHandler() = default;

void TestAlarmHandler::SetUp()
{
    Test::SetUp();
}

void TestAlarmHandler::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestAlarmHandler, InitSuccess)
{
    MOCKER(&ubse::event::UbseSubEvent).stubs().will(returnValue(VM_OK));
    VmResult ret = AlarmHandler::GetInstance().Init();
    EXPECT_EQ(ret, VM_OK);
    MOCKER(&ubse::event::UbseSubEvent).reset();

    MOCKER(&ubse::event::UbseSubEvent).stubs().will(returnValue(VM_ERROR));
    ret = AlarmHandler::GetInstance().Init();
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(&ubse::event::UbseSubEvent).reset();
}


std::string eventMessage =
    "{\"allNumaInfo\": [{\"mMemTotal\": 127257280512,\"mMemUsed\": 127257280512,\"mMemFree\": 0,\"mMemBorrowed\": "
    "12884901888,\"mMemLent\": 0,\"mMemShared\": 0,\"numaLoc\": \"Node0/0/0\"},{\"mMemTotal\": "
    "10181672960,\"mMemUsed\": 10181672960,\"mMemFree\": 0,\"mMemBorrowed\": 0,\"mMemLent\": 0,\"mMemShared\": "
    "0,\"numaLoc\": \"Node0/0/1\"},{\"mMemTotal\": 0,\"mMemUsed\": 0,\"mMemFree\": 0,\"mMemBorrowed\": "
    "0,\"mMemLent\": 0,\"mMemShared\": 0,\"numaLoc\": \"Node0/1/2\"},{\"mMemTotal\": 0,\"mMemUsed\": "
    "0,\"mMemFree\": 0,\"mMemBorrowed\": 0,\"mMemLent\": 0,\"mMemShared\": 0,\"numaLoc\": "
    "\"Node0/1/3\"},{\"mMemTotal\": 0,\"mMemUsed\": 0,\"mMemFree\": 0,\"mMemBorrowed\": 0,\"mMemLent\": "
    "12884901888,\"mMemShared\": 0,\"numaLoc\": \"Node1/0/0\"},{\"mMemTotal\": 0,\"mMemUsed\": 0,\"mMemFree\": "
    "0,\"mMemBorrowed\": 0,\"mMemLent\": 0,\"mMemShared\": 0,\"numaLoc\": \"Node1/0/1\"},{\"mMemTotal\": "
    "0,\"mMemUsed\": 0,\"mMemFree\": 0,\"mMemBorrowed\": 0,\"mMemLent\": 0,\"mMemShared\": 0,\"numaLoc\": "
    "\"Node1/1/2\"},{\"mMemTotal\": 0,\"mMemUsed\": 0,\"mMemFree\": 0,\"mMemBorrowed\": 0,\"mMemLent\": "
    "0,\"mMemShared\": 0,\"numaLoc\": \"Node1/1/3\"}], \"borrowItem\": [], \"notifyNumaLoc\": "
    "\"Node0/0/1\",\"oomEventFlag\": 1}";

TEST_F(TestAlarmHandler, MemNotifyEventHandlerMemNotify)
{
    MOCKER(&AlarmHandler::AlarmEventHandler).stubs().will(returnValue(VM_OK));

    std::string eventId = UBSE_MEM_BORROW_EVENT_ID;
    VmResult ret = AlarmHandler::MemNotifyEventHandler(eventId, eventMessage);
    MOCKER(&MemHandler::TransNotify).stubs().will(returnValue(VM_ERROR));
    EXPECT_EQ(ret, VM_ERROR);

    eventId = UBSE_MEM_RETURN_EVENT_ID;
    ret = AlarmHandler::MemNotifyEventHandler(eventId, eventMessage);
    EXPECT_EQ(ret, VM_ERROR);

    eventId = UBSE_MEM_CLEAR_EVENT_ID;
    ret = AlarmHandler::MemNotifyEventHandler(eventId, eventMessage);
    EXPECT_EQ(ret, VM_ERROR);

    eventId = "other";
    ret = AlarmHandler::MemNotifyEventHandler(eventId, eventMessage);
    EXPECT_EQ(ret, VM_ERROR_INVAL);
    MOCKER(&MemHandler::TransNotify).reset();

    MOCKER(&MemHandler::TransNotify).stubs().will(returnValue(VM_OK));
    eventId = UBSE_MEM_BORROW_EVENT_ID;
    MOCKER(&StatusManager::StillInTask).stubs().will(returnValue(true));
    ret = AlarmHandler::MemNotifyEventHandler(eventId, eventMessage);
    EXPECT_EQ(ret, VM_OK);
    MOCKER(&StatusManager::StillInTask).reset();

    MOCKER(&StatusManager::StillInTask).stubs().will(returnValue(false));
    eventId = UBSE_MEM_BORROW_EVENT_ID;
    MOCKER(AlarmHandler::GetVirtDebtInfos).stubs().will(returnValue(VM_ERROR));
    ret = AlarmHandler::MemNotifyEventHandler(eventId, eventMessage);
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(AlarmHandler::GetVirtDebtInfos).reset();

    MOCKER(AlarmHandler::GetVirtDebtInfos).stubs().will(returnValue(VM_OK));
    MOCKER(AlarmHandler::GenAlarmNumaInfo).stubs().will(returnValue(VM_ERROR));
    ret = AlarmHandler::MemNotifyEventHandler(eventId, eventMessage);
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(AlarmHandler::GenAlarmNumaInfo).reset();

    eventId = UBSE_MEM_BORROW_EVENT_ID;
    MOCKER(AlarmHandler::GenAlarmNumaInfo).stubs().will(returnValue(VM_OK));
    ret = AlarmHandler::MemNotifyEventHandler(eventId, eventMessage);
    EXPECT_EQ(ret, VM_OK);
    MOCKER(AlarmHandler::GenAlarmNumaInfo).reset();
}

UbseResult MockUbseGetNodeNumaInfoByNodeId(const std::string &nodeId,
                                           std::vector<UbseNodeNumaInfo> &numaNodeInfoList)
{
    numaNodeInfoList.push_back(UbseNodeNumaInfo{});
    return UBSE_OK;
}

TEST_F(TestAlarmHandler, GenAlarmNumaInfo)
{
    Notify notify{
        .nodeId = "node",
        .numaId = 1
    };
    std::vector<UbsVirtNumaMemoryDebtInfo> debtInfos{};
    AlarmNumaInfo alarmNumaInfo{};
    UbsVirtNumaMemoryDebtInfo debtInfo{};
    debtInfos.push_back(debtInfo);
    MOCKER(UbseGetNodeNumaInfoByNodeId).stubs().will(invoke(MockUbseGetNodeNumaInfoByNodeId));
    EXPECT_EQ(AlarmHandler::GenAlarmNumaInfo(notify, debtInfos, alarmNumaInfo), VM_OK);
    MOCKER(UbseGetNodeNumaInfoByNodeId).reset();
}

TEST_F(TestAlarmHandler, ConvertUbseDebtInfosToVirtDebtInfos)
{
    std::vector<UbseNumaMemoryImportDebtInfo> debtInfos{};
    std::vector<UbsVirtNumaMemoryDebtInfo> virtDebtInfos{};
    debtInfos.push_back(UbseNumaMemoryImportDebtInfo{});
    EXPECT_EQ(AlarmHandler::ConvertUbseDebtInfosToVirtDebtInfos(debtInfos, virtDebtInfos), VM_OK);
}

TEST_F(TestAlarmHandler, GetVirtDebtInfos)
{
    std::vector<UbsVirtNumaMemoryDebtInfo> virtDebtInfos{};
    MOCKER(UbseGetNumaMemImportDebtInfoWithLocalNode)
        .stubs()
        .will(returnValue(UBSE_ERR_AUTH_FAILED))
        .then(returnValue(UBSE_OK));
    EXPECT_EQ(AlarmHandler::GetVirtDebtInfos(virtDebtInfos), VM_ERROR);
    MOCKER(AlarmHandler::ConvertUbseDebtInfosToVirtDebtInfos)
        .stubs()
        .will(returnValue(VM_ERROR))
        .then(returnValue(VM_OK));
    EXPECT_EQ(AlarmHandler::GetVirtDebtInfos(virtDebtInfos), VM_ERROR);
    EXPECT_EQ(AlarmHandler::GetVirtDebtInfos(virtDebtInfos), VM_OK);
}

TEST_F(TestAlarmHandler, TestBorrowClearEventHandler)
{
    VMNodeLocInfo tempNodeLoc = VMNodeLocInfo{
        .hostName = "host1",
        .hostId = "host1",
        .socketId = 1,
        .numaId = 1,
    };
    struct AlarmNumaInfo alarmNumaInfoToEscape = AlarmNumaInfo{
        .isMemReturn = true,
        .numaLoc = tempNodeLoc,
    };
    BorrowItemInfo borrowItemInfo;
    alarmNumaInfoToEscape.borrowItemInfo = {borrowItemInfo};
    auto ret = AlarmHandler::BorrowClearEventHandler(alarmNumaInfoToEscape);
    EXPECT_EQ(ret, VM_INVALID_PARAM_ERROR);
    NodeLocInfo nodeLocInfo = {
        .hostId = "host1",
        .socketId = 1,
        .numaId = 3,
    };
    BorrowItem borrowItem1 = BorrowItem{
        .exportLocNum = 1,
        .exportLocInfo = {nodeLocInfo},
        .exportMemId = 1,
        .importMemId = 2,
        .name = "host1",
    };
    borrowItemInfo = BorrowItemInfo{};
    borrowItemInfo.borrowItem.emplace_back(borrowItem1);
    alarmNumaInfoToEscape.borrowItemInfo = {borrowItemInfo};
    ret = AlarmHandler::BorrowClearEventHandler(alarmNumaInfoToEscape);
    EXPECT_EQ(ret, VM_OK);
}

int MyEscapeAlgorithmOK(const StrategyConfig &conf, AlarmNumaInfo &alarm, GlobalNumaInfoMap &globalMap,
                        EscapeAction &action)
{
    return 0;
}

EscapeAlgorithmFunc MockGetStrategyAlgorithmReturnOK()
{
    return MyEscapeAlgorithmOK;
}

int MyEscapeAlgorithmWarn(const StrategyConfig &conf, AlarmNumaInfo &alarm, GlobalNumaInfoMap &globalMap,
                          EscapeAction &action)
{
    return 1;
}

EscapeAlgorithmFunc MockGetStrategyAlgorithmReturnWarn()
{
    return MyEscapeAlgorithmWarn;
}

EscapeAlgorithmFunc MockGetStrategyAlgorithmReturnNull()
{
    return nullptr;
}

VMNodeLocInfo expectedNodeLocInfo = { "node0", "testHostId", {0}, {0}}; // 告警节点位置
NodeLocInfo curNodeLoc = NodeLocInfo{
    .hostId = expectedNodeLocInfo.hostId,
    .socketId = expectedNodeLocInfo.socketId,
    .numaId = expectedNodeLocInfo.numaId,
};
NodeLocInfo nodeLocInfo = {
    .hostId = "host1",
    .socketId = 1,
    .numaId = 3,
};


GlobalBorrowMap MockGetGlobalBorrowMap()
{
    const VMNodeLocInfo nodeLocInfo{
        .hostName = "host1",
        .hostId = expectedNodeLocInfo.hostId,
        .socketId = expectedNodeLocInfo.socketId,
        .numaId = expectedNodeLocInfo.numaId,
    };
    const BorrowIdStatus borrowIdStatus{
        .borrowId = "host1",
        .presentNumaId = 4,
        .memMigrateStatus = MemMigrateStatus::READY_TO_MIGRATE,
        .nodeLocInfo = nodeLocInfo,
    };
    return GlobalBorrowMap{{"host1", borrowIdStatus}};
}

GlobalBorrowMap MockGetGlobalBorrowMapError()
{
    const VMNodeLocInfo nodeLocInfo{
        .hostName = "host1",
        .hostId = expectedNodeLocInfo.hostId,
        .socketId = expectedNodeLocInfo.socketId,
        .numaId = expectedNodeLocInfo.numaId,
    };
    const BorrowIdStatus borrowIdStatus{
        .borrowId = "host1",
        .presentNumaId = 4,
        .memMigrateStatus = MemMigrateStatus::MIGRATE_SUCCESS,
        .nodeLocInfo = nodeLocInfo,
    };
    return GlobalBorrowMap{{"host1", borrowIdStatus}};
}

TEST_F(TestAlarmHandler, HandlerNoUsedBorrowIdsTrue1)
{
    const BorrowItem borrowItem{
        .exportLocNum = 1,
        .exportLocInfo = {nodeLocInfo},
        .exportMemId = 1,
        .importMemId = 2,
        .name = "host1",
    };
    const BorrowItemInfo borrowItemInfo{.borrowItem = std::vector{borrowItem}};
    const LendItemInfo lendItemInfo{
        .lendItemNum = 0,
        .lendItem = {},
    };
    AlarmNumaInfo alarmNumaInfo{};
    alarmNumaInfo.borrowItemInfo = borrowItemInfo;
    alarmNumaInfo.lendItemInfo = lendItemInfo;
    WatermarkWarningType type = WatermarkWarningType::CLEAR_BORROW;
    const auto ret = AlarmHandler::HandlerNoUsedBorrowIds(alarmNumaInfo, type);
    EXPECT_TRUE(ret);
}

TEST_F(TestAlarmHandler, HandlerNoUsedBorrowIdsTrue2)
{
    const BorrowItem borrowItem{
        .exportLocNum = 1,
        .exportLocInfo = {nodeLocInfo},
        .exportMemId = 1,
        .importMemId = 2,
        .name = "host1",
    };
    const BorrowItemInfo borrowItemInfo{.borrowItem = std::vector{borrowItem}};
    const LendItemInfo lendItemInfo{
        .lendItemNum = 0,
        .lendItem = {},
    };
    AlarmNumaInfo alarmNumaInfo{};
    alarmNumaInfo.borrowItemInfo = borrowItemInfo;
    alarmNumaInfo.lendItemInfo = lendItemInfo;
    WatermarkWarningType type = WatermarkWarningType::LOW_WATERMARK;
    MOCKER_CPP(&ResourceCollect::GetGlobalBorrowMap, GlobalBorrowMap(ResourceCollect::*)())
        .stubs()
        .will(invoke(MockGetGlobalBorrowMapError));

    const auto ret = AlarmHandler::HandlerNoUsedBorrowIds(alarmNumaInfo, type);
    EXPECT_TRUE(ret);
    MOCKER_CPP(&ResourceCollect::GetGlobalBorrowMap, GlobalBorrowMap(ResourceCollect::*)()).reset();
}

TEST_F(TestAlarmHandler, HandlerNoUsedBorrowIdsTrue3)
{
    const BorrowItem borrowItem{
        .exportLocNum = 1,
        .exportLocInfo = {nodeLocInfo},
        .exportMemId = 1,
        .importMemId = 2,
        .name = "host1",
    };
    const BorrowItemInfo borrowItemInfo{.borrowItem = std::vector{borrowItem}};
    const LendItemInfo lendItemInfo{
        .lendItemNum = 0,
        .lendItem = {},
    };
    AlarmNumaInfo alarmNumaInfo{};
    alarmNumaInfo.borrowItemInfo = borrowItemInfo;
    alarmNumaInfo.lendItemInfo = lendItemInfo;
    WatermarkWarningType type = WatermarkWarningType::LOW_WATERMARK;
    MOCKER_CPP(&ResourceCollect::GetGlobalBorrowMap, GlobalBorrowMap(ResourceCollect::*)())
        .stubs()
        .will(invoke(MockGetGlobalBorrowMap));
    const auto ret = AlarmHandler::HandlerNoUsedBorrowIds(alarmNumaInfo, type);
    EXPECT_TRUE(ret);
    MOCKER_CPP(&ResourceCollect::GetGlobalBorrowMap, GlobalBorrowMap(ResourceCollect::*)()).reset();
}

TEST_F(TestAlarmHandler, HandlerNoUsedBorrowIdsTrue4)
{
    const BorrowItemInfo borrowItemInfo{
        .borrowItem = std::vector<BorrowItem>{},
    };
    const LendItemInfo lendItemInfo{
        .lendItemNum = 0,
        .lendItem = {},
    };
    AlarmNumaInfo alarmNumaInfo{};
    alarmNumaInfo.numaLoc = expectedNodeLocInfo;
    alarmNumaInfo.borrowItemInfo = borrowItemInfo;
    alarmNumaInfo.lendItemInfo = lendItemInfo;
    WatermarkWarningType type = WatermarkWarningType::HIGH_WATERMARK;
    MOCKER_CPP(&ResourceCollect::GetGlobalBorrowMap, GlobalBorrowMap(ResourceCollect::*)())
        .stubs()
        .will(invoke(MockGetGlobalBorrowMap));
    const auto ret = AlarmHandler::HandlerNoUsedBorrowIds(alarmNumaInfo, type);
    EXPECT_TRUE(ret);
    MOCKER_CPP(&ResourceCollect::GetGlobalBorrowMap, GlobalBorrowMap(ResourceCollect::*)()).reset();
}

std::vector<std::string> MockGenVectorByBorrowItem(const AlarmNumaInfo &alarmNumaInfo)
{
    std::string borrowId = "host1";
    std::vector<std::string> borrowIdsInMem{borrowId};
    return borrowIdsInMem;
}

TEST_F(TestAlarmHandler, HandlerNoUsedBorrowIdsFalse1)
{
    const BorrowItem borrowItem{
        .exportLocNum = 1,
        .exportLocInfo = {nodeLocInfo},
        .exportMemId = 1,
        .importMemId = 2,
        .name = "host1",
    };
    const BorrowItemInfo borrowItemInfo{.borrowItem = std::vector{borrowItem}};
    const LendItemInfo lendItemInfo{
        .lendItemNum = 0,
        .lendItem = {},
    };
    AlarmNumaInfo alarmNumaInfo;
    alarmNumaInfo.numaLoc = expectedNodeLocInfo;
    alarmNumaInfo.borrowItemInfo = borrowItemInfo;
    alarmNumaInfo.lendItemInfo = lendItemInfo;
    WatermarkWarningType type = WatermarkWarningType::HIGH_WATERMARK;
    MOCKER_CPP(&ResourceCollect::GetGlobalBorrowMap, GlobalBorrowMap(ResourceCollect::*)())
        .stubs()
        .will(invoke(MockGetGlobalBorrowMap));
    MOCKER(AlarmHandler::GenVectorByBorrowItem).stubs().will(invoke(MockGenVectorByBorrowItem));
    MOCKER(StatusManager::ReturnByBorrowIdStatus).stubs().will(returnValue(VM_OK));
    const auto ret = AlarmHandler::HandlerNoUsedBorrowIds(alarmNumaInfo, type);
    EXPECT_FALSE(ret);
    MOCKER(AlarmHandler::GenVectorByBorrowItem).reset();
    MOCKER_CPP(&ResourceCollect::GetGlobalBorrowMap, GlobalBorrowMap(ResourceCollect::*)()).reset();
}

TEST_F(TestAlarmHandler, HandlerNoUsedBorrowIdsFalse2)
{
    const BorrowItem borrowItem{
        .exportLocNum = 1,
        .exportLocInfo = {nodeLocInfo},
        .exportMemId = 1,
        .importMemId = 2,
        .name = "host1",
    };
    const BorrowItemInfo borrowItemInfo{.borrowItem = std::vector{borrowItem}};
    const LendItemInfo lendItemInfo{
        .lendItemNum = 0,
        .lendItem = {},
    };
    AlarmNumaInfo alarmNumaInfo;
    alarmNumaInfo.numaLoc = expectedNodeLocInfo;
    alarmNumaInfo.borrowItemInfo = borrowItemInfo;
    alarmNumaInfo.lendItemInfo = lendItemInfo;
    WatermarkWarningType type = WatermarkWarningType::HIGH_WATERMARK;
    MOCKER_CPP(&ResourceCollect::GetGlobalBorrowMap, GlobalBorrowMap(ResourceCollect::*)())
        .stubs()
        .will(invoke(MockGetGlobalBorrowMap));
    MOCKER(AlarmHandler::GenVectorByBorrowItem).stubs().will(invoke(MockGenVectorByBorrowItem));
    MOCKER(StatusManager::MigrateByBorrowIdStatus)
        .stubs()
        .will(returnValue(VM_OK));
    const auto ret = AlarmHandler::HandlerNoUsedBorrowIds(alarmNumaInfo, type);
    EXPECT_FALSE(ret);
    MOCKER(AlarmHandler::GenVectorByBorrowItem).reset();
    MOCKER_CPP(&ResourceCollect::GetGlobalBorrowMap, GlobalBorrowMap(ResourceCollect::*)()).reset();
}

TEST_F(TestAlarmHandler, GenVectorByBorrowItemSuccess)
{
    const BorrowItem borrowItem{
        .exportLocNum = 1,
        .exportLocInfo = {nodeLocInfo},
        .exportMemId = 1,
        .importMemId = 2,
        .name = "host1",
    };
    const BorrowItemInfo borrowItemInfo{.borrowItem = std::vector{borrowItem}};
    const LendItemInfo lendItemInfo{
        .lendItemNum = 0,
        .lendItem = {},
    };
    AlarmNumaInfo alarmNumaInfo;
    alarmNumaInfo.numaLoc = expectedNodeLocInfo;
    alarmNumaInfo.borrowItemInfo = borrowItemInfo;
    alarmNumaInfo.lendItemInfo = lendItemInfo;
    const auto ret = AlarmHandler::GenVectorByBorrowItem(alarmNumaInfo);
    EXPECT_EQ(ret.size(), 1);
}

TEST_F(TestAlarmHandler, TestAlarmEventHandlerSuccess)
{
    BorrowItem borrowItem = BorrowItem{
        .exportLocNum = 1,
        .exportLocInfo = {nodeLocInfo},
        .exportMemId = 1,
        .importMemId = 2,
        .name = "host1",
    };
    BorrowItemInfo borrowItemInfo = BorrowItemInfo{};
    borrowItemInfo.borrowItem.emplace_back(borrowItem);
    LendItemInfo lendItemInfo = LendItemInfo{
        .lendItemNum = 0,
        .lendItem = {},
    };

    AlarmNumaInfo alarmNumaInfo{};
    alarmNumaInfo.numaLoc = expectedNodeLocInfo;
    alarmNumaInfo.borrowItemInfo = borrowItemInfo;
    alarmNumaInfo.lendItemInfo = lendItemInfo;
    UbsVirtNumaMemoryDebtInfo debtInfo{};
    debtInfo.numaId = 0;
    debtInfo.size = 1;
    std::vector<UbsVirtNumaMemoryDebtInfo> debtInfos{};
    debtInfos.push_back(debtInfo);
    WatermarkWarningType type = WatermarkWarningType::LOW_WATERMARK;
    MOCKER(&EscapeAlgorithmModule::GetStrategyAlgorithm).stubs().will(invoke(MockGetStrategyAlgorithmReturnOK));
    MOCKER(UbseGetNodeNumaInfoByNodeId).stubs().will(invoke(MockUbseGetNodeNumaInfoByNodeId));
    auto ret = AlarmHandler::AlarmEventHandler(alarmNumaInfo, debtInfos, type);
    EXPECT_EQ(ret, VM_OK);
    MOCKER(&EscapeAlgorithmModule::GetStrategyAlgorithm).reset();

    MOCKER(&EscapeAlgorithmModule::GetStrategyAlgorithm).stubs().will(invoke(MockGetStrategyAlgorithmReturnWarn));
    ret = AlarmHandler::AlarmEventHandler(alarmNumaInfo, debtInfos, type);
    EXPECT_EQ(ret, VM_OK);
    MOCKER(&EscapeAlgorithmModule::GetStrategyAlgorithm).reset();
    MOCKER(AlarmHandler::GetGlobalResource).reset();
    MOCKER(UbseGetNodeNumaInfoByNodeId).reset();
}

TEST_F(TestAlarmHandler, TestAlarmEventHandlerFailed)
{
    BorrowItem borrowItem = BorrowItem{
        .exportLocNum = 1,
        .exportLocInfo = {nodeLocInfo},
        .exportMemId = 1,
        .importMemId = 2,
        .name = "host1",
    };
    BorrowItemInfo borrowItemInfo = BorrowItemInfo{};
    borrowItemInfo.borrowItem.emplace_back(borrowItem);
    LendItemInfo lendItemInfo = LendItemInfo{
        .lendItemNum = 0,
        .lendItem = {},
    };
    AlarmNumaInfo alarmNumaInfo{};
    alarmNumaInfo.numaLoc = expectedNodeLocInfo;
    alarmNumaInfo.borrowItemInfo = borrowItemInfo;
    alarmNumaInfo.lendItemInfo = lendItemInfo;
    UbsVirtNumaMemoryDebtInfo debtInfo{};
    debtInfo.numaId = 0;
    debtInfo.size = 1;
    std::vector<UbsVirtNumaMemoryDebtInfo> debtInfos{};
    debtInfos.push_back(debtInfo);
    WatermarkWarningType type = WatermarkWarningType::CLEAR_BORROW;
    MOCKER(&AlarmHandler::BorrowClearEventHandler).stubs().will(returnValue(VM_ERROR));
    auto ret = AlarmHandler::AlarmEventHandler(alarmNumaInfo, debtInfos, type);
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(&AlarmHandler::BorrowClearEventHandler).reset();

    type = WatermarkWarningType::LOW_WATERMARK;
    MOCKER(&EscapeAlgorithmModule::GetStrategyAlgorithm).stubs().will(invoke(MockGetStrategyAlgorithmReturnNull));
    ret = AlarmHandler::AlarmEventHandler(alarmNumaInfo, debtInfos, type);
    EXPECT_EQ(ret, VM_ERROR);
    MOCKER(&EscapeAlgorithmModule::GetStrategyAlgorithm).reset();
    MOCKER(AlarmHandler::GetGlobalResource).reset();
}

} // namespace ubse::vm::ut