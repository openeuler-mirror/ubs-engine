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

#ifndef UBSE_MANAGER_UBSE_CONF_COMMON_DEF_H
#define UBSE_MANAGER_UBSE_CONF_COMMON_DEF_H

namespace ubse::config {
constexpr int CONFIG_MAX_LINES = 1000;
constexpr int CONFIG_SECTION_MAX_FIELD_LENGTH = 32;
constexpr int CONFIG_KEY_MAX_FIELD_LENGTH = 32;
constexpr int CONFIG_VALUE_MAX_FIELD_LENGTH = 255;
constexpr int CONFIG_MIN_FIELD_LENGTH = 1;
constexpr int CONFIG_DIR_MAX_DEPTH = 10;
} // namespace ubse::config

#endif // UBSE_MANAGER_UBSE_CONF_COMMON_DEF_H
