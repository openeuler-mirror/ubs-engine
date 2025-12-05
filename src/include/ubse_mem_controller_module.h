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

#ifndef UBSE_MEM_CONTROLLER_MODULE_H
#define UBSE_MEM_CONTROLLER_MODULE_H

#include "ubse_context.h" // for context
#include "ubse_error.h"
#include "ubse_timer.h"

namespace ubse::mem::controller {
using namespace ubse::context;

class UbseMemControllerModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

private:
    ubse::timer::UbseTimer handleCheckTimer{}; // handle对账定时器
};

} // namespace ubse::mem::controller

#endif // UBSE_MEM_CONTROLLER_MODULE_H
