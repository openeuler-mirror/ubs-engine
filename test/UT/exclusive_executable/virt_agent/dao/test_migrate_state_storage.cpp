// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#include <mockcpp/mockcpp.hpp>

#include "migrate_state_map_message.h"
#include "migrate_state_storage.h"
#include "test_migrate_state_storage.h"

using namespace vm;
namespace ubse::ut::vm {
static const int BUFF_LEN = 10;
static const int PID = 1234;
static const int SOCKET_ID = 0;
static const int NUMA_ID = 0;
static const int MIGRATE_TIME = 1000;

// 设置测试环境
void TestMigrateStateStorage::SetUp()
{
    MigrateStateMapMessage migrateStateMapMessage;
    Test::SetUp();
}

// 拆卸测试环境
void TestMigrateStateStorage::TearDown()
{
    GlobalMockObject::verify();
    Test::TearDown();
}

uint32_t MockUbseStorageQueryData(const std::string& keyPrefix, const std::string& key, void* ctx,
                                  UbseStorageDealDataFunc func)
{
    auto* numaVMInfoMaps = static_cast<std::vector<NumaVMInfoMap>*>(ctx);
    NumaVMInfoMap numaVMInfoMap;
    numaVMInfoMaps->emplace_back(numaVMInfoMap);
    return VM_OK;
}

std::unordered_map<int16_t, mempooling::VmDomainNumaInfo> globalNumaMemInfo = {{1, {1, 1, 1, 1, 1}}};

TEST_F(TestMigrateStateStorage, SaveMigrateState_ShouldReturnSuccess)
{
    // Arrange
    MOCKER(UbseStoragePutData).stubs().will(returnValue(VM_OK));
    NumaVMInfoMap numaVmInfoMap;
    VMBasicInfo vmBasicInfo;
    VMNodeLocInfo vmNodeLocInfo{"", "", 1, 1};
    vmBasicInfo.numaMemInfo = globalNumaMemInfo;
    vmBasicInfo.uuid = "123456";

    // Act
    VmResult result = MigrateStateStorage::SaveMigrateState(numaVmInfoMap, vmBasicInfo);

    // Assert
    EXPECT_EQ(result, VM_OK);
    EXPECT_EQ(numaVmInfoMap[vmNodeLocInfo][vmBasicInfo.uuid].numaMemInfo.size(), vmBasicInfo.numaMemInfo.size());
    EXPECT_EQ(numaVmInfoMap[vmNodeLocInfo][vmBasicInfo.uuid].numaMemInfo.begin()->second.numaId,
              vmBasicInfo.numaMemInfo.begin()->second.numaId);
    EXPECT_EQ(numaVmInfoMap[vmNodeLocInfo][vmBasicInfo.uuid].numaMemInfo.begin()->second.socketId,
              vmBasicInfo.numaMemInfo.begin()->second.socketId);
    EXPECT_EQ(numaVmInfoMap[vmNodeLocInfo][vmBasicInfo.uuid].uuid, vmBasicInfo.uuid);
}

TEST_F(TestMigrateStateStorage, DelMigrateState_ShouldReturnSuccess)
{
    // Arrange
    MOCKER(UbseStoragePutData).stubs().will(returnValue(VM_OK));
    NumaVMInfoMap numaVmInfoMap;
    VMBasicInfo vmBasicInfo;
    VMNodeLocInfo vmNodeLocInfo{"", "", 1, 1};
    vmBasicInfo.numaMemInfo = globalNumaMemInfo;
    vmBasicInfo.uuid = "123456";
    numaVmInfoMap[vmNodeLocInfo][vmBasicInfo.uuid] = vmBasicInfo;

    // Act
    VmResult result = MigrateStateStorage::DelMigrateState(numaVmInfoMap, vmBasicInfo);

    // Assert
    EXPECT_EQ(result, VM_OK);
    EXPECT_NE(numaVmInfoMap[vmNodeLocInfo][vmBasicInfo.uuid].uuid, vmBasicInfo.uuid);
}

TEST_F(TestMigrateStateStorage, SaveMigrateState_ShouldReturnFailed_WhenPutDataFailed)
{
    // Arrange
    MOCKER(UbseStoragePutData).stubs().will(returnValue(VM_ERROR));
    NumaVMInfoMap numaVmInfoMap;
    VMBasicInfo vmBasicInfo;
    VMNodeLocInfo vmNodeLocInfo{"", "", 1, 1};
    vmBasicInfo.numaMemInfo = globalNumaMemInfo;
    vmBasicInfo.uuid = "123456";

    // Act
    VmResult result = MigrateStateStorage::SaveMigrateState(numaVmInfoMap, vmBasicInfo);

    // Assert
    EXPECT_EQ(result, VM_ERROR);
}

TEST_F(TestMigrateStateStorage, GetMigrateStates_ShouldReturnSuccess_WhenDataIsEmpty)
{
    // Arrange
    NumaVMInfoMap numaVmInfoMap;
    MOCKER(ubse::storage::UbseStorageQueryData).stubs().will(returnValue(VM_OK));

    // Act
    VmResult result = MigrateStateStorage::GetMigrateStates(numaVmInfoMap);

    // Assert
    EXPECT_EQ(result, VM_OK);
}

TEST_F(TestMigrateStateStorage, GetMigrateStates_ShouldReturnSuccess_WhenDataIsNotEmpty)
{
    // Arrange
    NumaVMInfoMap numaVmInfoMap;
    MOCKER(ubse::storage::UbseStorageQueryData)
        .stubs()
        .with(any(), any(), any(), any())
        .will(invoke(MockUbseStorageQueryData));

    // Act
    VmResult result = MigrateStateStorage::GetMigrateStates(numaVmInfoMap);

    // Assert
    EXPECT_EQ(result, VM_OK);
}

TEST_F(TestMigrateStateStorage, GetMigrateStates_ShouldReturnFailed_WhenQueryDataFailed)
{
    // Arrange
    NumaVMInfoMap numaVmInfoMap;
    MOCKER(ubse::storage::UbseStorageQueryData).stubs().will(returnValue(VM_ERROR));

    // Act
    VmResult result = MigrateStateStorage::GetMigrateStates(numaVmInfoMap);

    // Assert
    EXPECT_EQ(result, VM_ERROR);
}

TEST_F(TestMigrateStateStorage, QueryHandler_ShouldReturnWhenCtxIsNull)
{
    // Arrange
    std::string keyPrefix = "testPrefix";
    std::string key = "testKey";
    UbseByteBuffer buff;
    buff.data = new uint8_t[BUFF_LEN];
    buff.len = 0;
    void* ctx = nullptr;

    // Act
    MigrateStateStorage::QueryHandler(keyPrefix, key, buff, ctx);
    delete[] buff.data;
}

TEST_F(TestMigrateStateStorage, QueryHandler_ShouldReturnWhenBuffDataIsNull)
{
    std::string keyPrefix = "testPrefix";
    std::string key = "testKey";
    UbseByteBuffer buff;
    buff.data = nullptr;
    buff.len = 0;
    void* ctx = new std::vector<NumaVMInfoMap>();

    // Act
    MigrateStateStorage::QueryHandler(keyPrefix, key, buff, ctx);
    delete static_cast<std::vector<NumaVMInfoMap>*>(ctx);
}

TEST_F(TestMigrateStateStorage, QueryHandler_ShouldReturnWhenBuffDataIsValid)
{
    std::string keyPrefix = "testPrefix";
    std::string key = "testKey";
    UbseByteBuffer buff;
    buff.data = new uint8_t[BUFF_LEN];
    buff.len = BUFF_LEN;
    void* ctx = new std::vector<NumaVMInfoMap>();
    MigrateStateMapMessage migrateStateMapMessage;
    MOCKER_CPP_VIRTUAL(&migrateStateMapMessage, &MigrateStateMapMessage::Deserialize).stubs().will(returnValue(VM_OK));

    // Act
    MigrateStateStorage::QueryHandler(keyPrefix, key, buff, ctx);
    delete static_cast<std::vector<NumaVMInfoMap>*>(ctx);
    delete[] buff.data;
}

TEST_F(TestMigrateStateStorage, QueryHandler_ShouldReturnError_WhenSetInputRawDataFailed)
{
    std::string keyPrefix = "testPrefix";
    std::string key = "testKey";
    UbseByteBuffer buff;
    buff.data = new uint8_t[BUFF_LEN];
    buff.len = BUFF_LEN;
    void* ctx = new std::vector<NumaVMInfoMap>();
    MOCKER(&BaseMessage::SetInputRawData).stubs().will(returnValue(VM_ERROR));
    MigrateStateStorage::QueryHandler(keyPrefix, key, buff, ctx);
    delete static_cast<std::vector<NumaVMInfoMap>*>(ctx);
    delete[] buff.data;
}

TEST_F(TestMigrateStateStorage, QueryHandler_ShouldReturnError_WhenDeserializeFailed)
{
    std::string keyPrefix = "testPrefix";
    std::string key = "testKey";
    UbseByteBuffer buff;
    buff.data = new uint8_t[BUFF_LEN];
    buff.len = BUFF_LEN;
    void* ctx = new std::vector<NumaVMInfoMap>();
    MOCKER(&BaseMessage::SetInputRawData).stubs().will(returnValue(VM_OK));
    MigrateStateMapMessage migrateStateMapMessage;
    MOCKER_CPP_VIRTUAL(&migrateStateMapMessage, &MigrateStateMapMessage::Deserialize)
        .stubs()
        .will(returnValue(VM_ERROR));
    MigrateStateStorage::QueryHandler(keyPrefix, key, buff, ctx);
    delete static_cast<std::vector<NumaVMInfoMap>*>(ctx);
    delete[] buff.data;
}

TEST_F(TestMigrateStateStorage, ToString_ShouldReturnValidJson_WhenNumaVmInfoMapIsNotEmpty)
{
    NumaVMInfoMap numaVmInfoMap;
    VMBasicInfo vmBasicInfo;
    vmBasicInfo.uuid = "12345";
    vmBasicInfo.pid = PID;
    mempooling::VmDomainNumaInfo vmDomainNumaInfo{0, 0, 0, 0, false};
    vmBasicInfo.numaMemInfo.emplace(0, vmDomainNumaInfo);
    vmBasicInfo.vmMigrateInTime = MIGRATE_TIME;
    vmBasicInfo.vmMigrateStatus = MIGRATING;
    numaVmInfoMap[{"Node0", "Node0", 0, 1}][vmBasicInfo.uuid] = vmBasicInfo;
    std::string expected = "[{\"uuid\":\"12345\",\"pid\":1234,{\"socketId\":0,\"numaId\":0,},"
                           "\"time\":1000,\"status\":\"MIGRATING\"}]";
    std::string actual = MigrateStateStorage::ToString(numaVmInfoMap);
    EXPECT_EQ(expected, actual);
}
} // namespace ubse::ut::vm