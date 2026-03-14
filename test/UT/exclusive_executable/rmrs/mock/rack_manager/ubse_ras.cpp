/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_ras.h"

namespace ubse::ras {

/**
 * 注册故障处理函数
 * @param alarmFaultEvent
 * @param name
 * @param handler
 * @param priority
 * @return
 */
uint32_t RegisterAlarmFaultHandler(ubse::ras::ALARM_FAULT_TYPE, std::string, ubse::ras::AlarmFaultHandler,
    ubse::ras::AlarmHandlerPriority)
{
    return 0;
}
}