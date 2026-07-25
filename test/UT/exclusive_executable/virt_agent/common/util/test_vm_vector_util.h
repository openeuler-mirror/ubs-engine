/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef UBSE_MANAGER_TEST_VM_VECTOR_UTIL_H
#define UBSE_MANAGER_TEST_VM_VECTOR_UTIL_H

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "vm_vector_util.h"

namespace ubse::ut::vm {
class TestVmVectorUtil : public testing::Test {
public:
    TestVmVectorUtil() = default;
    std::vector<uint16_t> source;
    std::vector<uint16_t> targets;

    void SetUp() override;

    void TearDown() override;
};
} // namespace ubse::ut::vm

#endif // UBSE_MANAGER_TEST_VM_VECTOR_UTIL_H
