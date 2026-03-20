/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef UBSE_MANAGER_TEST_GLOBAL_BORROW_MAP_MESSAGE_H
#define UBSE_MANAGER_TEST_GLOBAL_BORROW_MAP_MESSAGE_H

#include "gtest/gtest.h"

namespace ubse::ut::vm {
class TestGlobalBorrowMapMessage : public testing::Test {
public:
    TestGlobalBorrowMapMessage() = default;

    void SetUp() override;

    void TearDown() override;
};
}
#endif // UBSE_MANAGER_TEST_GLOBAL_BORROW_MAP_MESSAGE_H
