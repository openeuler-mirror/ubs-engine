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

#include "test_ubse_topo_util.h"
#include "ubse_topo_util.h"

namespace ubse::ut::utils {
using namespace ubse::utils;
using namespace ubse::nodeController;
void TestUbseTopoUtil::SetUp()
{
    Test::SetUp();
}

void TestUbseTopoUtil::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseTopoUtil, TestIsMultiPortTopo_WhenTwoPortsSameRemoteSlotAndChip_ExpectTrue)
{
    UbseCpuInfo ubseCpuInfo{};
    UbsePortInfo portInfo1{};
    portInfo1.portStatus = PortStatus::UP;
    portInfo1.portId = "1";
    portInfo1.remoteSlotId = "1";
    portInfo1.remoteChipId = "1";
    UbsePortInfo portInfo2{};
    portInfo2.portStatus = PortStatus::UP;
    portInfo2.portId = "2";
    portInfo2.remoteSlotId = "1";
    portInfo2.remoteChipId = "1";
    std::unordered_map<std::string, UbsePortInfo> ports{};
    ports[portInfo1.portId] = portInfo1;
    ports[portInfo2.portId] = portInfo2;
    ubseCpuInfo.portInfos = ports;
    EXPECT_TRUE(IsMultiPortTopo(ubseCpuInfo));
}

} // namespace ubse::ut::utils
