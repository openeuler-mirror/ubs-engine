/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_plugin_module.h"

namespace ubse::plugin {
using namespace ubse::context;
using namespace ubse::common::def;
UBSE_DEFINE_THIS_MODULE("ubse");

bool GetPluginInitRes(const std::string& pluginName)
{
    auto module = UbseContext::GetInstance().GetModule<UbsePluginModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "plugin module not init";
        return false;
    }
    return module->GetPluginLoaded(pluginName);
}
bool GetPluginReadyStatus(const std::string& pluginName)
{
    auto module = UbseContext::GetInstance().GetModule<UbsePluginModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "plugin module not init";
        return false;
    }
    return module->GetPluginReadyStatus(pluginName);
}
uint32_t NotifyPluginReadyStatus(const std::string& pluginName, bool status)
{
    auto module = UbseContext::GetInstance().GetModule<UbsePluginModule>();
    if (module == nullptr) {
        UBSE_LOG_ERROR << "plugin module not init";
        return UBSE_ERROR_NULLPTR;
    }
    module->NotifyPluginReadyStatus(pluginName, status);
    return UBSE_OK;
}
} // namespace ubse::plugin