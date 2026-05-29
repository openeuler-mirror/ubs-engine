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

#include "ubse_mem_configuration.h"
#include "ubse_mem_functions.h"
#include "ubse_node_controller.h"

namespace ubse::mem::strategy {
using namespace ubse::log;
using namespace ubse::config;
using namespace ubse::nodeController;

UBSE_DEFINE_THIS_MODULE("ubse_mem_strategy");

namespace {
uint16_t ParseRadiusConfig(const std::string& configKey, const std::string& configName, uint16_t defaultValue)
{
    std::string radiusStr;
    auto ret = GetUbseConf(UBSE_MEMORY, configKey, radiusStr);
    if (ret == UBSE_OK) {
        try {
            int radius = std::stoi(radiusStr);
            if (radius >= 0 && radius <= defaultValue) {
                UBSE_LOG_INFO << "Set " << configName << " to " << radius;
                return static_cast<uint16_t>(radius);
            }
            UBSE_LOG_WARN << configKey << " value=" << radius
                          << " is out of range [0, 7], Use default value=" << defaultValue;
        } catch (const std::exception& e) {
            UBSE_LOG_WARN << "Convert " << configKey << " failed: " << e.what()
                          << ", Use default value=" << defaultValue;
        }
    } else {
        UBSE_LOG_WARN << "Get " << configKey << " failed, Use default value=" << defaultValue;
    }
    return defaultValue;
}
} // namespace
void UbseMemConfiguration::Init()
{
    SetPageType();
    SetMemoryRadius();
}

void UbseMemConfiguration::SetPageType()
{
    std::string osPageSize;
    auto ret = GetUbseConf("os", "page_size", osPageSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Get os page_size type failed, Use default value 4096";
        osPageSize = PAGE_SIZE_4K;
    }
    if (osPageSize == PAGE_SIZE_64K) {
        pageType_ = PageSizeType::Page64K;
    } else if (osPageSize == PAGE_SIZE_4K) {
        pageType_ = PageSizeType::Page4K;
    } else {
        UBSE_LOG_WARN << "Get os page_size type is invalid, Use default value 4096";
        pageType_ = PageSizeType::Page4K;
    }
}

void UbseMemConfiguration::SetMemoryRadius()
{
    lenderRadius_ = ParseRadiusConfig("radius.lender", "lender radius", DEFAULT_LENDER_RADIUS);
    borrowRadius_ = ParseRadiusConfig("radius.borrow", "borrow radius", DEFAULT_BORROW_RADIUS);
}

void UbseMemConfiguration::SetConfig(const NodeInfoMap& nodeMap)
{
    nodeConfigs_.clear();

    for (const auto& [nodeId, nodeInfo] : nodeMap) {
        if (nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_UNKNOWN ||
            nodeInfo.clusterState == UbseNodeClusterState::UBSE_NODE_FAULT) {
            continue;
        }
        nodeConfigs_.insert(
            {nodeInfo.nodeId, {nodeInfo.isLender, nodeInfo.allocator, nodeInfo.blockSize, nodeInfo.pmdMapping}});
    }
}

std::optional<uint32_t> UbseMemConfiguration::GetPmdMappingById(const std::string& nodeId) const
{
    auto nodeConfig = nodeConfigs_.find(nodeId);
    if (nodeConfig == nodeConfigs_.end()) {
        return std::nullopt;
    }
    return nodeConfig->second.pmdMapping;
}

std::optional<UbseAllocator> UbseMemConfiguration::GetObmmAllocatorById(const std::string& nodeId) const
{
    auto nodeConfig = nodeConfigs_.find(nodeId);
    if (nodeConfig == nodeConfigs_.end()) {
        return std::nullopt;
    }
    return nodeConfig->second.allocator;
}

std::optional<uint32_t> UbseMemConfiguration::GetBlockSizeById(const std::string& nodeId) const
{
    auto nodeConfig = nodeConfigs_.find(nodeId);
    if (nodeConfig == nodeConfigs_.end()) {
        return std::nullopt;
    }
    return nodeConfig->second.blockSize;
}

std::optional<uint32_t> UbseMemConfiguration::GetBlockSizeFromLenderNode() const
{
    for (const auto& [_, config] : nodeConfigs_) {
        if (config.isLender) {
            return config.blockSize;
        }
    }
    return std::nullopt;
}

std::optional<UbseAllocator> UbseMemConfiguration::GetAllocatorFromLenderNode() const
{
    for (const auto& [_, config] : nodeConfigs_) {
        if (config.isLender) {
            return config.allocator;
        }
    }
    return std::nullopt;
}

const std::unordered_map<std::string, NodeConfig>& UbseMemConfiguration::GetAllConfigs() const
{
    return nodeConfigs_;
}

bool UbseMemConfiguration::IsLenderBalance()
{
    bool res = false;
    if (GetUbseConf(UBSE_MEMORY, UBSE_LENDER_BALANCE, res) != UBSE_OK) {
        res = false;
        UBSE_LOG_WARN << "rmMemConfigInitFuncs get error , can not get " << UBSE_LENDER_BALANCE;
    }
    return res;
}
} // namespace ubse::mem::strategy
