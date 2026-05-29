/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#include "ubse_mem_controller_master_online_handler.h"

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_pre_online.h"
#include "ubse_mem_scheduler.h"
#include "ubse_node_controller.h"

namespace ubse::mem::controller {
using namespace ubse::nodeController;
using namespace ubse::election;
const uint32_t HA_SEQUENCE_ID = 101; // 需要确保在节点建链后触发，节点建链优先级100
UBSE_DEFINE_THIS_MODULE("ubse");

uint32_t UbseMemControllerMasterOnlineHandler::MasterOnlineHandler(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId)
{
    ClearNodeMap();
    ClearOnLineMap();
    return UBSE_OK;
}

void UbseMemControllerMasterOnlineHandler::Initial()
{
    // 监听主上线事件，如果非主则清理非本节点的数据.
    UbseElectionHandlerBuilder Builder;
    Builder.SetHandler(MasterOnlineHandler);
    Builder.SetPriority(UbseElectionHandlerPriority::HIGH);
    Builder.SetSequenceId(HA_SEQUENCE_ID); // 需要确保在节点建链后触发，节点建链优先级100
    Builder.SetType(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION);
    Builder.SetName("UbseControllerMasterOnLine");
    UbseElectionChangeAttachHandler(Builder.Build());
}

void UbseMemControllerMasterOnlineHandler::Uninitial()
{
    UbseElectionHandlerBuilder Builder;
    Builder.SetHandler(MasterOnlineHandler);
    Builder.SetPriority(UbseElectionHandlerPriority::HIGH);
    Builder.SetSequenceId(HA_SEQUENCE_ID); // 需要确保在节点建链后触发，节点建链优先级100
    Builder.SetType(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION);
    Builder.SetName("UbseControllerMasterOnLine");
    UbseElectionChangeDeAttachHandler(Builder.Build());
}

} // namespace ubse::mem::controller