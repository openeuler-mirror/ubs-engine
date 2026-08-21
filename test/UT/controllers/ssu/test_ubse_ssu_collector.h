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

#ifndef TEST_UBSE_SSU_COLLECTOR_H
#define TEST_UBSE_SSU_COLLECTOR_H

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "ubse_ssu_collector.h"

namespace ubse::ssu::service::ut {

class TestUbseSsuCollector : public testing::Test {
public:
    TestUbseSsuCollector() = default;

    void SetUp() override;
    void TearDown() override;

    // Helper: 构造测试用设备信息
    static UbseSsuDevInfo MakeDevInfo(const std::string &eid,
                                      uint64_t totalBytes = 1099511627776ULL,
                                      uint64_t usedBytes = 0);
};

} // namespace ubse::ssu::service::ut

#endif // TEST_UBSE_SSU_COLLECTOR_H
