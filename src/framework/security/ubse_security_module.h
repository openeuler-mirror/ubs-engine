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

#include <linux/capability.h>
#include <vector>
#include "ubse_common_def.h"
#include "ubse_module.h"
#include "ubse_security_manager.h"

namespace ubse::security {
using ubse::common::def::UbseResult;
using ubse::module::UbseModule;

class UbseSecurityModule final : public UbseModule {
public:
    ~UbseSecurityModule() override = default;

    UbseResult Initialize() override;
    void UnInitialize() override;
    UbseResult Start() override;
    void Stop() override;
    static UbseResult ModifyEffectiveCapabilities(std::vector<__u32>& caps, bool isAdd);
};

/*
 * 能力位的 RAII 守卫：构造时把能力加入有效集，析构时移出。
 * 即使中间调用的外部库抛异常，权限也会在栈展开时被收起，不会一直留在有效集里。
 * 需要提前收起时（比如只想包住一次调用）显式调用 Disable。
 */
class CapabilityGuard final {
public:
    explicit CapabilityGuard(const std::vector<__u32>& caps);
    ~CapabilityGuard();

    CapabilityGuard(const CapabilityGuard&) = delete;
    CapabilityGuard& operator=(const CapabilityGuard&) = delete;
    CapabilityGuard(CapabilityGuard&&) = delete;
    CapabilityGuard& operator=(CapabilityGuard&&) = delete;

    void Disable();
    [[nodiscard]] bool IsEnabled() const;

private:
    std::vector<__u32> caps_;
    bool enabled_{false};
};

} // namespace ubse::security

#endif // UBSE_SECURITY_MODULE_H
