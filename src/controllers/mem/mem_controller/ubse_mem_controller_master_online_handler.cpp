/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#include "ubse_mem_controller_master_online_handler.h"

#include "ubse_error.h"
#include "ubse_init_ledger_state.h"
#include "ubse_logger.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_pre_online.h"
#include "ubse_mem_migrate_session.h"

#include "ubse_node_controller.h"

namespace ubse::mem::controller {
using namespace ubse::nodeController;
using namespace ubse::election;
// 节点控制器在序号101的回调中投递对账任务，清理旧账本和完成记录必须先执行。
const uint32_t HA_SEQUENCE_ID = 100;
UBSE_DEFINE_THIS_MODULE("ubse");

uint32_t UbseMemControllerMasterOnlineHandler::MasterOnlineHandler(UbseElectionEventType& type, UBSE_ID_TYPE& nodeId)
{
    UbseInitLedgerState::GetInstance().ResetMigrationRecovery(nodeId ==
                                                              UbseNodeController::GetInstance().GetCurrentNodeId());
    UbseMemShmMigrateSessionManager::GetInstance().ClearSessionsIfNotLeader();
    auto ret = ClearNodeMap();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Clear node map failed, nodeId=" << nodeId << ", ret=" << ubse::log::FormatRetCode(ret);
    }
    ClearOnLineMap();
    return ret;
}

void UbseMemControllerMasterOnlineHandler::Initial()
{
    // 监听主上线事件，如果非主则清理非本节点的数据.
    UbseElectionHandlerBuilder Builder;
    Builder.SetHandler(MasterOnlineHandler);
    Builder.SetPriority(UbseElectionHandlerPriority::HIGH);
    Builder.SetSequenceId(HA_SEQUENCE_ID);
    Builder.SetType(UbseElectionEventType::MASTER_ONLINE_NOTIFICATION);
    Builder.SetName("UbseControllerMasterOnLine");
    UbseElectionChangeAttachHandler(Builder.Build());
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
}

} // namespace ubse::mem::controller
