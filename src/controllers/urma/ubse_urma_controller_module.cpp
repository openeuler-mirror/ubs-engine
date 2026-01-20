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
#include "ubse_node_module.h"
#include "ubse_thread_pool_module.h"
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

DYNAMIC_CREATE(UbseUrmaControllerModule, ubse::node::UbseNodeModule);
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_URMA_CONTROLLER_MID)

UbseResult UbseUrmaControllerModule::Initialize()
{
    // 注册消息处理函数,监听 topo变化事件
    auto taskExecutor = ubse::context::UbseContext::GetInstance().GetModule<UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = taskExecutor->Create("UrmaExecutor", NO_2, NO_128);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to create HeartBeat Executor";
        return UBSE_ERROR_CONF_INVALID;
    }
    return ret;
}

void UbseUrmaControllerModule::UnInitialize() {}

UbseResult RpcReg()
{
    auto comModule = ubse::context::UbseContext::GetInstance().GetModule<ubse::com::UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Getting ComModule failed.";
        return UBSE_ERROR_NULLPTR;
    }

    UbseComBaseMessageHandlerPtr pReportUrmaNodeInfoMessageHandler = new (std::nothrow)
        UbseUrmaReportUrmaNodeInfoMessageHandler;

    UbseComBaseMessageHandlerPtr pNotifyMessageHandler = new (std::nothrow) UbseUrmaNotifyMessageHandler();

    UbseComBaseMessageHandlerPtr pQueryMessageHandler = new (std::nothrow) UbseUrmaQueryMessageHandler();

    UbseComBaseMessageHandlerPtr pQueryDevHandler = new (std::nothrow) UbseUrmaDevQueryMessageHandler();

    if (pReportUrmaNodeInfoMessageHandler == nullptr || pNotifyMessageHandler == nullptr ||
        pQueryMessageHandler == nullptr || pQueryDevHandler == nullptr) {
        UBSE_LOG_ERROR << "Fail to create UbseComBaseMessageHandler";
        return UBSE_ERROR_NULLPTR;
    }

    auto ret = comModule->RegRpcService<UbseUrmaNotifyReqSimpo, UbseUrmaNotifyRspSimpo>(pNotifyMessageHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Reg UbseUrmaNotifyMessageHandler failed.";
        return ret;
    }

    ret = comModule->RegRpcService<UbseUrmaQueryReqSimpo, UbseUrmaQueryRspSimpo>(pQueryMessageHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Reg UbseUrmaQueryMessageHandler  failed.";
        return ret;
    }

    ret = comModule->RegRpcService<UrmaDevQueryReqSimpo, UrmaDevQueryRspSimpo>(pQueryDevHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Reg UbseUrmaQueryMessageHandler  failed.";
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
    auto ret = UbseUrmaControllerApi::Register();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Registration of UbseUrmaControllerApi failed," << FormatRetCode(ret);
        return ret;
    }
    if (RpcReg() != UBSE_OK) {
        return UBSE_ERROR;
    }

    std::string nodeJoinEventId = UBSE_EVENT_NODE_JOIN;
    ret = ubse::event::UbseSubEvent(nodeJoinEventId,
                                    ubse::urmaController::UrmaController::GetInstance().UbseNodeJoinHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to Follow the event=" << nodeJoinEventId;
    }

    std::string nodeTopoLinkChangeEventId = UBSE_EVENT_NODE_TOPO_LINK_CHANGE;
    ret = ubse::event::UbseSubEvent(nodeTopoLinkChangeEventId,
                                    ubse::urmaController::UrmaController::GetInstance().UbseTopoLinkChangeHandler);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Fail to Follow the event=" << nodeTopoLinkChangeEventId;
    }
    // 创建任务执行器
    auto taskExecutorModule = UbseContext::GetInstance().GetModule<ubse::task_executor::UbseTaskExecutorModule>();
    if (taskExecutorModule == nullptr) {
        return UBSE_ERROR_MODULE_LOAD_FAILED;
    }

    ret = taskExecutorModule->Create(URMA_CONTROLLER_TASK, 8, 1024);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Create urma controller task executor failed";
        return ret;
    }

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

} // namespace ubse::urmaController
