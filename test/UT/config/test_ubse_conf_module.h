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

#ifndef TEST_UBSE_CONFIG_MODULE_H
#define TEST_UBSE_CONFIG_MODULE_H

#include <gtest/gtest.h>
#include "ubse_conf_manager.h"
#include "ubse_conf_module.h"

using namespace ubse::config;

namespace ubse::ut::config {
class TestUbseConfModule : public testing::Test {
public:
    TestUbseConfModule() = default;

    void SetUp();

    void TearDown();

    static void SetUpTestSuite();
    static void TearDownTestSuite();
    static std::shared_ptr<UbseConfModule> confModulePtr;
};
} // namespace ubse::ut::config
#endif // TEST_UBSE_CONFIG_MODULE_H
