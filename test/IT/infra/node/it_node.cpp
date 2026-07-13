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
#include <utility>

#include "ubse_error.h"
#include "it_console_log.h"

namespace ubse::it::infra {

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
        lcneClient_ = std::make_unique<ItLcneClient>(lcneUdsPath_);
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

} // namespace ubse::it::infra
