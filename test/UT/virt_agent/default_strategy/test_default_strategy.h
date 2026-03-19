/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#ifndef TEST_DEFAULT_STRATEGY_H
#define TEST_DEFAULT_STRATEGY_H

#include "gtest/gtest.h"

namespace ubse::ut::df {
class TestDefaultStrategy : public testing::Test {
public:
    TestDefaultStrategy();

    void SetUp() override;

    void TearDown() override;
};
}

#endif // TEST_DEFAULT_STRATEGY_H
