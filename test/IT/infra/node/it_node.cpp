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

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
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

constexpr uint64_t UB_MEM_BORROW_NC_MASK = 1ULL << 0;
constexpr uint64_t UB_MEM_BORROW_CC_MASK = 1ULL << 1;
constexpr uint64_t UB_MEM_SHARE_NC_MASK = 1ULL << 2;
constexpr uint64_t UB_MEM_SHARE_CC_MASK = 1ULL << 3;
constexpr uint64_t UB_MEM_ALL_MASK = UB_MEM_BORROW_NC_MASK | UB_MEM_BORROW_CC_MASK | UB_MEM_SHARE_NC_MASK |
                                     UB_MEM_SHARE_CC_MASK;
constexpr uint64_t UB_URMA_ALL_MASK = 0xffULL << 16;
constexpr uint64_t UB_FEATURE_ALL_MASK = UB_MEM_ALL_MASK | UB_URMA_ALL_MASK;

// /sys/bus/ub/ub_feature default content: all UB features enabled.
// Line 1 is the feature bitmask read by UbseConfModule::LoadUbFeature()
// (parsed via std::stoull as uint64_t). Remaining lines are human-readable
// +/- markers. Both are kept in sync by BuildUbFeatureContent().
const std::string UB_FEATURE_DEFAULT_CONTENT = "0x00ffffff\n"
                                               "UB Memory Borrowing(Non Cacheable)+\n"
                                               "UB Memory Borrowing(Cacheable)+\n"
                                               "UB Memory Sharing(Non Cacheable)+\n"
                                               "UB Memory Sharing(Cacheable)+\n"
                                               "URMA RTP-ROI+\nURMA RTP-ROT+\nURMA RTP-ROL+\n"
                                               "URMA CTP-ROI+\nURMA CTP-ROT+\nURMA CTP-ROL+\n"
                                               "URMA CTP-UNO+\nURMA UTP-UNO+\n";

std::string BuildUbFeatureContent(bool borrowDisabled, bool shareNcDisabled, bool shareCcDisabled)
{
    uint64_t mask = UB_FEATURE_ALL_MASK;
    if (borrowDisabled) {
        mask &= ~(UB_MEM_BORROW_NC_MASK | UB_MEM_BORROW_CC_MASK);
    }
    if (shareNcDisabled) {
        mask &= ~UB_MEM_SHARE_NC_MASK;
    }
    if (shareCcDisabled) {
        mask &= ~UB_MEM_SHARE_CC_MASK;
    }

    std::ostringstream oss;
    oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << mask << "\n";
    oss << "UB Memory Borrowing(Non Cacheable)" << (borrowDisabled ? "-" : "+") << "\n";
    oss << "UB Memory Borrowing(Cacheable)" << (borrowDisabled ? "-" : "+") << "\n";
    oss << "UB Memory Sharing(Non Cacheable)" << (shareNcDisabled ? "-" : "+") << "\n";
    oss << "UB Memory Sharing(Cacheable)" << (shareCcDisabled ? "-" : "+") << "\n";
    oss << "URMA RTP-ROI+\nURMA RTP-ROT+\nURMA RTP-ROL+\nURMA CTP-ROI+\n";
    oss << "URMA CTP-ROT+\nURMA CTP-ROL+\nURMA CTP-UNO+\nURMA UTP-UNO+\n";
    return oss.str();
}

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

    // UB feature flags: default to all features enabled.
    // Read by UbseConfModule::LoadUbFeature() from /sys/bus/ub/ub_feature.
    // Path is redirected to here by RedirectSysfsPath() in ubse_interface_preload.cpp.
    WriteFile(base + "/sys/bus/ub/ub_feature", UB_FEATURE_DEFAULT_CONTENT);
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
    InitObmmShm();
    InitComShm();

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

    // UBSE HTTP 服务(UDS) socket 在宿主机上的实际路径:
    // 启动器将 <workDir>/run bind-mount 到 /var/run/ubse, 而 UBSE 监听 UBSE_UBM_UDS_ADDRESS=/var/run/ubse/ubse_ubm.socket
    mockLcneClient_ = std::make_unique<MockLcneClient>(spec_.workDir + "/run/ubse_ubm.socket");

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
    mockLcneClient_.reset();

    // Clean up LCNE UDS socket
    if (!lcneUdsPath_.empty()) {
        unlink(lcneUdsPath_.c_str());
    }

    DestroyObmmShm();
    DestroyComShm();
    return UBSE_OK;
}

UbseResult ItNode::KillUBSE()
{
    FinalizeSdkClient();

    if (!process_) {
        return UBSE_OK;
    }
    return process_->Kill();
}

UbseResult ItNode::StartUBSE(uint32_t startupTimeoutMs)
{
    auto procConfig = BuildProcessConfig();
    process_ = std::make_unique<NodeProcessManager>(std::move(procConfig));
    UbseResult ret = process_->Start();
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Failed to restart node " << spec_.nodeId;
        return ret;
    }

    ret = process_->WaitForStartup(startupTimeoutMs);
    if (ret != UBSE_OK) {
        IT_LOG_ERROR << "Node " << spec_.nodeId << " did not start up in time";
        process_->Stop();
        return ret;
    }

    IT_LOG_INFO << "Node " << spec_.nodeId << " started";
    return UBSE_OK;
}

UbseResult ItNode::StopUBSE()
{
    if (!process_) {
        return UBSE_OK;
    }
    return process_->Stop();
}

UbseResult ItNode::RestartUBSE(uint32_t startupTimeoutMs)
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

std::string ItNode::GetLogFaultFilePath() const
{
    return spec_.workDir + "/log/ubse_fault.log";
}

std::string ItNode::GetUbFeaturePath() const
{
    return spec_.workDir + "/sysfs/sys/bus/ub/ub_feature";
}

void ItNode::SetUbFeatureFault(bool borrowDisabled, bool shareNcDisabled, bool shareCcDisabled)
{
    WriteFile(GetUbFeaturePath(), BuildUbFeatureContent(borrowDisabled, shareNcDisabled, shareCcDisabled));
}

void ItNode::RestoreUbFeature()
{
    WriteFile(GetUbFeaturePath(), UB_FEATURE_DEFAULT_CONTENT);
}

void ItNode::RemoveUbFeatureMock()
{
    const std::string path = GetUbFeaturePath();
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec) {
        IT_LOG_WARN << "Failed to remove mock ub_feature file: " << path << ", ec=" << ec.message();
    }
}

void ItNode::InitObmmShm()
{
    if (obmmCtrl_ != nullptr) {
        return;
    }
    // shm name 与 obmm_stub.cpp 约定: /obmm_stub_<nodeId>
    obmmShmName_ = "/obmm_stub_" + spec_.nodeId;
    obmmShmFd_ = shm_open(obmmShmName_.c_str(), O_CREAT | O_RDWR, 0600);
    if (obmmShmFd_ < 0) {
        IT_LOG_WARN << "Failed to create OBMM shm '" << obmmShmName_ << "': " << strerror(errno);
        obmmShmName_.clear();
        return;
    }
    if (ftruncate(obmmShmFd_, sizeof(ObmmStubControl)) != 0) {
        IT_LOG_WARN << "Failed to truncate OBMM shm: " << strerror(errno);
        close(obmmShmFd_);
        obmmShmFd_ = -1;
        obmmShmName_.clear();
        return;
    }
    void* p = mmap(nullptr, sizeof(ObmmStubControl), PROT_READ | PROT_WRITE, MAP_SHARED, obmmShmFd_, 0);
    if (p == MAP_FAILED) {
        IT_LOG_WARN << "Failed to mmap OBMM shm: " << strerror(errno);
        close(obmmShmFd_);
        obmmShmFd_ = -1;
        obmmShmName_.clear();
        return;
    }
    obmmCtrl_ = static_cast<ObmmStubControl*>(p);
    obmmCtrl_->magic.store(ObmmStubControl::MAGIC, std::memory_order_release);
    obmmCtrl_->failMask.store(0, std::memory_order_relaxed);
    obmmCtrl_->errnoVal.store(ENOMEM, std::memory_order_relaxed);
    for (uint32_t i = 0; i < ObmmStubControl::OP_COUNT; ++i) {
        obmmCtrl_->count[i].store(0, std::memory_order_relaxed);
    }
}

void ItNode::DestroyObmmShm()
{
    if (obmmCtrl_ != nullptr) {
        obmmCtrl_->magic.store(0, std::memory_order_release);
        munmap(obmmCtrl_, sizeof(ObmmStubControl));
        obmmCtrl_ = nullptr;
    }
    if (obmmShmFd_ >= 0) {
        close(obmmShmFd_);
        obmmShmFd_ = -1;
    }
    if (!obmmShmName_.empty()) {
        shm_unlink(obmmShmName_.c_str());
        obmmShmName_.clear();
    }
}

UbseResult ItNode::SetObmmFault(uint32_t failMask, int errnoVal, const uint32_t count[8])
{
    if (obmmCtrl_ == nullptr) {
        IT_LOG_ERROR << "OBMM shm not initialized for node " << spec_.nodeId;
        return UBSE_ERROR_DEF(1);
    }
    obmmCtrl_->errnoVal.store(errnoVal, std::memory_order_relaxed);
    for (uint32_t i = 0; i < ObmmStubControl::OP_COUNT; ++i) {
        uint32_t c = (count != nullptr) ? count[i] : 0;
        obmmCtrl_->count[i].store(c, std::memory_order_relaxed);
    }
    obmmCtrl_->failMask.store(failMask, std::memory_order_release);
    IT_LOG_INFO << "OBMM fault set on node " << spec_.nodeId << ", failMask=0x" << std::hex << failMask
                << ", errno=" << std::dec << errnoVal;
    return UBSE_OK;
}

UbseResult ItNode::RestoreObmmFault()
{
    if (obmmCtrl_ == nullptr) {
        return UBSE_OK;
    }
    obmmCtrl_->failMask.store(0, std::memory_order_release);
    IT_LOG_INFO << "OBMM fault cleared on node " << spec_.nodeId;
    return UBSE_OK;
}

void ItNode::SetOpFailed(ObmmStubControl::OpBit op, bool fail)
{
    if (obmmCtrl_ == nullptr) {
        return;
    }
    uint32_t bit = 1u << op;
    if (fail) {
        obmmCtrl_->errnoVal.store(ENOMEM, std::memory_order_relaxed);
        obmmCtrl_->count[op].store(0, std::memory_order_relaxed);
        obmmCtrl_->failMask.fetch_or(bit, std::memory_order_release);
    } else {
        obmmCtrl_->failMask.fetch_and(~bit, std::memory_order_release);
    }
}

void ItNode::SetOpDelay(ObmmStubControl::OpBit op, uint32_t ms)
{
    if (obmmCtrl_ == nullptr) {
        return;
    }
    obmmCtrl_->delayMs[op].store(ms, std::memory_order_relaxed);
    IT_LOG_INFO << "OBMM delay set on node " << spec_.nodeId << ", op=" << op << ", ms=" << ms;
}

// --- Com (RpcSend) fault injection ---
// Mirrors InitObmmShm/DestroyObmmShm/SetObmmFault/RestoreObmmFault.

void ItNode::InitComShm()
{
    if (comCtrl_ != nullptr) {
        return;
    }
    // shm name 与 ubse_com_engine_it_mock.cpp 约定: /com_stub_<nodeId>
    comShmName_ = "/com_stub_" + spec_.nodeId;
    comShmFd_ = shm_open(comShmName_.c_str(), O_CREAT | O_RDWR, 0600);
    if (comShmFd_ < 0) {
        IT_LOG_WARN << "Failed to create Com shm '" << comShmName_ << "': " << strerror(errno);
        comShmName_.clear();
        return;
    }
    if (ftruncate(comShmFd_, sizeof(ComStubControl)) != 0) {
        IT_LOG_WARN << "Failed to truncate Com shm: " << strerror(errno);
        close(comShmFd_);
        comShmFd_ = -1;
        comShmName_.clear();
        return;
    }
    void* p = mmap(nullptr, sizeof(ComStubControl), PROT_READ | PROT_WRITE, MAP_SHARED, comShmFd_, 0);
    if (p == MAP_FAILED) {
        IT_LOG_WARN << "Failed to mmap Com shm: " << strerror(errno);
        close(comShmFd_);
        comShmFd_ = -1;
        comShmName_.clear();
        return;
    }
    comCtrl_ = static_cast<ComStubControl*>(p);
    comCtrl_->magic.store(ComStubControl::MAGIC, std::memory_order_release);
    comCtrl_->failMask.store(0, std::memory_order_relaxed);
    comCtrl_->errnoVal.store(UBSE_COM_ERROR_SYNC_CALL_FAIL, std::memory_order_relaxed);
    for (uint32_t i = 0; i < ComStubControl::OP_COUNT; ++i) {
        comCtrl_->count[i].store(0, std::memory_order_relaxed);
    }
    comCtrl_->dstSeq.store(0, std::memory_order_relaxed);
    std::memset(comCtrl_->dstNodeId, 0, ComStubControl::DST_NODE_ID_MAX);
}

void ItNode::DestroyComShm()
{
    if (comCtrl_ != nullptr) {
        comCtrl_->magic.store(0, std::memory_order_release);
        munmap(comCtrl_, sizeof(ComStubControl));
        comCtrl_ = nullptr;
    }
    if (comShmFd_ >= 0) {
        close(comShmFd_);
        comShmFd_ = -1;
    }
    if (!comShmName_.empty()) {
        shm_unlink(comShmName_.c_str());
        comShmName_.clear();
    }
}

UbseResult ItNode::SetComFault(uint32_t failMask, uint32_t errnoVal, const uint32_t count[2],
                               const std::string& dstNodeId)
{
    if (comCtrl_ == nullptr) {
        IT_LOG_ERROR << "Com shm not initialized for node " << spec_.nodeId;
        return UBSE_ERROR_DEF(1);
    }
    comCtrl_->errnoVal.store(errnoVal, std::memory_order_relaxed);
    for (uint32_t i = 0; i < ComStubControl::OP_COUNT; ++i) {
        uint32_t c = (count != nullptr) ? count[i] : 0;
        comCtrl_->count[i].store(c, std::memory_order_relaxed);
    }
    SetComDstNodeId(*comCtrl_, dstNodeId);
    comCtrl_->failMask.store(failMask, std::memory_order_release);
    IT_LOG_INFO << "Com fault set on node " << spec_.nodeId << ", failMask=0x" << std::hex << failMask
                << ", errno=" << std::dec << errnoVal << ", dstNodeId=" << (dstNodeId.empty() ? "*" : dstNodeId);
    return UBSE_OK;
}

UbseResult ItNode::RestoreComFault()
{
    if (comCtrl_ == nullptr) {
        return UBSE_OK;
    }
    comCtrl_->failMask.store(0, std::memory_order_release);
    SetComDstNodeId(*comCtrl_, "");
    IT_LOG_INFO << "Com fault cleared on node " << spec_.nodeId;
    return UBSE_OK;
}

void ItNode::SetComSendFailed(const std::string& dstNodeId, bool fail)
{
    if (comCtrl_ == nullptr) {
        return;
    }
    uint32_t bit = 1u << ComStubControl::OP_SYNC_SEND;
    if (fail) {
        comCtrl_->errnoVal.store(UBSE_COM_ERROR_SYNC_CALL_FAIL, std::memory_order_relaxed);
        comCtrl_->count[ComStubControl::OP_SYNC_SEND].store(0, std::memory_order_relaxed);
        SetComDstNodeId(*comCtrl_, dstNodeId);
        comCtrl_->failMask.fetch_or(bit, std::memory_order_release);
    } else {
        comCtrl_->failMask.fetch_and(~bit, std::memory_order_release);
    }
}

void ItNode::SetMemApiWaitTimeOut(uint32_t timeoutMs)
{
    if (comCtrl_ == nullptr) {
        IT_LOG_WARN << "Com shm not initialized for node " << spec_.nodeId << ", cannot set mem API wait timeout";
        return;
    }
    comCtrl_->waitExImSendTimeOutMs.store(timeoutMs, std::memory_order_relaxed);
    IT_LOG_INFO << "Mem API wait timeout set on node " << spec_.nodeId << ", timeoutMs=" << timeoutMs;
}

void ItNode::RestoreMemApiWaitTimeOut()
{
    SetMemApiWaitTimeOut(0);
}

void ItNode::MarkLinkDown(const std::string& peerNodeId, const std::set<int>& ubpuIds, const std::set<int>& portIds)
{
    // 通过集群上下文将 peerNodeId 映射为 slotId (ctx_.nodeIds 与 ctx_.clusterSlotIds 按序一一对应)
    bool found = false;
    uint32_t peerSlotId = 0;
    for (size_t i = 0; i < ctx_.nodeIds.size(); ++i) {
        if (ctx_.nodeIds[i] == peerNodeId) {
            peerSlotId = ctx_.clusterSlotIds[i];
            found = true;
            break;
        }
    }
    if (!found) {
        IT_LOG_WARN << "MarkLinkDown: unknown peer node " << peerNodeId;
        return;
    }
    if (mockLcneServer_) {
        mockLcneServer_->MarkLinkDown(peerSlotId, ubpuIds, portIds);
    }
}

void ItNode::ClearLinkDowns()
{
    if (mockLcneServer_) {
        mockLcneServer_->ClearLinkDowns();
    }
}

UbseResult ItNode::NotifyLinkUpDown(const bool isPortDown, const std::string& interfaceName)
{
    if (!mockLcneClient_) {
        IT_LOG_WARN << "NotifyLinkUpDown: mock lcne client is not initialized on node " << spec_.nodeId;
        return UBSE_ERROR;
    }
    return mockLcneClient_->NotifyLinkUpDown(isPortDown, interfaceName);
}
} // namespace ubse::it::infra
