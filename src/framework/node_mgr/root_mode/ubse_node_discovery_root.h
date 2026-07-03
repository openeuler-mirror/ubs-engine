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

#ifndef UBS_ENGINE_UBSE_NODE_DISCOVERY_ROOT_H
#define UBS_ENGINE_UBSE_NODE_DISCOVERY_ROOT_H

#include <functional>
#include <mutex>
#include <unordered_set>

#include "ubse_com_base.h"
#include "ubse_com_module.h"
#include "ubse_thread_pool.h"

namespace ubse::nodeMgr {

using namespace ubse::com;
using namespace ubse::task_executor;

class UbseNodeDiscoveryRoot {
public:
    static UbseNodeDiscoveryRoot &GetInstance()
    {
        std::lock_guard lock(instanceMutex_);
        if (instance_ == nullptr) {
            instance_ = new UbseNodeDiscoveryRoot();
        }
        return *instance_;
    }

    static void Destroy()
    {
        std::lock_guard lock(instanceMutex_);
        if (instance_ != nullptr) {
            instance_->UnInit();
            delete instance_;
            instance_ = nullptr;
        }
    }

    UbseResult Init();

    void UnInit();

    void BroadcastClusterNodeInfo();

    void AddConnectedNode(const std::string &nodeId);

    void RemoveConnectedNode(const std::string &nodeId);
private:
    UbseResult SubscribeNodeStateEvent();

    UbseResult HandleNodeStateEvent(const std::string &eventId, const std::string &eventMessage);

    void CleanupRetryTask(const std::string &taskName);

    void ScheduleRetry(const std::string &taskName, std::function<UbseResult()> retryFunc);

    void ExecBroadcastToRoot(const std::string &rootIp);

    UbseResult ConnectAndBroadcastToRoot(const std::string &rootIp);

    void ExecBroadcastToConnectedNode(const std::string &remoteId);

    UbseResult BroadcastToNode(const std::string &remoteId);

    static UbseNodeDiscoveryRoot *instance_;
    static std::mutex instanceMutex_;
    UbseTaskExecutorPtr taskExecutor_{};
    std::mutex retryMutex_{};
    std::unordered_set<std::string> retryBroadcastTasks_{};
    std::mutex connectedNodesMutex_{};
    std::unordered_set<std::string> connectedNodes_{};
};

class UbseNodeDiscoveryReportHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};
} // namespace ubse::nodeMgr

#endif // UBS_ENGINE_UBSE_NODE_DISCOVERY_ROOT_H