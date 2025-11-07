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

#ifndef UBSE_TEST_UBSE_LOG_SINK_H
#define UBSE_TEST_UBSE_LOG_SINK_H

#include "gtest/gtest.h"
#include "ubse_logger.h"
#include "ubse_logger_filesink.h"

namespace ubse::ut::log {
using namespace ubse::log;
const std::string FILE_PATH = "/log/run/";
const std::string FILE_NAME = "test_log";
class TestUbseLoggerFileSink : public testing::Test {
public:
    TestUbseLoggerFileSink() = default;

    void SetUp() override;

    void TearDown() override;

private:
    std::string currentPath;
};
}

#endif // UBSE_TEST_UBSE_LOG_H