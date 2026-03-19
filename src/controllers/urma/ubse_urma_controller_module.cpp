/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_urma_controller_module.h"
#include "ubse_com_module.h"
#include "ubse_common_def.h"
#include "ubse_context.h"
#include "ubse_event.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_module.h"
#include "ubse_thread_pool_module.h"
#include "ubse_timer.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_api.h"
#include "ubse_urma_controller_rpc.h"

namespace ubse::urmaController {
using namespace ubse::log;
using namespace ubse::module;
using namespace ubse::task_executor;
using namespace ubse::com;
using namespace ubse::common::def;
using namespace ubse::nodeController;

std::atomic<uint32_t> g_asyncHandlerCnt{0};

DYNAMIC_CREATE(UbseUrmaControllerModule, ubse::nodeController::UbseNodeControllerModule);
UBSE_DEFINE_THIS_MODULE("ubse");

AsyncHandlerGuard::AsyncHandlerGuard() : guardCnt(g_asyncHandlerCnt)
{
    guardCnt.fetch_add(1, std::memory_order_relaxed);
}

AsyncHandlerGuard::AsyncHandlerGuard(std::atomic<uint32_t> &cnt) : guardCnt(cnt)
{
    guardCnt.fetch_add(1, std::memory_order_relaxed);
}

AsyncHandlerGuard::~AsyncHandlerGuard()
{
    guardCnt.fetch_sub(1, std::memory_order_relaxed);
}

UbseResult RpcReg()
{
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }

    UbseComBaseMessageHandlerPtr pReportUrmaNodeInfoMessageHandler = new (std::nothrow)
        UbseUrmaReportUrmaNodeInfoMessageHandler;

    UbseComBaseMessageHandlerPtr pBrocastUrmaInfoHandler = new (std::nothrow) UbseUrmaNotifyMessageHandler();

    UbseComBaseMessageHandlerPtr pQueryMessageHandler = new (std::nothrow) UbseUrmaQueryMessageHandler();

    UbseComBaseMessageHandlerPtr pQueryDevHandler = new (std::nothrow) UbseUrmaDevQueryMessageHandler();

    UbseComBaseMessageHandlerPtr pActivateUrmaInfoHandler = new (std::nothrow) UbseUrmaActivateUrmaInfoMessageHandler();

    if (pReportUrmaNodeInfoMessageHandler == nullptr || pBrocastUrmaInfoHandler == nullptr ||
        pQueryMessageHandler == nullptr || pQueryDevHandler == nullptr || pActivateUrmaInfoHandler == nullptr) {
        UBSE_LOG_ERROR << "Fail to create UbseComBaseMessageHandler";
        return UBSE_ERROR_NULLPTR;
    }

    auto ret = comModule->RegRpcService<UbseUrmaBrocastReqSimpo, UbseUrmaBrocastRspSimpo>(pBrocastUrmaInfoHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Reg pBrocastUrmaInfoHandler failed.";
        return ret;
    }

    ret = comModule->RegRpcService<UbseUrmaQueryReqSimpo, UbseUrmaQueryRspSimpo>(pQueryMessageHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Reg pQueryMessageHandler  failed.";
        return ret;
    }

    ret = comModule->RegRpcService<UrmaDevQueryReqSimpo, UrmaDevQueryRspSimpo>(pQueryDevHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Reg pQueryDevHandler  failed.";
        return ret;
    }

    ret = comModule->RegRpcService<UbseUrmaReportUrmaNodeInfoReqSimpo, UbseUrmaReportUrmaNodeInfoRspSimpo>(
        pReportUrmaNodeInfoMessageHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to register report urma info handler for rpc";
        return ret;
    }

    ret = comModule->RegRpcService<UbseUrmaActivateUrmaInfoReqSimpo, UbseUrmaActivateUrmaInfoRspSimpo>(
        pActivateUrmaInfoHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to register activate urma info handler for rpc";
        return ret;
    }
    return UBSE_OK;
}

UbseResult UbseUrmaControllerModule::Initialize()
{
    // 注册消息处理函数,监听 topo变化事件
    auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = taskExecutor->Create("UrmaExecutor", NO_4, NO_128);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to create HeartBeat Executor";
        return UBSE_ERROR_CONF_INVALID;
    }
    ret = UbseUrmaControllerApi::Register();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseUrmaControllerApi failed," << FormatRetCode(ret);
        return ret;
    }
    if (RpcReg() != UBSE_OK) {
        return UBSE_ERROR;
    }

    std::string nodeJoinEventId = UBSE_EVENT_NODE_JOIN;
    ret = ubse::event::UbseSubEvent(nodeJoinEventId,
                                    ubse::urmaController::UrmaController::GetInstance().UbseNodeJoinHandler,
                                    ubse::event::UbseEventPriority::HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to Follow the event=" << nodeJoinEventId;
        return ret;
    }

    std::string nodeTopoLinkChangeEventId = UBSE_EVENT_NODE_TOPO_LINK_CHANGE;
    ret = ubse::event::UbseSubEvent(nodeTopoLinkChangeEventId,
                                    ubse::urmaController::UrmaController::GetInstance().UbseTopoLinkChangeHandler,
                                    ubse::event::UbseEventPriority::HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to Follow the event=" << nodeTopoLinkChangeEventId;
    }
    return ret;
}

void UbseUrmaControllerModule::UnInitialize() {}

void DisconnectAllNormalLink()
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_WARN << "Failed to get com module";
        return;
    }
    auto allNodes = UbseNodeController::GetInstance().GetAllNodes();
    for (const auto &node : allNodes) {
        comModule->RemoveChannel(node.second.nodeId, UbseChannelType::NORMAL);
    }
}

UbseResult UbseUrmaControllerModule::Start()
{
    return UBSE_OK;
}

void UbseUrmaControllerModule::Stop()
{
    std::string nodeJoinEventId = UBSE_EVENT_NODE_JOIN;
    auto ret = ubse::event::UbseUnSubEvent(nodeJoinEventId,
                                           ubse::urmaController::UrmaController::GetInstance().UbseNodeJoinHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to unsub the event=" << nodeJoinEventId;
    }

    std::string nodeTopoLinkChangeEventId = UBSE_EVENT_NODE_TOPO_LINK_CHANGE;
    ret = ubse::event::UbseUnSubEvent(nodeTopoLinkChangeEventId,
                                      ubse::urmaController::UrmaController::GetInstance().UbseTopoLinkChangeHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to unsub the event=" << nodeTopoLinkChangeEventId;
    }
    DisconnectAllNormalLink();
    while (g_asyncHandlerCnt != 0) {
        UBSE_LOG_DEBUG << "There are async operation, wait to stop";
        sleep(1);
    }
    return;
}

UbseResult DoTaskWithTimerCallback(const std::string &timerName, UbseUrmaRetryTaskHandler task)
{
    auto ret = task();
    if (ret == UBSE_OK) {
        UBSE_LOG_INFO << "Do timer task success, timer name=" << timerName;
        ubse::timer::UbseTimerHandlerUnregister(timerName);
        return UBSE_OK;
    }
    UBSE_LOG_WARN << "Do timer task failed, timer name=" << timerName << ", retry later";
    if (g_globalStop) {
        ubse::timer::UbseTimerHandlerUnregister(timerName);
    }
    return ret;
}

UbseResult HandleTaskWithRetry(const std::string &executorName, const std::string &taskName, uint32_t timerInterval,
                               UbseUrmaRetryTaskHandler task)
{
    UBSE_LOG_INFO << "HandleTaskWithRetry start, taskName=" << taskName;
    if (task() == UBSE_OK) {
        UBSE_LOG_INFO << "Do task success, taskName=" << taskName;
        return UBSE_OK;
    }
    UBSE_LOG_WARN << "Do task failed, taskName=" << taskName << ", retry later";
    auto ret = ubse::timer::UbseTimerHandlerRegister(
        taskName,
        [executorName, taskName, task]() {
            auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
            if (taskExecutor == nullptr) {
                UBSE_LOG_ERROR << "Get task executor failed";
                return UBSE_ERROR_NULLPTR;
            }
            auto urmaExecutor = taskExecutor->Get(executorName);
            if (urmaExecutor == nullptr) {
                UBSE_LOG_ERROR << "Get task executor for urma failed";
                return UBSE_ERROR_NULLPTR;
            }
            urmaExecutor->Execute([taskName, task]() { return DoTaskWithTimerCallback(taskName, task); });
            return UBSE_OK;
        },
        timerInterval);
    return ret;
}
} // namespace ubse::urmaController
