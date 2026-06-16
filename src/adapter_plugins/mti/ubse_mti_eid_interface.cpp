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

#include "ubse_mti_eid_interface.h"

#include <array>
#include <bitset>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_logger_module.h"
#include "ubse_str_util.h"
#include "securec.h"

namespace ubse::utils {
using namespace common::def;
std::string GenerateUrmaDevEid(uint16_t superPodId, uint32_t nodeId, uint16_t fe0Id, uint16_t fe1Id)
{
    uint32_t id0 = BEID_PREFIX;
    uint32_t id1 = 0;
    uint32_t id2 = (static_cast<uint32_t>(fe0Id) << 16) | fe1Id;
    uint32_t id3 = nodeId;
    std::array<unsigned char, IPV6_BYTE_COUNT> bondingEid{};
    uint32_t copyCnt = 0;

    auto ret =
        memcpy_s(bondingEid.data() + IPV6_SEGMENT_LENGTH * copyCnt++, IPV6_SEGMENT_LENGTH, &id0, IPV6_SEGMENT_LENGTH);
    ret |=
        memcpy_s(bondingEid.data() + IPV6_SEGMENT_LENGTH * copyCnt++, IPV6_SEGMENT_LENGTH, &id1, IPV6_SEGMENT_LENGTH);
    ret |=
        memcpy_s(bondingEid.data() + IPV6_SEGMENT_LENGTH * copyCnt++, IPV6_SEGMENT_LENGTH, &id2, IPV6_SEGMENT_LENGTH);
    ret |=
        memcpy_s(bondingEid.data() + IPV6_SEGMENT_LENGTH * copyCnt++, IPV6_SEGMENT_LENGTH, &id3, IPV6_SEGMENT_LENGTH);
    if (ret != EOK) {
        return {};
    }
    std::array<char, IPV6_FULL_FORMAT_LENGTH + 1> buffer{};
    int res = snprintf_s(buffer.data(), buffer.size(), IPV6_FULL_FORMAT_LENGTH,
                         "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x", bondingEid[NO_0],
                         bondingEid[NO_1], bondingEid[NO_2], bondingEid[NO_3], bondingEid[NO_4], bondingEid[NO_5],
                         bondingEid[NO_6], bondingEid[NO_7], bondingEid[NO_8], bondingEid[NO_9], bondingEid[NO_10],
                         bondingEid[NO_11], bondingEid[NO_12], bondingEid[NO_13], bondingEid[NO_14], bondingEid[NO_15]);
    if (res < 0) {
        return {};
    }
    return buffer.data();
}

UbseResult ParseBaseEid(const std::string& baseEid, std::string& bitStr)
{
    bitStr.clear();
    std::stringstream ss(baseEid);
    std::vector<std::string> segments;
    std::string segment;
    while (std::getline(ss, segment, ':')) {
        segments.push_back(segment);
    }

    std::vector<uint16_t> values;
    for (const auto& seg : segments) {
        uint16_t value = 0;
        if (ConvertStrToUint16(seg, value, NO_16) != UBSE_OK) {
            return UBSE_ERROR;
        }
        values.push_back(value);
    }

    for (auto value : values) {
        std::bitset<NO_16> bits(value);
        bitStr += bits.to_string();
    }
    if (bitStr.size() != NO_128) {
        return UBSE_ERROR;
    }
    return UBSE_OK;
}

void ConstructEid(const std::string& bitStr, std::string& eid)
{
    // eid 4245:4944:0000:0000:0000:0000:0100:0000 格式，bits字符长128位
    eid.clear();
    for (size_t i = 0; i < bitStr.size(); i += NO_16) {
        if (i > 0) {
            eid += ":";
        }
        std::string bitChunk = bitStr.substr(i, NO_16);
        std::bitset<NO_16> bits(bitChunk);
        uint16_t value = bits.to_ulong();
        char hexStr[10];
        auto res = snprintf_s(hexStr, sizeof(hexStr), sizeof(hexStr) - 1, "%04X", value);
        if (res == -1) {
            return;
        }
        eid += hexStr;
    }
}

UbseResult OverwriteEid(uint32_t serverIdx, const std::string& baseEid, std::string& result)
{
    uint32_t n = 96; // 从第97位开始，4位part1, 101-104为不变，9位part2
    uint8_t serverIdxHigh = NO_4;
    uint8_t serverIdxLow = NO_9;
    // 取serverIdx低13bit, 高4bit为part1, 低9bit为part2
    uint16_t serverIdLow13 = serverIdx & ((1 << (serverIdxHigh + serverIdxLow)) - 1);
    uint16_t part2 = serverIdLow13 & ((1 << serverIdxLow) - 1);                          // bits [0:8]
    uint16_t part1 = ((serverIdLow13 >> serverIdxLow) & ((1 << serverIdxHigh) - 1)) + 1; // bits [9:12]

    std::string bitStr;

    if (auto ret = ParseBaseEid(baseEid, bitStr); ret != UBSE_OK) {
        return UBSE_ERROR;
    }
    // 检查替换区域是否超出bitStr范围
    if (n + serverIdxHigh + serverIdxLow > bitStr.size()) {
        return UBSE_ERROR;
    }
    // part1和part2转bit字符串
    std::bitset<NO_16> part1Bits(part1);
    std::bitset<NO_16> part2Bits(part2);
    std::string part1BitStr = part1Bits.to_string().substr(NO_16 - serverIdxHigh);
    std::string part2BitStr = part2Bits.to_string().substr(NO_16 - serverIdxLow);

    // 128位bitStr中：
    // (positions 104-112) 替换为part2 (9位)
    // (positions 100-103) 不变 (4位)
    // (positions 96-99)  替换为part1 (4位)
    // 其余部分不变
    std::string eidBitStr = bitStr.substr(0, n) + part1BitStr + bitStr.substr(n + serverIdxHigh, NO_4) + part2BitStr +
                            bitStr.substr(n + serverIdxHigh + NO_4 + serverIdxLow);

    ConstructEid(eidBitStr, result);
    return UBSE_OK;
}
} // namespace ubse::utils
