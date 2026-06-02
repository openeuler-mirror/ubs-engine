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

#ifndef UBSE_MTI_UTIL_H
#define UBSE_MTI_UTIL_H
#include <array>
#include <cstdint>
#include <string>
namespace ubse::mti {
bool EidStrToArray(const std::string& eidStr, std::array<uint8_t, 16>& eid);

bool EidArrayToStr(const std::array<uint8_t, 16>& eid, std::string& eidStr);

bool GuidStrToArray(const std::string& guidStr, std::array<uint8_t, 16>& guid);

bool UpiStrToUint16(const std::string& upiStr, uint16_t& upi);

std::string UpiUint16ToStr(uint16_t upi);
} // namespace ubse::mti
#endif // UBSE_MTI_UTIL_H
