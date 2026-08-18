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

#ifndef TEST_PROCESS_MEM_PID_DECISION_H
#define TEST_PROCESS_MEM_PID_DECISION_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "mock/ubse/mock_control.h"
#include "mock/ubse/ubse_smap_mock.h"
#include "process_mem_pid_bridge.h"
#include "process_mem_pid_info_manager.h"

#define private public
#include "process_mem_pid_decision.h"
#undef private

namespace ubse::ut::process_mem {

class TestProcessMemPidDecision : public testing::Test {
public:
    TestProcessMemPidDecision() = default;

    void SetUp() override;
    void TearDown() override;
};
} // namespace ubse::ut::process_mem
#endif
