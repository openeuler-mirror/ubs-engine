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
#include <tuple>
#include <vector>

namespace ubse::utils {

using CnaBitRange = std::tuple<uint8_t, uint8_t, uint8_t>;

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

/**
 * @brief 设置EID中CNA字段的serverIdx位段规则
 * @details 位段基于CNA 24位字段从低比特位0开始计数，每个元素为{起始位, 结束位, 基础值}的三元组， 
 *          例如{7,15,0}表示低7~15位的闭区间，且基础值偏移为0，写入时该位段实际值为serverIdx对应比特位叠加基础值偏移。
 * @param ranges serverIdx位段集合
 */
void SetEidCnaRule(const std::vector<CnaBitRange>& ranges);

} // namespace ubse::utils

#endif // UBSE_MTI_EID_INTERNAL_H