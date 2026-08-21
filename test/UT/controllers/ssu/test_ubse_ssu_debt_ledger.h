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

#ifndef UBS_ENGINE_TEST_UBSE_SSU_DEBT_LEDGER_H
#define UBS_ENGINE_TEST_UBSE_SSU_DEBT_LEDGER_H

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "ubse_ssu_def.h"
#include "debt/ubse_ssu_debt_ledger.h"

namespace ubse::ssu::debt::ut {

using namespace ubse::adapter_plugins::ssu::def;

class TestUbseSsuDebtLedger : public testing::Test {
public:
    TestUbseSsuDebtLedger() = default;

    void SetUp() override;

    void TearDown() override;

protected:
    // 构造一个带自定义数据的命名空间
    static UbseSsuDevNameSpace MakeNs(const std::string& name, const std::string& eid, uint32_t nsId, uint64_t nsze);

    // 构造一个设备，包含多个命名空间
    static UbseSsuDevInfo MakeDev(const std::string& eid, const std::vector<UbseSsuDevNameSpace>& nsList,
                                  const std::vector<UbseSsuDevNameSpaceAttachInfo>& attachList = {},
                                  uint64_t totalBytes = 4096, uint64_t usedBytes = 0);
};

} // namespace ubse::ssu::debt::ut

#endif // UBS_ENGINE_TEST_UBSE_SSU_DEBT_LEDGER_H
