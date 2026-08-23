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

#include "test_process_mem_pid_bridge.h"

#include <map>

#include "ubse_api_server.h"
#include "ubse_node_controller.h"
#include "ubse_serial_util.h"
#include "mock/ubse/mock_control.h"
#include "process_mem_pid_info_manager.h"

namespace process_mem::pid::bridge {
uint32_t SendPidSetResponse(int successCode, const std::string& errorMsg, uint64_t requestId);
void BuildConfigEntries(std::vector<::process_mem::def::ProcessMemDisplayEntry>& entries,
                        const std::vector<::process_mem::def::ProcessMemNewConfigInfo>& newConfigs);
void BuildProcDetailEntries(std::vector<::process_mem::def::ProcessMemDisplayEntry>& entries,
                            const std::map<pid_t, ::process_mem::def::ManagedPidEntry>& managedSnapshot);
} // namespace process_mem::pid::bridge

namespace ubse::ut::process_mem {
using namespace ::process_mem::pid::bridge;
using namespace ::process_mem::manager;
namespace def = ::process_mem::def;

void TestProcessMemPidBridge::SetUp() {}

void TestProcessMemPidBridge::TearDown() {}

TEST_F(TestProcessMemPidBridge, SendPidSetResponseSuccess)
{
    auto ret = SendPidSetResponse(1, "", 12345);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, SendPidSetResponseFailure)
{
    auto ret = SendPidSetResponse(0, "Test error", 12345);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, SendPidSetResponseWithLongMessage)
{
    auto ret = SendPidSetResponse(1, "A very long error message that exceeds normal length", 99999);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, InitFailsWhenLibraryMissing)
{
    ProcessMemPidBridge::memPoolingHandle = nullptr;
    auto ret = ProcessMemPidBridge::Init();
    EXPECT_EQ(ret, UBSE_ERROR);
    EXPECT_EQ(ProcessMemPidBridge::memPoolingHandle, nullptr);
}

TEST_F(TestProcessMemPidBridge, UnInitWithNullHandle)
{
    ProcessMemPidBridge::memPoolingHandle = nullptr;
    auto ret = ProcessMemPidBridge::UnInit();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryReturnSuccess)
{
    auto ret = ProcessMemPidBridge::MemoryReturn("test_return_name");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryReturnWithEmptyName)
{
    auto ret = ProcessMemPidBridge::MemoryReturn("");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestProcessMemPidBridge, MemoryReturnEmptyNodeId)
{
    ubse::nodeController::MockSetCurrentNodeId("");
    auto ret = ProcessMemPidBridge::MemoryReturn("test_return");
    EXPECT_EQ(ret, UBSE_ERROR);
    ubse::nodeController::MockResetCurrentNodeId();
}

TEST_F(TestProcessMemPidBridge, MemoryReturnDeleteError)
{
    ubse::mem::controller::MockSetNumaDeleteError(UBSE_ERROR);

    auto ret = ProcessMemPidBridge::MemoryReturn("test_delete_error");
    EXPECT_NE(ret, UBSE_OK);

    ubse::mem::controller::MockSetNumaDeleteError(UBSE_OK);
}

namespace {

def::ProcessMemNewConfigInfo MakePidCfg(const std::string& pid, uint64_t maxMemory, double ratio)
{
    def::ProcessMemNewConfigInfo cfg{};
    cfg.isPid = true;
    cfg.identifier = pid;
    cfg.maxMemory = maxMemory;
    cfg.remoteRatio = ratio;
    return cfg;
}

def::ProcessMemNewConfigInfo MakeNameCfg(const std::string& name, uint64_t maxMemory, double ratio)
{
    def::ProcessMemNewConfigInfo cfg{};
    cfg.isPid = false;
    cfg.identifier = name;
    cfg.maxMemory = maxMemory;
    cfg.remoteRatio = ratio;
    return cfg;
}

} // namespace

TEST_F(TestProcessMemPidBridge, DisplayConfigPidAlone)
{
    std::vector<def::ProcessMemNewConfigInfo> configs = {MakePidCfg("1001", 10ull * 1024 * 1024 * 1024, 0.5)};

    std::vector<def::ProcessMemDisplayEntry> entries;
    BuildConfigEntries(entries, configs);

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].pid, 1001);
    EXPECT_EQ(entries[0].name, "N/A");
    EXPECT_EQ(entries[0].maxMemory, 10ull * 1024 * 1024 * 1024);
    EXPECT_DOUBLE_EQ(entries[0].remoteRatio, 0.5);
}

TEST_F(TestProcessMemPidBridge, DisplayConfigNameAlone)
{
    std::vector<def::ProcessMemNewConfigInfo> configs = {MakeNameCfg("testproc", 5ull * 1024 * 1024 * 1024, 0.3)};

    std::vector<def::ProcessMemDisplayEntry> entries;
    BuildConfigEntries(entries, configs);

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].pid, 0);
    EXPECT_EQ(entries[0].name, "testproc");
    EXPECT_EQ(entries[0].maxMemory, 5ull * 1024 * 1024 * 1024);
    EXPECT_DOUBLE_EQ(entries[0].remoteRatio, 0.3);
}

TEST_F(TestProcessMemPidBridge, DisplayConfigPidAndNameSorted)
{
    std::vector<def::ProcessMemNewConfigInfo> configs = {MakePidCfg("2002", 8ull * 1024 * 1024 * 1024, 0.4),
                                                         MakeNameCfg("testproc", 5ull * 1024 * 1024 * 1024, 0.3),
                                                         MakePidCfg("1001", 10ull * 1024 * 1024 * 1024, 0.5)};

    std::vector<def::ProcessMemDisplayEntry> entries;
    BuildConfigEntries(entries, configs);

    ASSERT_EQ(entries.size(), 3u);
    EXPECT_EQ(entries[0].pid, 1001);
    EXPECT_EQ(entries[0].name, "N/A");
    EXPECT_EQ(entries[1].pid, 2002);
    EXPECT_EQ(entries[1].name, "N/A");
    EXPECT_EQ(entries[2].pid, 0);
    EXPECT_EQ(entries[2].name, "testproc");
}

TEST_F(TestProcessMemPidBridge, DisplayDetailNameManagedEntries)
{
    std::map<pid_t, def::ManagedPidEntry> snapshot;
    def::ManagedPidEntry e1{};
    e1.pid = 1001;
    e1.nameConfigName = "testproc";
    e1.maxMemory = 5ull * 1024 * 1024 * 1024;
    e1.remoteRatio = 0.3;
    def::ManagedPidEntry e2{};
    e2.pid = 2002;
    e2.nameConfigName = "testproc";
    e2.maxMemory = 5ull * 1024 * 1024 * 1024;
    e2.remoteRatio = 0.3;
    snapshot[1001] = e1;
    snapshot[2002] = e2;

    std::vector<def::ProcessMemDisplayEntry> entries;
    BuildProcDetailEntries(entries, snapshot);

    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].pid, 1001);
    EXPECT_EQ(entries[0].name, "testproc");
    EXPECT_EQ(entries[0].maxMemory, 5ull * 1024 * 1024 * 1024);
    EXPECT_DOUBLE_EQ(entries[0].remoteRatio, 0.3);
    EXPECT_EQ(entries[1].pid, 2002);
    EXPECT_EQ(entries[1].name, "testproc");
    EXPECT_EQ(entries[1].maxMemory, 5ull * 1024 * 1024 * 1024);
}

TEST_F(TestProcessMemPidBridge, DisplayDetailNoManagedEntries)
{
    std::map<pid_t, def::ManagedPidEntry> snapshot;

    std::vector<def::ProcessMemDisplayEntry> entries;
    BuildProcDetailEntries(entries, snapshot);

    EXPECT_TRUE(entries.empty());
}

TEST_F(TestProcessMemPidBridge, DisplayDetailPidEntryShowsNA)
{
    std::map<pid_t, def::ManagedPidEntry> snapshot;
    def::ManagedPidEntry e{};
    e.pid = 2002;
    e.maxMemory = 10ull * 1024 * 1024 * 1024;
    e.remoteRatio = 0.5;
    snapshot[2002] = e;

    std::vector<def::ProcessMemDisplayEntry> entries;
    BuildProcDetailEntries(entries, snapshot);

    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].pid, 2002);
    EXPECT_EQ(entries[0].name, "N/A");
    EXPECT_EQ(entries[0].maxMemory, 10ull * 1024 * 1024 * 1024);
    EXPECT_DOUBLE_EQ(entries[0].remoteRatio, 0.5);
}

TEST_F(TestProcessMemPidBridge, DisplayDetailMixedNameAndPidEntries)
{
    std::map<pid_t, def::ManagedPidEntry> snapshot;
    def::ManagedPidEntry e1{};
    e1.pid = 1001;
    e1.sources = static_cast<uint8_t>(def::ConfigSource::NAME_CONFIG);
    e1.nameConfigName = "testproc";
    e1.maxMemory = 5ull * 1024 * 1024 * 1024;
    e1.remoteRatio = 0.3;
    def::ManagedPidEntry e2{};
    e2.pid = 2002;
    // name+pid 同时命中: pid 配置优先, name 强制 N/A, 值取 pid 配置
    e2.sources = static_cast<uint8_t>(def::ConfigSource::PID_CONFIG) |
                 static_cast<uint8_t>(def::ConfigSource::NAME_CONFIG);
    e2.nameConfigName = "testproc";
    e2.maxMemory = 10ull * 1024 * 1024 * 1024;
    e2.remoteRatio = 0.5;
    snapshot[1001] = e1;
    snapshot[2002] = e2;

    std::vector<def::ProcessMemDisplayEntry> entries;
    BuildProcDetailEntries(entries, snapshot);

    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].pid, 1001);
    EXPECT_EQ(entries[0].name, "testproc");
    EXPECT_EQ(entries[0].maxMemory, 5ull * 1024 * 1024 * 1024);
    EXPECT_DOUBLE_EQ(entries[0].remoteRatio, 0.3);
    EXPECT_EQ(entries[1].pid, 2002);
    EXPECT_EQ(entries[1].name, "N/A");
    EXPECT_EQ(entries[1].maxMemory, 10ull * 1024 * 1024 * 1024);
    EXPECT_DOUBLE_EQ(entries[1].remoteRatio, 0.5);
}
} // namespace ubse::ut::process_mem
