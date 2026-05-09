/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef VM_STRATEGY_STRUCT_H
#define VM_STRATEGY_STRUCT_H

#include <sstream>
#include <string>
#include <vector>

namespace vm {
enum VmMigrateStatus {
    MIGRATEABLE = 0,
    MIGRATING,
    MIGRATEUNABLE
};

/**
 * @brief Escape Strategy tips
 * Nope
 * Memory Return Tip: Memory borrow ledger is empty.
 * Memory Return Tip: Prohibition of memory reclamation actions leads to high memory watermark.
 * Memory Borrow Tip: Free remote memory > Expected borrowed memory for this session
 * Memory Borrow Tip: Borrowed memory has reached the memory borrowing limit of this node
 */
enum class StrategyTip : int {
    NOPE = 0,
    RET_BORROW_LEGER_EMPTY,
    RET_BAN_RET_TOO_LARGE_MEM,
    BOR_BIG_FREE_REMOTE_MEM,
    BOR_NOT_ENOUGH_CREDIT,
};

struct StrategyTipInfo {
    static std::string StrategyTipToString(const StrategyTip &strategyTip)
    {
        std::ostringstream oss;
        if (strategyTip == StrategyTip::NOPE) {
            oss << "\"NOPE\"";
        } else if (strategyTip == StrategyTip::RET_BORROW_LEGER_EMPTY) {
            oss << "\"RET_BORROW_LEGER_EMPTY\"";
        } else if (strategyTip == StrategyTip::RET_BAN_RET_TOO_LARGE_MEM) {
            oss << "\"RET_BAN_RET_TOO_LARGE_MEM\"";
        } else if (strategyTip == StrategyTip::BOR_BIG_FREE_REMOTE_MEM) {
            oss << "\"BOR_BIG_FREE_REMOTE_MEM\"";
        } else if (strategyTip == StrategyTip::BOR_NOT_ENOUGH_CREDIT) {
            oss << "\"BOR_NOT_ENOUGH_CREDIT\"";
        }
        return oss.str();
    }

    static std::string StrategyTips(const std::vector<StrategyTip> &strategyTips)
    {
        std::ostringstream oss;
        oss << "\"strategyTip\": [";
        if (strategyTips.empty()) {
            oss << "],";
        } else {
            for (auto it = strategyTips.begin(); it != strategyTips.end(); it++) {
                if (it == strategyTips.end() - 1) {
                    oss << StrategyTipToString(*it);
                } else {
                    oss << StrategyTipToString(*it) << ",";
                }
            }
            oss << "],";
        }
        return oss.str();
    }
};

struct VMNodeLocInfo {
    std::string hostName{};
    std::string hostId{};
    int16_t socketId{};
    int16_t numaId{};

    bool operator<(const VMNodeLocInfo &a) const
    {
        if (hostId != a.hostId) {
            return hostId < a.hostId;
        } else if (socketId != a.socketId) {
            return socketId < a.socketId;
        } else if (numaId != a.numaId) {
            return numaId < a.numaId;
        } else {
            return false;
        }
    }

    bool operator==(const VMNodeLocInfo &a) const
    {
        return (hostId == a.hostId && socketId == a.socketId && numaId == a.numaId);
    }

    std::string toString() const
    {
        std::ostringstream oss;
        oss << "\"" << hostId << "\"," << socketId << "," << numaId;
        return oss.str();
    }
};

struct NumaInfoCollected {
    uint16_t numaId{};                  // real numaId
    uint16_t numaCpuCounts{};           // from Collected
    std::vector<uint16_t> cpuIds{};     // from Collected
    uint64_t numaMemTotal{};            // from Collected
    uint64_t numaMemUsed{};             // from Collected
    uint16_t numaCpuUsedCounts{};       // from Collected
    uint64_t numaVMMemAllocated{};      // from Collected
    std::vector<uint16_t> idleCpuIds{}; // from Collected
};
} // namespace vm
#endif // VM_STRATEGY_STRUCT_H
