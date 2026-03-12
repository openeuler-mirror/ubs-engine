/*
* Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
*/

#ifndef UBSE_MANAGER_UBSE_MEM_CONTROLLER_LEDGER_FILTER_H
#define UBSE_MANAGER_UBSE_MEM_CONTROLLER_LEDGER_FILTER_H

#include "ubse_mmi_interface.h"

#include "ubse_common_def.h"
#include "ubse_error.h"

namespace ubse::mem::controller {
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::common::def;

std::string TransState(UbseMemState state);

/**
* 从master和agent账本中过滤掉master中处于running状态的账本；
* @param masterExportObjs [in] master侧账本
* @param agentExportObjs [in] agent侧账本
* @param masterRunningExportObjs [out] master侧处于running状态的账本
* @param masterFilterRunningExportObjs [out] master侧处于非running状态账本
* @param agentFilterRunningExportObjs [out] agent侧处于非running状态账本
*/
void FilterRunningFdExport(std::vector<UbseMemFdBorrowExportObj> masterExportObjs,
                           std::vector<UbseMemFdBorrowExportObj> agentExportObjs,
                           std::vector<UbseMemFdBorrowExportObj> &masterRunningExportObjs,
                           std::vector<UbseMemFdBorrowExportObj> &masterFilterRunningExportObjs,
                           std::vector<UbseMemFdBorrowExportObj> &agentFilterRunningExportObjs);

void FilterRunningFdImport(std::vector<UbseMemFdBorrowImportObj> masterImportObjs,
                           std::vector<UbseMemFdBorrowImportObj> agentImportObjs,
                           std::vector<UbseMemFdBorrowImportObj> &masterRunningImportObjs,
                           std::vector<UbseMemFdBorrowImportObj> &masterFilterRunningImportObjs,
                           std::vector<UbseMemFdBorrowImportObj> &agentFilterRunningImportObjs);

void FilterRunningNumaExport(std::vector<UbseMemNumaBorrowExportObj> masterExportObjs,
                             std::vector<UbseMemNumaBorrowExportObj> agentExportObjs,
                             std::vector<UbseMemNumaBorrowExportObj> &masterRunningExportObjs,
                             std::vector<UbseMemNumaBorrowExportObj> &masterFilterRunningExportObjs,
                             std::vector<UbseMemNumaBorrowExportObj> &agentFilterRunningExportObjs);

void FilterRunningNumaImport(std::vector<UbseMemNumaBorrowImportObj> masterImportObjs,
                             std::vector<UbseMemNumaBorrowImportObj> agentImportObjs,
                             std::vector<UbseMemNumaBorrowImportObj> &masterRunningImportObjs,
                             std::vector<UbseMemNumaBorrowImportObj> &masterFilterRunningImportObjs,
                             std::vector<UbseMemNumaBorrowImportObj> &agentFilterRunningImportObjs);

void FilterRunningAddrExport(std::vector<UbseMemAddrBorrowExportObj> masterExportObjs,
                             std::vector<UbseMemAddrBorrowExportObj> agentExportObjs,
                             std::vector<UbseMemAddrBorrowExportObj> &masterRunningExportObjs,
                             std::vector<UbseMemAddrBorrowExportObj> &masterFilterRunningExportObjs,
                             std::vector<UbseMemAddrBorrowExportObj> &agentFilterRunningExportObjs);

void FilterRunningAddrImport(std::vector<UbseMemAddrBorrowImportObj> masterImportObjs,
                             std::vector<UbseMemAddrBorrowImportObj> agentImportObjs,
                             std::vector<UbseMemAddrBorrowImportObj> &masterRunningImportObjs,
                             std::vector<UbseMemAddrBorrowImportObj> &masterFilterRunningImportObjs,
                             std::vector<UbseMemAddrBorrowImportObj> &agentFilterRunningImportObjs);

void FilterRunningShareExport(std::vector<UbseMemShareBorrowExportObj> masterExportObjs,
                              std::vector<UbseMemShareBorrowExportObj> agentExportObjs,
                              std::vector<UbseMemShareBorrowExportObj> &masterRunningExportObjs,
                              std::vector<UbseMemShareBorrowExportObj> &masterFilterRunningExportObjs,
                              std::vector<UbseMemShareBorrowExportObj> &agentFilterRunningExportObjs);

void FilterRunningShareImport(std::vector<UbseMemShareBorrowImportObj> masterImportObjs,
                              std::vector<UbseMemShareBorrowImportObj> agentImportObjs,
                              std::vector<UbseMemShareBorrowImportObj> &masterRunningImportObjs,
                              std::vector<UbseMemShareBorrowImportObj> &masterFilterRunningImportObjs,
                              std::vector<UbseMemShareBorrowImportObj> &agentFilterRunningImportObjs);

std::vector<UbseMemFdBorrowExportObj> FilterBothExistsFdExport(std::vector<UbseMemFdBorrowExportObj> masterExportObjs,
                                                               std::vector<UbseMemFdBorrowExportObj> agentExportObjs);

std::vector<UbseMemNumaBorrowExportObj> FilterBothExistsNumaExport(
    std::vector<UbseMemNumaBorrowExportObj> masterExportObjs, std::vector<UbseMemNumaBorrowExportObj> agentExportObjs);

std::vector<UbseMemAddrBorrowExportObj> FilterBothExistsAddrExport(
    std::vector<UbseMemAddrBorrowExportObj> masterExportObjs, std::vector<UbseMemAddrBorrowExportObj> agentExportObjs);

void FilterFdDifferentExportSet(std::vector<UbseMemFdBorrowExportObj> masterExportObjs,
                                std::vector<UbseMemFdBorrowExportObj> agentExportObjs,
                                std::vector<UbseMemFdBorrowExportObj> &masterDiffExportObjs,
                                std::vector<UbseMemFdBorrowExportObj> &agentDiffExportObjs);

void FilterFdDifferentImportSet(std::vector<UbseMemFdBorrowImportObj> masterImportObjs,
                                std::vector<UbseMemFdBorrowImportObj> agentImportObjs,
                                std::vector<UbseMemFdBorrowImportObj> &masterDiffImportObjs,
                                std::vector<UbseMemFdBorrowImportObj> &agentDiffImportObjs);

void FilterNumaDifferentExportSet(std::vector<UbseMemNumaBorrowExportObj> masterExportObjs,
                                  std::vector<UbseMemNumaBorrowExportObj> agentExportObjs,
                                  std::vector<UbseMemNumaBorrowExportObj> &masterDiffExportObjs,
                                  std::vector<UbseMemNumaBorrowExportObj> &agentDiffExportObjs);

void FilterNumaDifferentImportSet(std::vector<UbseMemNumaBorrowImportObj> masterImportObjs,
                                  std::vector<UbseMemNumaBorrowImportObj> agentImportObjs,
                                  std::vector<UbseMemNumaBorrowImportObj> &masterDiffImportObjs,
                                  std::vector<UbseMemNumaBorrowImportObj> &agentDiffImportObjs);

void FilterShareDifferentExportSet(std::vector<UbseMemShareBorrowExportObj> masterExportObjs,
                                   std::vector<UbseMemShareBorrowExportObj> agentExportObjs,
                                   std::vector<UbseMemShareBorrowExportObj> &masterDiffExportObjs,
                                   std::vector<UbseMemShareBorrowExportObj> &agentDiffExportObjs);

void FilterShareDifferentImportSet(std::vector<UbseMemShareBorrowImportObj> masterImportObjs,
                                   std::vector<UbseMemShareBorrowImportObj> agentImportObjs,
                                   std::vector<UbseMemShareBorrowImportObj> &masterDiffImportObjs,
                                   std::vector<UbseMemShareBorrowImportObj> &agentDiffImportObjs);

void FilterAddrDifferentExportSet(std::vector<UbseMemAddrBorrowExportObj> masterExportObjs,
                                  std::vector<UbseMemAddrBorrowExportObj> agentExportObjs,
                                  std::vector<UbseMemAddrBorrowExportObj> &masterDiffExportObjs,
                                  std::vector<UbseMemAddrBorrowExportObj> &agentDiffExportObjs);

void FilterAddrDifferentImportSet(std::vector<UbseMemAddrBorrowImportObj> masterImportObjs,
                                  std::vector<UbseMemAddrBorrowImportObj> agentImportObjs,
                                  std::vector<UbseMemAddrBorrowImportObj> &masterDiffImportObjs,
                                  std::vector<UbseMemAddrBorrowImportObj> &agentDiffImportObjs);

std::vector<UbseMemFdBorrowExportObj> TransFdExportList(UbseMemFdExportObjMap exportObjMap);

std::vector<UbseMemFdBorrowImportObj> TransFdImportList(UbseMemFdImportObjMap importObjMap);

std::vector<UbseMemNumaBorrowExportObj> TransNumaExportList(UbseMemNumaExportObjMap exportObjMap);

std::vector<UbseMemNumaBorrowImportObj> TransNumaImportList(UbseMemNumaImportObjMap importObjMap);

std::vector<UbseMemShareBorrowExportObj> TransShareExportList(UbseMemShareExportObjMap exportObjMap);

std::vector<UbseMemShareBorrowImportObj> TransShareImportList(UbseMemShareImportObjMap importObjMap);

std::vector<UbseMemAddrBorrowExportObj> TransAddrExportList(UbseMemAddrExportObjMap exportObjMap);

std::vector<UbseMemAddrBorrowImportObj> TransAddrImportList(UbseMemAddrImportObjMap importObjMap);

} // namespace ubse::mem::controller

#endif // UBSE_MANAGER_UBSE_MEM_CONTROLLER_LEDGER_FILTER_H
