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

#include "test_process_mem_plugin.h"

#include "ubse_error.h"

namespace ubse::ut::process_mem {

void TestProcessMemPlugin::SetUp() {}

void TestProcessMemPlugin::TearDown() {}

TEST_F(TestProcessMemPlugin, UbsePluginInitFailNoLibrary)
{
    uint32_t ret = UbsePluginInit(123);
    EXPECT_NE(ret, UBSE_OK);
}

TEST_F(TestProcessMemPlugin, UbsePluginDeInit)
{
    EXPECT_NO_THROW(UbsePluginDeInit());
}
} // namespace ubse::ut::process_mem
