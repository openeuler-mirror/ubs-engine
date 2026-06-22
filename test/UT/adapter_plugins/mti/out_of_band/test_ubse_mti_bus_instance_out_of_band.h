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

#ifndef TEST_UBSE_MTI_BUS_INSTANCE_OUT_OF_BAND_H
#define TEST_UBSE_MTI_BUS_INSTANCE_OUT_OF_BAND_H

#include <gtest/gtest.h>
#include <mockcpp/mockcpp.hpp>
#include "src/adapter_plugins/mti/out_of_band/ubse_mti_bus_instance_out_of_band.h"
#include "src/include/adapter_plugins/mti/ubse_mti_bus_instance.h"

namespace ubse::ut::mti::bus_instance {
using namespace ubse::mti::bus_instance;

class TestUbseMtiBusInstanceOutOfBand : public ::testing::Test {
public:
    TestUbseMtiBusInstanceOutOfBand() = default;
    ~TestUbseMtiBusInstanceOutOfBand() override = default;

protected:
    void SetUp() override;
    void TearDown() override;

    UbseMtiBusInstanceOutOfBand outOfBand_;
    UbseMtiBusInst testBusInstance_;
};

} // namespace ubse::ut::mti::bus_instance

#endif // TEST_UBSE_MTI_BUS_INSTANCE_OUT_OF_BAND_H
