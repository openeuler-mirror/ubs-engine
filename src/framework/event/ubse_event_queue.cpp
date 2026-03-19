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

#include "ubse_event_queue.h"
namespace ubse::event {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");
void UbseEventQueue::AddEventTask(EventTask &eventTask)
{
    std::unique_lock<std::mutex> lock(eventQueueMutex_);
    notFull_.wait(lock, [this]() { return queue_.size() < capacity_ || !isQueueRunning_.load(); });
    if (!isQueueRunning_.load()) {
        UBSE_LOG_INFO << "event thread queue stop";
        return;
    }
    queue_.push_back(std::move(eventTask));
    notEmpty_.notify_one();
}

EventTask UbseEventQueue::GetTask()
{
    std::unique_lock<std::mutex> lock(eventQueueMutex_);
    // 等待队列不为空，队列为空时，线程休眠
    notEmpty_.wait(lock, [this]() { return !queue_.empty() || !isQueueRunning_.load(); });
    if (!isQueueRunning_.load()) {
        UBSE_LOG_INFO << "event thread queue stop";
        return {};
    }
    EventTask eventTask = std::move(queue_.front());
    queue_.pop_front();
    notFull_.notify_one();
    return eventTask;
}
void UbseEventQueue::StopQueue()
{
    isQueueRunning_.store(false);
    notFull_.notify_all();
    notEmpty_.notify_all();
}
}
