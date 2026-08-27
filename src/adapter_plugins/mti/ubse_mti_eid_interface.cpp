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
#include "ubse_mti_eid_internal.h"

#include <array>
#include <bitset>
#include <tuple>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_logger_module.h"
#include "ubse_str_util.h"
#include "securec.h"

namespace ubse::utils {
using namespace common::def;
constexpr uint8_t CNA_BIT_OFFSET = 96;
constexpr uint8_t CNA_BIT_LEN = 24;

static std::vector<CnaBitRange> g_serverIdxBitRanges = {
    std::make_tuple(static_cast<uint8_t>(7), static_cast<uint8_t>(15), static_cast<uint8_t>(0)),
    std::make_tuple(static_cast<uint8_t>(20), static_cast<uint8_t>(23), static_cast<uint8_t>(1))};

void SetEidCnaRule(const std::vector<CnaBitRange>& ranges)
{
    g_serverIdxBitRanges = ranges;
}

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
        auto res = snprintf_s(hexStr, sizeof(hexStr), sizeof(hexStr) - 1, "%04x", value);
        if (res == -1) {
            return;
        }
        eid += hexStr;
    }
}

UbseResult OverwriteEid(uint32_t serverIdx, const std::string& baseEid, std::string& result)
{
    if (g_serverIdxBitRanges.empty()) {
        return UBSE_ERROR;
    }

    // 计算serverIdx总位宽并校验位段合法性（位段需落在CNA 24位字段内）
    uint32_t serverIdxLen = 0;
    for (const auto& range : g_serverIdxBitRanges) {
        uint8_t start = std::get<0>(range);
        uint8_t end = std::get<1>(range);
        if (start > end || end >= CNA_BIT_LEN) {
            return UBSE_ERROR;
        }
        serverIdxLen += static_cast<uint32_t>(end - start) + 1;
    }

    std::string bitStr;
    if (ParseBaseEid(baseEid, bitStr) != UBSE_OK) {
        return UBSE_ERROR;
    }

    // 从低比特位0开始，将serverIdx逐位写入各位段对应的CNA位，写入时叠加各位段基础值偏移
    uint32_t serverId = serverIdx & ((1u << serverIdxLen) - 1);
    uint32_t bitIdx = 0;
    for (const auto& range : g_serverIdxBitRanges) {
        uint8_t start = std::get<0>(range);
        uint8_t end = std::get<1>(range);
        uint8_t offset = std::get<2>(range);
        uint8_t fieldLen = end - start + 1;
        // 提取该位段承载的serverIdx比特位并叠加位段偏移，得到该位段的实际写入值
        uint32_t fieldVal = ((serverId >> bitIdx) & ((1u << fieldLen) - 1)) + offset;
        for (uint8_t i = 0; i < fieldLen; ++i) {
            // 位段低比特位0对应EID绝对位 CNA_BIT_OFFSET + CNA_BIT_LEN - 1
            size_t absPos = CNA_BIT_OFFSET + CNA_BIT_LEN - 1 - (start + i);
            bitStr[absPos] = ((fieldVal >> i) & 0x1) ? '1' : '0';
        }
        bitIdx += fieldLen;
    }

    ConstructEid(bitStr, result);
    return UBSE_OK;
}

uint32_t ParseCnaFromEid(const std::string& eid, std::string& cna)
{
    // 将EID解析为128位0/1字符串，从第97位起取24个bit位，其余位均为0，构造新的EID
    std::string bitStr;
    if (ParseBaseEid(eid, bitStr) != UBSE_OK) {
        return UBSE_ERROR;
    }
    if (CNA_BIT_OFFSET + CNA_BIT_LEN > bitStr.size()) {
        return UBSE_ERROR;
    }
    // 非CNA位全部置为0，CNA位保留原值
    std::string cnaBitStr(bitStr.size(), '0');
    for (size_t i = 0; i < CNA_BIT_LEN; ++i) {
        cnaBitStr[CNA_BIT_OFFSET + i] = bitStr[CNA_BIT_OFFSET + i];
    }
    ConstructEid(cnaBitStr, cna);
    return UBSE_OK;
}
} // namespace ubse::utils
