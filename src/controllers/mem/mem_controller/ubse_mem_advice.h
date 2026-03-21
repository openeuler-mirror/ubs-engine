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

#ifndef UBS_ENGINE_UBSE_MEM_ADVICE_H
#define UBS_ENGINE_UBSE_MEM_ADVICE_H

#include <string>
#include <cstdint>

namespace ubse::mem::controller {
enum class MemAdvice {
    INTERNAL_FAILED = 0,
    CHECK_FAILED = 1,
    COMM_FAILED = 2,
    SCHEDULE_FAILED = 3,
    OBMM_FAILED = 4,
    RESOURCE_EXIST = 5,
    TIME_OUT = 6,
    NODE_IN_MAITENANCE = 7,
    RESOURCE_NOT_EXIST = 8,
    RESOURCE_OPERATION_CONFLICT = 9,
    UBSE_NO_OPERATION_PERMISSION = 10,
};

void BorrowFailedAdvice(std::string prefix, std::string name, std::string borrowType, size_t size,
                        std::string exportNode, std::string importNode, uint32_t errorCode, MemAdvice advice);
} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_ADVICE_H
