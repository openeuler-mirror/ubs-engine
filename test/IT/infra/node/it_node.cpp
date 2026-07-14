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

#include "it_node.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

#include "ubse_error.h"
#include "it_console_log.h"

namespace ubse::it::infra {

namespace {
constexpr uint32_t DEFAULT_NUMA_COUNT = 2;
constexpr uint32_t DEFAULT_CPUS_PER_NUMA = 8;
// socket ID for each NUMA node (real hardware: 36 for socket0, 236 for socket1)
constexpr uint32_t SOCKET_IDS[] = {36, 236};

void WriteFile(const std::string& path, const std::string& content)
{
    auto parentDir = std::filesystem::path(path).parent_path();
    std::filesystem::create_directories(parentDir);
    std::ofstream out(path);
    if (!out.is_open()) {
        IT_LOG_WARN << "Failed to create sysfs file: " << path;
        return;
    }
    out << content;
}

std::string BuildMeminfo(uint32_t numa)
{
    // Real data from tongsuan 1D environment
    const uint64_t memTotalKb[] = {65795532, 65502204};
    const uint64_t memFreeKb[] = {61381892, 61743596};
    const uint64_t memUsedKb[] = {4413640, 3758608};
    const uint64_t fileHugePagesKb[] = {6144, 18432};
    const uint64_t filePmdMappedKb[] = {6144, 4096};
    const uint64_t slabKb[] = {218384, 307076};
    const uint64_t sReclaimableKb[] = {56460, 136956};
    const uint64_t sUnreclaimKb[] = {161924, 170120};

    std::ostringstream oss;
    oss << "Node " << numa << " MemTotal:\t\t" << memTotalKb[numa] << " kB\n"
        << "Node " << numa << " MemFree:\t\t" << memFreeKb[numa] << " kB\n"
        << "Node " << numa << " MemUsed:\t\t" << memUsedKb[numa] << " kB\n"
        << "Node " << numa << " SwapCached:\t\t\t0 kB\n"
        << "Node " << numa << " Active:\t\t\t443144 kB\n"
        << "Node " << numa << " Inactive:\t\t\t2118908 kB\n"
        << "Node " << numa << " Active(anon):\t\t123556 kB\n"
        << "Node " << numa << " Inactive(anon):\t\t319136 kB\n"
        << "Node " << numa << " Active(file):\t\t319588 kB\n"
        << "Node " << numa << " Inactive(file):\t\t1799772 kB\n"
        << "Node " << numa << " Unevictable:\t\t0 kB\n"
        << "Node " << numa << " Mlocked:\t\t\t0 kB\n"
        << "Node " << numa << " Dirty:\t\t\t\t0 kB\n"
        << "Node " << numa << " Writeback:\t\t\t0 kB\n"
        << "Node " << numa << " FilePages:\t\t\t2381944 kB\n"
        << "Node " << numa << " Mapped:\t\t\t209604 kB\n"
        << "Node " << numa << " AnonPages:\t\t183512 kB\n"
        << "Node " << numa << " Shmem:\t\t\t259180 kB\n"
        << "Node " << numa << " KernelStack:\t\t7704 kB\n"
        << "Node " << numa << " PageTables:\t\t5396 kB\n"
        << "Node " << numa << " SecPageTables:\t\t0 kB\n"
        << "Node " << numa << " NFS_Unstable:\t\t0 kB\n"
        << "Node " << numa << " Bounce:\t\t\t0 kB\n"
        << "Node " << numa << " WritebackTmp:\t\t0 kB\n"
        << "Node " << numa << " KReclaimable:\t\t" << sReclaimableKb[numa] << " kB\n"
        << "Node " << numa << " Slab:\t\t\t" << slabKb[numa] << " kB\n"
        << "Node " << numa << " SReclaimable:\t\t" << sReclaimableKb[numa] << " kB\n"
        << "Node " << numa << " SUnreclaim:\t\t" << sUnreclaimKb[numa] << " kB\n"
        << "Node " << numa << " AnonHugePages:\t\t0 kB\n"
        << "Node " << numa << " ShmemHugePages:\t\t0 kB\n"
        << "Node " << numa << " ShmemPmdMapped:\t\t0 kB\n"
        << "Node " << numa << " FileHugePages:\t\t" << fileHugePagesKb[numa] << " kB\n"
        << "Node " << numa << " FilePmdMapped:\t\t" << filePmdMappedKb[numa] << " kB\n"
        << "Node " << numa << " HugePages_Total:\t\t0\n"
        << "Node " << numa << " HugePages_Free:\t\t0\n"
        << "Node " << numa << " HugePages_Surp:\t\t0\n"
        << "Node " << numa << " RemoteMemPreonline:\t0 kB\n";
    return oss.str();
}
} // namespace

ItNode::ItNode(NodeSpec spec, ClusterContext ctx)
    : spec_(std::move(spec)),
      ctx_(std::move(ctx)),
      lcneUdsPath_(spec_.workDir + "/run/ubm/socket/ubm_nuds/restconf.sock"),
      udsSocketPath_(spec_.UdsSocketPath()),
      cliInvoker_(ctx_.cliBinaryPath, udsSocketPath_)
{
}

ItNode::~ItNode()
{
    Stop();
}

void ItNode::CreateWorkDirectories()
{
    std::filesystem::create_directories(spec_.workDir);
    std::filesystem::create_directories(spec_.RunDir());
    std::filesystem::create_directories(spec_.LogDir());
    std::filesystem::create_directories(spec_.workDir + "/run/ubm/socket/ubm_nuds");
    CreateSysfsTree();
}

void ItNode::CreateSysfsTree()
{
    const std::string base = spec_.workDir + "/sysfs";

    // /sys/devices/system/node/has_cpu -> "0-1"
    WriteFile(base + "/sys/devices/system/node/has_cpu", "0-1");

    for (uint32_t numa = 0; numa < DEFAULT_NUMA_COUNT; numa++) {
        const std::string nodeDir = base + "/sys/devices/system/node/node" + std::to_string(numa);

        // cpulist
        uint32_t cpuStart = numa * DEFAULT_CPUS_PER_NUMA;
        uint32_t cpuEnd = cpuStart + DEFAULT_CPUS_PER_NUMA - 1;
        WriteFile(nodeDir + "/cpulist", std::to_string(cpuStart) + "-" + std::to_string(cpuEnd));

        // meminfo (full format from real hardware)
        WriteFile(nodeDir + "/meminfo", BuildMeminfo(numa));

        // hugepages - 64kB
        const std::string hp64k = nodeDir + "/hugepages/hugepages-64kB";
        WriteFile(hp64k + "/nr_hugepages", "0");
        WriteFile(hp64k + "/free_hugepages", "0");

        // hugepages - 2M
        const std::string hp2m = nodeDir + "/hugepages/hugepages-2048kB";
        WriteFile(hp2m + "/nr_hugepages", "0");
        WriteFile(hp2m + "/free_hugepages", "0");

        // hugepages - 32M
        const std::string hp32m = nodeDir + "/hugepages/hugepages-32768kB";
        WriteFile(hp32m + "/nr_hugepages", "0");
        WriteFile(hp32m + "/free_hugepages", "0");

        // hugepages - 1G
        const std::string hp1g = nodeDir + "/hugepages/hugepages-1048576kB";
        WriteFile(hp1g + "/nr_hugepages", "0");
        WriteFile(hp1g + "/free_hugepages", "0");

        // OBMM mempool (real hex values)
        const std::string obmm = base + "/sys/kernel/obmm_mempool/obmm-" + std::to_string(numa);
        WriteFile(obmm + "/total", "0x20000000");
        WriteFile(obmm + "/used", "0x0");
        WriteFile(obmm + "/available_cleared", "0x20000000");
        WriteFile(obmm + "/available_uncleared", "0x0");
    }

    // CPU topology: physical_package_id (real hardware socket IDs: 36 for node0, 236 for node1)
    for (uint32_t numa = 0; numa < DEFAULT_NUMA_COUNT; numa++) {
        for (uint32_t c = 0; c < DEFAULT_CPUS_PER_NUMA; c++) {
            uint32_t cpuId = numa * DEFAULT_CPUS_PER_NUMA + c;
            const std::string topDir = base + "/sys/devices/system/cpu/cpu" + std::to_string(cpuId) + "/topology";
            WriteFile(topDir + "/physical_package_id", std::to_string(SOCKET_IDS[numa]));
        }
    }

    // /proc/net/fib_trie (empty - IP collection will fail gracefully)
    WriteFile(base + "/proc/net/fib_trie", "");
}

NodeProcessConfig ItNode::BuildProcessConfig() const
{
    NodeProcessConfig config;
    config.binaryPath = ctx_.binaryPath;
    config.nodeId = spec_.nodeId;
    config.nodeIp = spec_.ip;
    config.workDir = spec_.workDir;
    config.slotId = spec_.slotId;
    config.stubLibDir = ctx_.stubLibDir;
    config.clusterNodeIds = ctx_.joinedNodeIds;
    config.clusterIps = ctx_.joinedClusterIps;
    config.clusterSlotIds = ctx_.clusterSlotIds;
    config.sceneType = ctx_.sceneType;
    config.meshType = ctx_.meshType;
    config.lcneUdsPath = lcneUdsPath_;
    return config;
}

UbseResult ItNode::Start()
{
    CreateWorkDirectories();

    // Start mock LCNE server (daemon connects to it on startup)
    mockLcneServer_ = std::make_unique<MockLcneServer>(lcneUdsPath_, spec_.slotId, ctx_.clusterSlotIds);
    UbseResult ret = mockLcneServer_->Start();
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Failed to start MockLcneServer for node " << spec_.nodeId;
        mockLcneServer_.reset();
        return ret;
    }
    ret = mockLcneServer_->WaitForReady(5000);
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "MockLcneServer not ready within 5s for node " << spec_.nodeId;
        mockLcneServer_->Stop();
        mockLcneServer_.reset();
        return ret;
    }

    // Start daemon process
    auto procConfig = BuildProcessConfig();
    process_ = std::make_unique<NodeProcessManager>(std::move(procConfig));
    ret = process_->Start();
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Failed to start node " << spec_.nodeId;
        process_.reset();
        mockLcneServer_->Stop();
        mockLcneServer_.reset();
        return ret;
    }

    IT_LOG_INFO << "Node " << spec_.nodeId << " started";
    return UBSE_OK;
}

UbseResult ItNode::Stop()
{
    FinalizeSdkClient();
    sdkClient_.reset();

    if (process_) {
        process_->Stop();
        process_.reset();
    }

    if (mockLcneServer_) {
        mockLcneServer_->Stop();
        mockLcneServer_.reset();
    }

    // Clean up LCNE UDS socket
    if (!lcneUdsPath_.empty()) {
        unlink(lcneUdsPath_.c_str());
    }

    return UBSE_OK;
}

UbseResult ItNode::Kill()
{
    FinalizeSdkClient();

    if (!process_) {
        return UBSE_OK;
    }
    return process_->Kill();
}

UbseResult ItNode::Restart(uint32_t startupTimeoutMs)
{
    if (process_) {
        process_->Stop();
    }

    auto procConfig = BuildProcessConfig();
    process_ = std::make_unique<NodeProcessManager>(std::move(procConfig));
    UbseResult ret = process_->Start();
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Failed to restart node " << spec_.nodeId;
        return ret;
    }

    ret = process_->WaitForStartup(startupTimeoutMs);
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Node " << spec_.nodeId << " did not start up in time after restart";
        process_->Stop();
        return ret;
    }

    IT_LOG_INFO << "Node " << spec_.nodeId << " restarted";
    return UBSE_OK;
}

UbseResult ItNode::WaitForStartup(uint32_t timeoutMs)
{
    if (!process_) {
        IT_LOG_ERROR << "Node " << spec_.nodeId << " has no process to wait for";
        return UBSE_ERROR_DEF(1);
    }
    return process_->WaitForStartup(timeoutMs);
}

bool ItNode::IsRunning() const
{
    if (!process_) {
        return false;
    }
    return process_->IsRunning();
}

ItSdkClient& ItNode::GetSdkClient()
{
    if (!sdkClient_) {
        sdkClient_ = std::make_unique<ItSdkClient>(udsSocketPath_, spec_.LogDir());
    }
    return *sdkClient_;
}

ItCliInvoker& ItNode::GetCliInvoker()
{
    return cliInvoker_;
}

ItLcneClient& ItNode::GetLcneClient()
{
    if (!lcneClient_) {
        lcneClient_ = std::make_unique<ItLcneClient>(lcneUdsPath_, spec_.workDir + "/sysfs");
    }
    return *lcneClient_;
}

UbseResult ItNode::InitializeSdkClient()
{
    auto& client = GetSdkClient();
    if (client.IsInitialized()) {
        return UBSE_OK;
    }
    return client.Initialize();
}

void ItNode::FinalizeSdkClient()
{
    if (sdkClient_) {
        sdkClient_->Finalize();
    }
}

UbseResult ItNode::InjectAlarmEvent(unsigned short alarmId, const std::string& paras)
{
    if (!process_) {
        IT_LOG_ERROR << "Node " << spec_.nodeId << " has no process for alarm injection";
        return UBSE_ERROR_DEF(1);
    }
    return process_->InjectAlarmEvent(alarmId, paras);
}

const std::string& ItNode::GetNodeId() const
{
    return spec_.nodeId;
}

const NodeSpec& ItNode::GetSpec() const
{
    return spec_;
}

const std::string& ItNode::GetUdsSocketPath() const
{
    return udsSocketPath_;
}

const std::string& ItNode::GetXalarmFifoPath() const
{
    if (process_) {
        return process_->GetXalarmFifoPath();
    }
    static const std::string empty;
    return empty;
}

const std::string& ItNode::GetWorkDir() const
{
    if (process_) {
        return process_->GetWorkDir();
    }
    static const std::string empty;
    return empty;
}

const std::string& ItNode::GetLcneUdsPath() const
{
    return lcneUdsPath_;
}

std::string ItNode::GetConfigFilePath() const
{
    // Use spec_.workDir (always available, even before/after the process runs)
    // rather than GetWorkDir(), which depends on the process being started.
    return spec_.workDir + "/ubse.conf";
}

std::string ItNode::GetLogFilePath() const
{
    return spec_.workDir + "/log/ubse.log";
}

} // namespace ubse::it::infra
