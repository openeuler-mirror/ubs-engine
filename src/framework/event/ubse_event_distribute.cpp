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
#include "trace_context.h"

namespace ubse::event {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");

void UbseEventDistribute::RegisterSubscribe(const std::string& eventId, UbseEventPriority priority,
                                            UbseEventHandler registerFunc)
{
    if (registerFunc == nullptr) {
        UBSE_LOG_WARN << "event id: " << eventId << " register func null";
        return;
    }
    std::unique_lock<std::shared_mutex> writeLock(eventSubMutex_);
    EventTask task = EventTask{
        .eventId = eventId,
        .priority = priority,
        .registerFunc = std::move(registerFunc),
    };
    subscribes_[eventId].push_back(std::move(task));
}

void UbseEventDistribute::UnRegisterSubscribe(const std::string& eventId, UbseEventHandler registerFunc)
{
    std::unique_lock<std::shared_mutex> writeLock(eventSubMutex_);
    auto* targetFunc = registerFunc.target<uint32_t (*)(std::string&, std::string&)>();
    if (targetFunc == nullptr) {
        UBSE_LOG_WARN << "register function is nil, will skip.";
        return;
    }
    if (subscribes_.find(eventId) == subscribes_.end()) {
        return;
    }

    // 使用迭代器遍历并删除，避免索引越界问题
    auto& handlers = subscribes_[eventId];
    auto it = handlers.begin();
    while (it != handlers.end()) {
        // 获取需要比较的函数指针
        auto* storedFunc = it->registerFunc.target<uint32_t (*)(std::string&, std::string&)>();

        // 如果任意一个为 nullptr（类型不匹配），或者二者相等，移除
        if (storedFunc == nullptr || *storedFunc == *targetFunc) {
            it = handlers.erase(it); // 删除当前元素并更新迭代器
        } else {
            ++it;
        }
    }
}

void UbseEventDistribute::PubEvent(const std::string& eventId, const std::string& eventMsg)
{
    if (!IsRegisteredSubscribe(eventId)) {
        return;
    }
    std::unique_lock<std::mutex> lock(eventPubMutex_);
    MonitorCongestion(eventId, eventMsg);
    auto traceId = TraceContext::GetTraceId();
    events_.emplace_back(eventId, eventMsg, traceId);
    notEmpty_.notify_one();
}

void UbseEventDistribute::Distribute(EventTask& task)
{
    // 首先检查 task 是否有效
    if (&task == nullptr) { // 检查引用是否为 null，虽然引用通常不能为null，但可以检查地址。
        UBSE_LOG_ERROR << "task reference is invalid";
        return;
    }

    if (poolPtr_ == nullptr) {
        // 安全地访问 task.eventId
        UBSE_LOG_ERROR << "event thread pool not init, event " << task.eventId << " will ignore";
        return;
    }

    switch (task.priority) {
        case UbseEventPriority::HIGH:
            poolPtr_->highPriorityQueue_.AddEventTask(task);
            break;
        case UbseEventPriority::MEDIUM:
            poolPtr_->mediumPriorityQueue_.AddEventTask(task);
            break;
        case UbseEventPriority::LOW:
            poolPtr_->lowPriorityQueue_.AddEventTask(task);
            break;
        default:
            UBSE_LOG_ERROR << "event: " << task.eventId << " priority: " << task.priority << " not exists";
            break;
    }
}

void UbseEventDistribute::EventDistribute()
{
    std::unique_lock<std::mutex> lock(eventPubMutex_);
    notEmpty_.wait(lock, [this]() { return !events_.empty() || !threadRunning_.load(); });
    if (!threadRunning_.load()) {
        UBSE_LOG_INFO << "distribute thread stop";
        return;
    }

    auto event = std::move(events_.front());
    std::vector<EventTask> eventVec;
    eventPubMutex_.unlock();
    {
        std::shared_lock<std::shared_mutex> readLock(eventSubMutex_);
        auto it = subscribes_.find(std::get<0>(event));
        if (it != subscribes_.end()) {
            eventVec = it->second;
        }
    }
    const size_t traceIdIdx = NO_2; // traceId在tuple中的索引
    for (size_t i = 0; i < eventVec.size(); i++) {
        eventVec[i].eventId = std::get<0>(event);
        eventVec[i].eventMessage = std::get<1>(event);
        eventVec[i].traceId = std::get<traceIdIdx>(event);
        Distribute(eventVec[i]);
    }
    eventPubMutex_.lock();
    auto it = congestCntMap_.find(std::get<0>(event));
    if (it != congestCntMap_.end()) {
        it->second--;
        if (it->second == NO_0) {
            congestCntMap_.erase(it);
        }
    }
    events_.pop_front();
}

UbseResult UbseEventDistribute::Init()
{
    if (poolPtr_ == nullptr) {
        poolPtr_.Set(new (std::nothrow) UbseEventThreadPool(numsHighThs_, numsMidThs_, numsLowThs_));
        if (poolPtr_ == nullptr) {
            UBSE_LOG_ERROR << "new event thread pool failed";
            return UBSE_ERROR_NULLPTR;
        }
    }
    UbseResult ret = poolPtr_->Init();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "init event queue failed, " << FormatRetCode(ret);
    }
    return ret;
}

UbseResult UbseEventDistribute::Start()
{
    try {
        threadRunning_.store(true);
        distributeThread_ = std::thread(&UbseEventDistribute::RunDistribute, this);
    } catch (const std::system_error&) {
        UBSE_LOG_ERROR << "start thread failed, " << FormatRetCode(UBSE_ERROR);
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void UbseEventDistribute::Stop()
{
    threadRunning_.store(false);
    if (distributeThread_.joinable()) {
        eventPubMutex_.lock();
        notEmpty_.notify_one();
        eventPubMutex_.unlock();
        distributeThread_.join();
    }
    if (poolPtr_ != nullptr) {
        poolPtr_->Stop();
    }
}

void UbseEventDistribute::RunDistribute()
{
    while (threadRunning_.load()) {
        EventDistribute();
    }
}

void UbseEventDistribute::MonitorCongestion(const std::string& eventId, const std::string& eventMsg)
{
    // 总量控制
    if (UBSE_UNLIKELY(events_.size() >= eventQueueCapacity_)) {
        if ((events_.size() - eventQueueCapacity_) % NO_128 ==
            NO_0) { // 超过队列上限时，每新增128条即打印一次队列内的事件组成
            std::ostringstream oos;
            for (const auto& eventCnt : congestCntMap_) {
                oos << "[EventId:" << eventCnt.first << ", num:" << eventCnt.second << "]";
            }
            UBSE_LOG_WARN << oos.str();
        }
    }
    auto it = congestCntMap_.find(eventId);
    if (it == congestCntMap_.end()) {
        congestCntMap_[eventId] = NO_1;
        return;
    }
    it->second++;
}
} // namespace ubse::event