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

#ifndef UCACHE_ERROR_H
#define UCACHE_ERROR_H

#include <cstdint>
namespace ucache {
const uint32_t UCACHE_OK = 0;
const uint32_t UCACHE_ERR = 1;
const uint32_t UCACHE_ERR_MSG_LEN = 2;

// 借用策略错误码
const uint32_t BORROW_TOPO_ERROR = 0x1001;
const uint32_t BORROW_DATA_ERROR = 0x1002;
const uint32_t SCARCITY_INVALID_INPUT = 0x1003;
const uint32_t BORROW_SCARCITY_SHOCK = 0x1004;
const uint32_t BORROW_SCARCITY_LIMIT = 0x1005;

// 借用策略执行模块错误码
const uint32_t NO_RETURNABLE_MEM = 0x2001;
const uint32_t NO_BORROWABLE_MEM = 0x2002;
const uint32_t DEL_MEM_ERROR = 0x2003;
const uint32_t EXEC_MEM_BORROW_ERROR = 0x2004;
const uint32_t EXEC_MEM_RETURN_ERROR = 0x2005;
const uint32_t MEM_ACTION_TYPE_ERROR = 0x2006;

// 任务执行错误码
const uint32_t EXECUTE_DEFAULT_ERROR = 0x4001;
const uint32_t INVALID_TASK_TYPE = 0x4002;
const uint32_t MEM_COPY_ERROR = 0x4003;
const uint32_t UCACHE_ERROR_NOMEM = 0x4004;
const uint32_t TURBO_EXECUTE_ERROR = 0x4005;
const uint32_t INVALID_BUFFER = 0x4006;
const uint32_t EMPTY_BUFFER = 0x4007;
// UBSE接口执行错误
const uint32_t UBSE_API_ERROR = 0x3001;

// 字符串转换错误码
const uint32_t EMPTY_STRING = 0x5001;
const uint32_t INVALID_ARGUMENT = 0x5002;
const uint32_t OUT_OF_RANGE = 0x5003;

const uint32_t OPCODE_EXECUTE_TASK = 13;
} // namespace ucache

#endif /* UCACHE_ERROR_H */