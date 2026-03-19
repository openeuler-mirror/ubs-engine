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

#ifndef TURBO_RUNTIME_MANAGER_H
#define TURBO_RUNTIME_MANAGER_H
#include "turbo_def.h"
#include "turbo_ucache_interface.h"

#include <cstdint>

namespace turbo::ucache {

using UBTurboUCacheExecuteTaskPtr = uint32_t (*)(const TaskRequest &tReq, TaskResponse &tResp);

class TurboRuntimeManager {
public:
    static uint32_t Init();
    static void Deinit();
    static uint32_t DlsymUcacheInterface();
    static uint32_t InitOSTurboIpcClient();

    static void *osturboClientHandle;
    static UBTurboUCacheExecuteTaskPtr ucacheExecuteTask;
};
}  // namespace turbo::ucache
#endif