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

#include "ubse_mem_instance_inner.h"

#include "src/controllers/mem/mem_decoder_utils/ubse_mem_decoder_utils.h"
#include "src/controllers/mem/mem_decoder_utils/ubse_mem_prehandle_manager.h"
#include "ubse_error.h"
#include "ubse_file_util.h"
#include "ubse_mem_common_utils.h"
#include "ubse_mem_def.h"
#include "ubse_mem_obj_restore.h"
#include "ubse_node_controller.h"
#include "ubse_obmm_executor.h"
#include "ubse_obmm_utils.h"
#include "ubse_str_util.h"
#include "ubse_topo_util.h"
namespace ubse::mmi {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::nodeController;
using namespace ubse::utils;
using namespace ubse::nodeController;
using namespace ubse::log;
using namespace ubse::mmi::restore;

static int LOCAL_NUMA_MAX{0};

uint32_t MemInstanceInnerNumaBorrow::MemNumaImportExecutor(UbseMemNumaBorrowImportObj &importObj)
{
    auto ret = MemInstanceInnerCommon::GetInstance().RemoteNumaIdInit();
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Init numaBorrowRemoteNumaId failed.";
        return ret;
    }
    auto portId = importObj.algoResult.importNumaInfos[0].portId;
    auto attachSocketId = importObj.algoResult.attachSocketId;
    std::string chipPortStr = std::to_string(attachSocketId) + "-" + std::to_string(portId);
    int numa = MemInstanceInnerCommon::GetInstance().GetNuma(chipPortStr);
    if (numa == INVALID_NUMAID) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get remote numaid failed, lendNodeChipStr= " << chipPortStr;
        return UBSE_ERROR_INVAL;
    }
    UbseMemLocalObmmCustomMeta customMeta{};
    ret = GetCustomMetaFromNumaImportObj(importObj, customMeta);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get custom meta from spec json error.";
        return ret;
    }
    UbMemPrivData ubPrivData{};
    mode_t mode = 0u;
    ObmmOpParam opParam{UbseBorrowType::NUMA_BORROW,
                        importObj.req.udsInfo.uid,
                        importObj.req.udsInfo.gid,
                        customMeta,
                        ubPrivData,
                        mode};
    ConstructUbMemPrivData(opParam.privData, 0, 0);
    auto memids = RmObmmExecutor::GetInstance().ObmmImport(importObj.exportObmmInfo, opParam, importObj.status, &numa);
    UBSE_LOG_INFO << MMI_LOG_INFO << "AgentRpcObmmImportHandler Execute ObmmImport, memIdList="
                  << RmCommonUtils::GetInstance().MemToStr(memids) << " remoteNuma=" << numa;
    if (memids.empty()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obmm import failed.";
        importObj.status.errCode = UBSE_MMI_OBMM_OP_FAILED;
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    for (size_t i = 0; i < memids.size(); i++) {
        importObj.status.importResults.push_back({memids[i], numa});
    }
    importObj.status.errCode = UBSE_OK;
    UBSE_LOG_INFO << MMI_LOG_INFO << "AgentRpcObmmImportHandler execute ok.";
    return UBSE_OK;
}

uint32_t MemInstanceInnerNumaBorrow::MemNumaUnImportExecutor(const UbseMemNumaBorrowImportObj &importObj)
{
    UbseResult ret = UBSE_OK;
    // 通过pid来判断是不是大数据的numa借用
    std::vector<mem_id> memIds{};
    memIds.reserve(importObj.status.importResults.size());
    for (size_t i = 0; i < importObj.status.importResults.size(); i++) {
        memIds.push_back(importObj.status.importResults[i].memId);
    }
    ret = RmObmmExecutor::GetInstance().ObmmUnImport(memIds);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "Obmm unImport memid failed, memid= " << RmCommonUtils::GetInstance().MemToStr(memIds)
                       << ", res=" << ret;
    }
    return ret;
}

uint32_t MemInstanceInnerNumaBorrow::MemNumaExportExecutor(UbseMemNumaBorrowExportObj &exportObj)
{
    std::vector<ubse_mem_obmm_mem_desc> obmmMemDesc{};
    std::vector<uint64_t> memIdList{};
    size_t sizes[MAX_NUMA_NODES] = {0};
    auto code = MemInstanceInnerCommon::GetInstance().GetObmmExportParamFromRequest(exportObj, obmmMemDesc, sizes,
                                                                                    MAX_NUMA_NODES);
    if (code != UBSE_OK) {
        return code;
    }
    UbseMemLocalObmmCustomMeta customMeta{};
    if (GetCustomMetaFromNumaExportObj(exportObj, customMeta) != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get custom meta error.";
        exportObj.status.errCode = UBSE_ERROR_INVAL;
        return UBSE_ERROR_INVAL;
    }
    UbMemPrivData ubPrivData{};
    mode_t mode = 0u;
    ObmmOpParam opParam{UbseBorrowType::NUMA_BORROW,
                        exportObj.req.udsInfo.uid,
                        exportObj.req.udsInfo.gid,
                        customMeta,
                        ubPrivData,
                        mode};
    ConstructUbMemPrivData(opParam.privData, 0, 0);
    auto blockSize = RmCommonUtils::GetInstance().SizeMb2Byte(exportObj.algoResult.blockSize);
    memIdList = RmObmmExecutor::GetInstance().ObmmExport(sizes, MAX_NUMA_NODES, opParam, obmmMemDesc, blockSize);
    if (memIdList.empty() || memIdList.size() != obmmMemDesc.size()) {
        exportObj.status.errCode = UBSE_MMI_OBMM_OP_FAILED;
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obmm export failed." << RmCommonUtils::GetInstance().MemToStr(memIdList);
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    for (size_t i = 0; i < memIdList.size(); i++) {
        exportObj.status.exportObmmInfo.push_back({memIdList[i], obmmMemDesc[i]});
    }
    exportObj.status.errCode = UBSE_OK;
    return UBSE_OK;
}

uint32_t MemInstanceInnerNumaBorrow::MemNumaUnExportExecutor(const UbseMemNumaBorrowExportObj &exportObj)
{
    return MemInstanceInnerCommon::GetInstance().UnExportExecutor(exportObj);
}

uint32_t MemInstanceInnerFdBorrow::MemFdImportExecutor(UbseMemFdBorrowImportObj &importObj)
{
    UbseMemLocalObmmCustomMeta customMeta{};
    auto ret = GetCustomMetaFromFdImportObj(importObj, customMeta);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get custom meta from spec json error.";
        return ret;
    }
    ObmmOpParam obmmOpParam{UbseBorrowType::FD_BORROW, importObj.req.owner.uid, importObj.req.owner.gid, customMeta};
    obmmOpParam.mode = importObj.req.owner.mode;
    ConstructUbMemPrivData(obmmOpParam.privData, 0, 0);
    auto memids =
        RmObmmExecutor::GetInstance().ObmmImport(importObj.exportObmmInfo, obmmOpParam, importObj.status, nullptr);
    UBSE_LOG_INFO << MMI_LOG_INFO << "AgentRpcObmmImportHandler Execute ObmmImport, memIdList="
                  << RmCommonUtils::GetInstance().MemToStr(memids);
    if (memids.empty()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obmm import failed.";
        importObj.status.errCode = UBSE_MMI_OBMM_OP_FAILED;
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    for (size_t i = 0; i < memids.size(); i++) {
        importObj.status.importResults.push_back({memids[i], -1});
    }
    importObj.status.errCode = UBSE_OK;
    UBSE_LOG_INFO << MMI_LOG_INFO << "AgentRpcObmmImportHandler execute ok.";
    return 0;
}

uint32_t MemInstanceInnerFdBorrow::MemFdImportPermissionExecutor(UbseMemFdBorrowImportObj &importObj)
{
    std::vector<std::string> OBMMDevices{};
    OBMMDevices.reserve(importObj.status.importResults.size());
    for (auto importResult : importObj.status.importResults) {
        OBMMDevices.emplace_back("/dev/obmm_shmdev" + std::to_string(importResult.memId));
    }
    auto owner = importObj.req.owner;
    bool success = true;
    // 设置权限
    std::vector<__u32> caps = {CAP_FOWNER, CAP_CHOWN};
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, true);
    for (const auto &OBMMDevice : OBMMDevices) {
        if (UbseFileUtil::CheckFileExists(OBMMDevice)) {
            bool res = UbseFileUtil::SetFileAttributes(OBMMDevice, owner.uid, owner.gid, owner.mode);
            if (!res) {
                UBSE_LOG_ERROR << MMI_LOG_INFO << "OBMMDevice=" << OBMMDevice << ", SetFileAttributes failed!";
            }
            success = success && res;
        } else {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "OBMMDevice=" << OBMMDevice
                           << " does not exist! SetFileAttributes failed!";
            success = false;
        }
    }
    UbseSecurityModule::ModifyEffectiveCapabilities(caps, false);
    return success ? UBSE_OK : UBSE_ERROR;
}

uint32_t MemInstanceInnerFdBorrow::MemFdUnImportExecutor(const UbseMemFdBorrowImportObj &importObj)
{
    UbseResult ret = UBSE_OK;
    std::vector<mem_id> memIds{};
    for (size_t i = 0; i < importObj.status.importResults.size(); i++) {
        memIds.push_back(importObj.status.importResults[i].memId);
    }
    ret = RmObmmExecutor::GetInstance().ObmmUnImport(memIds);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "Obmm unImport memid failed, memid= " << RmCommonUtils::GetInstance().MemToStr(memIds);
    }
    return ret;
}

uint32_t MemInstanceInnerFdBorrow::MemFdExportExecutor(UbseMemFdBorrowExportObj &exportObj)
{
    std::vector<ubse_mem_obmm_mem_desc> obmmMemDesc{};
    std::vector<uint64_t> memIdList{};
    size_t sizes[MAX_NUMA_NODES] = {0};
    auto code = MemInstanceInnerCommon::GetInstance().GetObmmExportParamFromRequest(exportObj, obmmMemDesc, sizes,
                                                                                    MAX_NUMA_NODES);
    if (code != UBSE_OK) {
        return code;
    }
    UbseMemLocalObmmCustomMeta customMeta{};
    if (GetCustomMetaFromFdExportObj(exportObj, customMeta) != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get custom meta error.";
        exportObj.status.errCode = UBSE_ERROR_INVAL;
        return UBSE_ERROR_INVAL;
    }
    ObmmOpParam opParam{UbseBorrowType::FD_BORROW, exportObj.req.udsInfo.uid, exportObj.req.udsInfo.gid, customMeta};
    ConstructUbMemPrivData(opParam.privData, 0, 0);
    auto blockSize = RmCommonUtils::GetInstance().SizeMb2Byte(exportObj.algoResult.blockSize);
    memIdList = RmObmmExecutor::GetInstance().ObmmExport(sizes, MAX_NUMA_NODES, opParam, obmmMemDesc, blockSize);
    if (memIdList.empty() || memIdList.size() != obmmMemDesc.size()) {
        exportObj.status.errCode = UBSE_MMI_OBMM_OP_FAILED;
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obmm export failed." << RmCommonUtils::GetInstance().MemToStr(memIdList);
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    for (size_t i = 0; i < memIdList.size(); i++) {
        exportObj.status.exportObmmInfo.push_back({memIdList[i], obmmMemDesc[i]});
    }
    exportObj.status.errCode = UBSE_OK;
    return UBSE_OK;
}

uint32_t MemInstanceInnerFdBorrow::MemFdUnExportExecutor(const UbseMemFdBorrowExportObj &exportObj)
{
    return MemInstanceInnerCommon::GetInstance().UnExportExecutor(exportObj);
}

void MemInstanceInnerCommon::RollbackImport(const std::vector<mem_id> &memids)
{
    for (auto item : memids) {
        auto res = RmObmmExecutor::GetInstance().ObmmUnImport(item);
        if (UBSE_RESULT_FAIL(res)) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Pid borrow rollback export failed, memId=" << item << ", res=" << res;
        }
    }
}

void MemInstanceInnerCommon::RollbackExport(const std::vector<mem_id> &memids)
{
    for (auto item : memids) {
        auto res = RmObmmExecutor::GetInstance().ObmmUnExport(item);
        if (UBSE_RESULT_FAIL(res)) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Pid borrow rollback export failed, memid=" << item;
        }
    }
}

uint32_t MemInstanceInnerShm::MemShmImportExecutor(UbseMemShareBorrowImportObj &importObj)
{
    // 自己map自己的情况
    auto exportNodeId = importObj.algoResult.exportNumaInfos[0].nodeId;
    if (exportNodeId == importObj.importNodeId) {
        for (size_t i = 0; i < importObj.exportObmmInfo.size(); i++) {
            auto changeRet = RmObmmExecutor::GetInstance().BaseObmmDevChangeUidGid(
                importObj.exportObmmInfo[i].memId, importObj.shareAttr.owner.uid, importObj.shareAttr.owner.gid,
                importObj.shareAttr.owner.mode);
            if (!changeRet) {
                UBSE_LOG_ERROR << MMI_LOG_INFO
                               << "Change obmm dev uid/gid failed, memid=" << importObj.exportObmmInfo[i].memId;
                importObj.status.errCode = UBSE_MMI_OBMM_OP_FAILED;
                return UBSE_MMI_OBMM_OP_FAILED;
            }
        }
        importObj.realExe = false;
        for (size_t i = 0; i < importObj.exportObmmInfo.size(); i++) {
            importObj.status.importResults.push_back({importObj.exportObmmInfo[i].memId, -1});
        }
        importObj.status.errCode = UBSE_OK;
        UBSE_LOG_INFO << MMI_LOG_INFO << "self Node has export memid,so return.";
        return UBSE_OK;
    }
    // 真正的导入
    UbseMemLocalObmmCustomMeta customMeta{};
    auto ret = GetCustomMetaFromShmImportObj(importObj, customMeta);
    if (ret != UBSE_OK) {
        importObj.status.errCode = ret;
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get custom meta from spec json error.";
        return ret;
    }
    ObmmOpParam obmmOpParam{UbseBorrowType::SHARE_BORROW, importObj.shareAttr.owner.uid, importObj.shareAttr.owner.gid,
                            customMeta};
    obmmOpParam.mode = importObj.shareAttr.owner.mode;
    MemInstanceInnerCommon::GetInstance().SetPrivDataByShareReq(obmmOpParam.privData, importObj.req.ubseMemPrivData);
    auto memids =
        RmObmmExecutor::GetInstance().ObmmImport(importObj.exportObmmInfo, obmmOpParam, importObj.status, nullptr);
    UBSE_LOG_INFO << MMI_LOG_INFO << "AgentRpcObmmImportHandler Execute ObmmImport, memIdList="
                  << RmCommonUtils::GetInstance().MemToStr(memids);
    if (memids.empty()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obmm import failed.";
        importObj.status.errCode = UBSE_MMI_OBMM_OP_FAILED;
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    for (size_t i = 0; i < memids.size(); i++) {
        importObj.status.importResults.push_back({memids[i], -1});
    }
    importObj.status.errCode = UBSE_OK;
    UBSE_LOG_INFO << MMI_LOG_INFO << "AgentRpcObmmImportHandler execute ok.";
    return UBSE_OK;
}

uint32_t MemInstanceInnerShm::MemShmUnImportExecutor(const UbseMemShareBorrowImportObj &importObj)
{
    UbseResult ret = UBSE_OK;
    if (!importObj.realExe) {
        UBSE_LOG_INFO << MMI_LOG_INFO << "self map export memid, unmap do nothing.";
        return ret;
    }
    std::vector<mem_id> memIds{};
    for (size_t i = 0; i < importObj.status.importResults.size(); i++) {
        memIds.push_back(importObj.status.importResults[i].memId);
    }
    ret = RmObmmExecutor::GetInstance().ObmmUnImport(memIds);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO
                       << "Obmm unImport memid failed, memid= " << RmCommonUtils::GetInstance().MemToStr(memIds);
    }
    return ret;
}

bool IsValidUbMemPrivData(UbMemPrivData &ubPrivData)
{
    return !(ubPrivData.one_pth == 0 && ubPrivData.wr_delay_comp == 0 && ubPrivData.reduce_delay_comp == 0 &&
             ubPrivData.cmo_delay_comp == 0 && ubPrivData.so == 0 && ubPrivData.ad_tr_ochip == 0 &&
             ubPrivData.cacheable_flag == 0 && ubPrivData.mar_id == 0 && ubPrivData.rsv0 == 0);
}

uint32_t MemInstanceInnerShm::MemShmExportExecutor(UbseMemShareBorrowExportObj &exportObj)
{
    std::vector<ubse_mem_obmm_mem_desc> obmmMemDesc{};
    std::vector<uint64_t> memIdList{};
    size_t sizes[MAX_NUMA_NODES] = {0};
    auto code = MemInstanceInnerCommon::GetInstance().GetObmmExportParamFromRequest(exportObj, obmmMemDesc, sizes,
                                                                                    MAX_NUMA_NODES);
    if (code != UBSE_OK) {
        return code;
    }
    UbseMemLocalObmmCustomMeta customMeta{};
    if (GetCustomMetaFromShmExportObj(exportObj, customMeta) != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Get custom meta error.";
        exportObj.status.errCode = UBSE_ERROR_INVAL;
        return UBSE_ERROR_INVAL;
    }
    ObmmOpParam opParam{UbseBorrowType::SHARE_BORROW, exportObj.req.udsInfo.uid, exportObj.req.udsInfo.gid, customMeta};
    CopyUbMemPrivData(opParam.privData, exportObj.req.ubseMemPrivData);
    if (!IsValidUbMemPrivData(opParam.privData)) {
        ConstructUbMemPrivData(opParam.privData, 0, 0);
    }
    auto blockSize = RmCommonUtils::GetInstance().SizeMb2Byte(exportObj.algoResult.blockSize);
    memIdList = RmObmmExecutor::GetInstance().ObmmExport(sizes, MAX_NUMA_NODES, opParam, obmmMemDesc, blockSize);
    if (memIdList.empty() || memIdList.size() != obmmMemDesc.size()) {
        exportObj.status.errCode = UBSE_MMI_OBMM_OP_FAILED;
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Obmm export failed." << RmCommonUtils::GetInstance().MemToStr(memIdList);
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    for (size_t i = 0; i < memIdList.size(); i++) {
        exportObj.status.exportObmmInfo.push_back({memIdList[i], obmmMemDesc[i]});
    }
    exportObj.status.errCode = UBSE_OK;
    return UBSE_OK;
}

uint32_t MemInstanceInnerShm::MemShmUnExportExecutor(const UbseMemShareBorrowExportObj &exportObj)
{
    return MemInstanceInnerCommon::GetInstance().UnExportExecutor(exportObj);
}

static uint32_t MemGetObjDataFdNuma(const LocalObmmMetaData &localObmmMetaData, NodeMemDebtInfo &memBorrowObj)
{
    auto ret = ConstructFdImportObj(localObmmMetaData.FdImportMetaData, memBorrowObj.fdImportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ConstructFdImportObj failed, ret=" << ret;
        return ret;
    }
    ret = ConstructFdExportObj(localObmmMetaData.FdExportMetaData, memBorrowObj.fdExportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ConstructFdExportObj failed, ret=" << ret;
        return ret;
    }
    ret = ConstructNumaImportObj(localObmmMetaData.NumaImportMetaData, memBorrowObj.numaImportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ConstructNumaImportObj failed, ret=" << ret;
        return ret;
    }
    ret = ConstructNumaExportObj(localObmmMetaData.NumaExportMetaData, memBorrowObj.numaExportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ConstructNumaExportObj failed, ret=" << ret;
    }
    return ret;
}

uint32_t MemInstanceInnerCommon::MemGetObjData(NodeMemDebtInfo &memBorrowObj,
                                               std::vector<UbseMemLocalObmmMetaData> &allObmmDatas)
{
    LocalObmmMetaData localObmmMetaData{};
    auto ret = GetLocalObmmMeta(allObmmDatas, localObmmMetaData);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ReadAgentLocalObmmMetaData failed, ret=" << ret;
        return ret;
    }
    ret = MemGetObjDataFdNuma(localObmmMetaData, memBorrowObj);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "MemGetObjDataFdNuma failed, ret=" << ret;
        return ret;
    }
    ret = ConstructShareImportObj(localObmmMetaData.ShmImportMetaData, memBorrowObj.shareImportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ConstructShareImportObj failed, ret=" << ret;
        return ret;
    }
    ret = ConstructShareExportObj(localObmmMetaData.ShmExportMetaData, memBorrowObj.shareExportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ConstructShareExportObj failed, ret=" << ret;
        return ret;
    }
    ret =
        ConstructShareImportObjFromExportMetaData(localObmmMetaData.ShmExportMetaData, memBorrowObj.shareImportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ConstructShareImportObjFromExportMetaData failed, ret=" << ret;
        return ret;
    }
    ret = ConstructAddrImportObj(localObmmMetaData.AddrImportMetaData, memBorrowObj.addrImportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ConstructAddrImportObj failed, ret=" << ret;
        return ret;
    }
    ret = ConstructAddrExportObj(localObmmMetaData.AddrExportMetaData, memBorrowObj.addrExportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ConstructAddrExportObj failed, ret=" << ret;
    }
    return ret;
}

uint64_t MemInstanceInnerCommon::SetObmmDescDefaultErrorValue(std::vector<ubse_mem_obmm_mem_desc> &obmmMemDescs,
                                                              const uint64_t totalSize, const uint64_t blockSize)
{
    if (blockSize == 0) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to set default obmm descriptions. blockSize=" << blockSize;
        return 0;
    }
    bool isOverflow = false;
    const uint64_t blockCount =
        (RmCommonUtils::GetInstance().SafeAdd(totalSize, blockSize, isOverflow) - 1) / blockSize;
    if (isOverflow) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Overflow occurred during addition. totalSize=" << totalSize
                       << " blockSize=" << blockSize;
        return 0;
    }
    obmmMemDescs.clear();
    obmmMemDescs.reserve(blockCount);
    for (size_t i = 0; i < blockCount; ++i) {
        obmmMemDescs.push_back({});
    }
    for (auto &obmmMemDesc : obmmMemDescs) {
        obmmMemDesc.length = 0;
    }
    return blockCount;
}

static void RollBackAddrRemoteNuma(std::vector<int> addrRemoteNumaIds)
{
    for (auto &item : addrRemoteNumaIds) {
        MemInstanceInnerAddrBorrow::GetInstance().DeleteAddrRemoteNuma(item);
    }
}

uint32_t MemInstanceInnerAddrBorrow::MemAddrImportExecutor(UbseMemAddrBorrowImportObj &importObj)
{
    int addrRemoteNumaId = INVALID_NUMAID;
    std::vector<mem_id> memIds{};
    std::vector<int> addrRemoteNumaIds{};
    if (importObj.exportObmmInfo.size() != importObj.req.exportAddrList.size()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The sizes of exportAddrList and exportObmmInfo are different. infoSize="
                       << importObj.exportObmmInfo.size() << " addrSize=" << importObj.req.exportAddrList.size();
        return UBSE_ERROR_INVAL;
    }
    for (size_t i = 0; i < importObj.exportObmmInfo.size(); i++) {
        UbseMemLocalObmmCustomMeta customMeta{};
        ObmmOpParam obmmOpParam{UbseBorrowType::ADDR_BORROW, DEFAULT_UID, DEFAULT_GID, customMeta};
        auto ret = GetCustomMetaFromAddrImportObj(importObj, obmmOpParam.customMeta, i);
        if (UBSE_RESULT_FAIL(ret)) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Get custom meta error.";
            importObj.status.errCode = ret;
            return ret;
        }
        addrRemoteNumaId = INVALID_NUMAID;
        ret = GenerateAddrRemoteNuma(addrRemoteNumaId);
        if (UBSE_RESULT_FAIL(ret)) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "GenerateAddrRemoteNuma failed, importObj is "
                           << RmCommonUtils::GetInstance().TranStructToStr(importObj);
            return ret;
        }
        addrRemoteNumaIds.push_back(addrRemoteNumaId);
        obmmOpParam.customMeta.memidCount = importObj.exportObmmInfo.size();
        obmmOpParam.customMeta.virAddr = importObj.req.exportAddrList[i].addr;
        ConstructUbMemPrivData(obmmOpParam.privData, 0, importObj.req.wrDelayComp);
        UBSE_LOG_DEBUG << "The value of wr_delay_comp is " << importObj.req.wrDelayComp;
        mem_id memid =
            RmObmmExecutor::GetInstance().ObmmImport(importObj.exportObmmInfo[i].desc, obmmOpParam, &addrRemoteNumaId);
        if (memid == INVALID_MEM_ID || addrRemoteNumaId == -1) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "obmm import error, start to rollback.";
            MemInstanceInnerCommon::GetInstance().RollbackImport(memIds);
            RollBackAddrRemoteNuma(addrRemoteNumaIds);
            importObj.status.errCode = UBSE_MMI_OBMM_OP_FAILED;
            importObj.status.importResults.clear();
            return UBSE_MMI_OBMM_OP_FAILED;
        }
        memIds.emplace_back(memid);
        importObj.status.importResults.push_back({memid, addrRemoteNumaId});
    }
    return UBSE_OK;
}

uint32_t MemInstanceInnerAddrBorrow::MemAddrUnImportExecutor(const UbseMemAddrBorrowImportObj &importObj)
{
    UbseResult ret = UBSE_OK;
    for (size_t i = 0; i < importObj.status.importResults.size(); i++) {
        ret = RmObmmExecutor::GetInstance().ObmmUnImport(importObj.status.importResults[i].memId);
        if (UBSE_RESULT_FAIL(ret)) {
            UBSE_LOG_ERROR << MMI_LOG_INFO
                           << "Obmm unImport memid failed, memid= " << importObj.status.importResults[i].memId;
            return ret;
        }
        UBSE_LOG_DEBUG << MMI_LOG_INFO
                       << "Obmm unImport memid success, memid=" << importObj.status.importResults[i].memId;
        DeleteAddrRemoteNuma(importObj.status.importResults[i].numaId);
    }
    return ret;
}

uint32_t MemInstanceInnerAddrBorrow::MemAddrExportExecutor(UbseMemAddrBorrowExportObj &exportObj)
{
    const auto &addr = exportObj.req.exportAddrList;
    std::vector<mem_id> memIds{};
    std::vector<ubse_mem_obmm_mem_desc> memDescs{};
    UbseMemLocalObmmCustomMeta customMeta{};
    UbMemPrivData ubMemPrivData{};
    std::unordered_map<uint64_t, uint64_t> exportNumaInfoMap{};

    ConstructUbMemPrivData(ubMemPrivData, 0, 0);
    for (size_t i = 0; i < addr.size(); ++i) {
        auto ret = GetCustomMetaFromAddrExportObj(exportObj, customMeta);
        if (UBSE_RESULT_FAIL(ret)) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Get custom meta error.";
            exportObj.status.errCode = ret;
            return ret;
        }
        customMeta.memidCount = addr.size();
        customMeta.virAddr = addr[i].addr;
        ubse_mem_obmm_mem_desc obmmMemDesc{};
        ObmmPidExportParam param(static_cast<int>(exportObj.req.exportPid), reinterpret_cast<void *>(addr[i].addr),
                                 addr[i].size, 0, 0, 0);
        ret = RmObmmExecutor::GetInstance().ObmmExportPid(param, obmmMemDesc, customMeta, ubMemPrivData);
        if (UBSE_RESULT_FAIL(ret)) {
            RmObmmExecutor::GetInstance().ObmmUnExport(param.memid);
            MemInstanceInnerCommon::GetInstance().RollbackExport(memIds);
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Pid export failed, pid= " << exportObj.req.exportPid;
            exportObj.status.errCode = ret;
            return ret;
        }
        memIds.emplace_back(param.memid);
        memDescs.emplace_back(obmmMemDesc);
        exportObj.status.exportObmmInfo.push_back({param.memid, obmmMemDesc});
        UBSE_LOG_DEBUG << MMI_LOG_INFO << "AddrBorrow, exportNuma = " << param.exportNuma
                       << ", size = " << addr[i].size;
        bool isOverflow = false;
        exportNumaInfoMap[param.exportNuma] =
            RmCommonUtils::GetInstance().SafeAdd(exportNumaInfoMap[param.exportNuma], addr[i].size, isOverflow);
        if (isOverflow) {
            UBSE_LOG_ERROR << MMI_LOG_INFO
                           << "Overflow occurred during addition. numaSize=" << exportNumaInfoMap[param.exportNuma]
                           << " addSize=" << addr[i].size;
            return UBSE_ERROR_INVAL;
        }
    }
    if (AfterMemAddrExportExecutor(exportObj, exportNumaInfoMap, memIds) != UBSE_OK) {
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    exportObj.status.errCode = UBSE_OK;
    return UBSE_OK;
}

uint32_t MemInstanceInnerAddrBorrow::MemAddrUnExportExecutor(const UbseMemAddrBorrowExportObj &exportObj)
{
    return MemInstanceInnerCommon::GetInstance().UnExportExecutor(exportObj);
}

UbseResult MemInstanceInnerAddrBorrow::AfterMemAddrExportExecutor(
    UbseMemAddrBorrowExportObj &exportObj, const std::unordered_map<uint64_t, uint64_t> &exportNumaInfoMap,
    const std::vector<mem_id> &memIds)
{
    if (exportNumaInfoMap.size() > TOPOLOGY_MAX_NUMA_PER_SOCKET) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "The export numa count is " << exportNumaInfoMap.size();
        MemInstanceInnerCommon::GetInstance().RollbackExport(memIds);
        exportObj.status.errCode = UBSE_MMI_OBMM_OP_FAILED;
        return UBSE_MMI_OBMM_OP_FAILED;
    }
    for (auto &item : exportNumaInfoMap) {
        UbseMemDebtNumaInfo tmpImportDebtNumaInfo{};
        UbseMemDebtNumaInfo tmpExportDebtNumaInfo{};
        tmpImportDebtNumaInfo.nodeId = exportObj.req.importNodeId;
        tmpImportDebtNumaInfo.numaId = exportObj.req.srcNuma;
        tmpImportDebtNumaInfo.size = item.second;
        tmpExportDebtNumaInfo.nodeId = exportObj.req.exportNodeId;
        tmpExportDebtNumaInfo.numaId = static_cast<int64_t>(item.first);
        tmpExportDebtNumaInfo.size = item.second;
        exportObj.algoResult.importNumaInfos.push_back(tmpImportDebtNumaInfo);
        exportObj.algoResult.exportNumaInfos.push_back(tmpExportDebtNumaInfo);
    }
    return UBSE_OK;
}

UbseResult MemInstanceInnerCommon::RemoteNumaIdInit()
{
    if (mInited.load()) {
        return UBSE_OK;
    }
    std::unique_lock unique(mLock);
    auto nodeInfo = UbseNodeController::GetInstance().GetCurNode();
    if (nodeInfo.nodeId.empty()) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to get cur node info.";
        return UBSE_MMI_DAEMON_FAILED;
    }
    LOCAL_NUMA_MAX = nodeInfo.numaInfos.size();
    std::set<std::string> socketPair;
    for (const auto &cpuInfo : nodeInfo.cpuInfos) {
        for (const auto &portInfo : cpuInfo.second.portInfos) {
            std::ostringstream str;
            str << cpuInfo.second.socketId << "-" << portInfo.second.portId;
            socketPair.insert(str.str());
        }
    }
    int index = 0;
    for (const auto &pair : socketPair) {
        mRemoteNumaMap[pair] = index + LOCAL_NUMA_MAX;
        UBSE_LOG_INFO << MMI_LOG_INFO << pair << "=" << (index + LOCAL_NUMA_MAX);
        index++;
    }
    mInited.store(true);
    return UBSE_OK;
}

int MemInstanceInnerCommon::GetNuma(const std::string &nodesocketPair) const noexcept
{
    std::shared_lock<std::shared_mutex> lock(mLock);
    const auto iterator = mRemoteNumaMap.find(nodesocketPair);
    if (iterator == mRemoteNumaMap.end()) {
        UBSE_LOG_WARN << MMI_LOG_INFO << "nodesocketPair=" << nodesocketPair
                      << " mRemoteNumaMap.size=" << mRemoteNumaMap.size();
        return -1;
    }
    UBSE_LOG_INFO << MMI_LOG_INFO << "nodesocketPair=" << nodesocketPair << ", numaId=" << iterator->second;
    return iterator->second;
}
void MemInstanceInnerAddrBorrow::AddAddrRemoteNuma(int remoteNuma)
{
    std::unique_lock<std::mutex> lock(mAddrLock);
    addrRemoteNumaIdSet.insert(remoteNuma);
}

void MemInstanceInnerAddrBorrow::DeleteAddrRemoteNuma(int remoteNuma)
{
    std::unique_lock<std::mutex> lock(mAddrLock);
    if (addrRemoteNumaIdSet.find(remoteNuma) != addrRemoteNumaIdSet.end()) {
        addrRemoteNumaIdSet.erase(remoteNuma);
    }
}
UbseResult MemInstanceInnerAddrBorrow::GenerateAddrRemoteNuma(int &remoteNumaId)
{
    std::unique_lock<std::mutex> lock(mAddrLock);
    for (int i = MIN_ADDR_REMOTE_NUMA_ID; i < MAX_ADDR_REMOTE_NUMA_ID; i++) {
        if (addrRemoteNumaIdSet.find(i) == addrRemoteNumaIdSet.end()) {
            remoteNumaId = i;
            addrRemoteNumaIdSet.insert(i);
            return UBSE_OK;
        }
    }
    UBSE_LOG_ERROR << MMI_LOG_INFO << "GenerateAddrRemoteNuma failed, all available numa has used.";
    return UBSE_MMI_OBMM_OP_FAILED;
}

void RollBackPreOnline(const std::vector<obmm_preimport_info> &obmmPreImportInfos)
{
    for (auto &item : obmmPreImportInfos) {
        auto tempObmmPreImportInfo = item;
        auto ret = RmObmmExecutor::GetInstance().ObmmUnPreImport(&tempObmmPreImportInfo, 0u);
        if (UBSE_RESULT_FAIL(ret)) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmUnPreImport failed, scna= " << tempObmmPreImportInfo.scna
                           << ", dcna= " << tempObmmPreImportInfo.dcna;
        }
    }
}

UbseResult SetPreImportDecoderParam(const SocketCnaInfo &cnaTopoInfo, uint64_t preImportSize, uint64_t Dcna,
                                    mem::decoder::utils::PreImportDecoderParam &preImportDecoderParam)
{
    std::pair<uint32_t, uint32_t> chipDiePair{};
    auto res = mem::decoder::utils::MemDecoderUtils::GetChipAndDieId(cnaTopoInfo.importSocketId, chipDiePair);
    preImportDecoderParam.ubpuId = chipDiePair.first;
    preImportDecoderParam.iouId = chipDiePair.second;
    preImportDecoderParam.importType = UB_MEMORY_PREIMPORT_MEMORY_STATIC; // 静态预引入
    preImportDecoderParam.marId = cnaTopoInfo.marId;
    preImportDecoderParam.dstCNA = Dcna;
    preImportDecoderParam.size = preImportSize;
    return res;
}

UbseResult MemPreImport(BasicPreImportInfo &basicPreImportInfo,
                        const mem::decoder::utils::PreImportDecoderParam &preImportDecoderParam,
                        const adapter_plugins::mti::mami::UbseMamiMemImportResult importValue,
                        std::vector<obmm_preimport_info> &obmmPreImportInfos)
{
    auto ret = UBSE_OK;
    basicPreImportInfo.preOnlineSize = preImportDecoderParam.size;
    mem::decoder::utils::DecoderEntryLoc loc{preImportDecoderParam.ubpuId, preImportDecoderParam.iouId,
                                             preImportDecoderParam.marId, 0};
    std::vector<uint64_t> preImportHandles{};
    basicPreImportInfo.pa = importValue.hpa;
    auto preImportInfo = ConstructPreImportInfo(basicPreImportInfo);
    if (!preImportInfo) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "PreImportInfo is nullptr.";
        mem::decoder::utils::UbseMemPrehandleManager::GetInstance().RollbackPreImportHandle(loc);
        RollBackPreOnline(obmmPreImportInfos);
        return UBSE_ERROR_INVAL;
    }
    ret = RmObmmExecutor::GetInstance().ObmmPreImport(preImportInfo, 0u);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmPreImport failed.";
        RmCommonUtils::GetInstance().SafeFree(preImportInfo);
        mem::decoder::utils::UbseMemPrehandleManager::GetInstance().RollbackPreImportHandle(loc);
        RollBackPreOnline(obmmPreImportInfos);
        return ret;
    }
    obmmPreImportInfos.push_back(*preImportInfo);
    RmCommonUtils::GetInstance().SafeFree(preImportInfo);

    return ret;
}

UbseResult GetDcna(const UbsePortInfo portInfo, const SocketCnaInfo cnaTopoInfo,
                   std::vector<obmm_preimport_info> &obmmPreImportInfos, uint64_t preImportSize, const bool isPoc)
{
    uint32_t portId;
    auto ret = ConvertStrToUint32(portInfo.portId, portId);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to convert portId form string to int. PortId is " << portInfo.portId;
        return UBSE_ERROR;
    }
    std::string chipPortStr = std::to_string(cnaTopoInfo.importSocketId) + "-" + std::to_string(portId);
    int numaId = MemInstanceInnerCommon::GetInstance().GetNuma(chipPortStr);
    uint32_t portCna = portInfo.portCna;
    BasicPreImportInfo basicPreImportInfo{0,      cnaTopoInfo.scna, portCna,          cnaTopoInfo.marId,
                                          numaId, preImportSize,    cnaTopoInfo.seid, cnaTopoInfo.deid};
    if (!isPoc) {
        basicPreImportInfo.dcna = cnaTopoInfo.dcna;
        UBSE_LOG_INFO << MMI_LOG_INFO << "Use primary cna. Dcna is " << cnaTopoInfo.dcna;
    }

    mem::decoder::utils::PreImportDecoderParam preImportDecoderParam{};
    auto res = SetPreImportDecoderParam(cnaTopoInfo, preImportSize, basicPreImportInfo.dcna, preImportDecoderParam);
    if (res != UBSE_OK) {
        return UBSE_ERROR_INVAL;
    }

    adapter_plugins::mti::mami::UbseMamiMemImportResult importValue{};
    mem::decoder::utils::DecoderEntryLoc loc{preImportDecoderParam.ubpuId, preImportDecoderParam.iouId,
                                             preImportDecoderParam.marId, 0};
    auto isNeedPreOnline = mem::decoder::utils::UbseMemPrehandleManager::GetInstance().IsNeedPreOnline(
        loc, basicPreImportInfo.dcna, importValue);
    if (!isNeedPreOnline) {
        return UBSE_OK;
    }
    if (importValue.hpa == 0) {
        ret = mem::decoder::utils::MemDecoderUtils::PreImportDecoderEntry(preImportDecoderParam, importValue);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "PreImportDecoderEntry failed";
            return UBSE_ERROR_INVAL;
        }
    }
    UBSE_LOG_INFO << "dcna is " << basicPreImportInfo.dcna << "value is " << importValue.hpa << "handle is "
                  << importValue.handle;
    ret = MemPreImport(basicPreImportInfo, preImportDecoderParam, importValue, obmmPreImportInfos);
    if (ret != UBSE_OK) {
        return ret;
    }
    return UBSE_OK;
}

UbseResult PreOnlineHandler(const std::vector<SocketCnaInfo> &cnaTopoInfos, uint64_t preImportSize)
{
    std::vector<obmm_preimport_info> obmmPreImportInfos{};
    auto nodeInfos = UbseNodeController::GetInstance().GetAllNodes();
    auto isPoc = IsSameSocketMultiPortTopo();
    for (auto &cnaTopoInfo : cnaTopoInfos) {
        UBSE_LOG_INFO << MMI_LOG_INFO
                      << "PreImport socketCnaInfo= " << RmCommonUtils::GetInstance().TranStructToStr(cnaTopoInfo);
        auto nodeInfo = nodeInfos.find(cnaTopoInfo.exportNodeId);
        if (nodeInfo == nodeInfos.end()) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to find node info in all node infos, node id is "
                           << cnaTopoInfo.exportNodeId;
            return UBSE_ERROR;
        }
        std::pair<uint32_t, uint32_t> chipDiePair{};
        auto res = mem::decoder::utils::MemDecoderUtils::GetChipAndDieId(cnaTopoInfo.importSocketId, chipDiePair);
        UbseCpuLocation location{cnaTopoInfo.exportNodeId, cnaTopoInfo.exportSocketId};
        auto cpuInfos = nodeInfo->second.cpuInfos.find(location);
        if (res != UBSE_OK || cpuInfos == nodeInfo->second.cpuInfos.end()) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "Failed to find cpu info in all cpu infos, node id is " << location.nodeId
                           << ", socket id is " << location.chipId;
            return UBSE_ERROR;
        }
        for (const auto &portInfo : cpuInfos->second.portInfos) {
            if (portInfo.second.portStatus == PortStatus::DOWN ||
                portInfo.second.remoteSlotId != cnaTopoInfo.importNodeId ||
                portInfo.second.remoteChipId != std::to_string(chipDiePair.first)) {
                continue;
            }

            if (GetDcna(portInfo.second, cnaTopoInfo, obmmPreImportInfos, preImportSize, isPoc) != UBSE_OK) {
                return UBSE_ERROR;
            }
        }
    }
    return UBSE_OK;
}

bool CheckPreImportInfoSize(const std::vector<BasicPreImportInfo> &preImportInfos)
{
    if (preImportInfos.empty()) {
        return true;
    }

    // Step 1: 按 dcna 聚合总大小
    std::unordered_map<uint32_t, uint64_t> groupSizes;
    for (const auto &info : preImportInfos) {
        groupSizes[info.dcna] += info.preOnlineSize;
    }

    // Step 2: 检查所有组的大小是否一致
    uint64_t reference = groupSizes.begin()->second;
    for (const auto &kv : groupSizes) {
        if (kv.second != reference) {
            return false;
        }
    }

    return true;
}

UbseResult MemInstanceInnerCommon::MemPreOnline(const std::vector<SocketCnaInfo> &cnaTopoInfos, uint64_t preImportSize)
{
    auto ret = RemoteNumaIdInit();
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << MMI_LOG_INFO << "Init numaBorrowRemoteNumaId failed.";
        return ret;
    }

    // 初始化静态handle表
    std::vector<BasicPreImportInfo> preImportInfos{};
    ret = RmObmmUtils::GetPreOnlineInfo(preImportInfos);
    if (ret != UBSE_OK) {
        return ret;
    }
    ret = ubse::mem::decoder::utils::UbseMemPrehandleManager::GetInstance().InitPreHandle(preImportInfos);
    if (UBSE_RESULT_FAIL(ret)) {
        return ret;
    }

    return PreOnlineHandler(cnaTopoInfos, preImportSize);
}

UbseResult MemInstanceInnerCommon::MemUnPreOnline()
{
    std::vector<BasicPreImportInfo> preImportInfos{};
    auto ret = RmObmmUtils::GetPreOnlineInfo(preImportInfos);
    if (preImportInfos.empty()) {
        UBSE_LOG_WARN << MMI_LOG_INFO << "PreImportInfos is empty.";
        return ret;
    }
    for (auto &preImportInfoItem : preImportInfos) {
        auto preImportInfo = ConstructPreImportInfo(preImportInfoItem);
        if (!preImportInfo) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "PreImportInfo is nullptr.";
            return UBSE_ERROR_INVAL;
        }
        UBSE_LOG_INFO << "preImportInfo pa is " << preImportInfo->pa;
        ret = RmObmmExecutor::GetInstance().ObmmUnPreImport(preImportInfo, 0u);
        if (UBSE_RESULT_FAIL(ret)) {
            UBSE_LOG_ERROR << MMI_LOG_INFO << "ObmmUnPreImport failed, ret=" << ret;
            RmCommonUtils::GetInstance().SafeFree(preImportInfo);
            return ret;
        }
        RmCommonUtils::GetInstance().SafeFree(preImportInfo);
    }
    return UBSE_OK;
}

void MemInstanceInnerCommon::SetPrivDataByShareReq(UbMemPrivData &destPrivData, UbseMemPrivData &sourcePrivData)
{
    destPrivData.one_pth = sourcePrivData.onePth;
    destPrivData.wr_delay_comp = sourcePrivData.wrDelayComp;
    destPrivData.reduce_delay_comp = sourcePrivData.reduceDelayComp;
    destPrivData.cmo_delay_comp = sourcePrivData.cmoDelayComp;
    destPrivData.so = sourcePrivData.so;
    destPrivData.ad_tr_ochip = sourcePrivData.adTrOchip;
    destPrivData.cacheable_flag = sourcePrivData.cacheableFlag;
    destPrivData.mar_id = sourcePrivData.marId;
    destPrivData.rsv0 = sourcePrivData.rsv0;
}

} // namespace ubse::mmi