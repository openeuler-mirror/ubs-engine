/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ucache_config.h"
#include "ucache_migrate_strategy.h"

using namespace ubse::log;
namespace ucache::master {
namespace migration {

std::string PrintMigrationAction(const std::vector<MigrationAction>& actions)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < actions.size(); ++i) {
        const auto& action = actions[i];
        oss << "{";
        oss << "fromNode:" << action.fromNode << ",";
        oss << "dockerIds:[";
        for (size_t j = 0; j < action.dockerIds.size(); ++j) {
            if (j != 0) {
                oss << ",";
            }
            oss << action.dockerIds[j];
        }
        oss << "],";
        oss << "toNode:" << action.toNode << ",";
        oss << "dstNumaId:" << action.dstNumaId << ",";
        oss << "startWatermark:" << action.startWatermark << ",";
        oss << "stopWatermark:" << action.stopWatermark;
        oss << "}";
        if (i != actions.size() - 1) {
            oss << ",";
        }
    }
    oss << "]";
    return oss.str();
}

bool CompareNodes(const std::pair<std::string, NodeMemoryInfo>& a, const std::pair<std::string, NodeMemoryInfo>& b)
{
    const auto& infoA = a.second;
    const auto& infoB = b.second;
    // 优先级：紧缺度 > 借出使用率 > 节点ID
    if (std::abs(infoA.memoryShortage - infoB.memoryShortage) > std::numeric_limits<double>::epsilon()) {
        return infoA.memoryShortage < infoB.memoryShortage;
    }

    if (std::abs(infoA.borrowedUsageRate - infoB.borrowedUsageRate) > std::numeric_limits<double>::epsilon()) {
        return infoA.borrowedUsageRate < infoB.borrowedUsageRate;
    }
    return a.first < b.first;
}

std::pair<uint64_t, uint64_t> CalculateWatermarks(const NodeMemoryInfo& borrower)
{
    constexpr uint64_t pageSizeKb = 4;
    constexpr uint64_t minPageSize = 10240; // 迁移阈值

    uint64_t startWaterMark = 0;
    uint64_t stopWaterMark = 0;
    const double stopMarkMultiplier = 0.6;
    const double urgentMemMultiplier = 0.05;

    // 计算节点系统low、high水线
    uint64_t wmarkMin = borrower.minFreeKbytes * 7 / 8;
    uint64_t wmarkLow = wmarkMin * 5 / 4;
    uint64_t high = wmarkLow * 3 / 2;
    uint64_t availableMemory = borrower.pageCacheMemory + borrower.freeMemory;
    if (availableMemory == 0 || availableMemory < high) {
        UBSE_LOGGER_WARN(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Memory critical: available(" << availableMemory << ") < high(" << high
            << "). Force migration target set.";
        uint64_t emergencyStopBuffer = static_cast<uint64_t>(borrower.totalMemory * urgentMemMultiplier);
        uint64_t emergencyStop = high + emergencyStopBuffer;
        return {high / pageSizeKb, emergencyStop / pageSizeKb};
    }

    uint64_t deltaMemory = availableMemory - high;
    if (deltaMemory < minPageSize) {
        startWaterMark = high + minPageSize;
    } else {
        double migrateRate = (1.0 + borrower.pageCacheAppNums) * borrower.freeMemory / availableMemory;
        startWaterMark = migrateRate * borrower.usedMemory + high;
    }
    stopWaterMark = std::max(startWaterMark, high) + availableMemory * stopMarkMultiplier;
    return {startWaterMark / pageSizeKb, stopWaterMark / pageSizeKb};
}

uint32_t GetBestLenderNumaId(const std::vector<NodeMemBorrowInfo>& lenders, const std::string& bestLenderId,
                             int& bestLenderNumaId)
{
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "bestLenderId: " << bestLenderId;
    for (const auto& lender : lenders) {
        if (lender.destNodeId == bestLenderId) {
            bestLenderNumaId = lender.dstNumaId;
            return UCACHE_OK;
        }
    }
    UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "GetBestLenderNumaId error, can't find destNodeId: " << bestLenderId << "in lenders.";
    return UCACHE_ERR;
}
std::vector<std::string> GettempDockerIds(const std::map<std::string, PageCacheSensitiveTag>& borrowBodeTags)
{
    std::vector<std::string> tempDockerIds;
    for (const auto& [dockerId, category] : borrowBodeTags) {
        if (category == PageCacheSensitiveTag::SENSITIVE) {
            tempDockerIds.push_back(dockerId);
        }
    }
    return tempDockerIds;
}

std::vector<MigrationAction> MemoryMigrationStrategy(
    const std::map<std::string, std::vector<NodeMemBorrowInfo>>& borrowMap,
    const std::map<std::string, NodeMemoryInfo>& nodes,
    const std::map<std::string, std::map<std::string, PageCacheSensitiveTag>>& nodeTags)
{
    std::vector<MigrationAction> actions;
    for (const auto& [borrowerId, lenders] : borrowMap) {
        auto borrowerIt = nodes.find(borrowerId);
        if (borrowerIt == nodes.end()) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "MemoryMigrationStrategy error, borrowerKey:" << borrowerId << "not found.";
            break;
        }
        std::vector<std::string> tempDockerIds;
        auto tagsIt = nodeTags.find(borrowerId);
        if (tagsIt != nodeTags.end()) {
            const std::map<std::string, PageCacheSensitiveTag>& borrowBodeTags = tagsIt->second;
            tempDockerIds = GettempDockerIds(borrowBodeTags);
        }
        if (tempDockerIds.empty()) {
            continue;
        }
        std::vector<std::pair<std::string, NodeMemoryInfo>> candidates;

        for (const auto& lender : lenders) {
            auto lenderIt = nodes.find(lender.destNodeId);
            if (lenderIt != nodes.end()) {
                candidates.emplace_back(lender.destNodeId, lenderIt->second);
            }
        }
        std::sort(candidates.begin(), candidates.end(), CompareNodes);

        if (!candidates.empty()) {
            const auto& [bestLenderId, _] = candidates.front();
            int bestLenderNumaId = 0;
            if (GetBestLenderNumaId(lenders, bestLenderId, bestLenderNumaId) != UCACHE_OK) {
                continue;
            }
            auto [start, stop] = CalculateWatermarks(borrowerIt->second);
            if (start == 0 || stop == 0) {
                continue;
            }
            // 添加迁移动作到列表
            MigrationAction tempAction;
            tempAction.fromNode = borrowerId;
            tempAction.dockerIds = tempDockerIds;
            tempAction.toNode = bestLenderId;
            tempAction.dstNumaId = bestLenderNumaId;
            tempAction.startWatermark = start;
            tempAction.stopWatermark = stop;
            actions.push_back({tempAction});
        }
    }
    return actions;
}
} // namespace migration
} // namespace ucache::master