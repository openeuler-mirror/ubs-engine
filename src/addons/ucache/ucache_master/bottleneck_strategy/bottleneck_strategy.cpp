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

#include "bottleneck_strategy.h"

#include <ubse_logger.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include "ucache_config.h"
#include "ucache_error.h"

namespace ucache::master::bottleneck {
using namespace ubse::log;
using namespace ucache;

BottleneckStrategy::BottleneckStrategy() {}

double BottleneckStrategy::ConvertToPercentage(uint32_t bottleneckShortThreshold)
{
    double denominator = 100.0;
    return bottleneckShortThreshold / denominator;
}

void BottleneckStrategy::UpdateContainerHistory(const CgroupInfo& cginfo, ContainerHistory& history,
                                                uint32_t bottleneckTimespan)
{
    uint32_t shortWindowLen = UcacheConfig::GetInstance().GetBottleneckShortSize() / bottleneckTimespan;
    uint32_t longWindowLen = UcacheConfig::GetInstance().GetBottleneckLongSize() / bottleneckTimespan;
    PageCacheSensitiveTag ioAndPageinStatus;
    uint32_t bottleneckThreshold = UcacheConfig::GetInstance().GetBottleneckThreshold();
    if (cginfo.ioReadBandwidth > bottleneckThreshold && cginfo.pageCacheIn > bottleneckThreshold) {
        ioAndPageinStatus = PageCacheSensitiveTag::SENSITIVE;
    } else {
        ioAndPageinStatus = PageCacheSensitiveTag::NOT_SENSITIVE;
    }

    history.shortWindow.push_back(ioAndPageinStatus);
    if (history.shortWindow.size() > shortWindowLen) {
        history.shortWindow.pop_front();
    }

    history.longWindow.push_back(ioAndPageinStatus);
    if (history.longWindow.size() > longWindowLen) {
        history.longWindow.pop_front();
    }
}

double BottleneckStrategy::CalculateSensitivePercentage(const std::deque<PageCacheSensitiveTag>& history)
{
    if (history.empty()) {
        return 0;
    }
    double count = 0;
    for (size_t i = 0; i < history.size(); ++i) {
        if (history[i] == PageCacheSensitiveTag::SENSITIVE) {
            count++;
        }
    }
    return count / history.size();
}

void BottleneckStrategy::DetermineContainerState(ContainerHistory& history, uint32_t bottleneckTimespan,
                                                 std::string dockerId)
{
    // 计算短窗口和长窗口的窗口满的长度
    uint32_t shortWindowLen = UcacheConfig::GetInstance().GetBottleneckShortSize() / bottleneckTimespan;
    uint32_t longWindowLen = UcacheConfig::GetInstance().GetBottleneckLongSize() / bottleneckTimespan;
    // 计算当前短窗口和长窗口中达到敏感状态的比例
    double currentShortRatio = CalculateSensitivePercentage(history.shortWindow);
    double currentLongRatio = CalculateSensitivePercentage(history.longWindow);

    if (history.shortWindow.size() < shortWindowLen) {
        currentShortRatio = 0;
    }
    if (history.longWindow.size() < longWindowLen) {
        currentLongRatio = 1;
    }
    const double epsilon = 1e-9; // 定义容差值
                                 // 获取配置中的阈值，并将其转换为百分比
    double bottleneckShortThresholdPercentage =
        ConvertToPercentage(UcacheConfig::GetInstance().GetBottleneckShortThreshold());
    double bottleneckLongThresholdPercentage =
        ConvertToPercentage(UcacheConfig::GetInstance().GetBottleneckLongThreshold());

    if (history.currentState == PageCacheSensitiveTag::NOT_SENSITIVE) {
        // 使用容差比较
        // 如果当前短窗口中达到敏感状态标志的比例大于瓶颈短窗口的阈值，则将状态改为敏感状态
        if (fabs(currentShortRatio - bottleneckShortThresholdPercentage) > epsilon &&
            currentShortRatio > (bottleneckShortThresholdPercentage - epsilon)) {
            history.currentState = PageCacheSensitiveTag::SENSITIVE;
            // 清理长窗口，防止识别为敏感型后又跳变回非敏感型
            history.longWindow.clear();
            UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "docker id:" << dockerId << " from NOT_SENSITIVE change to SENSITIVE.";
        }
    } else if (history.currentState == PageCacheSensitiveTag::SENSITIVE) {
        if (fabs(currentLongRatio - bottleneckLongThresholdPercentage) > epsilon &&
            currentLongRatio < (bottleneckLongThresholdPercentage + epsilon)) {
            history.currentState = PageCacheSensitiveTag::NOT_SENSITIVE;
            UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "docker id:" << dockerId << " from SENSITIVE change to NOT_SENSITIVE.";
        }
    } else if (history.currentState == PageCacheSensitiveTag::UNKNOWN) {
        // 当还未高于短窗口或者低于长窗口阈值时,状态还为UNKNOWN
        if (fabs(currentLongRatio - bottleneckLongThresholdPercentage) > epsilon &&
            currentLongRatio < (bottleneckLongThresholdPercentage + epsilon)) {
            history.currentState = PageCacheSensitiveTag::NOT_SENSITIVE;
            UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "docker id:" << dockerId << " from UNKNOWN change to NOT_SENSITIVE.";
        } else if (fabs(currentShortRatio - bottleneckShortThresholdPercentage) > epsilon &&
                   currentShortRatio > (bottleneckShortThresholdPercentage - epsilon)) {
            history.currentState = PageCacheSensitiveTag::SENSITIVE;
            UBSE_LOGGER_INFO(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "docker id:" << dockerId << " from UNKNOWN change to SENSITIVE.";
        }
    }
}

uint32_t BottleneckStrategy::JudgeSensitive(const std::map<std::string, std::map<std::string, CgroupInfo>>& cgroupInfo,
                                            uint32_t bottleneckTimespan)
{
    if (bottleneckTimespan == 0) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "invalid bottleneckTimespan: " << bottleneckTimespan;
        return UCACHE_ERR;
    }

    // 清理：从 containerHistory 中删除本次 cgroupInfo 不存在的容器历史，防止无限增长
    for (auto rackIt = containerHistory.begin(); rackIt != containerHistory.end();) {
        const std::string& rackId = rackIt->first;
        auto& dockerHistoryMap = rackIt->second;
        auto cgRackIt = cgroupInfo.find(rackId);
        if (cgRackIt == cgroupInfo.end()) {
            // rack 都不存在了，整组历史直接删掉
            rackIt = containerHistory.erase(rackIt);
            continue;
        }

        const auto& cgContainers = cgRackIt->second;

        for (auto dockIt = dockerHistoryMap.begin(); dockIt != dockerHistoryMap.end();) {
            const std::string& dockerId = dockIt->first;
            if (cgContainers.find(dockerId) == cgContainers.end()) {
                // 容器不存在，删除历史条目
                dockIt = dockerHistoryMap.erase(dockIt);
            } else {
                ++dockIt;
            }
        }

        if (dockerHistoryMap.empty()) {
            rackIt = containerHistory.erase(rackIt);
        } else {
            ++rackIt;
        }
    }

    // 正常更新与判定
    std::map<std::string, std::map<std::string, PageCacheSensitiveTag>> newContainerState;
    for (const auto& [rackId, containers] : cgroupInfo) {
        for (const auto& [dockerId, cginfo] : containers) {
            auto& history = containerHistory[rackId][dockerId];
            UpdateContainerHistory(cginfo, history, bottleneckTimespan);
            DetermineContainerState(history, bottleneckTimespan, dockerId);
            newContainerState[rackId][dockerId] = history.currentState;
        }
    }
    containerState = std::move(newContainerState);
    return UCACHE_OK;
}

std::map<std::string, std::map<std::string, PageCacheSensitiveTag>> BottleneckStrategy::GetContainerState() const
{
    return containerState;
}
} // namespace ucache::master::bottleneck
