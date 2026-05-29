// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#ifndef UBSE_MANAGER_MXE_RAS_OOM_HANDLER_H
#define UBSE_MANAGER_MXE_RAS_OOM_HANDLER_H
#include "ubse_common_def.h"
#include "ubse_ras.h"

namespace ubse::ras {
using ubse::common::def::UbseResult;

UbseResult OomHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo);

UbseResult InitOomHandler();

} // namespace ubse::ras

#endif // UBSE_MANAGER_MXE_RAS_OOM_HANDLER_H
