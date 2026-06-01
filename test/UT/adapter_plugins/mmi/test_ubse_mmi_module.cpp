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
#include "test_ubse_mmi_module.h"
#include "ubse_mem_instance_inner.h"
#include "ubse_node_controller.h"
#include "ubse_obmm_executor.h"
#include "ubse_obmm_meta_restore.h"
#include "ubse_obmm_utils.h"
#include "ubse_security_module.h"
#include "adapter_plugins/mti/ubse_mti_def.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"
namespace ubse::ut::mmi {
using namespace ubse::adapter_plugins::mmi;
std::atomic<uint64_t> TestUbseMmiModule::mockMemId_{1};
TEST_F(TestUbseMmiModule, UbseMemFdImportExecutor_Success)
{
    GTEST_SKIP();
    UbseMmiModule module;
    std::vector<UbseMemObmmInfo> desc{};
    desc.resize(8);
    UbseMemFdBorrowImportObj importObj;

    importObj.exportObmmInfo = desc;
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.owner = {.uid = 0, .gid = 0, .pid = 114514U, .mode = 0600};
    importObj.req.lenderLocs = {{"2", 0}, {"2", 1}};
    importObj.req.lenderSizes = {1 << 29, 1 << 29};
    importObj.req.size = 1 << 30;
    importObj.req.udsInfo = {.uid = 0, .gid = 0, .pid = 114514U};
    importObj.req.name = "UbseMemFdImportExecutor_Success";
    importObj.algoResult.attachSocketId = 0;
    importObj.algoResult.exportNumaInfos = {{"2", 0, 0, 1 << 29}, {"2", 0, 1, 1 << 29}};
    importObj.algoResult.importNumaInfos = {{"1", 0, 0, 1 << 29}, {"1", 0, 1, 1 << 29}};
    importObj.status.decoderResult.resize(8);
    RmObmmExecutor::GetInstance().obmmImportFunc = [](const struct obmm_mem_desc* desc, unsigned long flags,
                                                      int base_dist, int* numa) {
        return TestUbseMmiModule::mockMemId_.fetch_add(1);
    };
    auto ret = module.UbseMemFdImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemFdExportExecutor_Success)
{
    UbseMmiModule module;
    UbseMemFdBorrowExportObj exportObj;

    exportObj.req.distance = UbseMemDistance::MEM_DISTANCE_L0;
    exportObj.req.owner = {.uid = 0, .gid = 0, .pid = 114514U, .mode = 0600};
    exportObj.req.size = 1 << 30;
    exportObj.req.lenderLocs = {{"2", 0}, {"2", 1}};
    exportObj.req.lenderSizes = {1 << 29, 1 << 29};
    exportObj.req.udsInfo = {.uid = 0, .gid = 0, .pid = 114514U};
    exportObj.req.requestNodeId = "1";
    exportObj.req.importNodeId = "1";
    exportObj.req.name = "UbseMemFdExportExecutor_Success";
    exportObj.algoResult.attachSocketId = 0;
    exportObj.algoResult.exportNumaInfos = {{"2", 0, 0, 1 << 29}, {"2", 0, 1, 1 << 29}};
    exportObj.algoResult.importNumaInfos = {{"1", 0, 0, 1 << 29}, {"1", 0, 1, 1 << 29}};
    MOCKER(&nodeController::UbseNodeController::GetLocalEidBySocket)
        .stubs()
        .with(_, outBound(static_cast<uint32_t>(0x444)))
        .will(returnValue(static_cast<uint32_t>(0)));
    RmObmmExecutor::GetInstance().obmmExportFunc = [](const size_t length[OBMM_MAX_LOCAL_NUMA_NODES],
                                                      unsigned long flags, struct obmm_mem_desc* desc) {
        return TestUbseMmiModule::mockMemId_.fetch_add(1);
    };
    auto ret = module.UbseMemFdExportExecutor(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemFdUnExportExecutor_Success)
{
    UbseMmiModule module;
    UbseMemFdBorrowExportObj exportObj;

    exportObj.req.distance = UbseMemDistance::MEM_DISTANCE_L0;
    exportObj.req.owner = {.uid = 0, .gid = 0, .pid = 114514U, .mode = 0600};
    exportObj.req.size = 1 << 30;
    exportObj.req.lenderLocs = {{"2", 0}, {"2", 1}};
    exportObj.req.lenderSizes = {1 << 29, 1 << 29};
    exportObj.req.udsInfo = {.uid = 0, .gid = 0, .pid = 114514U};
    exportObj.req.requestNodeId = "1";
    exportObj.req.importNodeId = "1";
    exportObj.req.name = "UbseMemFdUnExportExecutor_Success";
    exportObj.algoResult.attachSocketId = 0;
    exportObj.algoResult.exportNumaInfos = {{"2", 0, 0, 1 << 29}, {"2", 0, 1, 1 << 29}};
    exportObj.algoResult.importNumaInfos = {{"1", 0, 0, 1 << 29}, {"1", 0, 1, 1 << 29}};
    exportObj.status.state = UBSE_MEM_STATE_SUCCEEDED;
    exportObj.status.errCode = 0;
    exportObj.status.expectState = UBSE_MEM_STATE_DESTROYING;
    exportObj.status.exportObmmInfo = {{1, {}}, {2, {}}, {3, {}}, {4, {}}, {5, {}}, {6, {}}, {7, {}}, {8, {}}};
    RmObmmExecutor::GetInstance().obmmUnexportFunc = [](mem_id id, unsigned long flags) {
        return 0;
    };
    auto ret = module.UbseMemFdUnExportExecutor(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemFdUnImportExecutor_Success)
{
    UbseMmiModule module;

    UbseMemFdBorrowImportObj importObj;

    importObj.req.distance = UbseMemDistance::MEM_DISTANCE_L0;
    importObj.req.owner = {.uid = 0, .gid = 0, .pid = 114514U, .mode = 0600};
    importObj.req.size = 1 << 30;
    importObj.req.lenderLocs = {{"2", 0}, {"2", 1}};
    importObj.req.lenderSizes = {1 << 29, 1 << 29};
    importObj.req.udsInfo = {.uid = 0, .gid = 0, .pid = 114514U};
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.name = "UbseMemFdUnImportExecutor_Success";
    importObj.algoResult.attachSocketId = 0;
    importObj.algoResult.exportNumaInfos = {{"2", 0, 0, 1 << 29}, {"2", 0, 1, 1 << 29}};
    importObj.algoResult.importNumaInfos = {{"1", 0, 0, 1 << 29}, {"1", 0, 1, 1 << 29}};
    importObj.status.state = UBSE_MEM_STATE_SUCCEEDED;
    importObj.status.errCode = 0;
    importObj.status.expectState = UBSE_MEM_STATE_DESTROYING;
    importObj.status.scna = 0x401;
    importObj.status.importResults = {{1, -1}, {2, -1}, {3, -1}, {4, -1}, {5, -1}, {6, -1}, {7, -1}, {8, -1}};
    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
        return 0;
    };
    auto ret = module.UbseMemFdUnImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemNumaExportExecutor_Success)
{
    UbseMmiModule module;
    UbseMemNumaBorrowExportObj exportobj;
    exportobj.req.name = "numa";
    exportobj.req.srcSocket = 0;
    exportobj.req.srcNuma = 0;
    exportobj.req.lowWatermark = 11;
    exportobj.req.highWatermark = 88;
    exportobj.req.owner = {.uid = 0, .gid = 0, .pid = 114514U, .mode = 0600};
    exportobj.req.size = 1 << 30;
    exportobj.req.lenderLocs = {{"2", 0}, {"2", 1}};
    exportobj.req.lenderSizes = {1 << 29, 1 << 29};
    exportobj.req.udsInfo = {.uid = 0, .gid = 0, .pid = 114514U};
    exportobj.req.requestNodeId = "1";
    exportobj.req.importNodeId = "1";

    exportobj.algoResult.attachSocketId = 0;
    exportobj.algoResult.exportNumaInfos = {{"2", 0, 0, 1 << 29}, {"2", 0, 1, 1 << 29}};
    exportobj.algoResult.importNumaInfos = {{"1", 0, 0, 1 << 29}, {"1", 0, 1, 1 << 29}};
    MOCKER(&nodeController::UbseNodeController::GetLocalEidBySocket)
        .stubs()
        .with(_, outBound(static_cast<uint32_t>(0x444)))
        .will(returnValue(static_cast<uint32_t>(0)));

    RmObmmExecutor::GetInstance().obmmExportFunc = [](const size_t length[OBMM_MAX_LOCAL_NUMA_NODES],
                                                      unsigned long flags, struct obmm_mem_desc* desc) {
        return TestUbseMmiModule::mockMemId_.fetch_add(1);
    };
    auto ret = module.UbseMemNumaExportExecutor(exportobj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemNumaImportExecutor_Success)
{
    GTEST_SKIP();
    UbseMmiModule module;
    std::vector<UbseMemObmmInfo> desc{};
    desc.resize(8);
    UbseMemNumaBorrowImportObj importObj;
    importObj.exportObmmInfo = desc;
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.owner = {.uid = 0, .gid = 0, .pid = 114514U, .mode = 0600};
    importObj.req.lenderLocs = {{"2", 0}, {"2", 1}};
    importObj.req.lenderSizes = {1 << 29, 1 << 29};
    importObj.req.size = 1 << 30;
    importObj.req.udsInfo = {.uid = 0, .gid = 0, .pid = 114514U};
    importObj.req.name = "UbseMemNumaImportExecutor_Success";
    importObj.req.srcSocket = 0;
    importObj.req.srcNuma = 0;
    importObj.req.lowWatermark = 11;
    importObj.req.highWatermark = 88;

    importObj.algoResult.attachSocketId = 0;
    importObj.algoResult.exportNumaInfos = {{"2", 0, 0, 1 << 29}, {"2", 0, 1, 1 << 29}};
    importObj.algoResult.importNumaInfos = {{"1", 0, 0, 1 << 29}, {"1", 0, 1, 1 << 29}};
    RmObmmExecutor::GetInstance().obmmImportFunc = [](const struct obmm_mem_desc* desc, unsigned long flags,
                                                      int base_dist, int* numa) {
        return TestUbseMmiModule::mockMemId_.fetch_add(1);
    };

    nodeController::UbseNodeInfo node1;
    node1.nodeId = "node01";
    node1.slotId = 1;
    node1.hostName = "server01";
    node1.comIp = "192.168.10.11";
    node1.ipList = {};
    node1.numaInfos = {};
    nodeController::UbseCpuLocation location{"1", 1};
    nodeController::UbseCpuLocation location2{"2", 2};
    nodeController::UbseCpuLocation location3{"3", 3};
    nodeController::UbseCpuLocation location4{"4", 4};
    nodeController::UbseCpuInfo info{};
    nodeController::UbsePortInfo port{};
    port.portId = "1";
    port.ifName = "ifName";
    port.portRole = "master";
    port.portStatus = nodeController::PortStatus::UP;
    port.remoteSlotId = "2";
    port.remoteChipId = "0";
    port.remoteCardId = "0";
    port.remotePortId = "0";
    port.remoteIfName = "remoteIf";
    info.portInfos["1"] = port;
    node1.cpuInfos = {{location, info}, {location2, info}, {location3, info}, {location4, info}};

    node1.localState = nodeController::UbseNodeLocalState::UBSE_NODE_READY;
    node1.clusterState = nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;

    std::vector<mem_id> memIdList = {1};
    MOCKER(&nodeController::UbseNodeController::GetCurNode).stubs().will(returnValue(node1));
    MOCKER(&RmObmmExecutor::ObmmImport,
           std::vector<mem_id>(RmObmmExecutor::*)(const std::vector<UbseMemObmmInfo>& desc, ObmmOpParam& opParam,
                                                  UbseMemImportStatus& status, int* numa))
        .stubs()
        .will(returnValue(memIdList));
    MOCKER(&MemInstanceInnerCommon::GetNuma).stubs().will(returnValue(5));
    auto ret = module.UbseMemNumaImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemNumaUnExportExecutor_Success)
{
    UbseMmiModule module;
    UbseMemNumaBorrowExportObj exportObj;

    exportObj.req.srcSocket = 0;
    exportObj.req.srcNuma = 0;
    exportObj.req.lowWatermark = 11;
    exportObj.req.highWatermark = 88;
    exportObj.req.distance = UbseMemDistance::MEM_DISTANCE_L0;
    exportObj.req.owner = {.uid = 0, .gid = 0, .pid = 114514U, .mode = 0600};
    exportObj.req.size = 1 << 30;
    exportObj.req.lenderLocs = {{"2", 0}, {"2", 1}};
    exportObj.req.lenderSizes = {1 << 29, 1 << 29};
    exportObj.req.udsInfo = {.uid = 0, .gid = 0, .pid = 114514U};
    exportObj.req.requestNodeId = "1";
    exportObj.req.importNodeId = "1";
    exportObj.req.name = "UbseMemUnExportExecutor_Success";
    exportObj.algoResult.attachSocketId = 0;
    exportObj.algoResult.exportNumaInfos = {{"2", 0, 0, 1 << 29}, {"2", 0, 1, 1 << 29}};
    exportObj.algoResult.importNumaInfos = {{"1", 0, 0, 1 << 29}, {"1", 0, 1, 1 << 29}};
    exportObj.status.state = UBSE_MEM_STATE_SUCCEEDED;
    exportObj.status.errCode = 0;
    exportObj.status.expectState = UBSE_MEM_STATE_DESTROYING;
    exportObj.status.exportObmmInfo = {{1, {}}, {2, {}}, {3, {}}, {4, {}}, {5, {}}, {6, {}}, {7, {}}, {8, {}}};
    RmObmmExecutor::GetInstance().obmmUnexportFunc = [](mem_id id, unsigned long flags) {
        return 0;
    };
    auto ret = module.UbseMemNumaUnExportExecutor(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemNumaUnImportExecutor_Success)
{
    UbseMmiModule module;

    UbseMemNumaBorrowImportObj importObj;

    importObj.req.srcSocket = 0;
    importObj.req.srcNuma = 0;
    importObj.req.lowWatermark = 11;
    importObj.req.highWatermark = 88;
    importObj.req.distance = UbseMemDistance::MEM_DISTANCE_L0;
    importObj.req.owner = {.uid = 0, .gid = 0, .pid = 114514U, .mode = 0600};
    importObj.req.size = 1 << 30;
    importObj.req.lenderLocs = {{"2", 0}, {"2", 1}};
    importObj.req.lenderSizes = {1 << 29, 1 << 29};
    importObj.req.udsInfo = {.uid = 0, .gid = 0, .pid = 114514U};
    importObj.req.requestNodeId = "1";
    importObj.req.importNodeId = "1";
    importObj.req.name = "UbseMemNumaUnImportExecutor_Success";
    importObj.algoResult.attachSocketId = 0;
    importObj.algoResult.exportNumaInfos = {{"2", 0, 0, 1 << 29}, {"2", 0, 1, 1 << 29}};
    importObj.algoResult.importNumaInfos = {{"1", 0, 0, 1 << 29}, {"1", 0, 1, 1 << 29}};
    importObj.status.state = UBSE_MEM_STATE_SUCCEEDED;
    importObj.status.errCode = 0;
    importObj.status.expectState = UBSE_MEM_STATE_DESTROYING;
    importObj.status.scna = 0x401;
    importObj.status.importResults = {{1, -1}, {2, -1}, {3, -1}, {4, -1}, {5, -1}, {6, -1}, {7, -1}, {8, -1}};
    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
        return 0;
    };
    auto ret = module.UbseMemNumaUnImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

uint64_t MockReadAgentLocalObmmMetaData(uint64_t taskId,
                                        std::vector<UbseMemLocalObmmMetaData>& ubseMemLocalObmmMetaDatas,
                                        bool& lastPage)
{
    UbseMemLocalObmmMetaData data{};

    data.memIdType = 0;
    data.remoteNumaId = 3; // 3
    data.usedPidCount = 0;
    data.totalSize = 134217728; // 134217728b 128M
    data.localMemId = 2;        // 2
    data.localNodeId = "1";
    data.usedPidCount = 0;
    ubseMemLocalObmmMetaDatas.emplace_back(data);
    lastPage = true;
    return 0;
}

TEST_F(TestUbseMmiModule, UbseMemUbseMemGetObjData_Success)
{
    UbseMmiModule module;
    NodeMemDebtInfo debtInfo{};

    MOCKER(&RmObmmMetaRestore::ReadAgentLocalObmmMetaData).stubs().will(invoke(MockReadAgentLocalObmmMetaData));

    auto ret = module.UbseMemGetObjData(debtInfo);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemFdImportPermissionExecutor_Success)
{
    UbseMmiModule module;
    UbseMemFdBorrowImportObj importObj{};
    MOCKER(&MemInstanceInnerFdBorrow::MemFdImportPermissionExecutor).stubs().will(returnValue(UBSE_OK));
    auto ret = module.UbseMemFdImportPermissionExecutor(importObj);
    EXPECT_NE(ret, UBSE_OK);

    UbseMemDebtNumaInfo info{};
    importObj.algoResult.importNumaInfos.emplace_back(info);
    importObj.algoResult.exportNumaInfos.emplace_back(info);
    importObj.req.name = "UbseMemFdImportPermissionExecutor_Success";
    importObj.req.size = 1024;

    ret = module.UbseMemFdImportPermissionExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemShmImportExecutor_Success)
{
    UbseMmiModule module;
    UbseMemShareBorrowImportObj importObj{};
    MOCKER(&MemInstanceInnerShm::MemShmImportExecutor).stubs().will(returnValue(UBSE_OK));
    auto ret = module.UbseMemShmImportExecutor(importObj);
    EXPECT_NE(ret, UBSE_OK);

    UbseMemDebtNumaInfo info{};
    importObj.algoResult.importNumaInfos.emplace_back(info);
    importObj.algoResult.exportNumaInfos.emplace_back(info);
    importObj.req.name = "UbseMemShmImportExecutor_Success";
    importObj.req.size = 1024;

    ret = module.UbseMemShmImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemShmUnImportExecutor_Success)
{
    UbseMmiModule module;
    UbseMemShareBorrowImportObj importObj{};
    MOCKER(&MemInstanceInnerShm::MemShmUnImportExecutor).stubs().will(returnValue(UBSE_OK));
    auto ret = module.UbseMemShmUnImportExecutor(importObj);
    EXPECT_NE(ret, UBSE_OK);

    UbseMemDebtNumaInfo info{};
    importObj.algoResult.importNumaInfos.emplace_back(info);
    importObj.algoResult.exportNumaInfos.emplace_back(info);
    importObj.req.name = "UbseMemShmUnImportExecutor_Success";
    importObj.req.size = 1024;

    ret = module.UbseMemShmUnImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemShmExportExecutor_Success)
{
    UbseMmiModule module;
    UbseMemShareBorrowExportObj exportObj{};
    MOCKER(&MemInstanceInnerShm::MemShmExportExecutor).stubs().will(returnValue(UBSE_OK));
    auto ret = module.UbseMemShmExportExecutor(exportObj);
    EXPECT_NE(ret, UBSE_OK);

    UbseMemDebtNumaInfo info{};
    exportObj.algoResult.importNumaInfos.emplace_back(info);
    exportObj.algoResult.exportNumaInfos.emplace_back(info);
    exportObj.req.name = "UbseMemShmExportExecutor_Success";
    exportObj.req.size = 1024;

    ret = module.UbseMemShmExportExecutor(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemShmUnExportExecutor_Success)
{
    UbseMmiModule module;
    UbseMemShareBorrowExportObj exportObj{};
    MOCKER(&MemInstanceInnerShm::MemShmUnExportExecutor).stubs().will(returnValue(UBSE_OK));
    auto ret = module.UbseMemShmUnExportExecutor(exportObj);
    EXPECT_NE(ret, UBSE_OK);

    UbseMemDebtNumaInfo info{};
    exportObj.algoResult.importNumaInfos.emplace_back(info);
    exportObj.algoResult.exportNumaInfos.emplace_back(info);
    exportObj.req.name = "UbseMemShmUnExportExecutor_Success";
    exportObj.req.size = 1024;

    ret = module.UbseMemShmUnExportExecutor(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemAddrImportExecutor_Success)
{
    UbseMmiModule module;
    UbseMemAddrBorrowImportObj importObj{};
    UbseMemDebtNumaInfo info{};
    importObj.algoResult.importNumaInfos.emplace_back(info);
    importObj.algoResult.exportNumaInfos.emplace_back(info);
    importObj.req.name = "UbseMemAddrImportExecutor_Success";
    MOCKER(&MemInstanceInnerAddrBorrow::MemAddrImportExecutor).stubs().will(returnValue(UBSE_OK));
    auto ret = module.UbseMemAddrImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemAddrUnImportExecutor_Success)
{
    UbseMmiModule module;
    UbseMemAddrBorrowImportObj importObj{};
    UbseMemDebtNumaInfo info{};
    importObj.algoResult.importNumaInfos.emplace_back(info);
    importObj.algoResult.exportNumaInfos.emplace_back(info);
    importObj.req.name = "UbseMemAddrUnImportExecutor_Success";
    MOCKER(&MemInstanceInnerAddrBorrow::MemAddrUnImportExecutor).stubs().will(returnValue(UBSE_OK));
    auto ret = module.UbseMemAddrUnImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemAddrExportExecutor_Success)
{
    UbseMmiModule module;
    UbseMemAddrBorrowExportObj exportObj{};
    UbseMemDebtNumaInfo info{};
    exportObj.algoResult.importNumaInfos.emplace_back(info);
    exportObj.algoResult.exportNumaInfos.emplace_back(info);
    exportObj.req.name = "UbseMemAddrExportExecutor_Success";
    MOCKER(&MemInstanceInnerAddrBorrow::MemAddrExportExecutor).stubs().will(returnValue(UBSE_OK));
    auto ret = module.UbseMemAddrExportExecutor(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemAddrUnExportExecutor_Success)
{
    UbseMmiModule module;
    UbseMemAddrBorrowExportObj exportObj{};
    UbseMemDebtNumaInfo info{};
    exportObj.algoResult.importNumaInfos.emplace_back(info);
    exportObj.algoResult.exportNumaInfos.emplace_back(info);
    exportObj.req.name = "UbseMemAddrUnExportExecutor_Success";
    MOCKER(&MemInstanceInnerAddrBorrow::MemAddrUnExportExecutor).stubs().will(returnValue(UBSE_OK));
    auto ret = module.UbseMemAddrUnExportExecutor(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMmiPreOnline_Success)
{
    UbseMmiModule module;
    std::vector<SocketCnaInfo> cnaTopoInfos{};
    uint64_t preImportSize = 1024;
    MOCKER(&MemInstanceInnerCommon::MemPreOnline).stubs().will(returnValue(UBSE_OK));
    auto ret = module.UbseMmiPreOnline(cnaTopoInfos, preImportSize);
    EXPECT_EQ(ret, UBSE_OK);

    SocketCnaInfo info{};
    cnaTopoInfos.emplace_back(info);
    ret = module.UbseMmiPreOnline(cnaTopoInfos, preImportSize);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMmiUnPreOnline_Success)
{
    UbseMmiModule module;
    MOCKER(&MemInstanceInnerCommon::MemUnPreOnline).stubs().will(returnValue(UBSE_OK));
    auto ret = module.UbseMmiUnPreOnline();
    EXPECT_EQ(ret, UBSE_OK);
}
} // namespace ubse::ut::mmi