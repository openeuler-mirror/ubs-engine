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

#ifndef UBS_ENGINE_UBSE_MEM_DEBT_INFO_QUERY_H
#define UBS_ENGINE_UBSE_MEM_DEBT_INFO_QUERY_H

#include "ubse_mem_controller.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_controller_def.h"
#include "ubse_mem_debt_ledger.h"

namespace ubse::mem::controller::debt {
using ubse::mem::def::FdHandleInfoVec;
using ubse::mem::def::NumaHandleInfoVec;
using ubse::mem::def::ShareHandleInfoVec;
using ubse::mem::def::UbseMemDebtQueryRequest;
using ubse::mem::def::UbseMemFdDesc;
using ubse::mem::def::UbseMemShmDesc;
using ubse::mem::def::UbseMemShmMemStatusDesc;
using ubse::mem::def::UbseNodeBorrowInfo;
uint32_t UbseMemFdGet(const UbseMemDebtQueryRequest& request, UbseMemFdDesc& fdDesc);

uint32_t UbseMemFdList(const UbseMemDebtQueryRequest& request, std::vector<UbseMemFdDesc>& fdDescs);

uint32_t UbseMemNumaGet(const UbseMemDebtQueryRequest& request, def::UbseMemNumaDesc& numaDesc);

uint32_t UbseMemNumaList(const UbseMemDebtQueryRequest& request, std::vector<def::UbseMemNumaDesc>& numaDescs);

uint32_t UbseMemNumaGetWithImportNode(const UbseMemDebtQueryRequest& request, UbseMemNumaDesc& numaDesc);

uint32_t UbseMemShmGet(const UbseMemDebtQueryRequest& request, UbseMemShmDesc& shmDesc);

uint32_t UbseMemShmList(const UbseMemDebtQueryRequest& request, std::vector<UbseMemShmDesc>& shmDescs);

uint32_t UbseMemShmStatusGet(const UbseMemDebtQueryRequest& request, UbseMemShmMemStatusDesc& shmStatusDesc);

uint32_t UbseMemAddrGet(const UbseMemDebtQueryRequest& request, UbseMemAddrDesc& desc);

template <typename ImportType, typename ExportType>
std::pair<std::shared_ptr<const ImportType>, std::shared_ptr<const ExportType>>
FindBorrowObjPair(const std::string& name, const std::string& importNodeId)
{
    auto importObj = UbseMemDebtLedger::GetInstance().GetDebtMap<ImportType>().GetResource(importNodeId, name);
    auto exportKey = GenerateExportObjKey(name, importNodeId);
    auto exportObj = UbseMemDebtLedger::GetInstance().GetDebtMap<ExportType>().GetExportResourceByResId(exportKey);
    return {importObj, exportObj};
}

template <typename ImportType, typename ExportType>
UbseMemResult GetStageByObj(const std::string& name, const std::string& importNodeId)
{
    UbseMemResult result{};
    result.name = name;
    result.importNodeId = importNodeId;

    auto [importObjPtr, exportObjPtr] = FindBorrowObjPair<ImportType, ExportType>(name, importNodeId);
    // 无导入导出
    if (!importObjPtr && !exportObjPtr) {
        result.stage = UbseMemStage::UBSE_NOT_EXIST;
        return result;
    }
    // 单导出
    if (!importObjPtr) {
        if constexpr (IsAddrTypeV<ExportType>) {
            for (const auto& addrInfo : exportObjPtr->req.exportAddrList) {
                result.realSize += addrInfo.size;
            }
        } else {
            result.realSize = exportObjPtr->req.size;
        }
        result.stage = UbseMemStage::UBSE_ERR_WAIT_UNEXPORT;
        return result;
    }

    if constexpr (IsAddrTypeV<ImportType>) {
        for (const auto& addrInfo : importObjPtr->req.exportAddrList) {
            result.realSize += addrInfo.size;
        }
    } else {
        result.realSize = importObjPtr->req.size;
    }
    result.stage = GetOptStageByObjState(*importObjPtr, (exportObjPtr != nullptr));
    return result;
}

UbseMemResult GetFdStageByObj(const std::string& name, const std::string& importNodeId);

UbseMemResult GetNumaStageByObj(const std::string& name, const std::string& importNodeId);

UbseMemResult GetShmExportStageByObj(const std::string& name);

UbseMemResult GetShmImportStageByObj(const std::string& name, const std::string& importNodeId);

UbseMemResult GetAddrStageByObj(const std::string& name, const std::string& importNodeId);

uint32_t UbseMemNodeBorrowQuery(std::vector<UbseNodeBorrowInfo>& nodeBorrowInfo);

UbseMemFdBorrowExportObj UbseFdExportObjGet(const std::string& nodeId, const std::string& name,
                                            const std::string& importNodeId, bool isFromTaskManager = false);

UbseMemNumaBorrowExportObj UbseNumaExportObjGet(const std::string& nodeId, const std::string& name,
                                                const std::string& importNodeId, bool isFromTaskManager = false);

UbseMemShareBorrowExportObj UbseShareExportObjGet(const std::string& nodeId, const std::string& name,
                                                  bool isFromTaskManager = false);

UbseMemAddrBorrowExportObj UbseAddrExportObjGet(const std::string& nodeId, const std::string& name,
                                                const std::string& importNodeId, bool isFromTaskManager = false);

UbseMemFdBorrowImportObj UbseFdImportObjGet(const std::string& nodeId, const std::string& name,
                                            bool isFromTaskManager = false);

UbseMemNumaBorrowImportObj UbseNumaImportObjGet(const std::string& nodeId, const std::string& name,
                                                bool isFromTaskManager = false);

UbseMemShareBorrowImportObj UbseShareImportObjGet(const std::string& nodeId, const std::string& name,
                                                  bool isFromTaskManager = false);

UbseMemAddrBorrowImportObj UbseAddrImportObjGet(const std::string& nodeId, const std::string& name,
                                                bool isFromTaskManager = false);

UbseResult UbseQueryShareImportHandleByExportNodeId(const std::string& importNodeId, const std::string& exportNodeId,
                                                    ShareHandleInfoVec& importHandInfo);

UbseResult UbseQueryFdImportHandleByExportNodeId(const std::string& importNodeId, const std::string& exportNodeId,
                                                 FdHandleInfoVec& importHandInfo);

UbseResult UbseQueryNumaImportHandleByExportNodeId(const std::string& importNodeId, const std::string& exportNodeId,
                                                   NumaHandleInfoVec& importHandInfo);

UbseResult UbseQueryFdPortFaultHandleInfo(const std::string& nodeId, const std::string& chipId,
                                          const std::set<std::string>& portList, FdHandleInfoVec& importHandInfo);

UbseResult UbseQuerySharePortFaultHandleInfo(const std::string& nodeId, const std::string& chipId,
                                             const std::set<std::string>& portList, ShareHandleInfoVec& importHandInfo);

UbseResult UbseQueryNumaPortFaultHandleInfo(const std::string& nodeId, const std::string& chipId,
                                            const std::set<std::string>& portList, NumaHandleInfoVec& importHandInfo);

uint32_t UbseMemGetMemIdByImport(const def::UbseMemIdQueryRequest& request, def::UbseExportMemDesc& memDesc);
} // namespace ubse::mem::controller::debt
#endif // UBS_ENGINE_UBSE_MEM_DEBT_INFO_QUERY_H
