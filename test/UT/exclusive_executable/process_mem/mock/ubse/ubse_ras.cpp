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

#include "ubse_ras.h"

namespace ubse::ras {

uint32_t RegisterAlarmFaultHandler(AlarmHandler alarmHandler)
{
    return 0;
}

uint32_t RegisterAlarmFaultHandler(ALARM_FAULT_TYPE, std::string, AlarmFaultHandler, AlarmHandlerPriority)
{
    return 0;
}

uint32_t UnRegisterAlarmFaultHandler(ALARM_FAULT_TYPE, std::string& name)
{
    return 0;
}
} // namespace ubse::ras
