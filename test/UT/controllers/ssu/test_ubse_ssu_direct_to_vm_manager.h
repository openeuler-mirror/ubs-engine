/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 */

#ifndef TEST_UBSE_SSU_DIRECT_TO_VM_MANAGER_H
#define TEST_UBSE_SSU_DIRECT_TO_VM_MANAGER_H

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include "ubse_ssu_direct_to_vm_manager.h"

namespace ubse::ut::ssu {
using namespace ubse::ssu::service;

class TestUbseSsuDirectToVmManager : public testing::Test {
public:
    TestUbseSsuDirectToVmManager() = default;
    void SetUp() override;
    void TearDown() override;

    UbseMtiIdevPfe MakePfe(uint8_t slotId, uint8_t chipId, uint8_t dieId, uint8_t pfeId);
    UbseMtiIdevVfe MakeVfe(uint8_t slotId, uint8_t chipId, uint8_t dieId, uint8_t pfeId, uint8_t vfeId);
    UbseMtiBusInst MakeVmBusInst(uint16_t upi, const UbseMtiGuid &guid,
                                 const std::vector<UbseMtiGuid> &subDeviceGuids = {});
    UbseMtiGuid MakeGuid(uint8_t val);
};

} // namespace ubse::ut::ssu
#endif // TEST_UBSE_SSU_DIRECT_TO_VM_MANAGER_H
