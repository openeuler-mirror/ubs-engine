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
constexpr uint64_t UB_MEM_BORROW_NC_MASK = 1ULL << 0;
constexpr uint64_t UB_MEM_BORROW_CC_MASK = 1ULL << 1;
constexpr uint64_t UB_MEM_SHARE_NC_MASK = 1ULL << 2;
constexpr uint64_t UB_MEM_SHARE_CC_MASK = 1ULL << 3;
constexpr uint64_t UB_URMA_RTP_ROI_MASK = 1ULL << 16;
constexpr uint64_t UB_URMA_RTP_ROT_MASK = 1ULL << 17;
constexpr uint64_t UB_URMA_RTP_ROL_MASK = 1ULL << 18;
constexpr uint64_t UB_URMA_CTP_ROI_MASK = 1ULL << 19;
constexpr uint64_t UB_URMA_CTP_ROT_MASK = 1ULL << 20;
constexpr uint64_t UB_URMA_CTP_ROL_MASK = 1ULL << 21;
constexpr uint64_t UB_URMA_CTP_UNO_MASK = 1ULL << 22;
constexpr uint64_t UB_URMA_UTP_UNO_MASK = 1ULL << 23;

constexpr uint64_t UB_URMA_ALL_MASK = UB_URMA_RTP_ROI_MASK | UB_URMA_RTP_ROT_MASK | UB_URMA_RTP_ROL_MASK |
                                      UB_URMA_CTP_ROI_MASK | UB_URMA_CTP_ROT_MASK | UB_URMA_CTP_ROL_MASK |
                                      UB_URMA_CTP_UNO_MASK | UB_URMA_UTP_UNO_MASK;
constexpr uint64_t UB_MEM_ALL_MASK = UB_MEM_BORROW_NC_MASK | UB_MEM_BORROW_CC_MASK | UB_MEM_SHARE_NC_MASK |
                                     UB_MEM_SHARE_CC_MASK;
constexpr uint64_t UB_FEATURE_ALL_MASK = UB_MEM_ALL_MASK | UB_URMA_ALL_MASK;

/**
 * @brief 获取无符号短整型类型配置
 * @param section [in] 配置节
 * @param configKey [in] 配置参数key
 * @param configVal [out] 配置参数的值
 * @return #UBSE_OK 0 成功
 * @return #非UBSE_OK 失败
 */
uint32_t UbseGetUInt(const std::string& section, const std::string& configKey, uint32_t& configVal);

/**
 * @brief 获取Float类型配置
 * @param section [in] 配置节
 * @param configKey [in] 配置参数key
 * @param configVal [out] 配置参数的值
 * @return #UBSE_OK 0 成功
 * @return #非UBSE_OK 失败
 */
uint32_t UbseGetFloat(const std::string& section, const std::string& configKey, float& configVal);

/**
 * @brief 获取字符串类型配置
 * @param section [in] 配置节
 * @param configKey [in] 配置参数key
 * @param configVal [out] 配置参数的值
 * @return #UBSE_OK 0 成功
 * @return #非UBSE_OK 失败
 */
uint32_t UbseGetStr(const std::string& section, const std::string& configKey, std::string& configVal);

/**
 * @brief 获取布尔类型配置
 * @param section [in] 配置节
 * @param configKey [in] 配置参数key
 * @param configVal [out] 配置参数的值
 * @return #UBSE_OK 0 成功
 * @return #非UBSE_OK 失败
 */
uint32_t UbseGetBool(const std::string& section, const std::string& configKey, bool& configVal);

/**
 * @brief 获取无符号长整数类型配置
 * @param section [in] 配置节
 * @param configKey [in] 配置参数key
 * @param configVal [out] 配置参数的值
 * @return #UBSE_OK 0 成功
 * @return #非UBSE_OK 失败
 */
uint32_t UbseGetULong(const std::string& section, const std::string& configKey, uint64_t& configVal);

bool UbseIsUbFeatureSupported(uint64_t featureMask);

bool UbseIsUrmaSupported();

bool UbseIsMemBorrowNcSupported();

bool UbseIsMemBorrowCcSupported();

bool UbseIsMemShareNcSupported();

bool UbseIsMemShareCcSupported();

bool UbseIsMemBorrowSupported();

bool UbseIsMemShareSupported();

bool UbseIsMemSupported();

uint32_t UbseGetUBEnable(bool& ubEnable);

} // namespace ubse::config

#endif // UBSE_CONF_H
