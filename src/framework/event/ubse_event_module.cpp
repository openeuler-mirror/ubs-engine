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

#include "ubse_event_module.h"

#include <referable/ubse_ref.h> // for Ref
#include <cstdint>              // for uint32_t
#include <new>                  // for nothrow

#include "ubse_conf_module.h"      // for UbseConfModule
#include "ubse_context.h"          // for UbseContext, ProcessMode
#include "ubse_error.h"            // for UBSE_OK, UBSE_ERROR_NULLPTR
#include "ubse_event_distribute.h" // for UbseEventDistribute, UbseEventD...
#include "ubse_logger.h"           // for UbseLoggerEntry, FormatRetCode
#include "ubse_logger_module.h"
#include "ubse_pointer_process.h"

namespace ubse::event {
using namespace ubse::context;
using namespace ubse::config;
using namespace ubse::log;

UBSE_DEFINE_THIS_MODULE("ubse");
BASE_DYNAMIC_CREATE(UbseEventModule, UbseConfModule, UbseLoggerModule);

class UbseEventModule::Impl {
public:
    void UbseEventConfigCheck(uint32_t& queueMaxItem, uint32_t& highThreadMaxItem, uint32_t& mediumThreadMaxItem,
                              uint32_t& lowThreadMaxItem);
    UbseResult InitDistributePtr();

    UbseEventDistributePtr distributePtr_ = nullptr;
};

UbseEventModule::UbseEventModule() = default;
UbseEventModule::~UbseEventModule() = default;

UbseResult UbseEventModule::Initialize()
{
    pImpl_ = SafeMakeUnique<Impl>();
    if (pImpl_ == nullptr) {
        UBSE_LOG_ERROR << "Allocate memory failed, " << FormatRetCode(UBSE_ERROR_NULLPTR);
        return UBSE_ERROR_NULLPTR;
    }

    UbseResult res = pImpl_->InitDistributePtr();
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "InitDistributePtr failed, " << FormatRetCode(res);
        return res;
    }

    res = pImpl_->distributePtr_->Init();
    if (res != UBSE_OK) {
        UBSE_LOG_ERROR << "init distribute failed, " << FormatRetCode(res);
    }
    return res;
}

UbseResult UbseEventModule::Start()
{
    if (pImpl_ == nullptr) {
        UBSE_LOG_ERROR << "pImpl is null, Start failed";
        return UBSE_ERROR_NULLPTR;
    }

    if (pImpl_->distributePtr_ == nullptr) {
        UBSE_LOG_ERROR << "get distributePtr failed, Start failed";
        return UBSE_ERROR_NULLPTR;
    }
    return pImpl_->distributePtr_->Start();
}

void UbseEventModule::Stop()
{
    if (pImpl_ == nullptr) {
        UBSE_LOG_ERROR << "pImpl is null, Stop failed";
        return;
    }

    if (pImpl_->distributePtr_ == nullptr) {
        UBSE_LOG_ERROR << "get distributePtr failed, Stop failed";
        return;
    }
    pImpl_->distributePtr_->Stop();
}

void UbseEventModule::UnInitialize() {}

UbseResult UbseEventModule::UbseSubEvent(const std::string& eventId, UbseEventHandler registerFunc,
                                         UbseEventPriority priority)
{
    if (pImpl_->distributePtr_ == nullptr) {
        UBSE_LOG_ERROR << "get distributePtr failed, SubEvent failed";
        return UBSE_ERROR_NULLPTR;
    }
    pImpl_->distributePtr_->RegisterSubscribe(eventId, priority, std::move(registerFunc));
    return UBSE_OK;
}

UbseResult UbseEventModule::UbsePubEvent(const std::string& eventId, std::string& eventMessage)
{
    if (pImpl_->distributePtr_ == nullptr) {
        UBSE_LOG_ERROR << "get distributePtr failed, SubEvent failed";
        return UBSE_ERROR_NULLPTR;
    }
    pImpl_->distributePtr_->PubEvent(eventId, eventMessage);
    return UBSE_OK;
}

UbseResult UbseEventModule::UbseUnSubEvent(const std::string& eventId, UbseEventHandler registerFunc)
{
    if (pImpl_ == nullptr) {
        UBSE_LOG_ERROR << "pImpl is null, UnSubEvent failed";
        return UBSE_ERROR_NULLPTR;
    }

    if (pImpl_->distributePtr_ == nullptr) {
        UBSE_LOG_ERROR << "get distributePtr failed, UnSubEvent failed";
        return UBSE_ERROR_NULLPTR;
    }

    pImpl_->distributePtr_->UnRegisterSubscribe(eventId, registerFunc);
    return UBSE_OK;
}

UbseResult UbseEventModule::Impl::InitDistributePtr()
{
    auto& ctxRef = UbseContext::GetInstance();
    auto confRef = ctxRef.GetModule<UbseConfModule>();
    if (confRef == nullptr) {
        UBSE_LOG_ERROR << "get confRef failed, Initialize failed";
        return UBSE_ERROR_NULLPTR;
    }

    uint32_t queueMaxItem{1024};
    uint32_t highThreadMaxItem{10};
    uint32_t mediumThreadMaxItem{5};
    uint32_t lowThreadMaxItem{2};

    UbseResult res = confRef->GetConf<uint32_t>(UBSE_EVENT_SECTION, UBSE_EVENT_QUEUE_MAX_ITEM, queueMaxItem);
    if (res != UBSE_OK) {
        UBSE_LOG_WARN << "get event queue.maxItem from config failed, " << FormatRetCode(res)
                      << ", will use default value : " << queueMaxItem;
    }
    res = confRef->GetConf<uint32_t>(UBSE_EVENT_SECTION, UBSE_EVENT_HIGH_THREAD_MAX_ITEM, highThreadMaxItem);
    if (res != UBSE_OK) {
        UBSE_LOG_WARN << "get event highThreadMaxItem from config failed, " << FormatRetCode(res)
                      << ", will use default value : " << highThreadMaxItem;
    }
    res = confRef->GetConf<uint32_t>(UBSE_EVENT_SECTION, UBSE_EVENT_MEDIUM_THREAD_MAX_ITEM, mediumThreadMaxItem);
    if (res != UBSE_OK) {
        UBSE_LOG_WARN << "get event mediumThreadMaxItem from config failed, " << FormatRetCode(res)
                      << ", will use default value : " << mediumThreadMaxItem;
    }
    res = confRef->GetConf<uint32_t>(UBSE_EVENT_SECTION, UBSE_EVENT_LOW_THREAD_MAX_ITEM, lowThreadMaxItem);
    if (res != UBSE_OK) {
        UBSE_LOG_WARN << "get event lowThreadMaxItem from config failed, " << FormatRetCode(res)
                      << ", will use default value : " << lowThreadMaxItem;
    }
    UbseEventConfigCheck(queueMaxItem, highThreadMaxItem, mediumThreadMaxItem, lowThreadMaxItem);
    distributePtr_.Set(new (std::nothrow)
                           UbseEventDistribute(queueMaxItem, highThreadMaxItem, mediumThreadMaxItem, lowThreadMaxItem));
    if (distributePtr_ == nullptr) {
        UBSE_LOG_ERROR << "new distribute failed";
        return UBSE_ERROR_NULLPTR;
    }

    return UBSE_OK;
}

void UbseEventModule::Impl::UbseEventConfigCheck(uint32_t& queueMaxItem, uint32_t& highThreadMaxItem,
                                                 uint32_t& mediumThreadMaxItem, uint32_t& lowThreadMaxItem)
{
    if (queueMaxItem < 64 || queueMaxItem > 4096) { // 事件队列长度在64到4096之间
        queueMaxItem = 1024;                        // 1024为默认值
        UBSE_LOG_WARN << "queueMaxItem should be between 64 and 4096, use default value: 1024";
    }
    if (highThreadMaxItem < 2 || highThreadMaxItem > 16) { // 线程个数在2到16之间
        highThreadMaxItem = 10;                            // 10为高优先级线程默认值
        UBSE_LOG_WARN << "highThreadMaxItem should be between 2 and 16, user default value: 10";
    }
    if (mediumThreadMaxItem < 2 || mediumThreadMaxItem > 16) { // 线程个数在2到16之间
        mediumThreadMaxItem = 5;                               // 5为中优先级线程默认值
        UBSE_LOG_WARN << "mediumThreadMaxItem should be between 2 and 16, user default value: 10";
    }
    if (lowThreadMaxItem < 2 || lowThreadMaxItem > 16) { // 线程个数在2到16之间
        lowThreadMaxItem = 2;                            // 2为低优先级线程默认值
        UBSE_LOG_WARN << "lowThreadMaxItem should be between 2 and 16, user default value: 10";
    }
}
} // namespace ubse::event