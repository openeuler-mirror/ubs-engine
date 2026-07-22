/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef UBSE_MANAGER_TEST_RESPONSE_INFO_MESSAGE_H
#define UBSE_MANAGER_TEST_RESPONSE_INFO_MESSAGE_H

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "response_info_message.h"

namespace ubse::ut::vm {
class TestResponseInfoMessage : public testing::Test {
public:
    TestResponseInfoMessage() = default;

    void SetUp() override;

    void TearDown() override;
};
} // namespace ubse::ut::vm
#endif // UBSE_MANAGER_TEST_RESPONSE_INFO_MESSAGE_H
