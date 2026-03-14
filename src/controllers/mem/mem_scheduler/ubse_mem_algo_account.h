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
#ifndef MXE_MEM_ALGO_ACCOUNT_H
#define MXE_MEM_ALGO_ACCOUNT_H
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <vector>

#include "ubse_common_def.h"
#include "ubse_mmi_interface.h"
namespace ubse::mem::strategy {
using namespace ubse::common::def;
enum class AccountState {
    IMPORT_EXPORT_EXIST,
    ONLY_EXPORT_EXIST,
    ONLY_IMPORT_EXIST,
    BOTH_NOT_EXIST,
};

enum class BorrowedType {
    FD,   // Fd借用
    NUMA, // Numa借用
    ADDR, // 指定地址借用
    SHM,  // 共享内存借用
};

struct AlgoAccountID {
    std::string requestName{};
    std::string nodeId{};
    BorrowedType type;

    bool operator < (const AlgoAccountID &other) const
    {
        if (requestName != other.requestName) {
            return requestName < other.requestName;
        }
        if (nodeId != other.nodeId) {
            return nodeId < other.nodeId;
        }
        return type < other.type;
    }
};

class BaseAlgoAccount {
public:
    BaseAlgoAccount() = default;
    virtual void UpdateAlgoAccountState(ubse::adapter_plugins::mmi::UbseMemState memState,
                                        const ubse::adapter_plugins::mmi::UbseMemAlgoResult &algoResult) = 0;

    virtual AccountState GetAccountState() = 0;

    virtual ubse::adapter_plugins::mmi::UbseMemAlgoResult GetAlgoResult() = 0;

public:
    std::string name{};
    BorrowedType type{};
    std::string exportNodeId{};
    std::string importNodeId{};
};

template <class T>
class AccountImpl : public BaseAlgoAccount {
public:
    template <class... Ts>
    explicit AccountImpl(Ts &&...ts)  : account_{std::forward<Ts>(ts)...}
    {
    }

    void UpdateAlgoAccountState(ubse::adapter_plugins::mmi::UbseMemState memState,
                                const ubse::adapter_plugins::mmi::UbseMemAlgoResult &algoResult) override
    {
        account_.UpdateAlgoAccountState(memState, algoResult);
    }

    AccountState GetAccountState() override
    {
        return account_.GetAccountState();
    }

    ubse::adapter_plugins::mmi::UbseMemAlgoResult GetAlgoResult() override
    {
        return account_.GetAlgoResult();
    }

private:
    T account_;
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

    void AddAlgoAccount(const std::shared_ptr<BaseAlgoAccount>& algoAccountPtr);

    std::shared_ptr<BaseAlgoAccount> GetAlgoAccount(const AlgoAccountID &algoAccountID);

    std::vector<std::shared_ptr<BaseAlgoAccount>> GetAllAlgoAccount();

    std::vector<std::shared_ptr<BaseAlgoAccount>> GetAllAlgoAccountByNode(const std::string &nodeId);

    std::shared_ptr<BaseAlgoAccount> CreateAccountByAlgoResult(
        const std::string &name, const ubse::adapter_plugins::mmi::UbseMemAlgoResult &algoResult, BorrowedType type);

    bool CheckProviderNodeHasBorrowed(const std::string &providerNodeId);

    bool CheckBorrowNodeHasLent(const std::string &borrowNodeId);

    void UpdateAlgoAccountState(const std::string &name, ubse::adapter_plugins::mmi::UbseMemState state,
                                const ubse::adapter_plugins::mmi::UbseMemAlgoResult &algoResult, BorrowedType type);

    void Clear();

private:
    std::map<AlgoAccountID, std::shared_ptr<BaseAlgoAccount>> algoAccountMap_{};

private:
    AlgoAccountManger() = default;
};

} // namespace ubse::mem::strategy

#endif