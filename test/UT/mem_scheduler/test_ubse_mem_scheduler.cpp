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

#include "test_ubse_mem_scheduler.h"

#include <ubse_node.h>

#include "ubse_mem_account_helper.h"
#include "ubse_mem_configuration.h"
#include "ubse_mem_scheduler.h"
#include "ubse_mem_strategy_helper.h"
#include "ubse_mem_topology_info_manager.h"
#include "ubse_node_controller.h"

namespace ubse::mem_scheduler::ut {
using namespace ubse::mem::strategy;
using namespace ubse::mem::scheduler;
using namespace ubse::nodeController;
using namespace ubse::adapter_plugins::mmi;

ubse::common::def::UbseResult FAKE_GetUbseConf(const std::string& section, const std::string& configKey,
                                               std::string& configValue)
{
    if (section == "ubse.strategy" && configKey == "system.pool.memory.ratio") {
        configValue = "100";
    } else if (section == "ubse.strategy" && configKey == "borrow.max.limit") {
        configValue = "4194304";
    } else if (section == "ubse.strategy" && configKey == "socket.import.max.limit") {
        configValue = "262144";
    } else if (section == "ubse.strategy" && configKey == "ubse.mem.obmm.memory.block.size") {
        configValue = "256";
    }
    return UBSE_OK;
}

void TestUbseMemScheduler::SetUp()
{
    Test::SetUp();
}
void TestUbseMemScheduler::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemScheduler, Init)
{
    MOCKER_CPP(GetUbseConf<std::string>).stubs().will(invoke(FAKE_GetUbseConf));
    MOCKER_CPP(&UbseNodeController::RegClusterStateNotifyHandler).stubs().will(returnValue(UBSE_OK));
    auto ret = Init();
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemScheduler, UbseMemNodeObjChangeHandlerWorking)
{
    ubse::nodeController::UbseNodeInfo nodeInfo;
    ubse::nodeController::UbseNumaLocation location1{"1", 1};
    ubse::nodeController::UbseNumaInfo ubseNumaInfo1{location1, 36, {}, 1024, 512, 128, 128, 100};
    ubse::nodeController::UbseNumaLocation location2{"1", 2};
    ubse::nodeController::UbseNumaInfo ubseNumaInfo2{location2, 36, {}, 1024, 512, 128, 128, 100};
    nodeInfo.numaInfos.insert({location1, ubseNumaInfo1});
    nodeInfo.numaInfos.insert({location2, ubseNumaInfo2});
    nodeInfo.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeMap;
    nodeMap["1"] = nodeInfo;
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeMap));
    MOCKER_CPP(&ubse::mem::strategy::UbseMemTopologyInfoManager::NodesInit).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNodeObjChangeHandler(nodeInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemScheduler, UbseMemNodeObjChangeHandlerNotWorking)
{
    ubse::nodeController::UbseNodeInfo nodeInfo1;
    ubse::nodeController::UbseNumaLocation location1{"1", 1};
    ubse::nodeController::UbseNumaInfo ubseNumaInfo1{location1, 36, {}, 1024, 512, 128, 128, 100};
    nodeInfo1.numaInfos.insert({location1, ubseNumaInfo1});

    nodeInfo1.clusterState = UbseNodeClusterState::UBSE_NODE_SMOOTHING;
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeMap;
    nodeMap["1"] = nodeInfo1;
    ubse::nodeController::UbseNodeInfo nodeInfo2;
    ubse::nodeController::UbseNumaLocation location2{"1", 2};
    ubse::nodeController::UbseNumaInfo ubseNumaInfo2{location2, 36, {}, 1024, 512, 128, 128, 100};
    nodeInfo2.clusterState = UbseNodeClusterState::UBSE_NODE_WORKING;
    nodeInfo2.numaInfos.insert({location2, ubseNumaInfo2});
    nodeMap["2"] = nodeInfo2;
    MOCKER_CPP(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeMap));

    MOCKER_CPP(&ubse::mem::strategy::UbseMemTopologyInfoManager::NodesInit).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNodeObjChangeHandler(nodeInfo1);
    EXPECT_EQ(ret, UBSE_OK);
}

/*
 * 用例描述：测试fd借用
 * fd借用
 * 测试步骤：返回UBSE_OK
 */
TEST_F(TestUbseMemScheduler, UbseMemFdImportObjStateChangeHandlerScheduling)
{
    UbseMemFdBorrowImportObj importObj;
    importObj.status.state = UBSE_MEM_SCHEDULING;
    importObj.req.importNodeId = "1";
    UbseMemDebtNumaInfo ubseMemDebtNumaInfo{"2", 36, 1, 128};
    importObj.algoResult.exportNumaInfos.push_back(ubseMemDebtNumaInfo);
    importObj.algoResult.attachSocketId = 36;

    MOCKER_CPP(&ubse::mem::strategy::UbseMemStrategyHelper::FdMemoryBorrow)
        .stubs()
        .with(outBound(importObj.req), outBound(importObj.algoResult))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&ubse::mem::strategy::AlgoAccountManger::AddAlgoAccount).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdImportObjStateChangeHandler(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

/*
 * 用例描述：测试fd借用
 * fd借用
 * 测试步骤：返回UBSE_OK
 */
TEST_F(TestUbseMemScheduler, UbseMemFdImportObjStateChangeHandlerSchedulingByUserReq)
{
    UbseMemFdBorrowImportObj importObj;
    importObj.status.state = UBSE_MEM_SCHEDULING;
    importObj.req.importNodeId = "1";
    UbseMemDebtNumaInfo ubseMemDebtNumaInfo{"2", 36, 1, 128};
    importObj.algoResult.exportNumaInfos.push_back(ubseMemDebtNumaInfo);
    importObj.algoResult.attachSocketId = 36;
    ubse::adapter_plugins::mmi::UbseNumaLocation numaLocation{"1", 1};
    importObj.req.lenderLocs.push_back(numaLocation);

    MOCKER_CPP(&ubse::mem::strategy::UbseMemStrategyHelper::MemoryBorrowAccordingToUserRequest<UbseMemFdBorrowReq>)
        .stubs()
        .with(outBound(importObj.req), outBound(importObj.algoResult))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&ubse::mem::strategy::AlgoAccountManger::AddAlgoAccount).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemFdImportObjStateChangeHandler(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemScheduler, UbseMemFdImportObjStateChangeHandlerSuccess)
{
    UbseMemFdBorrowImportObj importObj;
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;

    auto ret = UbseMemFdImportObjStateChangeHandler(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemScheduler, UbseMemFdImportObjStateChangeHandlerDestroyed)
{
    UbseMemFdBorrowImportObj importObj;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    std::shared_ptr<BaseAlgoAccount> algoAccount;
    MOCKER_CPP(&ubse::mem::strategy::AlgoAccountManger::GetAlgoAccount).stubs().will(returnValue(algoAccount));
    auto ret = UbseMemFdImportObjStateChangeHandler(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemScheduler, UbseMemFdImportObjStateChangeHandlerFailed)
{
    UbseMemFdBorrowImportObj importObj;
    importObj.status.state = UBSE_MEM_STATE_FAILED;
    std::shared_ptr<BaseAlgoAccount> algoAccount;
    MOCKER_CPP(&AlgoAccountManger::GetAlgoAccount).stubs().will(returnValue(algoAccount));
    auto ret = UbseMemFdImportObjStateChangeHandler(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemScheduler, UbseMemFdExportObjStateChangeHandlerDestroyed)
{
    UbseMemFdBorrowExportObj exportObj;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    std::shared_ptr<BaseAlgoAccount> algoAccount;
    MOCKER_CPP(&ubse::mem::strategy::AlgoAccountManger::GetAlgoAccount).stubs().will(returnValue(algoAccount));
    auto ret = UbseMemFdExportObjStateChangeHandler(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemScheduler, UbseMemFdExportObjStateChangeHandlerFailed)
{
    UbseMemFdBorrowExportObj exportObj;
    exportObj.status.state = UBSE_MEM_STATE_FAILED;
    std::shared_ptr<BaseAlgoAccount> algoAccount;
    MOCKER_CPP(&ubse::mem::strategy::AlgoAccountManger::GetAlgoAccount).stubs().will(returnValue(algoAccount));
    auto ret = UbseMemFdExportObjStateChangeHandler(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

/*
 * 用例描述：测试numa借用
 * numa借用
 * 测试步骤：返回UBSE_OK
 */
TEST_F(TestUbseMemScheduler, UbseMemNumaImportObjStateChangeHandlerScheduling)
{
    UbseMemNumaBorrowImportObj importObj;
    importObj.status.state = UBSE_MEM_SCHEDULING;
    importObj.req.importNodeId = "1";
    UbseMemDebtNumaInfo ubseMemDebtNumaInfo{"2", 36, 1, 128};
    importObj.algoResult.exportNumaInfos.push_back(ubseMemDebtNumaInfo);
    importObj.algoResult.attachSocketId = 36;

    MOCKER_CPP(&ubse::mem::strategy::UbseMemStrategyHelper::NumaMemoryBorrow)
        .stubs()
        .with(outBound(importObj.req), outBound(importObj.algoResult))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&ubse::mem::strategy::AlgoAccountManger::AddAlgoAccount).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaImportObjStateChangeHandler(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemScheduler, UbseMemNumaImportObjStateChangeHandlerSchedulingByUserReq)
{
    UbseMemNumaBorrowImportObj importObj;
    importObj.status.state = UBSE_MEM_SCHEDULING;
    importObj.req.importNodeId = "1";
    UbseMemDebtNumaInfo ubseMemDebtNumaInfo{"2", 36, 1, 128};
    importObj.algoResult.exportNumaInfos.push_back(ubseMemDebtNumaInfo);
    importObj.algoResult.attachSocketId = 36;
    ubse::adapter_plugins::mmi::UbseNumaLocation numaLocation{"1", 1};
    importObj.req.lenderLocs.push_back(numaLocation);

    MOCKER_CPP(&ubse::mem::strategy::UbseMemStrategyHelper::MemoryBorrowAccordingToUserRequest<UbseMemNumaBorrowReq>)
        .stubs()
        .with(outBound(importObj.req), outBound(importObj.algoResult))
        .will(returnValue(UBSE_OK));
    MOCKER_CPP(UbseNodeMemGetTopologyCnaInfo).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&ubse::mem::strategy::AlgoAccountManger::AddAlgoAccount).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaImportObjStateChangeHandler(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemScheduler, UbseMemNumaImportObjStateChangeHandlerSuccess)
{
    UbseMemNumaBorrowImportObj importObj;
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;

    auto ret = UbseMemNumaImportObjStateChangeHandler(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemScheduler, UbseMemNumaImportObjStateChangeHandlerDestroyed)
{
    UbseMemNumaBorrowImportObj importObj;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    std::shared_ptr<BaseAlgoAccount> algoAccount;
    MOCKER_CPP(&ubse::mem::strategy::AlgoAccountManger::GetAlgoAccount).stubs().will(returnValue(algoAccount));
    auto ret = UbseMemNumaImportObjStateChangeHandler(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemScheduler, UbseMemNumaImportObjStateChangeHandlerFailed)
{
    UbseMemNumaBorrowImportObj importObj;
    importObj.status.state = UBSE_MEM_STATE_FAILED;
    std::shared_ptr<BaseAlgoAccount> algoAccount;
    MOCKER_CPP(&AlgoAccountManger::GetAlgoAccount).stubs().will(returnValue(algoAccount));
    auto ret = UbseMemNumaImportObjStateChangeHandler(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemScheduler, UbseMemNumaExportObjStateChangeHandlerDestroyed)
{
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    std::shared_ptr<BaseAlgoAccount> algoAccount;
    MOCKER_CPP(&ubse::mem::strategy::AlgoAccountManger::GetAlgoAccount).stubs().will(returnValue(algoAccount));
    auto ret = UbseMemNumaExportObjStateChangeHandler(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemScheduler, UbseMemNumaExportObjStateChangeHandlerFailed)
{
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.status.state = UBSE_MEM_STATE_FAILED;
    std::shared_ptr<BaseAlgoAccount> algoAccount;
    MOCKER_CPP(&ubse::mem::strategy::AlgoAccountManger::GetAlgoAccount).stubs().will(returnValue(algoAccount));
    auto ret = UbseMemNumaExportObjStateChangeHandler(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemScheduler, UbseMemAddrImportObjStateChangeHandlerScheduling)
{
    UbseMemAddrBorrowImportObj importObj;
    importObj.status.state = UBSE_MEM_SCHEDULING;
    std::shared_ptr<BaseAlgoAccount> algoAccount;
    MOCKER_CPP(&ubse::mem::strategy::UbseMemStrategyHelper::AddrMemoryBorrow).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(&ubse::mem::strategy::AlgoAccountManger::GetAlgoAccount).stubs().will(returnValue(algoAccount));
    auto ret = UbseMemAddrImportObjStateChangeHandler(importObj);
    ASSERT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemScheduler, UbseMemAddrExportObjStateChangeHandlerDestroy)
{
    UbseMemAddrBorrowExportObj exportObj;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    std::shared_ptr<BaseAlgoAccount> algoAccount;
    MOCKER_CPP(&ubse::mem::strategy::AlgoAccountManger::GetAlgoAccount).stubs().will(returnValue(algoAccount));
    auto ret = UbseMemAddrExportObjStateChangeHandler(exportObj);
    ASSERT_EQ(ret, UBSE_OK);
}

} // namespace ubse::mem_scheduler::ut