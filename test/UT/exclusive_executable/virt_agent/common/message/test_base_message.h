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

#ifndef TEST_BASE_MESSAGE_H
#define TEST_BASE_MESSAGE_H
#include "base_message.h"
#include "gtest/gtest.h"
using namespace vm;
namespace ubse::ut::vm {

class TestBaseMessage : public testing::Test {
public:
    TestBaseMessage() = default;

    void SetUp() override;

    void TearDown() override;
};

class TestBaseMessageClass : public BaseMessage {
    TestBaseMessageClass() = default;

    VmResult Serialize() override;

    VmResult Deserialize() override;
};
} // namespace ubse::ut::vm
#endif // TEST_BASE_MESSAGE_H
