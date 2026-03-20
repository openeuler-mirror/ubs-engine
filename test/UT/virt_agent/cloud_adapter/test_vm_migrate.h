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

#ifndef VM_TEST_VM_MIGRATE_H
#define VM_TEST_VM_MIGRATE_H
#include "gtest/gtest.h"
#include "vm_migrate.h"

namespace ubse::ut::vm {
class TestVmMigrate : public testing::Test {
public:
    TestVmMigrate() = default;

    void SetUp() override;

    void TearDown() override;
};
}
#endif // VM_TEST_VM_MIGRATE_H
