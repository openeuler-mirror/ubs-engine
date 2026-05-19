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

#ifndef MXE_MEM_STRATEGY_HELPER_H
#define MXE_MEM_STRATEGY_HELPER_H
#include <array>
#include <shared_mutex>
#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_functions.h"
#include "ubse_mem_meta_data.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_mem_types.h"
#include "ubse_mem_validator.h"
#include "ubse_mmi_interface.h"
#include "mem_pool_strategy.h"

namespace ubse::mem::strategy {
#define MODULE_LOG_NAME "ubse_mem_strategy"
using namespace ubse::common::def;

inline uint32_t GetRandomNumaId(const std::string& nodeId, uint32_t socketId)
{
    auto nodeInfo = ubse::nodeController::UbseNodeController::GetInstance().GetNodeById(nodeId);
    for (auto numaInfo : nodeInfo.numaInfos) {
        if (numaInfo.second.socketId == socketId) {
            return numaInfo.first.numaId;
        }
    }
    return 0;
}

template <class UserReq>
UbseResult GetAlgoResultFromUserRequest(ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult, UserReq req,
                                        int srcSocket = -1, int srcNuma = -1)
{
    if (req.lenderSizes.size() != req.lenderLocs.size()) {
        UBSE_LOG_ERROR << "Size mismatch between lenderSizes and lenderLocs.";
        return UBSE_ERROR;
    }
    auto numaInfo =
        UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(req.lenderLocs[0].nodeId, req.lenderLocs[0].numaId);
    if (numaInfo == nullptr) {
        UBSE_LOG_ERROR << "numaInfo is null for node: " << req.lenderLocs[0].nodeId
                       << " and numa: " << req.lenderLocs[0].numaId;
        return UBSE_ERROR_NULLPTR;
    }
    auto ret = UbseMemTopologyInfoManager::GetInstance().GetAttachNodeId(
        req.importNodeId, numaInfo->mUbseMemNumaLoc.nodeId, numaInfo->mUbseMemNumaLoc.socketId,
        algoResult.attachSocketId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Get socketCnaInfo failed, ret=" << ret;
        return ret;
    }

    auto borrowSocketId = srcSocket == -1 ? static_cast<int>(algoResult.attachSocketId) : srcSocket;
    auto borrowNumaId = srcNuma == -1 ? GetRandomNumaId(req.importNodeId, borrowSocketId) : srcNuma;
    for (int i = 0; i < req.lenderLocs.size(); ++i) {
        auto nodeNumaInfo =
            UbseMemTopologyInfoManager::GetInstance().GetNumaInfo(req.lenderLocs[i].nodeId, req.lenderLocs[i].numaId);
        if (nodeNumaInfo != nullptr) {
            auto blockSize = UbseMemConfiguration::GetInstance().GetBlockSizeFromLenderNode();
            if (!blockSize.has_value()) {
                UBSE_LOG_ERROR << "Get block size failed";
                return UBSE_ERROR;
            }
            algoResult.blockSize = blockSize.value();
            auto exportSize = CeilToN(req.lenderSizes[i], static_cast<uint64_t>(blockSize.value()) * ONE_M);
            UbseMemDebtNumaInfo exportDebtNumaInfo{nodeNumaInfo->mUbseMemNumaLoc.nodeId,
                                                   nodeNumaInfo->mUbseMemNumaLoc.socketId,
                                                   nodeNumaInfo->mUbseMemNumaLoc.numaId, exportSize};
            algoResult.exportNumaInfos.emplace_back(exportDebtNumaInfo);
            UbseMemDebtNumaInfo importDebtNumaInfo{req.importNodeId, borrowSocketId, borrowNumaId, exportSize};
            algoResult.importNumaInfos.emplace_back(importDebtNumaInfo);
        } else {
            UBSE_LOG_ERROR << "NodeNumaInfo is null for node: " << req.lenderLocs[i].nodeId
                           << " and numa: " << req.lenderLocs[i].numaId;
        }
    }
    return UBSE_OK;
}

class UbseMemStrategyHelper {
public:
    static UbseMemStrategyHelper& GetInstance()
    {
        static UbseMemStrategyHelper instance;
        return instance;
    }
    UbseMemStrategyHelper(const UbseMemStrategyHelper& other) = delete;
    UbseMemStrategyHelper(UbseMemStrategyHelper&& other) = delete;
    UbseMemStrategyHelper& operator=(const UbseMemStrategyHelper& other) = delete;
    UbseMemStrategyHelper& operator=(UbseMemStrategyHelper&& other) noexcept = delete;
    /* *
     * @brief 初始化算法，可以重复初始化
     * @return 成功返回0, 失败返回非0
     */
    UbseResult Init();

    UbseResult NumaMemoryBorrow(const ubse::adapter_plugins::mmi::UbseMemNumaBorrowReq& req,
                                ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult, uint64_t checkMaskCode);

    UbseResult FdMemoryBorrow(const ubse::adapter_plugins::mmi::UbseMemFdBorrowReq& req,
                              ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult, uint64_t checkMaskCode);

    UbseResult ShareMemoryBorrow(const ubse::adapter_plugins::mmi::UbseMemShareBorrowReq& req,
                                 ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult, uint64_t checkMaskCode);

    UbseResult AddrMemoryBorrow(const ubse::adapter_plugins::mmi::UbseMemAddrBorrowReq& req,
                                ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult, uint64_t checkMaskCode);

    template <class ReqType>
    UbseResult MemoryBorrowAccordingToUserRequest(ReqType& req,
                                                  ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult,
                                                  uint64_t checkMaskCode, int srcSocket = -1, int srcNuma = -1)
    {
        UbseMemValidator validator{};
        validator.importNodeId_ = req.importNodeId;
        validator.requestSize_ = req.size;
        validator.candidateNodeList_ = req.candidateNodeList;
        if (!req.lenderLocs.empty()) {
            validator.exportNodeId_ = req.lenderLocs[0].nodeId;
        }
        validator.lenderLocs_ = req.lenderLocs;
        validator.lenderSizes_ = req.lenderSizes;
        if (validator.CheckAndFilterParam(checkMaskCode) != UBSE_OK) {
            UBSE_LOG_ERROR << "CheckAndFilterParam failed";
            return UBSE_ERROR;
        }
        return GetAlgoResultFromUserRequest(algoResult, req, srcSocket, srcNuma);
    }

    UbseResult ShareMemoryBorrowAccordingToUserRequest(UbseMemShareBorrowReq& req,
                                                       ubse::adapter_plugins::mmi::UbseMemAlgoResult& algoResult,
                                                       uint64_t checkMaskCode);

    void AddSocketCnaSize(const std::string& importNodeIdSocketId, const std::string& exportNodeIdSocketId,
                          uint64_t AddCnaSize);

    void SubSocketCnaSize(const std::string& importNodeIdSocketId, const std::string& exportNodeIdSocketId,
                          uint64_t subCnaSize);

    void GetSocketCnaSize(const std::string& importNodeIdSocketId,
                          std::unordered_map<std::string, uint64_t>& socketCnaSize);

    void AddBorrowDebt(const std::string& importNodeId, const std::string& exportNodeId);
    void SubBorrowDebt(const std::string& importNodeId, const std::string& exportNodeId);

    const std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>>& GetBorrowDebt() const
    {
        return borrowDebt_;
    }

    /* importNodeId: borrower, exportNodeId: lender */
    void AddLenderDebt(const std::string &importNodeId, const std::string &exportNodeId);
    void SubLenderDebt(const std::string &importNodeId, const std::string &exportNodeId);

    const std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>>& GetLenderDebt() const
    {
        return lenderDebt_;
    }

    // 更新debt
    UbseResult GetNumaDebtInfoFromNumaPair(const GlobalNumaIndex& brwNumaGlobalIdx,
                                           const ubse::adapter_plugins::mmi::UbseMemDebtNumaInfo& lend, bool add);

    inline tc::rs::mem::DebtDetail GetDebtDetail()
    {
        return debtDetail_;
    }

    void Clear();

private:
    static void Log(int level, const char* msg)
    {
        switch (level) {
            case static_cast<int>(tc::rs::mem::LogLevel::DEBUG):
                UBSE_LOG_DEBUG << msg;
                break;
            case static_cast<int>(tc::rs::mem::LogLevel::INFO):
                UBSE_LOG_INFO << msg;
                break;
            case static_cast<int>(tc::rs::mem::LogLevel::WARN):
                UBSE_LOG_WARN << msg;
                break;
            case static_cast<int>(tc::rs::mem::LogLevel::ERROR):
                UBSE_LOG_ERROR << msg;
                break;
            default:
                UBSE_LOG_INFO << msg;
        }
    }
    UbseMemStrategyHelper() = default;
    tc::rs::mem::DebtDetail debtDetail_{};
    std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> borrowDebt_{};
    std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> lenderDebt_{};
    std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> socketCnaSizeCount_{};
};
#undef MODULE_LOG_NAME
} // namespace ubse::mem::strategy

#endif