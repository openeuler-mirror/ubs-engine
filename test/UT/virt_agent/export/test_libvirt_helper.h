/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2024. All rights reserved.
 */
#ifndef UBSE_MANAGER_TEST_LIBVIRT_HELPER_H
#define UBSE_MANAGER_TEST_LIBVIRT_HELPER_H
#include "gtest/gtest.h"
#include "libvirt_module.h"
namespace ubse::ut::vm {
using namespace ::vm::libvirt;
class TestLibvirtHelper : public testing::Test {
public:
    TestLibvirtHelper() = default;

    void SetUp() override;

    void TearDown() override;
};
}
#endif // UBSE_MANAGER_TEST_LIBVIRT_HELPER_H