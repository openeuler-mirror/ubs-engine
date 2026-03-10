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

#ifndef UBSE_NPU_CONTROLLER_DISPATCHER_H
#define UBSE_NPU_CONTROLLER_DISPATCHER_H
#include "ubse_common_def.h"
#include "ubse_ipc_server.h"

namespace ubse::npu::controller {
common::def::UbseResult RegisterSdkDispatcher();
} // namespace ubse::npu::controller
#endif // UBSE_NPU_CONTROLLER_DISPATCHER_H
