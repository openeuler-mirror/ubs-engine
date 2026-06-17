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

#ifndef TEST_UBSE_MTI_UTIL_H
#define TEST_UBSE_MTI_UTIL_H

#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include "src/adapter_plugins/mti/out_of_band/ubse_mti_util.h"

namespace ubse::ut::mti {
class TestUbseMtiUtil : public ::testing::Test {
public:
    TestUbseMtiUtil() = default;

    void SetUp() override;

    void TearDown() override;
};
} // namespace ubse::ut::mti

#endif // TEST_UBSE_MTI_UTIL_H
