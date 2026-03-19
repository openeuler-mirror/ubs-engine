/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef UBS_ENGINE_TEST_UBSE_NODE_CONTROLLER_AGENT_H
#define UBS_ENGINE_TEST_UBSE_NODE_CONTROLLER_AGENT_H

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "ubse_node_controller_agent.h"

namespace ubse::node_controller::ut {
using namespace ubse::nodeController;

class TestUbseNodeControllerAgent : public testing::Test {
public:
    TestUbseNodeControllerAgent() = default;

    void SetUp() override;

    void TearDown() override;
};
} // namespace ubse::node_controller::ut

#endif // UBS_ENGINE_TEST_UBSE_NODE_CONTROLLER_AGENT_H
