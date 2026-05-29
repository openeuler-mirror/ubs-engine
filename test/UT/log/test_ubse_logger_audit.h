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

#ifndef UBSE_TEST_UBSE_LOGGER_AUDIT_H
#define UBSE_TEST_UBSE_LOGGER_AUDIT_H
#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

#include "ubse_logger_audit.h"
// audit
namespace ubse::ut::log {
class TestUbseLoggerAudit : public testing::Test {
public:
    TestUbseLoggerAudit();
    virtual void SetUp(void);
    virtual void TearDown(void);
};
} // namespace ubse::ut::log

#endif // UBSE_TEST_UBSE_LOGGER_AUDIT_H
