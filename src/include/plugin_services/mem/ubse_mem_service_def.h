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

#ifndef UBSE_MEM_SERVICE_DEF_H
#define UBSE_MEM_SERVICE_DEF_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "adapter_plugins/mti/ubse_mti_mami_def.h"
namespace ubse::service::mem {

struct UbseNumaNodeInfo {
    std::string nodeId;
    std::string hostName;
    uint32_t numaId;
    uint32_t socketId;
    std::vector<uint16_t> mCpuList;
    uint64_t mReservedMemRatio;
    uint64_t mMemTotal;
    uint64_t mMemFree;
    unsigned int nrHugepages;
    unsigned int freeHugepages;
    uint64_t mTimestamp;
    uint64_t mMemBorrowed;
    uint64_t mMemLent;
    uint64_t mMemShared;
};

struct BasicPreImportInfo {
    uint64_t pa;
    uint32_t scna{};
    uint32_t dcna{};
    uint16_t marId{0};
    int numa_id{};
    uint64_t preOnlineSize{};
    uint32_t seid{0};
    uint32_t deid{0};
};
struct PreImportDecoderParam {
    uint32_t ubpuId{};
    uint32_t iouId{};
    uint32_t marId{};
    uint32_t dstCNA{};
    uint8_t importType{};
    uint32_t flag{};
    uint64_t size{};
};

struct DecoderEntryLoc {
    uint32_t ubpuId{};
    uint32_t iouId{};
    uint32_t marId{};
    uint8_t decoderId{};

    struct Hash {
        std::size_t operator()(const DecoderEntryLoc &loc) const
        {
            std::size_t seed = 0;
            auto hashUint32 = std::hash<uint32_t>{};
            auto hashUint8 = std::hash<uint8_t>{};
            constexpr uint32_t kGoldenRatio = 0x9e3779b9;
            constexpr int kShiftAmount = 6;

            seed ^= hashUint32(loc.ubpuId) + kGoldenRatio + (seed << kShiftAmount) + (seed >> kShiftAmount);
            seed ^= hashUint32(loc.iouId) + kGoldenRatio + (seed << kShiftAmount) + (seed >> kShiftAmount);
            seed ^= hashUint32(loc.marId) + kGoldenRatio + (seed << kShiftAmount) + (seed >> kShiftAmount);
            seed ^= hashUint8(loc.decoderId) + kGoldenRatio + (seed << kShiftAmount) + (seed >> kShiftAmount);
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
};
#endif
