// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#include "ubse_ras_module.h"
#include <cstdint>
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_os_util.h"
#include "ubse_ras_handler.h"
#include "ubse_thread_pool_module.h"
#include "ubse_timer_controller.h"

namespace ubse::ras {
using namespace ubse::common;
using namespace ubse::log;
using namespace ubse::context;

CONDITION_DYNAMIC_CREATE(context::GetSceneType() == context::SceneType::COMMON, UbseRasModule);
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseRasModule::Initialize()
{
    return UBSE_OK;
}

void UbseRasModule::UnInitialize()
{
    return;
}

UbseResult UbseRasModule::Start()
{
    auto ret = UbseRasHandler::GetInstance().StartRasHandler();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Start ras handler failed, " << FormatRetCode(ret);
        return ret;
    }
    auto taskExecutor = UbseContext::GetInstance().GetModule<task_executor::UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        UBSE_LOG_ERROR << "Task executor module not found";
        return UBSE_ERROR;
    }
    uint16_t threadNum = 4;
    uint32_t queueSize = 128;
    ret = taskExecutor->Create(UBSE_RAS_FAULT_HANDLE_THREAD_POOL, threadNum, queueSize);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Create ras task failed, " << FormatRetCode(ret);
        return ret;
    }
    ret = UbseRasHandler::GetInstance().RegisterFaultHandleResultClearTimer();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Register fault handle result clean timer failed, " << FormatRetCode(ret);
    }
    return ret;
}

void UbseRasModule::Stop()
{
    ubse::timer::UbseTimerHandlerUnregister(UBSE_RAS_FAULT_HANDLE_RESULT_CLEAN_TIMER);
    auto taskExecutor = UbseContext::GetInstance().GetModule<task_executor::UbseTaskExecutorModule>();
    if (taskExecutor == nullptr) {
        UBSE_LOG_ERROR << "Task executor module not found";
        return;
    }
    taskExecutor->Remove(UBSE_RAS_FAULT_HANDLE_THREAD_POOL);
    return;
}
UbseRasModule::~UbseRasModule() {}
} // namespace ubse::ras