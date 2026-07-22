/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#include "ubse_error.h"
#include "ubse_security_manager.h"

namespace ubse::security {

UbseResult UbseSecurityManager::GetCapabilities()
{
    return UBSE_OK;
}

UbseResult UbseSecurityManager::SetInitialCapabilities()
{
    return UBSE_OK;
}

UbseResult UbseSecurityManager::ModifyEffectiveCapabilities(const std::vector<__u32>&, bool)
{
    return UBSE_OK;
}

} // namespace ubse::security
