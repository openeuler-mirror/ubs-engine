/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef TEST_MEMPOOLING_MODULE_H
#define TEST_MEMPOOLING_MODULE_H

#include "gtest/gtest.h"
namespace ubse::ut::vm {

class TestMempoolingModule : public testing::Test {
public:
    TestMempoolingModule() = default;

    void SetUp() override;

    void TearDown() override;
};
} // namespace ubse::ut::vm

#endif // TEST_MEMPOOLING_MODULE_H
