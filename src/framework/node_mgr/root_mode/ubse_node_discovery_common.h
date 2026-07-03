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

#ifndef UBS_ENGINE_UBSE_NODE_DISCOVERY_COMMON_H
#define UBS_ENGINE_UBSE_NODE_DISCOVERY_COMMON_H

#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "ubse_com_base.h"
#include "ubse_com_module.h"
#include "ubse_common_def.h"
#include "ubse_thread_pool.h"
#include "ubse_node_mgr_def.h"

namespace ubse::nodeMgr {
using namespace ubse::com;
using namespace ubse::task_executor;

class UbseNodeDiscoveryCommon {
public:
    static UbseNodeDiscoveryCommon &GetInstance()
    {
        std::lock_guard lock(instanceMutex_);
        if (instance_ == nullptr) {
            instance_ = new UbseNodeDiscoveryCommon();
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

    UbseResult ConnectRootNode(const std::string &ip);

    UbseResult DisconnectRootNode(const std::string &ip);

    UbseResult GetClusterNodeIdByIp(const std::string &ip, std::string &nodeId);

    UbseResult GetClusterNodeIpById(const std::string &nodeId, std::string &ip);

private:
    UbseResult GetLocalAddr(UbseNodeStaticInfo &node, const std::vector<std::string> &rootIpList, bool &isRoot);

    UbseResult StartRpcServer();

    UbseResult NewChannelCallback(const std::string &remoteIp, const std::string &remoteNodeId);

    UbseResult RegisterComHandler();

    UbseResult SubscribeNodeStateEvent();

    UbseResult HandleNodeStateEvent(const std::string &eventId, const std::string &eventMessage);

    void ExecNodeDiscoveryTask(const std::string &rootIp);

    UbseResult ReportAndQuerySuperClusterTopo(const std::string &rootIp);

    UbseResult SendMsgToRoot(const std::string &rootIp);

    void OnReportSuccess(const std::string &rootIp, const std::string &taskName);

    void OnReportFailure(const std::string &rootIp, const std::string &taskName);

    void DisconnectNonDefaultRoot();

    void SetConnectedNonDefaultRoot(const std::string &ip);

    std::string GetConnectedNonDefaultRoot();

    static UbseNodeDiscoveryCommon *instance_;
    static std::mutex instanceMutex_;
    std::string defaultRoot_{};
    std::string connectedNonDefaultRoot_{};
    UbseTaskExecutorPtr taskExecutor_{};
    std::shared_mutex mutex_{};
    std::unordered_map<std::string, std::string> clusterNodeIdToIpMap_{};
    struct RetryRecord {
        uint32_t retryCount = 0;
        bool timerRegistered = false;
    };
    std::mutex reportRetryRecordsMutex_{};
    std::unordered_map<std::string, RetryRecord> reportRetryRecords_{};
};

class UbseNodeDiscoveryDistributeHandler : public UbseComBaseMessageHandler {
public:
    UbseResult Handle(const UbseBaseMessagePtr &req, const UbseBaseMessagePtr &rsp,
                      UbseComBaseMessageHandlerCtxPtr ctx) override;

    uint16_t GetOpCode() override;

    uint16_t GetModuleCode() override;
};
} // namespace ubse::nodeMgr

#endif // UBS_ENGINE_UBSE_NODE_DISCOVERY_COMMON_H