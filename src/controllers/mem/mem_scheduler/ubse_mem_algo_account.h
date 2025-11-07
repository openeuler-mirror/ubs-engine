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

#ifndef UBSE_MEM_ALGO_ACCOUNT_H
#define UBSE_MEM_ALGO_ACCOUNT_H

#include <cstdint>
#include <shared_mutex>
#include <vector>

#include <memory>
#include "ubse_common_def.h"
#include "ubse_mem_obj.h"
#include "ubse_mem_resource.h"

namespace ubse::mem::strategy {
using namespace ubse::common::def;
struct AlgoAccount {
    std::string name{};
    // 借入借出关系
    std::vector<ubse::mem::obj::UbseMemDebtNumaInfo> importNumaLocs{};
    std::vector<ubse::mem::obj::UbseMemDebtNumaInfo> exportNumaLocs{};
    // 实际的物理连线socketId
    int64_t attachSocketId{};
    int64_t exportSocketId{};
    uint64_t totalBorrowSize{}; // 记录借用大小

    AlgoAccount() = default;
    AlgoAccount(const std::string &name, const std::vector<ubse::mem::obj::UbseMemDebtNumaInfo> &importNumaLocs,
                const std::vector<ubse::mem::obj::UbseMemDebtNumaInfo> &exportNumaLocs, const int64_t &attachSocketId,
                const int64_t &exportSocketId, const uint64_t &totalBorrowSize)
    {
        this->name = name;
        this->importNumaLocs = importNumaLocs;
        this->exportNumaLocs = exportNumaLocs;
        this->attachSocketId = attachSocketId;
        this->exportSocketId = exportSocketId;
        this->totalBorrowSize = totalBorrowSize;
    }
};

class AlgoAccountManger {
public:
    static AlgoAccountManger &GetInstance()
    {
        static AlgoAccountManger instance;
        return instance;
    }
    AlgoAccountManger(const AlgoAccountManger &other) = delete;
    AlgoAccountManger(AlgoAccountManger &&other) = delete;
    AlgoAccountManger &operator=(const AlgoAccountManger &other) = delete;
    AlgoAccountManger &operator=(AlgoAccountManger &&other) noexcept = delete;
    void AddAlgoAccount(const std::shared_ptr<AlgoAccount> algoAccount);
    void DelAlgoAccount(const std::string &name);
    UbseResult AddAlgoAccountByLcneTopo(const std::string &algoAccount, const std::string &exportSocketId);
    std::shared_ptr<AlgoAccount> GetAlgoAccount(const std::string &name);
    void ClearAlgoAccount();
    std::vector<std::shared_ptr<AlgoAccount>> GetAllAlgoAccount();
    std::vector<std::shared_ptr<AlgoAccount>> GetAllAlgoAccountByNode(const std::string &nodeId);

private:
    AlgoAccountManger() = default;
    std::unordered_map<std::string, std::shared_ptr<AlgoAccount>> algoAccountMap{};
    std::shared_mutex mLock;
};
} // namespace ubse::mem::strategy

#endif