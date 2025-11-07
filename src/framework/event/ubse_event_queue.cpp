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
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_EVENT_MID)
void UbseEventQueue::AddEventTask(EventTask &eventTask)
{
    std::unique_lock<std::mutex> lock(eventQueueMutex);
    notFull.wait(lock, [this]() { return queue.size() < capacity || !isQueueRunning.load(); });
    if (!isQueueRunning.load()) {
        UBSE_LOG_INFO << "event thread queue stop";
        return;
    }
    queue.push_back(std::move(eventTask));
    notEmpty.notify_one();
}

EventTask UbseEventQueue::GetTask()
{
    std::unique_lock<std::mutex> lock(eventQueueMutex);
    // 等待队列不为空，队列为空时，线程休眠
    notEmpty.wait(lock, [this]() { return !queue.empty() || !isQueueRunning.load(); });
    if (!isQueueRunning.load()) {
        UBSE_LOG_INFO << "event thread queue stop";
        return {};
    }
    EventTask eventTask = std::move(queue.front());
    queue.pop_front();
    notFull.notify_one();
    return eventTask;
}
void UbseEventQueue::StopQueue()
{
    isQueueRunning.store(false);
    notFull.notify_all();
    notEmpty.notify_all();
}
}
