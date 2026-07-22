/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef UBSE_MANAGER_UBSE_UBSE_SERIAL_UTIL_H
#define UBSE_MANAGER_UBSE_UBSE_SERIAL_UTIL_H

#include "ubse_serial_util.h"
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

namespace ubse::ut::serial {
using namespace ubse::serial;
class TestUbseSerialUtil : public testing::Test {
public:
    TestUbseSerialUtil() = default;

    void SetUp() override;

    void TearDown() override;
};
} // namespace ubse::ut::serial
#endif // UBSE_MANAGER_UBSE_UBSE_SERIAL_UTIL_H