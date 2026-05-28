/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#ifndef UBSE_MANAGER_UBSE_MEM_CONTROLLER_MASTER_ONLINE_HANDLER_H
#define UBSE_MANAGER_UBSE_MEM_CONTROLLER_MASTER_ONLINE_HANDLER_H

#include <memory>

#include "ubse_node.h"

namespace ubse::mem::controller {
using namespace ubse::election;

class UbseMemControllerMasterOnlineHandler {
public:
    static void Initial();

    static void Uninitial();

private:
    static uint32_t MasterOnlineHandler(UbseElectionEventType &type, UBSE_ID_TYPE &nodeId);
    static uint32_t GlobalMasterOnlineEventHandler(std::string &eventId, std::string &eventMessage);
    static uint32_t HandleGlobalMasterOnline(const std::string &nodeId);
};

} // namespace ubse::mem::controller
#endif // UBSE_MANAGER_UBSE_MEM_CONTROLLER_MASTER_ONLINE_HANDLER_H
