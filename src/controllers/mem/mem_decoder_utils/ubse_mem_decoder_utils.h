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
#ifndef UBSE_MEM_DECODER_UTILS_H
#define UBSE_MEM_DECODER_UTILS_H

#include <unordered_map>
#include <unordered_set>
#include "adapter_plugins/mti/ubse_mti_mami_def.h"
#include "ubse_common_def.h"
#include "ubse_mmi_interface.h"

namespace ubse::mem::decoder::utils {
using namespace common::def;
using namespace adapter_plugins::mti::mami;

struct ImportDecoderParam {
    uint8_t importType;
    uint8_t decoderIdx;
    uint32_t portSet;
    uint32_t flag;   // 指定decoder表属性
    uint64_t handle; // 预引入需要传入预引入的handle
    bool isHighSafety = false; // 是否为高安配置
    adapter_plugins::mmi::UbseTrustRingData trustRingData{};
    std::string type;  // 借用类型
};

struct PreImportDecoderParam {
    uint32_t ubpuId{};
    uint32_t iouId{};
    uint32_t marId{}; // 指定端口组逻辑ID, 已包含chip id和die id; 1650和David V100硬件限制仅0, 3, 4号端口组支持配置UB
    uint32_t dstCNA{};    // 目的CNA
    uint8_t importType{}; // 内存借用类型：普通内存借入/共享、静态预引入或动态预引入
    uint32_t flag{};      // 指定decoder表的相关属性
    uint64_t size{};      // 借入内存的大小，单位：字节
};

struct DecoderEntryLoc {
    uint32_t ubpuId{};
    uint32_t iouId{};
    uint32_t marId{};
    uint8_t decoderId{}; // 指定decoder表索引

    struct Hash {
        std::size_t operator()(const DecoderEntryLoc &loc) const
        {
            std::size_t seed = 0;
            auto hashUint32 = std::hash<uint32_t>{};
            auto hashUint8 = std::hash<uint8_t>{};
            const uint32_t randValue = 0x9e3779b9;
            seed ^= hashUint32(loc.ubpuId) + randValue + (seed << NO_6) + (seed >> NO_2);
            seed ^= hashUint32(loc.iouId) + randValue + (seed << NO_6) + (seed >> NO_2);
            seed ^= hashUint32(loc.marId) + randValue + (seed << NO_6) + (seed >> NO_2);
            seed ^= hashUint8(loc.decoderId) + randValue + (seed << NO_6) + (seed >> NO_2);
            return seed;
        }
    };
    struct Equal {
        bool operator()(const DecoderEntryLoc &lhs, const DecoderEntryLoc &rhs) const
        {
            return lhs.ubpuId == rhs.ubpuId && lhs.iouId == rhs.iouId && lhs.marId == rhs.marId &&
                   lhs.decoderId == rhs.decoderId;
        }
    };
};

using DecoderLocTohandleValueMap = std::unordered_map<DecoderEntryLoc, std::vector<UbseMamiMemHandleValue>,
                                                      DecoderEntryLoc::Hash, DecoderEntryLoc::Equal>;
using DecoderLocTohandleMap =
    std::unordered_map<DecoderEntryLoc, std::unordered_set<uint64_t>, DecoderEntryLoc::Hash, DecoderEntryLoc::Equal>;

using DecoderLocTohandleDcnaMap = std::unordered_map<DecoderEntryLoc, std::vector<std::pair<uint32_t, uint64_t>>,
                                                     DecoderEntryLoc::Hash, DecoderEntryLoc::Equal>;
class MemDecoderUtils {
public:
    static std::unordered_map<uint32_t, uint32_t> portToPortSet;
    static UbseResult GetChipAndDieId(uint32_t socketId, std::pair<uint32_t, uint32_t> &chipDiePair);
    static UbseResult GetAllHandles(uint8_t type, DecoderLocTohandleValueMap &handleValues);
    static UbseResult GetCurNodeSocketInfo(std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> &outSocketInfo);
    static UbseResult SetParamMarId(uint32_t slotId, uint32_t remoteSlotId, uint32_t chipId, uint32_t remoteChipId,
                                    ImportDecoderParam &importParam);
    static UbseResult GetAllHandleFromImportObj(DecoderLocTohandleMap &handleMap);
    static void SetImportDecoderParam(decoder::utils::ImportDecoderParam &importParam);
    static void SetImportDecoderParam(decoder::utils::ImportDecoderParam &importParam,
                                      const ubse::adapter_plugins::mmi::UbseMemPrivData &privData);
    static void SetImportDecoderParam(decoder::utils::ImportDecoderParam &importParam, uint16_t wrDelayComp);
    static uint32_t PreImportDecoderEntry(const decoder::utils::PreImportDecoderParam &importDecoderParam,
                                          UbseMamiMemImportResult &outValue);
    static UbseResult GetAllHandleFromNumaImportObj(DecoderLocTohandleDcnaMap &handleMap);
};
} // namespace ubse::mem::decoder::utils

#endif
