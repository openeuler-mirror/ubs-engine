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

#include "debt/ubse_mem_debt_ledger.h"
#include "mockcpp/mockcpp.hpp"
#include "ubse_mem_configuration.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_mem_debt_info.h"
#include "water_process/resource_analysis.h"

namespace ubse::mem::strategy::ut {
using namespace ubse::mem::controller;
using namespace ubse::mem::controller::debt;

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
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    UbseMemNumaBorrowImportObj numaImportObj;
    numaImportObj.req.importNodeId = "0";
    numaImportObj.algoResult.importNumaInfos.resize(1);
    numaImportObj.algoResult.importNumaInfos[0].numaId = 1;
    numaImportObj.algoResult.importNumaInfos[0].socketId = 1;
    numaImportObj.algoResult.exportNumaInfos.resize(1);
    numaImportObj.algoResult.exportNumaInfos[0].nodeId = "1";
    numaImportObj.algoResult.exportNumaInfos[0].numaId = 1;
    numaImportObj.algoResult.exportNumaInfos[0].socketId = 1;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource("0", "test", std::make_shared<UbseMemNumaBorrowImportObj>(numaImportObj));
    MOCKER_CPP(&UbseMemTopologyInfoManager::GetAllNumaInfo).stubs().will(returnValue(numaInfoList));
    MOCKER(event::UbsePubEvent).stubs().will(returnValue(UBSE_OK));
    UbseMemNumaLoc warningNumaLoc{.nodeId = "0", .socketId = 1, .numaId = 1};
    auto ret = WaterWarningProcess(WatermarkWarningType::HIGH_WATERMARK, warningNumaLoc, true);
    UbseMemDebtLedger::GetInstance().ClearAllNodeMaps();
    EXPECT_EQ(ret, UBSE_OK);
}

}  // namespace ubse::mem::strategy::ut