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

#ifndef TEST_PROCESS_MEM_PID_MANAGER_DEF_H
#define TEST_PROCESS_MEM_PID_MANAGER_DEF_H

#include "gtest/gtest.h"
#include "process_mem_pid_manager_def.h"

namespace ubse::ut::process_mem {

class TestProcessMemPidManagerDef : public testing::Test {
public:
    TestProcessMemPidManagerDef() = default;

    void SetUp() override;
    void TearDown() override;
};
} // namespace ubse::ut::process_mem
#endif
