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
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "ubse_com_base.h"
#include "ubse_com_module.h"
#include "ubse_context.h"
using namespace ubse::com;

namespace ubse::ut::com {
class TestUbseCom : public testing::Test {
public:
    TestUbseCom() = default;
    void SetUp() override;
    void TearDown() override;
private:
    uint8_t *testTransData;
    UbseByteBuffer TEST_BUFFER;
};

class MockUbseServer : public UbseComBase {
public:
    MockUbseServer(const std::string &nodeId, const std::string &name) : UbseComBase(nodeId, name) {}

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

class MockUbseRpcMessage : public UbseRpcMessage {
public:
    uint32_t Serialize(std::unique_ptr<uint8_t[]> &buffer, uint32_t &bufferSize) const override
    {
        if (serializeRet == UBSE_OK) {
            bufferSize = 4;
            buffer = std::make_unique<uint8_t[]>(bufferSize);
            memset(buffer.get(), 0, bufferSize);
        } else {
            bufferSize = 0;
        }
        return serializeRet;
    }

    uint32_t Deserialize(const uint8_t *data, uint32_t size) override
    {
        return deserializeRet;
    }

    uint32_t serializeRet = UBSE_OK;
    uint32_t deserializeRet = UBSE_OK;
};

class MockUbseRpcAsyncCallBack : public UbseRpcAsyncCallBack {
public:
    uint32_t CallBackFunc(const uint8_t *respData, uint32_t respSize, int32_t retCode) override
    {
        return UBSE_OK;
    }
};
}

#endif // TEST_UBSE_COM_H
