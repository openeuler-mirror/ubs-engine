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

#include "mock_module.h"

#include "ubse_context.h"
#include "mock_dependency_module.h"

namespace ubse::mock {
using namespace ubse::context;
DYNAMIC_CREATE(MockModule, MockDependencyModule);
UbseResult MockModule::Initialize()
{
    return 0;
}
void MockModule::UnInitialize() {}
UbseResult MockModule::Start()
{
    return 0;
}
void MockModule::Stop() {}
} // namespace ubse::mock