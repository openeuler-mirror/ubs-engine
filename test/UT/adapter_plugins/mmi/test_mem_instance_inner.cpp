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
#include "test_mem_instance_inner.h"

#include <securec.h>

#include "ubse_file_util.h"
#include "ubse_mem_instance_inner.h"
#include "ubse_node_controller.h"
#include "ubse_obmm_executor.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_decoder_utils.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_prehandle_manager.h"

namespace ubse::ut::mmi {
using namespace ubse::mmi;
using namespace ubse::utils;
using namespace ubse::nodeController;

std::vector<mem_id> MockObmmImportReturnEmpty(RmObmmExecutor* mockClass, const std::vector<UbseMemObmmInfo>& desc,
                                              ObmmOpParam& opParam, UbseMemImportStatus& status, int* numa)
{
    return {};
}

std::vector<mem_id> MockObmmImportReturnMemIds(RmObmmExecutor* mockClass, const std::vector<UbseMemObmmInfo>& desc,
                                               ObmmOpParam& opParam, UbseMemImportStatus& status, int* numa)
{
    return {1};
}

UbseResult MockGetPreOnlineInfo(std::vector<BasicPreImportInfo>& basicPreImportInfos)
{
    BasicPreImportInfo info{};
    info.preOnlineSize = 1024;
    basicPreImportInfos.emplace_back(info);
    return UBSE_OK;
}

uint32_t MockGetBlockSize(uint64_t& blockSize)
{
    blockSize = 128;
    return UBSE_OK;
}

TEST_F(TestMemInstanceInner, ExportRollback_Success)
{
    RmObmmExecutor::GetInstance().obmmUnexportFunc = [](mem_id id, unsigned long flags) {
        return 0;
    };
    EXPECT_NO_THROW(MemInstanceInnerCommon::GetInstance().RollbackExport({1, 2, 3, 4, 5}));
}

TEST_F(TestMemInstanceInner, ImportRollback_Success)
{
    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
        return 0;
    };
    EXPECT_NO_THROW(MemInstanceInnerCommon::GetInstance().RollbackImport({1, 2, 3, 4, 5}));
}

TEST_F(TestMemInstanceInner, MemFdImportPermissionExecutor_Success)
{
    MOCKER(&UbseContext::GetModule<UbseSecurityModule>)
        .stubs()
        .will(returnValue(std::make_shared<UbseSecurityModule>()));
    MOCKER(&UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_OK));
    MOCKER(&UbseFileUtil::CheckFileExists).stubs().will(returnValue(true));
    MOCKER(&UbseFileUtil::SetFileAttributes).stubs().will(returnValue(true));
    UbseMemFdBorrowImportObj importObj{};
    std::vector<UbseMemImportResult> results{};
    UbseMemImportResult result{};
    result.memId = 1;
    results.emplace_back(result);
    importObj.status.importResults = results;
    auto ret = MemInstanceInnerFdBorrow::GetInstance().MemFdImportPermissionExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestMemInstanceInner, MemShmImportExecutor_Success)
{
    UbseMemShareBorrowImportObj importObj{};
    UbseMemDebtNumaInfo numaInfo{};
    numaInfo.nodeId = 1;
    importObj.algoResult.exportNumaInfos.emplace_back(numaInfo);
    importObj.importNodeId = 1;
    UbseMemObmmInfo obmmInfo{};
    importObj.exportObmmInfo.emplace_back(obmmInfo);
    importObj.status.decoderResult.push_back({0, 0, 0});
    auto ret = MemInstanceInnerShm::GetInstance().MemShmImportExecutor(importObj);
    EXPECT_NE(ret, UBSE_OK);

    MOCKER(&RmObmmExecutor::BaseObmmDevChangeUidGid).stubs().will(returnValue(true));
    ret = MemInstanceInnerShm::GetInstance().MemShmImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);

    importObj.importNodeId = 2;
    ret = MemInstanceInnerShm::GetInstance().MemShmImportExecutor(importObj);
    EXPECT_NE(ret, UBSE_OK);

    MOCKER(GetCustomMetaFromShmImportObj).stubs().will(returnValue(UBSE_OK));
    ret = MemInstanceInnerShm::GetInstance().MemShmImportExecutor(importObj);
    EXPECT_NE(ret, UBSE_OK);

    std::vector<mem_id> memIds{};
    MOCKER(&RmObmmExecutor::ObmmImport,
           std::vector<mem_id>(RmObmmExecutor::*)(const std::vector<UbseMemObmmInfo>& desc, ObmmOpParam& opParam,
                                                  UbseMemImportStatus& status, int* numa))
        .stubs()
        .will(invoke(MockObmmImportReturnEmpty));
    ret = MemInstanceInnerShm::GetInstance().MemShmImportExecutor(importObj);
    EXPECT_NE(ret, UBSE_OK);

    MOCKER(&RmObmmExecutor::ObmmImport,
           std::vector<mem_id>(RmObmmExecutor::*)(const std::vector<UbseMemObmmInfo>& desc, ObmmOpParam& opParam,
                                                  UbseMemImportStatus& status, int* numa))
        .reset();
    MOCKER(&RmObmmExecutor::ObmmImport,
           std::vector<mem_id>(RmObmmExecutor::*)(const std::vector<UbseMemObmmInfo>& desc, ObmmOpParam& opParam,
                                                  UbseMemImportStatus& status, int* numa))
        .stubs()
        .will(invoke(MockObmmImportReturnMemIds));
    ret = MemInstanceInnerShm::GetInstance().MemShmImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestMemInstanceInner, MemShmUnImportExecutor_Success)
{
    UbseMemShareBorrowImportObj importObj{};
    importObj.realExe = false;
    UbseMemImportResult result{};
    result.memId = 1;
    importObj.status.importResults.emplace_back(result);
    auto ret = MemInstanceInnerShm::GetInstance().MemShmUnImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);

    importObj.realExe = true;
    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
        errno = EIO;
        return -1;
    };
    ret = MemInstanceInnerShm::GetInstance().MemShmUnImportExecutor(importObj);
    EXPECT_NE(ret, UBSE_OK);

    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
        return 0;
    };
    ret = MemInstanceInnerShm::GetInstance().MemShmUnImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestMemInstanceInner, MemShmExportExecutor_Success)
{
    UbseMemShareBorrowExportObj exportObj{};
    exportObj.req.size = 128;
    UbseMemDebtNumaInfo numaInfo{};
    numaInfo.size = 128;
    numaInfo.numaId = 1;
    numaInfo.nodeId = 1;
    exportObj.algoResult.exportNumaInfos.emplace_back(numaInfo);
    exportObj.algoResult.importNumaInfos.emplace_back(numaInfo);
    exportObj.req.shmRegion.nodeNum = 1;
    ubse::adapter_plugins::mmi::UbseNodeInfo nodeInfo{};
    nodeInfo.index = 1;
    exportObj.req.shmRegion.nodelist.emplace_back(nodeInfo);
    std::vector<mem_id> memIds{};
    MOCKER(&RmObmmExecutor::ObmmExport,
           std::vector<mem_id>(RmObmmExecutor::*)(size_t size[MAX_NUMA_NODES], int arraySize, ObmmOpParam& opParam,
                                                  std::vector<ubse_mem_obmm_mem_desc>& desc, uint64_t blockSize))
        .stubs()
        .will(returnValue(memIds));
    auto ret = MemInstanceInnerShm::GetInstance().MemShmExportExecutor(exportObj);
    EXPECT_NE(ret, UBSE_OK);

    memIds.emplace_back(1);
    MOCKER(&RmObmmExecutor::ObmmExport,
           std::vector<mem_id>(RmObmmExecutor::*)(size_t size[MAX_NUMA_NODES], int arraySize, ObmmOpParam& opParam,
                                                  std::vector<ubse_mem_obmm_mem_desc>& desc, uint64_t blockSize))
        .reset();
    MOCKER(&RmObmmExecutor::ObmmExport,
           std::vector<mem_id>(RmObmmExecutor::*)(size_t size[MAX_NUMA_NODES], int arraySize, ObmmOpParam& opParam,
                                                  std::vector<ubse_mem_obmm_mem_desc>& desc, uint64_t blockSize))
        .stubs()
        .will(returnValue(memIds));
    ret = MemInstanceInnerShm::GetInstance().MemShmExportExecutor(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestMemInstanceInner, MemAddrImportExecutor_Success)
{
    UbseMemAddrBorrowImportObj importObj{};
    auto ret = UBSE_OK;

    UbseMemDebtNumaInfo numaInfo{};
    importObj.algoResult.exportNumaInfos.emplace_back(numaInfo);
    importObj.algoResult.importNumaInfos.emplace_back(numaInfo);
    UbseMemObmmInfo obmmInfo{};
    importObj.exportObmmInfo.emplace_back(obmmInfo);
    UbseMemAddrInfo addrInfo{};
    importObj.status.decoderResult.push_back({0, 0, 0});
    importObj.req.exportAddrList.emplace_back(addrInfo);
    mem_id memId = 0;
    MOCKER(&RmObmmExecutor::ObmmImport,
           mem_id(RmObmmExecutor::*)(const ubse_mem_obmm_mem_desc& desc, const ObmmOpParam& opParam, int* numa))
        .stubs()
        .will(returnValue(memId));
    ret = MemInstanceInnerAddrBorrow::GetInstance().MemAddrImportExecutor(importObj);
    EXPECT_NE(ret, UBSE_OK);

    memId = 1;
    MOCKER(&RmObmmExecutor::ObmmImport,
           mem_id(RmObmmExecutor::*)(const ubse_mem_obmm_mem_desc& desc, const ObmmOpParam& opParam, int* numa))
        .reset();
    MOCKER(&RmObmmExecutor::ObmmImport,
           mem_id(RmObmmExecutor::*)(const ubse_mem_obmm_mem_desc& desc, const ObmmOpParam& opParam, int* numa))
        .stubs()
        .will(returnValue(memId));

    ret = MemInstanceInnerAddrBorrow::GetInstance().MemAddrImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestMemInstanceInner, MemAddrUnImportExecutor_Success)
{
    UbseMemAddrBorrowImportObj importObj{};
    UbseMemImportResult result{};
    importObj.status.importResults.emplace_back(result);

    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
        errno = EIO;
        return -1;
    };
    auto ret = MemInstanceInnerAddrBorrow::GetInstance().MemAddrUnImportExecutor(importObj);
    EXPECT_NE(ret, UBSE_OK);

    RmObmmExecutor::GetInstance().obmmUnimportFunc = [](unsigned long id, unsigned long flags) {
        return 0;
    };
    ret = MemInstanceInnerAddrBorrow::GetInstance().MemAddrUnImportExecutor(importObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestMemInstanceInner, MemAddrExportExecutor_Success)
{
    UbseMemAddrBorrowExportObj exportObj{};
    exportObj.req.exportAddrList.emplace_back();
    UbseMemDebtNumaInfo numaInfo{};
    exportObj.algoResult.exportNumaInfos.emplace_back(numaInfo);
    exportObj.algoResult.importNumaInfos.emplace_back(numaInfo);

    auto ret = MemInstanceInnerAddrBorrow::GetInstance().MemAddrExportExecutor(exportObj);
    EXPECT_NE(ret, UBSE_OK);

    MOCKER(&UbseNodeController::GetLocalEidBySocket).stubs().will(returnValue(UBSE_OK));
    MOCKER(&memcpy_s).stubs().will(returnValue(-1));
    ret = MemInstanceInnerAddrBorrow::GetInstance().MemAddrExportExecutor(exportObj);
    EXPECT_NE(ret, UBSE_OK);

    MOCKER(&RmObmmExecutor::ObmmExportPid).stubs().will(returnValue(UBSE_OK));
    ret = MemInstanceInnerAddrBorrow::GetInstance().MemAddrExportExecutor(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestMemInstanceInner, MemAddrUnExportExecutor_Success)
{
    UbseMemAddrBorrowExportObj exportObj{};
    UbseMemObmmInfo obmmInfo{};
    obmmInfo.memId = 1;
    exportObj.status.exportObmmInfo.emplace_back(obmmInfo);
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(const std::vector<mem_id>& id)).reset();
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(const std::vector<mem_id>& id))
        .stubs()
        .will(returnValue(UBSE_OK));
    auto ret = MemInstanceInnerAddrBorrow::GetInstance().MemAddrUnExportExecutor(exportObj);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestMemInstanceInner, AddAddrRemoteNuma_Success)
{
    EXPECT_NO_THROW(MemInstanceInnerAddrBorrow::GetInstance().AddAddrRemoteNuma(5));
}

TEST_F(TestMemInstanceInner, MemPreOnline_Success)
{
    GTEST_SKIP();
    SocketCnaInfo cnaInfo{};
    cnaInfo.exportNodeId = "1";
    cnaInfo.exportSocketId = 1;
    std::vector<SocketCnaInfo> cnaTopoInfos{};
    cnaTopoInfos.emplace_back(cnaInfo);
    uint64_t preImportSize = 1024;
    ubse::nodeController::UbseNodeInfo nodeInfo{};
    nodeInfo.nodeId = "1";
    UbseCpuInfo cpuInfo{};
    UbsePortInfo portInfo{};
    portInfo.portStatus = PortStatus::UP;
    portInfo.portId = "1";
    cpuInfo.portInfos.emplace("1", portInfo);
    UbseCpuLocation cpuLocation{};
    cpuLocation.nodeId = "1";
    cpuLocation.chipId = 1;
    nodeInfo.cpuInfos.emplace(cpuLocation, cpuInfo);
    ubse::nodeController::UbseNumaLocation numaLocation{};
    UbseNumaInfo numaInfo{};
    nodeInfo.numaInfos.emplace(numaLocation, numaInfo);
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos{};
    nodeInfos.emplace("1", nodeInfo);
    MOCKER(&UbseNodeController::GetCurNode).stubs().will(returnValue(nodeInfo));
    MOCKER(&RmObmmUtils::GetPreOnlineInfo).stubs().will(invoke(MockGetPreOnlineInfo));
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfos));
    auto ret = MemInstanceInnerCommon::GetInstance().MemPreOnline(cnaTopoInfos, preImportSize);
    EXPECT_NE(ret, UBSE_OK);

    MOCKER(&RmObmmExecutor::ObmmPreImport).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::mem::decoder::utils::MemDecoderUtils::PreImportDecoderEntry).stubs().will(returnValue(UBSE_OK));
    MOCKER(&ubse::mem::decoder::utils::UbseMemPrehandleManager::InitPreHandle).stubs().will(returnValue(UBSE_OK));
    ret = MemInstanceInnerCommon::GetInstance().MemPreOnline(cnaTopoInfos, preImportSize);
    EXPECT_EQ(ret, UBSE_OK);
}

TEST_F(TestMemInstanceInner, MemUnPreOnline_Success)
{
    MOCKER(&RmObmmUtils::GetPreOnlineInfo).stubs().will(invoke(MockGetPreOnlineInfo));
    auto ret = MemInstanceInnerCommon::GetInstance().MemUnPreOnline();
    EXPECT_NE(ret, UBSE_OK);
    MOCKER(&RmObmmExecutor::ObmmUnPreImport).stubs().will(returnValue(UBSE_OK));
    ret = MemInstanceInnerCommon::GetInstance().MemUnPreOnline();
    EXPECT_EQ(ret, UBSE_OK);
}
} // namespace ubse::ut::mmi