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
#ifndef UBSE_MANAGER_PLUGIN_IT_H
#define UBSE_MANAGER_PLUGIN_IT_H

#include "main_test_it.h"
namespace ubse::it::plugin {
using namespace ubse::it;
int32_t ITestCmdPluginAddPlugin(ProcessMmap* pMmap);
int32_t ITestCmdPluginAddAdmission(ProcessMmap* pMmap);
int32_t ITestCmdPluginDeleteAdmission(ProcessMmap* pMmap);
int32_t ITestCmdPluginReloadPlugin(ProcessMmap* pMmap);
} // namespace ubse::it::plugin

#endif // UBSE_MANAGER_PLUGIN_IT_H
