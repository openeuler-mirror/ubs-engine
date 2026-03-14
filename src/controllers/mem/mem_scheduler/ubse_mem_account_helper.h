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
#ifndef MXE_MEM_ACCOUNT_HELPER_H
#define MXE_MEM_ACCOUNT_HELPER_H
#include "ubse_common_def.h"

#include <shared_mutex>

#include "ubse_mem_algo_account.h"
#include "ubse_mmi_interface.h"

namespace ubse::mem::strategy {
using namespace ubse::adapter_plugins::mmi;
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

    // 更新借用缓存的状态
    static void UpdateAlgoAccountState(const std::string &name, UbseMemState state, const UbseMemAlgoResult &algoResult,
        BorrowedType type);

private:
    UbseMemAccountHelper() = default;
};
} // namespace ubse::mem::strategy
#endif