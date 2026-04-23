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

#ifndef TEST_UBSE_COM_MODULE_H
#define TEST_UBSE_COM_MODULE_H

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_str_util.h"

namespace ubse::ut::com {
using namespace ubse::com;
using namespace ubse::context;
class TestUbseComModule : public testing::Test {
public:
    TestUbseComModule() = default;

    void SetUp() override;

    void TearDown() override;

protected:
    std::shared_ptr<UbseComModule> ubseComModulePtr;
};
} // namespace ubse::ut::com

#endif // TEST_UBSE_COM_MODULE_H
