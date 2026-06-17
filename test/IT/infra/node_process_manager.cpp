/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "node_process_manager.h"

#include <sched.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <thread>
#include <utility>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "it_console_log.h"

namespace ubse::it::infra {

NodeProcessManager::NodeProcessManager(NodeProcessConfig config)
    : binaryPath_(std::move(config.binaryPath)),
      nodeId_(std::move(config.nodeId)),
      nodeIp_(std::move(config.nodeIp)),
      workDir_(std::move(config.workDir)),
      slotId_(config.slotId),
      clusterNodeIds_(std::move(config.clusterNodeIds)),
      clusterIps_(std::move(config.clusterIps)),
      clusterSlotIds_(std::move(config.clusterSlotIds)),
      stubLibDir_(std::move(config.stubLibDir)),
      childPid_(-1),
      udsSocketPath_(workDir_ + "/run/ubse.sock")
{
}

UbseResult NodeProcessManager::Start()
{
    if (IsRunning()) {
        IT_LOG_INFO << "Node " << nodeId_ << " already running with pid " << childPid_;
        return UBSE_OK;
    }

    std::filesystem::create_directories(workDir_);
    std::filesystem::create_directories(workDir_ + "/run");
    std::filesystem::create_directories(workDir_ + "/log");

    isPrivileged_ = (geteuid() == 0);

    std::filesystem::create_directories(workDir_ + "/run/ubm/socket/ubm_nuds");
    lcneUdsWorkPath_ = workDir_ + "/run/ubm/socket/ubm_nuds/restconf.sock";
    mockLcneServer_ = std::make_unique<MockLcneServer>(lcneUdsWorkPath_, slotId_, clusterSlotIds_);

    mockLcneServer_->Start();
    auto lcneReadyRet = mockLcneServer_->WaitForReady(5000);
    if (lcneReadyRet != UBSE_OK) {
        IT_LOG_ERROR << "MockLcneServer failed to become ready within 5s for node " << nodeId_;
        mockLcneServer_->Stop();
        mockLcneServer_.reset();
        return UBSE_ERROR_DEF(8);
    }

    if (!isPrivileged_ && stubLibDir_.empty()) {
        CreateSymlinkFallback();
    }

    pid_t pid = fork();
    if (pid < 0) {
        IT_LOG_ERROR << "Failed to fork for node " << nodeId_ << ": " << strerror(errno);
        return UBSE_ERROR_DEF(1);
    }

    if (pid == 0) {
        SetupChildNamespace(isPrivileged_);

        if (!stubLibDir_.empty()) {
            setenv("LD_LIBRARY_PATH", stubLibDir_.c_str(), 1);
            std::string preloadPath = stubLibDir_ + "/libubse_interface_preload.so";
            setenv("LD_PRELOAD", preloadPath.c_str(), 1);
        }

        setenv("UBSE_IT_NODE_ID", nodeId_.c_str(), 1);
        setenv("UBSE_IT_NODE_IP", nodeIp_.c_str(), 1);
        setenv("UBSE_IT_LOCAL_IP", nodeIp_.c_str(), 1);
        setenv("UBSE_IT_CONF_DIR", workDir_.c_str(), 1);
        std::string slotId = std::to_string(slotId_);
        setenv("UBSE_IT_SLOT_ID", slotId.c_str(), 1);
        if (!clusterNodeIds_.empty()) {
            setenv("UBSE_IT_CLUSTER_NODES", clusterNodeIds_.c_str(), 1);
        }
        if (!clusterIps_.empty()) {
            setenv("UBSE_IT_CLUSTER_IPS", clusterIps_.c_str(), 1);
        }
        setenv("UBSE_IT_UDS_SOCKET_PATH", udsSocketPath_.c_str(), 1);
        setenv("UBSE_UDS_ADDRESS", udsSocketPath_.c_str(), 1);
        if (!lcneUdsWorkPath_.empty()) {
            setenv("UBSE_IT_LCNE_UDS_PATH", lcneUdsWorkPath_.c_str(), 1);
        }
        std::string logPath = workDir_ + "/log";
        setenv("UBSE_IT_LOG_PATH", logPath.c_str(), 1);
        std::string hcomPath = workDir_ + "/hcom";
        std::filesystem::create_directories(hcomPath);
        setenv(ubse::common::def::UBSE_HCOM_FILE_PATH_PREFIX.c_str(), hcomPath.c_str(), 1);

        // SMBIOS mock environment variables (used by ubse_it_daemon's mock SMBIOS stub)
        setenv("UBSE_IT_MESH_TYPE", "1", 1); // "1"=FULL_MESH, "8"=CLOS
        setenv("UBSE_IT_POD_ID", "1", 1);
        setenv("UBSE_IT_SUPER_POD_ID", "1", 1);
        setenv("UBSE_IT_SERVER_IDX", "0", 1);

        if (chdir(workDir_.c_str()) != 0) {
            IT_LOG_ERROR << "Failed to chdir to " << workDir_ << ": " << strerror(errno);
            _exit(1);
        }

        execl(binaryPath_.c_str(), binaryPath_.c_str(), static_cast<char*>(nullptr));
        IT_LOG_ERROR << "Failed to exec " << binaryPath_ << ": " << strerror(errno);
        _exit(1);
    }

    childPid_ = pid;
    IT_LOG_INFO << "Node " << nodeId_ << " started with pid " << childPid_;
    return UBSE_OK;
}

void NodeProcessManager::SetupChildNamespace(bool isPrivileged)
{
    if (!isPrivileged) {
        return;
    }

    if (unshare(CLONE_NEWNS) != 0) {
        IT_LOG_WARN << "unshare(CLONE_NEWNS) failed (errno=" << errno << "), proceeding without mount isolation";
        return;
    }

    mount("none", "/", nullptr, MS_PRIVATE | MS_REC, nullptr);

    std::string runDir = workDir_ + "/run";
    if (!std::filesystem::exists("/var/run/ubse")) {
        std::filesystem::create_directories("/var/run/ubse");
    }
    if (mount(runDir.c_str(), "/var/run/ubse", "none", MS_BIND, nullptr) != 0) {
        IT_LOG_WARN << "bind mount /var/run/ubse failed (errno=" << errno << ")";
    }

    std::string logDir = workDir_ + "/log";
    if (!std::filesystem::exists("/var/log/ubse")) {
        std::filesystem::create_directories("/var/log/ubse");
    }
    if (mount(logDir.c_str(), "/var/log/ubse", "none", MS_BIND, nullptr) != 0) {
        IT_LOG_WARN << "bind mount /var/log/ubse failed (errno=" << errno << ")";
    }

    std::string lcneUdsWorkParentDir = workDir_ + "/run/ubm/socket/ubm_nuds";
    if (!std::filesystem::exists("/run/ubm/socket/ubm_nuds")) {
        std::filesystem::create_directories("/run/ubm/socket/ubm_nuds");
    }
    if (mount(lcneUdsWorkParentDir.c_str(), "/run/ubm/socket/ubm_nuds", "none", MS_BIND, nullptr) != 0) {
        IT_LOG_WARN << "bind mount /run/ubm/socket/ubm_nuds failed (errno=" << errno << ")";
    }
}

void NodeProcessManager::CreateSymlinkFallback()
{
    std::string ubseRunDir = workDir_ + "/run";
    try {
        if (!std::filesystem::exists("/var/run/ubse")) {
            std::filesystem::create_symlink(ubseRunDir, "/var/run/ubse");
            symlinkCreatedRun_ = true;
        } else if (std::filesystem::is_directory("/var/run/ubse")) {
            IT_LOG_WARN << "/var/run/ubse already exists as directory, "
                        << "daemon will use this path directly";
        }
    } catch (const std::filesystem::filesystem_error& e) {
        IT_LOG_WARN << "Cannot create symlink /var/run/ubse -> " << ubseRunDir << ": " << e.what()
                    << " (non-root, /var/run not writable)";
    }
    std::string ubseLogDir = workDir_ + "/log";
    try {
        if (!std::filesystem::exists("/var/log/ubse")) {
            std::filesystem::create_symlink(ubseLogDir, "/var/log/ubse");
            symlinkCreatedLog_ = true;
        } else if (std::filesystem::is_directory("/var/log/ubse")) {
            IT_LOG_WARN << "/var/log/ubse already exists as directory, "
                        << "daemon will use this path directly";
        }
    } catch (const std::filesystem::filesystem_error& e) {
        IT_LOG_WARN << "Cannot create symlink /var/log/ubse -> " << ubseLogDir << ": " << e.what()
                    << " (non-root, /var/log not writable)";
    }
}

UbseResult NodeProcessManager::Stop()
{
    if (!IsRunning()) {
        IT_LOG_INFO << "Node " << nodeId_ << " not running";
        CleanupSymlinks();
        return UBSE_OK;
    }

    if (kill(childPid_, SIGTERM) != 0) {
        IT_LOG_ERROR << "Failed to send SIGTERM to node " << nodeId_ << ": " << strerror(errno);
        return UBSE_ERROR_DEF(2);
    }

    constexpr uint32_t waitTimeoutMs = 5000;
    constexpr uint32_t pollIntervalMs = 100;
    uint32_t elapsed = 0;
    while (elapsed < waitTimeoutMs) {
        int status = 0;
        pid_t ret = waitpid(childPid_, &status, WNOHANG);
        if (ret == childPid_) {
            IT_LOG_INFO << "Node " << nodeId_ << " terminated gracefully";
            childPid_ = -1;
            if (mockLcneServer_) {
                mockLcneServer_->Stop();
            }
            CleanupSymlinks();
            return UBSE_OK;
        }
        if (ret < 0) {
            IT_LOG_ERROR << "waitpid error for node " << nodeId_ << ": " << strerror(errno);
            childPid_ = -1;
            return UBSE_ERROR_DEF(3);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        elapsed += pollIntervalMs;
    }

    IT_LOG_INFO << "Node " << nodeId_ << " did not terminate, sending SIGKILL";
    if (kill(childPid_, SIGKILL) != 0) {
        IT_LOG_ERROR << "Failed to send SIGKILL to node " << nodeId_ << ": " << strerror(errno);
        return UBSE_ERROR_DEF(4);
    }

    int status = 0;
    pid_t ret = waitpid(childPid_, &status, 0);
    if (ret == childPid_) {
        IT_LOG_INFO << "Node " << nodeId_ << " killed";
        childPid_ = -1;
        if (mockLcneServer_) {
            mockLcneServer_->Stop();
        }
        CleanupSymlinks();
        return UBSE_OK;
    }

    IT_LOG_ERROR << "Failed to kill node " << nodeId_;
    return UBSE_ERROR_DEF(5);
}

void NodeProcessManager::CleanupSymlinks()
{
    if (symlinkCreatedRun_) {
        std::filesystem::remove("/var/run/ubse");
        symlinkCreatedRun_ = false;
    }
    if (symlinkCreatedLog_) {
        std::filesystem::remove("/var/log/ubse");
        symlinkCreatedLog_ = false;
    }
    if (!lcneUdsWorkPath_.empty() && lcneUdsWorkPath_.find("/run/ubm") != std::string::npos) {
        unlink(lcneUdsWorkPath_.c_str());
    }
}

bool NodeProcessManager::IsRunning() const
{
    if (childPid_ <= 0) {
        return false;
    }
    int status = 0;
    pid_t ret = waitpid(childPid_, &status, WNOHANG);
    return ret == 0;
}

pid_t NodeProcessManager::GetPid() const
{
    return childPid_;
}

UbseResult NodeProcessManager::WaitForStartup(uint32_t timeoutMs)
{
    constexpr uint32_t pollIntervalMs = 100;
    std::string fallbackSocketPath = "/var/run/ubse/ubse.sock";
    uint32_t elapsed = 0;
    while (elapsed < timeoutMs) {
        struct stat st{};
        if (stat(udsSocketPath_.c_str(), &st) == 0) {
            IT_LOG_INFO << "Node " << nodeId_ << " UDS socket ready at " << udsSocketPath_;
            return UBSE_OK;
        }
        if (stat(fallbackSocketPath.c_str(), &st) == 0) {
            IT_LOG_INFO << "Node " << nodeId_ << " UDS socket ready at fallback " << fallbackSocketPath;
            udsSocketPath_ = fallbackSocketPath;
            return UBSE_OK;
        }
        if (!IsRunning()) {
            IT_LOG_ERROR << "Node " << nodeId_ << " process died during startup";
            return UBSE_ERROR_DEF(6);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        elapsed += pollIntervalMs;
    }
    IT_LOG_ERROR << "Node " << nodeId_ << " startup timed out after " << timeoutMs << "ms";
    return UBSE_ERROR_DEF(7);
}

const std::string& NodeProcessManager::GetNodeId() const
{
    return nodeId_;
}

const std::string& NodeProcessManager::GetWorkDir() const
{
    return workDir_;
}

const std::string& NodeProcessManager::GetUdsSocketPath() const
{
    return udsSocketPath_;
}

} // namespace ubse::it::infra
