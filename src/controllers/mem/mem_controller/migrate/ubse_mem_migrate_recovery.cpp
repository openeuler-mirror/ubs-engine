/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * See the Mulan PSL v2 for more details.
 */
#include "ubse_mem_controller_migrate_api.h"

#include <algorithm>
#include <limits>
#include <set>
#include "ubse_context.h"
#include "ubse_election_module.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller_ledger.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_mem_migrate_helper.h"
#include "ubse_node.h"
#include "ubse_node_controller.h"
#include "../logging_lock_guard.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace migrate_detail;
using namespace debt;
using namespace ubse::adapter_plugins::mmi;

namespace migrate_detail {
RecoveryBlockState ReadRecoveryBlock(const MigrationRunContext& ctx, const std::string& nodeId, uint32_t b)
{
    const auto obj =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(nodeId, ctx.name);
    if (!obj || b >= obj->exportObmmInfo.size() || b >= obj->migrateInfos.size() ||
        b >= obj->status.importResults.size() || obj->status.importResults[b].memId == 0) {
        UBSE_LOG_WARN << "Recovery block information incomplete, name=" << ctx.name << ", nodeId=" << nodeId
                      << ", block=" << b << ", importPresent=" << (obj != nullptr)
                      << ", exportReferenceCount=" << (obj ? obj->exportObmmInfo.size() : 0)
                      << ", markerCount=" << (obj ? obj->migrateInfos.size() : 0)
                      << ", localImportCount=" << (obj ? obj->status.importResults.size() : 0) << ", localMemId="
                      << (obj && b < obj->status.importResults.size() ? obj->status.importResults[b].memId : 0);
        return RecoveryBlockState::INCOMPLETE;
    }
    if (ShmMigrateIndex(obj->migrateInfos[b]) != b) {
        UBSE_LOG_WARN << "Recovery block index mismatch, name=" << ctx.name << ", nodeId=" << nodeId
                      << ", expectedIndex=" << b << ", actualIndex=" << ShmMigrateIndex(obj->migrateInfos[b])
                      << ", localMemId=" << obj->status.importResults[b].memId;
        return RecoveryBlockState::INCOMPLETE; // 混合身份恢复出的残缺分组尚未按完整index对齐。
    }
    if (obj->status.state != UBSE_MEM_IMPORT_SUCCESS || b >= ctx.newExportObj.migrateInfos.size()) {
        UBSE_LOG_WARN << "Recovery import state or target marker invalid, name=" << ctx.name << ", nodeId=" << nodeId
                      << ", block=" << b << ", importState=" << obj->status.state
                      << ", expectedImportState=" << UBSE_MEM_IMPORT_SUCCESS
                      << ", targetMarkerCount=" << ctx.newExportObj.migrateInfos.size();
        return RecoveryBlockState::INVALID;
    }
    const auto& block = obj->exportObmmInfo[b];
    if (block.desc.length == 0) {
        UBSE_LOG_WARN << "Recovery block length missing, name=" << ctx.name << ", nodeId=" << nodeId << ", block=" << b
                      << ", localMemId=" << obj->status.importResults[b].memId << ", exportMemId=" << block.memId
                      << ", length=" << block.desc.length;
        return RecoveryBlockState::INCOMPLETE;
    }
    const auto marker = ctx.newExportObj.migrateInfos[b];
    if (block.memIdStatus != UB_MEM_HEALTHY && block.memIdStatus != UB_MEM_SWITCHING) {
        UBSE_LOG_WARN << "Recovery block unavailable, name=" << ctx.name << ", nodeId=" << nodeId << ", block=" << b
                      << ", localMemId=" << obj->status.importResults[b].memId << ", memIdStatus=" << block.memIdStatus
                      << ", healthyStatus=" << UB_MEM_HEALTHY << ", switchingStatus=" << UB_MEM_SWITCHING;
        return RecoveryBlockState::INVALID;
    }
    if (b < ctx.newExportObj.status.exportObmmInfo.size()) {
        const UbseMemMigrateRpcInfo info{ctx.name, b, nodeId, ctx.newExportObj.status.exportObmmInfo[b].memId, marker};
        if (IsMigrateBlockCommitted(*obj, info)) {
            return RecoveryBlockState::COMMITTED;
        }
        if (obj->switchContext && MatchesSwitchContext(*obj->switchContext, info) &&
            (obj->switchContext->endDecision.has_value() || obj->status.errCode != UBSE_OK)) {
            UBSE_LOG_WARN << "Recovery block already ending or failed, name=" << ctx.name << ", nodeId=" << nodeId
                          << ", block=" << b << ", exportMemId=" << info.exportMemId
                          << ", hasEndDecision=" << obj->switchContext->endDecision.has_value()
                          << ", endCommit=" << obj->switchContext->endDecision.value_or(false)
                          << ", errCode=" << obj->status.errCode;
            return RecoveryBlockState::INVALID;
        }
    }
    // 尚未提交的OLD和GAP都必须属于目标的上一代，兼容15到0的回绕。
    const auto nextVersion = (ShmMigrateVersion(obj->migrateInfos[b]) + 1U) & 0xfU;
    if (ShmMigrateVersion(marker) != nextVersion) {
        UBSE_LOG_WARN << "Recovery block version mismatch, name=" << ctx.name << ", nodeId=" << nodeId
                      << ", block=" << b << ", oldVersion=" << ShmMigrateVersion(obj->migrateInfos[b])
                      << ", targetVersion=" << ShmMigrateVersion(marker) << ", expectedTargetVersion=" << nextVersion;
        return RecoveryBlockState::INVALID;
    }
    if (block.memIdStatus == UB_MEM_SWITCHING) {
        // 重启后的gap只有旧priv；不补造target，实际执行时由agent核实设备状态。
        return RecoveryBlockState::GAP;
    }
    if (ctx.oldObj && b < ctx.oldObj->status.exportObmmInfo.size() &&
        ctx.oldObj->status.exportObmmInfo[b].memId != block.memId) {
        UBSE_LOG_WARN << "Recovery old export reference mismatch, name=" << ctx.name << ", nodeId=" << nodeId
                      << ", block=" << b << ", srcNodeId=" << ctx.srcNodeId
                      << ", expectedExportMemId=" << ctx.oldObj->status.exportObmmInfo[b].memId
                      << ", actualExportMemId=" << block.memId;
        return RecoveryBlockState::INVALID;
    }
    if (!ctx.oldObj && obj->req.baseNodeId != ctx.srcNodeId &&
        (obj->algoResult.exportNumaInfos.empty() || obj->algoResult.exportNumaInfos.front().nodeId != ctx.srcNodeId)) {
        UBSE_LOG_WARN << "Recovery source identity cannot be confirmed, name=" << ctx.name << ", nodeId=" << nodeId
                      << ", block=" << b << ", expectedSourceNodeId=" << ctx.srcNodeId
                      << ", baseNodeId=" << obj->req.baseNodeId
                      << ", exportNumaCount=" << obj->algoResult.exportNumaInfos.size() << ", referencedExportNodeId="
                      << (obj->algoResult.exportNumaInfos.empty() ? "" :
                                                                    obj->algoResult.exportNumaInfos.front().nodeId);
        return RecoveryBlockState::INCOMPLETE;
    }
    return RecoveryBlockState::OLD;
}
} // namespace migrate_detail

namespace {
bool HasUnfinishedBlock(const UbseMemShareBorrowImportObj& obj)
{
    return std::any_of(obj.exportObmmInfo.begin(), obj.exportObmmInfo.end(), [](const auto& block) {
        return block.memIdStatus == UB_MEM_SWITCHING || block.memIdStatus == UB_MEM_STRANDED;
    });
}

// 源节点旧导出仍存在且版本与本导出构成新旧配对，才可作为回退依据。
std::shared_ptr<const UbseMemShareBorrowExportObj> GetValidOldExport(
    UbseMemTypeDebtMap<UbseMemShareBorrowExportObj>& exports, const UbseMemShareBorrowExportObj& obj,
    const std::string& name, const std::string& src)
{
    const auto old = exports.GetResource(src, name);
    const auto version = ShmMigrateVersion(obj.migrateInfos.front());
    if (old && !old->migrateInfos.empty() && version == ((ShmMigrateVersion(old->migrateInfos.front()) + 1U) & 0xfU) &&
        old->status.state != UBSE_MEM_EXPORT_DESTROYED) {
        return old;
    }
    return nullptr;
}

// 导入侧switchContext仍指向本轮迁移且未下发end，或存在下一代导入痕迹，说明迁移尚未完成。
bool HasUnfinishedImport(const std::vector<std::shared_ptr<const UbseMemShareBorrowImportObj>>& imports,
                         const UbseMemShareBorrowExportObj& obj)
{
    const auto version = ShmMigrateVersion(obj.migrateInfos.front());
    return std::any_of(imports.begin(), imports.end(), [&](const auto& imp) {
        if (imp->switchContext &&
            std::find(obj.migrateInfos.begin(), obj.migrateInfos.end(), imp->switchContext->migrateInfo) !=
                obj.migrateInfos.end() &&
            (!imp->switchContext->endDecision.has_value() || HasUnfinishedBlock(*imp))) {
            return true;
        }
        return std::any_of(imp->migrateInfos.begin(), imp->migrateInfos.end(),
                           [version](auto marker) { return version == ((ShmMigrateVersion(marker) + 1U) & 0xfU); });
    });
}

// 旧导出已清理时，只有全部导入块明确指向当前导出，才恢复已完成的迁移会话。
bool HasCompletedMigration(const UbseMemShareBorrowExportObj& obj, const std::vector<ConstImportObjPtr>& imports)
{
    const auto count = obj.migrateInfos.size();
    if (obj.status.state != UBSE_MEM_EXPORT_SUCCESS || count == 0 || obj.status.exportObmmInfo.size() != count ||
        imports.empty()) {
        return false;
    }
    for (const auto& imp : imports) {
        if (imp->status.state != UBSE_MEM_IMPORT_SUCCESS || imp->migrateInfos.size() != count ||
            imp->exportObmmInfo.size() != count || imp->status.importResults.size() != count) {
            return false;
        }
        for (uint32_t b = 0; b < count; ++b) {
            const auto& block = obj.status.exportObmmInfo[b];
            const UbseMemMigrateRpcInfo info{obj.req.name, b, imp->importNodeId, block.memId, obj.migrateInfos[b]};
            if (block.memIdStatus != UB_MEM_HEALTHY || imp->status.importResults[b].memId == 0 ||
                !IsMigrateBlockCommitted(*imp, info)) {
                return false;
            }
        }
    }
    return true;
}

ConstExportObjPtr FindStrandedExport(const std::string& name)
{
    auto& exports = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>();
    for (const auto& [nodeId, map] : exports.GetAllNodeMaps()) {
        const auto obj = map->Get(name);
        if (obj && obj->status.state != UBSE_MEM_EXPORT_DESTROYED &&
            std::any_of(obj->status.exportObmmInfo.begin(), obj->status.exportObmmInfo.end(),
                        [](const auto& block) { return block.memIdStatus == UB_MEM_STRANDED; })) {
            return obj;
        }
    }
    return nullptr;
}

// 目标无法确定时，仅登记有gap/STRANDED依据的现场，普通资源不创建恢复会话。
bool BuildUnresolvedRecoverySession(const std::string& name, const std::vector<ConstImportObjPtr>& imports,
                                    UbseMemShmMigrateSession& session)
{
    for (const auto& imp : imports) {
        if (!HasUnfinishedBlock(*imp)) {
            continue;
        }
        session.owner = imp->req.udsInfo;
        session.totalMemids = static_cast<uint32_t>(imp->status.importResults.size());
        return true;
    }
    if (const auto exp = FindStrandedExport(name)) {
        session.owner = exp->req.udsInfo;
        session.totalMemids = static_cast<uint32_t>(exp->migrateInfos.size());
        return true;
    }
    return false;
}

void BuildRecoveryParticipants(MigrationRunContext& ctx, UbseMemShmMigrateSession& session,
                               const std::vector<ConstImportObjPtr>& imports)
{
    const auto target =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(ctx.dstNodeId, ctx.name);
    // 同名import存在即走目标已导入路径，包含已转换后的self-import。
    ctx.targetImportObj = target;
    const bool mapped = target != nullptr;
    ctx.targetKind = session.targetKind = mapped ? UbseMemMigrateTargetKind::IMPORT_NODE :
                                                   UbseMemMigrateTargetKind::NOT_IMPORT_NODE;
    for (const auto& imp : imports) {
        if (imp->importNodeId != ctx.srcNodeId && (!mapped || imp->importNodeId != ctx.dstNodeId)) {
            ctx.importNodes.push_back(imp->importNodeId);
        }
    }
    std::sort(ctx.importNodes.begin(), ctx.importNodes.end());
}

void FillRecoveryTotals(const MigrationRunContext& ctx, UbseMemShmMigrateSession& session,
                        const std::vector<ConstImportObjPtr>& imports)
{
    session.totalMemids = static_cast<uint32_t>(ctx.newExportObj.migrateInfos.size());
    if (ctx.oldObj) {
        session.totalMemids =
            std::max(session.totalMemids, static_cast<uint32_t>(ctx.oldObj->status.exportObmmInfo.size()));
    }
    for (const auto& imp : imports) {
        for (const auto marker : imp->migrateInfos) {
            session.totalMemids = std::max(session.totalMemids, ShmMigrateIndex(marker) + 1U);
        }
    }
    for (const auto& numa : ctx.newExportObj.algoResult.exportNumaInfos) {
        if (numa.size > std::numeric_limits<uint64_t>::max() - session.totalSize) {
            UBSE_LOG_ERROR << "Migration recovery total size overflow, name=" << ctx.name
                           << ", totalSize=" << session.totalSize << ", numaSize=" << numa.size;
            session.rebuildRequired = true;
            session.failReason = MIGRATE_FAIL_RECOVERY_INCOMPLETE;
            break;
        }
        session.totalSize += numa.size;
    }
}

// mapped 后端逐块创建；重启只能读到已存在设备的标记，补齐计划但不补造设备。
uint32_t RestoreMappedMigrationMarkers(MigrationRunContext& ctx, const UbseMemShmMigrateSession& session)
{
    auto& markers = ctx.newExportObj.migrateInfos;
    const auto count = session.totalMemids;
    if (ctx.targetKind != UbseMemMigrateTargetKind::IMPORT_NODE || markers.size() >= count) {
        return UBSE_OK;
    }
    if (!ctx.targetImportObj || markers.empty() || count > 0x1000U ||
        ctx.targetImportObj->status.importResults.size() != count ||
        ctx.targetImportObj->migrateInfos.size() != count ||
        markers.size() != ctx.newExportObj.status.exportObmmInfo.size()) {
        return MIGRATE_FAIL_RECOVERY_INCOMPLETE;
    }
    const auto version = ShmMigrateVersion(markers.front());
    const auto origin = ShmMigrateOrigin(markers.front());
    for (uint32_t b = 0; b < markers.size(); ++b) {
        if (markers[b] != MakeShmMigrateInfo(version, b, origin, true)) {
            return MIGRATE_FAIL_RECOVERY_INCONSISTENT;
        }
    }
    for (uint32_t b = static_cast<uint32_t>(markers.size()); b < count; ++b) {
        markers.push_back(MakeShmMigrateInfo(version, b, origin, true));
    }
    return UBSE_OK;
}

// 实际迁移痕迹用于确认待续跑目标；历史版本需所有导入块已提交才能恢复SUCCESS会话。
bool BuildRecoveryContext(const std::string& name, MigrationRunContext& ctx, UbseMemShmMigrateSession& session)
{
    ctx.name = session.name = name;
    const auto imports = GetAllImportObjsByName(name);
    auto& exports = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>();
    const auto exportMaps = exports.GetAllNodeMaps();
    size_t candidates = 0;
    for (const auto& [nodeId, map] : exportMaps) {
        const auto obj = map->Get(name);
        if (!obj || obj->status.state == UBSE_MEM_EXPORT_DESTROYED || obj->migrateInfos.empty() ||
            ShmMigrateOrigin(obj->migrateInfos.front()) == 0) {
            continue;
        }
        const auto src = std::to_string(ShmMigrateOrigin(obj->migrateInfos.front()));
        const auto version = ShmMigrateVersion(obj->migrateInfos.front());
        const auto old = GetValidOldExport(exports, *obj, name, src);
        const bool hasOld = old != nullptr;
        const bool unfinished = HasUnfinishedImport(imports, *obj);
        if (!hasOld && !unfinished && !HasCompletedMigration(*obj, imports)) {
            continue;
        }
        if (imports.empty()) {
            UBSE_LOG_WARN << "Skip migration recovery: paired exports without import ledger, name=" << name
                          << ", srcNodeId=" << src << ", dstNodeId=" << nodeId;
            continue;
        }
        // 前次迁移的旧导出可能尚未物理删除；已有下一代指向本节点时，本节点是源而非本轮目标。
        const bool hasNextGeneration = std::any_of(exportMaps.begin(), exportMaps.end(), [&](const auto& entry) {
            const auto next = entry.second->Get(name);
            return entry.first != nodeId && next && next->status.state != UBSE_MEM_EXPORT_DESTROYED &&
                   !next->migrateInfos.empty() &&
                   std::to_string(ShmMigrateOrigin(next->migrateInfos.front())) == nodeId &&
                   ShmMigrateVersion(next->migrateInfos.front()) == ((version + 1U) & 0xfU);
        });
        if (hasNextGeneration) {
            continue;
        }
        if (++candidates > 1) {
            break;
        }
        ctx.srcNodeId = session.srcNodeId = src;
        ctx.dstNodeId = session.dstNodeId = nodeId;
        ctx.oldObj = old;
        ctx.newExportObj = *obj;
        session.owner = obj->req.udsInfo;
    }
    if (candidates == 0) {
        return BuildUnresolvedRecoverySession(name, imports, session);
    }
    if (candidates != 1) {
        // 目标不唯一，收尾仅使用各节点自己的账本，不携带任意候选的来源和目标描述。
        ctx.srcNodeId.clear();
        ctx.dstNodeId.clear();
        ctx.oldObj.reset();
        ctx.newExportObj = {};
        session.srcNodeId.clear();
        session.dstNodeId.clear();
        session.state = UBS_MEM_MIGRATE_STATE_FAILED;
        session.rebuildRequired = true;
        session.failReason = MIGRATE_FAIL_RECOVERY_TARGET_UNKNOWN;
        return true;
    }
    BuildRecoveryParticipants(ctx, session, imports);
    FillRecoveryTotals(ctx, session, imports);
    if (!session.rebuildRequired) {
        session.failReason = RestoreMappedMigrationMarkers(ctx, session);
        session.rebuildRequired = session.failReason != UBSE_OK;
    }
    ctx.exportConfirmed = ctx.newExportObj.status.state == UBSE_MEM_EXPORT_SUCCESS;
    return true;
}

bool HasStrandedBlock(const std::string& name)
{
    const auto isStranded = [](const auto& block) {
        return block.memIdStatus == UB_MEM_STRANDED;
    };
    for (const auto& obj : GetAllImportObjsByName(name)) {
        if (obj->status.state != UBSE_MEM_IMPORT_DESTROYED &&
            std::any_of(obj->exportObmmInfo.begin(), obj->exportObmmInfo.end(), isStranded)) {
            return true;
        }
    }
    return FindStrandedExport(name) != nullptr;
}

uint32_t CheckRecoveryResources(const MigrationRunContext& ctx, const UbseMemShmMigrateSession& session,
                                const std::vector<std::string>& participants)
{
    const auto count = ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE && ctx.targetImportObj ?
                           ctx.targetImportObj->status.importResults.size() :
                           ctx.newExportObj.status.exportObmmInfo.size();
    if (count != session.totalMemids || ctx.newExportObj.migrateInfos.size() != session.totalMemids ||
        ctx.newExportObj.status.exportObmmInfo.size() > session.totalMemids ||
        ctx.newExportObj.algoResult.exportNumaInfos.empty()) {
        return MIGRATE_FAIL_RECOVERY_INCOMPLETE;
    }
    for (const auto& block : ctx.newExportObj.status.exportObmmInfo) {
        if (block.memId == 0 || block.desc.length == 0) {
            return MIGRATE_FAIL_RECOVERY_INCOMPLETE;
        }
    }
    const auto nodeWorking = [](const std::string& nodeId) {
        const auto info = ubse::nodeController::UbseNodeController::GetInstance().GetNodeById(nodeId);
        return info.clusterState == ubse::nodeController::UbseNodeClusterState::UBSE_NODE_WORKING;
    };
    for (const auto& node : participants) {
        if (!nodeWorking(node)) {
            return MIGRATE_FAIL_RECOVERY_NODE_UNAVAILABLE;
        }
    }
    // 离线共享域成员可能仍有未上报的导入引用，不能把未采集到当作没有导入。
    for (const auto& member : ctx.newExportObj.req.shmRegion.nodelist) {
        const auto memberId = member.nodeId.empty() ? std::to_string(member.index + 1) : member.nodeId;
        if (memberId == ctx.srcNodeId) {
            continue;
        }
        if (!nodeWorking(memberId)) {
            return MIGRATE_FAIL_RECOVERY_NODE_UNAVAILABLE;
        }
    }
    return UBSE_OK;
}

uint32_t RestoreRecoveryProgress(const MigrationRunContext& ctx, UbseMemShmMigrateSession& session)
{
    auto participants = ctx.importNodes;
    if (ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE) {
        participants.push_back(ctx.dstNodeId);
    }
    const auto resourceReason = CheckRecoveryResources(ctx, session, participants);
    if (resourceReason != UBSE_OK) {
        UBSE_LOG_WARN << "Migration recovery progress check failed at resource check, name=" << ctx.name
                      << ", srcNodeId=" << ctx.srcNodeId << ", dstNodeId=" << ctx.dstNodeId
                      << ", totalBlocks=" << session.totalMemids
                      << ", exportBlocks=" << ctx.newExportObj.status.exportObmmInfo.size()
                      << ", migrateInfos=" << ctx.newExportObj.migrateInfos.size()
                      << ", failReason=" << MigrateFailReasonName(resourceReason);
        return resourceReason;
    }
    for (uint32_t b = 0; b < session.totalMemids; ++b) {
        const auto marker = ctx.newExportObj.migrateInfos[b];
        if (!ShmMigrateIsExport(marker) || ShmMigrateIndex(marker) != b ||
            ShmMigrateOrigin(marker) != ShmMigrateOrigin(ctx.newExportObj.migrateInfos.front()) ||
            ShmMigrateVersion(marker) != ShmMigrateVersion(ctx.newExportObj.migrateInfos.front())) {
            return MIGRATE_FAIL_RECOVERY_INCONSISTENT;
        }
    }
    for (uint32_t b = 0; b < session.totalMemids; ++b) {
        // completedBlocks指向首个未完成块；该块允许混合状态，其后各块必须全为OLD。
        const bool requireOld = b > *session.completedBlocks;
        bool allCommitted = true;
        for (const auto& node : participants) {
            const auto state = ReadRecoveryBlock(ctx, node, b);
            // 缺失后端仅允许 mapped 尚未开始的后缀；GAP/COMMITTED 均必须有真实目标后端。
            const bool missingBackend = b >= ctx.newExportObj.status.exportObmmInfo.size();
            if (state == RecoveryBlockState::INCOMPLETE || state == RecoveryBlockState::INVALID ||
                (requireOld && state != RecoveryBlockState::OLD) ||
                (missingBackend &&
                 (ctx.targetKind != UbseMemMigrateTargetKind::IMPORT_NODE || state != RecoveryBlockState::OLD))) {
                const auto reason = state == RecoveryBlockState::INCOMPLETE ? MIGRATE_FAIL_RECOVERY_INCOMPLETE :
                                                                              MIGRATE_FAIL_RECOVERY_INCONSISTENT;
                UBSE_LOG_WARN << "Migration recovery progress check failed, name=" << ctx.name << ", block=" << b
                              << ", node=" << node << ", blockState=" << static_cast<int>(state)
                              << ", completedBlocks=" << *session.completedBlocks
                              << ", failReason=" << MigrateFailReasonName(reason);
                return reason;
            }
            allCommitted = allCommitted && state == RecoveryBlockState::COMMITTED;
        }
        if (!requireOld && allCommitted) {
            ++*session.completedBlocks; // 只累计连续完成前缀。
        }
    }
    return UBSE_OK;
}

void CleanupRecoveredMigration(MigrationRunContext ctx)
{
    if (ctx.dstNodeId.empty()) {
        for (const auto& obj : GetAllImportObjsByName(ctx.name)) {
            ctx.importNodes.push_back(obj->importNodeId);
        }
        std::sort(ctx.importNodes.begin(), ctx.importNodes.end());
        ctx.importNodes.erase(std::unique(ctx.importNodes.begin(), ctx.importNodes.end()), ctx.importNodes.end());
    }
    auto participants = ctx.importNodes;
    if (ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE) {
        participants.push_back(ctx.dstNodeId);
    }
    for (const auto& node : participants) {
        const auto obj =
            UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(node, ctx.name);
        if (!obj) {
            continue;
        }
        for (uint32_t b = 0; b < obj->exportObmmInfo.size(); ++b) {
            if (obj->exportObmmInfo[b].memIdStatus == UB_MEM_SWITCHING) {
                UBSE_LOG_INFO << "Dispatch recovery gap cleanup, name=" << ctx.name << ", nodeId=" << node
                              << ", block=" << b << ", dstNodeId=" << ctx.dstNodeId
                              << ", targetExportBlocks=" << ctx.newExportObj.status.exportObmmInfo.size();
                ctx.endCommitStarted = true;
                ctx.targetBeginStarted = ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE;
                CleanupMigration(std::move(ctx), b);
                return;
            }
        }
    }
    UBSE_LOG_INFO << "No switching block found for recovery cleanup, name=" << ctx.name;
}

// 返回不能续跑的具体原因；只有设备、目标和进度均可确认时才返回成功。
uint32_t CheckMigrationRecovery(const MigrationRunContext& ctx, UbseMemShmMigrateSession& session)
{
    if (HasStrandedBlock(ctx.name)) {
        return MIGRATE_FAIL_RECOVERY_STRANDED;
    }
    if (session.rebuildRequired) {
        return session.failReason; // 上下文组装阶段已经发现异常，保留其原因。
    }
    if (ctx.dstNodeId.empty()) {
        return MIGRATE_FAIL_RECOVERY_TARGET_UNKNOWN;
    }
    return RestoreRecoveryProgress(ctx, session);
}

// 会话已登记：成功则接续迁移，失败只尝试必要收尾，不重新发起迁移。
void DispatchRecoveredMigration(MigrationRunContext ctx, const UbseMemShmMigrateSession& session)
{
    const auto& name = ctx.name;
    if (session.state == UBS_MEM_MIGRATE_STATE_FAILED) {
        // FAILED不代表gap已经关闭；必要收尾不改变外部重建要求。
        UBSE_LOG_INFO << "Attempt required gap cleanup after recovery failure, name=" << name
                      << ", dstNodeId=" << ctx.dstNodeId
                      << ", failReason=" << MigrateFailReasonName(session.failReason);
        CleanupRecoveredMigration(std::move(ctx));
        return;
    }
    // 从连续完成前缀之后接续；保留提交保护，已提交块不得重拷贝或回退。
    ctx.endCommitStarted = *session.completedBlocks != 0;
    ctx.targetBeginStarted =
        ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE &&
        (*session.completedBlocks != 0 || (!ctx.newExportObj.status.exportObmmInfo.empty() &&
                                           ctx.newExportObj.status.exportObmmInfo.back().desc.addr != 0));
    UBSE_LOG_INFO << "Dispatch recovered migration, name=" << name << ", srcNodeId=" << ctx.srcNodeId
                  << ", dstNodeId=" << ctx.dstNodeId << ", nextBlock=" << *session.completedBlocks
                  << ", totalBlocks=" << session.totalMemids
                  << ", finishOnly=" << (*session.completedBlocks == session.totalMemids)
                  << ", endCommitStarted=" << ctx.endCommitStarted << ", targetBeginStarted=" << ctx.targetBeginStarted;
    ResumeMigration(std::move(ctx), *session.completedBlocks);
}

// 返回 false 表示该 name 因参与节点暂不可用暂缓处理，等待节点上线后重新触发扫描；其余路径返回 true。
bool RecoverName(const std::string& name)
{
    // 与同名迁移受理、归还共用锁，保护现场分析到会话登记的过程。
    auto nameLock = LoggingLockGuard(name);
    UBSE_LOG_INFO << "Recovering migrate ledger, name=" << name;
    auto& manager = UbseMemShmMigrateSessionManager::GetInstance();
    UbseMemShmMigrateSession existing;
    if (manager.ReadSessionSnapshot(name, existing)) {
        // 不覆盖新主已受理或已恢复的会话，也不重新尝试失败终态。
        UBSE_LOG_INFO << "Skip migration recovery: session already exists, name=" << name
                      << ", state=" << existing.state << ", completedBlocks=" << existing.completedBlocks.value_or(0)
                      << ", rebuildRequired=" << existing.rebuildRequired
                      << ", failReason=" << MigrateFailReasonName(existing.failReason);
        return true;
    }
    // 只根据已采集账本恢复源、目标和块信息，不重新调度或创建导出设备。
    MigrationRunContext ctx;
    UbseMemShmMigrateSession session;
    session.completedBlocks = 0;
    if (!BuildRecoveryContext(name, ctx, session)) {
        UBSE_LOG_INFO << "Skip migration recovery: no recoverable migration found, name=" << name;
        return true;
    }
    UBSE_LOG_INFO << "Migration recovery context collected, name=" << name << ", srcNodeId=" << ctx.srcNodeId
                  << ", dstNodeId=" << ctx.dstNodeId << ", targetKind=" << static_cast<uint32_t>(ctx.targetKind)
                  << ", sourceExportPresent=" << (ctx.oldObj != nullptr)
                  << ", importNodeCount=" << ctx.importNodes.size()
                  << ", targetExportBlocks=" << ctx.newExportObj.status.exportObmmInfo.size()
                  << ", totalBlocks=" << session.totalMemids << ", totalSize=" << session.totalSize;
    // 先明确恢复结论：可续跑，或失败并要求外部重建，不通过中间状态反推结果。
    session.failReason = CheckMigrationRecovery(ctx, session);
    if (session.failReason == MIGRATE_FAIL_RECOVERY_NODE_UNAVAILABLE) {
        // 参与节点暂不可用属于临时状态：不登记终态会话，等待节点上线后重新扫描，避免将可恢复的迁移误判为终态失败。
        UBSE_LOG_INFO << "Migration recovery deferred for unavailable participants, name=" << name
                      << ", srcNodeId=" << ctx.srcNodeId << ", dstNodeId=" << ctx.dstNodeId;
        return false;
    }
    if (session.failReason == UBSE_OK && ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE &&
        !UpdateMigrateExport(ctx.dstNodeId, name, [&](auto& obj) {
            obj.migrateInfos = ctx.newExportObj.migrateInfos;
            return true;
        })) {
        session.failReason = MIGRATE_FAIL_RECOVERY_INCOMPLETE;
    }
    session.rebuildRequired = session.failReason != UBSE_OK;
    session.state = session.rebuildRequired ? UBS_MEM_MIGRATE_STATE_FAILED : UBS_MEM_MIGRATE_STATE_DATA_COPYING;
    if (!session.rebuildRequired && !ctx.oldObj && ctx.exportConfirmed &&
        *session.completedBlocks == session.totalMemids &&
        HasCompletedMigration(ctx.newExportObj, GetAllImportObjsByName(name))) {
        session.state = UBS_MEM_MIGRATE_STATE_SUCCESS;
    }
    if (session.rebuildRequired) {
        UBSE_LOG_WARN << "Migration recovery cannot resume; external rebuild required, name=" << name
                      << ", srcNodeId=" << ctx.srcNodeId << ", dstNodeId=" << ctx.dstNodeId
                      << ", completedBlocks=" << *session.completedBlocks << ", totalBlocks=" << session.totalMemids
                      << ", failReason=" << session.failReason
                      << ", reason=" << MigrateFailReasonToMsg(session.failReason);
    }
    const auto [token, restored] = manager.RestoreSession(session);
    if (!restored) {
        UBSE_LOG_WARN << "Skip migration recovery: session registered concurrently, name=" << name;
        return true;
    }
    UBSE_LOG_INFO << "Recovered migrate ledger, name=" << name << ", state=" << session.state
                  << ", completedBlocks=" << *session.completedBlocks << ", totalBlocks=" << session.totalMemids
                  << ", rebuildRequired=" << session.rebuildRequired
                  << ", failReason=" << MigrateFailReasonName(session.failReason);
    if (session.state == UBS_MEM_MIGRATE_STATE_SUCCESS) {
        return true; // 已全部提交且旧导出已清理，只恢复查询会话，不再次编排。
    }
    ctx.sessionToken = token;
    DispatchRecoveredMigration(std::move(ctx), session);
    return true;
}

} // namespace

// 该 name 在已采集账本中是否存在迁移痕迹(迁移标记/切换中或搁浅块/导入switchContext)；
// 主备倒换后初始对账未收齐时，存在痕迹说明该 name 可能有待恢复的迁移，查询/受理按对账采集中处理；
// 若会话表已存在该 name 的会话，说明恢复扫描已处理过该 name(终态留存或活跃会话)，
// 直接放行，活跃会话的互斥/拒绝由受理等后续逻辑判定。
bool NameHasMigrationTrace(const std::string& name)
{
    auto hasTrace = false;
    const auto isSwitchingOrStranded = [](const auto& block) {
        return block.memIdStatus == UB_MEM_SWITCHING || block.memIdStatus == UB_MEM_STRANDED;
    };
    auto& ledger = UbseMemDebtLedger::GetInstance();
    auto& exports = ledger.GetDebtMap<UbseMemShareBorrowExportObj>();
    for (const auto& [nodeId, map] : exports.GetAllNodeMaps()) {
        const auto obj = map ? map->Get(name) : nullptr;
        if (!obj || obj->status.state == UBSE_MEM_EXPORT_DESTROYED) {
            continue;
        }
        if (std::any_of(obj->migrateInfos.begin(), obj->migrateInfos.end(),
                        [](uint32_t marker) { return ShmMigrateOrigin(marker) != 0; }) ||
            std::any_of(obj->status.exportObmmInfo.begin(), obj->status.exportObmmInfo.end(), isSwitchingOrStranded)) {
            hasTrace = true;
            break;
        }
    }
    if (!hasTrace) {
        auto& imports = ledger.GetDebtMap<UbseMemShareBorrowImportObj>();
        for (const auto& [nodeId, map] : imports.GetAllNodeMaps()) {
            const auto obj = map ? map->Get(name) : nullptr;
            if (!obj || obj->status.state == UBSE_MEM_IMPORT_DESTROYED) {
                continue;
            }
            if (obj->switchContext.has_value() ||
                std::any_of(obj->exportObmmInfo.begin(), obj->exportObmmInfo.end(), isSwitchingOrStranded)) {
                hasTrace = true;
                break;
            }
        }
    }
    if (!hasTrace) {
        return false;
    }
    UbseMemShmMigrateSession snapshot{};
    if (UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(name, snapshot)) {
        return false;
    }
    return true;
}

bool UbseMemShmMigrateService::RecoverAfterReconciliation()
{
    auto module = ubse::context::UbseContext::GetInstance().GetModule<ubse::election::UbseElectionModule>();
    if (module == nullptr || !module->IsLeader()) {
        return true;
    }
    std::set<std::string> names;
    const auto collectNames = [&names](const auto& debtMap, auto destroyedState) {
        for (const auto& [nodeId, map] : debtMap.GetAllNodeMaps()) {
            if (!map) {
                continue;
            }
            for (const auto& [name, obj] : map->GetAll()) {
                if (obj && obj->status.state != destroyedState) {
                    names.insert(name);
                }
            }
        }
    };
    const auto& ledger = UbseMemDebtLedger::GetInstance();
    collectNames(ledger.GetDebtMap<UbseMemShareBorrowImportObj>(), UBSE_MEM_IMPORT_DESTROYED);
    collectNames(ledger.GetDebtMap<UbseMemShareBorrowExportObj>(), UBSE_MEM_EXPORT_DESTROYED);
    bool allSettled = true;
    for (const auto& name : names) {
        if (!module->IsLeader()) {
            return allSettled;
        }
        if (!RecoverName(name)) {
            allSettled = false;
        }
    }
    return allSettled;
}

} // namespace ubse::mem::controller
