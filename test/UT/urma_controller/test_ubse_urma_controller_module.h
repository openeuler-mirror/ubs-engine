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

#ifndef TEST_UBSE_URMA_CONTROLLER_MODULE_H
#define TEST_UBSE_URMA_CONTROLLER_MODULE_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <mockcpp/mokc.h>
#include "ubse_urma_controller_module.h"
#include "ubse_context.h"

namespace ubse::urmaControllerModule::ut {

class TestUbseUrmaControllerModule : public testing::Test {
protected:
    void SetUp() override
    {
        Test::SetUp();
        ubse::context::g_globalStop = false;
    }

    void TearDown() override
    {
        Test::TearDown();
        ubse::context::g_globalStop = false;
        GlobalMockObject::verify();
    }
};

} // namespace ubse::urmaControllerModule::ut

#endif // TEST_UBSE_URMA_CONTROLLER_MODULE_H
