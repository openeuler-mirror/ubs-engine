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

#include "test_process_mem_pid_manager_def.h"

namespace ubse::ut::process_mem {
using namespace ::process_mem::def;

void TestProcessMemPidManagerDef::SetUp() {}

void TestProcessMemPidManagerDef::TearDown() {}

TEST_F(TestProcessMemPidManagerDef, ProcessMemNewConfigInfoDefaultValues)
{
    ProcessMemNewConfigInfo config{};
    EXPECT_TRUE(config.isPid);
    EXPECT_TRUE(config.identifier.empty());
    EXPECT_EQ(config.maxMemory, 0u);
    EXPECT_DOUBLE_EQ(config.remoteRatio, 0.0);
    EXPECT_EQ(config.startTime, 0u);
}

TEST_F(TestProcessMemPidManagerDef, SerializeProcessMemNewConfigInfoPidMode)
{
    ProcessMemNewConfigInfo config{};
    config.isPid = true;
    config.identifier = "1234";
    config.maxMemory = 1024000;
    config.remoteRatio = 0.5;
    config.startTime = 9999;

    ubse::serial::UbseSerialization serializer;
    auto ret = config.Serialize(serializer);
    EXPECT_EQ(ret, UBSE_OK);

    ubse::serial::UbseDeSerialization deserializer(serializer.GetBuffer(), serializer.GetLength());
    ProcessMemNewConfigInfo deserialized{};
    ret = deserialized.Deserialize(deserializer);
    EXPECT_EQ(ret, UBSE_OK);

    EXPECT_TRUE(deserialized.isPid);
    EXPECT_EQ(deserialized.identifier, "1234");
    EXPECT_EQ(deserialized.maxMemory, 1024000u);
    EXPECT_DOUBLE_EQ(deserialized.remoteRatio, 0.5);
    EXPECT_EQ(deserialized.startTime, 9999u);
}

TEST_F(TestProcessMemPidManagerDef, SerializeProcessMemNewConfigInfoNameMode)
{
    ProcessMemNewConfigInfo config{};
    config.isPid = false;
    config.identifier = "myproc";
    config.maxMemory = 2048000;
    config.remoteRatio = 0.3;
    config.startTime = 0;

    ubse::serial::UbseSerialization serializer;
    auto ret = config.Serialize(serializer);
    EXPECT_EQ(ret, UBSE_OK);

    ubse::serial::UbseDeSerialization deserializer(serializer.GetBuffer(), serializer.GetLength());
    ProcessMemNewConfigInfo deserialized{};
    ret = deserialized.Deserialize(deserializer);
    EXPECT_EQ(ret, UBSE_OK);

    EXPECT_FALSE(deserialized.isPid);
    EXPECT_EQ(deserialized.identifier, "myproc");
    EXPECT_EQ(deserialized.maxMemory, 2048000u);
    EXPECT_DOUBLE_EQ(deserialized.remoteRatio, 0.3);
}

TEST_F(TestProcessMemPidManagerDef, SerializeProcessMemNewConfigInfoZeroValues)
{
    ProcessMemNewConfigInfo config{};
    config.isPid = true;
    config.identifier = "0";
    config.maxMemory = 0;
    config.remoteRatio = 0.0;

    ubse::serial::UbseSerialization serializer;
    auto ret = config.Serialize(serializer);
    EXPECT_EQ(ret, UBSE_OK);

    ubse::serial::UbseDeSerialization deserializer(serializer.GetBuffer(), serializer.GetLength());
    ProcessMemNewConfigInfo deserialized{};
    ret = deserialized.Deserialize(deserializer);
    EXPECT_EQ(ret, UBSE_OK);
    EXPECT_TRUE(deserialized.isPid);
    EXPECT_EQ(deserialized.identifier, "0");
}

TEST_F(TestProcessMemPidManagerDef, ProcessMemDisplayEntryDefaultValues)
{
    ProcessMemDisplayEntry entry{};
    EXPECT_EQ(entry.pid, 0);
    EXPECT_TRUE(entry.name.empty());
    EXPECT_EQ(entry.maxMemory, 0u);
    EXPECT_DOUBLE_EQ(entry.remoteRatio, 0.0);
}

TEST_F(TestProcessMemPidManagerDef, SerializeProcessMemDisplayEntry)
{
    ProcessMemDisplayEntry entry{};
    entry.pid = 42;
    entry.name = "myproc";
    entry.maxMemory = 4096000;
    entry.remoteRatio = 0.75;

    ubse::serial::UbseSerialization serializer;
    auto ret = entry.Serialize(serializer);
    EXPECT_EQ(ret, UBSE_OK);

    ubse::serial::UbseDeSerialization deserializer(serializer.GetBuffer(), serializer.GetLength());
    ProcessMemDisplayEntry deserialized{};
    ret = deserialized.Deserialize(deserializer);
    EXPECT_EQ(ret, UBSE_OK);

    EXPECT_EQ(deserialized.pid, 42);
    EXPECT_EQ(deserialized.name, "myproc");
    EXPECT_EQ(deserialized.maxMemory, 4096000u);
    EXPECT_DOUBLE_EQ(deserialized.remoteRatio, 0.75);
}

TEST_F(TestProcessMemPidManagerDef, ParsePidFromIdentifierValid)
{
    auto pid = ParsePidFromIdentifier("1234");
    ASSERT_TRUE(pid.has_value());
    EXPECT_EQ(pid.value(), 1234);
}

TEST_F(TestProcessMemPidManagerDef, ParsePidFromIdentifierInvalid)
{
    EXPECT_FALSE(ParsePidFromIdentifier("not_a_pid").has_value());
    EXPECT_FALSE(ParsePidFromIdentifier("").has_value());
    EXPECT_FALSE(ParsePidFromIdentifier("999999999999999999999").has_value());
}

TEST_F(TestProcessMemPidManagerDef, ParsePidFromIdentifierNonPositive)
{
    EXPECT_FALSE(ParsePidFromIdentifier("0").has_value());
    EXPECT_FALSE(ParsePidFromIdentifier("-1").has_value());
}

TEST_F(TestProcessMemPidManagerDef, ManagedPidEntryDefaultValues)
{
    ManagedPidEntry entry{};
    EXPECT_EQ(entry.pid, 0);
    EXPECT_EQ(entry.parentPid, 0);
    EXPECT_FALSE(entry.isChild);
    EXPECT_EQ(entry.sources, 0u);
    EXPECT_TRUE(entry.nameConfigName.empty());
    EXPECT_EQ(entry.maxMemory, 0u);
    EXPECT_DOUBLE_EQ(entry.remoteRatio, 0.0);
    EXPECT_EQ(entry.vmRss, 0u);
    EXPECT_EQ(entry.processStatus, ProcessStatus::IDLE);
    EXPECT_TRUE(entry.borrow.slots.empty());
}

TEST_F(TestProcessMemPidManagerDef, ConfigSourceBitValues)
{
    EXPECT_EQ(static_cast<uint8_t>(ConfigSource::PID_CONFIG), 1u);
    EXPECT_EQ(static_cast<uint8_t>(ConfigSource::NAME_CONFIG), 2u);
}

TEST_F(TestProcessMemPidManagerDef, BorrowSlotDefaultValues)
{
    BorrowSlot slot{};
    EXPECT_EQ(slot.migratedBytes, 0u);
    EXPECT_EQ(slot.exportSlotId, -1);
    EXPECT_TRUE(slot.debtId.empty());
    EXPECT_EQ(slot.srcNumaId, -1);
    EXPECT_EQ(slot.remoteNumaId, -1);
    EXPECT_EQ(slot.status, BorrowSlotStatus::BORROWING);
}

TEST_F(TestProcessMemPidManagerDef, BorrowStateDefaultValues)
{
    BorrowState state{};
    EXPECT_EQ(state.currentRemote, 0u);
    EXPECT_TRUE(state.slots.empty());
    EXPECT_TRUE(state.remoteNumaMigrated.empty());
}

TEST_F(TestProcessMemPidManagerDef, BorrowCandidateDefaultValues)
{
    BorrowCandidate candidate{};
    EXPECT_EQ(candidate.pid, 0);
    EXPECT_FALSE(candidate.isChild);
    EXPECT_EQ(candidate.actual, 0u);
    EXPECT_EQ(candidate.maxMemory, 0u);
    EXPECT_DOUBLE_EQ(candidate.remoteRatio, 0.0);
    EXPECT_EQ(candidate.currentRemote, 0u);
    EXPECT_EQ(candidate.canMigrate, 0u);
}

TEST_F(TestProcessMemPidManagerDef, BorrowSlotStatusDistinct)
{
    EXPECT_NE(BorrowSlotStatus::BORROWING, BorrowSlotStatus::COMPLETED);
    EXPECT_NE(BorrowSlotStatus::COMPLETED, BorrowSlotStatus::FAILED);
    EXPECT_NE(BorrowSlotStatus::FAILED, BorrowSlotStatus::BORROWING);
}

TEST_F(TestProcessMemPidManagerDef, KeyPrefixConstants)
{
    EXPECT_FALSE(PROC_MEM_PID_KEY_PREFIX.empty());
    EXPECT_EQ(PROC_MEM_PID_KEY_PREFIX.find("procMem_pid_"), 0u);
    EXPECT_FALSE(PROC_MEM_NAME_KEY_PREFIX.empty());
    EXPECT_EQ(PROC_MEM_NAME_KEY_PREFIX.find("procMem_name_"), 0u);
    EXPECT_NE(PROC_MEM_PID_KEY_PREFIX, PROC_MEM_NAME_KEY_PREFIX);
}

TEST_F(TestProcessMemPidManagerDef, ProcessStatusEnumValues)
{
    EXPECT_NE(ProcessStatus::IDLE, ProcessStatus::BORROWING);
    EXPECT_NE(ProcessStatus::BORROWING, ProcessStatus::BORROWED);
    EXPECT_NE(ProcessStatus::IDLE, ProcessStatus::BORROWED);
}

TEST_F(TestProcessMemPidManagerDef, ProcessMemUsrInfoStaticAssert)
{
    EXPECT_EQ(sizeof(UsrInfoPluginType), sizeof(uint32_t));
    EXPECT_TRUE(sizeof(ProcessMemUsrInfo) <= ubse::mem::controller::UBSE_MAX_USR_INFO_LEN);
}

TEST_F(TestProcessMemPidManagerDef, ProcessMemUsrInfoDefaultValues)
{
    ProcessMemUsrInfo usrInfo{};
    EXPECT_EQ(usrInfo.pluginId, UsrInfoPluginType::PROCESS_MEM);
    EXPECT_EQ(usrInfo.pid, 0);
    EXPECT_EQ(usrInfo.startTime, 0);
    EXPECT_EQ(usrInfo.srcNuma, -1);
}
} // namespace ubse::ut::process_mem
