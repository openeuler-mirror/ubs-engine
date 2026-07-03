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

#include "ubse_node_mgr_root_mode_utils.h"
#include "ubse_str_util.h"

namespace ubse::nodeMgr::utils {
using namespace ubse::utils;

std::mutex NodeDiscoveryLockGuard::mutex_;
std::unordered_map<std::string, std::shared_ptr<std::shared_mutex>> NodeDiscoveryLockGuard::locks_{};

std::shared_ptr<std::shared_mutex> NodeDiscoveryLockGuard::GetLock(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = locks_.find(key);
    if (it != locks_.end()) {
        return it->second;
    }
    auto lockPtr = std::make_shared<std::shared_mutex>();
    locks_[key] = lockPtr;
    return lockPtr;
}

NodeDiscoveryLockGuard::NodeDiscoveryLockGuard(const std::string &key) : lock_(GetLock(key))
{
    lock_->lock();
}

NodeDiscoveryLockGuard::~NodeDiscoveryLockGuard()
{
    lock_->unlock();
}

constexpr size_t FIELDS_COUNT = 4;
constexpr size_t PAIR_SIZE = 2;
std::vector<NodeState> UbseNodeMgrRootModeUtils::ParseNodeStateEventMessage(const std::string &eventMessage)
{
    std::vector<NodeState> linkList;
    std::vector<std::string> linkBlocks;
    Split(eventMessage, ";", linkBlocks);
    for (const auto &block : linkBlocks) {
        if (block.empty()) {
            continue;
        }
        std::vector<std::string> fields;
        Split(block, ",", fields);
        if (fields.size() != FIELDS_COUNT) {
            continue;
        }
        std::vector<std::string> nodeId;
        Split(fields[0], ":", nodeId);
        std::vector<std::string> state;
        Split(fields[1], ":", state);
        std::vector<std::string> timeStamp;
        Split(fields[2], ":", timeStamp);
        std::vector<std::string> channelType;
        Split(fields[3], ":", channelType);
        if (nodeId.size() != PAIR_SIZE || state.size() != PAIR_SIZE || timeStamp.size() != PAIR_SIZE ||
            channelType.size() != PAIR_SIZE) {
            continue;
            }
        uint32_t ubseLinkState = 0;
        if (ConvertStrToUint32(state[1], ubseLinkState) != UBSE_OK) {
            continue;
        }
        linkList.emplace_back(NodeState{nodeId[1], ubseLinkState, channelType[1]});
    }
    return linkList;
}
}