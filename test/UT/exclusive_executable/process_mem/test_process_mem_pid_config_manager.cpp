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

void TestProcessMemPidConfigManager::SetUp()
{
    ubse::storage::MockResetStorageErrors();
}

void TestProcessMemPidConfigManager::TearDown()
{
    ubse::storage::MockResetStorageErrors();
}

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

TEST_F(TestProcessMemPidConfigManager, PersistProcMemConfigPidMode)
{
    ProcessMemNewConfigInfo config{};
    config.isPid = true;
    config.identifier = "1234";
    config.maxMemory = 1024000;
    config.remoteRatio = 0.5;
    config.startTime = 9999;

    auto ret = ProcessMemPidConfigManager::PersistProcMemConfig(config);
    EXPECT_EQ(ret, UBSE_OK);

    ProcessMemPidConfigManager::DeleteProcMemConfig(true, "1234");
}

TEST_F(TestProcessMemPidConfigManager, PersistProcMemConfigNameMode)
{
    ProcessMemNewConfigInfo config{};
    config.isPid = false;
    config.identifier = "myproc";
    config.maxMemory = 2048000;
    config.remoteRatio = 0.3;

    auto ret = ProcessMemPidConfigManager::PersistProcMemConfig(config);
    EXPECT_EQ(ret, UBSE_OK);

    ProcessMemPidConfigManager::DeleteProcMemConfig(false, "myproc");
}

TEST_F(TestProcessMemPidConfigManager, PersistProcMemConfigStoragePutError)
{
    ubse::storage::MockSetStoragePutError(UBSE_ERROR);

    ProcessMemNewConfigInfo config{};
    config.isPid = true;
    config.identifier = "4001";
    config.maxMemory = 1024000;

    auto ret = ProcessMemPidConfigManager::PersistProcMemConfig(config);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestProcessMemPidConfigManager, DeleteProcMemConfigNonExistent)
{
    EXPECT_NO_THROW(ProcessMemPidConfigManager::DeleteProcMemConfig(true, "99999999"));
}

TEST_F(TestProcessMemPidConfigManager, DeleteProcMemConfigStorageDeleteError)
{
    ubse::storage::MockSetStorageDeleteError(UBSE_ERROR);
    EXPECT_NO_THROW(ProcessMemPidConfigManager::DeleteProcMemConfig(true, "4002"));
}

TEST_F(TestProcessMemPidConfigManager, GetAllPersistedProcMemConfigsEmpty)
{
    std::vector<ProcessMemNewConfigInfo> configs;
    EXPECT_NO_THROW(ProcessMemPidConfigManager::GetAllPersistedProcMemConfigs(configs));
    EXPECT_TRUE(configs.empty());
}

TEST_F(TestProcessMemPidConfigManager, GetAllPersistedProcMemConfigsStorageListError)
{
    ubse::storage::MockSetStorageListError(UBSE_ERROR);

    std::vector<ProcessMemNewConfigInfo> configs;
    EXPECT_NO_THROW(ProcessMemPidConfigManager::GetAllPersistedProcMemConfigs(configs));
    EXPECT_TRUE(configs.empty());
}

TEST_F(TestProcessMemPidConfigManager, QueryProcMemConfigCallbackNullBuffer)
{
    UbseByteBuffer buff{};
    buff.data = nullptr;
    buff.len = 0;
    std::vector<ProcessMemNewConfigInfo> configs;
    EXPECT_NO_THROW(ProcessMemPidConfigManager::QueryProcMemConfigCallback("prefix", "key", buff, &configs));
    EXPECT_TRUE(configs.empty());
}

TEST_F(TestProcessMemPidConfigManager, QueryProcMemConfigCallbackValidBuffer)
{
    ProcessMemNewConfigInfo config{};
    config.isPid = true;
    config.identifier = "3333";
    config.maxMemory = 3072000;
    config.remoteRatio = 0.6;
    config.startTime = 3000;

    ubse::serial::UbseSerialization serializer;
    auto ret = config.Serialize(serializer);
    EXPECT_EQ(ret, UBSE_OK);

    UbseByteBuffer buff{};
    buff.data = serializer.GetBuffer();
    buff.len = serializer.GetLength();

    std::vector<ProcessMemNewConfigInfo> configs;
    EXPECT_NO_THROW(ProcessMemPidConfigManager::QueryProcMemConfigCallback("prefix", "3333", buff, &configs));
    ASSERT_EQ(configs.size(), 1u);
    EXPECT_TRUE(configs[0].isPid);
    EXPECT_EQ(configs[0].identifier, "3333");
    EXPECT_EQ(configs[0].maxMemory, 3072000u);
    EXPECT_DOUBLE_EQ(configs[0].remoteRatio, 0.6);
    EXPECT_EQ(configs[0].startTime, 3000u);
}

TEST_F(TestProcessMemPidConfigManager, QueryProcMemConfigCallbackInvalidData)
{
    UbseByteBuffer buff{};
    uint8_t badData = 0xFF;
    buff.data = &badData;
    buff.len = 1;

    std::vector<ProcessMemNewConfigInfo> configs;
    EXPECT_NO_THROW(ProcessMemPidConfigManager::QueryProcMemConfigCallback("prefix", "key", buff, &configs));
    EXPECT_TRUE(configs.empty());
}
} // namespace ubse::ut::process_mem
