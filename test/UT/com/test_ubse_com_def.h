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

#ifndef TEST_UBSE_COM_DEF_H
#define TEST_UBSE_COM_DEF_H

#include "ubse_com_def.h"
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

using namespace ubse::com;
using namespace ubse::message;

namespace ubse::ut::com {
class TestUbseComDef : public testing::Test {
public:
    TestUbseComDef() = default;

    void SetUp() override;

    void TearDown() override;
};

class TestRpcMessage : public UbseBaseMessage {
public:
    TestRpcMessage(uint32_t SerializeRet, uint32_t DeserializeRet)
        : SerializeRet(SerializeRet),
          DeserializeRet(DeserializeRet){};

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    uint32_t SerializeRet;
    uint32_t DeserializeRet;
};
} // namespace ubse::ut::com
#endif // TEST_UBSE_COM_DEF_H
