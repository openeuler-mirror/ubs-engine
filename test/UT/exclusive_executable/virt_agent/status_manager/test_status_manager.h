/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UBSE_MANAGER_TEST_STATUS_MANAGER_H
#define UBSE_MANAGER_TEST_STATUS_MANAGER_H

#include "gtest/gtest.h"

namespace ubse::ut::vm {
class TestStatusManager : public testing::Test {
public:
    TestStatusManager() = default;

    void SetUp() override;

    void TearDown() override;
};
} // namespace ubse::ut::vm
#endif // UBSE_MANAGER_TEST_STATUS_MANAGER_H
