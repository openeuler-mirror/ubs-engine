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

#include "mem_pool_strategy.h"
#include "mem_pool_strategy_impl.h"

namespace tc::rs::mem {
MemPoolStrategy& MemPoolStrategy::GetInstance()
{
    static MemPoolStrategyImpl impl;
    return impl;
}

BResult MemPoolStrategy::Init(const StrategyParam& param)
{
    return UBSE_ERROR;
}

BResult MemPoolStrategy::MemoryBorrow(const BorrowRequest& borrowRequest, const UbseStatus& ubseStatus,
                                      BorrowResult& result)
{
    return UBSE_ERROR;
}

BResult MemPoolStrategy::MemoryShare(const ShareRequest& shareRequest, const UbseStatus& ubseStatus,
                                     ShareResult& result)
{
    return UBSE_ERROR;
}

} // namespace tc::rs::mem
