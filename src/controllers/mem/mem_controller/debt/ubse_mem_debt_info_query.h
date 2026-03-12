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
#include "ubse_mem_controller_def.h"

namespace ubse::mem::controller::debt {
using namespace ubse::mem::def;
uint32_t UbseMemFdGet(const UbseMemDebtQueryRequest &request, UbseMemFdDesc &fdDesc);

uint32_t UbseMemFdList(const UbseMemDebtQueryRequest &request, std::vector<UbseMemFdDesc> &fdDescs);

uint32_t UbseMemNumaGet(const UbseMemDebtQueryRequest &request, def::UbseMemNumaDesc &numaDesc);

uint32_t UbseMemNumaList(const UbseMemDebtQueryRequest &request, std::vector<def::UbseMemNumaDesc> &numaDescs);

uint32_t UbseMemNumaGetWithImportNode(const UbseMemDebtQueryRequest &request, UbseMemNumaDesc &numaDesc);

uint32_t UbseMemShmGet(const UbseMemDebtQueryRequest &request, UbseMemShmDesc &shmDesc);

uint32_t UbseMemShmList(const UbseMemDebtQueryRequest &request, std::vector<UbseMemShmDesc> &shmDescs);

uint32_t UbseMemShmStatusGet(const UbseMemDebtQueryRequest &request, UbseMemShmMemStatusDesc &shmStatusDesc);

uint32_t UbseMemAddrGet(const UbseMemDebtQueryRequest &request, UbseMemAddrDesc &desc);

UbseMemResult GetFdStageByObj(const std::string &name, const std::string &importNodeId,
                              const NodeMemDebtInfoMap &nodeMemDebtInfoMap);

UbseMemResult GetNumaStageByObj(const std::string &name, const std::string &importNodeId,
                                const NodeMemDebtInfoMap &debtInfoMap);

UbseMemResult GetShmExportStageByObj(const std::string &name, const NodeMemDebtInfoMap &debtInfoMap);

UbseMemResult GetShmImportStageByObj(const std::string &name, const std::string &importNodeId,
                                     const NodeMemDebtInfoMap &debtInfoMap);

UbseMemResult GetAddrStageByObj(const std::string &name, const std::string &importNodeId,
                                const NodeMemDebtInfoMap &debtInfoMap);

uint32_t UbseMemNodeBorrowQuery(std::vector<UbseNodeBorrowInfo> &nodeBorrowInfo);

UbseMemFdBorrowExportObj UbseFdExportObjGet(const std::string &nodeId, const std::string &name,
                                            const std::string &importNodeId, bool isFromTaskManager = false);

UbseMemNumaBorrowExportObj UbseNumaExportObjGet(const std::string &nodeId, const std::string &name,
                                                const std::string &importNodeId, bool isFromTaskManager = false);

UbseMemShareBorrowExportObj UbseShareExportObjGet(const std::string &nodeId, const std::string &name,
                                                  bool isFromTaskManager = false);

UbseMemAddrBorrowExportObj UbseAddrExportObjGet(const std::string &nodeId, const std::string &name,
                                                const std::string &importNodeId, bool isFromTaskManager = false);

UbseMemFdBorrowImportObj UbseFdImportObjGet(const std::string &nodeId, const std::string &name,
                                            bool isFromTaskManager = false);

UbseMemNumaBorrowImportObj UbseNumaImportObjGet(const std::string &nodeId, const std::string &name,
                                                bool isFromTaskManager = false);

UbseMemShareBorrowImportObj UbseShareImportObjGet(const std::string &nodeId, const std::string &name,
                                                  bool isFromTaskManager = false);

UbseMemAddrBorrowImportObj UbseAddrImportObjGet(const std::string &nodeId, const std::string &name,
                                                bool isFromTaskManager = false);
} // namespace ubse::mem::controller::debt
#endif // UBS_ENGINE_UBSE_MEM_DEBT_INFO_QUERY_H
