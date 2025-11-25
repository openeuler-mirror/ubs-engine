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
#ifndef UBS_ENGINE_TEST_UBSE_MEM_DEBINFO_QUERY_HANDLER_H_H
#define UBS_ENGINE_TEST_UBSE_MEM_DEBINFO_QUERY_HANDLER_H_H
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "rpc/UbseMemDebInfoQueryHandler.h"

namespace ubse::mem_controller::ut {
using namespace ubse::mem_controller;

class TestUbseMemDebInfoQueryHandler : public testing::Test {
public:
    TestUbseMemDebInfoQueryHandler() = default;

    void SetUp() override;

    void TearDown() override;
};
} // namespace ubse::mem_controller::ut
#endif // UBS_ENGINE_TEST_UBSE_MEM_DEBINFO_QUERY_HANDLER_H_H
