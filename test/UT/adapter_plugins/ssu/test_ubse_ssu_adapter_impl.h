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

#ifndef TEST_UBSE_SSU_ADAPTER_IMPL_H
#define TEST_UBSE_SSU_ADAPTER_IMPL_H

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "ubse_ssu_def.h"
#include "ubse_ssu_adapter_impl.h"
#include "ubse_conf.h"

namespace ubse::adapter_plugins::ssu::ut {
using namespace ubse::adapter_plugins::ssu::def;

class TestUbseSsuAdapterImpl : public testing::Test {
public:
    TestUbseSsuAdapterImpl() = default;

    void SetUp() override;
    void TearDown() override;

    static std::string MakeEid(char c);
    static UbseSsuDevInfo MakeDevInfo(const std::string &eid, const std::string &subNqn);
    static UbseSsuDevNameSpace MakeNameSpaceForCreate(const std::string &eid,
                                                      const std::string &subNqn,
                                                      uint64_t nsze,
                                                      uint64_t ncap);
    static UbseSsuDevNameSpace MakeNameSpaceForBasic(const std::string &eid,
                                                     const std::string &subNqn,
                                                     uint32_t namespaceId,
                                                     const std::string &guid);
    static DevInfoT MakeDevInfoT(const std::string &eid, const std::string &subNqn,
                                 DevStatusT state, uint32_t nsCount);
};

} // namespace ubse::adapter_plugins::ssu::ut

#endif // TEST_UBSE_SSU_ADAPTER_IMPL_H
