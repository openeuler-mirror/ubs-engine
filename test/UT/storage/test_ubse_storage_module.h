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

#ifndef UBSE_MANAGER_TEST_UBSE_STORAGE_MODULE_H
#define UBSE_MANAGER_TEST_UBSE_STORAGE_MODULE_H

#include <gtest/gtest.h>
#include "ubse_storage_module.h"

namespace ubse::ut::storage {
using namespace ubse::storage;

class TestUbseStorageModule : public testing::Test {
public:
    TestUbseStorageModule() = default;

    void SetUp() override;

    void TearDown() override;

public:
    inline static std::shared_ptr<ubse::storage::UbseStorageModule> module = std::make_shared<UbseStorageModule>();
};
} // namespace ubse::ut::storage

#endif // UBSE_MANAGER_TEST_UBSE_STORAGE_MODULE_H
