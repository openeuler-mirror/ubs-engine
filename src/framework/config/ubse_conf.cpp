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

#include "ubse_conf.h"

#include <memory> // for operator==, shared_ptr, __shared_ptr_...

#include "ubse_common_def.h"  // for UbseResult
#include "ubse_conf_module.h" // for UbseConfModule
#include "ubse_context.h"     // for UbseContext
#include "ubse_error.h"
#include "ubse_logger.h" // for UBSE_DEFINE_THIS_MODULE

namespace ubse::config {
using namespace ubse::log;
UBSE_DEFINE_THIS_MODULE("ubse");
template <typename T>
UbseResult GetConf(const std::string& section, const std::string& configKey, T& configVal)
{
    // 调取UbseConfModule类的单例对象
    auto& ctxRef = context::UbseContext::GetInstance();
    auto cfgPtr = ctxRef.GetModule<UbseConfModule>();
    if (cfgPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to get configuration module instance, "
                       << FormatRetCode(UBSE_CONF_ERROR_KEY_OFFSETCONFIG_MODULE_LOAD_FAIL);
        return UBSE_CONF_ERROR_KEY_OFFSETCONFIG_MODULE_LOAD_FAIL;
    }
    return cfgPtr->GetConf(section, configKey, configVal);
}

uint32_t UbseGetUInt(const std::string& section, const std::string& configKey, uint32_t& configVal)
{
    return GetConf(section, configKey, configVal);
}

uint32_t UbseGetFloat(const std::string& section, const std::string& configKey, float& configVal)
{
    return GetConf(section, configKey, configVal);
}

uint32_t UbseGetStr(const std::string& section, const std::string& configKey, std::string& configVal)
{
    return GetConf(section, configKey, configVal);
}

uint32_t UbseGetBool(const std::string& section, const std::string& configKey, bool& configVal)
{
    return GetConf(section, configKey, configVal);
}

uint32_t UbseGetULong(const std::string& section, const std::string& configKey, uint64_t& configVal)
{
    return GetConf(section, configKey, configVal);
}

std::shared_ptr<UbseConfModule> GetConfModule()
{
    auto& ctxRef = context::UbseContext::GetInstance();
    auto cfgPtr = ctxRef.GetModule<UbseConfModule>();
    if (cfgPtr == nullptr) {
        UBSE_LOG_ERROR << "Failed to get configuration module instance, "
                       << FormatRetCode(UBSE_CONF_ERROR_KEY_OFFSETCONFIG_MODULE_LOAD_FAIL);
    }
    return cfgPtr;
}

bool UbseIsUbFeatureSupported(uint64_t featureMask)
{
    auto cfgPtr = GetConfModule();
    if (cfgPtr == nullptr) {
        return false;
    }
    return cfgPtr->IsUbFeatureSupported(featureMask);
}

bool UbseIsUrmaSupported()
{
    auto cfgPtr = GetConfModule();
    if (cfgPtr == nullptr) {
        return false;
    }
    return cfgPtr->IsUrmaSupported();
}

bool UbseIsMemBorrowNcSupported()
{
    auto cfgPtr = GetConfModule();
    if (cfgPtr == nullptr) {
        return false;
    }
    return cfgPtr->IsMemBorrowNcSupported();
}

bool UbseIsMemBorrowCcSupported()
{
    auto cfgPtr = GetConfModule();
    if (cfgPtr == nullptr) {
        return false;
    }
    return cfgPtr->IsMemBorrowCcSupported();
}

bool UbseIsMemShareNcSupported()
{
    auto cfgPtr = GetConfModule();
    if (cfgPtr == nullptr) {
        return false;
    }
    return cfgPtr->IsMemShareNcSupported();
}

bool UbseIsMemShareCcSupported()
{
    auto cfgPtr = GetConfModule();
    if (cfgPtr == nullptr) {
        return false;
    }
    return cfgPtr->IsMemShareCcSupported();
}

bool UbseIsMemBorrowSupported()
{
    auto cfgPtr = GetConfModule();
    if (cfgPtr == nullptr) {
        return false;
    }
    return cfgPtr->IsMemBorrowSupported();
}

bool UbseIsMemShareSupported()
{
    auto cfgPtr = GetConfModule();
    if (cfgPtr == nullptr) {
        return false;
    }
    return cfgPtr->IsMemShareSupported();
}

bool UbseIsMemSupported()
{
    auto cfgPtr = GetConfModule();
    if (cfgPtr == nullptr) {
        return false;
    }
    return cfgPtr->IsMemSupported();
}
} // namespace ubse::config
