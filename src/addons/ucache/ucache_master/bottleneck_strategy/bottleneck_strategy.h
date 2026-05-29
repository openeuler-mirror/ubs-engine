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

#ifndef BOTTLENECK_STRATEGY_H
#define BOTTLENECK_STRATEGY_H

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include "data_collect.h"

namespace ucache::master::bottleneck {

using namespace ucache::data_collect;

enum class PageCacheSensitiveTag
{
    UNKNOWN,       //  未知状态，比如数据太少
    NOT_SENSITIVE, //  非PageCache紧缺型
    SENSITIVE      //  PageCache紧缺型
};

// 维护每个容器的历史数据和状态
struct ContainerHistory {
    std::deque<PageCacheSensitiveTag> shortWindow;
    std::deque<PageCacheSensitiveTag> longWindow;
    PageCacheSensitiveTag currentState;
    // 构造函数，初始化当前状态为未知状态
    ContainerHistory() : currentState(PageCacheSensitiveTag::UNKNOWN)
    {
        // 初始化队列为空
    }
};

class BottleneckStrategy {
public:
    BottleneckStrategy();
    BottleneckStrategy(const BottleneckStrategy&) = delete;
    BottleneckStrategy& operator=(const BottleneckStrategy&) = delete;
    static BottleneckStrategy& GetInstance(){{static BottleneckStrategy instance;
    return instance;
}
}; // namespace ucache::master::bottleneck

uint32_t JudgeSensitive(const std::map<std::string, std::map<std::string, CgroupInfo>>& cgroupInfo,
                        uint32_t bottleneckTimespan);
std::map<std::string, std::map<std::string, PageCacheSensitiveTag>> GetContainerState() const;

private:
//  维护每个容器的历史数据和状态
std::map<std::string, std::map<std::string, ContainerHistory>> containerHistory;
std::map<std::string, std::map<std::string, PageCacheSensitiveTag>> containerState;
void UpdateContainerHistory(const CgroupInfo& cginfo, ContainerHistory& history, uint32_t bottleneckTimespan);
void JudgeWindowLength(uint32_t bottleneckShortSize, uint32_t bottleneckLongSize);
void JudgeThreshold(uint32_t bottleneckShortThreshold, uint32_t bottleneckLongThreshold);
double ConvertToPercentage(uint32_t bottleneckShortThreshold);
double CalculateSensitivePercentage(const std::deque<PageCacheSensitiveTag>& history);
void CleanUpObsoleteContainerHistory(const std::map<std::string, std::map<std::string, CgroupInfo>>& cgroupInfo);
void DetermineContainerState(ContainerHistory& history, uint32_t bottleneckTimespan, std::string dockerId);
}
;
} // namespace ucache::master::bottleneck

#endif /* BOTTLENECK_STRATEGY_H */
