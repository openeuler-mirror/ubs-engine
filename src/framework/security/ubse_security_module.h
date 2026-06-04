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

#ifndef UBSE_SECURITY_MODULE_H
#define UBSE_SECURITY_MODULE_H

#include <vector>
#include <linux/capability.h>
#include "ubse_common_def.h"
#include "ubse_module.h"
#include "ubse_security_manager.h"

namespace ubse::security {
using namespace ubse::common::def;
using ubse::module::UbseModule;

class UbseSecurityModule final : public UbseModule {
public:
    static constexpr const char* kModuleName = "UbseSecurityModule";
    std::string Name() const override { return kModuleName; }
    ~UbseSecurityModule() override = default;

    UbseResult Initialize() override;
    void UnInitialize() override;
    UbseResult Start() override;
    void Stop() override;
    static UbseResult ModifyEffectiveCapabilities(std::vector<__u32> &caps, bool isAdd);
};

}

#endif // UBSE_SECURITY_MODULE_H
