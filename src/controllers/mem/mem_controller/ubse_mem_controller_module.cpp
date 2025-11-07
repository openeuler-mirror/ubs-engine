/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#include "ubse_mem_controller_module.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_controller_api_agent.h"
#include "ubse_mem_rpc.h"
#include "rpc/UbseMemDebInfoQueryHandler.h"
#include "ubse_conf.h"
#include "ubse_context.h"

namespace ubse::mem::controller {
using namespace ubse::config;
using namespace ubse::mem::controller::agent;
using namespace ubse::mem::controller;
using namespace ubse::mem_controller::rpc;
using namespace ubse::mem_controller;
DYNAMIC_CREATE(UbseMemControllerModule);
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)

const uint32_t RES_MAX_TIMEOUT_SECONDS = 3600;
#ifdef UB_ENVIRONMENT
const uint32_t RES_WAIT_TIMEOUT(3600); // 秒
#else
const uint32_t RES_WAIT_TIMEOUT(60); // 秒
#endif

UbseResult UbseMemControllerModule::Initialize()
{
    UbseResult ret = RegisterNodeCtlNotify();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to reg node ctl notify.";
        return ret;
    }
    return UBSE_OK;
}

void UbseMemControllerModule::UnInitialize() {}

UbseResult UbseMemControllerModule::Start()
{
    ubse::mem::controller::Init();
    if (auto ret = MemScheduleHandler::RegHandler(); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to reg mem schedule handler.";
        return ret;
    }
    UbseResult ret = UbseMemDebInfoQueryHandler::RegUbseMemDebInfoQueryHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to reg UbseMemDebInfoQueryHandler.";
        return ret;
    }
    return ubse::mem::controller::agent::Init();
}

void UbseMemControllerModule::Stop() {}
} // namespace ubse::mem::controller
