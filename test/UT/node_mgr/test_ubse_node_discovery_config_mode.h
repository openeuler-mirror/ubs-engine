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

#ifndef UBS_ENGINE_TEST_UBSE_NODE_DISCOVERY_CONFIG_MODE_H
#define UBS_ENGINE_TEST_UBSE_NODE_DISCOVERY_CONFIG_MODE_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

namespace ubse::nodeMgr {
class TestUbseNodeDiscoveryConfigMode : public testing::Test {
public:
    TestUbseNodeDiscoveryConfigMode() = default;

    void SetUp() override;

    void TearDown() override;
};
} // namespace ubse::nodeDiscovery

#endif // UBS_ENGINE_TEST_UBSE_NODE_DISCOVERY_CONFIG_MODE_H
