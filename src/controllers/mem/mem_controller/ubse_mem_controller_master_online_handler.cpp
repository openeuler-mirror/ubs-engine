/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#include "ubse_mem_controller_master_online_handler.h"

#include "ubse_error.h"
#include "ubse_event.h"
#include "ubse_logger.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_pre_online.h"
#include "ubse_mem_global_ledger_report.h"
#include "ubse_mem_scheduler.h"
#include "ubse_mem_util.h"
#include "ubse_node_controller.h"

namespace ubse::mem::controller {
using namespace ubse::nodeController;
using namespace ubse::mem::util;
using namespace ubse::event;
const uint32_t HA_SEQUENCE_ID = 101;
UBSE_DEFINE_THIS_MODULE("ubse");

uint32_t UbseMemControllerMasterOnlineHandler::MasterOnlineHandler(UbseElectionEventType &type, UBSE_ID_TYPE &nodeId)
{
    ClearNodeMap();
    ClearOnLineMap();
    return UBSE_OK;
}

uint32_t UbseMemControllerMasterOnlineHandler::GlobalMasterOnlineEventHandler(std::string &eventId,
                                                                              std::string &eventMessage)
{
    return HandleGlobalMasterOnline(eventMessage);
}

uint32_t UbseMemControllerMasterOnlineHandler::HandleGlobalMasterOnline(const std::string &nodeId)
{
    ClearStoredGlobalNodeLedgerSummaries();
    UBSE_LOG_INFO << "[CLOS_STATE] cleared global ledger summaries on global master online";

    auto allNodes = UbseNodeController::GetInstance().GetAllNodes();
    auto resourceExecutor = GetExecutor("ubseMemController");
    if (resourceExecutor == nullptr) {
        UBSE_LOG_ERROR << "Failed to get ubseMemController executor for summary reporting";
        return UBSE_OK;
    }

    for (const auto &[nodeIdStr, nodeInfo] : allNodes) {
        if (nodeInfo.clusterState != UbseNodeClusterState::UBSE_NODE_WORKING) {
            continue;
        }
        resourceExecutor->Execute([nodeIdStr]() {
            auto ret = ReportExistingSummaryForWorkingNode(nodeIdStr);
            if (ret != UBSE_OK) {
                UBSE_LOG_WARN << "report existing summary for WORKING node failed on global master online, nodeId="
                              << nodeIdStr << ", " << ubse::log::FormatRetCode(ret);
            }
        });
    }
    return UBSE_OK;
}

void UbseMemControllerMasterOnlineHandler::Initial()
{
    UbseElectionHandlerBuilder Builder;
    Builder.SetHandler(MasterOnlineHandler);
    Builder.SetPriority(UbseElectionHandlerPriority::HIGH);
    Builder.SetSequenceId(HA_SEQUENCE_ID);
    Builder.SetType(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION);
    Builder.SetName("UbseControllerMasterOnLine");
    UbseElectionChangeAttachHandler(Builder.Build());

    std::string eventId = UBSE_EVENT_GLOBAL_MASTER_ONLINE;
    UbseSubEvent(eventId, GlobalMasterOnlineEventHandler, HIGH);
}

void UbseMemControllerMasterOnlineHandler::Uninitial()
{
    UbseElectionHandlerBuilder Builder;
    Builder.SetHandler(MasterOnlineHandler);
    Builder.SetPriority(UbseElectionHandlerPriority::HIGH);
    Builder.SetSequenceId(HA_SEQUENCE_ID);
    Builder.SetType(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION);
    Builder.SetName("UbseControllerMasterOnLine");
    UbseElectionChangeDeAttachHandler(Builder.Build());

    std::string eventId = UBSE_EVENT_GLOBAL_MASTER_ONLINE;
    UbseUnSubEvent(eventId, GlobalMasterOnlineEventHandler);
}

} // namespace ubse::mem::controller
