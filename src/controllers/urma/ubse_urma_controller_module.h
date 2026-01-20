/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBSE_URMA_CONTROLLER_MODULE_H
#define UBSE_URMA_CONTROLLER_MODULE_H

#include <atomic>

#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_module.h"

namespace ubse::urmaController {

using namespace ubse::log;
using namespace ubse::module;

class AsyncHandlerGuard {
public:
    AsyncHandlerGuard(std::atomic<uint32_t> &cnt) : guard_cnt(cnt)
    {
        guard_cnt.fetch_add(1, std::memory_order_relaxed);
    }

    ~AsyncHandlerGuard()
    {
        guard_cnt.fetch_sub(1, std::memory_order_relaxed);
    }

    AsyncHandlerGuard(const AsyncHandlerGuard &) = delete;
    AsyncHandlerGuard &operator=(const AsyncHandlerGuard &) = delete;

private:
    std::atomic<uint32_t> &guard_cnt;
};

class UbseUrmaControllerModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;
};

} // namespace ubse::urmaController

#endif // UBSE_URMA_CONTROLLER_MODULE_H