/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
  *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBS_ENGINE_TEST_UBSE_MEM_ASYNC_PROCESSOR_H
#define UBS_ENGINE_TEST_UBSE_MEM_ASYNC_PROCESSOR_H

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "ubse_mem_async_processor.h"

namespace ubse::mem_controller::ut {
using namespace ubse::mem_controller;
class TestUbseMemAsyncProcessor : public testing::Test {
public:
    TestUbseMemAsyncProcessor() = default;

    void SetUp() override;

    void TearDown() override;
};
}
#endif // UBS_ENGINE_TEST_UBSE_MEM_ASYNC_PROCESSOR_H
