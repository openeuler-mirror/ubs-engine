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

#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <thread>
#include <utility>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "it_console_log.h"

extern char** environ;

namespace ubse::it::infra {

NodeProcessManager::NodeProcessManager(NodeProcessConfig config)
    : binaryPath_(std::move(config.binaryPath)),
      launcherPath_((std::filesystem::path(binaryPath_).parent_path() / "ubse_it_node_launcher").string()),
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

NodeProcessManager::~NodeProcessManager()
{
    Stop();
}

std::vector<std::string> NodeProcessManager::BuildChildEnvironment() const
{
    std::vector<std::string> environment;
    for (char** entry = environ; entry != nullptr && *entry != nullptr; ++entry) {
        std::string value(*entry);
        if (value.rfind("LD_LIBRARY_PATH=", 0) == 0 || value.rfind("LD_PRELOAD=", 0) == 0 ||
            value.rfind("UBSE_IT_", 0) == 0 || value.rfind("UBSE_UDS_ADDRESS=", 0) == 0 ||
            value.rfind(ubse::common::def::UBSE_HCOM_FILE_PATH_PREFIX + "=", 0) == 0) {
            continue;
        }
        environment.push_back(std::move(value));
    }

    environment.emplace_back("UBSE_IT_NODE_ID=" + nodeId_);
    environment.emplace_back("UBSE_IT_NODE_IP=" + nodeIp_);
    environment.emplace_back("UBSE_IT_LOCAL_IP=" + nodeIp_);
    environment.emplace_back("UBSE_IT_CONF_DIR=" + workDir_);
    environment.emplace_back("UBSE_IT_SLOT_ID=" + std::to_string(slotId_));
    environment.emplace_back("UBSE_IT_CLUSTER_NODES=" + clusterNodeIds_);
    environment.emplace_back("UBSE_IT_CLUSTER_IPS=" + clusterIps_);
    environment.emplace_back("UBSE_IT_UDS_SOCKET_PATH=" + udsSocketPath_);
    environment.emplace_back("UBSE_UDS_ADDRESS=" + udsSocketPath_);
    environment.emplace_back("UBSE_IT_LCNE_UDS_PATH=" + lcneUdsWorkPath_);
    environment.emplace_back("UBSE_IT_LOG_PATH=" + workDir_ + "/log");
    environment.emplace_back(ubse::common::def::UBSE_HCOM_FILE_PATH_PREFIX + "=" + workDir_ + "/hcom");
    environment.emplace_back("UBSE_IT_MESH_TYPE=1");
    environment.emplace_back("UBSE_IT_POD_ID=1");
    environment.emplace_back("UBSE_IT_SUPER_POD_ID=1");
    environment.emplace_back("UBSE_IT_SERVER_IDX=0");
    return environment;
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
    std::filesystem::create_directories(workDir_ + "/hcom");

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

    if (!std::filesystem::exists(launcherPath_)) {
        IT_LOG_ERROR << "IT node launcher not found: " << launcherPath_;
        StopAuxiliaryServices();
        return UBSE_ERROR_DEF(9);
    }

    std::vector<std::string> environment = BuildChildEnvironment();
    std::vector<char*> envp;
    envp.reserve(environment.size() + 1);
    for (auto& entry : environment) {
        envp.push_back(entry.data());
    }
    envp.push_back(nullptr);

    std::string privileged = isPrivileged_ ? "1" : "0";
    std::vector<char*> argv = {launcherPath_.data(), binaryPath_.data(), workDir_.data(),
                               privileged.data(),    stubLibDir_.data(), nullptr};
    pid_t pid = -1;
    int spawnRet = posix_spawn(&pid, launcherPath_.c_str(), nullptr, nullptr, argv.data(), envp.data());
    if (spawnRet != 0) {
        IT_LOG_ERROR << "Failed to spawn node " << nodeId_ << ": " << strerror(spawnRet);
        StopAuxiliaryServices();
        return UBSE_ERROR_DEF(1);
    }

    childPid_ = pid;
    IT_LOG_INFO << "Node " << nodeId_ << " started with pid " << childPid_;
    return UBSE_OK;
}

void NodeProcessManager::StopAuxiliaryServices()
{
    if (mockLcneServer_) {
        mockLcneServer_->Stop();
        mockLcneServer_.reset();
    }
    CleanupSymlinks();
}

UbseResult NodeProcessManager::Stop()
{
    if (childPid_ <= 0 && !mockLcneServer_) {
        return UBSE_OK;
    }
    if (!IsRunning()) {
        IT_LOG_INFO << "Node " << nodeId_ << " not running";
        childPid_ = -1;
        StopAuxiliaryServices();
        return UBSE_OK;
    }
    return StopProcess(SIGTERM);
}

UbseResult NodeProcessManager::Kill()
{
    if (!IsRunning()) {
        childPid_ = -1;
        StopAuxiliaryServices();
        return UBSE_OK;
    }
    return StopProcess(SIGKILL);
}

UbseResult NodeProcessManager::StopProcess(int signal)
{
    if (kill(childPid_, signal) != 0 && errno != ESRCH) {
        IT_LOG_ERROR << "Failed to send signal " << signal << " to node " << nodeId_ << ": " << strerror(errno);
        return UBSE_ERROR_DEF(2);
    }

    if (signal == SIGKILL) {
        int status = 0;
        (void)waitpid(childPid_, &status, 0);
        childPid_ = -1;
        StopAuxiliaryServices();
        return UBSE_OK;
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
            StopAuxiliaryServices();
            return UBSE_OK;
        }
        if (ret < 0) {
            IT_LOG_ERROR << "waitpid error for node " << nodeId_ << ": " << strerror(errno);
            childPid_ = -1;
            StopAuxiliaryServices();
            return UBSE_ERROR_DEF(3);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
        elapsed += pollIntervalMs;
    }

    IT_LOG_INFO << "Node " << nodeId_ << " did not terminate, sending SIGKILL";
    if (kill(childPid_, SIGKILL) != 0 && errno != ESRCH) {
        IT_LOG_ERROR << "Failed to send SIGKILL to node " << nodeId_ << ": " << strerror(errno);
        return UBSE_ERROR_DEF(4);
    }

    int status = 0;
    pid_t ret = waitpid(childPid_, &status, 0);
    if (ret == childPid_) {
        IT_LOG_INFO << "Node " << nodeId_ << " killed";
        childPid_ = -1;
        StopAuxiliaryServices();
        return UBSE_OK;
    }

    IT_LOG_ERROR << "Failed to kill node " << nodeId_;
    return UBSE_ERROR_DEF(5);
}

void NodeProcessManager::CleanupSymlinks()
{
    if (!lcneUdsWorkPath_.empty()) {
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
    if (ret == childPid_ || (ret < 0 && errno == ECHILD)) {
        childPid_ = -1;
        return false;
    }
    return ret == 0;
}

pid_t NodeProcessManager::GetPid() const
{
    return childPid_;
}

UbseResult NodeProcessManager::WaitForStartup(uint32_t timeoutMs)
{
    constexpr uint32_t pollIntervalMs = 100;
    uint32_t elapsed = 0;
    while (elapsed < timeoutMs) {
        struct stat st {
        };
        if (stat(udsSocketPath_.c_str(), &st) == 0) {
            IT_LOG_INFO << "Node " << nodeId_ << " UDS socket ready at " << udsSocketPath_;
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
