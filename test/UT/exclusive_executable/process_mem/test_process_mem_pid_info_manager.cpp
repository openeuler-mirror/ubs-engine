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

#include "test_process_mem_pid_info_manager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "ubse_mem_controller.h"
#include "ubse_node_controller.h"
#include "ubse_serial_util.h"
#include "mock/ubse/mock_control.h"
#include "mock/ubse/ubse_smap_mock.h"
#include "process_mem_pid_bridge.h"
#include "process_mem_pid_collect.h"
#include "process_mem_pid_config_manager.h"

namespace ubse::ut::process_mem {
using namespace ::process_mem::manager;
using namespace ::process_mem::decision;
using namespace ::process_mem::def;
using namespace ::process_mem::pid::bridge;
namespace def = ::process_mem::def;
namespace collect = ::process_mem::collect;

namespace {
bool GetTestPid(pid_t& outPid)
{
    if (getuid() != 0) {
        outPid = getpid();
        return true;
    }
    try {
        for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
            if (!entry.is_directory()) {
                continue;
            }
            std::string pidStr = entry.path().filename().string();
            if (pidStr.empty() || !std::all_of(pidStr.begin(), pidStr.end(), ::isdigit)) {
                continue;
            }
            std::ifstream statusFile(entry.path().string() + "/status");
            std::string line;
            while (std::getline(statusFile, line)) {
                if (line.compare(0, 4, "Uid:") == 0) {
                    std::istringstream iss(line.substr(4));
                    uint32_t uid = 0;
                    iss >> uid;
                    if (uid != 0) {
                        outPid = static_cast<pid_t>(std::stoi(pidStr));
                        return true;
                    }
                    break;
                }
            }
        }
    } catch (...) {
    }
    return false;
}

std::string ReadSelfComm()
{
    std::ifstream commFile("/proc/self/comm");
    std::string comm;
    std::getline(commFile, comm);
    return comm;
}

std::vector<uint8_t> SerializeConfig(const def::ProcessMemNewConfigInfo& cfg)
{
    ubse::serial::UbseSerialization serializer;
    cfg.Serialize(serializer);
    return std::vector<uint8_t>(serializer.GetBuffer(), serializer.GetBuffer() + serializer.GetLength());
}

std::vector<uint8_t> SerializeFossil(const def::FossilPidConfigInfo& fossil)
{
    ubse::serial::UbseSerialization serializer;
    fossil.Serialize(serializer);
    return std::vector<uint8_t>(serializer.GetBuffer(), serializer.GetBuffer() + serializer.GetLength());
}

ubse::mem::controller::UbseNumaMemoryDebtInfo MakeDebtInfo(const std::string& name, int32_t pid)
{
    ubse::mem::controller::UbseNumaMemoryDebtInfo debt;
    debt.name = name;
    debt.borrowNodeId = "NODE0";
    debt.lentNodeId = "NODE0";
    debt.size = 1 * 1024ull * 1024ull * 1024ull;
    debt.remoteNumaId = 5;
    def::ProcessMemUsrInfo usr{};
    usr.pid = pid;
    usr.srcNuma = 1;
    memset(debt.usrInfo, 0, sizeof(debt.usrInfo));
    memcpy(debt.usrInfo, &usr, sizeof(usr));
    return debt;
}

std::string ReadProcComm(pid_t pid)
{
    std::ifstream commFile("/proc/" + std::to_string(pid) + "/comm");
    std::string comm;
    std::getline(commFile, comm);
    return comm;
}

constexpr pid_t MOCK_PID_A = 31111;
constexpr pid_t MOCK_PID_B = 32222;
constexpr pid_t MOCK_PID_C = 33333;
const std::string MOCK_NAME = "mock_proc";

std::vector<std::string> MockPidLifecycleKeys()
{
    return {std::to_string(MOCK_PID_A), std::to_string(MOCK_PID_B), MOCK_NAME, std::to_string(MOCK_PID_C)};
}

void InjectMockPidLifecycleState(const std::vector<std::string>& storageKeys)
{
    ubse::storage::MockSetStorageKeys(storageKeys);

    def::ProcessMemNewConfigInfo pidCfg{};
    pidCfg.isPid = true;
    pidCfg.maxMemory = 32 * 1073741824ull;
    pidCfg.remoteRatio = 0.5;
    pidCfg.identifier = std::to_string(MOCK_PID_A);
    ubse::storage::MockSetStorageQueryPayload(def::PROC_MEM_PID_KEY_PREFIX, pidCfg.identifier, SerializeConfig(pidCfg));
    pidCfg.identifier = std::to_string(MOCK_PID_B);
    ubse::storage::MockSetStorageQueryPayload(def::PROC_MEM_PID_KEY_PREFIX, pidCfg.identifier, SerializeConfig(pidCfg));

    def::ProcessMemNewConfigInfo nameCfg{};
    nameCfg.isPid = false;
    nameCfg.identifier = MOCK_NAME;
    nameCfg.maxMemory = 32 * 1073741824ull;
    nameCfg.remoteRatio = 0.5;
    ubse::storage::MockSetStorageQueryPayload(def::PROC_MEM_NAME_KEY_PREFIX, MOCK_NAME, SerializeConfig(nameCfg));

    def::FossilPidConfigInfo fossil{};
    fossil.name = MOCK_NAME;
    fossil.maxMemory = 32 * 1073741824ull;
    fossil.remoteRatio = 0.5;
    ubse::storage::MockSetStorageQueryPayload(def::PROC_MEM_FOSSIL_KEY_PREFIX, std::to_string(MOCK_PID_B),
                                              SerializeFossil(fossil));
    ubse::storage::MockSetStorageQueryPayload(def::PROC_MEM_FOSSIL_KEY_PREFIX, std::to_string(MOCK_PID_C),
                                              SerializeFossil(fossil));
}

pid_t GetAnyOtherLivePid(pid_t excluded = -1)
{
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        std::string pidStr = entry.path().filename().string();
        if (pidStr.empty() || !std::all_of(pidStr.begin(), pidStr.end(), ::isdigit)) {
            continue;
        }
        pid_t pid = static_cast<pid_t>(std::stoi(pidStr));
        if (pid <= 0 || pid == getpid() || pid == excluded) {
            continue;
        }
        std::ifstream statusFile(entry.path().string() + "/status");
        std::string line;
        while (std::getline(statusFile, line)) {
            if (line.compare(0, 4, "Uid:") == 0) {
                std::istringstream iss(line.substr(4));
                uint32_t uid = 0;
                iss >> uid;
                if (uid != 0) {
                    return pid;
                }
                break;
            }
        }
    }
    return -1;
}
} // namespace

void TestProcessMemPidInfoManager::SetUp()
{
    if (!::process_mem::pid::bridge::ProcessMemPidBridge::rmrsMigrateOut) {
        ::process_mem::pid::bridge::ProcessMemPidBridge::rmrsMigrateOut =
            [](const std::vector<mempooling::smap::MigrateOutPayload>&, int) -> int {
            return 0;
        };
    }
    if (!::process_mem::pid::bridge::ProcessMemPidBridge::rmrsRemove) {
        ::process_mem::pid::bridge::ProcessMemPidBridge::rmrsRemove = [](const uint16_t, const std::vector<pid_t>&,
                                                                         int) -> int {
            return 0;
        };
    }
    if (!::process_mem::pid::bridge::ProcessMemPidBridge::rmrsFreeWithMigrate) {
        ::process_mem::pid::bridge::ProcessMemPidBridge::rmrsFreeWithMigrate = [](const std::string&) -> uint32_t {
            return UBSE_OK;
        };
    }
    auto& decision = ::process_mem::decision::ProcessMemPidDecision::GetInstance();
    if (decision.returnExecutor_ == nullptr || !decision.returnExecutor_->IsStart()) {
        decision.returnExecutor_ = ubse::task_executor::UbseTaskExecutor::Create("ProcMemInfoMgrReturn", 2, 64);
        decision.returnExecutor_->Start();
    }
    ubse::storage::MockResetStorageErrors();
}

void TestProcessMemPidInfoManager::TearDown()
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    std::vector<def::ProcessMemNewConfigInfo> configs;
    mgr.GetAllProcMemConfigs(configs);
    for (const auto& cfg : configs) {
        mgr.RemoveProcMemConfig(cfg.isPid, cfg.identifier);
    }
    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    for (const auto& [pid, entry] : snapshot) {
        mgr.RemoveManagedPidEntry(pid);
    }
    ubse::storage::MockResetStorageErrors();
}

TEST_F(TestProcessMemPidInfoManager, InitBasic)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    EXPECT_NO_THROW(mgr.Init());
    EXPECT_NO_THROW(mgr.UnInit());
}

TEST_F(TestProcessMemPidInfoManager, UnInitWithoutInit)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    EXPECT_NO_THROW(mgr.UnInit());
}

TEST_F(TestProcessMemPidInfoManager, RefreshProcMemConfigCacheEmpty)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    EXPECT_NO_THROW(mgr.RefreshProcMemConfigCache());
    std::vector<def::ProcessMemNewConfigInfo> configs;
    mgr.GetAllProcMemConfigs(configs);
    EXPECT_TRUE(configs.empty());
}

TEST_F(TestProcessMemPidInfoManager, CleanupStalePidConfigsRemovesMismatch)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();

    pid_t alivePid = 0;
    if (!GetTestPid(alivePid)) {
        GTEST_SKIP() << "no non-root process available";
    }
    auto aliveStart = ProcessMemPidConfigManager::GetExactStartTime(alivePid);
    ASSERT_NE(aliveStart, 0u);

    pid_t stalePid = GetAnyOtherLivePid(alivePid);
    if (stalePid < 0) {
        GTEST_SKIP() << "no other live non-root process available";
    }
    auto staleStart = ProcessMemPidConfigManager::GetExactStartTime(stalePid);
    ASSERT_NE(staleStart, 0u);

    def::ProcessMemNewConfigInfo alive{};
    alive.isPid = true;
    alive.identifier = std::to_string(alivePid);
    alive.maxMemory = 1073741824;
    alive.remoteRatio = 0.5;
    alive.startTime = aliveStart;

    def::ProcessMemNewConfigInfo stale{};
    stale.isPid = true;
    stale.identifier = std::to_string(stalePid);
    stale.maxMemory = 2147483648;
    stale.remoteRatio = 0.7;
    stale.startTime = staleStart + 1;

    def::ProcessMemNewConfigInfo gone{};
    gone.isPid = true;
    gone.identifier = "99999999";
    gone.maxMemory = 1073741824;
    gone.remoteRatio = 0.5;
    gone.startTime = 12345;

    def::ProcessMemNewConfigInfo name{};
    name.isPid = false;
    name.identifier = "restore_name";
    name.maxMemory = 512000;
    name.remoteRatio = 0.3;

    ubse::storage::MockSetStorageKeys({alive.identifier, stale.identifier, gone.identifier, name.identifier});
    ubse::storage::MockSetStorageQueryPayload(def::PROC_MEM_PID_KEY_PREFIX, alive.identifier, SerializeConfig(alive));
    ubse::storage::MockSetStorageQueryPayload(def::PROC_MEM_PID_KEY_PREFIX, stale.identifier, SerializeConfig(stale));
    ubse::storage::MockSetStorageQueryPayload(def::PROC_MEM_PID_KEY_PREFIX, gone.identifier, SerializeConfig(gone));
    ubse::storage::MockSetStorageQueryPayload(def::PROC_MEM_NAME_KEY_PREFIX, name.identifier, SerializeConfig(name));

    mgr.RefreshProcMemConfigCache();
    mgr.CleanupStalePidConfigs();

    auto aliveStored = mgr.GetProcMemConfig(true, alive.identifier);
    EXPECT_EQ(aliveStored.maxMemory, 1073741824u);
    EXPECT_EQ(aliveStored.startTime, aliveStart);
    EXPECT_EQ(mgr.GetProcMemConfig(true, stale.identifier).maxMemory, 0u);
    EXPECT_EQ(mgr.GetProcMemConfig(true, gone.identifier).maxMemory, 0u);
    auto nameStored = mgr.GetProcMemConfig(false, name.identifier);
    EXPECT_EQ(nameStored.maxMemory, 512000u);
}

TEST_F(TestProcessMemPidInfoManager, FossilizePidConfigSkipsExistingWhenNotForced)
{
    pid_t testPid = 0;
    if (!GetTestPid(testPid)) {
        GTEST_SKIP() << "no non-root process available";
    }
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    def::ProcessMemNewConfigInfo config{};
    config.isPid = true;
    config.identifier = std::to_string(testPid);
    config.maxMemory = 1073741824;
    config.remoteRatio = 0.5;
    ASSERT_EQ(mgr.SetProcMemConfig(config), UBSE_OK);

    EXPECT_EQ(mgr.FossilizePidConfig(testPid, "testproc", 999999, 0.9, false), UBSE_OK);
    auto stored = mgr.GetProcMemConfig(true, config.identifier);
    EXPECT_EQ(stored.maxMemory, 1073741824u);
    EXPECT_DOUBLE_EQ(stored.remoteRatio, 0.5);
}

TEST_F(TestProcessMemPidInfoManager, FossilizePidConfigForceWritesFossilNotExplicit)
{
    pid_t testPid = 0;
    if (!GetTestPid(testPid)) {
        GTEST_SKIP() << "no non-root process available";
    }
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    def::ProcessMemNewConfigInfo config{};
    config.isPid = true;
    config.identifier = std::to_string(testPid);
    config.maxMemory = 1073741824;
    config.remoteRatio = 0.5;
    ASSERT_EQ(mgr.SetProcMemConfig(config), UBSE_OK);

    EXPECT_EQ(mgr.FossilizePidConfig(testPid, "testproc", 2048000, 0.8, true), UBSE_OK);
    auto stored = mgr.GetProcMemConfig(true, config.identifier);
    EXPECT_EQ(stored.maxMemory, 1073741824u);
    EXPECT_DOUBLE_EQ(stored.remoteRatio, 0.5);
}

TEST_F(TestProcessMemPidInfoManager, FossilizePidConfigNonExistentPid)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    EXPECT_EQ(mgr.FossilizePidConfig(99999999, "ghost", 1024000, 0.5, false), UBSE_ERR_NOT_EXIST);
}

TEST_F(TestProcessMemPidInfoManager, RebuildAndCleanupSkipRootPidConfig)
{
    pid_t rootPid = 1;
    ASSERT_EQ(::process_mem::collect::GetProcUid(rootPid), 0);
    auto rootStart = ProcessMemPidConfigManager::GetExactStartTime(rootPid);
    ASSERT_NE(rootStart, 0u);

    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    def::ProcessMemNewConfigInfo rootCfg{};
    rootCfg.isPid = true;
    rootCfg.identifier = std::to_string(rootPid);
    rootCfg.maxMemory = 1073741824;
    rootCfg.remoteRatio = 0.5;
    rootCfg.startTime = rootStart;

    ubse::storage::MockSetStorageKeys({rootCfg.identifier});
    ubse::storage::MockSetStorageQueryPayload(def::PROC_MEM_PID_KEY_PREFIX, rootCfg.identifier,
                                              SerializeConfig(rootCfg));

    mgr.RefreshProcMemConfigCache();
    mgr.RebuildManagedPidCache();
    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    EXPECT_EQ(snapshot.find(rootPid), snapshot.end());

    mgr.CleanupStalePidConfigs();
    EXPECT_EQ(mgr.GetProcMemConfig(true, rootCfg.identifier).maxMemory, 0u);
}

TEST_F(TestProcessMemPidInfoManager, RebuildNamePassSkipsRootProcess)
{
    pid_t rootPid = 1;
    std::ifstream commFile("/proc/" + std::to_string(rootPid) + "/comm");
    std::string comm;
    std::getline(commFile, comm);
    if (comm.empty()) {
        GTEST_SKIP() << "cannot read /proc/1/comm";
    }

    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    def::ProcessMemNewConfigInfo config{};
    config.isPid = false;
    config.identifier = comm;
    config.maxMemory = 1073741824;
    config.remoteRatio = 0.5;
    ASSERT_EQ(mgr.SetProcMemConfig(config), UBSE_OK);

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    EXPECT_EQ(snapshot.find(rootPid), snapshot.end());

    EXPECT_EQ(mgr.RemoveProcMemConfig(false, comm), UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, SetProcMemConfigInvalidPidFormat)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    def::ProcessMemNewConfigInfo config{};
    config.isPid = true;
    config.identifier = "not_a_pid";
    config.maxMemory = 1073741824;
    auto ret = mgr.SetProcMemConfig(config);
    EXPECT_EQ(ret, UBSE_ERR_INVALID_ARG);
}

TEST_F(TestProcessMemPidInfoManager, SetProcMemConfigNonExistentPid)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    def::ProcessMemNewConfigInfo config{};
    config.isPid = true;
    config.identifier = "99999999";
    config.maxMemory = 1073741824;
    auto ret = mgr.SetProcMemConfig(config);
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);
}

TEST_F(TestProcessMemPidInfoManager, SetProcMemConfigPidModeSuccess)
{
    pid_t testPid = 0;
    if (!GetTestPid(testPid)) {
        GTEST_SKIP() << "no non-root process available";
    }
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    def::ProcessMemNewConfigInfo config{};
    config.isPid = true;
    config.identifier = std::to_string(testPid);
    config.maxMemory = 1073741824;
    config.remoteRatio = 0.5;
    auto ret = mgr.SetProcMemConfig(config);
    EXPECT_EQ(ret, UBSE_OK);

    auto stored = mgr.GetProcMemConfig(true, config.identifier);
    EXPECT_TRUE(stored.isPid);
    EXPECT_EQ(stored.identifier, config.identifier);
    EXPECT_EQ(stored.maxMemory, 1073741824u);
    EXPECT_DOUBLE_EQ(stored.remoteRatio, 0.5);
    EXPECT_GT(stored.startTime, 0u);

    EXPECT_EQ(mgr.RemoveProcMemConfig(true, config.identifier), UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, SetProcMemConfigPidModeUpdateExisting)
{
    pid_t testPid = 0;
    if (!GetTestPid(testPid)) {
        GTEST_SKIP() << "no non-root process available";
    }
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    def::ProcessMemNewConfigInfo config{};
    config.isPid = true;
    config.identifier = std::to_string(testPid);
    config.maxMemory = 1073741824;
    config.remoteRatio = 0.5;
    ASSERT_EQ(mgr.SetProcMemConfig(config), UBSE_OK);

    config.maxMemory = 2147483648;
    config.remoteRatio = 0.7;
    auto ret = mgr.SetProcMemConfig(config);
    EXPECT_EQ(ret, UBSE_OK);

    auto stored = mgr.GetProcMemConfig(true, config.identifier);
    EXPECT_EQ(stored.maxMemory, 2147483648u);
    EXPECT_DOUBLE_EQ(stored.remoteRatio, 0.7);

    EXPECT_EQ(mgr.RemoveProcMemConfig(true, config.identifier), UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, SetProcMemConfigNameModeSuccess)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    def::ProcessMemNewConfigInfo config{};
    config.isPid = false;
    config.identifier = "testproc_name";
    config.maxMemory = 2147483648;
    config.remoteRatio = 0.3;
    auto ret = mgr.SetProcMemConfig(config);
    EXPECT_EQ(ret, UBSE_OK);

    auto stored = mgr.GetProcMemConfig(false, config.identifier);
    EXPECT_FALSE(stored.isPid);
    EXPECT_EQ(stored.identifier, config.identifier);
    EXPECT_EQ(stored.maxMemory, 2147483648u);
    EXPECT_DOUBLE_EQ(stored.remoteRatio, 0.3);

    EXPECT_EQ(mgr.RemoveProcMemConfig(false, config.identifier), UBSE_OK);
}

TEST_F(TestProcessMemPidInfoManager, SetProcMemConfigPersistFailure)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    ubse::storage::MockSetStoragePutError(UBSE_ERROR);

    def::ProcessMemNewConfigInfo config{};
    config.isPid = false;
    config.identifier = "persist_fail";
    config.maxMemory = 1073741824;
    auto ret = mgr.SetProcMemConfig(config);
    EXPECT_EQ(ret, UBSE_ERROR);

    auto stored = mgr.GetProcMemConfig(false, config.identifier);
    EXPECT_EQ(stored.maxMemory, 0u);
}

TEST_F(TestProcessMemPidInfoManager, RemoveProcMemConfigNotExist)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    auto ret = mgr.RemoveProcMemConfig(true, "99999999");
    EXPECT_EQ(ret, UBSE_ERR_NOT_EXIST);
}

TEST_F(TestProcessMemPidInfoManager, RemoveProcMemConfigNameModeSuccess)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    def::ProcessMemNewConfigInfo config{};
    config.isPid = false;
    config.identifier = "remove_me";
    config.maxMemory = 1073741824;
    ASSERT_EQ(mgr.SetProcMemConfig(config), UBSE_OK);

    EXPECT_EQ(mgr.RemoveProcMemConfig(false, config.identifier), UBSE_OK);
    auto stored = mgr.GetProcMemConfig(false, config.identifier);
    EXPECT_EQ(stored.maxMemory, 0u);
}

TEST_F(TestProcessMemPidInfoManager, RemoveProcMemConfigNameSkipsExplicitPidConfig)
{
    pid_t testPid = 0;
    if (!GetTestPid(testPid)) {
        GTEST_SKIP() << "no non-root process available";
    }
    std::string comm = ReadProcComm(testPid);
    if (comm.empty()) {
        GTEST_SKIP() << "cannot read /proc/<pid>/comm";
    }
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    def::ProcessMemNewConfigInfo pidConfig{};
    pidConfig.isPid = true;
    pidConfig.identifier = std::to_string(testPid);
    pidConfig.maxMemory = 1073741824;
    ASSERT_EQ(mgr.SetProcMemConfig(pidConfig), UBSE_OK);

    def::ProcessMemNewConfigInfo nameConfig{};
    nameConfig.isPid = false;
    nameConfig.identifier = comm;
    nameConfig.maxMemory = 2147483648;
    ASSERT_EQ(mgr.SetProcMemConfig(nameConfig), UBSE_OK);

    ubse::mem::controller::MockSetDebtInfos({MakeDebtInfo("debt-self", static_cast<int32_t>(testPid))});
    std::vector<std::string> freedDebts;
    ::process_mem::pid::bridge::ProcessMemPidBridge::rmrsFreeWithMigrate =
        [&freedDebts](const std::string& name) -> uint32_t {
        freedDebts.push_back(name);
        return UBSE_OK;
    };

    EXPECT_EQ(mgr.RemoveProcMemConfig(false, comm), UBSE_OK);

    EXPECT_TRUE(freedDebts.empty());
    EXPECT_EQ(mgr.GetProcMemConfig(true, std::to_string(testPid)).maxMemory, 1073741824u);

    ::process_mem::pid::bridge::ProcessMemPidBridge::rmrsFreeWithMigrate = [](const std::string&) -> uint32_t {
        return UBSE_OK;
    };
    ubse::mem::controller::MockClearDebtInfos();
}

TEST_F(TestProcessMemPidInfoManager, RebuildRestoresFossilWithName)
{
    pid_t testPid = 0;
    if (!GetTestPid(testPid)) {
        GTEST_SKIP() << "no non-root process available";
    }
    auto startTime = ProcessMemPidConfigManager::GetExactStartTime(testPid);
    ASSERT_NE(startTime, 0u);

    def::FossilPidConfigInfo fossil{};
    fossil.name = "testproc";
    fossil.maxMemory = 1073741824;
    fossil.remoteRatio = 0.5;
    fossil.startTime = static_cast<uint64_t>(startTime);
    ubse::storage::MockSetStorageKeys({std::to_string(testPid)});
    ubse::storage::MockSetStorageQueryPayload(def::PROC_MEM_FOSSIL_KEY_PREFIX, std::to_string(testPid),
                                              SerializeFossil(fossil));

    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    mgr.RefreshProcMemConfigCache();
    mgr.RebuildManagedPidCache();

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(testPid);
    ASSERT_NE(it, snapshot.end());
    EXPECT_TRUE(it->second.sources & static_cast<uint8_t>(ConfigSource::NAME_CONFIG));
    EXPECT_FALSE(it->second.sources & static_cast<uint8_t>(ConfigSource::PID_CONFIG));
    EXPECT_EQ(it->second.nameConfigName, "testproc");
    EXPECT_EQ(it->second.maxMemory, 1073741824u);
    EXPECT_DOUBLE_EQ(it->second.remoteRatio, 0.5);
}

TEST_F(TestProcessMemPidInfoManager, GetAllProcMemConfigsWithEntries)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    def::ProcessMemNewConfigInfo config1{};
    config1.isPid = false;
    config1.identifier = "name_a";
    config1.maxMemory = 1073741824;
    def::ProcessMemNewConfigInfo config2{};
    config2.isPid = false;
    config2.identifier = "name_b";
    config2.maxMemory = 2147483648;
    ASSERT_EQ(mgr.SetProcMemConfig(config1), UBSE_OK);
    ASSERT_EQ(mgr.SetProcMemConfig(config2), UBSE_OK);

    std::vector<def::ProcessMemNewConfigInfo> configs;
    mgr.GetAllProcMemConfigs(configs);
    ASSERT_EQ(configs.size(), 2u);
    EXPECT_EQ(configs[0].identifier, "name_a");
    EXPECT_EQ(configs[1].identifier, "name_b");

    mgr.RemoveProcMemConfig(false, "name_a");
    mgr.RemoveProcMemConfig(false, "name_b");
}

TEST_F(TestProcessMemPidInfoManager, GetProcMemConfigNotExist)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    auto stored = mgr.GetProcMemConfig(true, "77777");
    EXPECT_EQ(stored.maxMemory, 0u);
    EXPECT_TRUE(stored.identifier.empty());
}

TEST_F(TestProcessMemPidInfoManager, AddNameSourceToManagedPidNewEntry)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    pid_t pid = getpid();
    mgr.AddNameSourceToManagedPid(pid, "myname", 1024000, 0.5);

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(pid);
    ASSERT_NE(it, snapshot.end());
    EXPECT_TRUE(it->second.sources & static_cast<uint8_t>(ConfigSource::NAME_CONFIG));
    EXPECT_FALSE(it->second.sources & static_cast<uint8_t>(ConfigSource::PID_CONFIG));
    EXPECT_EQ(it->second.nameConfigName, "myname");
    EXPECT_EQ(it->second.maxMemory, 1024000u);
    EXPECT_DOUBLE_EQ(it->second.remoteRatio, 0.5);
}

TEST_F(TestProcessMemPidInfoManager, AddNameSourceToManagedPidKeepsPidValues)
{
    pid_t testPid = 0;
    if (!GetTestPid(testPid)) {
        GTEST_SKIP() << "no non-root process available";
    }
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    def::ProcessMemNewConfigInfo config{};
    config.isPid = true;
    config.identifier = std::to_string(testPid);
    config.maxMemory = 1073741824;
    config.remoteRatio = 0.5;
    ASSERT_EQ(mgr.SetProcMemConfig(config), UBSE_OK);

    mgr.AddNameSourceToManagedPid(testPid, "overlap_name", 999999, 0.9);

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(testPid);
    ASSERT_NE(it, snapshot.end());
    EXPECT_TRUE(it->second.sources & static_cast<uint8_t>(ConfigSource::PID_CONFIG));
    EXPECT_TRUE(it->second.sources & static_cast<uint8_t>(ConfigSource::NAME_CONFIG));
    EXPECT_EQ(it->second.nameConfigName, "overlap_name");
    EXPECT_EQ(it->second.maxMemory, 1073741824u);
    EXPECT_DOUBLE_EQ(it->second.remoteRatio, 0.5);
}

TEST_F(TestProcessMemPidInfoManager, AddChildSourceToManagedPidNewEntry)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    pid_t childPid = getpid();
    pid_t parentPid = 1;
    mgr.AddChildSourceToManagedPid(childPid, parentPid, 1024000, 0.5);

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(childPid);
    ASSERT_NE(it, snapshot.end());
    EXPECT_TRUE(it->second.isChild);
    EXPECT_EQ(it->second.parentPid, parentPid);
    EXPECT_EQ(it->second.maxMemory, 1024000u);
    EXPECT_DOUBLE_EQ(it->second.remoteRatio, 0.5);
}

TEST_F(TestProcessMemPidInfoManager, AddChildSourceToManagedPidExistingFollowsParent)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    pid_t childPid = getpid();
    mgr.AddChildSourceToManagedPid(childPid, 1, 1024000, 0.5);
    mgr.AddChildSourceToManagedPid(childPid, 2, 2048000, 0.8);

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(childPid);
    ASSERT_NE(it, snapshot.end());
    EXPECT_TRUE(it->second.isChild);
    EXPECT_EQ(it->second.parentPid, 2);
    EXPECT_EQ(it->second.maxMemory, 2048000u);
    EXPECT_DOUBLE_EQ(it->second.remoteRatio, 0.8);
}

TEST_F(TestProcessMemPidInfoManager, RemoveManagedPidEntrySuccess)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    pid_t pid = getpid();
    mgr.AddNameSourceToManagedPid(pid, "rm_entry", 1024000, 0.5);
    EXPECT_NE(mgr.GetManagedPidCacheSnapshot().find(pid), mgr.GetManagedPidCacheSnapshot().end());

    mgr.RemoveManagedPidEntry(pid);
    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    EXPECT_EQ(snapshot.find(pid), snapshot.end());
}

TEST_F(TestProcessMemPidInfoManager, RemovePidSourceFromManagedPidNoSourcesErases)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    pid_t pid = getpid();
    mgr.AddChildSourceToManagedPid(pid, 1, 1024000, 0.5);
    EXPECT_NE(mgr.GetManagedPidCacheSnapshot().find(pid), mgr.GetManagedPidCacheSnapshot().end());

    mgr.RemovePidSourceFromManagedPid(pid);
    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    EXPECT_EQ(snapshot.find(pid), snapshot.end());
}

TEST_F(TestProcessMemPidInfoManager, RemovePidSourceFromManagedPidRecalcFromNameConfig)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    def::ProcessMemNewConfigInfo config{};
    config.isPid = false;
    config.identifier = "recalc_name";
    config.maxMemory = 2147483648;
    config.remoteRatio = 0.7;
    ASSERT_EQ(mgr.SetProcMemConfig(config), UBSE_OK);

    pid_t pid = getpid();
    mgr.AddNameSourceToManagedPid(pid, "recalc_name", 1073741824, 0.5);

    mgr.RemovePidSourceFromManagedPid(pid);

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(pid);
    ASSERT_NE(it, snapshot.end());
    EXPECT_TRUE(it->second.sources & static_cast<uint8_t>(ConfigSource::NAME_CONFIG));
    EXPECT_EQ(it->second.maxMemory, 2147483648u);
    EXPECT_DOUBLE_EQ(it->second.remoteRatio, 0.7);
}

TEST_F(TestProcessMemPidInfoManager, UpdateManagedPidVmRssBatch)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    pid_t pid = getpid();
    mgr.AddNameSourceToManagedPid(pid, "vmrss", 1024000, 0.5);

    collect::PidCollectInfoMap collectInfo;
    collectInfo.entries[pid] = {100, false};
    mgr.UpdateManagedPidVmRssBatch(collectInfo);

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(pid);
    ASSERT_NE(it, snapshot.end());
    EXPECT_EQ(it->second.vmRss, 100u * 1024);
}

TEST_F(TestProcessMemPidInfoManager, UpdateManagedPidBorrowStateAndStatus)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    pid_t pid = getpid();
    mgr.AddNameSourceToManagedPid(pid, "borrow", 1024000, 0.5);

    def::BorrowState borrow;
    borrow.currentRemote = 512000;
    mgr.UpdateManagedPidBorrowState(pid, borrow, ProcessStatus::BORROWING);

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(pid);
    ASSERT_NE(it, snapshot.end());
    EXPECT_EQ(it->second.borrow.currentRemote, 512000u);
    EXPECT_EQ(it->second.processStatus, ProcessStatus::BORROWING);

    mgr.UpdateManagedPidStatus(pid, ProcessStatus::BORROWED);
    snapshot = mgr.GetManagedPidCacheSnapshot();
    EXPECT_EQ(snapshot.find(pid)->second.processStatus, ProcessStatus::BORROWED);
}

TEST_F(TestProcessMemPidInfoManager, UpdateManagedPidLastMigrateTime)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    pid_t pid = getpid();
    mgr.AddNameSourceToManagedPid(pid, "migrate", 1024000, 0.5);

    mgr.UpdateManagedPidLastMigrateTime(pid);
    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(pid);
    ASSERT_NE(it, snapshot.end());
    EXPECT_GT(it->second.lastMigrateTime.time_since_epoch().count(), 0);
}

TEST_F(TestProcessMemPidInfoManager, GetManagedPidCacheSnapshotEmpty)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    EXPECT_TRUE(snapshot.empty());
}

TEST_F(TestProcessMemPidInfoManager, VmRssCheckCallBackUpdatesVmRss)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    pid_t pid = getpid();
    mgr.AddNameSourceToManagedPid(pid, "callback", 1024000, 0.0);

    collect::PidCollectInfoMap collectInfo;
    collectInfo.entries[pid] = {200, false};
    EXPECT_NO_THROW(mgr.VmRssCheckCallBack(collectInfo));

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(pid);
    ASSERT_NE(it, snapshot.end());
    EXPECT_EQ(it->second.vmRss, 200u * 1024);
}

TEST_F(TestProcessMemPidInfoManager, RebalanceRemoteCheckNoRemote)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    pid_t pid = getpid();
    mgr.AddNameSourceToManagedPid(pid, "no_remote", 1024000, 0.5);
    EXPECT_NO_THROW(mgr.RebalanceRemoteCheck());
}

TEST_F(TestProcessMemPidInfoManager, RebalanceRemoteCheckUnderMigratedNoAction)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    pid_t pid = getpid();
    mgr.AddNameSourceToManagedPid(pid, "migrate_out", 1024000, 0.5);

    collect::PidCollectInfoMap collectInfo;
    collectInfo.entries[pid] = {4000, false};
    mgr.UpdateManagedPidVmRssBatch(collectInfo);

    def::BorrowState borrow;
    borrow.currentRemote = 1024000;
    mgr.UpdateManagedPidBorrowState(pid, borrow, ProcessStatus::BORROWED);

    mgr.RebalanceRemoteCheck();

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(pid);
    ASSERT_NE(it, snapshot.end());
    EXPECT_EQ(it->second.borrow.currentRemote, 1024000u);
    EXPECT_EQ(it->second.lastMigrateTime.time_since_epoch().count(), 0u);
}

TEST_F(TestProcessMemPidInfoManager, RebalanceRemoteCheckZeroRatioSkips)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    pid_t pid = getpid();
    mgr.AddNameSourceToManagedPid(pid, "zero_ratio", 1024000, 0.0);

    collect::PidCollectInfoMap collectInfo;
    collectInfo.entries[pid] = {4000, false};
    mgr.UpdateManagedPidVmRssBatch(collectInfo);

    def::BorrowState borrow;
    borrow.currentRemote = 1024000;
    mgr.UpdateManagedPidBorrowState(pid, borrow, ProcessStatus::BORROWED);

    mgr.RebalanceRemoteCheck();

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    EXPECT_EQ(snapshot.find(pid)->second.borrow.currentRemote, 1024000u);
}

TEST_F(TestProcessMemPidInfoManager, RebalanceRemoteCheckSubPageOverageSkipsCommand)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    const pid_t pid = 7301;
    mgr.AddNameSourceToManagedPid(pid, "subpage_overage", 1024000, 1.0);

    def::BorrowSlot slot;
    slot.capacity = 148434944;
    slot.migratedBytes = 148434944;
    slot.debtId = "d1";
    slot.remoteNumaId = 3;
    slot.status = def::BorrowSlotStatus::COMPLETED;
    def::BorrowState borrow;
    borrow.currentRemote = 148434944;
    borrow.slots.push_back(slot);
    mgr.UpdateManagedPidBorrowState(pid, borrow, ProcessStatus::BORROWED);

    collect::PidCollectInfoMap collectInfo;
    collectInfo.entries[pid] = {148432896 / 1024, false};
    mgr.UpdateManagedPidVmRssBatch(collectInfo);

    auto old = ::process_mem::pid::bridge::ProcessMemPidBridge::rmrsMigrateOut;
    std::vector<uint64_t> migrateTargetsKb;
    ::process_mem::pid::bridge::ProcessMemPidBridge::rmrsMigrateOut =
        [&migrateTargetsKb](const std::vector<mempooling::smap::MigrateOutPayload>& payloads, int) -> int {
        for (const auto& payload : payloads) {
            for (uint16_t i = 0; i < payload.count; ++i) {
                migrateTargetsKb.push_back(payload.inner[i].memSize);
            }
        }
        return 0;
    };

    mgr.RebalanceRemoteCheck();

    EXPECT_TRUE(migrateTargetsKb.empty());
    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    EXPECT_EQ(snapshot.at(pid).borrow.currentRemote, 148434944u);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].migratedBytes, 148434944u);

    ::process_mem::pid::bridge::ProcessMemPidBridge::rmrsMigrateOut = old;
}

TEST_F(TestProcessMemPidInfoManager, RebalanceRemoteCheckAlignsLedgerToPage)
{
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    const pid_t pid = 7301;
    mgr.AddNameSourceToManagedPid(pid, "align_ledger", 1024000, 1.0);

    def::BorrowSlot slot;
    slot.capacity = 148434944;
    slot.migratedBytes = 148434944;
    slot.debtId = "d1";
    slot.remoteNumaId = 3;
    slot.status = def::BorrowSlotStatus::COMPLETED;
    def::BorrowState borrow;
    borrow.currentRemote = 148434944;
    borrow.slots.push_back(slot);
    mgr.UpdateManagedPidBorrowState(pid, borrow, ProcessStatus::BORROWED);

    collect::PidCollectInfoMap collectInfo;
    collectInfo.entries[pid] = {(148434944 - 5000) / 1024, false};
    mgr.UpdateManagedPidVmRssBatch(collectInfo);

    auto old = ::process_mem::pid::bridge::ProcessMemPidBridge::rmrsMigrateOut;
    std::vector<uint64_t> migrateTargetsKb;
    ::process_mem::pid::bridge::ProcessMemPidBridge::rmrsMigrateOut =
        [&migrateTargetsKb](const std::vector<mempooling::smap::MigrateOutPayload>& payloads, int) -> int {
        for (const auto& payload : payloads) {
            for (uint16_t i = 0; i < payload.count; ++i) {
                migrateTargetsKb.push_back(payload.inner[i].memSize);
            }
        }
        return 0;
    };

    mgr.RebalanceRemoteCheck();

    ASSERT_EQ(migrateTargetsKb.size(), 1u);
    EXPECT_EQ(migrateTargetsKb[0], 144952u);
    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    EXPECT_EQ(snapshot.at(pid).borrow.currentRemote, 148430848u);
    EXPECT_EQ(snapshot.at(pid).borrow.slots[0].migratedBytes, 148430848u);

    mgr.RebalanceRemoteCheck();
    EXPECT_EQ(migrateTargetsKb.size(), 1u);

    ::process_mem::pid::bridge::ProcessMemPidBridge::rmrsMigrateOut = old;
}

TEST_F(TestProcessMemPidInfoManager, RebuildManagedPidCacheWithNameConfig)
{
    ubse::config::ScopedRootFilterDisabled rootFilterOff;
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    std::string comm = ReadSelfComm();
    ASSERT_FALSE(comm.empty());

    def::ProcessMemNewConfigInfo config{};
    config.isPid = false;
    config.identifier = comm;
    config.maxMemory = 1073741824;
    config.remoteRatio = 0.3;
    ASSERT_EQ(mgr.SetProcMemConfig(config), UBSE_OK);

    EXPECT_NO_THROW(mgr.RebuildManagedPidCache());

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(getpid());
    ASSERT_NE(it, snapshot.end());
    EXPECT_TRUE(it->second.sources & static_cast<uint8_t>(ConfigSource::NAME_CONFIG));
    EXPECT_EQ(it->second.nameConfigName, comm);
    EXPECT_EQ(it->second.maxMemory, 1073741824u);
    EXPECT_DOUBLE_EQ(it->second.remoteRatio, 0.3);
}

TEST_F(TestProcessMemPidInfoManager, RebuildManagedPidCacheWithPidConfig)
{
    pid_t testPid = 0;
    if (!GetTestPid(testPid)) {
        GTEST_SKIP() << "no non-root process available";
    }
    auto& mgr = ProcessMemPidInfoManager::GetInstance();
    def::ProcessMemNewConfigInfo config{};
    config.isPid = true;
    config.identifier = std::to_string(testPid);
    config.maxMemory = 4294967296;
    config.remoteRatio = 0.8;
    ASSERT_EQ(mgr.SetProcMemConfig(config), UBSE_OK);

    EXPECT_NO_THROW(mgr.RebuildManagedPidCache());

    auto snapshot = mgr.GetManagedPidCacheSnapshot();
    auto it = snapshot.find(testPid);
    ASSERT_NE(it, snapshot.end());
    EXPECT_TRUE(it->second.sources & static_cast<uint8_t>(ConfigSource::PID_CONFIG));
    EXPECT_EQ(it->second.maxMemory, 4294967296u);
    EXPECT_DOUBLE_EQ(it->second.remoteRatio, 0.8);
}

} // namespace ubse::ut::process_mem
