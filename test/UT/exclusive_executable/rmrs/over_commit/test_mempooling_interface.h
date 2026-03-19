/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef TEST_MEMPOOLING_INTERFACE_H
#define TEST_MEMPOOLING_INTERFACE_H

#include <gtest/gtest.h>

namespace mempooling::ut::over_commit {

class TestMempoolingInterface : public testing::Test {
public:
    TestMempoolingInterface() = default;
    void SetUp() override;
    void TearDown() override;
};

} // namespace mempooling::ut::over_commit

#endif // TEST_MEMPOOLING_INTERFACE_H
