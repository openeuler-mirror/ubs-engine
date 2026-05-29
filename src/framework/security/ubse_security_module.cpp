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

#include "ubse_security_module.h"

#include <iostream> // for char_traits, basic_ostream

#include "ubse_context.h"  // for DYNAMIC_CREATE
#include "ubse_env_util.h" // for GetEnv
#include "ubse_error.h"    // for UBSE_OK, UBSE_ERROR
#include "ubse_logger.h"
#include "ubse_security_manager.h" // for UbseSecurityManager

namespace ubse::security {
BASE_DYNAMIC_CREATE(UbseSecurityModule);
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
UbseResult SetMinCapabilities()
{
    // 必须先获取一次 Capabilities，不然无法成功设置 Capabilities
    if (UbseSecurityManager::GetCapabilities() != UBSE_OK) {
        return UBSE_ERROR;
    }
    if (UbseSecurityManager::SetInitialCapabilities() != UBSE_OK) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

UbseResult UbseSecurityModule::Initialize()
{
    return SetMinCapabilities();
}

void UbseSecurityModule::UnInitialize()
{
    // Do Nothing
}

UbseResult UbseSecurityModule::Start()
{
    return UBSE_OK;
}

void UbseSecurityModule::Stop()
{
    // Do Nothing
}

UbseResult UbseSecurityModule::ModifyEffectiveCapabilities(std::vector<__u32>& caps, bool isAdd)
{
    return UbseSecurityManager::ModifyEffectiveCapabilities(caps, isAdd);
}
} // namespace ubse::security
