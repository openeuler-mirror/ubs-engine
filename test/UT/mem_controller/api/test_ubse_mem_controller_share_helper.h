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
#ifndef TEST_UBSE_MEM_CONTROLLER_SHARE_HELPER_H
#define TEST_UBSE_MEM_CONTROLLER_SHARE_HELPER_H
#include "ubse_com_module.h"
#include "ubse_mem_controller_api.h"
namespace ubse::mem_controller::share::ut {
using ubse::adapter_plugins::mmi::UbseMemShareBorrowExportObj;
using ubse::adapter_plugins::mmi::UbseMemShareBorrowImportObj;
using ubse::adapter_plugins::mmi::UbseMemState;
void SendShareExportObjMockSet();
void BuildOperationSuccessMock(std::shared_ptr<com::UbseComModule>& module);
void PutShareExportObj(const std::string& nodeId, const std::string& name, const UbseMemShareBorrowExportObj& obj);
void PutShareImportObj(const std::string& nodeId, const std::string& name, const UbseMemShareBorrowImportObj& obj);
std::shared_ptr<const UbseMemShareBorrowExportObj> GetShareExportObj(const std::string& nodeId,
                                                                     const std::string& name);
std::shared_ptr<const UbseMemShareBorrowImportObj> GetShareImportObj(const std::string& nodeId,
                                                                     const std::string& name);
void ExportCallbackExportObjSet(UbseMemShareBorrowExportObj& exportObj, const UbseMemState& memState,
                                const UbseMemState& expectMemState);
void ImportCallbackImportObjSet(UbseMemShareBorrowImportObj& importObj, UbseMemShareBorrowExportObj& exportObj,
                                const UbseMemState& memState, const UbseMemState& expectMemState);
void MasterExportCallbackMockSet();
} // namespace ubse::mem_controller::share::ut
#endif
