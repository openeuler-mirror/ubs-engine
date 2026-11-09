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
#include "ubse_mem_migrate_helper.h"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller_api.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_mem_scheduler_impl.h"
#include "ubse_mem_util.h"
#include "ubse_str_util.h"
#include "../logging_lock_guard.h"
#include "../ubse_mem_controller_ledger.h"
#include "api/ubse_mem_controller_share_api.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace migrate_detail;
using namespace ubse::log;
using namespace ubse::mem::scheduler;
using namespace ubse::common::def;
using namespace ubse::mem::util;
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::utils;

namespace migrate_detail {
std::string MigrateRpcLog(const UbseMemMigrateRpcInfo& info)
{
    std::ostringstream out;
    out << "nodeId=" << info.srcNodeId << ", exportMemId=" << info.exportMemId << ", migrateInfo=" << info.migrateInfo
        << ", rollback=" << info.rollback;
    return out.str();
}

std::string MigrateExportLog(const UbseMemShareBorrowExportObj& obj)
{
    std::ostringstream out;
    out << "exportNodeId="
        << (obj.algoResult.exportNumaInfos.empty() ? "unknown" : obj.algoResult.exportNumaInfos.front().nodeId)
        << ", state=" << obj.status.state << ", resultRet=" << FormatRetCode(obj.status.errCode)
        << ", blockCount=" << obj.status.exportObmmInfo.size();
    return out.str();
}

namespace {
struct MigrateFailureDescription {
    uint32_t reason;
    const char* name;
    const char* message;
};
constexpr MigrateFailureDescription MIGRATE_FAILURES[] = {
    {MIGRATE_FAIL_ALLOCATE, "MIGRATE_FAIL_ALLOCATE", "schedule allocate failed"},
    {MIGRATE_FAIL_RPC, "MIGRATE_FAIL_RPC", "migration RPC send or admission ACK failed"},
    {MIGRATE_FAIL_OBMM_EXPORT, "MIGRATE_FAIL_OBMM_EXPORT", "agent obmm export failed"},
    {MIGRATE_FAIL_LAYOUT, "MIGRATE_FAIL_LAYOUT", "old/new block layout mismatch"},
    {MIGRATE_FAIL_OLD_OBJ_LOST, "MIGRATE_FAIL_OLD_OBJ_LOST", "old export obj lost"},
    {MIGRATE_FAIL_SIGN, "MIGRATE_FAIL_SIGN", "sign/verify failed"},
    {MIGRATE_FAIL_COPY, "MIGRATE_FAIL_COPY", "agent data copy failed"},
    {MIGRATE_FAIL_CONVERGE, "MIGRATE_FAIL_CONVERGE", "migration ledger convergence failed"},
    {MIGRATE_FAIL_RESULT_TIMEOUT, "MIGRATE_FAIL_RESULT_TIMEOUT", "migration asynchronous business result timed out"},
    {MIGRATE_FAIL_DISPATCH, "MIGRATE_FAIL_DISPATCH", "migration task dispatch failed"},
    {MIGRATE_FAIL_TIMER, "MIGRATE_FAIL_TIMER", "migration phase timer registration failed"},
    {MIGRATE_FAIL_BEGIN_IMPORT, "MIGRATE_FAIL_BEGIN_IMPORT",
     "agent begin_import failed (including decoder preparation)"},
    {MIGRATE_FAIL_BEGIN_EXPORT, "MIGRATE_FAIL_BEGIN_EXPORT", "agent begin_export failed"},
    {MIGRATE_FAIL_END_IMPORT, "MIGRATE_FAIL_END_IMPORT", "agent end_import(1) failed"},
    {MIGRATE_FAIL_END_EXPORT, "MIGRATE_FAIL_END_EXPORT", "agent end_export(1) failed"},
    {MIGRATE_FAIL_REQUEST_PREPARE, "MIGRATE_FAIL_REQUEST_PREPARE", "master migration block request preparation failed"},
    {MIGRATE_FAIL_ROLLBACK_IMPORT, "MIGRATE_FAIL_ROLLBACK_IMPORT", "agent end_import(0) rollback failed"},
    {MIGRATE_FAIL_ROLLBACK_EXPORT, "MIGRATE_FAIL_ROLLBACK_EXPORT", "agent end_export(0) rollback failed"},
};

const MigrateFailureDescription* FindMigrateFailure(uint32_t reason)
{
    for (const auto& failure : MIGRATE_FAILURES) {
        if (failure.reason == reason) {
            return &failure;
        }
    }
    return nullptr;
}
} // namespace

const char* MigrateFailReasonName(uint32_t reason)
{
    const auto* failure = FindMigrateFailure(reason);
    return failure ? failure->name : (reason == UBSE_OK ? "NONE" : "UNKNOWN");
}

std::string MigrateFailReasonToMsg(uint32_t reason)
{
    const auto* failure = FindMigrateFailure(reason);
    return failure ? failure->message : "migrate failed, failReason=" + std::to_string(reason);
}

static UbseMemShareBorrowReq BuildMigrateReq(const UbseMemShareBorrowExportObj& oldObj, const std::string& dstNodeId)
{
    auto req = oldObj.req;
    UbseMemLenderInfo lenderInfo = {
        .lender_size = req.size,
        .nodeId = dstNodeId,
    };
    req.lenderInfo = lenderInfo;
    req.requestNodeId = GetCurNodeId(); // master 自身
    return req;
}

std::vector<std::string> CollectImportNodes(const std::string& name, const std::string& panicNodeId)
{
    auto importObjs = GetAllImportObjsByName(name);
    std::vector<std::string> nodes;
    for (const auto& importObj : importObjs) {
        if (importObj->status.state != UBSE_MEM_IMPORT_SUCCESS || importObj->status.importResults.empty() ||
            importObj->importNodeId == panicNodeId) {
            continue;
        }
        nodes.push_back(importObj->importNodeId);
    }
    return nodes;
}

static uint32_t SetNextMigrateMarkers(const UbseMemShareBorrowExportObj& oldObj,
                                      UbseMemShareBorrowExportObj& newExportObj)
{
    const auto& srcNodeId = oldObj.algoResult.exportNumaInfos.front().nodeId;
    const auto count = oldObj.status.exportObmmInfo.size();
    const auto& oldInfos = oldObj.migrateInfos;
    uint32_t originExportNodeId = 0;
    if (ConvertStrToUint32(srcNodeId, originExportNodeId) != UBSE_OK || originExportNodeId == 0 ||
        originExportNodeId > SHM_MIGRATE_MAX_ORIGIN_NODE_ID || count > SHM_MIGRATE_MAX_BLOCKS ||
        (!oldInfos.empty() && oldInfos.size() != count)) {
        UBSE_LOG_ERROR << "Migrate source node or block markers invalid, name=" << oldObj.req.name
                       << ", srcNodeId=" << srcNodeId << ", maxOriginNodeId=" << SHM_MIGRATE_MAX_ORIGIN_NODE_ID
                       << ", blocks=" << count << ", markers=" << oldInfos.size();
        return UBSE_ERROR_INVAL;
    }
    const auto version = oldInfos.empty() ? 1U : (ShmMigrateVersion(oldInfos.front()) + 1U) & 0xfU;
    newExportObj.migrateInfos.clear();
    for (size_t b = 0; b < count; ++b) {
        if (!oldInfos.empty() && ShmMigrateVersion(oldInfos[b]) != ShmMigrateVersion(oldInfos.front())) {
            UBSE_LOG_ERROR << "Migrate source block versions inconsistent, name=" << oldObj.req.name
                           << ", indexId=" << b << ", version=" << ShmMigrateVersion(oldInfos[b])
                           << ", expectedVersion=" << ShmMigrateVersion(oldInfos.front());
            return UBSE_ERROR_INVAL;
        }
        newExportObj.migrateInfos.push_back(MakeShmMigrateInfo(version, b, originExportNodeId, true));
    }
    return UBSE_OK;
}

static uint32_t AllocateNewExport(const UbseMemShareBorrowExportObj& oldObj, const std::string& dstNodeId,
                                  UbseMemShareBorrowExportObj& newExportObj)
{
    // 标记只依赖旧账本；在调度预留容量前校验，避免标记无效时还需释放预留。
    if (SetNextMigrateMarkers(oldObj, newExportObj) != UBSE_OK) {
        return UBSE_ERROR_INVAL;
    }
    const auto newReq = BuildMigrateReq(oldObj, dstNodeId);
    auto ret = ShareBorrowSchedule(newReq, newExportObj, true);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Migrate target scheduling failed, name=" << oldObj.req.name << ", dstNodeId=" << dstNodeId
                       << ", ret=" << FormatRetCode(ret);
        return ret;
    }
    if (newExportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "shm migrate new borrow allocate result mismatch, name=" << oldObj.req.name
                       << ", srcNodeId=" << oldObj.algoResult.exportNumaInfos.front().nodeId
                       << ", dstNodeId=" << dstNodeId << ", exportNumaInfos empty";
        return UBSE_ERR_ALLOCATE;
    }
    return UBSE_OK;
}

uint32_t PrepareMigrateExport(const ConstExportObjPtr& oldObj, const std::string& dstNodeId,
                              const ConstImportObjPtr& targetImportObj, UbseMemShareBorrowExportObj& exportObj)
{
    UbseMemShareBorrowExportObj newExportObj;
    const auto ret = AllocateNewExport(*oldObj, dstNodeId, newExportObj);
    if (ret != UBSE_OK) {
        return ret;
    }
    if (targetImportObj) {
        newExportObj.req.baseNodeId = dstNodeId;
    }
    newExportObj.status.state = UBSE_MEM_EXPORT_RUNNING;
    newExportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    exportObj = std::move(newExportObj);
    return UBSE_OK;
}

void DiscardPreparedExport(const UbseMemShareBorrowExportObj& exportObj)
{
    if (exportObj.algoResult.exportNumaInfos.empty()) {
        UBSE_LOG_ERROR << "Cannot discard prepared migrate export: exportNumaInfos empty, name=" << exportObj.req.name
                       << ", state=" << exportObj.status.state;
        return;
    }
    auto failed = exportObj;
    failed.status.state = UBSE_MEM_STATE_FAILED;
    SchedulerImpl::GetInstance().MemoryObjChangeHandler(failed);
    const auto& dstNodeId = exportObj.algoResult.exportNumaInfos.front().nodeId;
    auto& exportMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>();
    exportMap.RemoveResource(dstNodeId, exportObj.req.name);
}

uint32_t CommitExportRsp(const UbseMemShareBorrowExportObj& result)
{
    auto nameLock = LoggingLockGuard(result.req.name);
    const auto& name = result.req.name;
    const auto& exportNodeId = result.algoResult.exportNumaInfos.front().nodeId;
    auto& exports = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>();
    const auto current = exports.GetResource(exportNodeId, name);
    if (current != nullptr && current->isCreateReportReceived) {
        UBSE_LOG_INFO << "Export result already accounted, name=" << name << ", exportNodeId=" << exportNodeId;
        return UBSE_OK;
    }
    auto ledger = result;
    ledger.isCreateReportReceived = false;
    auto obj = ledger;
    if (ledger.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        // 先登记实际结果，通知决策模块后删除账本。
        exports.PutResource(exportNodeId, name, ledger);
        obj.status.state = UBSE_MEM_STATE_FAILED;
    } else {
        ShareExportUpdateState(ledger, UBSE_MEM_EXPORT_SUCCESS);
    }
    SchedulerImpl::GetInstance().MemoryObjChangeHandler(obj);
    if (result.status.state == UBSE_MEM_EXPORT_DESTROYED) {
        EraseShareExport(ledger);
        return UBSE_OK;
    }
    return UpdateMigrateExport(exportNodeId, name,
                               [](UbseMemShareBorrowExportObj& ledger) {
                                   ledger.isCreateReportReceived = true;
                                   return true;
                               }) ?
               UBSE_OK :
               UBSE_ERR_INTERNAL;
}

bool PrepareImportNodes(MigrationRunContext& ctx)
{
    for (size_t b = 0; b < ctx.newExportObj.status.exportObmmInfo.size(); ++b) {
        if (ctx.newExportObj.status.exportObmmInfo[b].memId == 0 ||
            ctx.newExportObj.status.exportObmmInfo[b].desc.length == 0) {
            UBSE_LOG_ERROR << "Invalid migrate export block, name=" << ctx.name << ", indexId=" << b
                           << ", targetNodeId=" << ctx.dstNodeId
                           << ", length=" << ctx.newExportObj.status.exportObmmInfo[b].desc.length;
            return false;
        }
    }
    ctx.importNodes = CollectImportNodes(ctx.name, ctx.srcNodeId);
    return true;
}

uint32_t ConvergeImportObjsToMaster(const MigrationRunContext& ctx)
{
    const auto& importNodes = ctx.importNodes;
    const auto blockCount = static_cast<uint32_t>(ctx.newExportObj.status.exportObmmInfo.size());
    for (size_t n = 0; n < importNodes.size(); n++) {
        const bool updated = UpdateMigrateImport(importNodes[n], ctx.name, [&](UbseMemShareBorrowImportObj& obj) {
            if (obj.exportObmmInfo.size() != blockCount || obj.migrateInfos.size() != blockCount) {
                UBSE_LOG_ERROR << "Converge import layout mismatch, name=" << ctx.name << ", nodeId=" << importNodes[n]
                               << ", expectedBlocks=" << blockCount << ", actualBlocks=" << obj.exportObmmInfo.size()
                               << ", markerCount=" << obj.migrateInfos.size();
                return false;
            }
            for (size_t b = 0; b < blockCount; ++b) {
                if (obj.exportObmmInfo[b].memIdStatus != UB_MEM_HEALTHY ||
                    obj.exportObmmInfo[b].memId != ctx.newExportObj.status.exportObmmInfo[b].memId ||
                    obj.migrateInfos[b] != ShmMigrateImportInfo(ctx.newExportObj.migrateInfos[b])) {
                    UBSE_LOG_ERROR << "Converge import block not committed or identity mismatch, name=" << ctx.name
                                   << ", indexId=" << b << ", nodeId=" << importNodes[n]
                                   << ", status=" << obj.exportObmmInfo[b].memIdStatus << ", exportIdentityMatches="
                                   << (obj.exportObmmInfo[b].memId == ctx.newExportObj.status.exportObmmInfo[b].memId)
                                   << ", expectedMarker=" << ShmMigrateImportInfo(ctx.newExportObj.migrateInfos[b])
                                   << ", actualMarker=" << obj.migrateInfos[b];
                    return false;
                }
            }
            obj.algoResult.exportNumaInfos = ctx.newExportObj.algoResult.exportNumaInfos;
            obj.algoResult.blockSize = ctx.newExportObj.algoResult.blockSize;
            obj.req.baseNodeId = ctx.dstNodeId;
            obj.req.lenderInfo.nodeId = ctx.dstNodeId;
            return true;
        });
        if (!updated) {
            UBSE_LOG_ERROR << "Converge import ledger failed or missing, name=" << ctx.name
                           << ", nodeId=" << importNodes[n];
            return UBSE_ERR_INTERNAL;
        }
    }
    return UBSE_OK;
}

bool CommitSelfImportBlock(UbseMemShareBorrowImportObj& obj, const UbseMemShareBorrowExportObj& exp, uint32_t b)
{
    if (b >= obj.status.importResults.size() || b >= obj.exportObmmInfo.size() || b >= obj.migrateInfos.size() ||
        b >= exp.status.exportObmmInfo.size() || b >= exp.migrateInfos.size() ||
        obj.status.importResults[b].memId != exp.status.exportObmmInfo[b].memId) {
        UBSE_LOG_ERROR << "name=" << obj.req.name << ", indexId=" << b
                       << ", operation=end_export, Self import commit layout mismatch";
        return false;
    }
    if (b + 1 == exp.migrateInfos.size() &&
        !std::all_of(exp.status.exportObmmInfo.begin(), exp.status.exportObmmInfo.end(), [](const auto& block) {
            return block.memIdStatus == UB_MEM_HEALTHY && block.desc.addr != 0 && block.desc.length != 0;
        })) {
        UBSE_LOG_ERROR << "name=" << obj.req.name << ", indexId=" << b
                       << ", operation=end_export, Self import has uncommitted blocks";
        return false;
    }
    obj.exportObmmInfo[b] = exp.status.exportObmmInfo[b];
    obj.migrateInfos[b] = ShmMigrateImportInfo(exp.migrateInfos[b]);
    if (b < obj.status.decoderResult.size()) {
        obj.status.decoderResult[b] = {};
    }
    if (b + 1 == exp.migrateInfos.size()) {
        obj.realExe = false;
        obj.status.decoderResult.clear();
        obj.algoResult.exportNumaInfos = exp.algoResult.exportNumaInfos;
        obj.req.trustRingData = exp.req.trustRingData;
        obj.req.baseNodeId = obj.importNodeId;
        obj.req.lenderInfo.nodeId = obj.importNodeId;
        obj.status.state = UBSE_MEM_IMPORT_SUCCESS;
        obj.status.expectState = UBSE_MEM_IMPORT_SUCCESS;
    }
    return true;
}

bool PrepareBeginImportRequest(const MigrationRunContext& ctx, const std::string& nodeId, uint32_t b,
                               UbseMemMigrateSwitchBeginImportReq& req)
{
    const auto local =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(nodeId, ctx.name);
    if (local == nullptr || b >= local->exportObmmInfo.size()) {
        UBSE_LOG_ERROR << "name=" << ctx.name << ", indexId=" << b
                       << ", operation=begin_import, Prepare migrate begin missing import block, nodeId=" << nodeId;
        return false;
    }
    // 链路计算只消费当前块，避免每个节点、每个块再复制整份导入对象。
    UbseMemBorrowImportBaseObj link{};
    link.algoResult.exportNumaInfos = ctx.newExportObj.algoResult.exportNumaInfos;
    link.algoResult.blockSize = ctx.newExportObj.algoResult.blockSize;
    link.exportObmmInfo = {ctx.newExportObj.status.exportObmmInfo[b]};
    if (GetCnaInfoWhenImport(ctx.dstNodeId, nodeId, link, false, UINT32_MAX) != UBSE_OK) {
        UBSE_LOG_ERROR << "name=" << ctx.name << ", indexId=" << b
                       << ", operation=begin_import, Prepare migrate block link failed, nodeId=" << nodeId;
        return false;
    }
    // 与 attach 一致，导入布局由新导出 NUMA 和新链路构造，不能沿用旧导入布局。
    const auto& exportNuma = link.algoResult.exportNumaInfos.front();
    link.algoResult.importNumaInfos.push_back(
        {nodeId, static_cast<int>(link.algoResult.attachSocketId), exportNuma.numaId, exportNuma.size});
    FillImportNumaPortAndChipId(exportNuma.nodeId, exportNuma.socketId, nodeId, link.algoResult.importNumaInfos);
    req.targetBlock = link.exportObmmInfo.front();
    req.targetBlock.memIdStatus = UB_MEM_SWITCHING;
    req.targetLayout = std::move(link.algoResult);
    req.oldExportMemId = local->exportObmmInfo[b].memId;
    const auto& trust = ctx.newExportObj.req.trustRingData;
    req.trustRingData.trustRingId = trust.trustRingId;
    req.trustRingData.reqSignedData = trust.reqSignedData;
    if (!trust.lendSignedDatas.empty()) {
        if (b >= trust.lendSignedDatas.size()) {
            UBSE_LOG_ERROR << "name=" << ctx.name << ", indexId=" << b
                           << ", operation=begin_import, Migrate block signature missing";
            return false;
        }
        req.trustRingData.lendSignedDatas = {trust.lendSignedDatas[b]};
    }
    return true;
}

template <typename T>
static bool UpdateMigrateLedger(const std::string& nodeId, const std::string& name,
                                const std::function<bool(T&)>& modify)
{
    bool applied = false;
    UbseMemDebtLedger::GetInstance().GetDebtMap<T>().UpdateResource(nodeId, name, [&](T& obj) {
        applied = modify(obj);
        return applied;
    });
    if (!applied) {
        UBSE_LOG_ERROR << "Migrate ledger update not applied, name=" << name << ", nodeId=" << nodeId;
    }
    return applied;
}

bool UpdateMigrateImport(const std::string& nodeId, const std::string& name,
                         const std::function<bool(UbseMemShareBorrowImportObj&)>& modify)
{
    return UpdateMigrateLedger(nodeId, name, modify);
}

bool UpdateMigrateExport(const std::string& nodeId, const std::string& name,
                         const std::function<bool(UbseMemShareBorrowExportObj&)>& modify)
{
    return UpdateMigrateLedger(nodeId, name, modify);
}

bool MatchesSwitchContext(const UbseMemMigrateSwitchContext& context, const UbseMemMigrateRpcInfo& info)
{
    return context.blockIndex == info.blockIndex && context.exportMemId == info.exportMemId &&
           context.migrateInfo == info.migrateInfo;
}

UbseMemMigrateSwitchContext MakeSwitchContext(const UbseMemMigrateRpcInfo& info)
{
    UbseMemMigrateSwitchContext context{};
    context.blockIndex = info.blockIndex;
    context.exportMemId = info.exportMemId;
    context.migrateInfo = info.migrateInfo;
    return context;
}

bool IsMigrateBlockCommitted(const UbseMemShareBorrowImportObj& obj, const UbseMemMigrateRpcInfo& info)
{
    const auto b = info.blockIndex;
    return b < obj.exportObmmInfo.size() && b < obj.migrateInfos.size() &&
           obj.exportObmmInfo[b].memIdStatus == UB_MEM_HEALTHY && obj.exportObmmInfo[b].memId == info.exportMemId &&
           obj.migrateInfos[b] == ShmMigrateImportInfo(info.migrateInfo);
}

UbseMemMigrateBlockData ReadMigrateBlock(const UbseMemShareBorrowImportObj& obj, uint32_t b)
{
    UbseMemMigrateBlockData block{};
    block.exportInfo = obj.exportObmmInfo[b];
    if (b < obj.status.decoderResult.size()) {
        block.decoder = obj.status.decoderResult[b];
    }
    block.targetLayout = obj.algoResult;
    return block;
}

void ApplyEndSuccess(UbseMemShareBorrowImportObj& obj, const UbseMemMigrateRpcInfo& info,
                     const UbseMemMigrateBlockData& block)
{
    const auto b = info.blockIndex;
    obj.exportObmmInfo[b] = block.exportInfo;
    obj.exportObmmInfo[b].memIdStatus = UB_MEM_HEALTHY;
    obj.status.decoderResult[b] = *block.decoder;
    obj.migrateInfos[b] = ShmMigrateImportInfo(info.migrateInfo);
    if (b + 1 == obj.status.importResults.size()) {
        obj.algoResult = block.targetLayout;
        obj.req.baseNodeId = block.targetLayout.exportNumaInfos.front().nodeId;
        obj.req.lenderInfo.nodeId = obj.req.baseNodeId;
    }
}

void ApplyEndRollback(UbseMemShareBorrowImportObj& obj, uint32_t b)
{
    if (obj.exportObmmInfo[b].memIdStatus == UB_MEM_SWITCHING) {
        obj.exportObmmInfo[b].memIdStatus = UB_MEM_HEALTHY;
    }
}

bool MergeMigrateImportResult(const UbseMemMigrateOpResp& result, bool begin)
{
    const auto& info = result.rpcInfo;
    if (!result.block) {
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << info.blockIndex
                       << ", operation=" << (begin ? "begin_import" : "end_import")
                       << ", Migrate result missing block data, nodeId=" << info.srcNodeId;
        return false;
    }
    const auto& block = *result.block;
    return UpdateMigrateImport(info.srcNodeId, info.name, [&](auto& obj) {
        const auto b = info.blockIndex;
        const bool commit = result.committed || (!begin && !info.rollback);
        if (b >= obj.exportObmmInfo.size() || b >= obj.status.decoderResult.size() || b >= obj.migrateInfos.size() ||
            b >= obj.status.importResults.size() || block.exportInfo.desc.length != obj.exportObmmInfo[b].desc.length ||
            ((commit || !info.rollback) &&
             (block.exportInfo.memId != info.exportMemId || !block.decoder || block.decoder->hpa == 0 ||
              block.targetLayout.exportNumaInfos.empty() || block.targetLayout.importNumaInfos.empty()))) {
            UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << info.blockIndex
                           << ", operation=" << (begin ? "begin_import" : "end_import")
                           << ", Invalid migrate result block, nodeId=" << info.srcNodeId;
            return false;
        }
        if (!commit && !begin && block.exportInfo.memId != obj.exportObmmInfo[b].memId) {
            UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << info.blockIndex
                           << ", operation=end_import, Rollback result old reference mismatch, nodeId="
                           << info.srcNodeId;
            return false;
        }
        if (!obj.switchContext || !MatchesSwitchContext(*obj.switchContext, info)) {
            obj.switchContext = MakeSwitchContext(info);
        }
        if (commit) {
            ApplyEndSuccess(obj, info, block);
            obj.switchContext->endDecision = true;
            obj.switchContext->target.reset();
        } else if (begin) {
            obj.switchContext->target = block;
            obj.exportObmmInfo[b].memIdStatus = UB_MEM_SWITCHING;
        } else {
            ApplyEndRollback(obj, b);
            obj.switchContext->endDecision = false;
            obj.switchContext->target.reset();
        }
        return true;
    });
}
} // namespace migrate_detail
} // namespace ubse::mem::controller
