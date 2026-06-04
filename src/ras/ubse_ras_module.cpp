// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#include "ubse_ras_module.h"
#include "ubse_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_os_util.h"
#include "ubse_ras_handler.h"

namespace ubse::ras {
using namespace ubse::common;
using namespace ubse::log;

OPTIONAL_MODULE_IMPL(UbseRasModule);
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
    }
    return ret;
}

void UbseRasModule::Stop()
{
    return;
}
UbseRasModule::~UbseRasModule() {}
} // namespace ubse::ras