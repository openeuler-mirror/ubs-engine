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

#ifndef TEST_UBSE_SSU_RPC_HANDLER_H
#define TEST_UBSE_SSU_RPC_HANDLER_H

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "debt/ubse_ssu_debt_ledger.h"
#include "ubse_ssu_def.h"

namespace ubse::ssu::controller::ut {

using namespace ubse::ssu::debt;
using namespace ubse::adapter_plugins::ssu::def;

class TestUbseSsuRpcHandler : public testing::Test {
public:
    TestUbseSsuRpcHandler() = default;

    void SetUp() override;
    void TearDown() override;

protected:
    // 辅助方法：在账本中放入一个entry
    static void PutLedgerEntry(const std::string &name, UbseSsuNsState state);

    // 辅助方法：清空账本
    static void CleanupLedger();
};

} // namespace ubse::ssu::controller::ut

#endif // TEST_UBSE_SSU_RPC_HANDLER_H
