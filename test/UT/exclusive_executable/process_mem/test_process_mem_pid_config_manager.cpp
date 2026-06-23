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

#include "test_process_mem_pid_config_manager.h"

#include "mock/ubse/mock_control.h"

namespace ubse::ut::process_mem {
using namespace ::process_mem::manager;
using namespace ::process_mem::def;

void TestProcessMemPidConfigManager::SetUp() {}

void TestProcessMemPidConfigManager::TearDown() {}

TEST_F(TestProcessMemPidConfigManager, GetExactStartTimeInvalidPid)
{
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(-1);
    EXPECT_EQ(startTime, 0u);
}

TEST_F(TestProcessMemPidConfigManager, GetExactStartTimeNonExistentPid)
{
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(99999999);
    EXPECT_EQ(startTime, 0u);
}

TEST_F(TestProcessMemPidConfigManager, IsPidInfoExistInvalidPid)
{
    bool result = ProcessMemPidConfigManager::IsPidInfoExist(-1, 12345);
    EXPECT_FALSE(result);
}

TEST_F(TestProcessMemPidConfigManager, IsPidInfoExistNonExistentPid)
{
    bool result = ProcessMemPidConfigManager::IsPidInfoExist(99999999, 12345);
    EXPECT_FALSE(result);
}

TEST_F(TestProcessMemPidConfigManager, IsPidInfoExistValidPidWrongStartTime)
{
    bool result = ProcessMemPidConfigManager::IsPidInfoExist(1, 0);
    EXPECT_FALSE(result);
}

TEST_F(TestProcessMemPidConfigManager, IsPidInfoExistValidPid)
{
    auto exactStartTime = ProcessMemPidConfigManager::GetExactStartTime(1);
    bool result = ProcessMemPidConfigManager::IsPidInfoExist(1, exactStartTime);
    EXPECT_TRUE(result);
}

TEST_F(TestProcessMemPidConfigManager, CheckPidConfigInfoInvalidPid)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = -1;
    pidInfo.startTime = 12345;
    bool result = ProcessMemPidConfigManager::CheckPidConfigInfo(pidInfo);
    EXPECT_FALSE(result);
}

TEST_F(TestProcessMemPidConfigManager, CheckPidConfigInfoNonExistentPid)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 99999999;
    pidInfo.startTime = 12345;
    bool result = ProcessMemPidConfigManager::CheckPidConfigInfo(pidInfo);
    EXPECT_FALSE(result);
}

TEST_F(TestProcessMemPidConfigManager, CheckPidConfigInfoValidPidWrongStartTime)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 1;
    pidInfo.startTime = 0;
    bool result = ProcessMemPidConfigManager::CheckPidConfigInfo(pidInfo);
    EXPECT_FALSE(result);
}

TEST_F(TestProcessMemPidConfigManager, PersistPidConfigInfo)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 1234;
    pidInfo.configInfo.evictThreshold = 80;
    pidInfo.configInfo.targetEvictThreshold = 70;
    pidInfo.configInfo.reclaimThreshold = 50;
    pidInfo.configInfo.expectedMemoryUsage = 1024000;
    pidInfo.startTime = 9999;

    EXPECT_NO_THROW(ProcessMemPidConfigManager::PersistPidConfigInfo(pidInfo));

    ProcessMemPidConfigManager::DeletePidConfigInfo(1234);
}

TEST_F(TestProcessMemPidConfigManager, DeletePidConfigInfo)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 5678;
    pidInfo.configInfo.evictThreshold = 85;
    pidInfo.startTime = 8888;

    ProcessMemPidConfigManager::PersistPidConfigInfo(pidInfo);
    EXPECT_NO_THROW(ProcessMemPidConfigManager::DeletePidConfigInfo(5678));
}

TEST_F(TestProcessMemPidConfigManager, DeletePidConfigInfoNonExistent)
{
    EXPECT_NO_THROW(ProcessMemPidConfigManager::DeletePidConfigInfo(99999999));
}

TEST_F(TestProcessMemPidConfigManager, GetAllPersistedPidConfigInfo)
{
    ProcessMemPidInfo pidInfo1{};
    pidInfo1.configInfo.pid = 1111;
    pidInfo1.startTime = 1000;
    pidInfo1.configInfo.evictThreshold = 80;

    ProcessMemPidInfo pidInfo2{};
    pidInfo2.configInfo.pid = 2222;
    pidInfo2.startTime = 2000;
    pidInfo2.configInfo.evictThreshold = 90;

    ProcessMemPidConfigManager::PersistPidConfigInfo(pidInfo1);
    ProcessMemPidConfigManager::PersistPidConfigInfo(pidInfo2);

    std::vector<ProcessMemPidInfo> pidInfos;
    EXPECT_NO_THROW(ProcessMemPidConfigManager::GetAllPersistedPidConfigInfo(pidInfos));

    ProcessMemPidConfigManager::DeletePidConfigInfo(1111);
    ProcessMemPidConfigManager::DeletePidConfigInfo(2222);
}

TEST_F(TestProcessMemPidConfigManager, QueryPidConfigCallbackNullBuffer)
{
    UbseByteBuffer buff{};
    buff.data = nullptr;
    buff.len = 0;
    std::vector<ProcessMemPidInfo> pidInfos;
    EXPECT_NO_THROW(ProcessMemPidConfigManager::QueryPidConfigCallback("prefix", "key", buff, &pidInfos));
    EXPECT_TRUE(pidInfos.empty());
}

TEST_F(TestProcessMemPidConfigManager, QueryPidConfigCallbackValidBuffer)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 3333;
    pidInfo.startTime = 3000;
    pidInfo.configInfo.evictThreshold = 75;

    ubse::serial::UbseSerialization serializer;
    auto ret = pidInfo.SerializePidInfo(serializer);
    EXPECT_EQ(ret, UBSE_OK);

    UbseByteBuffer buff{};
    buff.data = serializer.GetBuffer();
    buff.len = serializer.GetLength();

    std::vector<ProcessMemPidInfo> pidInfos;
    EXPECT_NO_THROW(ProcessMemPidConfigManager::QueryPidConfigCallback("prefix", "3333", buff, &pidInfos));
}

// ==================== Storage error path tests ====================

TEST_F(TestProcessMemPidConfigManager, PersistPidConfigInfoStoragePutError)
{
    ubse::storage::MockSetStoragePutError(UBSE_ERROR);

    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 4001;
    pidInfo.configInfo.evictThreshold = 80;
    pidInfo.startTime = 1000;

    EXPECT_NO_THROW(ProcessMemPidConfigManager::PersistPidConfigInfo(pidInfo));

    ubse::storage::MockSetStoragePutError(UBSE_OK);
}

TEST_F(TestProcessMemPidConfigManager, DeletePidConfigInfoStorageDeleteError)
{
    ubse::storage::MockSetStorageDeleteError(UBSE_ERROR);

    EXPECT_NO_THROW(ProcessMemPidConfigManager::DeletePidConfigInfo(4002));

    ubse::storage::MockSetStorageDeleteError(UBSE_OK);
}

TEST_F(TestProcessMemPidConfigManager, GetAllPersistedPidConfigInfoStorageQueryError)
{
    ubse::storage::MockSetStorageQueryError(UBSE_ERROR);

    std::vector<ProcessMemPidInfo> pidInfos;
    EXPECT_NO_THROW(ProcessMemPidConfigManager::GetAllPersistedPidConfigInfo(pidInfos));

    ubse::storage::MockSetStorageQueryError(UBSE_OK);
}

// ==================== CheckPidConfigInfo with valid pid ====================

TEST_F(TestProcessMemPidConfigManager, CheckPidConfigInfoValidPid)
{
    ProcessMemPidInfo pidInfo{};
    pidInfo.configInfo.pid = 1;
    auto exactStartTime = ProcessMemPidConfigManager::GetExactStartTime(1);
    pidInfo.startTime = exactStartTime;
    bool result = ProcessMemPidConfigManager::CheckPidConfigInfo(pidInfo);
    EXPECT_TRUE(result);
}

// ==================== QueryPidConfigCallback with deserialization error path ====================

TEST_F(TestProcessMemPidConfigManager, QueryPidConfigCallbackInvalidData)
{
    UbseByteBuffer buff{};
    uint8_t badData = 0xFF;
    buff.data = &badData;
    buff.len = 1;

    std::vector<ProcessMemPidInfo> pidInfos;
    EXPECT_NO_THROW(ProcessMemPidConfigManager::QueryPidConfigCallback("prefix", "key", buff, &pidInfos));
    EXPECT_TRUE(pidInfos.empty());
}
} // namespace ubse::ut::process_mem
