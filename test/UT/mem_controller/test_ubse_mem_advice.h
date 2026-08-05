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

#ifndef UBS_ENGINE_TEST_MEM_ADVICE_H
#define UBS_ENGINE_TEST_MEM_ADVICE_H

#include "gtest/gtest.h"
#include "mockcpp/mockcpp.hpp"

namespace ubse::mem::controller::ut {

class TestUbseMemAdvice : public ::testing::Test {
protected:
    void SetUp() override;

    void TearDown() override;

    void SetupDefaultMocks();

    static std::string BuildExpectedMsg(const char* processDesc, const std::string& name, const char* borrowTypeStr,
                                        size_t size, const std::string& exportNode, const std::string& importNode,
                                        const std::string& requestNode, const std::string& masterId,
                                        const std::string& errorCode, const char* errorInfo, uint32_t adviceCode,
                                        const char* adviceStr);
};

} // namespace ubse::mem::controller::ut

#endif // UBS_ENGINE_TEST_MEM_ADVICE_H
