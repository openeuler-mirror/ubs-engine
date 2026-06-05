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
#include "test_ubse_mem_controller_numa_api.h"

#include "ubse_com_module.h"
#include "ubse_election.h"
#include "ubse_mem_debt_info_query.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_mem_numa_borrow_exportobj_simpo.h"
#include "ubse_mmi_interface_impl.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_decoder_utils.h"
#include "ubse_mem_controller_numa_api.cpp"

namespace ubse::mem_controller::numa::ut {
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::mem::controller::message;
using namespace ubse::mem::controller;

void TestUbseMemControllerNumaApi::SetUp()
{
    Test::SetUp();
}
void TestUbseMemControllerNumaApi::TearDown()
{
    Test::TearDown();
    GlobalMockObject::verify();
}

TEST_F(TestUbseMemControllerNumaApi, GetPortIdSuccess)
{
    UbseCpuInfo cpuInfo;
    UbsePortInfo portInfo;
    portInfo.portId = "1";
    portInfo.portStatus = PortStatus::UP;
    UbseMemDebtNumaInfo numaInfo;
    portInfo.remoteSlotId = "1";
    cpuInfo.portInfos["1"] = portInfo;
    auto ret = ubse::mem::controller::GetPortId(cpuInfo, numaInfo, "1");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerNumaApi, GetPortIdFail)
{
    UbseCpuInfo cpuInfo;
    UbsePortInfo portInfo;
    portInfo.portStatus = PortStatus::DOWN;
    UbseMemDebtNumaInfo numaInfo;
    portInfo.remoteSlotId = "1";
    cpuInfo.portInfos["1"] = portInfo;
    auto ret = ubse::mem::controller::GetPortId(cpuInfo, numaInfo, "1");
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerNumaApi, FillChipIdAndPortIdForExport)
{
    ubse::nodeController::UbseNodeInfo nodeInfo;
    UbseCpuLocation cpuLocation{"1", 1};
    UbseCpuInfo cpuInfo;
    UbseMemDebtNumaInfo numaInfo;
    int socketId = 36;
    cpuInfo.socketId = socketId;
    cpuInfo.chipId = "1";
    numaInfo.socketId = socketId;
    nodeInfo.cpuInfos[cpuLocation] = cpuInfo;
    MOCKER_CPP(ubse::mem::controller::FillChipIdAndPortIdByNodeId).stubs().will(returnValue(UBSE_OK));
    auto ret = ubse::mem::controller::FillChipIdAndPortIdByNodeId(nodeInfo, numaInfo, "1");
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerNumaApi, FillChipIdForImport)
{
    ubse::nodeController::UbseNodeInfo nodeInfo;
    UbseCpuLocation cpuLocation{"1", 1};
    UbseCpuInfo cpuInfo;
    UbseMemDebtNumaInfo numaInfo;
    int socketId = 36;
    cpuInfo.socketId = socketId;
    cpuInfo.chipId = "1";
    numaInfo.socketId = socketId;
    nodeInfo.cpuInfos[cpuLocation] = cpuInfo;
    auto ret = ubse::mem::controller::FillChipIdAndPortIdForImport(nodeInfo, numaInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerNumaApi, ConstructNumaObjs)
{
    UbseMemNumaBorrowImportObj importObj;
    UbseMemDebtNumaInfo exportNumaInfo{"2", 36, 0};
    UbseMemDebtNumaInfo importNumaInfo{"1", 36, 0};
    ubse::nodeController::UbseNodeInfo nodeInfo;
    importObj.algoResult.exportNumaInfos.push_back(exportNumaInfo);
    importObj.algoResult.importNumaInfos.push_back(importNumaInfo);
    UbseMemNumaBorrowExportObj exportObj;
    UbseMemNumaBorrowReq req;
    req.linkInfo.lenderPort = 1;
    MOCKER_CPP(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));
    MOCKER_CPP(ubse::mem::controller::FillChipIdAndPortIdByNodeId).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::mem::controller::FillChipIdAndPortIdForImport).stubs().will(returnValue(UBSE_OK));
    auto ret = ubse::mem::controller::ConstructNumaObjs(importObj, exportObj, req);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerNumaApi, AgentSendNumaExportObj)
{
    std::shared_ptr<ubse::com::UbseComModule> comModule = std::make_shared<ubse::com::UbseComModule>();
    UbseMemNumaBorrowExportobjSimpoPtr ptr;
    UbseBaseMessagePtr ubseResponsePtr;
    UbseMemNumaBorrowExportObj exportObj;
    ubse::com::SendParam sendParam{"1", 0, 0};
    const auto func = &ubse::com::UbseComModule::RpcSend<UbseMemNumaBorrowExportobjSimpoPtr, UbseBaseMessagePtr>;
    MOCKER_CPP(func).stubs().will(returnValue(UBSE_ERROR)).then(returnValue(UBSE_OK));
    MOCKER_CPP(ubse::election::UbseGetMasterInfo).stubs().will(returnValue(UBSE_OK));
    auto ret = AgentSendNumaExportObj(comModule, sendParam, ptr, ubseResponsePtr, exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerNumaApi, HandleSendNumaExportError)
{
    UbseMemOperationResp resp;
    UbseMemNumaBorrowReq req;
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = "test";
    importObj.req.importNodeId = "1";
    UbseMemNumaBorrowExportObj exportObj;
    UbseMemDebtNumaInfo numaInfo{"2", 36, 0};
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);
    auto ret = HandleSendNumaExportError(resp, req, importObj, exportObj);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerNumaApi, ValidSocketAndNumaIdParams)
{
    UbseMemNumaBorrowReq req;
    req.linkInfo.lenderSocketId = 36;
    ubse::adapter_plugins::mmi::UbseNumaLocation numaLocation{"2", 0};
    req.lenderLocs.push_back(numaLocation);
    ubse::nodeController::UbseNodeInfo nodeInfo;
    ubse::nodeController::UbseNumaLocation numaLocation1{"2", 0};
    ubse::nodeController::UbseNumaInfo numaInfo;
    nodeInfo.numaInfos[numaLocation1] = numaInfo;
    MOCKER_CPP(&UbseNodeController::GetNodeById).stubs().will(returnValue(nodeInfo));
    auto ret = ValidSocketAndNumaIdParams(req);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerNumaApi, DealSendNumaUnExportObjFailed)
{
    UbseMemOperationResp resp;
    const std::string name = "test";
    UbseMemNumaBorrowExportObj exportObj;
    MOCKER_CPP(BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    auto ret = DealSendNumaUnExportObjFailed(resp, name, exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerNumaApi, DealSendNumaUnImportObjFailed)
{
    UbseMemNumaBorrowImportObj importObj;
    const UbseMemReturnReq req;
    UbseMemOperationResp resp;
    const std::string name;
    MOCKER_CPP(BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    auto ret = DealSendNumaUnImportObjFailed(importObj, req, resp, name);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerNumaApi, NumaReturnExistImportExportDestroyed)
{
    UbseMemNumaBorrowImportObj importObj;
    bool hasExport = true;
    UbseMemNumaBorrowExportObj exportObj;
    const UbseMemReturnReq req;
    UbseMemOperationResp resp;
    importObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    MOCKER_CPP(BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    auto ret = NumaReturnExistImport(importObj, hasExport, exportObj, req, resp);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerNumaApi, NumaReturnExistImportImportDestroyed)
{
    UbseMemNumaBorrowImportObj importObj;
    bool hasExport = true;
    UbseMemNumaBorrowExportObj exportObj;
    UbseMemDebtNumaInfo numaInfo{"1", 36, 0};
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);
    const UbseMemReturnReq req;
    UbseMemOperationResp resp;
    importObj.status.state = UBSE_MEM_IMPORT_DESTROYED;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    MOCKER_CPP(BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(SendNumaExportObj).stubs().will(returnValue(UBSE_OK));
    auto ret = NumaReturnExistImport(importObj, hasExport, exportObj, req, resp);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerNumaApi, NumaReturnExistImportImportSuccess)
{
    UbseMemNumaBorrowImportObj importObj;
    bool hasExport = true;
    UbseMemNumaBorrowExportObj exportObj;
    UbseMemDebtNumaInfo numaInfo{"1", 36, 0};
    exportObj.algoResult.exportNumaInfos.push_back(numaInfo);
    const UbseMemReturnReq req;
    UbseMemOperationResp resp;
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    MOCKER_CPP(BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    MOCKER_CPP(SendNumaImportObj).stubs().will(returnValue(UBSE_OK));
    auto ret = NumaReturnExistImport(importObj, hasExport, exportObj, req, resp);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerNumaApi, FindBorrowObjPair)
{
    const std::string name = "test";
    const std::string importNodeId = "1";
    auto [importObjPtr, exportObjPtr] =
        ubse::mem::controller::debt::FindBorrowObjPair<UbseMemNumaBorrowImportObj, UbseMemNumaBorrowExportObj>(
            name, importNodeId);
    EXPECT_EQ(importObjPtr, nullptr);
    EXPECT_EQ(exportObjPtr, nullptr);
}

TEST_F(TestUbseMemControllerNumaApi, HandleSingleExportReturnExportDestroyed)
{
    const UbseMemReturnReq req;
    UbseMemOperationResp resp;
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    MOCKER_CPP(BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    auto ret = HandleSingleExportReturn(req, resp, exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerNumaApi, HandleSingleExportReturnExportSuccess)
{
    const UbseMemReturnReq req;
    UbseMemOperationResp resp;
    UbseMemNumaBorrowExportObj exportObj;
    exportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    MOCKER_CPP(BuildOperationRespWhenFail).stubs().will(returnValue(UBSE_OK));
    auto ret = HandleSingleExportReturn(req, resp, exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMemControllerNumaApi, UbseMemNumaReturnImportNodeNok)
{
    UbseMemReturnReq req;
    req.name = "test";
    req.importNodeId = "1";
    UbseMemOperationResp resp;
    const std::string realRequestNodeId = "1";
    MOCKER_CPP(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_ERROR));
    auto ret = UbseMemNumaReturn(req, resp, realRequestNodeId);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerNumaApi, UbseMemNumaReturnNoBorrowObj)
{
    UbseMemReturnReq req;
    req.name = "test";
    req.importNodeId = "1";
    UbseMemOperationResp resp;
    const std::string realRequestNodeId = "1";
    MOCKER_CPP(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaReturn(req, resp, realRequestNodeId);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerNumaApi, UbseMemNumaReturn)
{
    UbseMemReturnReq req;
    req.name = "test";
    req.importNodeId = "1";
    UbseMemOperationResp resp;
    const std::string realRequestNodeId = "1";
    MOCKER_CPP(WaitInitLedgerSuccess).stubs().will(returnValue(UBSE_OK));
    auto ret = UbseMemNumaReturn(req, resp, realRequestNodeId);
    EXPECT_EQ(ret, UBSE_ERROR);
}

TEST_F(TestUbseMemControllerNumaApi, CheckNumaResourceState)
{
    std::string name = "test";
    std::string importNodeId = "1";
    UbseMemNumaBorrowImportObj importObj;
    importObj.req.name = name;
    importObj.status.state = UBSE_MEM_IMPORT_SUCCESS;
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().PutResource(
        importNodeId, name, std::make_shared<UbseMemNumaBorrowImportObj>(importObj));
    auto ret = CheckNumaResourceState(name, importNodeId);
    EXPECT_EQ(ret, UBSE_ERR_EXISTED);
    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemNumaBorrowImportObj>().RemoveResource(importNodeId, name);
}

} // namespace ubse::mem_controller::numa::ut