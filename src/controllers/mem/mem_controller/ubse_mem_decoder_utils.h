/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 * http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_MEM_DECODER_UTILS_H
#define UBSE_MEM_DECODER_UTILS_H

#include <unordered_map>
#include <unordered_set>

#include "ubse_common_def.h"
#include "ubse_http_common.h"
#include "ubse_mem_mami_def.h"

namespace ubse::mem::decoder::utils {
using namespace common::def;
using namespace mami;

const uint32_t OFFSET = 2;

struct ImportDecoderParam {
    uint8_t importType;
    uint8_t decoderIdx;
    uint32_t portSet;
    uint32_t flag; // 指定decoder表属性
    uint64_t handle; // 预引入需要传入预引入的handle
};

struct DecoderEntryLoc {
    uint32_t ubpuId{};
    uint32_t iouId{};
    uint32_t marId{};
    uint8_t decoderId{}; // 指定decoder表索引

    struct Hash {
        std::size_t operator()(const DecoderEntryLoc &loc) const
        {
            return ((std::hash<uint32_t>{}(loc.ubpuId) ^ (std::hash<uint32_t>{}(loc.iouId) << 1)) >> 1) ^
                (std::hash<uint32_t>{}(loc.marId) << OFFSET);
        }
    };
    struct Equal {
        bool operator()(const DecoderEntryLoc &lhs, const DecoderEntryLoc &rhs) const
        {
            return lhs.ubpuId == rhs.ubpuId && lhs.iouId == rhs.iouId && lhs.marId == rhs.marId;
        }
    };
};

using DecoderLocTohandleValueMap = std::unordered_map<DecoderEntryLoc, std::vector<UbseMamiMemHandleValue>,
    DecoderEntryLoc::Hash, DecoderEntryLoc::Equal>;
using DecoderLocTohandleMap =
    std::unordered_map<DecoderEntryLoc, std::unordered_set<uint64_t>, DecoderEntryLoc::Hash, DecoderEntryLoc::Equal>;
class MemDecoderUtils {
public:
    static std::unordered_map<uint32_t, uint32_t> portToPortSet;
    static UbseResult GetChipAndDieId(uint32_t socketId, std::pair<uint32_t, uint32_t> &chipDiePair);
    static UbseResult GetAllHandles(DecoderLocTohandleValueMap &handleValues);
    static UbseResult GetCurNodeSocketInfo(std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> &outSocketInfo);
    static UbseResult SetMarIdParam(uint32_t chipId, uint32_t remoteChipId, ImportDecoderParam &importParam);
    static UbseResult GetHandleMapFromImportObj(DecoderLocTohandleMap &handleMap);
private:
    static std::vector<uint32_t> GetAllChipId();
};
}

#endif