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

#ifndef UBSE_MANAGER_TEST_UBSE_CONTEXT_H
#define UBSE_MANAGER_TEST_UBSE_CONTEXT_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ubse_context.h"

namespace ubse::ut::context {
using namespace ubse::context;
using namespace ubse::module;
class MockUbseModule : public UbseModule {
public:
    MOCK_METHOD0(Initialize, UbseResult());
    MOCK_METHOD0(Start, UbseResult());
    MOCK_METHOD0(RegArgs, void());
    MOCK_METHOD0(Stop, void());
    MOCK_METHOD0(UnInitialize, void());
};

class MockUbseModuleD : public UbseModule {
public:
    MOCK_METHOD0(Initialize, UbseResult());
    MOCK_METHOD0(Start, UbseResult());
    MOCK_METHOD0(RegArgs, void());
    MOCK_METHOD0(Stop, void());
    MOCK_METHOD0(UnInitialize, void());
};

class TestUbseContext : public testing::Test {
public:
    TestUbseContext() = default;

    void SetUp() override;

    void TearDown() override;

private:
    UbseContext &context = UbseContext::GetInstance();
};
}
#endif // UBSE_MANAGER_TEST_UBSE_CONTEXT_H
