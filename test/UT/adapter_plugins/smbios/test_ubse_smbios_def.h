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

#ifndef TEST_UBSE_SMBIOS_DEF_H
#define TEST_UBSE_SMBIOS_DEF_H

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>

#include "smbios/ubse_smbios_def.h"

namespace ubse::adapter_plugins::smbios::ut {

class TestUbseSmbiosDef : public testing::Test {
public:
    TestUbseSmbiosDef() = default;

    void SetUp() override;
    void TearDown() override;

    static std::vector<uint8_t> BuildType131DmiTable(uint8_t flag, uint16_t podId, uint8_t slotId,
                                                     uint8_t meshType, uint16_t superPodId);
    static std::vector<uint8_t> BuildSmbios3EntryPoint(uint32_t dmiSize);
};

} // namespace ubse::adapter_plugins::smbios::ut

#endif // TEST_UBSE_SMBIOS_DEF_H
