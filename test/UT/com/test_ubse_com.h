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

#ifndef TEST_UBSE_COM_H
#define TEST_UBSE_COM_H
#include "ubse_com_base.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
using namespace ubse::com;

namespace ubse::ut::com {
class TestUbseCom : public testing::Test {
public:
    TestUbseCom() = default;
    void SetUp() override;
    void TearDown() override;

private:
    uint8_t* testTransData;
    UbseByteBuffer TEST_BUFFER;
};

class MockUbseServer : public UbseComBase {
public:
    MockUbseServer(const std::string& nodeId, const std::string& name) : UbseComBase(nodeId, name) {}

    UbseResult Start() override
    {
        return UBSE_OK;
    }

    UbseResult Connect() override
    {
        return UBSE_OK;
    }
    void Stop() override {}
};

class MockUbseBaseMessage : public UbseBaseMessage {
public:
    UbseResult Deserialize() override
    {
        return UBSE_OK;
    }

    UbseResult Serialize() override
    {
        return UBSE_OK;
    }
    ~MockUbseBaseMessage() override {}
};
} // namespace ubse::ut::com

#endif // TEST_UBSE_COM_H
