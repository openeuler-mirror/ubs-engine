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

#ifndef UBSE_MTI_EID_INTERNAL_H
#define UBSE_MTI_EID_INTERNAL_H

#include <cstdint>
#include <string>

namespace ubse::utils {
/**
 * @brief 解析 EID 字符串 为128位bit字符串
 * @param baseEid 基础 EID
 * @param bitStr 128位bit字符串
 * @return 操作结果
 */
uint32_t ParseBaseEid(const std::string& baseEid, std::string& bitStr);

/**
 * @brief 从128位bit字符串构造EID字符串
 * @param bitStr 128位bit字符串
 * @param eid 构造出的EID字符串
 */
void ConstructEid(const std::string& bitStr, std::string& eid);

} // namespace ubse::utils

#endif // UBSE_MTI_EID_INTERNAL_H