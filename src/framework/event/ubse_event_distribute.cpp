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

#include "ubse_event_distribute.h"

namespace ubse::event {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_EVENT_MID)

void UbseEventDistribute::RegisterSubscribe(const std::string &eventId, UbseEventPriority priority,
    UbseEventHandler registerFunc)
{
    if (registerFunc == nullptr) {
        UBSE_LOG_WARN << "event id: " << eventId << " register func null";
        return;
    }
    std::unique_lock<std::shared_mutex> writeLock(eventSubMutex);
    EventTask task = EventTask{
        .eventId = eventId,
        .priority = priority,
        .registerFunc = std::move(registerFunc),
    };
    subscribes[eventId].push_back(std::move(task));
}


void UbseEventDistribute::UnRegisterSubscribe(const std::string &eventId, UbseEventHandler registerFunc)
{
    std::unique_lock<std::shared_mutex> writeLock(eventSubMutex);
    auto *targetFunc = registerFunc.target<uint32_t (*)(std::string &, std::string &)>();
    if (targetFunc == nullptr) {
        UBSE_LOG_WARN << "register function is nil, will skip.";
        return;
    }
    if (subscribes.find(eventId) == subscribes.end()) {
        return;
    }

    // 使用迭代器遍历并删除，避免索引越界问题
    auto &handlers = subscribes[eventId];
    auto it = handlers.begin();
    while (it != handlers.end()) {
        // 获取需要比较的函数指针
        auto *storedFunc = it->registerFunc.target<uint32_t (*)(std::string &, std::string &)>();

        // 如果任意一个为 nullptr（类型不匹配），或者二者相等，移除
        if (storedFunc == nullptr || *storedFunc == *targetFunc) {
            it = handlers.erase(it); // 删除当前元素并更新迭代器
        } else {
            ++it;
        }
    }
}


void UbseEventDistribute::PubEvent(const std::string &eventId, const std::string &eventMsg)
{
    if (!IsRegisteredSubscribe(eventId)) {
        return;
    }
    std::unique_lock<std::mutex> lock(eventPubMutex);
    MonitorCongestion(eventId, eventMsg);
    events.emplace_back(std::pair<std::string, std::string>{ eventId, eventMsg });
    notEmpty.notify_one();
}

void UbseEventDistribute::Distribute(EventTask &task)
{
    if (poolPtr == nullptr) {
        UBSE_LOG_ERROR << "event thread pool not init, event " << task.eventId << " will ignore";
        return;
    }
    switch (task.priority) {
        case UbseEventPriority::HIGH:
            poolPtr->highPriorityQueue.AddEventTask(task);
            break;
        case UbseEventPriority::MEDIUM:
            poolPtr->mediumPriorityQueue.AddEventTask(task);
            break;
        case UbseEventPriority::LOW:
            poolPtr->lowPriorityQueue.AddEventTask(task);
            break;
        default:
            UBSE_LOG_ERROR << "event: " << task.eventId << " priority: " << task.priority << " not exists";
            break;
    }
}

void UbseEventDistribute::EventDistribute()
{
    std::unique_lock<std::mutex> lock(eventPubMutex);
    notEmpty.wait(lock, [this]() { return !events.empty() || !threadRunning.load(); });
    if (!threadRunning.load()) {
        UBSE_LOG_INFO << "distribute thread stop";
        return;
    }

    std::pair<std::string, std::string> event = std::move(events.front());
    std::vector<EventTask> eventVec;
    eventPubMutex.unlock();
    {
        std::shared_lock<std::shared_mutex> readLock(eventSubMutex);
        auto it = subscribes.find(event.first);
        if (it != subscribes.end()) {
            eventVec = it->second;
        }
    }
    for (size_t i = 0; i < eventVec.size(); i++) {
        eventVec[i].eventMessage = event.second;
        Distribute(eventVec[i]);
    }
    eventPubMutex.lock();
    auto it = congestCntMap.find(event.first);
    if (it != congestCntMap.end()) {
        it->second--;
        if (it->second == NO_0) {
            congestCntMap.erase(it);
        }
    }
    events.pop_front();
}

UbseResult UbseEventDistribute::Init()
{
    if (poolPtr == nullptr) {
        poolPtr.Set(new (std::nothrow) UbseEventThreadPool(numsHighThs, numsMidThs, numsLowThs));
        if (poolPtr == nullptr) {
            UBSE_LOG_ERROR << "new event thread pool failed";
            return UBSE_ERROR_NULLPTR;
        }
    }
    UbseResult ret = poolPtr->Init();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "init event queue failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult UbseEventDistribute::Start()
{
    try {
        distributeThread = std::thread(&UbseEventDistribute::RunDistribute, this);
        threadRunning.store(true);
    } catch (const std::system_error &) {
        UBSE_LOG_ERROR << "start thread failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseEventDistribute::Stop()
{
    threadRunning.store(false);
    if (distributeThread.joinable()) {
        eventPubMutex.lock();
        notEmpty.notify_one();
        eventPubMutex.unlock();
        distributeThread.join();
    }
    if (poolPtr != nullptr) {
        poolPtr->Stop();
    }
}

void UbseEventDistribute::RunDistribute()
{
    while (threadRunning.load()) {
        EventDistribute();
    }
}


void UbseEventDistribute::MonitorCongestion(const std::string &eventId, const std::string &eventMsg)
{
    // 总量控制
    if (UBSE_UNLIKELY(events.size() >= eventQueueCapacity)) {
        if ((events.size() - eventQueueCapacity) % NO_128 == NO_0) { // 超过队列上限时，每新增128条即打印一次队列内的事件组成
            std::ostringstream oos;
            for (const auto &eventCnt : congestCntMap) {
                oos << "[EventId:" << eventCnt.first << ", num:" << eventCnt.second << "]";
            }
            UBSE_LOG_WARN << oos.str();
        }
    }
    auto it = congestCntMap.find(eventId);
    if (it == congestCntMap.end()) {
        congestCntMap[eventId] = NO_1;
        return;
    }
    it->second++;
}
}