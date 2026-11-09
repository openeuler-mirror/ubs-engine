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
#include "ubse_mem_controller_migrate_api.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "ubse_conf_module.h"
#include "ubse_context.h"

#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_mem_migrate_helper.h"
#include "ubse_node_controller.h"
#include "ubse_str_util.h"
#include "../logging_lock_guard.h"
#include "../ubse_mem_async_processor.h"
#include "../ubse_mem_controller_ledger.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace migrate_detail;
using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::common::def;
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;

namespace migrate_detail {

static uint32_t CheckAcceptReq(const UbseMemShmMigrateReq& req)
{
    if (req.name.empty() || req.targetNodeId.empty()) {
        UBSE_LOG_ERROR << "shm migrate invalid param, name=" << req.name << ", targetNodeId=" << req.targetNodeId;
        return UBSE_ERR_OUT_OF_RANGE;
    }
    return UBSE_OK;
}

static uint32_t CheckBorrowRelation(const std::string& name, ConstExportObjPtr& oldObj,
                                    std::vector<ConstImportObjPtr>& importObjs)
{
    auto [exportObjPtr, importObjPtrs] = GetMaxRefCountExportObj(name);
    if (exportObjPtr == nullptr) {
        UBSE_LOG_ERROR << "shm migrate borrow relation not exist, name=" << name;
        return UBSE_ERR_NOT_EXIST;
    }
    if (exportObjPtr->status.state != UBSE_MEM_EXPORT_SUCCESS || exportObjPtr->algoResult.exportNumaInfos.empty() ||
        exportObjPtr->status.exportObmmInfo.empty()) {
        UBSE_LOG_ERROR << "shm migrate export obj state not allowed, name=" << name
                       << ", state=" << static_cast<uint32_t>(exportObjPtr->status.state)
                       << ", exportNumaInfos empty=" << exportObjPtr->algoResult.exportNumaInfos.empty()
                       << ", exportObmmInfo empty=" << exportObjPtr->status.exportObmmInfo.empty();
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    // V1恢复账本没有迁移标记；在调度目标资源和创建会话前拒绝，不能等到Agent执行时才失败。
    if (exportObjPtr->migrateInfos.empty()) {
        UBSE_LOG_WARN << "shm migrate legacy export metadata has no migration markers, name=" << name
                      << ", nodeId=" << exportObjPtr->algoResult.exportNumaInfos.front().nodeId;
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    // 与CollectImportNodes使用相同的参与节点范围，覆盖目标及其他实际导入节点。
    for (const auto& importObj : GetAllImportObjsByName(name)) {
        if (importObj->status.state != UBSE_MEM_IMPORT_SUCCESS || importObj->status.importResults.empty() ||
            importObj->importNodeId == exportObjPtr->algoResult.exportNumaInfos.front().nodeId) {
            continue;
        }
        if (importObj->migrateInfos.empty()) {
            UBSE_LOG_WARN << "shm migrate legacy import metadata has no migration markers, name=" << name
                          << ", nodeId=" << importObj->importNodeId;
            return UBSE_ERR_MIGRATE_NOT_ALLOWED;
        }
    }
    oldObj = exportObjPtr;
    importObjs = std::move(importObjPtrs);
    return UBSE_OK;
}

static uint32_t CheckTargetNode(const UbseMemShmMigrateReq& req, const std::string& srcNodeId,
                                const std::vector<ConstImportObjPtr>& importObjs, size_t oldBlockCount,
                                std::string& dstNodeId, ConstImportObjPtr& targetImportObj)
{
    auto dstNode = UbseNodeController::GetInstance().GetNodeById(req.targetNodeId);
    if (dstNode.nodeId.empty()) {
        UBSE_LOG_ERROR << "shm migrate target node not exist, name=" << req.name
                       << ", targetNodeId=" << req.targetNodeId;
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    if (dstNode.clusterState != UbseNodeClusterState::UBSE_NODE_WORKING) {
        UBSE_LOG_ERROR << "shm migrate target node not working, name=" << req.name << ", dstNodeId=" << dstNode.nodeId
                       << ", clusterState=" << static_cast<int>(dstNode.clusterState);
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    dstNodeId = dstNode.nodeId;
    if (dstNodeId == srcNodeId) {
        UBSE_LOG_ERROR << "shm migrate target equals source, name=" << req.name << ", nodeId=" << dstNodeId;
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    const auto mappedTarget = std::find_if(importObjs.begin(), importObjs.end(), [&dstNodeId](const auto& obj) {
        return obj != nullptr && obj->importNodeId == dstNodeId;
    });
    if (mappedTarget == importObjs.end()) {
        return UBSE_OK;
    }
    const auto& targetImport = **mappedTarget;
    if (targetImport.status.state != UBSE_MEM_IMPORT_SUCCESS ||
        targetImport.status.importResults.size() != oldBlockCount ||
        targetImport.status.decoderResult.size() != oldBlockCount ||
        targetImport.exportObmmInfo.size() != oldBlockCount ||
        std::any_of(targetImport.status.importResults.begin(), targetImport.status.importResults.end(),
                    [](const auto& result) { return result.memId == 0; })) {
        UBSE_LOG_ERROR << "shm migrate mapped target import layout invalid, name=" << req.name
                       << ", dstNodeId=" << dstNodeId << ", oldBlockCount=" << oldBlockCount;
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    targetImportObj = *mappedTarget;
    return UBSE_OK;
}

static void FillMigrateProgress(const UbseMemShmMigrateSession& session, UbseMemShmMigrateGetResp& resp)
{
    resp.name = session.name;
    resp.srcNodeId = session.srcNodeId;
    resp.dstNodeId = session.dstNodeId;
    resp.state = static_cast<uint32_t>(session.state);
    resp.totalMemids = session.totalMemids;
    resp.migratedMemids = session.completedBlocks.value_or(0);
    resp.totalSize = session.totalSize;
    resp.failReason = session.failReason;
    resp.errMsg = (session.state == UBS_MEM_MIGRATE_STATE_FAILED) ? MigrateFailReasonToMsg(session.failReason) : "";
    if (session.rebuildRequired) {
        resp.errMsg += "; detach/delete and rebuild (1044)";
    }
}

} // namespace migrate_detail

UbseMemShmMigrateService& UbseMemShmMigrateService::GetInstance()
{
    static UbseMemShmMigrateService instance;
    return instance;
}

uint32_t UbseMemShmMigrateService::CheckUserPermission(const std::string& name, const UbseUdsInfo& caller,
                                                       bool sessionOnly)
{
    if (caller.username.empty()) {
        UBSE_LOG_WARN << "Migration caller identity missing, name=" << name << ", uid=" << caller.uid;
        return UBSE_ERR_AUTH_FAILED;
    }
    auto obj =
        sessionOnly ?
            nullptr :
            UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetExportResourceByResId(name);
    UbseUdsInfo owner{};
    if (obj != nullptr) {
        owner = obj->req.udsInfo;
    } else {
        UbseMemShmMigrateSession session{};
        if (!UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(name, session)) {
            UBSE_LOG_WARN << "Migration permission resource missing, name=" << name;
            return UBSE_ERR_NOT_EXIST;
        }
        owner = session.owner;
    }
    // 内置管理用户默认具有迁移、查询及取消权限，无需配置额外用户名单。
    if (caller.username == "root" || caller.username == "ubse") {
        return UBSE_OK;
    }
    if (owner.username.empty() ? owner.uid == caller.uid : owner.username == caller.username) {
        return UBSE_OK;
    }
    auto conf = ubse::context::UbseContext::GetInstance().GetModule<ubse::config::UbseConfModule>();
    std::string users;
    if (conf == nullptr || conf->GetConf<std::string>("auth.shm.migrate", "allowedUsers", users) != UBSE_OK) {
        UBSE_LOG_WARN << "Migration user configuration unavailable, name=" << name
                      << ", section=auth.shm.migrate, key=allowedUsers, username=" << caller.username;
        return UBSE_ERR_AUTH_FAILED;
    }
    std::istringstream stream(users);
    std::string user;
    while (std::getline(stream, user, ',')) {
        user = ubse::utils::Trim(user);
        if (user.empty()) {
            continue;
        }
        if (user == caller.username) {
            return UBSE_OK;
        }
    }
    UBSE_LOG_WARN << "Migration user permission denied, name=" << name << ", username=" << caller.username
                  << ", owner username=" << owner.username;
    return UBSE_ERR_AUTH_FAILED;
}

bool UbseMemShmMigrateService::HasActiveMigration(const std::string& name) const
{
    return UbseMemShmMigrateSessionManager::GetInstance().HasActiveSession(name);
}

uint32_t UbseMemShmMigrateService::CheckOperationAllowed(const std::string& name, bool releasing) const
{
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    UbseMemShmMigrateSession session{};
    if (!mgr.ReadSessionSnapshot(name, session)) {
        return UBSE_OK;
    }
    if (!IsMigrateTerminalState(session.state)) {
        return UBSE_ERR_MIGRATE_IN_PROGRESS;
    }
    // 归还仅与非终态迁移互斥；终态的重建标记只限制新增使用/重新迁移。
    if (releasing || !session.rebuildRequired) {
        return UBSE_OK;
    }
    const auto hasResources = [&name](const auto& nodeMaps, UbseMemState destroyed) {
        return std::any_of(nodeMaps.begin(), nodeMaps.end(), [&](const auto& item) {
            const auto obj = item.second->Get(name);
            return obj != nullptr && obj->status.state != destroyed;
        });
    };
    auto& ledger = UbseMemDebtLedger::GetInstance();
    if (!hasResources(ledger.GetDebtMap<UbseMemShareBorrowExportObj>().GetAllNodeMaps(), UBSE_MEM_EXPORT_DESTROYED) &&
        !hasResources(ledger.GetDebtMap<UbseMemShareBorrowImportObj>().GetAllNodeMaps(), UBSE_MEM_IMPORT_DESTROYED)) {
        mgr.SetRebuildRequired(name, false);
        return UBSE_OK;
    }
    UBSE_LOG_WARN << "Migrate resource requires detach/delete and rebuild, name=" << name
                  << ", failReason=" << session.failReason
                  << ", ret=" << FormatRetCode(UBSE_ERR_MIGRATE_REBUILD_REQUIRED);
    return UBSE_ERR_MIGRATE_REBUILD_REQUIRED;
}

uint32_t UbseMemShmMigrateService::Accept(const UbseMemShmMigrateReq& req)
{
    UBSE_LOG_INFO << "shm migrate request, name=" << req.name << ", targetNodeId=" << req.targetNodeId;
    const auto authRet = CheckUserPermission(req.name, req.caller);
    if (authRet != UBSE_OK) {
        return authRet;
    }
    // 参数校验（name 字符集/长度已由 dispatcher UnpackMemName 校验）
    auto ret = CheckAcceptReq(req);
    if (ret != UBSE_OK) {
        return ret;
    }
    auto nameLock = LoggingLockGuard(req.name);
    // 同名活跃任务统一在新建会话前拦截；GetOrCreateSession保留原子检查。
    if (const auto access = CheckOperationAllowed(req.name); access != UBSE_OK) {
        return access;
    }
    ConstExportObjPtr oldObjPtr;
    std::vector<ConstImportObjPtr> importObjPtrs;
    ret = CheckBorrowRelation(req.name, oldObjPtr, importObjPtrs);
    if (ret != UBSE_OK) {
        return ret;
    }
    const auto& srcNodeId = oldObjPtr->algoResult.exportNumaInfos.front().nodeId;
    // 根据目标节点是否已导入该共享内存，确定新建导出或将已有导入设备切为导出设备。
    std::string dstNodeId;
    ConstImportObjPtr targetImportObj;
    ret = CheckTargetNode(req, srcNodeId, importObjPtrs, oldObjPtr->status.exportObmmInfo.size(), dstNodeId,
                          targetImportObj);
    if (ret != UBSE_OK) {
        return ret;
    }
    const auto targetKind = targetImportObj ? UbseMemMigrateTargetKind::IMPORT_NODE :
                                              UbseMemMigrateTargetKind::NOT_IMPORT_NODE;
    const uint32_t totalMemids = static_cast<uint32_t>(oldObjPtr->status.exportObmmInfo.size());
    uint64_t totalSize = 0;
    for (const auto& info : oldObjPtr->algoResult.exportNumaInfos) {
        if (info.size > std::numeric_limits<uint64_t>::max() - totalSize) {
            UBSE_LOG_ERROR << "Migrate total size overflow, name=" << req.name << ", targetNodeId=" << req.targetNodeId;
            return UBSE_ERR_OUT_OF_RANGE;
        }
        totalSize += info.size;
    }
    UbseMemShareBorrowExportObj newExportObj;
    ret = PrepareMigrateExport(oldObjPtr, dstNodeId, targetImportObj, newExportObj);
    if (ret != UBSE_OK) {
        return ret;
    }
    // 会话创建保留原子幂等检查；执行体直接使用下方登记的准备结果。
    auto [session, isNewSession] = UbseMemShmMigrateSessionManager::GetInstance().GetOrCreateSession(
        req.name, srcNodeId, req.targetNodeId, totalMemids, targetKind, totalSize, oldObjPtr->req.udsInfo);
    (void)session;
    if (!isNewSession) {
        DiscardPreparedExport(newExportObj);
        return UBSE_ERR_MIGRATE_IN_PROGRESS;
    }

    UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().PutResource(dstNodeId, req.name,
                                                                                           newExportObj);
    ret = AsyncMemShmMigrateProcessor(req.name);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Dispatch migrate task failed, name=" << req.name << ", targetNodeId=" << dstNodeId
                       << ", failReason=" << MIGRATE_FAIL_DISPATCH << ", ret=" << FormatRetCode(ret);
        DiscardPreparedExport(newExportObj);
        UbseMemShmMigrateSessionManager::GetInstance().FinishSession(req.name, UBS_MEM_MIGRATE_STATE_FAILED,
                                                                     MIGRATE_FAIL_DISPATCH);
        return ret;
    }
    UBSE_LOG_INFO << "shm migrate accepted, name=" << req.name << ", srcNodeId=" << srcNodeId
                  << ", dstNodeId=" << req.targetNodeId << ", totalMemids=" << totalMemids;
    return UBSE_OK;
}

void UbseMemShmMigrateService::Get(const std::string& name, UbseMemShmMigrateGetResp& resp)
{
    UbseMemShmMigrateSession session{};
    if (!UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(name, session)) {
        UBSE_LOG_INFO << "shm migrate get not exist, name=" << name;
        resp.errCode = UBSE_ERR_NOT_EXIST;
        return;
    }
    resp.errCode = 0;
    FillMigrateProgress(session, resp);
}

void UbseMemShmMigrateService::Cancel(const std::string& name, UbseMemMigrateCancelResp& resp)
{
    // 竞态控制在会话表锁内完成: 判定状态 + 置 cancelRequested;
    // 清理由Execute启动检查点或导出完成回调消费取消标志后异步执行(异步, 回包≠清理完成, CMA 用 migrate_get 轮询 CANCELED)
    resp.errCode = UbseMemShmMigrateSessionManager::GetInstance().RequestMigrateCancel(name);
}

uint32_t UbseMemShmMigrateService::CheckSessionExecutable(const std::string& name, UbseMemShmMigrateSession& session,
                                                          std::shared_ptr<UbseMemShmMigrateSession>& token)
{
    if (!UbseMemShmMigrateSessionManager::GetInstance().ReadSessionSnapshot(name, session, &token)) {
        UBSE_LOG_ERROR << "migrate session not found, name=" << name;
        return UBSE_ERR_NOT_EXIST;
    }
    if (session.state != UBS_MEM_MIGRATE_STATE_INIT) {
        UBSE_LOG_ERROR << "migrate session state not INIT, name=" << name
                       << ", state=" << static_cast<uint32_t>(session.state);
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    return UBSE_OK;
}

uint32_t UbseMemShmMigrateService::Execute(const std::string& name)
{
    auto lock = LoggingLockGuard(name);
    UbseMemShmMigrateSession session{};
    MigrationRunContext ctx;
    auto ret = CheckSessionExecutable(name, session, ctx.sessionToken);
    if (ret != UBSE_OK) {
        return ret;
    }
    ctx.name = session.name;
    ctx.srcNodeId = session.srcNodeId;
    ctx.dstNodeId = session.dstNodeId;
    ctx.targetKind = session.targetKind;
    UBSE_LOG_INFO << "shm migrate new borrow begins, name=" << ctx.name << ", srcNodeId=" << ctx.srcNodeId
                  << ", dstNodeId=" << ctx.dstNodeId;
    const auto prepared =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(ctx.dstNodeId, ctx.name);
    if (prepared == nullptr || prepared->status.state != UBSE_MEM_EXPORT_RUNNING) {
        UbseMemShmMigrateSessionManager::GetInstance().FinishSession(name, UBS_MEM_MIGRATE_STATE_FAILED,
                                                                     MIGRATE_FAIL_LAYOUT);
        UBSE_LOG_ERROR << "Migrate prepared export ledger missing or not running, name=" << name
                       << ", dstNodeId=" << ctx.dstNodeId;
        return UBSE_ERR_NOT_EXIST;
    }
    ctx.newExportObj = *prepared;
    // 此时尚未下发 obmm，取消只释放准备结果，不发送删除导出请求。
    if (session.cancelRequested) {
        DiscardPreparedExport(ctx.newExportObj);
        UbseMemShmMigrateSessionManager::GetInstance().FinishSession(name, UBS_MEM_MIGRATE_STATE_CANCELED);
        return UBSE_OK;
    }

    ctx.oldObj =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(ctx.srcNodeId, ctx.name);
    if (ctx.oldObj == nullptr) {
        DiscardPreparedExport(ctx.newExportObj);
        UbseMemShmMigrateSessionManager::GetInstance().FinishSession(name, UBS_MEM_MIGRATE_STATE_FAILED,
                                                                     MIGRATE_FAIL_OLD_OBJ_LOST);
        UBSE_LOG_ERROR << "Migrate source export ledger disappeared before execution, name=" << name
                       << ", dstNodeId=" << ctx.dstNodeId;
        return UBSE_ERR_NOT_EXIST;
    }

    if (ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE) {
        return ExecuteMappedTargetMigration(ctx);
    }
    UbseMemShmMigrateSessionManager::GetInstance().UpdateSessionState(ctx.name, UBS_MEM_MIGRATE_STATE_EXPORTING);

    return StartNewExport(std::move(ctx));
}
} // namespace ubse::mem::controller
