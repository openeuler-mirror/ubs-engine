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
#include <sstream>

#include "ubse_file_util.h"
#include "ubse_logger.h"
#include "ubse_mem_instance_inner.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller.h"
#include "ubse_obmm_executor.h"
#include "ubse_security_module.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_decoder_utils.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_prehandle_manager.h"

namespace ubse::mmi {
UbseResult GetDcna(const nodeController::UbsePortInfo portInfo, const adapter_plugins::mmi::SocketCnaInfo cnaTopoInfo,
                   std::vector<obmm_preimport_info>& obmmPreImportInfos,
                   std::vector<PreImportHandleRecord>& preImportHandleRecords, uint64_t preImportSize);
UbseResult PreOnlineHandler(const std::vector<adapter_plugins::mmi::SocketCnaInfo>& cnaTopoInfos,
                            uint64_t preImportSize);
} // namespace ubse::mmi

namespace ubse::ut::mmi {
using namespace ubse::mmi;
using namespace ubse::utils;
using namespace ubse::nodeController;

static int gAddrImportCount = 0;
static int gUnPreImportCount = 0;
static std::vector<mem_id> gUnimportMemIds{};
static std::vector<std::pair<ubse::log::UbseLogLevel, std::string>> gSwitchLogEntries{};

static bool CaptureSwitchLog(ubse::log::UbseLog*, ubse::log::UbseLoggerEntry& entry)
{
    std::ostringstream out;
    entry.OutPutLog(out);
    gSwitchLogEntries.emplace_back(entry.GetLogLevel(), out.str());
    return true;
}

mem_id MockAddrImportWithUpdatedNuma(RmObmmExecutor* mockClass, const ubse_mem_obmm_mem_desc& desc,
                                     const ObmmOpParam& opParam, int* numa)
{
    if (gAddrImportCount++ == 0) {
        *numa = 100;
        return 1;
    }
    *numa = INVALID_NUMAID;
    return 2;
}

UbseResult MockGetDcnaWithPreImport(const UbsePortInfo portInfo, const SocketCnaInfo cnaTopoInfo,
                                    std::vector<obmm_preimport_info>& obmmPreImportInfos,
                                    std::vector<PreImportHandleRecord>& preImportHandleRecords, uint64_t preImportSize)
{
    mem::decoder::utils::DecoderEntryLoc loc{1, 2, 3, 0};
    obmmPreImportInfos.emplace_back();
    preImportHandleRecords.push_back({loc, 4});
    return UBSE_OK;
}

UbseResult MockObmmUnImport(RmObmmExecutor* mockClass, mem_id memId)
{
    gUnimportMemIds.push_back(memId);
    return UBSE_OK;
}

UbseResult MockObmmUnPreImport(RmObmmExecutor* mockClass, obmm_preimport_info* preImportInfo, unsigned long flags)
{
    ++gUnPreImportCount;
    return UBSE_OK;
}
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::context;

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

TEST_F(TestMemInstanceInner, MemAddrImportExecutor_RollbackUpdatedNumaAndCurrentMemId)
{
    auto& addrBorrow = MemInstanceInnerAddrBorrow::GetInstance();
    auto oldRemoteNumaIds = addrBorrow.addrRemoteNumaIdSet;
    addrBorrow.addrRemoteNumaIdSet.clear();
    UbseMemAddrBorrowImportObj importObj{};
    importObj.exportObmmInfo.resize(2);
    importObj.req.exportAddrList.resize(2);
    importObj.status.decoderResult.resize(2);
    importObj.algoResult.exportNumaInfos.emplace_back();
    importObj.algoResult.importNumaInfos.emplace_back();
    gAddrImportCount = 0;
    gUnimportMemIds.clear();
    MOCKER(&RmObmmExecutor::ObmmImport,
           mem_id(RmObmmExecutor::*)(const ubse_mem_obmm_mem_desc& desc, const ObmmOpParam& opParam, int* numa))
        .stubs()
        .will(invoke(MockAddrImportWithUpdatedNuma));
    MOCKER(&RmObmmExecutor::ObmmUnImport, UbseResult(RmObmmExecutor::*)(mem_id id))
        .stubs()
        .will(invoke(MockObmmUnImport));

    EXPECT_EQ(addrBorrow.MemAddrImportExecutor(importObj), UBSE_MMI_OBMM_OP_FAILED);
    EXPECT_TRUE(addrBorrow.addrRemoteNumaIdSet.empty());
    EXPECT_EQ(gUnimportMemIds, (std::vector<mem_id>{1, 2}));
    addrBorrow.addrRemoteNumaIdSet = std::move(oldRemoteNumaIds);
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

TEST_F(TestMemInstanceInner, GetDcna_InvalidNuma)
{
    auto& common = MemInstanceInnerCommon::GetInstance();
    auto oldRemoteNumaMap = common.mRemoteNumaMap;
    common.mRemoteNumaMap.clear();
    UbsePortInfo portInfo{};
    portInfo.portId = "1";
    SocketCnaInfo cnaInfo{};
    std::vector<obmm_preimport_info> obmmPreImportInfos{};
    std::vector<PreImportHandleRecord> preImportHandleRecords{};

    EXPECT_EQ(GetDcna(portInfo, cnaInfo, obmmPreImportInfos, preImportHandleRecords, 1024), UBSE_ERROR_INVAL);
    common.mRemoteNumaMap = std::move(oldRemoteNumaMap);
}

TEST_F(TestMemInstanceInner, PreOnlineHandler_RollbackOnNodeLookupFailure)
{
    auto& prehandleManager = mem::decoder::utils::UbseMemPrehandleManager::GetInstance();
    prehandleManager.preHandleMap.clear();
    mem::decoder::utils::DecoderEntryLoc preImportLoc{1, 2, 3, 0};
    adapter_plugins::mti::mami::UbseMamiMemImportResult preImportValue{};
    preImportValue.handle = 4;
    prehandleManager.CreatePreHandle(preImportLoc, preImportValue, 100, 1024);
    SocketCnaInfo validCnaInfo{};
    validCnaInfo.importNodeId = "import-node";
    validCnaInfo.exportNodeId = "export-node";
    UbsePortInfo portInfo{};
    portInfo.portId = "1";
    portInfo.portStatus = PortStatus::UP;
    portInfo.remoteSlotId = validCnaInfo.importNodeId;
    portInfo.remoteChipId = "0";
    UbseCpuInfo cpuInfo{};
    cpuInfo.portInfos.emplace(portInfo.portId, portInfo);
    UbseCpuLocation cpuLocation{validCnaInfo.exportNodeId, validCnaInfo.exportSocketId};
    ubse::nodeController::UbseNodeInfo nodeInfo{};
    nodeInfo.cpuInfos.emplace(cpuLocation, cpuInfo);
    std::unordered_map<std::string, ubse::nodeController::UbseNodeInfo> nodeInfos{};
    nodeInfos.emplace(validCnaInfo.exportNodeId, nodeInfo);
    SocketCnaInfo missingNodeCnaInfo{};
    missingNodeCnaInfo.exportNodeId = "missing-node";
    std::vector<SocketCnaInfo> cnaTopoInfos{validCnaInfo, missingNodeCnaInfo};
    gUnPreImportCount = 0;
    MOCKER(&UbseNodeController::GetAllNodes).stubs().will(returnValue(nodeInfos));
    MOCKER(&mem::decoder::utils::MemDecoderUtils::GetChipAndDieId).stubs().will(returnValue(UBSE_OK));
    MOCKER(GetDcna).stubs().will(invoke(MockGetDcnaWithPreImport));
    MOCKER(&RmObmmExecutor::ObmmUnPreImport).stubs().will(invoke(MockObmmUnPreImport));

    EXPECT_EQ(PreOnlineHandler(cnaTopoInfos, 1024), UBSE_ERROR);
    EXPECT_EQ(gUnPreImportCount, 1);
    adapter_plugins::mti::mami::UbseMamiMemImportResult outValue{};
    EXPECT_EQ(prehandleManager.GetPreHandleByDcna(preImportLoc, 100, outValue), UBSE_ERROR);
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

// 故障迁移块级切换：Obj→ObmmSwitchParam 逐字段透传 + 基础参数校验 + end 三态返回
TEST_F(TestMemInstanceInner, MemShmSwitchBeginImportExecutor_FieldRoundTrip)
{
    // 字段透传测试不依赖运行环境实际具有 CAP_SYS_ADMIN。
    MOCKER(&ubse::security::UbseSecurityModule::ModifyEffectiveCapabilities).stubs().will(returnValue(UBSE_OK));
    auto& executor = RmObmmExecutor::GetInstance();
    auto oldBeginFunc = executor.obmmSwitchBeginImportFunc;
    auto oldEndFunc = executor.obmmSwitchEndFunc;

    static struct obmm_switch_import_target captured {
    };
    static uint8_t capturedPriv[OBMM_MAX_PRIV_LEN] = {};
    static unsigned long capturedFlags = 0;
    static mem_id capturedId = 0;
    static uint16_t capturedLen = 0;
    executor.obmmSwitchBeginImportFunc = [](mem_id id, unsigned long flags,
                                            const struct obmm_switch_import_target* target, const void* priv,
                                            uint16_t privLen) {
        captured = *target;
        if (privLen > 0) {
            memcpy_s(capturedPriv, sizeof(capturedPriv), priv, privLen);
        }
        capturedFlags = flags;
        capturedId = id;
        capturedLen = privLen;
        return 0;
    };

    UbseMemShmSwitchObj switchObj{};
    switchObj.name = "migrate-res";
    switchObj.importNodeId = "node-1";
    switchObj.oldImportMemId = 42;
    switchObj.newPa = 0x1234567800ULL;
    switchObj.newScna = 7;
    switchObj.newDcna = 9;
    switchObj.baseDist = 21;
    switchObj.newSeid = {1, 2, 3};
    switchObj.newDeid = {4, 5, 6};
    switchObj.priv = {0xAA, 0xBB, 0xCC};

    EXPECT_EQ(MemInstanceInnerShm::GetInstance().MemShmSwitchBeginImportExecutor(switchObj), UBSE_OK);
    EXPECT_EQ(capturedId, 42u);
    EXPECT_EQ(captured.pa, 0x1234567800ULL);
    EXPECT_EQ(captured.scna, 7u);
    EXPECT_EQ(captured.dcna, 9u);
    EXPECT_EQ(captured.base_dist, 21u);
    EXPECT_EQ(captured.seid[0], 1);
    EXPECT_EQ(captured.seid[1], 2);
    EXPECT_EQ(captured.deid[0], 4);
    EXPECT_EQ(capturedLen, 3u);
    EXPECT_EQ(capturedPriv[0], 0xAA);
    EXPECT_EQ(capturedFlags, OBMM_SWITCH_FLAG_KEEP_OLD_ALIVE);

    // end 透传：success 映射 1/0，返回值直通（三态语义由对接层翻译，已在对接层 UT 覆盖）
    static int capturedSuccess = -1;
    executor.obmmSwitchEndFunc = [](mem_id id, int success) {
        EXPECT_EQ(id, 42u);
        capturedSuccess = success;
        return 0;
    };
    EXPECT_EQ(MemInstanceInnerShm::GetInstance().MemShmSwitchEndExecutor(switchObj, true), UBSE_OK);
    EXPECT_EQ(capturedSuccess, 1);
    EXPECT_EQ(MemInstanceInnerShm::GetInstance().MemShmSwitchEndExecutor(switchObj, false), UBSE_OK);
    EXPECT_EQ(capturedSuccess, 0);

    executor.obmmSwitchBeginImportFunc = oldBeginFunc;
    executor.obmmSwitchEndFunc = oldEndFunc;
}

TEST_F(TestMemInstanceInner, MemShmSwitchBeginImportExecutor_IllegalParam)
{
    UbseMemShmSwitchObj switchObj{};
    switchObj.name = "migrate-res";
    // memId 为 0
    switchObj.newPa = 0x1234567800ULL;
    EXPECT_EQ(MemInstanceInnerShm::GetInstance().MemShmSwitchBeginImportExecutor(switchObj), UBSE_ERROR_INVAL);
    // newPa 为 0
    switchObj.oldImportMemId = 42;
    switchObj.newPa = 0;
    EXPECT_EQ(MemInstanceInnerShm::GetInstance().MemShmSwitchBeginImportExecutor(switchObj), UBSE_ERROR_INVAL);
}

TEST_F(TestMemInstanceInner, MemShmSwitchBeginImportExecutor_PaOnlyInDebugLog)
{
    gSwitchLogEntries.clear();
    MOCKER_CPP(ubse::log::UbseIsLog, bool (*)(ubse::log::UbseLogLevel)).stubs().will(returnValue(true));
    MOCKER_CPP(&ubse::log::UbseLog::operator==).stubs().will(invoke(CaptureSwitchLog));
    MOCKER_CPP(&RmObmmExecutor::ObmmSwitchBeginImport).stubs().will(returnValue(UBSE_OK));

    UbseMemShmSwitchObj switchObj{};
    switchObj.name = "migrate-log-test";
    switchObj.importNodeId = "4";
    switchObj.oldImportMemId = 42;
    switchObj.newPa = 0x1234567800ULL;
    EXPECT_EQ(MemInstanceInnerShm::GetInstance().MemShmSwitchBeginImportExecutor(switchObj), UBSE_OK);
    switchObj.oldImportMemId = 0;
    EXPECT_EQ(MemInstanceInnerShm::GetInstance().MemShmSwitchBeginImportExecutor(switchObj), UBSE_ERROR_INVAL);

    bool hasDebugPa = false;
    bool hasInfo = false;
    bool hasError = false;
    const auto address = std::to_string(switchObj.newPa);
    for (const auto& [level, message] : gSwitchLogEntries) {
        if (level == ubse::log::UbseLogLevel::DEBUG) {
            hasDebugPa = hasDebugPa || message.find(address) != std::string::npos;
        } else {
            EXPECT_EQ(message.find(address), std::string::npos);
        }
        hasInfo = hasInfo || level == ubse::log::UbseLogLevel::INFO;
        hasError = hasError || level == ubse::log::UbseLogLevel::ERROR;
    }
    EXPECT_TRUE(hasDebugPa);
    EXPECT_TRUE(hasInfo);
    EXPECT_TRUE(hasError);
}

TEST_F(TestMemInstanceInner, ImportObjectFormattingIncludesCurrentAddresses)
{
    UbseMemShmSwitchObj switchObj{};
    switchObj.name = "migrate-log-test";
    switchObj.oldImportMemId = 42;
    switchObj.newPa = 0x1234567800ULL;
    UbseMemShareBorrowImportObj importObj{};
    importObj.exportObmmInfo.resize(1);
    importObj.exportObmmInfo[0].memId = 43;
    importObj.exportObmmInfo[0].desc.addr = 0x2345678900ULL;
    importObj.status.decoderResult.resize(1);
    importObj.status.decoderResult[0].hpa = switchObj.newPa;
    std::ostringstream before;
    before << switchObj << importObj;

    // 通用格式化包含当前地址；实际日志输出策略由调用点决定。
    switchObj.newPa = 0x3456789A00ULL;
    importObj.exportObmmInfo[0].desc.addr = 0x456789AB00ULL;
    importObj.status.decoderResult[0].hpa = switchObj.newPa;
    std::ostringstream after;
    after << switchObj << importObj;
    EXPECT_NE(before.str(), after.str());
    EXPECT_NE(after.str().find(std::to_string(switchObj.newPa)), std::string::npos);
    EXPECT_NE(after.str().find(std::to_string(importObj.exportObmmInfo[0].desc.addr)), std::string::npos);
    EXPECT_NE(after.str().find("migrate-log-test"), std::string::npos);
    EXPECT_NE(after.str().find("oldImportMemId: 42"), std::string::npos);
    EXPECT_NE(after.str().find("memId: 43"), std::string::npos);
}
} // namespace ubse::ut::mmi
