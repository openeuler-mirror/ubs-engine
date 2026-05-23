/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef UBS_ENGINE_TEST_UBSE_MEM_DEBT_INFO_QUERY_NUMA_H
#define UBS_ENGINE_TEST_UBSE_MEM_DEBT_INFO_QUERY_NUMA_H

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "debt/ubse_mem_debt_info_query.h"

namespace ubse::mem_controller::ut {

class TestUbseMemDebtInfoQueryNuma : public testing::Test {
public:
    TestUbseMemDebtInfoQueryNuma() = default;

    void SetUp() override;

    void TearDown() override;
};

} // namespace ubse::mem_controller::ut
#endif // UBS_ENGINE_TEST_UBSE_MEM_DEBT_INFO_QUERY_NUMA_H