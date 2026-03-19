/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#ifndef TEST_MEM_SCHEDULER_H
#define TEST_MEM_SCHEDULER_H
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
namespace ubse::mem::mem_scheduler::ut {
class TestMemConfiguration : public testing::Test {
public:
    void SetUp() override;

    void TearDown() override;
};
} // namespace ubse::mem::mem_scheduler::ut
#endif