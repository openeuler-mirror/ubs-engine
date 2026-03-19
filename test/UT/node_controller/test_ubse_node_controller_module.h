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

#ifndef UBS_ENGINE_TEST_UBSE_NODE_CONTROLLER_MODULE_H
#define UBS_ENGINE_TEST_UBSE_NODE_CONTROLLER_MODULE_H

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include "ubse_node_controller_module.h"

namespace ubse::node_controller::ut {
using namespace ubse::nodeController;
class TestUbseNodeControllerModule : public testing::Test {
public:
    TestUbseNodeControllerModule() = default;

    void SetUp() override;

    void TearDown() override;
};
} // namespace ubse::node_controller::ut

#endif // UBS_ENGINE_TEST_UBSE_NODE_CONTROLLER_MODULE_H
