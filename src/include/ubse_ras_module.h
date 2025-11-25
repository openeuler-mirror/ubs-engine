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

#ifndef UBSE_MANAGER_UBSE_RAS_MODULE_H
#define UBSE_MANAGER_UBSE_RAS_MODULE_H
#include "ubse_module.h"
#include "src/res_plugins/syssentry/sentry_observer.h"

namespace ubse::ras {
using namespace ubse::module;

class UbseRasModule : public UbseModule {
public:
    UbseResult Initialize() override;

    void UnInitialize() override;

    UbseResult Start() override;

    void Stop() override;

    ~UbseRasModule();
};
} // namespace ubse::ras
#endif // UBSE_MANAGER_UBSE_RAS_MODULE_H
