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
using ubse::common::def::UbseResult;
using ubse::utils::Ref;
using ubse::utils::Referable;

class UbseEventDistribute : public Referable {
public:
    explicit UbseEventDistribute() = default;

    explicit UbseEventDistribute(uint32_t capacity, uint32_t numsHighThs, uint32_t numsMidThs, uint32_t numsLowThs)
        : eventQueueCapacity_(capacity),
          numsHighThs_(numsHighThs),
          numsMidThs_(numsMidThs),
          numsLowThs_(numsLowThs){};

    void RegisterSubscribe(const std::string& eventId, UbseEventPriority priority, UbseEventHandler registerFunc);

    void UnRegisterSubscribe(const std::string& eventId, UbseEventHandler registerFunc);

    inline bool IsRegisteredSubscribe(const std::string& eventId)
    {
        std::shared_lock<std::shared_mutex> readLock(eventSubMutex_);
        return subscribes_.find(eventId) != subscribes_.end();
    }

    void PubEvent(const std::string& eventId, const std::string& eventMsg);

    void EventDistribute();

    void Distribute(EventTask& task);

    UbseResult Init();

    UbseResult Start();

    void RunDistribute();

    void Stop();

private:
    UbseEventThreadPoolPtr poolPtr_ = nullptr;
    uint32_t eventQueueCapacity_;
    uint32_t numsHighThs_;
    uint32_t numsMidThs_;
    uint32_t numsLowThs_;
    std::map<std::string, std::vector<EventTask>> subscribes_;
    std::deque<std::tuple<std::string, std::string, std::string>> events_;
    std::atomic<bool> threadRunning_{false};
    std::mutex eventPubMutex_;
    std::shared_mutex eventSubMutex_;
    std::condition_variable notEmpty_;
    std::thread distributeThread_;

private:
    std::map<std::string, uint32_t> congestCntMap_;

    void MonitorCongestion(const std::string& eventId, const std::string& eventMsg);
};

using UbseEventDistributePtr = Ref<UbseEventDistribute>;
} // namespace ubse::event

#endif // UBSE_EVENT_DISTRIBUTE_H
