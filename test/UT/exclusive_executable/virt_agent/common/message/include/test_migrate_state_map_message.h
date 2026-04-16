/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef UBSE_MANAGER_TEST_MIGRATE_STATE_MAP_MESSAGE_H
#define UBSE_MANAGER_TEST_MIGRATE_STATE_MAP_MESSAGE_H

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "migrate_state_map_message.h"

namespace ubse::ut::vm {
class TestMigrateStateMapMessage : public testing::Test {
public:
    TestMigrateStateMapMessage() = default;

    void SetUp() override;

    void TearDown() override;
};
}
#endif // UBSE_MANAGER_TEST_MIGRATE_STATE_MAP_MESSAGE_H
