/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UBSE_MANAGER_TEST_VM_CONFIGURATION_H
#define UBSE_MANAGER_TEST_VM_CONFIGURATION_H
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

namespace ubse::vm::ut {
class TestVmConfiguration : public testing::Test {
public:
    TestVmConfiguration();

    void SetUp();

    void TearDown();
};
} // namespace ubse::vm::ut
#endif // UBSE_MANAGER_TEST_VM_CONFIGURATION_H
