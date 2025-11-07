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

#ifndef UBSE_MEM_ACCOUNT_HELPER_H
#define UBSE_MEM_ACCOUNT_HELPER_H
#include "ubse_common_def.h"

#include <shared_mutex>

#include "ubse_mem_resource.h"
#include "ubse_mem_algo_account.h"
#include "ubse_mem_obj.h"

namespace ubse::mem::strategy {
using namespace ubse::mem::obj;
using namespace ubse::resource::mem;
using namespace ubse::common::def;

class UbseMemAccountHelper {
public:
    static UbseMemAccountHelper &GetInstance()
    {
        static UbseMemAccountHelper instance;
        return instance;
    }
    UbseMemAccountHelper(const UbseMemAccountHelper &other) = delete;
    UbseMemAccountHelper(UbseMemAccountHelper &&other) = delete;
    UbseMemAccountHelper &operator=(const UbseMemAccountHelper &other) = delete;
    UbseMemAccountHelper &operator=(UbseMemAccountHelper &&other) noexcept = delete;
    // 根据不同情况 处理账本和统计关系的更新

    // 清理预占 在export成功之后 或者借用失败途中 通过numa位置把预占清理
    void DeleteStrategyNumaLendInfo(const AlgoAccount &algoAccount); // 传递算法结果 传递name 自己去查，缓存了算法结果
    void DeleteStrategyNumaSharedInfo(const AlgoAccount &algoAccount);

    // 增加对应numa的统计信息，即实际的占用情况 在export 和 import 成功之后需要 通过numa位置 增加numa位置
    void AddNumaLentInfo(const AlgoAccount &algoAccount);

    void AddNumaBorrowInfo(const AlgoAccount &algoAccount);

    void AddNumaSharedInfo(const AlgoAccount &algoAccount);

    void DelNumaLentInfo(const AlgoAccount &algoAccount);

    void DelNumaBorrowInfo(const AlgoAccount &algoAccount);

    void DelNumaSharedInfo(const AlgoAccount &algoAccount);

    // 更新算法参数 borrowDetail
    UbseResult UpdateBorrowDebtDetail(const AlgoAccount &algoAccount, const bool &isAdd);

    // 借用成功 清理预占 增加实际占用
    void BorrowSuccess(const std::string &name);

    void ShareSuccess(const std::string &name);

    // 归还成功 清理时机占用和cna占用
    void BorrowReturnSuccess(const std::shared_ptr<AlgoAccount> accountPtr);

    // 借用失败 清理预占
    void BorrowFailed(const std::string &name);

    // 归还成功 清理实际占用的内存；

private:
    UbseMemAccountHelper() = default;
    std::shared_mutex mLock;
};
} // namespace ubse::mem::strategy
#endif