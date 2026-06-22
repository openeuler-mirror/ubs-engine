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

#ifndef IT_TEST_FIXTURE_H
#define IT_TEST_FIXTURE_H

#include <filesystem>
#include <string>

#include "config.h"
#include "gtest/gtest.h"
#include "it_console_log.h"

namespace ubse::it::infra {

class ItTestFixture : public testing::Test {
protected:
    static void SetUpTestSuite()
    {
        workDir_ = "/tmp/ubs_engine_it_" + std::to_string(::getpid());
        binaryPath_ = std::filesystem::path(BUILD_DIRECTORY) / "bin" / "ubse_it_daemon";
        stubLibDir_ = std::filesystem::path(BUILD_DIRECTORY) / "lib";
    }

    static void TearDownTestSuite()
    {
        if (!workDir_.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(workDir_, ec);
            if (ec) {
                IT_LOG_WARN << "Failed to clean up work directory " << workDir_ << ": " << ec.message();
            }
        }
    }

    static std::string workDir_;
    static std::filesystem::path binaryPath_;
    static std::filesystem::path stubLibDir_;
};

inline std::string ItTestFixture::workDir_;
inline std::filesystem::path ItTestFixture::binaryPath_;
inline std::filesystem::path ItTestFixture::stubLibDir_;

} // namespace ubse::it::infra

#endif // IT_TEST_FIXTURE_H