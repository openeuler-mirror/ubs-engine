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
uint32_t ChangeOverrideCapability(bool isAdd)
{
    return 0;
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
} // namespace ubse::security
