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

#ifndef UBSE_MEM_STRATEGY_HELPER_H
#define UBSE_MEM_STRATEGY_HELPER_H
#include <array>
#include <shared_mutex>

#include "mem_pool_strategy.h"
#include "ubse_error.h"
#include "ubse_mem_meta_data.h"
#include "ubse_mem_obj.h"
#include "ubse_mem_resource.h"
#include "ubse_memoryfabric_types.h"
#include "ulog.h"

namespace ubse::mem::strategy {
using namespace ubse::resource::mem;
class UbseMemStrategyHelper {
public:


    static UbseMemStrategyHelper &GetInstance()
    {
        static UbseMemStrategyHelper instance;
        return instance;
    }
    UbseMemStrategyHelper(const UbseMemStrategyHelper &other) = delete;
    UbseMemStrategyHelper(UbseMemStrategyHelper &&other) = delete;
    UbseMemStrategyHelper &operator=(const UbseMemStrategyHelper &other) = delete;
    UbseMemStrategyHelper &operator=(UbseMemStrategyHelper &&other) noexcept = delete;
    /* *
     * @brief 初始化算法，可以重复初始化
     * @return 成功返回0, 失败返回非0
     */
    UbseResult Init();

    UbseResult NumaMemoryBorrow(const obj::UbseMemNumaBorrowReq &req, obj::UbseMemAlgoResult &algoResult);

    UbseResult FdMemoryBorrow(const obj::UbseMemFdBorrowReq &req, obj::UbseMemAlgoResult &algoResult);

    bool FillInitParam(tc::rs::mem::StrategyParam &strategyParam);

    UbseResult CheckBorrowSizeMeetLimit(tc::rs::mem::UbseStatus &ubseStatus, tc::rs::mem::BorrowRequest &borrowRequest);

    void AddNumaLendOutCountByMemIdLoc(const resource::mem::UbseMemNumaLoc &idLoc, uint64_t memLent);

    void AddNumaShareOutCountByMemIdLoc(const resource::mem::UbseMemNumaLoc &idLoc, uint64_t memLent);

    void SubNumaLendOutCountByMemIdLoc(const resource::mem::UbseMemNumaLoc &idLoc, uint64_t memLent);

    void SubNumaShareOutCountByMemIdLoc(const resource::mem::UbseMemNumaLoc &idLoc, uint64_t memLent);

    uint64_t GetNumaLendOutCountByMemIdLoc(const resource::mem::UbseMemNumaLoc &idLoc);

    uint64_t GetNumaShareOutCountByMemIdLoc(const resource::mem::UbseMemNumaLoc &idLoc);

    void RollBackPreNumaLendOut(const obj::UbseMemAlgoResult &algoResult);

    // 更新CNA
    UbseResult AddPreSocketCnaSize(const std::string &importNodeIdSocketId, const std::string &exportNodeIdSocketId,
                                   uint64_t preAddCnaSize);
    UbseResult SubPreSocketCnaSize(const std::string &importNodeIdSocketId, const std::string &exportNodeIdSocketId,
                                   uint64_t preAddCnaSize);

    UbseResult AddSocketCnaSize(const std::string &importNodeIdSocketId, const std::string &exportNodeIdSocketId,
                                uint64_t AddCnaSize);

    UbseResult SubSocketCnaSize(const std::string &importNodeIdSocketId, const std::string &exportNodeIdSocketId,
                                uint64_t subCnaSize);

    void GetSocketCnaAccount(
        std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> &socketCnaSizeCount);

    void GetSocketCnaSize(const std::string &importNodeIdSocketId,
                          std::unordered_map<std::string, uint64_t> &socketCnaSize);

    void GetPreSocketCnaSize(const std::string &importNodeIdSocketId,
                             std::unordered_map<std::string, uint64_t> &socketCnaSize);
    // 更新debt
    UbseResult GetNumaDebtInfoFromNumaPair(const GlobalNumaIndex &brwNumaGlobalIdx,
                                           const obj::UbseMemDebtNumaInfo &lend, bool add);
    // 过滤numa
    void FilterOutUnavailableNuma(tc::rs::mem::UbseStatus &ubseStatus, const NodeId &importNodeId, uint64_t curSize,
                                  const std::shared_ptr<MemNumaInfo> numa);

    bool CheckRequestNodeHasLentOut(const NodeId &requestNodeId);

    bool CheckProviderNodeHasBorrowed(const NodeId &providerNodeId);

    bool CheckSocketExportOverLimit(const UbseMemNumaLoc &exportLoc, uint64_t curSize);

    UbseResult GetSocketTotalLendOutByMemIdLoc(const UbseMemNumaLoc &idLoc, uint64_t &preSocketTotalExportMem);

    bool CheckSocketImportOverLimit(const UbseMemNumaLoc &exportLoc, const NodeId &importNodeId, uint64_t curSize);

private:
    static void Log(int level, const char *msg)
    {
        switch (level) {
            case static_cast<int>(tc::rs::mem::LogLevel::DEBUG):
                LOG_DEBUG(msg);
                break;
            case static_cast<int>(tc::rs::mem::LogLevel::INFO):
                LOG_INFO(msg);
                break;
            case static_cast<int>(tc::rs::mem::LogLevel::WARN):
                LOG_WARN(msg);
                break;
            case static_cast<int>(tc::rs::mem::LogLevel::ERROR):
                LOG_ERROR(msg);
                break;
            default:
                LOG_INFO(msg);
        }
    }
    UbseMemStrategyHelper() = default;
    UbseResult GetUbseStatus(tc::rs::mem::UbseStatus &ubseStatus, const NodeId &importNodeId, uint64_t curSize);
    std::mutex mIniLock{};
    SpinLock mSpinLock;
    SpinLock mNumaOutCountLock{};
    std::shared_mutex preCnaCountLock{};
    std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> preSocketCnaSizeAccount{};
    std::unordered_map<std::string, int64_t> mNumaLendOutCount{};
    std::unordered_map<std::string, int64_t> mNumaShareOutCount{};
    SpinLock mDebtDetailLock;
    tc::rs::mem::DebtDetail debtDetail{};
    std::atomic_bool mInited{false};
    std::shared_mutex socketCnaAccountLock{};
    std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> socketCnaSizeCount{};
};
} // namespace ubse::mem::strategy

#endif