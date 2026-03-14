// Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.

#ifndef TEST_ALARM_HANDLER_H
#define TEST_ALARM_HANDLER_H
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

namespace ubse::vm::ut {
class TestAlarmHandler : public testing::Test {
public:
    TestAlarmHandler();

    void SetUp();

    void TearDown();
};
}
#endif // TEST_ALARM_HANDLER_H
