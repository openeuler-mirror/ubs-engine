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
#include "default_strategy.h"
#include <algorithm>
#include <sstream>
#include <string>

namespace default_strategy {
using std::to_string;

const int BYTE2MB = 20;
const int BYTE2GB = 30;
const uint64_t SPECS_1G = static_cast<uint64_t>(1) << BYTE2GB;
const uint64_t SPECS_2G = static_cast<uint64_t>(2) << BYTE2GB;
const uint64_t SPECS_3G = static_cast<uint64_t>(3) << BYTE2GB;
const uint64_t SPECS_4G = static_cast<uint64_t>(4) << BYTE2GB;
const uint64_t SPECS_8G = static_cast<uint64_t>(8) << BYTE2GB;
const uint64_t SPECS_16G = static_cast<uint64_t>(16) << BYTE2GB;
const uint64_t SPECS_20G = static_cast<uint64_t>(20) << BYTE2GB;
const float MAX_OVER_COMMIT = 1.5;
const uint64_t SPECS_128M = static_cast<uint64_t>(128) << BYTE2MB;
const size_t MAX_ONE_BORROW = 5;
const float MIG_OVER_RECOVERY = 3;
const int TIME_SECOND_90 = 90;
const int TIME_SECOND_120 = 120;
const int TIME_SECOND_300 = 300;
const int TIME_SECOND_600 = 600;
const int TIME_SECOND_1800 = 1800;
const float MAX_MEM_BORROW_LOWEST = 0;
const float MAX_MEM_BORROW_HIGHEST = 0.8;
std::vector<uint64_t> MemPerBorrow{SPECS_1G, SPECS_2G, SPECS_3G, SPECS_4G};
Logfunc g_vmLogFunc = nullptr;

void LogOut(StrategyLogLevel level, std::string msg)
{
    if (g_vmLogFunc != nullptr) {
        g_vmLogFunc(static_cast<uint32_t>(level), msg.c_str());
    }
}

void StrategyLogInfo(const std::string& msg)
{
    LogOut(StrategyLogLevel::INFO, msg);
}

void StrategyLogDebug(const std::string& msg)
{
    LogOut(StrategyLogLevel::DEBUG, msg);
}
void StrategyLogWarn(const std::string& msg)
{
    LogOut(StrategyLogLevel::WARN, msg);
}
void StrategyLogError(const std::string& msg)
{
    LogOut(StrategyLogLevel::ERROR, msg);
}

#ifdef __cplusplus
extern "C" {
#endif
int EscapeAlgorithmInit(const StrategyConfig& strategyConf, Logfunc logfunc)
{
    auto ret = DefaultStrategy::GetInstance().Init(logfunc, strategyConf);
    return static_cast<int>(ret);
}

int EscapeAlgorithm(const StrategyConfig& strategyConf, AlarmNumaInfo& alarmNumaInfo,
                    GlobalNumaInfoMap& globalNumaInfoMap, EscapeAction& escapeAction)
{
    auto& defaultStrategy = DefaultStrategy::GetInstance();
    defaultStrategy.LoadConfig(strategyConf);
    auto ret = defaultStrategy.EscapeHandleEvent(alarmNumaInfo, globalNumaInfoMap, escapeAction);
    return static_cast<int>(ret);
}
#ifdef __cplusplus
}
#endif

VMNodeLocInfo TransferNodeInfo(NodeLocInfo& nodeLocInfo)
{
    VMNodeLocInfo tmp{};
    tmp.hostId = nodeLocInfo.hostId;
    tmp.numaId = nodeLocInfo.numaId;
    tmp.socketId = nodeLocInfo.socketId;
    return tmp;
}

DsResult DefaultStrategy::InitLogFunc(const Logfunc& logFunc) const
{
    if (logFunc == nullptr) {
        return DS_ERR_LOG_INIT;
    }
    g_vmLogFunc = logFunc;
    return DS_OK;
}

DsResult DefaultStrategy::Init(Logfunc& logFunc, const StrategyConfig& strategyConf)
{
    // Initialization Log Function Log
    DsResult ret = InitLogFunc(logFunc);
    if (ret != DS_OK) {
        return DS_ERR_LOG_INIT;
    }
    LoadConfig(strategyConf);
    std::ostringstream oss;
    oss << "Init strategy: reclaimWaterMark = " << to_string(memReturnLine)
        << ", migrateWaterMark = " << to_string(memFirstLine) << ", borrowWaterMark = " << to_string(memSecondLine)
        << ", maxMemBorrow = " << to_string(maxMemBorrow)
        << ", maxMemPerBorrowBytes = " << to_string(maxMemPerBorrowBytes)
        << ", minMemPerBorrowBytes = " << to_string(minMemPerBorrowBytes)
        << ", memBorrowAlignBytes = " << to_string(memBorrowAlignBytes)
        << ", maxPerTotalMemBorrowBytes = " << to_string(maxPerTotalMemBorrowBytes)
        << ", oomEventBorrowBytes = " << to_string(oomEventBorrowBytes);
    StrategyLogInfo(oss.str());
    return DS_OK;
}

void DefaultStrategy::LoadConfig(const StrategyConfig& strategyConf)
{
    // If the configuration is improper, use the default value.
    try {
        (void)SetWaterLine(std::stoi(strategyConf.borrowWatermark) * 0.01f,
                           std::stoi(strategyConf.highWatermark) * 0.01f, std::stoi(strategyConf.lowWatermark) * 0.01f);
    } catch (const std::exception& e) {
        StrategyLogError("Failed to setWaterLine, error: " + std::string(e.what()));
        return;
    }

    (void)SetMaxMemBorrow(strategyConf.maxMemBorrow);

    (void)SetPerBorrowBound(strategyConf.minMemPerBorrowBytes, strategyConf.maxMemPerBorrowBytes);
    (void)SetMaxPerTotalMemBorrowBytes(strategyConf.maxPerTotalMemBorrowBytes);
    (void)SetOomBorrowMemSize(strategyConf.oomEventBorrowBytes);

    StrategyLogDebug(ToString());
}

static uint64_t GetMaxBorrowedBlockSize(const BorrowItemInfo* borrowItemInfo)
{
    if (borrowItemInfo == nullptr) {
        StrategyLogInfo("borrowItemInfo is nullptr");
        return 0;
    }
    uint64_t maxBorrowedBlockSize = 0;
    for (size_t i = 0; i < borrowItemInfo->borrowItem.size(); i++) {
        uint64_t theBorrowedBlockSize = 0;

        for (auto size : borrowItemInfo->borrowItem[i].requestSize) {
            auto requestSize = size;
            if (theBorrowedBlockSize > std::numeric_limits<uint64_t>::max() - requestSize) {
                theBorrowedBlockSize = DefaultStrategy::GetInstance().GetMaxMemPerBorrowBytes() >> 1;
                StrategyLogWarn("The sum of requestSize exceeds the range of uint64_t, "
                                "the name of borrowItem is " +
                                borrowItemInfo->borrowItem[i].name);
                break;
            }
            theBorrowedBlockSize += requestSize;
        }
        maxBorrowedBlockSize = std::max(maxBorrowedBlockSize, theBorrowedBlockSize);
    }
    StrategyLogInfo("maxBorrowedBlockSize=" + to_string(maxBorrowedBlockSize));
    return maxBorrowedBlockSize;
}

static bool CmpNumaRisk(const std::pair<std::string, uint64_t>& a, const std::pair<std::string, uint64_t>& b)
{
    return a.second > b.second;
}

static uint64_t Align(const uint64_t n, const uint64_t align)
{
    if (n > std::numeric_limits<uint64_t>::max() - (align - 1)) {
        return (std::numeric_limits<uint64_t>::max() & (~(align - 1)));
    }
    return ((n + align - 1) & (~(align - 1)));
}

/**
 * @brief Safe multiplication between a floating-point factor and an unsigned 64-bit integer
 *
 * This function provides a safe multiplication operation with the following characteristics：
 * 1. The input factor is restricted to the range [0, 1]
 * 2. Integer overflow is prevented
 * 3. The result is calculated using floor rounding
 *
 * @param a Multiplication factor, must be within the range [0, 1]
 * @param b Multiplicand, an unsigned 64-bit integer
 *
 * @return The calculated result:
 *         - Returns 0 if the factor is not within [0, 1]
 *         - Returns the maximum value of uint64_t if the computation would overflow
 *         - Otherwise, returns the floored multiplication result
 *
 * @note Suitable for scenarios such as memory ratio calculations
 * @warning Not suitable for cases requiring precise arithmetic
 */
static uint64_t Mul(const float a, uint64_t b)
{
    // 1. range check
    if (a < 0.0f || a > 1.0f) {
        // Record abnormalities or return 0.
        return 0;
    }

    // 2. Overflow Prevention
    if (b > 0 && a > static_cast<float>(std::numeric_limits<uint64_t>::max()) / b) {
        // Return the maximum value or handle overflow
        return std::numeric_limits<uint64_t>::max();
    }

    // 3. secure computing
    return static_cast<uint64_t>(std::floor(a * static_cast<float>(b)));
}

void ResetEscapeAction(EscapeAction& escapeAction)
{
    escapeAction.actionType = EscapeActionType::NOPE;
    escapeAction.returnMemNames.clear();
    escapeAction.borrowSizes.clear();
}

DsResult DefaultStrategy::EscapeHandleEvent(AlarmNumaInfo& alarmNumaInfo, GlobalNumaInfoMap& globalNumaInfoMap,
                                            EscapeAction& escapeAction) const
{
    // check params
    if (globalNumaInfoMap.find(alarmNumaInfo.numaLoc) == globalNumaInfoMap.end()) {
        StrategyLogError("alarm numa loc: " + alarmNumaInfo.numaLoc.ToString() + " not in global numa info map.");
        return DS_ERROR_INVAL;
    }
    const DsResult ret = RealEscapeHandleEvent(alarmNumaInfo, globalNumaInfoMap, escapeAction);
    const GlobalNumaInfo& numaInfo = globalNumaInfoMap[alarmNumaInfo.numaLoc];
    const uint64_t numaMemTotalSize = numaInfo.numaMemTotal;
    const uint64_t numaMemUsedSize = numaInfo.numaMemUsed;
    const uint64_t memReturnLineSize = Mul(GetMemReturnLine(), numaMemTotalSize);
    const uint64_t memFirstLineSize = Mul(GetMemFirstLine(), numaMemTotalSize);
    const uint64_t memSecondLineSize = Mul(memSecondLine, numaMemTotalSize);

    StrategyLogInfo("AlarmNumaLoc = " + numaInfo.numaLoc.ToString() +
                    ", numaMemTotalSize = " + to_string(numaMemTotalSize >> BYTE2MB) +
                    "MB, numaMemUsedSize = " + to_string(numaMemUsedSize >> BYTE2MB) +
                    "MB, memReturnLineSize = " + to_string(memReturnLineSize >> BYTE2MB) +
                    "MB, memFirstLineSize = " + to_string(memFirstLineSize >> BYTE2MB) +
                    "MB, memSecondLineSize = " + to_string(memSecondLineSize >> BYTE2MB) +
                    "MB, FinalDecision actionType = " + to_string(static_cast<int>(escapeAction.actionType)) +
                    R"(, default_strategy_json = {"input": {)" + alarmNumaInfo.ToString() + ", " +
                    GlobalNumaInfoMapToString(globalNumaInfoMap) + R"(}, "output": )" + escapeAction.ToString() + ", " +
                    ToString() + "}");
    return ret;
}

DsResult DefaultStrategy::RealEscapeHandleEvent(AlarmNumaInfo& alarmNumaInfo, GlobalNumaInfoMap& globalNumaInfoMap,
                                                EscapeAction& escapeAction) const
{
    if (globalNumaInfoMap.find(alarmNumaInfo.numaLoc) == globalNumaInfoMap.end()) {
        StrategyLogError("alarm numa not in global numa info map. ");
        return DS_ERR_BOR_MEM_RETMOTE;
    }
    ResetEscapeAction(escapeAction);
    escapeAction.curNodeLoc = alarmNumaInfo.numaLoc;
    ExpectedRevenue expectedRevenue{
        .returnSize = 0,
        .borrowSize = 0,
        .maxReturnSize = 0,
    };
    const DsResult ret = Preprocess(alarmNumaInfo, globalNumaInfoMap, expectedRevenue, escapeAction);
    StrategyLogDebug("preDecision making actionType: " + to_string(static_cast<int>(escapeAction.actionType)));
    if (ret != DS_OK || escapeAction.actionType == EscapeActionType::NOPE) {
        return ret;
    }
    return GetActionType(alarmNumaInfo, globalNumaInfoMap, escapeAction, expectedRevenue);
}

DsResult DefaultStrategy::GetActionType(AlarmNumaInfo& alarmNumaInfo, GlobalNumaInfoMap& globalNumaInfoMap,
                                        EscapeAction& escapeAction, ExpectedRevenue& expectedRevenue) const
{
    DsResult ret = DS_OK;
    if (escapeAction.actionType == EscapeActionType::RETURN) {
        ret = ReturnMem(alarmNumaInfo, globalNumaInfoMap, expectedRevenue, escapeAction);
        if (ret != DS_OK || escapeAction.returnMemNames.empty()) {
            escapeAction.actionType = EscapeActionType::NOPE;
        }
        return ret;
    }
    if (escapeAction.actionType == EscapeActionType::BORROW) {
        // when request to borrow 4GB,  can borrow 1 GB?
        if (expectedRevenue.borrowSizes.empty()) {
            escapeAction.actionType = EscapeActionType::NOPE;
        } else {
            escapeAction.borrowSizes = expectedRevenue.borrowSizes;
        }
        return DS_OK;
    }
    StrategyLogWarn("attention: EscapeHandleEvent has failed.");
    escapeAction.actionType = EscapeActionType::NOPE;
    return ret;
}

void DefaultStrategy::PreprocessBorrowMem(AlarmNumaInfo& alarmNumaInfo, GlobalNumaInfo& numaInfo,
                                          ExpectedRevenue& expectedRevenue, EscapeAction& escapeAction) const
{
    const uint64_t numaMemTotalSize = numaInfo.numaMemTotal;
    const uint64_t numaMemUsedSize = numaInfo.numaMemUsed;
    const uint64_t minMemOverageRecoverySize = Mul(minMemOverageRecovery, numaMemTotalSize);

    // Judgment strategy based on memory watermark
    const uint64_t memSecondLineSize = Mul(memSecondLine, numaMemTotalSize);
    StrategyLogDebug("memSecondLineSize = " + to_string(memSecondLineSize >> BYTE2MB) + "MB");
    if (numaMemUsedSize > memSecondLineSize) {
        // Calculate the expected return on borrowed funds
        uint64_t expectedBorrowRevenue = numaMemUsedSize - memSecondLineSize + minMemOverageRecoverySize;
        if (const StrategyTip ret =
                CalBorrowMem(alarmNumaInfo, numaInfo, expectedBorrowRevenue, expectedRevenue.borrowSizes);
            ret != StrategyTip::NOPE) {
            escapeAction.strategyTips.emplace_back(ret);
        }
        expectedRevenue.borrowSize = expectedBorrowRevenue;

        escapeAction.actionType = EscapeActionType::BORROW;
        StrategyLogDebug("expected.borrowSize = " + to_string(expectedRevenue.borrowSize >> BYTE2MB) + "MB");
        return;
    }
    escapeAction.actionType = EscapeActionType::NOPE;
}

DsResult DefaultStrategy::Preprocess(AlarmNumaInfo& alarmNumaInfo, GlobalNumaInfoMap& globalNumaInfoMap,
                                     ExpectedRevenue& expectedRevenue, EscapeAction& escapeAction) const
{
    GlobalNumaInfo& numaInfo = globalNumaInfoMap[alarmNumaInfo.numaLoc];
    uint64_t numaMemTotalSize = numaInfo.numaMemTotal;
    uint64_t numaMemUsedSize = numaInfo.numaMemUsed;
    uint64_t memReturnLineSize = Mul(GetMemReturnLine(), numaMemTotalSize);
    uint64_t memFirstLineSize = Mul(GetMemFirstLine(), numaMemTotalSize);
    StrategyLogDebug("alarmNumaInfo:" + numaInfo.numaLoc.ToString() +
                     ", numaMemTotalSize = " + to_string(numaMemTotalSize >> BYTE2MB) +
                     "MB, numaMemUsedSize = " + to_string(numaMemUsedSize >> BYTE2MB) +
                     "MB, memReturnLineSize = " + to_string(memReturnLineSize >> BYTE2MB) +
                     "MB, memFirstLineSize = " + to_string(memFirstLineSize >> BYTE2MB) + "MB");

    if (alarmNumaInfo.isMemReturn) {
        escapeAction.actionType = EscapeActionType::RETURN;
        if (numaMemUsedSize >= memReturnLineSize) {
            StrategyLogWarn("DefaultStrategy::ReturnMem failed. numaMemUsedSize = " + to_string(numaMemUsedSize) +
                            "Byte, memReturnLineSize = " + to_string(memReturnLineSize) + "Byte");
            escapeAction.actionType = EscapeActionType::NOPE;
            return DS_ERR_RET_MEM_LINE;
        }
        // Calculate the expected return on the repayment target
        expectedRevenue.returnSize = memReturnLineSize - numaMemUsedSize;
        StrategyLogDebug("returnSize = " + to_string(expectedRevenue.returnSize >> BYTE2MB) + "MB");
        // update maxReturnSize
        expectedRevenue.maxReturnSize = (memFirstLineSize >> 1) + (memReturnLineSize >> 1) - numaMemUsedSize;
        // When both memFirstLineSize and memReturnLineSize are odd numbers,
        // dividing them directly by 2 will result in a value that is one less. Here, we add one more.
        expectedRevenue.maxReturnSize += ((memFirstLineSize & 1) + (memReturnLineSize & 1)) >> 1;
        StrategyLogDebug("maxReturnSize = " + to_string(expectedRevenue.maxReturnSize >> BYTE2MB) + "MB");
        return DS_OK;
    }
    if (alarmNumaInfo.oomEventFlag != 0) {
        uint64_t expectedBorrowRevenue = oomEventBorrowBytes;
        StrategyTip ret = CalBorrowMem(alarmNumaInfo, numaInfo, expectedBorrowRevenue, expectedRevenue.borrowSizes);
        if (ret != StrategyTip::NOPE) {
            escapeAction.strategyTips.emplace_back(ret);
        }
        expectedRevenue.borrowSize = expectedBorrowRevenue;
        escapeAction.actionType = EscapeActionType::BORROW;
        StrategyLogDebug("mem event with oom.");
        StrategyLogDebug("expected.borrowSize = " + to_string(expectedRevenue.borrowSize >> BYTE2MB) + "MB");
        return DS_OK;
    }
    PreprocessBorrowMem(alarmNumaInfo, numaInfo, expectedRevenue, escapeAction);
    return DS_OK;
}

void MonitorNumaMemoryBorrowing(const AlarmNumaInfo& alarmNumaInfo, const GlobalNumaInfo& numaInfo,
                                std::vector<size_t>& borrowSizes, uint64_t& numaRemoteUsedMemSize)
{
    for (auto& it : alarmNumaInfo.vmBasicInfos) {
        numaRemoteUsedMemSize += it.second.remoteUsedMem;
        StrategyLogDebug("vm_name = " + it.second.name +
                         ", remoteUsedMem = " + to_string(it.second.remoteUsedMem >> BYTE2MB) + "MB");
    }
    StrategyLogDebug("numaMemBorrow = " + to_string(numaInfo.numaMemBorrow >> BYTE2MB) +
                     "MB, numaRemoteUsedMemSize = " + to_string(numaRemoteUsedMemSize >> BYTE2MB) + "MB");
    borrowSizes.clear();
}

StrategyTip DefaultStrategy::CalBorrowMem(AlarmNumaInfo& alarmNumaInfo, GlobalNumaInfo& numaInfo,
                                          uint64_t& expectedRevenue, std::vector<size_t>& borrowSizes) const
{
    StrategyTip strategyTip = StrategyTip::NOPE;
    uint64_t remainMaxBorrowSize = Mul(GetMaxMemBorrow(), numaInfo.numaMemTotal) - numaInfo.numaMemBorrow;
    uint64_t numaRemoteUsedMemSize = 0;
    MonitorNumaMemoryBorrowing(alarmNumaInfo, numaInfo, borrowSizes, numaRemoteUsedMemSize);
    // Expected borrowed memory size + Current used borrowed memory size > Current total borrowed memory size
    if ((expectedRevenue + numaRemoteUsedMemSize) > numaInfo.numaMemBorrow) {
        expectedRevenue = expectedRevenue + numaRemoteUsedMemSize - numaInfo.numaMemBorrow;

        // align:
        remainMaxBorrowSize = Align(remainMaxBorrowSize, memBorrowAlignBytes);
        expectedRevenue = Align(expectedRevenue, memBorrowAlignBytes);

        StrategyLogDebug("expectedRevenue = " + to_string(expectedRevenue >> BYTE2MB) +
                         "MB, remainMaxBorrowSize = " + to_string(remainMaxBorrowSize >> BYTE2MB) +
                         "MB, maxPerTotalMemBorrowBytes = " + to_string(maxPerTotalMemBorrowBytes >> BYTE2MB) + "MB");
        uint64_t maxBorrowedBlockSize = GetMaxBorrowedBlockSize(&alarmNumaInfo.borrowItemInfo);
        if (maxBorrowedBlockSize == 0) {
            maxBorrowedBlockSize = minMemPerBorrowBytes >> 1;
        }
        // Generate request for borrowing memory block
        // The messages were repeatedly calculated and borrowed between the two instances;
        // those exceeding the limit would fail to be borrowed
        uint64_t sumBorrowSize = 0;
        while (sumBorrowSize < expectedRevenue && sumBorrowSize < remainMaxBorrowSize &&
               sumBorrowSize < maxPerTotalMemBorrowBytes) {
            // upper limit
            uint64_t oneBorrowedBlockSize = std::min<uint64_t>(
                maxMemPerBorrowBytes, std::max<uint64_t>(2 * maxBorrowedBlockSize, expectedRevenue - sumBorrowSize));
            oneBorrowedBlockSize = std::min<uint64_t>(oneBorrowedBlockSize, maxPerTotalMemBorrowBytes - sumBorrowSize);
            oneBorrowedBlockSize = std::min<uint64_t>(oneBorrowedBlockSize, remainMaxBorrowSize - sumBorrowSize);
            strategyTip = (oneBorrowedBlockSize == remainMaxBorrowSize) ? StrategyTip::BOR_NOT_ENOUGH_CREDIT :
                                                                          strategyTip;
            // lower limit
            if (oneBorrowedBlockSize < minMemPerBorrowBytes) {
                break;
            }
            sumBorrowSize += oneBorrowedBlockSize;
            borrowSizes.emplace_back(oneBorrowedBlockSize);
            StrategyLogDebug("oneBorrowedBlockSize = " + to_string(oneBorrowedBlockSize >> BYTE2MB) + "MB");
            if (oneBorrowedBlockSize > maxBorrowedBlockSize) {
                maxBorrowedBlockSize = oneBorrowedBlockSize;
            }
        }
        expectedRevenue = sumBorrowSize; // update sum(requestBorrowSize)
    } else {
        strategyTip = StrategyTip::BOR_BIG_FREE_REMOTE_MEM;
        expectedRevenue = 0;
    }
    StrategyLogDebug("borrowBlockSizes.size() = " + to_string(borrowSizes.size()));
    for (size_t i = 0; i < borrowSizes.size(); i++) {
        StrategyLogDebug("final oneBorrowedBlockSize = " + to_string(borrowSizes[i] >> BYTE2MB) +
                         "MB, block index = " + to_string(i));
    }
    return strategyTip;
}

DsResult GenBorrowTable(AlarmNumaInfo& alarmNumaInfo,
                        std::vector<std::pair<std::string, uint64_t>>& borrowTable) // [<memId, memSize>...]
{
    BorrowItemInfo* borrowItemInfo = &alarmNumaInfo.borrowItemInfo;
    // Prevent overstepping boundaries
    for (int i = 0; i < borrowItemInfo->borrowItem.size(); i++) {
        uint64_t memBorrowedSize = borrowItemInfo->borrowItem[i].requestSize[0];
        borrowTable.emplace_back(borrowItemInfo->borrowItem[i].name, memBorrowedSize);
    }
    return DS_OK;
}

DsResult DefaultStrategy::ReturnMem(AlarmNumaInfo& alarmNumaInfo, GlobalNumaInfoMap& globalNumaInfoMap,
                                    const ExpectedRevenue& expectedRevenue, EscapeAction& escapeAction) const
{
    GlobalNumaInfo& numaInfo = globalNumaInfoMap[alarmNumaInfo.numaLoc];
    StrategyLogDebug("numaInfo.numaMemBorrow = " + to_string(numaInfo.numaMemBorrow >> BYTE2MB) + "MB");
    // Calculate the size of the used remote memory.
    uint64_t numaRemoteUsedMemSize = 0;
    for (auto& it : alarmNumaInfo.vmBasicInfos) {
        numaRemoteUsedMemSize += it.second.remoteUsedMem;
    }
    StrategyLogDebug("numaRemoteUsedMemSize = " + to_string(numaRemoteUsedMemSize >> BYTE2MB) + "MB");
    // Sort by Borrowing in descending order
    std::vector<std::pair<std::string, uint64_t>> borrowTable; // [<memId, memSize>...]
    DsResult ret = GenBorrowTable(alarmNumaInfo, borrowTable);
    if (ret != DS_OK) {
        return ret;
    }
    uint64_t sumReturnSize = 0;
    sort(borrowTable.begin(), borrowTable.end(), CmpNumaRisk);
    for (auto& m : borrowTable) {
        if (sumReturnSize > std::numeric_limits<uint64_t>::max() - m.second) {
            return DS_ERROR_INVAL;
        }
        sumReturnSize += m.second;
        // check whether the return conditions are met.
        if (numaRemoteUsedMemSize > std::numeric_limits<uint64_t>::max() - sumReturnSize ||
            expectedRevenue.maxReturnSize > std::numeric_limits<uint64_t>::max() - numaInfo.numaMemBorrow) {
            return DS_ERROR_INVAL;
        } else if (numaRemoteUsedMemSize + sumReturnSize > expectedRevenue.maxReturnSize + numaInfo.numaMemBorrow) {
            sumReturnSize -= m.second;
            continue;
        }
        escapeAction.returnMemNames.emplace_back(m.first);
        StrategyLogDebug("returnMemNames = " + escapeAction.returnMemNames.back() +
                         ", returnMemBlockSize = " + to_string(m.second >> BYTE2MB) +
                         "MB, sumReturnSize = " + to_string(sumReturnSize >> BYTE2MB) + "MB");
    }
    if (sumReturnSize == 0) {
        escapeAction.strategyTips.emplace_back(StrategyTip::RET_BAN_RET_TOO_LARGE_MEM);
    }
    StrategyLogDebug("ReturnMem:expectedRevenue = " + to_string(expectedRevenue.returnSize >> BYTE2MB) +
                     "MB, sumReturnSize = " + to_string(sumReturnSize >> BYTE2MB) + "MB");
    return DS_OK;
}

std::string DefaultStrategy::GlobalNumaInfoMapToString(const GlobalNumaInfoMap& globalNumaInfoMap)
{
    std::ostringstream oss;
    oss << R"("globalNumaInfoMap": [)";
    size_t count = 0;
    for (const auto& [fst, snd] : globalNumaInfoMap) {
        count++;
        if (count == globalNumaInfoMap.size()) {
            oss << snd.ToString();
        } else {
            oss << snd.ToString() << ",";
        }
    }
    oss << "]";
    return oss.str();
}

DsResult DefaultStrategy::SetWaterLine(float secondLine, float firstLine, float returnLine)
{
    if (!(returnLine > 0)) {
        StrategyLogError("!(returnLine > 0), mem_second_line = " + to_string(secondLine) +
                         ", mem_first_line = " + to_string(firstLine) + ", mem_return_line = " + to_string(returnLine));
        return DS_ERR_SET_INVAL;
    }
    if (!(firstLine > returnLine)) {
        StrategyLogError("!(firstLine > returnLine), mem_second_line = " + to_string(secondLine) +
                         ", mem_first_line = " + to_string(firstLine) + ", mem_return_line = " + to_string(returnLine));
        return DS_ERR_SET_INVAL;
    }
    if (!(secondLine >= firstLine)) {
        StrategyLogError("!(secondLine >= firstLine), mem_second_line = " + to_string(secondLine) +
                         ", mem_first_line = " + to_string(firstLine) + ", mem_return_line = " + to_string(returnLine));
        return DS_ERR_SET_INVAL;
    }
    if (!(secondLine <= 1)) {
        StrategyLogError("!(secondLine <= 1), mem_second_line = " + to_string(secondLine) +
                         ", mem_first_line = " + to_string(firstLine) + ", mem_return_line = " + to_string(returnLine));
        return DS_ERR_SET_INVAL;
    }
    if (!(secondLine - minMemOverageRecovery > returnLine)) {
        StrategyLogError(
            "!(secondLine - minMemOverageRecovery > returnLine), mem_second_line = " + to_string(secondLine) +
            ", mem_first_line = " + to_string(firstLine) + ", mem_return_line = " + to_string(returnLine));
        return DS_ERR_SET_INVAL;
    }
    memSecondLine = secondLine;
    memFirstLine = firstLine;
    memReturnLine = returnLine;
    StrategyLogDebug("memSecondLine = " + to_string(memSecondLine) + ", memFirstLine = " + to_string(memFirstLine) +
                     ", memReturnLine = " + to_string(memReturnLine));
    return DS_OK;
}

DsResult DefaultStrategy::SetMinMemOverageRecovery(float minOverageRecovery)
{
    if (minOverageRecovery > 0 && minOverageRecovery < (memSecondLine - memReturnLine)) {
        minMemOverageRecovery = minOverageRecovery;
        return DS_OK;
    } else {
        StrategyLogWarn("SetMinMemOverageRecovery = " + to_string(minOverageRecovery) +
                        ", (memSecondLine - memReturnLine) = " + to_string(memSecondLine - memReturnLine) +
                        ", not in range(0, (memSecondLine - memReturnLine)), " +
                        "default minMemOverageRecovery = " + to_string(minMemOverageRecovery));
        return DS_ERR_SET_INVAL;
    }
}

DsResult DefaultStrategy::SetMaxMemBorrow(float maxBorrow)
{
    if (maxBorrow > MAX_MEM_BORROW_LOWEST && maxBorrow <= MAX_MEM_BORROW_HIGHEST) {
        maxMemBorrow = maxBorrow;
        return DS_OK;
    } else {
        StrategyLogWarn("SetMaxMemBorrow = " + to_string(maxBorrow) + ", not in range(0, 0.8], " +
                        "default maxMemBorrow = " + to_string(maxMemBorrow));
        return DS_ERR_SET_INVAL;
    }
}

DsResult DefaultStrategy::SetMemBorrowAlignBytes(uint64_t borrowAlignBytes)
{
    if (borrowAlignBytes >= SPECS_128M && borrowAlignBytes <= SPECS_4G) {
        memBorrowAlignBytes = borrowAlignBytes;
        return DS_OK;
    } else {
        StrategyLogWarn("SetMemBorrowAlignBytes = " + to_string(borrowAlignBytes) + ", not in range[128MB, 4GB], " +
                        "default memBorrowAlignBytes = " + to_string(memBorrowAlignBytes));
        return DS_ERR_SET_INVAL;
    }
}

DsResult DefaultStrategy::SetPerBorrowBound(uint64_t minPerBorrowBytes, uint64_t maxPerBorrowBytes)
{
    auto it1 = std::find(MemPerBorrow.begin(), MemPerBorrow.end(), minPerBorrowBytes);
    auto it2 = std::find(MemPerBorrow.begin(), MemPerBorrow.end(), maxPerBorrowBytes);
    if (minPerBorrowBytes >= memBorrowAlignBytes && maxPerBorrowBytes >= minPerBorrowBytes) {
        minMemPerBorrowBytes = (it1 != MemPerBorrow.end()) ? *it1 : minMemPerBorrowBytes;
        maxMemPerBorrowBytes = (it2 != MemPerBorrow.end()) ? *it2 : maxMemPerBorrowBytes;
        return DS_OK;
    } else {
        StrategyLogWarn(
            "SetPerBorrowBound:minPerBorrowBytes = " + to_string(minPerBorrowBytes) + ", maxPerBorrowBytes = " +
            to_string(maxPerBorrowBytes) + ", borrowAlignBytes = " + to_string(memBorrowAlignBytes) +
            ", not in range[borrowAlignBytes, 4GB], default minMemPerBorrowBytes = " + to_string(minMemPerBorrowBytes) +
            ", default maxMemPerBorrowBytes = " + to_string(maxMemPerBorrowBytes));
        return DS_ERR_SET_INVAL;
    }
}

DsResult DefaultStrategy::SetMaxPerTotalMemBorrowBytes(uint64_t maxOnceMemBorrowBytes)
{
    if (maxOnceMemBorrowBytes >= SPECS_4G && maxOnceMemBorrowBytes <= SPECS_20G) {
        // Use the value aligned to 4 GB
        uint64_t alignMaxOnceMemBorrowBytes = Align(maxOnceMemBorrowBytes, SPECS_4G);
        StrategyLogDebug("SetMaxPerTotalMemBorrowBytes, before alignment = " + to_string(maxOnceMemBorrowBytes) +
                         ", after alignment = " + to_string(alignMaxOnceMemBorrowBytes) + ", use aligned value.");
        maxPerTotalMemBorrowBytes = alignMaxOnceMemBorrowBytes;
        return DS_OK;
    } else {
        StrategyLogWarn("SetMaxPerTotalMemBorrowBytes = " + to_string(maxOnceMemBorrowBytes) +
                        ", not in range[4GB, 20GB], " +
                        "default maxPerTotalMemBorrowBytes = " + to_string(maxPerTotalMemBorrowBytes));
        return DS_ERR_SET_INVAL;
    }
}

DsResult DefaultStrategy::SetOomBorrowMemSize(uint64_t oomBorrowMemSize)
{
    if (oomBorrowMemSize >= SPECS_1G && oomBorrowMemSize <= SPECS_4G) {
        oomEventBorrowBytes = oomBorrowMemSize;
        return DS_OK;
    } else {
        StrategyLogWarn("SetOomBorrowMemSize = " + to_string(oomBorrowMemSize) + ", not in range[1G,4G], " +
                        "default oomBorrowMemSize = " + to_string(oomEventBorrowBytes));
        return DS_ERR_SET_INVAL;
    }
}

float DefaultStrategy::GetMemReturnLine() const
{
    return memReturnLine;
}

float DefaultStrategy::GetMemFirstLine() const
{
    return memFirstLine;
}

float DefaultStrategy::GetMemSecondLine() const
{
    return memSecondLine;
}

float DefaultStrategy::GetMinMemOverageRecovery() const
{
    return minMemOverageRecovery;
}

float DefaultStrategy::GetMaxMemBorrow() const
{
    return maxMemBorrow;
}

uint64_t DefaultStrategy::GetMaxMemPerBorrowBytes() const
{
    return maxMemPerBorrowBytes;
}

uint64_t DefaultStrategy::GetMinMemPerBorrowBytes() const
{
    return minMemPerBorrowBytes;
}

uint64_t DefaultStrategy::GetMemBorrowAlignBytes() const
{
    return memBorrowAlignBytes;
}

uint64_t DefaultStrategy::GetMaxPerTotalMemBorrowBytes() const
{
    return maxPerTotalMemBorrowBytes;
}

uint64_t DefaultStrategy::GetOomBorrowMemSize() const
{
    return oomEventBorrowBytes;
}
} // namespace default_strategy