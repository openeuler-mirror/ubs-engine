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

#ifndef UBSE_SECURITY_MANAGER_H
#define UBSE_SECURITY_MANAGER_H

#include <vector>

#include <linux/capability.h>

#include "ubse_common_def.h"

namespace ubse::security {
using namespace ubse::common::def;

enum class UbseCapOperateType {
    CAP_ADD,
    CAP_DELETE,
};

class UbseSecurityManager {
public:
    static UbseResult GetCapabilities();
    static UbseResult SetInitialCapabilities();
    static UbseResult ModifyEffectiveCapabilities(std::vector<__u32> caps, UbseCapOperateType opType);
};
} // namespace ubse::security

#endif // UBSE_SECURITY_MANAGER_H
