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

#ifndef UBS_ENGINE_TEST_UBSE_SSU_SCHEDULER_H
#define UBS_ENGINE_TEST_UBSE_SSU_SCHEDULER_H

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include <memory>
#include <string>
#include <vector>

#include "ubse_ssu_def.h"
#include "ubse_ssu_scheduler.h"

namespace ubse::ssu::scheduler::ut {

class TestUbseSsuScheduler : public testing::Test {
public:
    TestUbseSsuScheduler() = default;

    void SetUp() override;

    void TearDown() override;

protected:
    // 构造一个在线的 SSU 设备
    static ubse::adapter_plugins::ssu::def::UbseSsuDevInfo MakeDev(
        const std::string &eid, uint64_t totalBytes, uint64_t usedBytes, uint32_t nsCount = 0,
        ubse::adapter_plugins::ssu::def::UbseSsuState state = ubse::adapter_plugins::ssu::def::UbseSsuState::ONLINE);
};

class CustomFilter : public UbseFilter<UbseSsuAllocationContext> {
public:
    bool Handle(UbseSsuAllocationContext &ctx) override
    {
        ctx.result.msg += " | CUSTOM";
        return true;
    }
};
} // namespace ubse::ssu::scheduler::ut

#endif // UBS_ENGINE_TEST_UBSE_SSU_SCHEDULER_H
