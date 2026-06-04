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
#include "ubse_conf.h"
#include "ubse_context.h"
#include "ubse_event.h"
#include "ubse_logger.h"
#include "ubse_node_controller.h"
#include "ubse_node_controller_module.h"
#include "ubse_thread_pool_module.h"
#include "ubse_timer.h"
#include "ubse_urma_controller.h"
#include "ubse_urma_controller_api.h"
#include "ubse_urma_controller_qos.h"
#include "ubse_urma_controller_rpc.h"
#include "ubse_urma_controller_util.h"

namespace ubse::urmaController {
using namespace ubse::log;
using namespace ubse::module;
using namespace ubse::task_executor;
using namespace ubse::com;
using namespace ubse::common::def;
using namespace ubse::nodeController;
using namespace ubse::config;
using namespace ubse::context;

DYNAMIC_CREATE(UbseUrmaControllerModule, ubse::nodeController::UbseNodeControllerModule);
UBSE_DEFINE_THIS_MODULE("ubse");

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

    if (pReportUrmaNodeInfoMessageHandler == nullptr || pBrocastUrmaInfoHandler == nullptr ||
        pQueryMessageHandler == nullptr || pQueryDevHandler == nullptr) {
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
    return UBSE_OK;
}

UbseResult UbseUrmaControllerModule::Initialize()
{
    auto ret = UbseUrmaControllerApi::Register();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseUrmaControllerApi failed," << FormatRetCode(ret);
        return ret;
    }
    enabled_ = UbseIsUrmaSupported();
    if (!enabled_) {
        UBSE_LOG_INFO << "Urma is not supported in current environment, skip initialize UrmaControllerModule.";
        return UBSE_OK;
    }
    // 注册消息处理函数,监听 topo变化事件
    auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    ret = taskExecutor->Create("UrmaExecutor", NO_4, NO_128);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to create HeartBeat Executor";
        return UBSE_ERROR_CONF_INVALID;
    }
    if (RpcReg() != UBSE_OK) {
        return UBSE_ERROR;
    }

    std::string nodeJoinEventId = UBSE_EVENT_NODE_JOIN;
    ret = ubse::event::UbseSubEvent(nodeJoinEventId,
                                    ubse::urmaController::UbseUrmaController::GetInstance().UbseNodeJoinHandler,
                                    ubse::event::UbseEventPriority::HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to Follow the event=" << nodeJoinEventId;
        return ret;
    }

    std::string nodeTopoLinkChangeEventId = UBSE_EVENT_NODE_TOPO_LINK_CHANGE;
    ret = ubse::event::UbseSubEvent(nodeTopoLinkChangeEventId,
                                    ubse::urmaController::UbseUrmaController::GetInstance().UbseTopoLinkChangeHandler,
                                    ubse::event::UbseEventPriority::HIGH);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to Follow the event=" << nodeTopoLinkChangeEventId;
    }
    // 初始化QoS配置，执行失败后异步重试，因此不判断返回值
    (void)UbseUrmaControllerQos<EtsQosConfig>::GetInstance().UbseUrmaQosInit();
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
    for (const auto& node : allNodes) {
        comModule->RemoveChannel(node.second.nodeId, UbseChannelType::NORMAL);
    }
}

UbseResult UbseUrmaControllerModule::Start()
{
    return UBSE_OK;
}

void UbseUrmaControllerModule::Stop()
{
    if (!enabled_) {
        UBSE_LOG_INFO << "Urma is not supported in current environment, skip stop UrmaControllerModule.";
        return;
    }
    std::string nodeJoinEventId = UBSE_EVENT_NODE_JOIN;
    auto ret = ubse::event::UbseUnSubEvent(nodeJoinEventId,
                                           ubse::urmaController::UbseUrmaController::GetInstance().UbseNodeJoinHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to unsub the event=" << nodeJoinEventId;
    }

    std::string nodeTopoLinkChangeEventId = UBSE_EVENT_NODE_TOPO_LINK_CHANGE;
    ret = ubse::event::UbseUnSubEvent(
        nodeTopoLinkChangeEventId, ubse::urmaController::UbseUrmaController::GetInstance().UbseTopoLinkChangeHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to unsub the event=" << nodeTopoLinkChangeEventId;
    }
    DisconnectAllNormalLink();
    WaitAndCleanupRetryTasks();

    auto taskExecutor = UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor != nullptr) {
        taskExecutor->Remove("UrmaExecutor");
    }
}
} // namespace ubse::urmaController
