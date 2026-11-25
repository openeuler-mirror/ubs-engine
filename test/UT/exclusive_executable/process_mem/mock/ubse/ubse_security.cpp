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

#include "ubse_security_module.h"

namespace ubse::security {
// CapabilityGuard 桩实现：UT 环境不真正操作 capability，构造即视为提权成功
CapabilityGuard::CapabilityGuard(const std::vector<__u32>& caps) : caps_(caps)
{
    enabled_ = true;
}

CapabilityGuard::~CapabilityGuard()
{
    Disable();
}

void CapabilityGuard::Disable()
{
    enabled_ = false;
}

bool CapabilityGuard::IsEnabled() const
{
    return enabled_;
}

UbseResult UbseSecurityModule::Initialize()
{
    return 0;
}

void UbseSecurityModule::UnInitialize() {}

UbseResult UbseSecurityModule::Start()
{
    return 0;
}

void UbseSecurityModule::Stop() {}

UbseResult UbseSecurityModule::ModifyEffectiveCapabilities(std::vector<__u32>& caps, bool isAdd)
{
    return 0;
}

UbseResult UbseSecurityManager::GetCapabilities()
{
    return 0;
}

UbseResult UbseSecurityManager::SetInitialCapabilities()
{
    return 0;
}

UbseResult UbseSecurityManager::ModifyEffectiveCapabilities(const std::vector<__u32>& caps, bool isAdd)
{
    return 0;
}

CapabilityGuard::CapabilityGuard(const std::vector<__u32>& caps) : caps_(caps)
{
    enabled_ = (UbseSecurityModule::ModifyEffectiveCapabilities(caps_, true) == 0);
}

CapabilityGuard::~CapabilityGuard()
{
    Disable();
}

void CapabilityGuard::Disable()
{
    if (!enabled_) {
        return;
    }
    enabled_ = false;
    UbseSecurityModule::ModifyEffectiveCapabilities(caps_, false);
}

bool CapabilityGuard::IsEnabled() const
{
    return enabled_;
}
} // namespace ubse::security
