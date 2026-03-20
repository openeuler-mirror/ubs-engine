/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef MP_DEFAULT_STRUCT_H
#define MP_DEFAULT_STRUCT_H
#include <bits/types.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <mutex>
#include <ostream>
#include <sstream>
#include <algorithm>

using uint8_t = __uint8_t;
using uint16_t = __uint16_t;
using uint32_t = __uint32_t;
using uint64_t = __uint64_t;
using time_t = __time_t;
using DsResult = uint32_t;
const uint32_t DS_OK = 0;                         /* 正确 */
const uint32_t DS_ERROR = 1;                      /* 错误 */
const uint32_t DS_ERROR_NULLPTR = 2;              /* 空指针 */
const uint32_t DS_ERROR_NULLPTR_EMPTY_VECTOR = 3; /* 空数组 */
const uint32_t DS_ERROR_INVAL = 4;                /* Invalid argument */

const uint32_t DS_ERR_LOG_INIT = 5;        /* 策略日志初始化错误 */
const uint32_t DS_ERR_SET_INVAL = 6;       /* 策略参数设置错误 */

constexpr int BYTE2GB = 30;
constexpr uint64_t SPECS_1G = static_cast<uint64_t>(1) << BYTE2GB;

#endif // MP_DEFAULT_STRUCT_H
