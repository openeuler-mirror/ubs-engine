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
#include "ubse_mem_scheduler_sub_health_mode.h"

#include "ubse_conf_module.h"
#include "ubse_context.h"
#include "ubse_logger.h"
#include "ubse_smbios.h"

namespace ubse::mem::scheduler {
UBSE_DEFINE_THIS_MODULE("ubse_mem_scheduler");

namespace {

constexpr char CONF_MEM_SECTION[] = "ubse.memory";
constexpr char CONF_SUB_HEALTH_ENABLE[] = "subHealthPenaltyEnabled";
constexpr char CONF_SUB_HEALTH_STRATEGY[] = "subHealth.strategy";

const char* ModeName(SubHealthMode mode)
{
    switch (mode) {
        case SubHealthMode::DISABLED:
            return "DISABLED";
        case SubHealthMode::EXCLUDE:
            return "EXCLUDE";
        case SubHealthMode::WEIGHT:
            return "WEIGHT";
        default:
            return "UNKNOWN";
    }
}

} // namespace

SubHealthMode ResolveSubHealthModeFromConfig()
{
    auto confModule = context::UbseContext::GetInstance().GetModule<config::UbseConfModule>();
    if (confModule == nullptr) {
        UBSE_LOG_WARN << "confModule is null, subHealth disabled";
        return SubHealthMode::DISABLED;
    }
    bool enable = false;
    auto ret = confModule->GetConf<bool>(CONF_MEM_SECTION, CONF_SUB_HEALTH_ENABLE, enable);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get config ubse.memory." << CONF_SUB_HEALTH_ENABLE
                      << ", subHealth disabled (default)";
        return SubHealthMode::DISABLED;
    }
    if (!enable) {
        UBSE_LOG_INFO << "Config ubse.memory.subHealthPenaltyEnabled=false, subHealth disabled";
        return SubHealthMode::DISABLED;
    }
    // CLOS 组网由 LocalPortDownFilter + RegionFilter 负责链路质量，亚健康不覆盖。
    if (::ubse::adapter_plugins::smbios::UbseSmbios::GetInstance().IsClosType()) {
        UBSE_LOG_WARN << "Current topo is CLOS, subHealth disabled even though config enable=true";
        return SubHealthMode::DISABLED;
    }
    std::string strategy;
    ret = confModule->GetConf<std::string>(CONF_MEM_SECTION, CONF_SUB_HEALTH_STRATEGY, strategy);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Failed to get config ubse.memory." << CONF_SUB_HEALTH_STRATEGY
                      << ", use default strategy=weight";
        strategy = "weight";
    }
    auto mode = ResolveSubHealthMode(enable, strategy);
    UBSE_LOG_INFO << "Config ubse.memory.subHealthPenaltyEnabled=true, strategy=" << strategy
                  << ", resolved mode=" << ModeName(mode);
    return mode;
}

} // namespace ubse::mem::scheduler
