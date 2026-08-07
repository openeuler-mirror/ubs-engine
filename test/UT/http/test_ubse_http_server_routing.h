/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#ifndef TEST_UBSE_HTTP_SERVER_ROUTING_H
#define TEST_UBSE_HTTP_SERVER_ROUTING_H

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

namespace ubse::ut::http {
class TestUbseHttpServerRouting : public testing::Test {
public:
    TestUbseHttpServerRouting() = default;

    void SetUp();

    void TearDown();
};
} // namespace ubse::ut::http

#endif // TEST_UBSE_HTTP_SERVER_ROUTING_H
