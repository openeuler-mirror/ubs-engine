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
#ifndef DEFAULT_STRATEGY_H
#define DEFAULT_STRATEGY_H

#include <cmath>
#include <sstream>
#include "default_struct.h"

namespace default_strategy {

using Logfunc = void (*)(uint32_t, const char *);
struct StrategyConfig {
    std::string borrowWatermark{};
    // Decision configuration, but it depends on mem for provisioning. In case of loading failure,
    // default values must be provided
    std::string lowWatermark{};
    std::string highWatermark{};

    float_t maxMemBorrow = 0;
    uint64_t maxMemPerBorrowBytes = 0;
    uint64_t minMemPerBorrowBytes = 0;
    uint64_t maxPerTotalMemBorrowBytes = 0;
    uint64_t oomEventBorrowBytes = 0;
};

#ifdef __cplusplus
extern "C" {
#endif
int EscapeAlgorithmInit(const StrategyConfig &strategyConf, Logfunc logfunc);
int EscapeAlgorithm(const StrategyConfig &strategyConf, AlarmNumaInfo &alarmNumaInfo,
                    GlobalNumaInfoMap &globalNumaInfoMap, EscapeAction &escapeAction);
#ifdef __cplusplus
}
#endif

struct ExpectedRevenue {
    uint64_t returnSize{};
    uint64_t borrowSize{};
    uint64_t maxReturnSize{};
    std::vector<size_t> borrowSizes{};
};

class DefaultStrategy {
public:
    static DefaultStrategy &GetInstance()
    {
        static DefaultStrategy gInstance;
        return gInstance;
    };
    DsResult Init(Logfunc &logFunc, const StrategyConfig &strategyConf);
    void LoadConfig(const StrategyConfig &strategyConf);
    DsResult InitLogFunc(const Logfunc &logFunc) const;
    DsResult GetActionType(AlarmNumaInfo &alarmNumaInfo, GlobalNumaInfoMap &globalNumaInfoMap,
                           EscapeAction &escapeAction, ExpectedRevenue &expectedRevenue) const;
    // memSecondLine range (0,1], MemFirstLine range (0,secondLine], MemReturnLine range (0,firstLine)
    DsResult SetWaterLine(float secondLine, float firstLine, float returnLine);
    DsResult SetMinMemOverageRecovery(float minOverageRecovery); // range (0,secondLine-returnLine)

    DsResult SetMaxMemBorrow(float maxBorrow);                                          // range (0, 0.8]
    DsResult SetMemBorrowAlignBytes(uint64_t borrowAlignBytes);                         // range [128MB, 4GB]
    DsResult SetPerBorrowBound(uint64_t minPerBorrowBytes, uint64_t maxPerBorrowBytes); // range [borrowAlignBytes, 4GB]
    DsResult SetMaxPerTotalMemBorrowBytes(uint64_t maxOnceMemBorrowBytes);              // range [4GB, 20GB]
    DsResult SetOomBorrowMemSize(uint64_t oomBorrowMemSize);                            // range [1GB,4GB]

    float GetMemReturnLine() const;
    float GetMemFirstLine() const;
    float GetMemSecondLine() const;
    float GetMinMemOverageRecovery() const;
    float GetMaxMemBorrow() const;
    uint64_t GetMaxMemPerBorrowBytes() const;
    uint64_t GetMinMemPerBorrowBytes() const;
    uint64_t GetMemBorrowAlignBytes() const;
    uint64_t GetMaxPerTotalMemBorrowBytes() const;
    uint64_t GetOomBorrowMemSize() const;

    DsResult EscapeHandleEvent(AlarmNumaInfo &alarmNumaInfo, GlobalNumaInfoMap &globalNumaInfoMap,
                               EscapeAction &escapeAction) const;
    void Exit() const {};

private:
    // escape
    float memReturnLine = 0.8;          // Waterline for memory return, floor(numaMemTotal * memReturnLine) bytes
    float memFirstLine = 0.85;          // First escape trigger waterline, floor(numaMemTotal * memFirstLine) bytes
    float memSecondLine = 0.92;         // Second escape trigger waterline, floor(numaMemTotal * memSecondLine) bytes
    float minMemOverageRecovery = 0.02; // Minimum excess memory to recover during escape,
                                        // floor(numaMemTotal * minMemOverageRecovery) bytes
    // mem
    float maxMemBorrow = 0.25; // Upper limit of borrowed memory capacity for a NUMA node,
                               // floor(numaMemTotal*maxMemBorrow) Bytes
    uint64_t maxMemPerBorrowBytes = static_cast<uint64_t>(4) << 30;       // Upper limit per borrow block, in bytes
    uint64_t minMemPerBorrowBytes = static_cast<uint64_t>(1) << 30;       // Lower limit per borrow block, in bytes
    uint64_t memBorrowAlignBytes = static_cast<uint64_t>(1) << 30;        // Alignment size per borrow block
    uint64_t maxPerTotalMemBorrowBytes = static_cast<uint64_t>(16) << 30; // Maximum total memory borrowed per request
    uint64_t oomEventBorrowBytes = static_cast<uint64_t>(1) << 30;        // Maximum total memory borrowed per request

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << R"("conf": {)";
        oss << R"("memReturnLine": )" << memReturnLine << ",";
        oss << R"("memFirstLine": )" << memFirstLine << ",";
        oss << R"("memSecondLine": )" << memSecondLine << ",";
        oss << R"("minMemOverageRecovery": )" << minMemOverageRecovery << ",";

        oss << R"("maxMemBorrow": )" << maxMemBorrow << ",";
        oss << R"("maxMemPerBorrowBytes": )" << maxMemPerBorrowBytes << ",";
        oss << R"("minMemPerBorrowBytes": )" << minMemPerBorrowBytes << ",";
        oss << R"("memBorrowAlignBytes": )" << memBorrowAlignBytes << ",";
        oss << R"("maxPerTotalMemBorrowBytes": )" << maxPerTotalMemBorrowBytes << ",";
        oss << R"("oomEventBorrowBytes": )" << oomEventBorrowBytes;
        oss << "}";
        return oss.str();
    }

    DsResult RealEscapeHandleEvent(AlarmNumaInfo &alarmNumaInfo, GlobalNumaInfoMap &globalNumaInfoMap,
                                   EscapeAction &escapeAction) const;
    void PreprocessBorrowMem(AlarmNumaInfo &alarmNumaInfo, GlobalNumaInfo &numaInfo, ExpectedRevenue &expectedRevenue,
                             EscapeAction &escapeAction) const;
    DsResult Preprocess(AlarmNumaInfo &alarmNumaInfo, GlobalNumaInfoMap &globalNumaInfoMap,
                        ExpectedRevenue &expectedRevenue, EscapeAction &escapeAction) const;

    StrategyTip CalBorrowMem(AlarmNumaInfo &alarmNumaInfo, GlobalNumaInfo &numaInfo, uint64_t &expectedRevenue,
                             std::vector<size_t> &borrowSizes) const;

    DsResult ReturnMem(AlarmNumaInfo &alarmNumaInfo, GlobalNumaInfoMap &globalNumaInfoMap,
                       const ExpectedRevenue &expectedRevenue, EscapeAction &escapeAction) const;
    static std::string GlobalNumaInfoMapToString(const GlobalNumaInfoMap &globalNumaInfoMap);
};
} // namespace default_strategy
#endif // VM_ESCAPE_STRATEGY_H
