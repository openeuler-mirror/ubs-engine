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

#ifndef UBSE_MANAGER_TEST_UBSE_PLUGIN_ADMISSION_H
#define UBSE_MANAGER_TEST_UBSE_PLUGIN_ADMISSION_H

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

namespace ubse::ut::plugin {
class TestUbsePluginAdmission : public testing::Test {
public:
    TestUbsePluginAdmission() = default;

    void SetUp() override;

    void TearDown() override;
};
} // namespace ubse::ut::plugin

#endif // UBSE_MANAGER_TEST_UBSE_PLUGIN_ADMISSION_H
