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

#include <cstdint>
#include <string>

namespace ubse::mem::controller {

enum class MemFault : uint8_t
{
    UNKNOWN = 0,
    BORROW_CHECK_FAILED = 1,
    BORROW_NAME_EXIST = 2,
    BORROW_CHIP_NOT_SUPPORTED = 3,
    BORROW_SCHEDULE_FAILED = 4,
    BORROW_MASTER_TO_EX_SEND_FAILED = 5,
    BORROW_MASTER_TO_IM_SEND_FAILED = 6,
    BORROW_MASTER_TO_REQ_SEND_FAILED = 7,
    BORROW_EXPORT_SEND_FAILED = 8,
    BORROW_IMPORT_SEND_FAILED = 9,
    BORROW_REQ_SEND_FAILED = 10,
    BORROW_DECODE_FAILED = 11,
    BORROW_IMPORT_IN_MAINTENANCE = 12,
    BORROW_OBMM_EXPORT_FAILED = 13,
    BORROW_OBMM_IMPORT_FAILED = 14,
    BORROW_TIME_OUT = 15,
    SHARED_BORROW_CHECK_FAILED = 16,
    SHARED_ATTACH_CHECK_FAILED = 17,
    SHARED_ATTACH_AUTH_FAILED = 18,
    SHARED_ATTACH_EXIST = 19,
    SHARED_CHIP_NOT_SUPPORTED = 20,
    SHARED_CHIP_MODE_NOT_SUPPORTED = 21,
    RETURN_NAME_NOT_EXIST = 22,
    RETURN_CHIP_NOT_SUPPORTED = 23,
    RETURN_MASTER_TO_EX_SEND_FAILED = 24,
    RETURN_MASTER_TO_IM_SEND_FAILED = 25,
    RETURN_MASTER_TO_REQ_SEND_FAILED = 26,
    RETURN_EXPORT_SEND_FAILED = 27,
    RETURN_IMPORT_SEND_FAILED = 28,
    RETURN_REQ_SEND_FAILED = 29,
    RETURN_REQ_CONFLICT = 30,
    RETURN_AUTH_FAILED = 31,
    RETURN_IMPORT_IN_MAINTENANCE = 32,
    RETURN_EXPORT_IN_MAINTENANCE = 33,
    RETURN_OBMM_EXPORT_FAILED = 34,
    RETURN_OBMM_IMPORT_FAILED = 35,
    RETURN_TIME_OUT = 36,
    SHARED_RETURN_IN_ATTACH = 37,
    SHARED_RETURN_REG_FAILED = 38,
    SHARED_DETACH_AUTH_FAILED = 39,
    SHARED_DETACH_REQ_CONFLICT = 40,
    SHARED_DETACH_NOT_EXIST = 41,
    SHARED_RETURN_CHIP_NOT_SUPPORTED = 42,

    // Fault
    BORROW_FAULT_INTERNAL = 248,
    BORROW_FAULT_IMPORT_INTERNAL = 249,
    BORROW_FAULT_EXPORT_INTERNAL = 250,
    SHARED_FAULT_ATTACH_INTERNAL = 251,
    RETURN_FAULT_INTERNAL = 252,
    RETURN_FAULT_IMPORT_INTERNAL = 253,
    RETURN_FAULT_EXPORT_INTERNAL = 254,
    SHARED_FAULT_DETACH_INTERNAL = 255,
};

enum class MemType : uint8_t
{
    FD = 1,
    NUMA = 2,
    SHM = 3,
    ADDR = 4,
};

struct BorrowFailedAdviceCtx {
    MemFault faultCode{MemFault::UNKNOWN}; // 内存借用故障码，必填
    std::string name;                      // 内存名称，必填
    MemType borrowType;                    // 内存类型，必填
    size_t size{0};                        // 内存大小，默认0
    std::string exportNode;                // 导出节点ID，默认空
    std::string importNode;                // 导入节点ID，默认空
    std::string requestNode;               // 请求节点ID，必填
};

void BorrowFailedAdvice(const BorrowFailedAdviceCtx& ctx);
} // namespace ubse::mem::controller

#endif // UBS_ENGINE_UBSE_MEM_ADVICE_H
