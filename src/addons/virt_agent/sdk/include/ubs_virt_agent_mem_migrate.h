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
#ifndef UBS_VIRT_AGENT_MEM_MIGRATE_H
#define UBS_VIRT_AGENT_MEM_MIGRATE_H

#include "ubs_virt_agent_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief update page flow and status
 * @param opt [IN] "true"、"false"、"none_migrating"、"none_migrate_success"、"none_migrate_failed"
 * @param uuid [IN] vm uuid
 * @return 0 for success, non-zero for error
 */
int32_t update_page_flow_and_status(const char* opt, const char* uuid);

#ifdef __cplusplus
}
#endif

#endif // UBS_VIRT_AGENT_MEM_MIGRATE_H
