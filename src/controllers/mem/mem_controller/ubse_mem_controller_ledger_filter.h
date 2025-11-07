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

#ifndef UBSE_MEM_CONTROLLER_LEDGER_FILTER_H
#define UBSE_MEM_CONTROLLER_LEDGER_FILTER_H

#include "ubse_mem_obj.h"

#include "ubse_common_def.h"
#include "ubse_mem_resource.h"
#include "ubse_error.h"

namespace ubse::mem::controller {
using namespace ubse::mem::obj;
using namespace ubse::common::def;
using namespace ubse::resource::mem;

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

std::vector<UbseMemFdBorrowExportObj> FilterBothExistsFdExport(std::vector<UbseMemFdBorrowExportObj> masterExportObjs,
                                                               std::vector<UbseMemFdBorrowExportObj> agentExportObjs);

std::vector<UbseMemNumaBorrowExportObj> FilterBothExistsNumaExport(
    std::vector<UbseMemNumaBorrowExportObj> masterExportObjs, std::vector<UbseMemNumaBorrowExportObj> agentExportObjs);

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

std::vector<UbseMemFdBorrowExportObj> TransFdExportList(UbseMemFdExportObjMap exportObjMap);

std::vector<UbseMemFdBorrowImportObj> TransFdImportList(UbseMemFdImportObjMap importObjMap);

std::vector<UbseMemNumaBorrowExportObj> TransNumaExportList(UbseMemNumaExportObjMap exportObjMap);

std::vector<UbseMemNumaBorrowImportObj> TransNumaImportList(UbseMemNumaImportObjMap importObjMap);

} // namespace ubse::mem::controller

#endif // UBSE_MEM_CONTROLLER_LEDGER_FILTER_H
