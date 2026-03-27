/*
* Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * UbseEngine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MONIOR_SERVICE_API_H
#define UBSE_MONIOR_SERVICE_API_H

#include "ubse_common_def.h"
#include "ubse_npu_monitor_def.h"

namespace  ubse::npu::vm_monitor {
ubse::common::def::UbseResult StartVMMonitor();

void ResetNpuOfBusInstance(const std::string &busInstance, VmEventType event);

ubse::common::def::UbseResult ResetNpu(const uint8_t &chipId);
}
#endif // UBSE_MONIOR_SERVICE_API_H