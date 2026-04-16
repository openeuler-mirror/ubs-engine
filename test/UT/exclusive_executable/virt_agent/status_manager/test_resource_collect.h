/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */

#ifndef UBS_ENGINE_TEST_RESOURCE_COLLECT_H
#define UBS_ENGINE_TEST_RESOURCE_COLLECT_H

#include "gtest/gtest.h"

namespace ubse::vm::ut {
class TestResourceCollect : public testing::Test {
public:
    TestResourceCollect() = default;

    void SetUp() override;

    void TearDown() override;
};
}  // namespace ubse::vm::ut

#endif  // UBS_ENGINE_TEST_RESOURCE_COLLECT_H
