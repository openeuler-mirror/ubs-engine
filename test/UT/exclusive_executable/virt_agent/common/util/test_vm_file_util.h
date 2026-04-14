/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef UBSE_MANAGER_TEST_VM_FILE_UTIL_H
#define UBSE_MANAGER_TEST_VM_FILE_UTIL_H

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "vm_file_util.h"

namespace ubse::ut::vm {
class TestVmFileUtil : public testing::Test {
public:
    TestVmFileUtil() = default;

    void SetUp() override;

    void TearDown() override;
};
}

#endif // UBSE_MANAGER_TEST_VM_FILE_UTIL_H
