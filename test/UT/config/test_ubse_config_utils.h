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

#ifndef UBSE_MANAGER_TEST_UBSE_CONFIG_UTILS_H
#define UBSE_MANAGER_TEST_UBSE_CONFIG_UTILS_H

#include "ubse_common_def.h"
namespace ubse::ut::config {
using namespace ubse::common::def;
std::string RandomString(size_t length = NO_4);
uint32_t RandomNumber(uint32_t min = NO_0, uint32_t max = NO_3);
} // namespace ubse::ut::config
#endif // UBSE_MANAGER_TEST_UBSE_CONFIG_UTILS_H
