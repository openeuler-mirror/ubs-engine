/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 *
 * virtagent is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBS_VIRT_AGENT_CASE_CONF_HELPER_H
#define UBS_VIRT_AGENT_CASE_CONF_HELPER_H

#include "ubs_virt_agent_case_conf.h"

using namespace vm;
typedef struct {
    const uint8_t *ptr;
    size_t remaining;
} unpack_ctx_t;

virt_agent_ret_t ubse_case_conf_info_unpack(uint8_t *buffer, uint32_t len, case_conf_info_t *case_conf_info);

virt_agent_ret_t ubse_case_conf_set_unpack(uint8_t *buffer, uint32_t len, case_conf_set_info_t *case_conf_info);

#endif // UBS_VIRT_AGENT_CASE_CONF_HELPER_H
