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
#include "mem_instance_inner.h"
#include "mmi_stub.h"
#include "ubse_node_controller.h"
#define private public
#include "ubse_obmm_executor.h"
#undef private
namespace ubse::ut::mmi {
TEST_F(TestUbseMmiModule, UbseMemFdImportExecutor_Success)
{
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
    RmObmmExecutor::GetInstance().obmmImportFunc = [](const struct obmm_mem_desc *desc, unsigned long flags,
                                                      int base_dist, int *numa) {
        return gMockMemId_.fetch_add(1);
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
    RmObmmExecutor::GetInstance().blockSize = 1 << 27;
    MOCKER(&RmObmmUtils::GetBlockSize)
        .stubs()
        .with(outBound(static_cast<uint64_t>(1) << 27))
        .will(returnValue(static_cast<uint32_t>(0)));
    MOCKER(&nodeController::UbseNodeController::GetLocalEidBySocket).stubs().with(
        _, outBound(static_cast<uint32_t>(0x444))).will(
        returnValue(static_cast<uint32_t>(0)));
    RmObmmExecutor::GetInstance().obmmExportFunc = [](const size_t length[OBMM_MAX_LOCAL_NUMA_NODES],
                                                      unsigned long flags,
                                                      struct obmm_mem_desc *desc) {
        return gMockMemId_.fetch_add(1);
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
    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](mem_id id, unsigned long flags) {
        return 0;
    };
    auto ret = module.UbseMemFdUnImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemNumaExportExecutor_Success)
{
    UbseMmiModule module;
    UbseMemNumaBorrowExportObj exportobj;

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
    RmObmmExecutor::GetInstance().blockSize = 1 << 27;

    MOCKER(&RmObmmUtils::GetBlockSize).stubs().with(outBound(static_cast<uint64_t>(1) << 27)).will(
        returnValue(static_cast<uint32_t>(0)));
    MOCKER(&nodeController::UbseNodeController::GetLocalEidBySocket).stubs().with(
        _, outBound(static_cast<uint32_t>(0x444))).will(returnValue(static_cast<uint32_t>(0)));

    RmObmmExecutor::GetInstance().obmmExportFunc = [](const size_t length[OBMM_MAX_LOCAL_NUMA_NODES],
                                                      unsigned long flags,
                                                      struct obmm_mem_desc *desc) {
        return gMockMemId_.fetch_add(1);
    };
    auto ret = module.UbseMemNumaExportExecutor(exportobj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestUbseMmiModule, UbseMemNumaImportExecutor_Success)
{
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
    RmObmmExecutor::GetInstance().obmmImportFunc = [](const struct obmm_mem_desc *desc, unsigned long flags,
                                                      int base_dist, int *numa) {
        return gMockMemId_.fetch_add(1);
    };
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
    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](mem_id id, unsigned long flags) {
        return 0;
    };
    auto ret = module.UbseMemNumaUnImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

uint64_t MockReadAgentLocalObmmMetaData(uint64_t taskId,
                                        std::vector<UbseMemLocalObmmMetaData> &ubseMemLocalObmmMetaDatas,
                                        bool &lastPage)
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
}