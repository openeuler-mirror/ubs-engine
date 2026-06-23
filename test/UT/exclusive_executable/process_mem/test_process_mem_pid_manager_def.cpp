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

TEST_F(TestProcessMemPidManagerDef, SerializeConfigInfoWithoutSrcNuma)
{
    ProcessMemPidConfigInfo configInfo{};
    configInfo.pid = 1234;
    configInfo.evictThreshold = 80;
    configInfo.targetEvictThreshold = 70;
    configInfo.reclaimThreshold = 50;
    configInfo.expectedMemoryUsage = 1024000;
    configInfo.srcNumaId = std::nullopt;

    ubse::serial::UbseSerialization serializer;
    auto ret = configInfo.SerializeConfigInfo(serializer);
    EXPECT_EQ(ret, UBSE_OK);

    ubse::serial::UbseDeSerialization deserializer(serializer.GetBuffer(), serializer.GetLength());
    ProcessMemPidConfigInfo deserialized{};
    ret = deserialized.DeserializeConfigInfo(deserializer);
    EXPECT_EQ(ret, UBSE_OK);

    EXPECT_EQ(deserialized.pid, 1234);
    EXPECT_EQ(deserialized.evictThreshold, 80);
    EXPECT_EQ(deserialized.targetEvictThreshold, 70);
    EXPECT_EQ(deserialized.reclaimThreshold, 50);
    EXPECT_EQ(deserialized.expectedMemoryUsage, 1024000);
    EXPECT_FALSE(deserialized.srcNumaId.has_value());
}

TEST_F(TestProcessMemPidManagerDef, SerializeConfigInfoWithSrcNuma)
{
    ProcessMemPidConfigInfo configInfo{};
    configInfo.pid = 5678;
    configInfo.evictThreshold = 90;
    configInfo.targetEvictThreshold = 60;
    configInfo.reclaimThreshold = 40;
    configInfo.expectedMemoryUsage = 2048000;
    configInfo.srcNumaId = 2;

    ubse::serial::UbseSerialization serializer;
    auto ret = configInfo.SerializeConfigInfo(serializer);
    EXPECT_EQ(ret, UBSE_OK);

    ubse::serial::UbseDeSerialization deserializer(serializer.GetBuffer(), serializer.GetLength());
    ProcessMemPidConfigInfo deserialized{};
    ret = deserialized.DeserializeConfigInfo(deserializer);
    EXPECT_EQ(ret, UBSE_OK);

    EXPECT_EQ(deserialized.pid, 5678);
    EXPECT_EQ(deserialized.evictThreshold, 90);
    EXPECT_EQ(deserialized.targetEvictThreshold, 60);
    EXPECT_EQ(deserialized.reclaimThreshold, 40);
    EXPECT_EQ(deserialized.expectedMemoryUsage, 2048000);
    EXPECT_TRUE(deserialized.srcNumaId.has_value());
    EXPECT_EQ(deserialized.srcNumaId.value(), 2u);
}

TEST_F(TestProcessMemPidManagerDef, SerializePidInfoWithoutSrcNuma)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.ppid = 100;
    pidInfo.startTime = 9999;
    pidInfo.configInfo.pid = 1234;
    pidInfo.configInfo.evictThreshold = 80;
    pidInfo.configInfo.targetEvictThreshold = 70;
    pidInfo.configInfo.reclaimThreshold = 50;
    pidInfo.configInfo.expectedMemoryUsage = 1024000;
    pidInfo.configInfo.srcNumaId = std::nullopt;

    ubse::serial::UbseSerialization serializer;
    auto ret = pidInfo.SerializePidInfo(serializer);
    EXPECT_EQ(ret, UBSE_OK);

    ubse::serial::UbseDeSerialization deserializer(serializer.GetBuffer(), serializer.GetLength());
    ProcessMemPidInfo deserialized{};
    ret = deserialized.DeserializePidInfo(deserializer);
    EXPECT_EQ(ret, UBSE_OK);

    EXPECT_EQ(deserialized.startTime, 9999);
    EXPECT_EQ(deserialized.configInfo.pid, 1234);
    EXPECT_EQ(deserialized.configInfo.evictThreshold, 80);
    EXPECT_FALSE(deserialized.configInfo.srcNumaId.has_value());
}

TEST_F(TestProcessMemPidManagerDef, SerializePidInfoWithSrcNuma)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.ppid = 200;
    pidInfo.startTime = 12345;
    pidInfo.configInfo.pid = 5678;
    pidInfo.configInfo.evictThreshold = 90;
    pidInfo.configInfo.srcNumaId = 3;

    ubse::serial::UbseSerialization serializer;
    auto ret = pidInfo.SerializePidInfo(serializer);
    EXPECT_EQ(ret, UBSE_OK);

    ubse::serial::UbseDeSerialization deserializer(serializer.GetBuffer(), serializer.GetLength());
    ProcessMemPidInfo deserialized{};
    ret = deserialized.DeserializePidInfo(deserializer);
    EXPECT_EQ(ret, UBSE_OK);

    EXPECT_EQ(deserialized.startTime, 12345);
    EXPECT_EQ(deserialized.configInfo.pid, 5678);
    EXPECT_TRUE(deserialized.configInfo.srcNumaId.has_value());
    EXPECT_EQ(deserialized.configInfo.srcNumaId.value(), 3u);
}

TEST_F(TestProcessMemPidManagerDef, ProcessMemPidInfoDefaultValues)
{
    ProcessMemPidInfo pidInfo{};
    EXPECT_EQ(pidInfo.ppid, 0);
    EXPECT_EQ(pidInfo.startTime, 0);
    EXPECT_EQ(pidInfo.configInfo.pid, 0);
    EXPECT_EQ(pidInfo.processStatus, ProcessStatus::IDLE);
    EXPECT_EQ(pidInfo.memBorrowInfo.remoteNumaId, -1);
    EXPECT_EQ(pidInfo.memBorrowInfo.exportSlotId, -1);
    EXPECT_EQ(pidInfo.memBorrowInfo.importSocketId, -1);
    EXPECT_TRUE(pidInfo.memBorrowInfo.debtInfos.empty());
}

TEST_F(TestProcessMemPidManagerDef, BorrowInfoDefaultValues)
{
    BorrowInfo borrowInfo{};
    EXPECT_EQ(borrowInfo.remoteNumaId, -1);
    EXPECT_EQ(borrowInfo.exportSlotId, -1);
    EXPECT_EQ(borrowInfo.importSocketId, -1);
    EXPECT_TRUE(borrowInfo.debtInfos.empty());
}

TEST_F(TestProcessMemPidManagerDef, DebtInfoDefaultValues)
{
    DebtInfo debtInfo{};
    EXPECT_EQ(debtInfo.status, BorrowStatus::COMPLETED);
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

TEST_F(TestProcessMemPidManagerDef, SerializeDeserializeRoundTrip)
{
    ProcessMemPidConfigInfo configInfo{};
    configInfo.pid = 9999;
    configInfo.evictThreshold = 85;
    configInfo.targetEvictThreshold = 75;
    configInfo.reclaimThreshold = 55;
    configInfo.expectedMemoryUsage = 4096000;
    configInfo.srcNumaId = 5;

    ubse::serial::UbseSerialization serializer;
    auto ret = configInfo.SerializeConfigInfo(serializer);
    EXPECT_EQ(ret, UBSE_OK);

    ubse::serial::UbseDeSerialization deserializer(serializer.GetBuffer(), serializer.GetLength());
    ProcessMemPidConfigInfo deserialized{};
    ret = deserialized.DeserializeConfigInfo(deserializer);
    EXPECT_EQ(ret, UBSE_OK);

    EXPECT_EQ(deserialized.pid, configInfo.pid);
    EXPECT_EQ(deserialized.evictThreshold, configInfo.evictThreshold);
    EXPECT_EQ(deserialized.targetEvictThreshold, configInfo.targetEvictThreshold);
    EXPECT_EQ(deserialized.reclaimThreshold, configInfo.reclaimThreshold);
    EXPECT_EQ(deserialized.expectedMemoryUsage, configInfo.expectedMemoryUsage);
    EXPECT_EQ(deserialized.srcNumaId.value(), configInfo.srcNumaId.value());
}

// ==================== Edge case tests ====================

TEST_F(TestProcessMemPidManagerDef, SerializeConfigInfoZeroPid)
{
    ProcessMemPidConfigInfo configInfo{};
    configInfo.pid = 0;
    configInfo.evictThreshold = 0;
    configInfo.targetEvictThreshold = 0;
    configInfo.reclaimThreshold = 0;
    configInfo.expectedMemoryUsage = 0;
    configInfo.srcNumaId = std::nullopt;

    ubse::serial::UbseSerialization serializer;
    auto ret = configInfo.SerializeConfigInfo(serializer);
    EXPECT_EQ(ret, UBSE_OK);

    ubse::serial::UbseDeSerialization deserializer(serializer.GetBuffer(), serializer.GetLength());
    ProcessMemPidConfigInfo deserialized{};
    ret = deserialized.DeserializeConfigInfo(deserializer);
    EXPECT_EQ(ret, UBSE_OK);

    EXPECT_EQ(deserialized.pid, 0);
    EXPECT_EQ(deserialized.evictThreshold, 0);
    EXPECT_EQ(deserialized.targetEvictThreshold, 0);
    EXPECT_EQ(deserialized.reclaimThreshold, 0);
    EXPECT_EQ(deserialized.expectedMemoryUsage, 0u);
    EXPECT_FALSE(deserialized.srcNumaId.has_value());
}

TEST_F(TestProcessMemPidManagerDef, SerializeConfigInfoLargeValues)
{
    ProcessMemPidConfigInfo configInfo{};
    configInfo.pid = 65535;
    configInfo.evictThreshold = 100;
    configInfo.targetEvictThreshold = 100;
    configInfo.reclaimThreshold = 100;
    configInfo.expectedMemoryUsage = UINT64_MAX;
    configInfo.srcNumaId = 0;

    ubse::serial::UbseSerialization serializer;
    auto ret = configInfo.SerializeConfigInfo(serializer);
    EXPECT_EQ(ret, UBSE_OK);

    ubse::serial::UbseDeSerialization deserializer(serializer.GetBuffer(), serializer.GetLength());
    ProcessMemPidConfigInfo deserialized{};
    ret = deserialized.DeserializeConfigInfo(deserializer);
    EXPECT_EQ(ret, UBSE_OK);

    EXPECT_EQ(deserialized.pid, 65535);
    EXPECT_EQ(deserialized.evictThreshold, 100);
    EXPECT_EQ(deserialized.expectedMemoryUsage, UINT64_MAX);
    EXPECT_TRUE(deserialized.srcNumaId.has_value());
    EXPECT_EQ(deserialized.srcNumaId.value(), 0u);
}

TEST_F(TestProcessMemPidManagerDef, SerializePidInfoZeroConfigInfo)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.ppid = 0;
    pidInfo.startTime = 0;
    pidInfo.configInfo.pid = 0;
    pidInfo.configInfo.evictThreshold = 0;
    pidInfo.configInfo.targetEvictThreshold = 0;
    pidInfo.configInfo.reclaimThreshold = 0;
    pidInfo.configInfo.expectedMemoryUsage = 0;
    pidInfo.configInfo.srcNumaId = std::nullopt;

    ubse::serial::UbseSerialization serializer;
    auto ret = pidInfo.SerializePidInfo(serializer);
    EXPECT_EQ(ret, UBSE_OK);

    ubse::serial::UbseDeSerialization deserializer(serializer.GetBuffer(), serializer.GetLength());
    ProcessMemPidInfo deserialized{};
    ret = deserialized.DeserializePidInfo(deserializer);
    EXPECT_EQ(ret, UBSE_OK);

    EXPECT_EQ(deserialized.startTime, 0);
    EXPECT_EQ(deserialized.configInfo.pid, 0);
    EXPECT_FALSE(deserialized.configInfo.srcNumaId.has_value());
}

TEST_F(TestProcessMemPidManagerDef, ProcessStatusEnumValues)
{
    EXPECT_NE(ProcessStatus::IDLE, ProcessStatus::FAULT);
    EXPECT_NE(ProcessStatus::BORROWING, ProcessStatus::REPAYING);
    EXPECT_NE(ProcessStatus::WAIT_BORROW, ProcessStatus::INACTIVE);
}

TEST_F(TestProcessMemPidManagerDef, BorrowStatusEnumValues)
{
    EXPECT_NE(BorrowStatus::COMPLETED, BorrowStatus::CREATING);
}

TEST_F(TestProcessMemPidManagerDef, ConstantsAreValid)
{
    EXPECT_EQ(PROCESS_MEM_MODULE_CODE, 123u);
    EXPECT_EQ(PROCESS_MEM_SERVICE_ID, 11u);
    EXPECT_EQ(INVALID_REMOTE_NUMA, 0u);
}

TEST_F(TestProcessMemPidManagerDef, ProcessMemNamePrefix)
{
    EXPECT_FALSE(PROCESS_MEM_NAME_PREFIX.empty());
    EXPECT_EQ(PROCESS_MEM_NAME_PREFIX.find("ProcessMem"), 0u);
}

TEST_F(TestProcessMemPidManagerDef, PidCollectInfoDefault)
{
    PidCollectInfo info{};
    EXPECT_TRUE(info.numaMemDistribution.empty());
    EXPECT_TRUE(info.childrenInfo.empty());
}

TEST_F(TestProcessMemPidManagerDef, DoubleSerializeDeserialize)
{
    ProcessMemPidConfigInfo configInfo{};
    configInfo.pid = 1234;
    configInfo.evictThreshold = 80;
    configInfo.expectedMemoryUsage = 1024000;
    configInfo.srcNumaId = 1;

    // First serialize-deserialize
    ubse::serial::UbseSerialization serializer1;
    configInfo.SerializeConfigInfo(serializer1);
    ubse::serial::UbseDeSerialization deserializer1(serializer1.GetBuffer(), serializer1.GetLength());
    ProcessMemPidConfigInfo intermediate{};
    intermediate.DeserializeConfigInfo(deserializer1);

    // Second serialize-deserialize
    ubse::serial::UbseSerialization serializer2;
    intermediate.SerializeConfigInfo(serializer2);
    ubse::serial::UbseDeSerialization deserializer2(serializer2.GetBuffer(), serializer2.GetLength());
    ProcessMemPidConfigInfo final_{};
    final_.DeserializeConfigInfo(deserializer2);

    EXPECT_EQ(final_.pid, configInfo.pid);
    EXPECT_EQ(final_.evictThreshold, configInfo.evictThreshold);
    EXPECT_EQ(final_.expectedMemoryUsage, configInfo.expectedMemoryUsage);
    EXPECT_TRUE(final_.srcNumaId.has_value());
    EXPECT_EQ(final_.srcNumaId.value(), configInfo.srcNumaId.value());
}
} // namespace ubse::ut::process_mem
