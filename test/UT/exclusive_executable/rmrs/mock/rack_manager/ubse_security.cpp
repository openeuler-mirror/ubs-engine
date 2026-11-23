/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <linux/capability.h>
#include <cstdint>
#include <vector>
#include "src/framework/security/ubse_security_module.h"

namespace ubse::security {
// mock 内部桩：生产代码已无包装函数，仅供本 UT 通过 MOCKER 观察守卫开启/收起序列。
uint32_t ChangeOverrideCapability(bool isAdd)
{
    return 0;
}

uint32_t ChangePtraceCapability(bool isAdd)
{
    return 0;
}

// CapabilityGuard 桩实现：按能力位转发到对应的提权封装，
// 便于 UT 通过 MOCKER(ChangeOverrideCapability/ChangePtraceCapability) 观察开启/收起序列。
CapabilityGuard::CapabilityGuard(const std::vector<__u32>& caps) : caps_(caps)
{
    for (auto cap : caps_) {
        uint32_t ret = 0;
        if (cap == CAP_DAC_OVERRIDE) {
            ret = ChangeOverrideCapability(true);
        } else if (cap == CAP_SYS_PTRACE) {
            ret = ChangePtraceCapability(true);
        }
        if (ret != 0) {
            enabled_ = false;
            return;
        }
    }
    enabled_ = true;
}

CapabilityGuard::~CapabilityGuard()
{
    Disable();
}

void CapabilityGuard::Disable()
{
    if (!enabled_) {
        return;
    }
    for (auto cap : caps_) {
        uint32_t ret = 0;
        if (cap == CAP_DAC_OVERRIDE) {
            ret = ChangeOverrideCapability(false);
        } else if (cap == CAP_SYS_PTRACE) {
            ret = ChangePtraceCapability(false);
        }
        if (ret != 0) {
            // 与生产实现一致：收起失败保持 enabled_，析构阶段再重试一次
            return;
        }
    }
    enabled_ = false;
}

bool CapabilityGuard::IsEnabled() const
{
    return enabled_;
}
} // namespace ubse::security
