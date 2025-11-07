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

#include "mem_instance_inner.h"
#include "ubse_error.h"
#include "ubse_mem_def.h"
#include "ubse_mem_common_utils.h"
#include "ubse_mem_obj_restore.h"
#include "ubse_obmm_executor.h"
#include "ubse_obmm_utils.h"

namespace ubse::mmi {
using namespace ubse::mem::obj;

uint32_t MemInstanceInner::MemNumaImportExecutor(UbseMemNumaBorrowImportObj &importObj)
{
    std::string lendNodeSocketStr;
    RmCommonUtils::GetInstance().GenerateNodeSocketStr(importObj.algoResult.exportNumaInfos[0].nodeId,
                                                       importObj.algoResult.exportNumaInfos[0].socketId,
                                                       lendNodeSocketStr);
    int numa = RmCommonUtils::GetInstance().HashStringToByte(lendNodeSocketStr);
    UbseMemLocalObmmCustomMeta customMeta{};
    auto ret = RmObmmUtils::GetCustomMetaFromNumaImportObj(importObj, customMeta);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Get custom meta from spec json error.";
        return ret;
    }
    ObmmOpParam obmmOpParam{UbseBorrowType::NUMA_BORROW, importObj.req.udsInfo.uid, importObj.req.udsInfo.gid,
                            customMeta};
    auto memids = RmObmmExecutor::GetInstance().ObmmImport(importObj.exportObmmInfo, obmmOpParam, &numa);
    UBSE_LOG_INFO << "AgentRpcObmmImportHandler Execute ObmmImport, memIdList="
                << RmCommonUtils::GetInstance().MemToStr(memids) << " remoteNuma=" << numa;
    if (memids.empty()) {
        UBSE_LOG_ERROR << "Obmm import failed.";
        importObj.status.errCode = E_CODE_OBMM_OP_FAILED;
        return E_CODE_OBMM_OP_FAILED;
    }
    for (int i = 0; i < memids.size(); i++) {
        importObj.status.importResults.push_back({memids[i], numa});
    }
    importObj.status.errCode = HOK;
    UBSE_LOG_INFO << "AgentRpcObmmImportHandler execute ok.";
    return HOK;
}

uint32_t MemInstanceInner::MemNumaUnImportExecutor(const UbseMemNumaBorrowImportObj &importObj)
{
    UbseResult ret = HOK;
    // 通过pid来判断是不是大数据的numa借用
    std::vector<mem_id> memIds{};
    for (int i = 0; i < importObj.status.importResults.size(); i++) {
        memIds.push_back(importObj.status.importResults[i].memId);
    }
    ret = RmObmmExecutor::GetInstance().ObmmUnImport(memIds);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Obmm unImport memid failed, memid= " << RmCommonUtils::GetInstance().MemToStr(memIds);
    }
    return HOK;
}

uint32_t MemInstanceInner::MemNumaExportExecutor(UbseMemNumaBorrowExportObj &exportObj)
{
    std::vector<ubse_mem_obmm_mem_desc> obmmMemDesc{};
    std::vector<uint64_t> memIdList{};
    size_t sizes[MAX_NUMA_NODES] = {0};
    auto code = GetObmmExportParamFromRequest(exportObj, obmmMemDesc, sizes);
    if (code != HOK) {
        return code;
    }
    UbseMemLocalObmmCustomMeta customMeta{};
    if (RmObmmUtils::GetCustomMetaFromNumaExportObj(exportObj, customMeta) != HOK) {
        UBSE_LOG_ERROR << "Get custom meta error.";
        exportObj.status.errCode = E_CODE_INVALID_PAR;
        return E_CODE_INVALID_PAR;
    }
    ObmmOpParam opParam{UbseBorrowType::NUMA_BORROW, exportObj.req.udsInfo.uid, exportObj.req.udsInfo.gid, customMeta};
    memIdList = RmObmmExecutor::GetInstance().ObmmExport(sizes, opParam, obmmMemDesc);
    if (memIdList.empty() || memIdList.size() != obmmMemDesc.size()) {
        exportObj.status.errCode = E_CODE_OBMM_OP_FAILED;
        UBSE_LOG_ERROR << "Obmm export failed." << RmCommonUtils::GetInstance().MemToStr(memIdList);
        return E_CODE_OBMM_OP_FAILED;
    }
    for (int i = 0; i < memIdList.size(); i++) {
        exportObj.status.exportObmmInfo.push_back({memIdList[i], obmmMemDesc[i]});
    }
    exportObj.status.errCode = HOK;
    return HOK;
}

uint32_t MemInstanceInner::MemNumaUnExportExecutor(const UbseMemNumaBorrowExportObj &exportObj)
{
    return UnExportExecutor(exportObj);
}

uint32_t MemInstanceInner::MemFdImportExecutor(UbseMemFdBorrowImportObj &importObj)
{
    UbseMemLocalObmmCustomMeta customMeta{};
    auto ret = RmObmmUtils::GetCustomMetaFromFdImportObj(importObj, customMeta);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Get custom meta from spec json error.";
        return ret;
    }
    ObmmOpParam obmmOpParam{UbseBorrowType::FD_BORROW, importObj.req.owner.uid, importObj.req.owner.gid, customMeta,
                            importObj.req.owner.mode};
    auto memids = RmObmmExecutor::GetInstance().ObmmImport(importObj.exportObmmInfo, obmmOpParam, nullptr);
    UBSE_LOG_INFO << "AgentRpcObmmImportHandler Execute ObmmImport, memIdList="
                << RmCommonUtils::GetInstance().MemToStr(memids);
    if (memids.empty()) {
        UBSE_LOG_ERROR << "Obmm import failed.";
        importObj.status.errCode = E_CODE_OBMM_OP_FAILED;
        return E_CODE_OBMM_OP_FAILED;
    }
    for (int i = 0; i < memids.size(); i++) {
        importObj.status.importResults.push_back({memids[i], -1});
    }
    importObj.status.errCode = HOK;
    UBSE_LOG_INFO << "AgentRpcObmmImportHandler execute ok.";
    return 0;
}

uint32_t MemInstanceInner::MemFdUnImportExecutor(const UbseMemFdBorrowImportObj &importObj)
{
    UbseResult ret = HOK;
    std::vector<mem_id> memIds{};
    for (int i = 0; i < importObj.status.importResults.size(); i++) {
        memIds.push_back(importObj.status.importResults[i].memId);
    }
    ret = RmObmmExecutor::GetInstance().ObmmUnImport(memIds);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "Obmm unImport memid failed, memid= " << RmCommonUtils::GetInstance().MemToStr(memIds);
    }
    return ret;
}

uint32_t MemInstanceInner::MemFdExportExecutor(UbseMemFdBorrowExportObj &exportObj)
{
    std::vector<ubse_mem_obmm_mem_desc> obmmMemDesc{};
    std::vector<uint64_t> memIdList{};
    size_t sizes[MAX_NUMA_NODES] = {0};
    auto code = GetObmmExportParamFromRequest(exportObj, obmmMemDesc, sizes);
    if (code != HOK) {
        return code;
    }
    UbseMemLocalObmmCustomMeta customMeta{};
    if (RmObmmUtils::GetCustomMetaFromFdExportObj(exportObj, customMeta) != HOK) {
        UBSE_LOG_ERROR << "Get custom meta error.";
        exportObj.status.errCode = E_CODE_INVALID_PAR;
        return E_CODE_INVALID_PAR;
    }
    ObmmOpParam opParam{UbseBorrowType::FD_BORROW, exportObj.req.udsInfo.uid, exportObj.req.udsInfo.gid, customMeta};
    memIdList = RmObmmExecutor::GetInstance().ObmmExport(sizes, opParam, obmmMemDesc);
    if (memIdList.empty() || memIdList.size() != obmmMemDesc.size()) {
        exportObj.status.errCode = E_CODE_OBMM_OP_FAILED;
        UBSE_LOG_ERROR << "Obmm export failed." << RmCommonUtils::GetInstance().MemToStr(memIdList);
        return E_CODE_OBMM_OP_FAILED;
    }
    for (int i = 0; i < memIdList.size(); i++) {
        exportObj.status.exportObmmInfo.push_back({memIdList[i], obmmMemDesc[i]});
    }
    exportObj.status.errCode = HOK;
    return HOK;
}

uint32_t MemInstanceInner::MemFdUnExportExecutor(const UbseMemFdBorrowExportObj &exportObj)
{
    return UnExportExecutor(exportObj);
}

void MemInstanceInner::RollbackImport(const std::vector<mem_id> &memids)
{
    for (auto item : memids) {
        auto res = RmObmmExecutor::GetInstance().ObmmUnImport(item);
        if (UBSE_RESULT_FAIL(res)) {
            UBSE_LOG_ERROR << "Pid borrow rollback export failed, memId=" << item << ", res=" << res;
        }
    }
}

void MemInstanceInner::RollbackExport(const std::vector<mem_id> &memids)
{
    for (auto item : memids) {
        auto res = RmObmmExecutor::GetInstance().ObmmUnExport(item);
        if (UBSE_RESULT_FAIL(res)) {
            UBSE_LOG_ERROR << "Pid borrow rollback export failed, memid=" << item;
        }
    }
}

uint32_t MemInstanceInner::MemGetObjData(NodeMemDebtInfo &memBorrowObj,
                                         std::vector<UbseMemLocalObmmMetaData> &allObmmDatas)
{
    LocalObmmMetaData localObmmMetaData{};
    auto ret = RmMemObjRestore::GetInstance().GetLocalObmmMeta(allObmmDatas, localObmmMetaData);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "ReadAgentLocalObmmMetaData failed.";
        return ret;
    }
    ret = RmMemObjRestore::GetInstance().ConstructFdImportObj(localObmmMetaData.FdImportMetaData,
                                                              memBorrowObj.fdImportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "ConstructFdImportObj failed.";
        return ret;
    }
    ret = RmMemObjRestore::GetInstance().ConstructFdExportObj(localObmmMetaData.FdExportMetaData,
                                                              memBorrowObj.fdExportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "ConstructFdExportObj failed.";
        return ret;
    }
    ret = RmMemObjRestore::GetInstance().ConstructNumaImportObj(localObmmMetaData.NumaImportMetaData,
                                                                memBorrowObj.numaImportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "ConstructImportObj failed.";
        return ret;
    }
    ret = RmMemObjRestore::GetInstance().ConstructNumaExportObj(localObmmMetaData.NumaExportMetaData,
                                                                memBorrowObj.numaExportObjMap);
    if (UBSE_RESULT_FAIL(ret)) {
        UBSE_LOG_ERROR << "ConstructNumaExportObj failed.";
        return ret;
    }
    return ret;
}

uint64_t MemInstanceInner::SetObmmDescDefaultErrorValue(std::vector<ubse_mem_obmm_mem_desc> &obmmMemDescs,
                                                        const uint64_t totalSize, const uint64_t blockSize)
{
    const uint64_t blockCount = (totalSize + blockSize - 1) / blockSize;
    obmmMemDescs.clear();
    obmmMemDescs.reserve(blockCount);
    for (int i = 0; i < blockCount; ++i) {
        obmmMemDescs.push_back({});
    }
    for (auto &obmmMemDesc : obmmMemDescs) {
        obmmMemDesc.length = 0;
    }
    return blockCount;
}

} // namespace ubse::mmi