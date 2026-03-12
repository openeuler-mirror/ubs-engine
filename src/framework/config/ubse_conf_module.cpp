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

#include "ubse_conf_module.h"

#include <cstddef>                // for size_t
#include <regex>
#include <vector>

#include "ubse_common_def.h"      // for UbseResult
#include "ubse_security_module.h"
#include "ubse_conf_manager.h"    // for UbseConfigManager
#include "ubse_context.h"         // for UbseContext, ProcessMode
#include "ubse_logger.h"          // for UbseLoggerEntry, UBSE_D...
#include "ubse_str_util.h"

namespace ubse::config {
using namespace ubse::context;
using namespace ubse::log;
using namespace ubse::common::def;
using namespace ubse::utils;

BASE_DYNAMIC_CREATE(UbseConfModule, ubse::security::UbseSecurityModule);
UBSE_DEFINE_THIS_MODULE("ubse");

const std::string CONFIG_DEFAULT_DIR = "/etc/ubse";

std::tuple<std::string, std::string, std::string> TrimConf(const std::string& section, const std::string& configKey,
                                                           const std::string& configVal = "");

class RegisterFunc {
public:
    static RegisterFunc& GetInstance()
    {
        static RegisterFunc _ins;
        return _ins;
    }
public:
    void push(register_config_func &f)
    {
        funcs_.push_back(std::move(f));
    }

    const auto& GetFuncs()
    {
        return funcs_;
    }
private:
    RegisterFunc() = default;
    std::vector<register_config_func> funcs_;
};

RegisterConfigHelper::RegisterConfigHelper(register_config_func f)
{
    RegisterFunc::GetInstance().push(f);
}

UbseResult UbseConfModule::Initialize()
{
    auto& confMgrRef = UbseConfigManager::GetInstance();
    auto& ctxRef = UbseContext::GetInstance();
    UbseResult ret = confMgrRef.Init(CONFIG_DEFAULT_DIR);
    if (ret != UBSE_OK) {
        return ret;
    }

    ctxRef.GetArgStr("f", confCliDir_);
    ret = confMgrRef.Init(confCliDir_);  // 允许命令行不传配置目录文件
    if (ret != UBSE_OK && ret != UBSE_CONF_ERROR_KEY_OFFSETDIR_OPEN_ERROR) {
        return ret;
    }
    for (auto &f : RegisterFunc::GetInstance().GetFuncs()) {
        auto result = f();
        if (result.has_value()) {
            confMgrRef.AddConfig(result->section, result->key, result->value);
        }
    }
    return UBSE_OK;
}

void UbseConfModule::UnInitialize()
{
}

UbseResult UbseConfModule::Start()
{
    return UbseConfigManager::GetInstance().Start();
}

void UbseConfModule::Stop()
{
    UbseConfigManager::GetInstance().Stop();
}

UbseResult UbseConfModule::GetAllConfigWithPrefix(const std::string& sectionPrefix,
                                                  std::map<std::string, std::map<std::string, std::string>>& configVals)
{
    auto trimPrefix = Trim(sectionPrefix);
    return UbseConfigManager::GetInstance().GetAllConf(trimPrefix, configVals);
}

template <typename T>
UbseResult GetNumConf(const std::string& section, const std::string& configKey, T& configVal)
{
    auto [trimSection, trimConfigKey, configValString] = TrimConf(section, configKey, "");
    UbseResult ret = UbseConfigManager::GetInstance().GetConf(trimSection, trimConfigKey, configValString);
    if (ret != UBSE_OK) {
        PrintConfLog(ErrorType::CONFIG_RETURN_FAILURE, trimSection, trimConfigKey, ret);
        return ret;
    }

    if (!IsValidNumber(configValString, !std::is_unsigned_v<T>)) {
        PrintConfLog(ErrorType::CONFIG_CONVERSION_FAILED, trimSection, trimConfigKey,
                     UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR);
        return UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR;
    }

    try {
        if constexpr (std::is_same<T, float>::value) {
            configVal = std::stof(configValString);
        } else if constexpr (std::is_same<T, uint32_t>::value) {
            configVal = static_cast<uint32_t>(std::stoul(configValString));
            if (std::to_string(configVal) != configValString) {
                throw std::invalid_argument("Invalid argument: Configuring non-integer types.");
            }
        } else if constexpr (std::is_same<T, uint64_t>::value) {
            configVal = static_cast<uint64_t>(std::stoull(configValString));
            if (std::to_string(configVal) != configValString) {
                throw std::invalid_argument("Invalid argument: Configuring non-integer types.");
            }
        }
    } catch (const std::invalid_argument&) {
        PrintConfLog(ErrorType::CONFIG_CONVERSION_FAILED, trimSection, trimConfigKey,
                     UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR);
        return UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR;
    } catch (const std::out_of_range&) {
        PrintConfLog(ErrorType::CONFIG_OUT_RANGE, trimSection, trimConfigKey,
                     UBSE_CONF_ERROR_KEY_OFFSETVALUE_OUT_OF_RANGE);
        return UBSE_CONF_ERROR_KEY_OFFSETVALUE_OUT_OF_RANGE;
    }

    return UBSE_OK;
}

UbseResult UbseConfModule::GetUIntConf(const std::string& section, const std::string& configKey, uint32_t& configVal)
{
    return GetNumConf(section, configKey, configVal);
}

UbseResult UbseConfModule::GetULongConf(const std::string& section, const std::string& configKey, uint64_t& configVal)
{
    return GetNumConf(section, configKey, configVal);
}

UbseResult UbseConfModule::GetFloatConf(const std::string& section, const std::string& configKey, float& configVal)
{
    return GetNumConf(section, configKey, configVal);
}

UbseResult UbseConfModule::GetStringConf(const std::string& section, const std::string& configKey,
                                         std::string& configVal)
{
    auto [trimSection, trimConfigKey, configValString] = TrimConf(section, configKey, "");
    UbseResult ret = UbseConfigManager::GetInstance().GetConf(trimSection, trimConfigKey, configVal);
    if (ret != UBSE_OK) {
        PrintConfLog(ErrorType::CONFIG_RETURN_FAILURE, trimSection, trimConfigKey, ret);
        return ret;
    }

    return UBSE_OK;
}

UbseResult UbseConfModule::GetBoolConf(const std::string& section, const std::string& configKey, bool& configVal)
{
    auto [trimSection, trimConfigKey, configValString] = TrimConf(section, configKey, "");
    UbseResult ret = UbseConfigManager::GetInstance().GetConf(trimSection, trimConfigKey, configValString);
    if (ret != UBSE_OK) {
        PrintConfLog(ErrorType::CONFIG_RETURN_FAILURE, trimSection, trimConfigKey, ret);
        return ret;
    }

    if (configValString == "true") {
        configVal = true;
    } else if (configValString == "false") {
        configVal = false;
    } else {
        PrintConfLog(ErrorType::CONFIG_CONVERSION_FAILED, trimSection, trimConfigKey, ret);
        return UBSE_CONF_ERROR_KEY_OFFSETCONVERT_ERROR;
    }

    return UBSE_OK;
}

bool IsValidNumber(const std::string& str, bool allowFloating)
{
    try {
        if (allowFloating) {
            std::regex re(R"(^-?(0|[1-9]\d*)?(\.\d+)?$)");
            return std::regex_match(str, re);
        }

        std::regex re(R"(^[1-9]\d*$|^0$)");
        return std::regex_match(str, re);
    } catch (const std::regex_error& e) {
        return false;
    }
}

void PrintConfLog(ErrorType type, const std::string& section, const std::string& configKey, const UbseResult& result)
{
    std::string errorMessage = "Unable to read key=" + configKey + " in section=" + section + ", ";
    switch (type) {
        case ErrorType::CONFIG_RETURN_FAILURE:
            UBSE_LOG_WARN << errorMessage << FormatRetCode(result);
            break;
        case ErrorType::CONFIG_UNSUPPORTED_TYPE:
            UBSE_LOG_WARN << errorMessage << "the query type is not supported, " << FormatRetCode(result);
            break;
        case ErrorType::CONFIG_CONVERSION_FAILED:
            UBSE_LOG_WARN << errorMessage << "the query result can't be converted to specified type, "
                        << FormatRetCode(result);
            break;
        case ErrorType::CONFIG_OUT_RANGE:
            UBSE_LOG_WARN << errorMessage << "the query result exceeds the range, " << FormatRetCode(result);
            break;
        default:
            UBSE_LOG_ERROR << "Unknown error.";
            break;
    }
}

std::tuple<std::string, std::string, std::string> TrimConf(const std::string& section, const std::string& configKey,
                                                           const std::string& configVal)
{
    return {Trim(section), Trim(configKey), Trim(configVal)};
}


}  // namespace ubse::config