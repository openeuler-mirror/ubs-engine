/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#ifndef TEST_HUGEPAGE_HANDLER_H
#define TEST_HUGEPAGE_HANDLER_H

#include "gtest/gtest.h"
namespace ubse::ut::vm {
class TestHugepageHandler : public testing::Test {
public:
    TestHugepageHandler() = default;

    void SetUp() override;

    void TearDown() override;
};
}

#endif // TEST_HUGEPAGE_HANDLER_H
