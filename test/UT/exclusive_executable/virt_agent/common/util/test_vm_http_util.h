/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#ifndef UBSE_MANAGER_TEST_VM_HTTP_UTIL_H
#define UBSE_MANAGER_TEST_VM_HTTP_UTIL_H

#include "../../test_common.h"
#include "gtest/gtest.h"

namespace ubse::ut::vm {
class TestVmHttpUtil : public testing::Test {
public:
    TestVmHttpUtil() = default;

    void SetUp() override;

    void TearDown() override;
};
}

#endif // UBSE_MANAGER_TEST_VM_HTTP_UTIL_H
