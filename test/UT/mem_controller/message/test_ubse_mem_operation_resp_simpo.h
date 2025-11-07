/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

#ifndef UBSE_MANAGER_TEST_MXE_MEM_OPERATION_RESP_SIMPO_H
#define UBSE_MANAGER_TEST_MXE_MEM_OPERATION_RESP_SIMPO_H

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "message/ubse_mem_operation_resp_simpo.h"

namespace ubse::mem::controller::message::ut {
class TestUbseMemOperationRespSimpo : public testing::Test {
public:
    void SetUp() override;

    void TearDown() override;

private:
    UbseMemOperationRespSimpoPtr obj;
};
} // namespace ubse::mem::controller::message::ut

#endif // UBSE_MANAGER_TEST_MXE_MEM_OPERATION_RESP_SIMPO_H
