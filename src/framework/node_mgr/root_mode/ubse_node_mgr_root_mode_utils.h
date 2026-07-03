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

#ifndef UBS_ENGINE_UBSE_NODE_MGR_ROOT_MODE_UTILS_H
#define UBS_ENGINE_UBSE_NODE_MGR_ROOT_MODE_UTILS_H

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace ubse::nodeMgr::utils {
struct NodeState {
    std::string nodeId;
    uint32_t ubseLinkState;
    std::string channelType;
};

class UbseNodeMgrRootModeUtils {
public:
    static std::vector<NodeState> ParseNodeStateEventMessage(const std::string &eventMessage);
};

class NodeDiscoveryLockGuard {
public:
    explicit NodeDiscoveryLockGuard(const std::string &key);
    ~NodeDiscoveryLockGuard();
    NodeDiscoveryLockGuard(const NodeDiscoveryLockGuard &) = delete;
    NodeDiscoveryLockGuard &operator=(const NodeDiscoveryLockGuard &) = delete;
    NodeDiscoveryLockGuard(NodeDiscoveryLockGuard &&) = delete;
    NodeDiscoveryLockGuard &operator=(NodeDiscoveryLockGuard &&) = delete;

private:
    static std::shared_ptr<std::shared_mutex> GetLock(const std::string &key);
    static std::mutex mutex_;
    static std::unordered_map<std::string, std::shared_ptr<std::shared_mutex>> locks_;
    std::shared_ptr<std::shared_mutex> lock_;
};
}
#endif //UBS_ENGINE_UBSE_NODE_MGR_ROOT_MODE_UTILS_H
