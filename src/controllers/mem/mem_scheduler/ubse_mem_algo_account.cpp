/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#include "ubse_mem_algo_account.h"

#include <mutex>

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_node_controller.h"
#include "ubse_node_topology.h"

namespace ubse::mem::strategy {
UBSE_DEFINE_THIS_MODULE("ubse", UBSE_CONTROLLER_MID)
using namespace ubse::log;
using namespace ubse::nodeController;

void AlgoAccountManger::AddAlgoAccount(const std::shared_ptr<AlgoAccount> algoAccountPtr)
{
    std::unique_lock<std::shared_mutex> locker(mLock);
    if (algoAccountPtr == nullptr) {
        UBSE_LOG_WARN << "add algoaccount is nullptr";
        return;
    }
    algoAccountMap.insert({algoAccountPtr->name, algoAccountPtr});
}

void AlgoAccountManger::DelAlgoAccount(const std::string &name)
{
    std::unique_lock<std::shared_mutex> locker(mLock);
    auto iterator = algoAccountMap.find(name);
    if (iterator != algoAccountMap.end()) {
        algoAccountMap.erase(iterator);
    }
}

void AlgoAccountManger::ClearAlgoAccount()
{
    std::unique_lock<std::shared_mutex> locker(mLock);
    algoAccountMap.clear();
}

std::shared_ptr<AlgoAccount> AlgoAccountManger::GetAlgoAccount(const std::string &name)
{
    std::shared_lock<std::shared_mutex> locker(mLock);
    if (algoAccountMap.find(name) != algoAccountMap.end()) {
        auto algoAccountPtr = algoAccountMap[name];
        return algoAccountPtr;
    }
    return nullptr;
}

std::vector<std::shared_ptr<AlgoAccount>> AlgoAccountManger::GetAllAlgoAccount()
{
    std::shared_lock<std::shared_mutex> locker(mLock);
    std::vector<std::shared_ptr<AlgoAccount>> algoAccountPtrs{};
    for (auto [key, value] : algoAccountMap) {
        algoAccountPtrs.push_back(value);
    }
    return algoAccountPtrs;
}

std::vector<std::shared_ptr<AlgoAccount>> AlgoAccountManger::GetAllAlgoAccountByNode(const std::string &nodeId)
{
    std::shared_lock<std::shared_mutex> locker(mLock);
    std::vector<std::shared_ptr<AlgoAccount>> algoAccountPtrs{};
    for (auto [key, value] : algoAccountMap) {
        if (value->importNumaLocs.empty() || value->exportNumaLocs.empty()) {
            continue;
        }
        if (value->importNumaLocs[0].nodeId == nodeId) {
            algoAccountPtrs.push_back(value);
        }
        if (value->exportNumaLocs[0].nodeId == nodeId) {
            algoAccountPtrs.push_back(value);
        }
    }
    return algoAccountPtrs;
}

UbseResult AlgoAccountManger::AddAlgoAccountByLcneTopo(const std::string &name, const std::string &exportSocketId)
{
    auto algoAccountPtr = GetAlgoAccount(name);
    if (algoAccountPtr == nullptr || algoAccountPtr->importNumaLocs.size() < 1 ||
        algoAccountPtr->exportNumaLocs.size() < 1) {
        UBSE_LOG_ERROR << "Get Algo Account Failed";
        return UBSE_ERROR_NULLPTR;
    }
    UbseNodeMemCnaInfoInput cnaInput{};
    cnaInput.borrowNodeId = algoAccountPtr->importNumaLocs[0].nodeId;
    cnaInput.exportNodeId = algoAccountPtr->exportNumaLocs[0].nodeId;
    cnaInput.exportSocketId = exportSocketId;
    UbseNodeMemCnaInfoOutput cnaOutput{};
    auto ret = UbseNodeMemGetTopologyCnaInfo(cnaInput, cnaOutput);
    if (ret != 0) {
        UBSE_LOG_ERROR << "failed to get cna, ubse manager return=" << ret << ", exportNodeId=" << cnaInput.exportNodeId
                     << ", exportSocketId=" << cnaInput.exportSocketId << ", importNodeId=" << cnaInput.borrowNodeId;
        return UBSE_ERROR;
    }
    try {
        algoAccountPtr->exportSocketId = std::stoi(exportSocketId);
        algoAccountPtr->attachSocketId = std::stoi(cnaOutput.borrowSocketId);
    } catch (...) {
        UBSE_LOG_ERROR << "Get algoAccount socketId failed";
        return UBSE_ERROR;
    }

    return UBSE_OK;
}
} // namespace ubse::mem::strategy