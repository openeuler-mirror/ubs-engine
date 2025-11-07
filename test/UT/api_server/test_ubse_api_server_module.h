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

#ifndef TEST_UBSE_API_SERVER_MODULE_H
#define TEST_UBSE_API_SERVER_MODULE_H

class test_ubse_api_server {};
#include <gtest/gtest.h>

#include "ubse_api_server_module.h"

namespace ubse::ut::api::server {
using namespace ::api::server;
class TestUbseApiServerModule : public testing::Test {
public:
    TestUbseApiServerModule();

    virtual void SetUp();

    virtual void TearDown();

private:
    UbseApiServerModule apiServerModule{};
};
} // namespace ubse::ut::api::server
#endif // TEST_UBSE_API_SERVER_MODULE_H
