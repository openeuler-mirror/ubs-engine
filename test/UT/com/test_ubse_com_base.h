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

#ifndef TEST_UBSE_COM_BASE_H
#define TEST_UBSE_COM_BASE_H

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "ubse_com_base.h"
#include "ubse_com_def.h"
#include "ubse_error.h"
#include "ubse_context.h"
#include "ubse_event_module.h"

using namespace ubse::com;

namespace ubse::ut::com {
class TestUbseComBase : public testing::Test {
public:
    TestUbseComBase() = default;

    void SetUp() override;

    void TearDown() override;
};

class TestComBaseMessage : public UbseBaseMessage {
public:
    TestComBaseMessage() = default;

    TestComBaseMessage(uint32_t SerializeRet, uint32_t DeserializeRet)
        : SerializeRet(SerializeRet),
          DeserializeRet(DeserializeRet){};

    UbseResult Serialize() override;

    UbseResult Deserialize() override;

private:
    uint32_t SerializeRet;
    uint32_t DeserializeRet;
};

using TestBaseMessagePtr = Ref<TestComBaseMessage>;

class TestUbseComBaseMessage : public UbseComBaseMessageHandler {
public:
    explicit TestUbseComBaseMessage(uint32_t handRet) : handRet(handRet){};

    UbseResult Handle(const UbseBaseMessage &req, const UbseBaseMessage &rsp);

    uint16_t GetOpCode();

    uint16_t GetModuleCode();

private:
    uint32_t handRet;
};

class MockUbseComBase : public UbseComBase {
public:
    MockUbseComBase(const std::string &nodeId, const std::string &name) : UbseComBase(nodeId, name) {}

    UbseResult RegNewChannelCb(UbseComCallBackForHA func) override
    {
        return UBSE_OK;
    }

    UbseResult RegBrokenChannelCb(UbseComCallBackForHA func) override
    {
        return UBSE_OK;
    }

    UbseResult Start() override
    {
        return UBSE_OK;
    };

    UbseResult Connect() override
    {
        return UBSE_OK;
    }
    void Stop() override {}
};
} // namespace ubse::ut::com

#endif // TEST_UBSE_COM_BASE_H
