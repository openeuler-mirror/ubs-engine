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

#include "ubse_npu_controller_module.h"
#include <array>
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_npu_controller_dispatcher.h"
#include "ubse_npu_manager_api.h"
#include "vm_state_monitor/ubse_npu_monitor_service_api.h"

namespace ubse::npu::controller {
using namespace ubse::log;
using namespace ubse::context;

// npu 插件不依赖其它可选模块，运行时所需外部符号由 ubse 主程序统一导出解析
static constexpr auto G_UBSE_NPU_DEPS = std::array<UbseOptionModule, 0>{};
PLUGIN_MODULE_IMPL(UbseNpuControllerModule, G_UBSE_NPU_DEPS);
UBSE_DEFINE_THIS_MODULE("ubse");

UbseResult UbseNpuControllerModule::Initialize()
{
    return UBSE_OK;
}

void UbseNpuControllerModule::UnInitialize() {}

UbseResult UbseNpuControllerModule::Start()
{
    UBSE_LOG_INFO << "[NPU] Start to register sdk dispatcher";
    UbseResult ret = RegisterSdkDispatcher();
    ret |= vm_monitor::StartVMMonitor();
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Failed to start npu controller.";
        return UBSE_ERROR;
    }
    StartCollect();
    return UBSE_OK;
}

void UbseNpuControllerModule::Stop() {}
} // namespace ubse::npu::controller