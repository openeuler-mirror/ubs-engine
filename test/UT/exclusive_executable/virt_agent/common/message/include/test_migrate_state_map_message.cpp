/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "test_migrate_state_map_message.h"

using namespace vm;
namespace ubse::ut::vm {
void TestMigrateStateMapMessage::SetUp() {}
void TestMigrateStateMapMessage::TearDown() {}

std::unordered_map<int16_t, mempooling::VmDomainNumaInfo> numaMemInfo = {
    {1, {1, 1, 1, 1, 1}}
};
/**
 * 序列化成功
 */
TEST_F(TestMigrateStateMapMessage, SerializeSuccess)
{
    MigrateStateMapMessage dataMessage{};
    NumaVMInfoMap numaVmInfoMap{};
    VMNodeLocInfo nodeLocInfo = VMNodeLocInfo{
        .hostName = "",
        .hostId = "",
        .socketId = 1,
        .numaId = 1,
    };

    VMBasicInfo vmBasicInfo = VMBasicInfo{
        .remoteUsedMem = 0,
        .vmSampleTime = 0,
    };
    vmBasicInfo.numaMemInfo = numaMemInfo;
    vmBasicInfo.uuid = "4aa51a33-eef5-46de-9f52-8f67cbe6a987";

    numaVmInfoMap[nodeLocInfo][vmBasicInfo.uuid] = vmBasicInfo;
    dataMessage.SetData(numaVmInfoMap);
    auto ret = dataMessage.Serialize();
    EXPECT_EQ(ret, VM_OK);
}

/**
 * 反序列化成功
 */
TEST_F(TestMigrateStateMapMessage, DeserializeSuccess)
{
    MigrateStateMapMessage dataMessage{};
    NumaVMInfoMap numaVmInfoMap{};
    VMNodeLocInfo nodeLocInfo = VMNodeLocInfo{
        .hostName = "",
        .hostId = "",
        .socketId = 1,
        .numaId = 1,
    };
    VMBasicInfo vmBasicInfo = VMBasicInfo{
        .remoteUsedMem = 0,
        .vmSampleTime = 0,
    };
    vmBasicInfo.numaMemInfo = numaMemInfo;
    vmBasicInfo.uuid = "4aa51a33-eef5-46de-9f52-8f67cbe6a987";

    numaVmInfoMap[nodeLocInfo][vmBasicInfo.uuid] = vmBasicInfo;
    dataMessage.SetData(numaVmInfoMap);
    auto ret = dataMessage.Serialize();
    EXPECT_EQ(ret, VM_OK);
    dataMessage.SetInputRawData(dataMessage.SerializedData(), dataMessage.SerializedDataSize());
    auto ret2 = dataMessage.Deserialize();
    EXPECT_EQ(ret2, VM_OK);
}

/**
 * InputRawData异常,反序列化失败
 */
TEST_F(TestMigrateStateMapMessage, DeserializeFailed)
{
    MigrateStateMapMessage dataMessage{};
    auto aa = new uint8_t[10];
    dataMessage.SetInputRawData(aa, 1);
    auto ret = dataMessage.Deserialize();
    EXPECT_EQ(ret, VM_ERROR);
}

}