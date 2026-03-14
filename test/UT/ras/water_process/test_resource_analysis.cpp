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

#include "test_resource_analysis.h"

#include <ubse_event.h>
#include <gmock/gmock.h>
#include <mem/mem_scheduler/ubse_mem_meta_data.h>

#include "mockcpp/mockcpp.hpp"
#include "ubse_mem_configuration.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_mem_debt_info.h"
#include "water_process/resource_analysis.h"

namespace ubse::mem::strategy::ut {
using namespace ubse::mem::controller;

void TestResourceAnalysis::SetUp()
{
    Test::SetUp();
}

void TestResourceAnalysis::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}
TEST_F(TestResourceAnalysis, WaterWarningProcess)
{
    MOCKER(&UbseMemConfiguration::GetManagerVmEnable).stubs().will(returnValue(true));
    UbseMemNumaLoc numaLoc{.nodeId = "0", .socketId = 1, .numaId = 1};
    UbseMemNumaIndexLoc numaIdx{};
    std::vector<std::shared_ptr<MemNumaInfo>> numaInfoList{std::make_shared<MemNumaInfo>(numaLoc, numaIdx, 0)};
    auto saveDept = nodeMemDebtInfoMap;
    nodeMemDebtInfoMap.clear();
    nodeMemDebtInfoMap["0"].numaImportObjMap["test"].req.importNodeId = "0";
    nodeMemDebtInfoMap["0"].numaImportObjMap["test"].algoResult.importNumaInfos.resize(1);
    nodeMemDebtInfoMap["0"].numaImportObjMap["test"].algoResult.importNumaInfos[0].numaId = 1;
    nodeMemDebtInfoMap["0"].numaImportObjMap["test"].algoResult.importNumaInfos[0].socketId = 1;
    nodeMemDebtInfoMap["0"].numaImportObjMap["test"].algoResult.exportNumaInfos.resize(1);
    nodeMemDebtInfoMap["0"].numaImportObjMap["test"].algoResult.exportNumaInfos[0].nodeId = "1";
    nodeMemDebtInfoMap["0"].numaImportObjMap["test"].algoResult.exportNumaInfos[0].numaId = 1;
    nodeMemDebtInfoMap["0"].numaImportObjMap["test"].algoResult.exportNumaInfos[0].socketId = 1;
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetAllNumaInfo).stubs().will(returnValue(numaInfoList));
    MOCKER(event::UbsePubEvent).stubs().will(returnValue(UBSE_OK));
    UbseMemNumaLoc warningNumaLoc{.nodeId = "0", .socketId = 1, .numaId = 1};
    auto ret = WaterWarningProcess(WatermarkWarningType::HIGH_WATERMARK, warningNumaLoc, true);
    nodeMemDebtInfoMap = saveDept;
    EXPECT_EQ(ret, UBSE_OK);
}

}  // namespace ubse::mem::strategy::ut