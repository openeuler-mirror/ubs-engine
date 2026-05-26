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
#include "securec.h"
#include "ubse_common_def.h"
#include "ubse_logger_module.h"
#include "ubse_str_util.h"

namespace ubse::utils {
using namespace common::def;
std::string GenerateUrmaDevEid(uint16_t superPodId, uint32_t nodeId, uint16_t fe0Id, uint16_t fe1Id)
{
    uint32_t id0 = BEID_PREFIX + superPodId;
    uint32_t id1 = nodeId;
    uint32_t id2 = (static_cast<uint32_t>(fe0Id) << 16) | fe1Id;
    uint32_t id3 = 0;
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

UbseResult ParseBaseEid(const std::string &baseEid, std::string &bitStr)
{
    bitStr.clear();
    std::stringstream ss(baseEid);
    std::vector<std::string> segments;
    std::string segment;
    while (std::getline(ss, segment, ':')) {
        segments.push_back(segment);
    }

    std::vector<uint16_t> values;
    for (const auto &seg : segments) {
        try {
            values.push_back(static_cast<uint16_t>(std::stoul(seg, nullptr, NO_16)));
        } catch (const std::invalid_argument &e) {
            return UBSE_ERROR;
        }
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

void ConstructEid(const std::string &bitStr, std::string &eid)
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


UbseResult OverwriteEid(uint32_t serverIdx, const std::string &baseEid, std::string &result)
{
    uint32_t podId = serverIdx / NO_8;
    uint32_t serverId = serverIdx % NO_8;
    return OverwriteEid(podId, serverId, baseEid, result);

    /* 当前eid算法逻辑保持原有，待LCNE更新后替换 */
}

UbseResult OverwriteEid(uint32_t podId, uint32_t serverId, const std::string &baseEid, std::string &result)
{
    uint32_t n = 116; // 从第117位开始，3位podId，3位serverId
    uint8_t podBitSize = NO_3;
    uint8_t serverBitSize = NO_3;

    std::string bitStr;
    auto ret = ParseBaseEid(baseEid, bitStr);
    if (ret != UBSE_OK) {
        return ret;
    }

    if (n + podBitSize + serverBitSize > bitStr.size()) {
        return UBSE_ERROR;
    }

    std::bitset<NO_32> podIdBits(podId);
    std::bitset<NO_32> serverIdBits(serverId);

    std::string podIdBitStr = podIdBits.to_string().substr(NO_32 - podBitSize);
    std::string serverIdBitStr = serverIdBits.to_string().substr(NO_32 - serverBitSize);

    std::string eidBitStr = bitStr.substr(0, n) + podIdBitStr + serverIdBitStr +
                            bitStr.substr(n + podBitSize + serverBitSize);
    ConstructEid(eidBitStr, result);
    return UBSE_OK;
}

} // namespace ubse::utils
