// Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

#ifndef RACK_MANAGER_TEST_MXE_RAS_OOM_HANDLER_H
#define RACK_MANAGER_TEST_MXE_RAS_OOM_HANDLER_H
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

namespace ubse::ras::ut {
class TestUbseRasOomHandler : public testing::Test {
public:
    TestUbseRasOomHandler() = default;

    void SetUp() override;

    void TearDown() override;
};
} // namespace ubse::ras::ut
#endif // RACK_MANAGER_TEST_MXE_RAS_OOM_HANDLER_H
