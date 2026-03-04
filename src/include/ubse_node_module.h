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

#ifndef UBSE_MANAGER_UBSE_NODE_MODULE_H
#define UBSE_MANAGER_UBSE_NODE_MODULE_H

#include <cstdint>              // for uint32_t, uint16_t
#include <map>                  // for map
#include <string>               // for string, basic_string
#include <vector>               // for vector
#include <shared_mutex>

#include "ubse_base_message.h"  // for UbseBaseMessagePtr
#include "ubse_common_def.h"    // for UbseResult
#include "ubse_event_module.h"
#include "ubse_module.h"        // for UbseModule
#include "ubse_node_module.h"
#include "ubse_thread_pool_module.h"
#include "ubse_context.h"       // for context
#include "ubse_election_module.h"
#include "ubse_event.h"         // for event
#include "ubse_logger.h"        // for log
#include "ubse_logger_module.h"

namespace ubse::node {
using namespace ubse::context;
using namespace ubse::event;
using namespace ubse::log;
using namespace ubse::module;
using namespace ubse::election;
using namespace ubse::task_executor;

constexpr uint32_t NODE_NAME_MIN_LENGTH = 1;
constexpr uint32_t NODE_NAME_MAX_LENGTH = 64;
constexpr const char* const EXPORT_IP = "Ip";
constexpr const char* const EXPORT_HOSTNAME = "hostName";

class UbseNodeModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

public:
    static std::vector<UbseRoleInfo> GetLinkUpNodes();

    static UbseResult NodeDownHandler(std::string nodeId);

    static UbseResult BrokenHandler(std::string &, std::string &eventMessage);

private:
    UbseResult EstablishDataChannel(UbseElectionEventType &type);

    UbseResult Connect(std::vector<ubse::election::Node> nodes, std::string standbyId);

    UbseResult ConnectChannel(UbseTaskExecutorPtr ptr, const std::vector<Node>& nodes, std::string executorName,
                              const std::string& standbyId);

    UbseResult CheckNodeId();

    static void ResetLinkUpNode(std::string masterId);

    static void PushLinkUpNode(std::string nodeId, std::string role);

    static UbseResult NodePanicHandler(std::string &, std::string &eventMessage);

private:
    std::string nodeId_;

    static std::shared_mutex nodeMutex;
    static std::vector<UbseRoleInfo> linkUpNodes;
    static std::unordered_set<std::string> taskSet;
    static std::shared_mutex taskMutex;
};
} // namespace ubse::node

#endif // UBSE_MANAGER_UBSE_NODE_MODULE_H
