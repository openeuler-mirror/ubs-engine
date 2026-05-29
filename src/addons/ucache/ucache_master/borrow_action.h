/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UCACHE_BORROW_ACTION_H
#define UCACHE_BORROW_ACTION_H

#include "ubse_error.h"
#include "mem_borrow.h"

namespace ucache {
namespace borrow_action {

using namespace ucache::mem_borrow;

enum class ActionType
{
    BORROW,
    RETURN
};

struct BorrowAction {
    ActionType type;
    struct NodeMemBorrowInfo nodeMemBorrowInfo;

    std::string ToString() const;
};

template <size_t N>
UbseResult UbseMemNumaCreateWithLenderSafe(const std::string& name, const UbseMemBorrower& borrower,
                                           const std::vector<UbseMemNumaLender>& lenders, uint8_t (&usrInfo)[N],
                                           UbseMemNumaDesc& desc)
{
    static_assert(N == UBSE_MAX_USR_INFO_LEN,
                  "Error: usrInfo buffer size does not meet UbseMemNumaCreateWithLender requirements!");

    return UbseMemNumaCreateWithLender(name, borrower, lenders, usrInfo, desc);
}

// 借用/归还内存相关接口

// 借用内存
uint32_t ExecuteBorrowMem(const std::string& from, const std::string& to);

// 归还内存
uint32_t ExecuteReturnMem(const std::string& from, const std::string& to);

// 执行单个借用动作
uint32_t ProcessOneBorrowAction(const BorrowAction& action);

// 执行借用动作集
uint32_t ExecuteBorrowActions(const std::vector<BorrowAction>& actionSet);

} // namespace borrow_action
} //  end namespace ucache
#endif
