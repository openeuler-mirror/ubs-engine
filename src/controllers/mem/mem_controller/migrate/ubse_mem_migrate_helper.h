/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef UBSE_MEM_MIGRATE_HELPER_H
#define UBSE_MEM_MIGRATE_HELPER_H
#include <functional>
#include "ubse_mem_migrate_internal.h"
namespace ubse::mem::controller::migrate_detail {
// 只记录设备关联字段，不能输出块描述中的地址、授权凭证或完整对象。
std::string MigrateRpcLog(const UbseMemMigrateRpcInfo& info);
std::string MigrateExportLog(const UbseMemShareBorrowExportObj& obj);
const char* MigrateFailReasonName(uint32_t reason);
std::string MigrateFailReasonToMsg(uint32_t reason);
bool CommitSelfImportBlock(UbseMemShareBorrowImportObj& obj, const UbseMemShareBorrowExportObj& exp, uint32_t b);
// 同名锁内消费受理入口已校验的源/目标快照；targetImportObj 为空时准备普通新导出。
uint32_t PrepareMigrateExport(const ConstExportObjPtr& oldObj, const std::string& dstNodeId,
                              const ConstImportObjPtr& targetImportObj, UbseMemShareBorrowExportObj& exportObj);
void DiscardPreparedExport(const UbseMemShareBorrowExportObj& exportObj);
uint32_t CommitExportRsp(const UbseMemShareBorrowExportObj& result);
// 调用前由 master 确认新旧块数一致；校验失败仅返回 false，不推进会话或清理资源。
bool PrepareImportNodes(MigrationRunContext& ctx);
bool PrepareBeginImportRequest(const MigrationRunContext& ctx, const std::string& nodeId, uint32_t blockIndex,
                               UbseMemMigrateSwitchBeginImportReq& req);
std::vector<std::string> CollectImportNodes(const std::string& name, const std::string& panicNodeId);
uint32_t ConvergeImportObjsToMaster(const MigrationRunContext& ctx);
// 账本 UpdateResource 的返回值只表示存在，迁移写入必须确认回调实际应用。
bool UpdateMigrateImport(const std::string& nodeId, const std::string& name,
                         const std::function<bool(UbseMemShareBorrowImportObj&)>& modify);
bool UpdateMigrateExport(const std::string& nodeId, const std::string& name,
                         const std::function<bool(UbseMemShareBorrowExportObj&)>& modify);
bool IsMigrateBlockCommitted(const UbseMemShareBorrowImportObj& obj, const UbseMemMigrateRpcInfo& info);
bool MatchesSwitchContext(const UbseMemMigrateSwitchContext& context, const UbseMemMigrateRpcInfo& info);
UbseMemMigrateSwitchContext MakeSwitchContext(const UbseMemMigrateRpcInfo& info);
UbseMemMigrateBlockData ReadMigrateBlock(const UbseMemShareBorrowImportObj& obj, uint32_t blockIndex);
void ApplyEndSuccess(UbseMemShareBorrowImportObj& obj, const UbseMemMigrateRpcInfo& info,
                     const UbseMemMigrateBlockData& block);
void ApplyEndRollback(UbseMemShareBorrowImportObj& obj, uint32_t blockIndex);
// 结果入口已校验请求归属；本函数核验外部块数据并只合入当前 index。
bool MergeMigrateImportResult(const UbseMemMigrateOpResp& result, bool begin);
} // namespace ubse::mem::controller::migrate_detail
#endif
