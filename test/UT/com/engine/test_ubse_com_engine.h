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

#ifndef TEST_UBSE_COM_ENGINE_H
#define TEST_UBSE_COM_ENGINE_H

#include "engine/ubse_com_engine.h"
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

namespace ubse::ut::com {
using namespace ubse::com;
class TestUbseComEngine : public testing::Test {
public:
    TestUbseComEngine() = default;

    void SetUp() override;

    void TearDown() override;

    UBSHcomService* mockService;
};

class TestUbseComLinkManager : public testing::Test {
public:
    TestUbseComLinkManager() = default;

    void SetUp() override;

    void TearDown() override;

    UbseComLinkManager manager;
};

class TestUbseComEngineManager : public testing::Test {
public:
    TestUbseComEngineManager() = default;

    void SetUp() override;

    void TearDown() override;

    UbseComEngineManager manager;
};

class TestUbseCommunication : public testing::Test {
public:
    TestUbseCommunication() = default;

    void SetUp() override;

    void TearDown() override;

    UbseCommunication manager;
};
} // namespace ubse::ut::com
#endif // TEST_UBSE_COM_ENGINE_H
