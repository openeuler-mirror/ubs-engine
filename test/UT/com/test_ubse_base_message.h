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

#ifndef TEST_UBSE_BASE_MESSAGE_H
#define TEST_UBSE_BASE_MESSAGE_H

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include "ubse_base_message.h"
#include "ubse_error.h"

namespace ubse::ut::message {
using namespace ubse::message;
using namespace ubse::common::def;
class TestUbseBaseMessage : public testing::Test {
public:
    TestUbseBaseMessage() = default;

    void SetUp() override;

    void TearDown() override;
};

class MockUbseMessage : public UbseBaseMessage {
public:
    UbseResult Serialize() override
    {
        return UBSE_OK;
    }

    UbseResult Deserialize() override
    {
        return UBSE_OK;
    }
};
} // namespace ubse::ut::message

#endif // TEST_UBSE_BASE_MESSAGE_H
