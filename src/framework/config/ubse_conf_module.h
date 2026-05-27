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

#ifndef UBSE_CONF_MODULE_H
#define UBSE_CONF_MODULE_H
#include <atomic>
#include <cstdint> // for uint32_t, uint64_t, UINT32_MAX, UINT64_MAX
#include <functional>
#include <map> // for map
#include <optional>
#include <string>      // for string, basic_string, allocator, stof
#include <type_traits> // for is_same, remove_extent_t

#include "ubse_common_def.h" // for UbseResult
#include "ubse_conf.h"
#include "ubse_error.h"  // for UBSE_OK
#include "ubse_module.h" // for UbseModule

namespace ubse::config {
using ubse::common::def::UbseResult;
using ubse::module::UbseModule;

// 校验错误枚举类
enum class ErrorType : uint16_t {
    CONFIG_RETURN_FAILURE = 1,    // 调用下层接口失败, 返回非0
    CONFIG_UNSUPPORTED_TYPE = 2,  // 非法类型
    CONFIG_CONVERSION_FAILED = 3, // 读取结果无法转化为指定类型
    CONFIG_OUT_RANGE = 4,         // 数值超过类型最大范围
};

struct ConfigItem {
    std::string section{};
    std::string key{};
    std::string value{};
};
using register_config_func = std::function<std::optional<ConfigItem>()>;
class RegisterConfigHelper {
public:
    RegisterConfigHelper() = delete;
    explicit RegisterConfigHelper(register_config_func f);
};

class UbseConfModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    /**
    * @brief 读取配置
    * @param [in] section: 配置节
    * @param [in] configKey: 配置参数key
    * @param [out] configVal: 配置参数值
    * @return UbseResult, 成功返回0, 失败返回非0
    */
    template <typename T>
    UbseResult GetConf(const std::string& section, const std::string& configKey, T& configVal);

    /**
    * @brief 根据前缀读取配置
    * @param [in] sectionPrefix: 前缀
    * @param [out] configVals: 配置参数值表
    * @return UbseResult, 成功返回0, 失败返回非0
    */
    UbseResult GetAllConfigWithPrefix(const std::string& sectionPrefix,
                                      std::map<std::string, std::map<std::string, std::string>>& configVals);

    bool IsUbFeatureSupported(uint64_t featureMask) const;

    bool IsUrmaSupported() const;

    bool IsMemBorrowNcSupported() const;

    bool IsMemBorrowCcSupported() const;

    bool IsMemShareNcSupported() const;

    bool IsMemShareCcSupported() const;

    bool IsMemBorrowSupported() const;

    bool IsMemShareSupported() const;

    bool IsMemSupported() const;

private:
    void LoadUbFeature();

    UbseResult GetUIntConf(const std::string& section, const std::string& configKey, uint32_t& configVal);

    UbseResult GetULongConf(const std::string& section, const std::string& configKey, uint64_t& configVal);

    UbseResult GetFloatConf(const std::string& section, const std::string& configKey, float& configVal);

    UbseResult GetStringConf(const std::string& section, const std::string& configKey, std::string& configVal);

    UbseResult GetBoolConf(const std::string& section, const std::string& configKey, bool& configVal);

    std::string configDefaultDir_;
    std::string confCliDir_;
    std::atomic<uint64_t> ubFeature_{UB_FEATURE_ALL_MASK};
};

bool IsValidNumber(const std::string& str, bool allowFloating = false);

void PrintConfLog(ErrorType type, const std::string& section, const std::string& configKey, const UbseResult& result);

template <typename T>
UbseResult UbseConfModule::GetConf(const std::string& section, const std::string& configKey, T& configVal)
{
    if constexpr (std::is_same_v<T, uint32_t>) {
        return GetUIntConf(section, configKey, configVal);
    }

    if constexpr (std::is_same_v<T, uint64_t>) {
        return GetULongConf(section, configKey, configVal);
    }

    if constexpr (std::is_same_v<T, float>) {
        return GetFloatConf(section, configKey, configVal);
    }

    if constexpr (std::is_same_v<T, std::string>) {
        return GetStringConf(section, configKey, configVal);
    }

    if constexpr (std::is_same_v<T, bool>) {
        return GetBoolConf(section, configKey, configVal);
    }

    PrintConfLog(ErrorType::CONFIG_UNSUPPORTED_TYPE, section, configKey, UBSE_CONF_ERROR_KEY_OFFSETUNSUPPORTED_TYPE);
    return UBSE_CONF_ERROR_KEY_OFFSETUNSUPPORTED_TYPE;
}
} // namespace ubse::config
#endif // UBSE_CONF_MODULE_H
