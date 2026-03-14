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

#ifndef UBS_VIRT_AGENT_CASE_CONF_H
#define UBS_VIRT_AGENT_CASE_CONF_H

#include "ubs_virt_agent_base.h"
#include "case_conf_msg.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace vm;

/**
 * @brief  get current case configuration information
 * @param case_conf_info [OUT] case configuration information returned by the interface
 * @return 0 for success, non-zero for error
 */
virt_agent_ret_t ubs_virt_agent_case_conf_get(case_conf_info_t *case_conf_info);

/**
 * @brief  set case configuration information
 * @param param                 [IN]  configuration parameters in string format
 * @param case_conf_set_info    [OUT] result of the case configuration set operation
 * @return 0 for success, non-zero for error
 */
virt_agent_ret_t ubs_virt_agent_case_conf_set(const char *param, case_conf_set_info_t *case_conf_set_info);

#ifdef __cplusplus
}
#endif

#endif // UBS_VIRT_AGENT_CASE_CONF_H
