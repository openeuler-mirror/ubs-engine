/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of the Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_SSU_RPC_PROCESSOR_H
#define UBSE_SSU_RPC_PROCESSOR_H

#include <cstdint>

namespace ubse::ssu::controller {

class UbseSsuRpcProcessor {
public:
    static uint32_t RegHandler();

private:
    static uint32_t RegisterAllocHandlers();
    static uint32_t RegisterStatusHandler();
    static uint32_t RegisterFreeHandlers();
    static uint32_t RegisterAddPermHandlers();
    static uint32_t RegisterRemovePermHandlers();
};
} // namespace ubse::ssu::controller

#endif // UBSE_SSU_RPC_PROCESSOR_H
