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

#ifndef UBSE_CONF_H
#define UBSE_CONF_H
#include <cstdint>
#include <string>

namespace ubse::config {
/**
 * @brief 获取无符号短整型类型配置
 * @param section [in] 配置节
 * @param configKey [in] 配置参数key
 * @param configVal [out] 配置参数的值
 * @return #UBSE_OK 0 成功
 * @return #非UBSE_OK 失败
 */
uint32_t UbseGetUInt(const std::string &section, const std::string &configKey, uint32_t &configVal);

/**
 * @brief 获取Float类型配置
 * @param section [in] 配置节
 * @param configKey [in] 配置参数key
 * @param configVal [out] 配置参数的值
 * @return #UBSE_OK 0 成功
 * @return #非UBSE_OK 失败
 */
uint32_t UbseGetFloat(const std::string &section, const std::string &configKey, float &configVal);

/**
 * @brief 获取字符串类型配置
 * @param section [in] 配置节
 * @param configKey [in] 配置参数key
 * @param configVal [out] 配置参数的值
 * @return #UBSE_OK 0 成功
 * @return #非UBSE_OK 失败
 */
uint32_t UbseGetStr(const std::string &section, const std::string &configKey, std::string &configVal);

/**
 * @brief 获取布尔类型配置
 * @param section [in] 配置节
 * @param configKey [in] 配置参数key
 * @param configVal [out] 配置参数的值
 * @return #UBSE_OK 0 成功
 * @return #非UBSE_OK 失败
 */
uint32_t UbseGetBool(const std::string &section, const std::string &configKey, bool &configVal);

/**
 * @brief 获取无符号长整数类型配置
 * @param section [in] 配置节
 * @param configKey [in] 配置参数key
 * @param configVal [out] 配置参数的值
 * @return #UBSE_OK 0 成功
 * @return #非UBSE_OK 失败
 */
uint32_t UbseGetULong(const std::string &section, const std::string &configKey, uint64_t &configVal);

uint32_t UbseGetUBEnable(bool& ubEnable);
}

#endif // UBSE_CONF_H
