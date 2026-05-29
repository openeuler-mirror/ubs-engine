// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
#include "ubse_ras.h"
#include "ubse_error.h"
#include "ubse_ras_handler.h"

namespace ubse::ras {
uint32_t RegisterAlarmFaultHandler(AlarmHandler alarmHandler)
{
    return UbseRasHandler::GetInstance().RegisterAlarmFaultHandler(alarmHandler);
}

uint32_t RegisterAlarmFaultHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string name, AlarmFaultHandler handler,
                                   AlarmHandlerPriority priority)
{
    return RegisterAlarmFaultHandler({alarmFaultEvent, name, handler, priority});
}

uint32_t UnRegisterAlarmFaultHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string& name)
{
    return UbseRasHandler::GetInstance().UnRegisterAlarmFaultHandler(alarmFaultEvent, name);
}
} // namespace ubse::ras