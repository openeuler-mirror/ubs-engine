/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#ifndef UBSE_MANAGER_TEST_HAM_MIGRATE_VM_INFO_STORAGE_H
#define UBSE_MANAGER_TEST_HAM_MIGRATE_VM_INFO_STORAGE_H

#include "gtest/gtest.h"

using namespace vm;
namespace ubse::ut::vm {
class TestHamMigrateVmInfoStorage : public testing::Test {
public:
    TestHamMigrateVmInfoStorage() = default;

    void SetUp() override;

    void TearDown() override;
};
}

#endif // UBSE_MANAGER_TEST_HAM_MIGRATE_VM_INFO_STORAGE_H