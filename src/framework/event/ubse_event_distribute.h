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

#ifndef UBSE_EVENT_DISTRIBUTE_H
#define UBSE_EVENT_DISTRIBUTE_H
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_event.h"
#include "ubse_event_def.h"
#include "ubse_event_thread_pool.h"
#include "ubse_logger_module.h"

namespace ubse::event {
using namespace ubse::utils;
using namespace ubse::common::def;

class UbseEventDistribute : public Referable {
public:
    explicit UbseEventDistribute() = default;

    explicit UbseEventDistribute(uint32_t capacity, uint32_t numsHighThs, uint32_t numsMidThs, uint32_t numsLowThs)
        : eventQueueCapacity(capacity), numsHighThs(numsHighThs), numsMidThs(numsMidThs), numsLowThs(numsLowThs){};

    void RegisterSubscribe(const std::string &eventId, UbseEventPriority priority, UbseEventHandler registerFunc);

    void UnRegisterSubscribe(const std::string &eventId, UbseEventHandler registerFunc);

    inline bool IsRegisteredSubscribe(const std::string &eventId)
    {
        std::shared_lock<std::shared_mutex> readLock(eventSubMutex);
        return subscribes.find(eventId) != subscribes.end();
    }

    void PubEvent(const std::string &eventId, const std::string &eventMsg);

    void EventDistribute();

    void Distribute(EventTask &task);

    UbseResult Init();

    UbseResult Start();

    void RunDistribute();

    void Stop();

private:
    UbseEventThreadPoolPtr poolPtr = nullptr;
    uint32_t eventQueueCapacity;
    uint32_t numsHighThs;
    uint32_t numsMidThs;
    uint32_t numsLowThs;
    std::map<std::string, std::vector<EventTask>> subscribes;
    std::deque<std::pair<std::string, std::string>> events;
    std::atomic<bool> threadRunning;
    std::mutex eventPubMutex;
    std::shared_mutex eventSubMutex;
    std::condition_variable notEmpty;
    std::thread distributeThread;

private:
    std::map<std::string, uint32_t> congestCntMap;

    void MonitorCongestion(const std::string &eventId, const std::string &eventMsg);
};

using UbseEventDistributePtr = Ref<UbseEventDistribute>;
} // namespace ubse::event

#endif // UBSE_EVENT_DISTRIBUTE_H
