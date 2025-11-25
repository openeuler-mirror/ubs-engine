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

#ifndef UBSE_MANAGER_TEST_UBSE_LOGGER_ENTRY_H
#define UBSE_MANAGER_TEST_UBSE_LOGGER_ENTRY_H
#include <iostream>
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"
#include "securec.h"

#include "ubse_logger.cpp"
#include "ubse_logger.h"
#include "ubse_logger_manager.h"

namespace ubse::ut::log {
class TestUbseLogger : public testing::Test {
public:
    TestUbseLogger();
    virtual void SetUp(void);
    virtual void TearDown(void);
};
}
#endif // UBSE_MANAGER_TEST_UBSE_LOGGER_ENTRY_H
