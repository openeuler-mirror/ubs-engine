/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#ifndef UBSE_MANAGER_TEST_LIBVIRT_MODULE_H
#define UBSE_MANAGER_TEST_LIBVIRT_MODULE_H

#include "gtest/gtest.h"

namespace ubse::ut::vm {

class TestLibvirtModule : public testing::Test {
public:
    TestLibvirtModule() = default;

    void SetUp() override;

    void TearDown() override;
};
} // namespace ubse::ut::vm

#endif // UBSE_MANAGER_TEST_LIBVIRT_MODULE_H