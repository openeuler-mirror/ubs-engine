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

#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <mutex>
#include <new>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_mem_migrate_helper.h"
#include "ubse_mem_scheduler_impl.h"
#include "ubse_mem_util.h"
#include "ubse_str_util.h"
#include "ubse_timer.h"
#include "../logging_lock_guard.h"
#include "api/ubse_mem_controller_share_api.h"
#include "message/ubse_mem_auto_serial.h"
#include "message/ubse_mem_callback_message.h"
#include "trace_context.h"
#include "ubse_auto_simpo.hpp"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::mem::scheduler;
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::com;
using namespace ubse::mem::util;
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::utils;
using namespace migrate_detail;

namespace migrate_detail {
static bool IsCurrentMigration(const MigrationRunContext& ctx);
static void StartExportCleanup(const MigrationRunContext& ctx, uint32_t reason);
static void StartOldChainCleanup(const MigrationRunContext& ctx);
static uint32_t StartMappedMigration(MigrationRunContext&& ctx);

// 与普通借用一致：RpcSend只等待对端handler入队确认，业务结果另行RPC上报。
template <typename Message>
uint32_t SendMigrateMessage(const std::string& nodeId, UbseMemBorrowCallbackOpCode opCode, const Message& data)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Migrate request communication module unavailable, targetNodeId=" << nodeId
                       << ", opCode=" << static_cast<uint16_t>(opCode);
        return UBSE_ERROR_NULLPTR;
    }
    ubse::simpo::util::UbseAutoSimpoPtr<Message> request = new (std::nothrow)
        ubse::simpo::util::UbseAutoSimpo<Message>();
    UbseBaseMessagePtr response = new (std::nothrow) UbseMemCallbackMessage();
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Migrate request allocation failed, targetNodeId=" << nodeId
                       << ", opCode=" << static_cast<uint16_t>(opCode);
        return UBSE_ERROR_NULLPTR;
    }
    request->SetUbseMesgInfo(data);
    SendParam param(nodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW), static_cast<uint16_t>(opCode));
    auto ret = UBSE_ERROR;
    for (uint32_t attempt = 0; attempt < SEND_RETRY_TIMES; ++attempt) {
        ret = comModule->RpcSend(param, request, response);
        if (ret == UBSE_OK) {
            return UBSE_OK;
        }
        UBSE_LOG_WARN << "Send migrate request failed, targetNodeId=" << nodeId
                      << ", opCode=" << static_cast<uint16_t>(opCode) << ", attempt=" << attempt + 1
                      << ", ret=" << FormatRetCode(ret);
        if (attempt + 1 < SEND_RETRY_TIMES) {
            sleep(SEND_RETRY_DURATION); // 最后一次发送失败后不再休眠
        }
    }
    return ret;
}

enum class ExportCleanup
{
    PRESERVE,
    ALLOW_DELETE
};

static void FailMigration(const MigrationRunContext& ctx, uint32_t failReason, ExportCleanup cleanup,
                          const std::string& failureDetail = {})
{
    if (ctx.sessionToken != nullptr && !IsCurrentMigration(ctx)) {
        UBSE_LOG_WARN << "Ignore failure of replaced migration, name=" << ctx.name << ", reason=" << failReason;
        return;
    }
    UbseMemShmMigrateSession session{};
    auto& manager = UbseMemShmMigrateSessionManager::GetInstance();
    if (manager.ReadSessionSnapshot(ctx.name, session) && (session.completedBlocks || ctx.targetBeginStarted)) {
        manager.SetRebuildRequired(ctx.name, true);
    }
    UBSE_LOG_ERROR << "Migrate failed, name=" << ctx.name << ", srcNodeId=" << ctx.srcNodeId
                   << ", dstNodeId=" << ctx.dstNodeId << ", failReason=" << failReason
                   << ", failReasonName=" << MigrateFailReasonName(failReason)
                   << ", allowExportDelete=" << (cleanup == ExportCleanup::ALLOW_DELETE)
                   << ", targetBeginStarted=" << ctx.targetBeginStarted << ", endCommitStarted=" << ctx.endCommitStarted
                   << ", state=" << session.state << ", completedBlocks=" << session.completedBlocks.value_or(0)
                   << ", failureDetail={" << failureDetail << "}";
    if (ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE) {
        // 未提交目标可通过end(0)清理；已提交或结果未知时保留设备及账本供核对。
        if (!ctx.targetBeginStarted) {
            auto& exportMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>();
            exportMap.RemoveResource(ctx.dstNodeId, ctx.name);
        }
        manager.FinishSession(ctx.name, UBS_MEM_MIGRATE_STATE_FAILED, failReason);
        return;
    }
    // 回滚不能确认或 逐块迁移与账本收敛 已经开始时，保留会话及新导出供核对，禁止覆盖未确认资源重新受理。
    if (cleanup == ExportCleanup::ALLOW_DELETE && ctx.exportConfirmed && !ctx.endCommitStarted) {
        StartExportCleanup(ctx, failReason);
        return;
    }
    if (!ctx.newExportObj.algoResult.exportNumaInfos.empty()) {
        manager.SetRebuildRequired(ctx.name, true);
    }
    manager.FinishSession(ctx.name, UBS_MEM_MIGRATE_STATE_FAILED, failReason);
}

uint32_t ExecuteMappedTargetMigration(MigrationRunContext& ctx)
{
    ctx.targetImportObj =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(ctx.dstNodeId, ctx.name);
    if (ctx.targetImportObj == nullptr ||
        ctx.targetImportObj->status.importResults.size() != ctx.newExportObj.migrateInfos.size()) {
        DiscardPreparedExport(ctx.newExportObj);
        UbseMemShmMigrateSessionManager::GetInstance().FinishSession(ctx.name, UBS_MEM_MIGRATE_STATE_FAILED,
                                                                     MIGRATE_FAIL_LAYOUT);
        UBSE_LOG_WARN << "Migrate target import ledger changed before execution, name=" << ctx.name
                      << ", dstNodeId=" << ctx.dstNodeId;
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    // 首块begin_export完成后才关闭取消窗口；等待回调期间保持EXPORTING。
    UbseMemShmMigrateSessionManager::GetInstance().UpdateSessionState(ctx.name, UBS_MEM_MIGRATE_STATE_EXPORTING);
    ctx.importNodes.clear();
    for (const auto& nodeId : CollectImportNodes(ctx.name, ctx.srcNodeId)) {
        if (nodeId == ctx.dstNodeId) {
            continue;
        }
        ctx.importNodes.push_back(nodeId);
    }
    return StartMappedMigration(std::move(ctx));
}

// 各节点结果已逐块合入；整轮完成后核对并收敛 name 级关系。
static void FinishMigration(MigrationRunContext& ctx)
{
    UbseMemShmMigrateSessionManager::GetInstance().UpdateSessionState(ctx.name, UBS_MEM_MIGRATE_STATE_FINISHING);
    // 核对所有已提交块，再清理旧导出关系。
    if (ConvergeImportObjsToMaster(ctx) != UBSE_OK) {
        UBSE_LOG_ERROR << "Final import ledger convergence failed, name=" << ctx.name
                       << ", dstNodeId=" << ctx.dstNodeId;
        FailMigration(ctx, MIGRATE_FAIL_CONVERGE, ExportCleanup::PRESERVE);
        return;
    }
    StartOldChainCleanup(ctx);
}

/* ==================== 逐块迁移编排 ==================== */

// 当前等待的实际操作；A/B差异由启动点绑定的完成函数处理。
enum class MigrationOperation : uint8_t
{
    BEGIN_IMPORT,
    BEGIN_EXPORT,
    COPY,
    END_IMPORT,
    END_EXPORT,
    ROLLBACK_IMPORT,
    ROLLBACK_EXPORT
};

const char* MigrationOperationName(MigrationOperation operation)
{
    switch (operation) {
        case MigrationOperation::BEGIN_IMPORT:
            return "begin_import";
        case MigrationOperation::BEGIN_EXPORT:
            return "begin_export";
        case MigrationOperation::COPY:
            return "copy";
        case MigrationOperation::END_IMPORT:
            return "end_import(1)";
        case MigrationOperation::END_EXPORT:
            return "end_export(1)";
        case MigrationOperation::ROLLBACK_IMPORT:
            return "end_import(0)";
        case MigrationOperation::ROLLBACK_EXPORT:
            return "end_export(0)";
    }
    return "unknown";
}

uint32_t MigrationBusinessFailureReason(MigrationOperation operation)
{
    switch (operation) {
        case MigrationOperation::BEGIN_IMPORT:
            return MIGRATE_FAIL_BEGIN_IMPORT;
        case MigrationOperation::BEGIN_EXPORT:
            return MIGRATE_FAIL_BEGIN_EXPORT;
        case MigrationOperation::COPY:
            return MIGRATE_FAIL_COPY;
        case MigrationOperation::END_IMPORT:
            return MIGRATE_FAIL_END_IMPORT;
        case MigrationOperation::END_EXPORT:
            return MIGRATE_FAIL_END_EXPORT;
        case MigrationOperation::ROLLBACK_IMPORT:
            return MIGRATE_FAIL_ROLLBACK_IMPORT;
        case MigrationOperation::ROLLBACK_EXPORT:
            return MIGRATE_FAIL_ROLLBACK_EXPORT;
    }
    return MIGRATE_FAIL_CONVERGE;
}

struct MigrationTask;

enum class MigrationStepState : uint8_t
{
    WAITING_RESULT,  // 正在收集节点回调结果
    RESULT_READY,    // 已收齐结果、等待线程池调用onComplete处理成功推进或失败清理
    CALLBACK_STARTED // 已经触发过onComplete,重复完成事件直接忽略
};

enum class NodeResultState : uint8_t
{
    WAITING,
    SUCCEEDED,
    FAILED
};

struct MigrationNodeResult {
    NodeResultState state{NodeResultState::WAITING};
    std::chrono::steady_clock::time_point deadline{std::chrono::steady_clock::time_point::max()};
    uint32_t transportRet{UBSE_OK};
    uint32_t resultRet{UBSE_OK};
    UbseMemMigrateRpcInfo request{};
    bool committed{false};
};

struct MigrationStep {
    MigrationOperation operation;
    uint32_t blockIndex;
    MigrationStepState state{MigrationStepState::WAITING_RESULT};
    uint64_t timeoutSeconds{MIGRATE_PHASE_TIMEOUT_S};
    void (*onComplete)(const std::shared_ptr<MigrationTask>&, const std::shared_ptr<MigrationStep>&);
    std::vector<MigrationNodeResult> nodeResults;
    std::string timerName{};
    uint32_t failReason{UBSE_OK}; // 所有失败路径均记录首错；完成回调中为OK即表示本阶段全部成功。
};

struct MigrationTask {
    MigrationRunContext ctx{};
    std::mutex mutex{};
    uint64_t stepSequence{0};
    bool finished{false};
    std::optional<uint32_t> cleanupReason; // 无值=未清理，0=取消，非0=失败原因
    std::string failureDetail; // 仅诊断：保存清理前的失败阶段，避免最终日志只剩回退阶段。
    bool targetBlockCommitted{false}; // 当前块目标end(1)已确认，不再向目标发送end(0)
    uint32_t blockIndex{0};

    std::shared_ptr<MigrationStep> step;
};

std::mutex g_migrateTasksMutex;
std::unordered_map<std::string, std::shared_ptr<MigrationTask>> g_migrateTasks;

std::string MigrationStepLog(const MigrationStep& step)
{
    std::ostringstream out;
    out << "operation=" << MigrationOperationName(step.operation) << ", indexId=" << step.blockIndex << ", nodes=[";
    for (size_t n = 0; n < step.nodeResults.size(); ++n) {
        const auto& node = step.nodeResults[n];
        out << (n == 0 ? "" : "; ") << node.request.srcNodeId << ":"
            << (node.state == NodeResultState::WAITING ?
                    "waiting" :
                    (node.state == NodeResultState::SUCCEEDED ? "succeeded" : "failed"))
            << "/transportRet=" << FormatRetCode(node.transportRet) << "/resultRet=" << FormatRetCode(node.resultRet)
            << "/committed=" << node.committed
            << "/resultReceived=" << (node.transportRet == UBSE_OK && node.state != NodeResultState::WAITING);
    }
    out << "], stepFailReason=" << step.failReason << ", stepFailReasonName=" << MigrateFailReasonName(step.failReason);
    return out.str();
}

std::shared_ptr<MigrationTask> CreateMigrationTask(MigrationRunContext&& ctx)
{
    auto task = std::make_shared<MigrationTask>();
    task->ctx = std::move(ctx);
    std::lock_guard<std::mutex> lock(g_migrateTasksMutex);
    if (!g_migrateTasks.emplace(task->ctx.name, task).second) {
        UBSE_LOG_ERROR << "Reject duplicate migrate task, name=" << task->ctx.name
                       << ", dstNodeId=" << task->ctx.dstNodeId;
        return nullptr;
    }
    return task;
}

// 仅在完成回调中使用：结果已冻结；失败回包仍可能确认设备提交，不能仅凭failReason判断是否结束。
bool AllImportEndsConfirmed(const MigrationStep& step)
{
    return std::all_of(step.nodeResults.begin(), step.nodeResults.end(), [](const auto& node) {
        return node.state == NodeResultState::SUCCEEDED || (node.transportRet == UBSE_OK && node.committed);
    });
}

void OnBeginImportCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step);
void OnCopyCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step);
void OnEndCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step);
void OnRollbackCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step);
void OnBeginExportCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step);
void OnEndExportCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step);
void OnImportRollbackCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step);
void OnExportRollbackCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step);

void FinishMigrationTask(const std::shared_ptr<MigrationTask>& task)
{
    std::string timerName;
    {
        std::lock_guard<std::mutex> lock(task->mutex);
        task->finished = true;
        if (task->step) {
            timerName = task->step->timerName;
        }
    }
    {
        std::lock_guard<std::mutex> lock(g_migrateTasksMutex);
        const auto it = g_migrateTasks.find(task->ctx.name);
        if (it != g_migrateTasks.end() && it->second == task) {
            g_migrateTasks.erase(it);
        }
    }
    if (!timerName.empty()) {
        timer::UbseTimerHandlerUnregister(timerName);
    }
}

// Accept阶段已通过会话占用资源；完成处理仅用任务锁领取阶段结果，不再竞争同名资源锁。
void ProcessMigrationCompletion(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step)
{
    if (!IsCurrentMigration(task->ctx)) {
        UBSE_LOG_WARN << "Ignore completion of replaced migration, name=" << task->ctx.name
                      << ", indexId=" << task->blockIndex;
        FinishMigrationTask(task);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(task->mutex);
        if (task->finished || !step || task->step != step) {
            UBSE_LOG_WARN << "Ignore completion of inactive step, name=" << task->ctx.name
                          << ", indexId=" << task->blockIndex << ", finished=" << task->finished;
            return;
        }
        if (step->state != MigrationStepState::RESULT_READY) {
            UBSE_LOG_WARN << "Ignore unready or duplicate completion, name=" << task->ctx.name
                          << ", indexId=" << step->blockIndex << ", state=" << static_cast<uint32_t>(step->state);
            return;
        }
        // 锁内领取后，结果/超时/发送失败入口均禁止再修改此step，回调直接读取冻结结果。
        step->state = MigrationStepState::CALLBACK_STARTED;
    }
    timer::UbseTimerHandlerUnregister(step->timerName);
    step->onComplete(task, step);
}

void DispatchMigrationCompletion(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step)
{
    const auto traceId = TraceContext::GetTraceId();
    const auto work = [task, step, traceId]() {
        TraceContext::SetTraceId(traceId);
        ProcessMigrationCompletion(task, step);
        TraceContext::Clear();
    };
    const auto executor = GetExecutor("ubseMemController");
    const bool submitted = executor != nullptr && executor->Execute(work);
    if (submitted) {
        return;
    }

    // 保留结果及阶段定时器，只重投完成事件，不重发begin/copy，避免丢失必要end。
    UBSE_LOG_ERROR << "name=" << task->ctx.name << ", indexId=" << step->blockIndex
                   << ", operation=" << MigrationOperationName(step->operation)
                   << ", Submit migrate completion failed; retain step for timer dispatch";
}

bool ApplyBeginExportResult(const std::shared_ptr<MigrationTask>& task, const UbseMemMigrateBeginExportResp& response)
{
    const auto b = response.rpcInfo.blockIndex;
    const auto& block = response.block;
    auto& ctx = task->ctx;
    if (block.memId != response.rpcInfo.exportMemId || block.desc.addr == 0 ||
        block.desc.length != ctx.targetImportObj->exportObmmInfo[b].desc.length ||
        block.memIdStatus != UB_MEM_SWITCHING) {
        UBSE_LOG_ERROR << "name=" << ctx.name << ", indexId=" << b
                       << ", operation=begin_export, Invalid export begin block result";
        return false;
    }
    ctx.newExportObj.status.exportObmmInfo.resize(b + 1);
    ctx.newExportObj.status.exportObmmInfo[b] = block;
    if (!UpdateMigrateExport(ctx.dstNodeId, ctx.name, [&](auto& obj) {
            obj.status.exportObmmInfo.resize(b + 1);
            obj.status.exportObmmInfo[b] = block;
            return true;
        })) {
        return false;
    }
    return UpdateMigrateImport(ctx.dstNodeId, ctx.name, [&](auto& obj) {
        if (!obj.switchContext || !MatchesSwitchContext(*obj.switchContext, response.rpcInfo)) {
            obj.switchContext = MakeSwitchContext(response.rpcInfo);
        }
        obj.exportObmmInfo[b].memIdStatus = UB_MEM_SWITCHING;
        return true;
    });
}

// 调用方已核验步骤和节点身份；先按操作合账，再统一登记业务结果和阶段完成情况。
bool ApplyMigrationNodeResult(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step,
                              size_t nodeIndex, uint32_t transportRet, const UbseMemMigrateOpResp& response,
                              const UbseMemMigrateBeginExportResp* exportResponse = nullptr)
{
    const bool transportOk = transportRet == UBSE_OK;
    const bool businessOk = response.errCode == UBSE_OK;
    const auto operation = step->operation;
    bool applied = true;
    const bool exportEnd = operation == MigrationOperation::END_EXPORT ||
                           operation == MigrationOperation::ROLLBACK_EXPORT;
    if (transportOk && exportEnd &&
        (response.committed || (businessOk && operation == MigrationOperation::END_EXPORT))) {
        const auto b = response.rpcInfo.blockIndex;
        const auto& blocks = task->ctx.newExportObj.status.exportObmmInfo;
        applied = response.block && b < blocks.size() &&
                  response.block->exportInfo.memId == response.rpcInfo.exportMemId &&
                  response.block->exportInfo.desc.addr != 0 &&
                  response.block->exportInfo.desc.length == blocks[b].desc.length &&
                  response.block->exportInfo.memIdStatus == UB_MEM_HEALTHY;
        if (!applied) {
            UBSE_LOG_ERROR << "name=" << response.rpcInfo.name << ", indexId=" << b
                           << ", operation=" << MigrationOperationName(operation)
                           << ", Reject incomplete or invalid export end result, nodeId=" << response.rpcInfo.srcNodeId;
        }
    }
    if (transportOk && applied) {
        switch (operation) {
            case MigrationOperation::BEGIN_EXPORT:
                if (businessOk && exportResponse) {
                    applied = ApplyBeginExportResult(task, *exportResponse);
                }
                break;
            case MigrationOperation::BEGIN_IMPORT:
                if (businessOk) {
                    applied = MergeMigrateImportResult(response, true);
                }
                break;
            case MigrationOperation::END_IMPORT:
            case MigrationOperation::ROLLBACK_IMPORT:
                if (businessOk || (operation == MigrationOperation::ROLLBACK_IMPORT && response.committed)) {
                    applied = MergeMigrateImportResult(response, false);
                }
                break;
            case MigrationOperation::ROLLBACK_EXPORT:
                if (businessOk && !response.committed) {
                    applied = UpdateMigrateImport(task->ctx.dstNodeId, task->ctx.name, [&](auto& obj) {
                        ApplyEndRollback(obj, step->blockIndex);
                        if (!obj.switchContext || !MatchesSwitchContext(*obj.switchContext, response.rpcInfo)) {
                            obj.switchContext = MakeSwitchContext(response.rpcInfo);
                        }
                        obj.switchContext->endDecision = false;
                        return true;
                    });
                }
                break;
            case MigrationOperation::END_EXPORT:
            case MigrationOperation::COPY:
                break;
        }
    }
    const bool ok = transportOk && businessOk && applied;
    auto& node = step->nodeResults[nodeIndex];
    node.state = ok ? NodeResultState::SUCCEEDED : NodeResultState::FAILED;
    node.transportRet = transportRet;
    const auto errCode = response.errCode == UBSE_OK && !applied ? UBSE_ERR_INTERNAL : response.errCode;
    node.resultRet = errCode;
    node.committed = transportOk && applied && response.committed &&
                     (businessOk || operation == MigrationOperation::ROLLBACK_IMPORT ||
                      operation == MigrationOperation::ROLLBACK_EXPORT);
    if (!ok) {
        if (step->failReason == UBSE_OK) {
            if (!transportOk) {
                step->failReason = MIGRATE_FAIL_RPC;
            } else if (!businessOk) {
                step->failReason = MigrationBusinessFailureReason(operation);
            } else {
                step->failReason = MIGRATE_FAIL_CONVERGE;
            }
        }
        UBSE_LOG_ERROR << "name=" << task->ctx.name << ", indexId=" << step->blockIndex
                       << ", operation=" << MigrationOperationName(operation)
                       << ", Migrate stage result failed, nodeId=" << node.request.srcNodeId
                       << ", transportRet=" << FormatRetCode(transportRet)
                       << ", agentRet=" << FormatRetCode(response.errCode) << ", resultRet=" << FormatRetCode(errCode)
                       << ", ledgerApplied=" << applied << ", committed=" << node.committed
                       << ", stepFailReason=" << step->failReason
                       << ", stepFailReasonName=" << MigrateFailReasonName(step->failReason);
    }
    const bool cleaning = operation == MigrationOperation::ROLLBACK_IMPORT ||
                          operation == MigrationOperation::ROLLBACK_EXPORT;
    const bool allResultsReceived =
        std::none_of(step->nodeResults.begin(), step->nodeResults.end(),
                     [](const auto& item) { return item.state == NodeResultState::WAITING; });
    const bool done = (!ok && !cleaning) || allResultsReceived;
    if (done) {
        step->state = MigrationStepState::RESULT_READY;
    }
    return done;
}

// 本地准备/发送失败没有业务回包，按实际入口记录原因，不混同Agent业务失败。
void RecordMigrationLocalFailure(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step,
                                 size_t nodeIndex, uint32_t ret, uint32_t failReason)
{
    bool done = false;
    {
        std::lock_guard<std::mutex> lock(task->mutex);
        if (task->finished || task->step != step || step->state != MigrationStepState::WAITING_RESULT ||
            step->nodeResults[nodeIndex].state != NodeResultState::WAITING) {
            UBSE_LOG_WARN << "name=" << task->ctx.name << ", indexId=" << step->blockIndex
                          << ", operation=" << MigrationOperationName(step->operation)
                          << ", Ignore late migrate local failure, nodeIndex=" << nodeIndex;
            return;
        }
        if (step->failReason == UBSE_OK) {
            step->failReason = failReason;
        }
        done = ApplyMigrationNodeResult(task, step, nodeIndex, ret, {});
    }
    if (done) {
        DispatchMigrationCompletion(task, step);
    }
}

uint32_t MigratePhaseTimeout(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step)
{
    {
        std::lock_guard<std::mutex> lock(task->mutex);
        if (task->finished || task->step != step || step->state == MigrationStepState::CALLBACK_STARTED) {
            return UBSE_OK;
        }
        const auto now = std::chrono::steady_clock::now();
        if (step->state == MigrationStepState::WAITING_RESULT &&
            std::none_of(step->nodeResults.begin(), step->nodeResults.end(), [now](const auto& node) {
                return node.state == NodeResultState::WAITING && now >= node.deadline;
            })) {
            return UBSE_OK;
        }
        for (size_t n = 0; n < step->nodeResults.size(); ++n) {
            if (step->state == MigrationStepState::WAITING_RESULT &&
                step->nodeResults[n].state == NodeResultState::WAITING) {
                if (step->failReason == UBSE_OK) {
                    step->failReason = MIGRATE_FAIL_RESULT_TIMEOUT;
                }
                UBSE_LOG_ERROR << "name=" << task->ctx.name << ", indexId=" << step->blockIndex
                               << ", operation=" << MigrationOperationName(step->operation)
                               << ", Migrate node result missing at stage timeout, nodeId="
                               << step->nodeResults[n].request.srcNodeId << ", timeoutSeconds=" << step->timeoutSeconds;
                step->nodeResults[n].state = NodeResultState::FAILED;
                step->nodeResults[n].transportRet = UBSE_ERR_TIMEOUT;
            }
        }
        step->state = MigrationStepState::RESULT_READY;
    }
    DispatchMigrationCompletion(task, step);
    return UBSE_OK;
}

// 发送成功只表示对端入队；各操作独立结果入口收到上报后才记业务结果。
template <typename ReqT>
void SendMigrateBlockRequest(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step,
                             size_t nodeIndex, UbseMemBorrowCallbackOpCode opCode, ReqT& request)
{
    if (!step) {
        return; // 阶段创建失败已记录原因并终止任务。
    }
    {
        std::lock_guard<std::mutex> lock(task->mutex);
        if (task->finished || task->step != step || step->state != MigrationStepState::WAITING_RESULT) {
            UBSE_LOG_WARN << "name=" << task->ctx.name << ", indexId=" << step->blockIndex
                          << ", operation=" << MigrationOperationName(step->operation)
                          << ", Skip request for closed migrate stage, nodeId="
                          << step->nodeResults[nodeIndex].request.srcNodeId;
            return;
        }
        request.rpcInfo = step->nodeResults[nodeIndex].request;
    }
    UBSE_LOG_INFO << "name=" << request.rpcInfo.name << ", indexId=" << request.rpcInfo.blockIndex
                  << ", operation=" << MigrationOperationName(step->operation) << ", Send migrate block request, "
                  << MigrateRpcLog(request.rpcInfo);
    const auto ret = SendMigrateMessage(request.rpcInfo.srcNodeId, opCode, request);
    if (ret != UBSE_OK) {
        RecordMigrationLocalFailure(task, step, nodeIndex, ret, MIGRATE_FAIL_RPC);
    } else {
        std::lock_guard<std::mutex> lock(task->mutex);
        // 请求已成功入队，但begin/copy/end是否执行成功仍需等待业务回包。
        // deadline初始为最大时间点；此处设为“当前时间+阶段超时”，供定时器识别回包超时，
        // 否则请求发送成功却一直没有回包时，该节点会持续等待，无法按预期触发超时处理。
        // RPC发送期间未持有task锁，其他线程可能已处理回包并推进阶段，因此仅在以下条件均满足时计时：
        // 任务仍处于本次发送的阶段、阶段仍在收集结果、该节点尚未收到结果或被标记失败。
        if (task->step == step && step->state == MigrationStepState::WAITING_RESULT &&
            step->nodeResults[nodeIndex].state == NodeResultState::WAITING) {
            step->nodeResults[nodeIndex].deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(step->timeoutSeconds);
        }
    }
}

bool SetCurrentExportSwitching(const std::shared_ptr<MigrationTask>& task, bool switching)
{
    auto& ctx = task->ctx;
    return UpdateMigrateExport(ctx.dstNodeId, ctx.name, [&](auto& obj) {
        auto& status = obj.status.exportObmmInfo[task->blockIndex].memIdStatus;
        // 仅设置或清除当前块的迁移标记，保留实际故障状态。
        if (status == (switching ? UB_MEM_HEALTHY : UB_MEM_SWITCHING)) {
            status = switching ? UB_MEM_SWITCHING : UB_MEM_HEALTHY;
        }
        ctx.newExportObj.status.exportObmmInfo[task->blockIndex].memIdStatus = status;
        return true;
    });
}

// 调用方持有task锁：所有阶段共用块身份校验，B的设备身份来自入口已校验的原导入快照。
static uint32_t CheckMigrationBlockIdentity(const MigrationTask& task, MigrationOperation operation)
{
    const auto& blocks = task.ctx.newExportObj.status.exportObmmInfo;
    const auto& markers = task.ctx.newExportObj.migrateInfos;
    const auto b = task.blockIndex;
    const bool mappedTarget = task.ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE;
    if (b >= markers.size() || (!mappedTarget && b >= blocks.size())) {
        UBSE_LOG_ERROR << "name=" << task.ctx.name << ", indexId=" << b
                       << ", operation=" << MigrationOperationName(operation)
                       << ", Cannot create migrate stage: missing block identity"
                       << ", blockCount=" << blocks.size() << ", markerCount=" << markers.size();
        const auto reason = task.cleanupReason.value_or(MIGRATE_FAIL_LAYOUT);
        return reason == UBSE_OK ? MIGRATE_FAIL_LAYOUT : reason;
    }
    return UBSE_OK;
}

// begin准备：只有A的begin_import需要在下发前标记当前导出块；B在begin_export结果中合入。
static bool PrepareMigrationBegin(const std::shared_ptr<MigrationTask>& task, MigrationOperation operation)
{
    const bool beginBlock = operation == MigrationOperation::BEGIN_IMPORT &&
                            task->ctx.targetKind == UbseMemMigrateTargetKind::NOT_IMPORT_NODE;
    return !beginBlock || SetCurrentExportSwitching(task, true);
}

// copy按块长计算等待时间，其他阶段使用固定超时。
static uint64_t MigrationOperationTimeout(const MigrationTask& task, MigrationOperation operation)
{
    if (operation != MigrationOperation::COPY) {
        return MIGRATE_PHASE_TIMEOUT_S;
    }
    const auto bytes = task.ctx.newExportObj.status.exportObmmInfo[task.blockIndex].desc.length;
    return MIGRATE_COPY_TIMEOUT_BASE_S + bytes / MIGRATE_COPY_TIMEOUT_BYTES_PER_S +
           (bytes % MIGRATE_COPY_TIMEOUT_BYTES_PER_S != 0 ? 1 : 0);
}

// 公共阶段监控：注册失败和空节点阶段均通过原完成入口处理；调用方不持有task锁。
static void StartMigrationStepMonitor(const std::shared_ptr<MigrationTask>& task,
                                      const std::shared_ptr<MigrationStep>& step)
{
    {
        const auto traceId = TraceContext::GetTraceId();
        const auto ret = timer::UbseTimerHandlerRegister(
            step->timerName,
            [task, step, traceId]() {
                TraceContext::SetTraceId(traceId);
                const auto result = MigratePhaseTimeout(task, step);
                TraceContext::Clear();
                return result;
            },
            1);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "name=" << task->ctx.name << ", indexId=" << step->blockIndex
                           << ", operation=" << MigrationOperationName(step->operation)
                           << ", Register migrate phase timer failed, ret=" << FormatRetCode(ret);
            {
                std::lock_guard<std::mutex> phaseLock(task->mutex);
                step->failReason = MIGRATE_FAIL_TIMER;
                for (auto& node : step->nodeResults) {
                    node.state = NodeResultState::FAILED;
                    node.transportRet = ret;
                }
                step->state = MigrationStepState::RESULT_READY;
            }
            DispatchMigrationCompletion(task, step);
            return;
        }
    }
    if (step->nodeResults.empty()) {
        DispatchMigrationCompletion(task, step);
    }
}

/**
 * @brief 创建当前迁移块的执行阶段，初始化节点结果记录并启动超时监控。
 * @param task 所属迁移任务。
 * @param operation 当前阶段的操作类型。
 * @param nodes 当前阶段的参与节点列表。
 * @param onComplete 阶段完成处理函数，统一处理成功、失败及超时结果。
 * @return 创建的阶段对象；块身份校验或阶段准备失败时返回 nullptr。
 */
std::shared_ptr<MigrationStep> StartMigrationOperation(
    const std::shared_ptr<MigrationTask>& task, MigrationOperation operation, const std::vector<std::string>& nodes,
    void (*onComplete)(const std::shared_ptr<MigrationTask>&, const std::shared_ptr<MigrationStep>&))
{
    std::unique_lock<std::mutex> lock(task->mutex);
    const auto identityRet = CheckMigrationBlockIdentity(*task, operation);
    if (identityRet != UBSE_OK) {
        lock.unlock();
        // 无合法块身份不能构造end请求，保留资源及未确认状态。
        FinishMigrationTask(task);
        FailMigration(task->ctx, identityRet, ExportCleanup::PRESERVE, task->failureDetail);
        return nullptr;
    }
    if (!PrepareMigrationBegin(task, operation)) {
        lock.unlock();
        FinishMigrationTask(task);
        FailMigration(task->ctx, MIGRATE_FAIL_CONVERGE, ExportCleanup::PRESERVE);
        return nullptr;
    }

    // 公共阶段及请求身份初始化；end(0)标记回退方向，end(1)沿用默认方向。
    const auto& ctx = task->ctx;
    const auto b = task->blockIndex;
    const bool mappedTarget = ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE;
    const auto memId = mappedTarget ? ctx.targetImportObj->status.importResults[b].memId :
                                      ctx.newExportObj.status.exportObmmInfo[b].memId;
    const auto timeoutSeconds = MigrationOperationTimeout(*task, operation);
    const bool rollback = operation == MigrationOperation::ROLLBACK_IMPORT ||
                          operation == MigrationOperation::ROLLBACK_EXPORT;
    const auto step = task->step = std::make_shared<MigrationStep>(
        MigrationStep{operation, b, MigrationStepState::WAITING_RESULT, timeoutSeconds, onComplete});
    step->timerName = ctx.name + "_" + std::to_string(reinterpret_cast<uintptr_t>(task.get())) + "_" +
                      std::to_string(b) + "_" + std::to_string(++task->stepSequence);
    step->nodeResults.assign(nodes.size(), {});
    for (size_t n = 0; n < nodes.size(); ++n) {
        auto& info = step->nodeResults[n].request;
        info.name = ctx.name;
        info.blockIndex = b;
        info.srcNodeId = nodes[n];
        info.rollback = rollback;
        info.exportMemId = memId;
        info.migrateInfo = ctx.newExportObj.migrateInfos[b];
    }
    UBSE_LOG_INFO << "name=" << ctx.name << ", indexId=" << b << ", operation=" << MigrationOperationName(operation)
                  << ", Start migrate stage, nodeCount=" << nodes.size() << ", timeoutSeconds=" << timeoutSeconds;
    if (nodes.empty()) {
        step->state = MigrationStepState::RESULT_READY;
    }
    lock.unlock();
    StartMigrationStepMonitor(task, step);
    return step;
}

bool MatchesMigrateRequest(const UbseMemMigrateRpcInfo& expected, const UbseMemMigrateRpcInfo& result)
{
    return expected.name == result.name && expected.blockIndex == result.blockIndex &&
           expected.srcNodeId == result.srcNodeId && expected.exportMemId == result.exportMemId &&
           expected.migrateInfo == result.migrateInfo && expected.rollback == result.rollback;
}

static bool IsCurrentMigration(const MigrationRunContext& ctx)
{
    return ctx.sessionToken != nullptr &&
           UbseMemShmMigrateSessionManager::GetInstance().GetSession(ctx.name) == ctx.sessionToken;
}

uint32_t StartNewExport(MigrationRunContext ctx)
{
    const auto ret =
        SendMigrateMessage(ctx.dstNodeId, UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_EXPORT, ctx.newExportObj);
    if (ret != UBSE_OK) {
        FailMigration(ctx, MIGRATE_FAIL_RPC, ExportCleanup::PRESERVE);
    }
    return ret;
}

static void StartExportCleanup(const MigrationRunContext& ctx, uint32_t reason)
{
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    if (!ctx.exportConfirmed) {
        UBSE_LOG_WARN << "Cannot clean up unconfirmed export, name=" << ctx.name << ", targetNodeId=" << ctx.dstNodeId;
        mgr.SetRebuildRequired(ctx.name, true);
        mgr.FinishSession(ctx.name, UBS_MEM_MIGRATE_STATE_FAILED, reason == UBSE_OK ? MIGRATE_FAIL_CONVERGE : reason);
        return;
    }
    auto exportObj = ctx.newExportObj;
    exportObj.returnReq = {};
    const auto ret = DeleteShareExport(exportObj);
    if (ret != UBSE_OK) {
        UBSE_LOG_WARN << "Unused export delete send failed; continue migrate cleanup, name=" << exportObj.req.name
                      << ", targetNodeId=" << ctx.dstNodeId << ", ret=" << FormatRetCode(ret);
    }
    exportObj.status.state = UBSE_MEM_EXPORT_DESTROYED;
    if (const auto cleanupRet = AddShareExport(exportObj); cleanupRet != UBSE_OK) {
        UBSE_LOG_WARN << "Unused export account cleanup failed; continue migrate cleanup, name=" << ctx.name
                      << ", targetNodeId=" << ctx.dstNodeId << ", ret=" << FormatRetCode(cleanupRet);
    }
    auto& exports = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>();
    const auto source = exports.GetResource(ctx.srcNodeId, ctx.name);
    if (source != nullptr) {
        exports.PutResource(ctx.srcNodeId, ctx.name, *source);
    }
    // 已确认没有引用；删除发送和旧账户回收失败均不改变本次迁移结果。
    UbseMemShmMigrateSession session{};
    if (mgr.ReadSessionSnapshot(ctx.name, session) && !session.completedBlocks.has_value()) {
        // 尚未开始块切换，临时目标清理后可以重新受理；已经切换过的资源仍需归还重建。
        mgr.SetRebuildRequired(ctx.name, false);
    }
    mgr.FinishSession(ctx.name, reason == UBSE_OK ? UBS_MEM_MIGRATE_STATE_CANCELED : UBS_MEM_MIGRATE_STATE_FAILED,
                      reason);
}

static void StartOldChainCleanup(const MigrationRunContext& ctx)
{
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    auto& exportMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>();
    const auto current = exportMap.GetResource(ctx.srcNodeId, ctx.name);
    if (current != nullptr) {
        auto exportObj = *current;
        exportObj.returnReq = {};
        const auto deleteRet = DeleteShareExport(exportObj);
        if (deleteRet != UBSE_OK) {
            UBSE_LOG_WARN << "Old export delete send failed; continue migrate cleanup, name=" << exportObj.req.name
                          << ", srcNodeId=" << ctx.srcNodeId << ", ret=" << FormatRetCode(deleteRet);
        }
    }
    // 删除请求会更新旧账本；重新读取后同步收尾，不等待旧源删除回包。
    const auto oldExport = exportMap.GetResource(ctx.srcNodeId, ctx.name);
    if (oldExport != nullptr) {
        // 只清理Master的旧关系；发送失败时仍可能残留物理导出，不作为迁移失败条件。
        auto removed = *oldExport;
        removed.status.state = UBSE_MEM_EXPORT_DESTROYED;
        if (const auto ret = AddShareExport(removed); ret != UBSE_OK) {
            UBSE_LOG_WARN << "Old export account cleanup failed; continue migrate cleanup, name=" << ctx.name
                          << ", srcNodeId=" << ctx.srcNodeId << ", ret=" << FormatRetCode(ret);
        }
    }
    auto& importMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>();
    const auto oldImport = importMap.GetResource(ctx.srcNodeId, ctx.name);
    if (oldImport != nullptr && oldImport->req.baseNodeId == ctx.srcNodeId) {
        auto removed = *oldImport;
        removed.status.state = UBSE_MEM_IMPORT_DESTROYED;
        if (const auto ret = AddShareImport(removed); ret != UBSE_OK) {
            UBSE_LOG_WARN << "Old source import account cleanup failed; continue migrate cleanup, name=" << ctx.name
                          << ", srcNodeId=" << ctx.srcNodeId << ", ret=" << FormatRetCode(ret);
        }
    }
    const auto target = exportMap.GetResource(ctx.dstNodeId, ctx.name);
    if (target == nullptr || target->status.state != UBSE_MEM_EXPORT_SUCCESS ||
        !IsSameExportResource(*target, ctx.newExportObj)) {
        UBSE_LOG_ERROR << "Final target export ledger missing or mismatched, name=" << ctx.name
                       << ", dstNodeId=" << ctx.dstNodeId;
        FailMigration(ctx, MIGRATE_FAIL_CONVERGE, ExportCleanup::PRESERVE);
        return;
    }
    auto finalExport = *target;
    finalExport.req.baseNodeId = ctx.dstNodeId;
    // 旧源的删除写回可能覆盖名称索引，收尾后重新指向有效目标。
    exportMap.PutResource(ctx.dstNodeId, ctx.name, finalExport);
    // A导出结果、B末块提交已登记目标账户；name级索引收尾不重复记账。
    mgr.FinishSession(ctx.name, UBS_MEM_MIGRATE_STATE_SUCCESS);
}

bool IsMigrationOperationCurrent(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step)
{
    std::lock_guard<std::mutex> lock(task->mutex);
    return step && !task->finished && task->step == step && step->state == MigrationStepState::WAITING_RESULT;
}

void StartAsyncBeginImport(const std::shared_ptr<MigrationTask>& task)
{
    const auto& ctx = task->ctx;
    const auto step =
        StartMigrationOperation(task, MigrationOperation::BEGIN_IMPORT, ctx.importNodes, OnBeginImportCompleted);
    for (size_t n = 0; n < ctx.importNodes.size(); ++n) {
        if (!IsMigrationOperationCurrent(task, step)) {
            break;
        }
        UbseMemMigrateSwitchBeginImportReq req{};
        if (!PrepareBeginImportRequest(ctx, ctx.importNodes[n], task->blockIndex, req)) {
            UBSE_LOG_ERROR << "name=" << ctx.name << ", indexId=" << task->blockIndex
                           << ", operation=begin_import, Cannot prepare migrate begin request, nodeId="
                           << ctx.importNodes[n];
            RecordMigrationLocalFailure(task, step, n, UBSE_ERR_INTERNAL, MIGRATE_FAIL_REQUEST_PREPARE);
            break;
        }
        SendMigrateBlockRequest(task, step, n, UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_SWITCH_BEGIN_IMPORT,
                                req);
    }
}

void StartAsyncCopy(const std::shared_ptr<MigrationTask>& task)
{
    auto& ctx = task->ctx;
    if (ctx.targetKind != UbseMemMigrateTargetKind::IMPORT_NODE && ctx.importNodes.empty()) {
        StartMigrationOperation(task, MigrationOperation::COPY, {}, OnCopyCompleted);
        return;
    }
    const auto step = StartMigrationOperation(
        task, MigrationOperation::COPY,
        {ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE ? ctx.dstNodeId : ctx.importNodes.front()},
        OnCopyCompleted);
    UbseMemMigrateDataCopyReq req{};

    SendMigrateBlockRequest(task, step, 0, UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_DATA_COPY, req);
}

void StartImportEnd(const std::shared_ptr<MigrationTask>& task, bool success);

void StartMappedBeginExport(const std::shared_ptr<MigrationTask>& task)
{
    auto& ctx = task->ctx;
    UbseMemMigrateBeginExportReq req{};
    req.exportObj = ctx.newExportObj;
    ctx.targetBeginStarted = true;
    task->targetBlockCommitted = false;
    const auto step =
        StartMigrationOperation(task, MigrationOperation::BEGIN_EXPORT, {ctx.dstNodeId}, OnBeginExportCompleted);
    SendMigrateBlockRequest(task, step, 0, UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_SWITCH_BEGIN_EXPORT, req);
}

void StartMappedEndExport(const std::shared_ptr<MigrationTask>& task, bool success)
{
    const auto& ctx = task->ctx;
    const auto operation = success ? MigrationOperation::END_EXPORT : MigrationOperation::ROLLBACK_EXPORT;
    if (success) {
        task->ctx.endCommitStarted = true;
    }
    UbseMemMigrateEndExportReq req{success ? 1U : 0U};
    const auto step = StartMigrationOperation(task, operation, {ctx.dstNodeId},
                                              success ? OnEndExportCompleted : OnExportRollbackCompleted);
    SendMigrateBlockRequest(task, step, 0, UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_EXPORT_END, req);
}

// A 先结束 copy 节点，再释放其余节点；B 的 copy 节点已经由目标 end_export 关闭。
void OnCopyNodeEndCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step);

bool HasConfirmedImportCommit(const UbseMemShareBorrowImportObj& obj, const UbseMemMigrateRpcInfo& info)
{
    if (IsMigrateBlockCommitted(obj, info)) {
        return true;
    }
    return obj.switchContext && MatchesSwitchContext(*obj.switchContext, info) &&
           obj.switchContext->endDecision == true;
}

void StartImportEndRange(const std::shared_ptr<MigrationTask>& task, bool success, size_t first, size_t count,
                         void (*onComplete)(const std::shared_ptr<MigrationTask>&,
                                            const std::shared_ptr<MigrationStep>&))
{
    if (success) {
        task->ctx.endCommitStarted = true;
    }
    const auto operation = success ? MigrationOperation::END_IMPORT : MigrationOperation::ROLLBACK_IMPORT;
    std::vector<std::string> nodes;
    for (size_t n = first; n < first + count; ++n) {
        const auto& nodeId = task->ctx.importNodes[n];
        if (!success) {
            const auto local = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(
                nodeId, task->ctx.name);
            const UbseMemMigrateRpcInfo info{task->ctx.name,
                                             task->blockIndex,
                                             nodeId,
                                             task->ctx.newExportObj.status.exportObmmInfo[task->blockIndex].memId,
                                             task->ctx.newExportObj.migrateInfos[task->blockIndex],
                                             true};
            if (local && HasConfirmedImportCommit(*local, info)) {
                UBSE_LOG_WARN << "name=" << info.name << ", indexId=" << info.blockIndex
                              << ", operation=end_import, Skip end(0) for confirmed committed import, nodeId="
                              << info.srcNodeId;
                continue;
            }
        }
        nodes.push_back(nodeId);
    }
    const auto step = StartMigrationOperation(task, operation, nodes, onComplete);
    for (size_t n = 0; n < nodes.size(); ++n) {
        if (!IsMigrationOperationCurrent(task, step)) {
            break;
        }
        UbseMemMigrateSwitchEndImportReq req{success ? 1U : 0U};
        SendMigrateBlockRequest(task, step, n, UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_SWITCH_END, req);
    }
}

void StartImportEnd(const std::shared_ptr<MigrationTask>& task, bool success)
{
    const auto& ctx = task->ctx;
    if (ctx.targetKind == UbseMemMigrateTargetKind::NOT_IMPORT_NODE && !ctx.importNodes.empty()) {
        if (!success) {
            // 已确认 copy 节点提交时，其入口已关闭，失败清理只需处理其余节点。
            const auto copyLedger =
                UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(
                    ctx.importNodes.front(), ctx.name);
            const UbseMemMigrateRpcInfo info{ctx.name, task->blockIndex, ctx.importNodes.front(),
                                             ctx.newExportObj.status.exportObmmInfo[task->blockIndex].memId,
                                             ctx.newExportObj.migrateInfos[task->blockIndex]};
            if (copyLedger && HasConfirmedImportCommit(*copyLedger, info)) {
                UBSE_LOG_WARN << "name=" << info.name << ", indexId=" << info.blockIndex
                              << ", operation=end_import, Skip end(0) for confirmed committed copy node, nodeId="
                              << info.srcNodeId;
                StartImportEndRange(task, false, 1, ctx.importNodes.size() - 1, OnRollbackCompleted);
                return;
            }
        }
        // 回退同样先关闭 copy 节点，否则失败清理也可能与迟到 copy 并发。
        StartImportEndRange(task, success, 0, 1, OnCopyNodeEndCompleted);
        return;
    }
    auto onComplete = success ? OnEndCompleted : OnRollbackCompleted;
    if (!success && ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE) {
        onComplete = OnImportRollbackCompleted;
    }
    StartImportEndRange(task, success, 0, ctx.importNodes.size(), onComplete);
}

// 目标最后一块end(1)成功即完成身份转换，不等待其余节点end，也不重复执行账户转换。
bool CommitMappedTarget(MigrationRunContext& ctx)
{
    ctx.newExportObj.status.state = UBSE_MEM_EXPORT_SUCCESS;
    ctx.newExportObj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    if (!UpdateMigrateExport(ctx.dstNodeId, ctx.name, [](auto& obj) {
            obj.status.state = UBSE_MEM_EXPORT_SUCCESS;
            obj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
            return true;
        })) {
        return false;
    }
    auto& importMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>();
    const auto selfImport = importMap.GetResource(ctx.dstNodeId, ctx.name);
    if (selfImport == nullptr || selfImport->realExe || selfImport->status.state != UBSE_MEM_IMPORT_SUCCESS) {
        UBSE_LOG_ERROR << "Incomplete self import at target account conversion, name=" << ctx.name
                       << ", targetNodeId=" << ctx.dstNodeId;
        return false;
    }
    auto oldImport = *ctx.targetImportObj;
    oldImport.status.state = UBSE_MEM_IMPORT_DESTROYED;
    SchedulerImpl::GetInstance().MemoryObjChangeHandler(oldImport);
    SchedulerImpl::GetInstance().MemoryObjChangeHandler(ctx.newExportObj);
    auto selfImportAccount = *selfImport;
    SchedulerImpl::GetInstance().MemoryObjChangeHandler(selfImportAccount);
    return true;
}

void BeginFailureCleanup(const std::shared_ptr<MigrationTask>& task, uint32_t reason)
{
    bool beginExportOnly = false;
    {
        std::lock_guard<std::mutex> lock(task->mutex);
        if (task->cleanupReason.has_value() || task->finished) {
            UBSE_LOG_WARN << "Ignore duplicate migrate cleanup, name=" << task->ctx.name
                          << ", indexId=" << task->blockIndex;
            return;
        }
        task->cleanupReason = reason;
        task->failureDetail = MigrationStepLog(*task->step);
        beginExportOnly = task->step->operation == MigrationOperation::BEGIN_EXPORT;
    }
    UbseMemShmMigrateSessionManager::GetInstance().SetRebuildRequired(task->ctx.name, true);
    if (reason != UBSE_OK) {
        UBSE_LOG_ERROR << "Migrate operation failed; start single cleanup, name=" << task->ctx.name
                       << ", indexId=" << task->blockIndex << ", srcNodeId=" << task->ctx.srcNodeId
                       << ", dstNodeId=" << task->ctx.dstNodeId << ", reason=" << reason
                       << ", reasonName=" << MigrateFailReasonName(reason)
                       << ", operation=" << MigrationOperationName(task->step->operation)
                       << ", targetBlockCommitted=" << task->targetBlockCommitted << ", failureDetail={"
                       << task->failureDetail << "}";
    } else {
        UBSE_LOG_INFO << "Migrate canceled; start single cleanup, name=" << task->ctx.name
                      << ", indexId=" << task->blockIndex;
    }
    if (task->ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE && beginExportOnly) {
        // 尚未下发其他节点begin_import，只回退目标设备。
        StartMappedEndExport(task, false);
    } else {
        StartImportEnd(task, false);
    }
}

void OnBeginImportCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step)
{
    if (step->failReason != UBSE_OK) {
        BeginFailureCleanup(task, step->failReason);
        return;
    }
    StartAsyncCopy(task);
}

void OnCopyNodeEndCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step)
{
    const bool success = step->operation == MigrationOperation::END_IMPORT;
    const bool canCloseOtherImports = success ? step->failReason == UBSE_OK : AllImportEndsConfirmed(*step);
    if (!canCloseOtherImports) {
        if (success) {
            BeginFailureCleanup(task, step->failReason);
        } else {
            for (size_t n = 1; n < task->ctx.importNodes.size(); ++n) {
                UBSE_LOG_ERROR << "name=" << task->ctx.name << ", indexId=" << task->blockIndex
                               << ", operation=end_import, Skip end(0): copy node cleanup unconfirmed, copyNodeId="
                               << task->ctx.importNodes.front() << ", nodeId=" << task->ctx.importNodes[n];
            }
            OnRollbackCompleted(task, step);
        }
        return;
    }
    StartImportEndRange(task, success, 1, task->ctx.importNodes.size() - 1,
                        success ? OnEndCompleted : OnRollbackCompleted);
}

void OnCopyCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step)
{
    if (step->failReason != UBSE_OK) {
        BeginFailureCleanup(task, step->failReason);
        return;
    }
    if (task->ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE) {
        StartMappedEndExport(task, true);
    } else {
        StartImportEnd(task, true);
    }
}

// 仅全部相关节点提交成功后调用；先提交账本，再发布完成进度。
void CompleteCurrentBlock(const std::shared_ptr<MigrationTask>& task)
{
    auto& ctx = task->ctx;
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    if (ctx.targetKind == UbseMemMigrateTargetKind::NOT_IMPORT_NODE && !SetCurrentExportSwitching(task, false)) {
        FinishMigrationTask(task);
        FailMigration(ctx, MIGRATE_FAIL_CONVERGE, ExportCleanup::PRESERVE);
        return;
    }
    if (!mgr.MarkBlockCompleted(ctx.name, ctx.sessionToken, task->blockIndex)) {
        UBSE_LOG_ERROR << "Migrate block progress rejected, name=" << ctx.name << ", indexId=" << task->blockIndex
                       << ", dstNodeId=" << ctx.dstNodeId;
        FinishMigrationTask(task);
        FailMigration(ctx, MIGRATE_FAIL_CONVERGE, ExportCleanup::PRESERVE);
        return;
    }
    UBSE_LOG_INFO << "Migrate block completed, name=" << ctx.name << ", indexId=" << task->blockIndex
                  << ", dstNodeId=" << ctx.dstNodeId << ", totalBlocks=" << ctx.newExportObj.migrateInfos.size();
    ++task->blockIndex;
    if (task->blockIndex == ctx.newExportObj.migrateInfos.size()) {
        FinishMigrationTask(task);
        FinishMigration(ctx);
    } else if (ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE) {
        StartMappedBeginExport(task);
    } else {
        StartAsyncBeginImport(task);
    }
}

void OnEndCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step)
{
    if (step->failReason != UBSE_OK) {
        BeginFailureCleanup(task, step->failReason);
        return;
    }
    CompleteCurrentBlock(task);
}

uint32_t GetCleanupReason(const std::shared_ptr<MigrationTask>& task)
{
    if (!task->cleanupReason.has_value()) {
        UBSE_LOG_ERROR << "Missing migrate cleanup reason, name=" << task->ctx.name << ", indexId=" << task->blockIndex;
        return MIGRATE_FAIL_CONVERGE;
    }
    return *task->cleanupReason;
}

enum class CleanupConclusion
{
    PRESERVE_RESOURCES,
    DELETE_NEW_EXPORT,
    CANCELED
};

void FinishCleanup(const std::shared_ptr<MigrationTask>& task, CleanupConclusion conclusion)
{
    UBSE_LOG_INFO << "Migrate cleanup attempt finished, name=" << task->ctx.name << ", indexId=" << task->blockIndex
                  << ", conclusion=" << static_cast<uint32_t>(conclusion)
                  << ", preserveResources=" << (conclusion == CleanupConclusion::PRESERVE_RESOURCES) << ", "
                  << MigrationStepLog(*task->step);
    FinishMigrationTask(task);
    if (conclusion == CleanupConclusion::CANCELED) {
        UbseMemShmMigrateSessionManager::GetInstance().FinishSession(task->ctx.name, UBS_MEM_MIGRATE_STATE_CANCELED);
        return;
    }
    auto reason = GetCleanupReason(task);
    // 保留原始业务失败；取消期间回退失败时，才以回退阶段错误作为本次失败原因。
    if (reason == UBSE_OK && conclusion == CleanupConclusion::PRESERVE_RESOURCES) {
        reason = task->step->failReason == UBSE_OK ? MIGRATE_FAIL_CONVERGE : task->step->failReason;
    }
    FailMigration(
        task->ctx, reason,
        conclusion == CleanupConclusion::DELETE_NEW_EXPORT ? ExportCleanup::ALLOW_DELETE : ExportCleanup::PRESERVE,
        task->failureDetail);
}

bool AllImportsRolledBack(const MigrationRunContext& ctx, uint32_t blockIndex)
{
    const bool mappedTarget = ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE;
    if (blockIndex >= ctx.newExportObj.migrateInfos.size() ||
        (mappedTarget && (!ctx.targetImportObj || blockIndex >= ctx.targetImportObj->status.importResults.size())) ||
        (!mappedTarget && blockIndex >= ctx.newExportObj.status.exportObmmInfo.size())) {
        UBSE_LOG_ERROR << "Cannot verify import rollback: missing block identity, name=" << ctx.name
                       << ", dstNodeId=" << ctx.dstNodeId << ", indexId=" << blockIndex;
        return false;
    }
    for (const auto& nodeId : ctx.importNodes) {
        const auto obj =
            UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(nodeId, ctx.name);
        const UbseMemMigrateRpcInfo info{ctx.name,
                                         blockIndex,
                                         nodeId,
                                         ctx.targetKind == UbseMemMigrateTargetKind::IMPORT_NODE ?
                                             ctx.targetImportObj->status.importResults[blockIndex].memId :
                                             ctx.newExportObj.status.exportObmmInfo[blockIndex].memId,
                                         ctx.newExportObj.migrateInfos[blockIndex],
                                         true};
        if (!obj || blockIndex >= obj->exportObmmInfo.size() || !obj->switchContext ||
            !MatchesSwitchContext(*obj->switchContext, info) || obj->switchContext->endDecision != false ||
            obj->exportObmmInfo[blockIndex].memIdStatus == UB_MEM_SWITCHING || obj->status.errCode != UBSE_OK) {
            return false;
        }
    }
    return true;
}

void OnRollbackCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step)
{
    const bool clean = AllImportEndsConfirmed(*step) && SetCurrentExportSwitching(task, false);
    // 发送过end(1)不等于已经提交；只有所有旧块回退事实完整时才解除保留。
    const bool restored = clean && task->blockIndex == 0 && AllImportsRolledBack(task->ctx, task->blockIndex);
    if (restored) {
        task->ctx.endCommitStarted = false;
    }
    if (!restored) {
        UBSE_LOG_WARN << "name=" << task->ctx.name << ", indexId=" << task->blockIndex
                      << ", operation=end_import(0), Preserve resources after import rollback, rollbackSucceeded="
                      << clean << ", endCommitStarted=" << task->ctx.endCommitStarted;
    }
    FinishCleanup(task, restored ? CleanupConclusion::DELETE_NEW_EXPORT : CleanupConclusion::PRESERVE_RESOURCES);
}

void OnBeginExportCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step)
{
    auto& ctx = task->ctx;
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    UbseMemShmMigrateSession snapshot{};
    if (!mgr.ReadSessionSnapshot(ctx.name, snapshot)) {
        UBSE_LOG_WARN << "name=" << ctx.name << ", indexId=" << task->blockIndex
                      << ", operation=begin_export, Ignore export begin completion: missing session";
        FinishMigrationTask(task);
        return;
    }
    if (step->failReason != UBSE_OK) {
        BeginFailureCleanup(task, snapshot.cancelRequested ? UBSE_OK : step->failReason);
        return;
    }
    const auto b = task->blockIndex;
    if (b == 0) {
        const auto decision = mgr.TryStartCopy(ctx.name, ctx.sessionToken);
        if (decision == UbseMemShmMigrateSessionManager::CopyStartResult::CANCELED) {
            BeginFailureCleanup(task, UBSE_OK);
            return;
        }
        if (decision != UbseMemShmMigrateSessionManager::CopyStartResult::STARTED) {
            UBSE_LOG_WARN << "name=" << ctx.name << ", indexId=" << b
                          << ", operation=begin_export, Mapped export completion cannot start copy, nodeId="
                          << ctx.dstNodeId;
            BeginFailureCleanup(task, MIGRATE_FAIL_CONVERGE);
            return;
        }
    }
    StartAsyncBeginImport(task);
    return;
}

bool CommitCurrentExportBlock(const std::shared_ptr<MigrationTask>& task)
{
    auto& ctx = task->ctx;
    task->targetBlockCommitted = true;
    // 内核提交事实立即写入正式块账本，不等待下一块begin或最后一块身份转换。
    if (!SetCurrentExportSwitching(task, false)) {
        return false;
    }
    const auto target =
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(ctx.dstNodeId, ctx.name);
    if (!target) {
        UBSE_LOG_ERROR << "name=" << ctx.name << ", indexId=" << task->blockIndex
                       << ", operation=end_export, Confirmed target commit missing export ledger";
        return false;
    }
    if (!UpdateMigrateImport(ctx.dstNodeId, ctx.name, [&](auto& obj) {
            if (!CommitSelfImportBlock(obj, *target, task->blockIndex)) {
                return false;
            }
            const auto& info = task->step->nodeResults.front().request;
            obj.switchContext = MakeSwitchContext(info);
            obj.switchContext->endDecision = true;
            return true;
        })) {
        UBSE_LOG_ERROR << "name=" << ctx.name << ", indexId=" << task->blockIndex
                       << ", operation=end_export, Master self import block commit failed";
        return false;
    }
    if (task->blockIndex + 1 == ctx.newExportObj.migrateInfos.size()) {
        return CommitMappedTarget(ctx);
    }
    return true;
}

void OnEndExportCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step)
{
    auto& ctx = task->ctx;
    if (step->failReason != UBSE_OK) {
        UBSE_LOG_ERROR << "name=" << ctx.name << ", indexId=" << task->blockIndex
                       << ", operation=end_export, Target end_export did not confirm commit, nodeId=" << ctx.dstNodeId;
        BeginFailureCleanup(task, step->failReason);
        return;
    }
    if (!CommitCurrentExportBlock(task)) {
        BeginFailureCleanup(task, MIGRATE_FAIL_CONVERGE);
        return;
    }
    if (ctx.importNodes.empty()) {
        CompleteCurrentBlock(task);
    } else {
        StartImportEnd(task, true);
    }
    return;
}

void OnImportRollbackCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step)
{
    if (step->failReason != UBSE_OK) {
        UBSE_LOG_ERROR << "name=" << task->ctx.name << ", indexId=" << task->blockIndex
                       << ", operation=end_import(0), Import rollback failed; skip target end(0) to preserve "
                          "referenced backend, targetNodeId="
                       << task->ctx.dstNodeId;
        // 远端gap可能仍引用目标后端，不能释放。
        FinishCleanup(task, CleanupConclusion::PRESERVE_RESOURCES);
        return;
    }
    if (task->targetBlockCommitted) {
        UBSE_LOG_WARN << "name=" << task->ctx.name << ", indexId=" << task->blockIndex
                      << ", operation=end_import(0), Skip target end(0): target already committed, targetNodeId="
                      << task->ctx.dstNodeId;
        FinishCleanup(task, CleanupConclusion::PRESERVE_RESOURCES);
        return;
    }
    StartMappedEndExport(task, false);
}

void OnExportRollbackCompleted(const std::shared_ptr<MigrationTask>& task, const std::shared_ptr<MigrationStep>& step)
{
    if (step->nodeResults.front().committed) {
        UBSE_LOG_WARN
            << "name=" << task->ctx.name << ", indexId=" << task->blockIndex
            << ", operation=end_export(0), Target already committed; apply commit and preserve target, nodeId="
            << task->ctx.dstNodeId;
        if (!CommitCurrentExportBlock(task)) {
            UBSE_LOG_ERROR << "name=" << task->ctx.name << ", indexId=" << task->blockIndex
                           << ", operation=end_export(0), Confirmed target commit requires ledger reconciliation";
        }
        FinishCleanup(task, CleanupConclusion::PRESERVE_RESOURCES);
        return;
    }
    if (step->failReason != UBSE_OK) {
        FinishCleanup(task, CleanupConclusion::PRESERVE_RESOURCES);
        return;
    }
    bool restored = task->blockIndex == 0 && !task->targetBlockCommitted &&
                    (task->ctx.importNodes.empty() || AllImportsRolledBack(task->ctx, task->blockIndex) ||
                     !task->ctx.endCommitStarted);
    auto& exportMap = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>();
    // 首块完整回退会删除临时导出；保留混合账本时也必须清除已结束块的迁移标记。
    if (!restored) {
        if (!UpdateMigrateExport(task->ctx.dstNodeId, task->ctx.name, [&](auto& obj) {
                if (task->blockIndex >= obj.status.exportObmmInfo.size()) {
                    UBSE_LOG_WARN << "name=" << task->ctx.name << ", indexId=" << task->blockIndex
                                  << ", operation=end_export(0), No successful begin result to clear";
                    return true;
                }
                auto& block = obj.status.exportObmmInfo[task->blockIndex];
                block.desc = {};
                if (block.memIdStatus == UB_MEM_SWITCHING) {
                    block.memIdStatus = UB_MEM_HEALTHY;
                }
                task->ctx.newExportObj.status.exportObmmInfo[task->blockIndex] = block;
                return true;
            })) {
            FinishCleanup(task, CleanupConclusion::PRESERVE_RESOURCES);
            return;
        }
    }
    if (restored) {
        task->ctx.endCommitStarted = false;
        task->ctx.targetBeginStarted = false;
        exportMap.RemoveResource(task->ctx.dstNodeId, task->ctx.name);
        // 临时目标曾占用名称索引；首块提交前完整回滚后恢复原导出的名称查询。
        const auto source = exportMap.GetResource(task->ctx.srcNodeId, task->ctx.name);
        if (source != nullptr) {
            exportMap.PutResource(task->ctx.srcNodeId, task->ctx.name, *source);
        } else {
            UBSE_LOG_ERROR << "name=" << task->ctx.name << ", indexId=" << task->blockIndex
                           << ", operation=end_export(0), Original export missing after target rollback, srcNodeId="
                           << task->ctx.srcNodeId;
            UbseMemShmMigrateSessionManager::GetInstance().SetRebuildRequired(task->ctx.name, true);
            restored = false;
        }
    }
    const bool canceled = restored && task->cleanupReason == UBSE_OK;
    FinishCleanup(task, canceled ? CleanupConclusion::CANCELED : CleanupConclusion::PRESERVE_RESOURCES);
}

static uint32_t StartBlockMigration(MigrationRunContext&& ctx)
{
    auto task = CreateMigrationTask(std::move(ctx));
    if (!task) {
        return UBSE_ERR_MIGRATE_IN_PROGRESS;
    }
    StartAsyncBeginImport(task);
    return UBSE_OK;
}

static uint32_t StartMappedMigration(MigrationRunContext&& ctx)
{
    auto task = CreateMigrationTask(std::move(ctx));
    if (!task) {
        return UBSE_ERR_MIGRATE_IN_PROGRESS;
    }
    StartMappedBeginExport(task);
    return UBSE_OK;
}

// false表示本次结果已忽略或已完成失败收尾，调用方直接返回。
bool ReadNewExportMigrationSession(const UbseMemShareBorrowExportObj& obj, UbseMemShmMigrateSession& session)
{
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    if (!mgr.ReadSessionSnapshot(obj.req.name, session)) {
        UBSE_LOG_WARN << "Ignore migrate export result: session not found, name=" << obj.req.name;
        return false;
    }
    if (session.state != UBS_MEM_MIGRATE_STATE_EXPORTING &&
        !(session.state == UBS_MEM_MIGRATE_STATE_FAILED && session.rebuildRequired &&
          !session.completedBlocks.has_value())) {
        UBSE_LOG_WARN << "Ignore migrate export result: unexpected session state, name=" << obj.req.name
                      << ", state=" << session.state << ", rebuildRequired=" << session.rebuildRequired
                      << ", copyStarted=" << session.completedBlocks.has_value();
        return false;
    }
    if (session.targetKind != UbseMemMigrateTargetKind::NOT_IMPORT_NODE) {
        UBSE_LOG_WARN << "Ignore migrate export result: target kind mismatch, name=" << obj.req.name
                      << ", targetKind=" << static_cast<uint32_t>(session.targetKind);
        return false;
    }
    const auto& exportNodeId = obj.algoResult.exportNumaInfos.front().nodeId;
    if (session.dstNodeId != exportNodeId) {
        UBSE_LOG_WARN << "Ignore migrate export result: target mismatch, name=" << obj.req.name
                      << ", expectedNodeId=" << session.dstNodeId << ", exportNodeId=" << exportNodeId;
        return false;
    }
    return true;
}

// 调用方持有name锁；复用已校验的会话快照，取消竞争由启动前的原子检查收敛。
void ContinueNewExportMigration(const UbseMemShareBorrowExportObj& result, const UbseMemShmMigrateSession& session)
{
    MigrationRunContext ctx{};
    std::optional<uint32_t> cleanupReason;
    auto& exports = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>();
    ctx.name = result.req.name;
    ctx.srcNodeId = session.srcNodeId;
    ctx.dstNodeId = session.dstNodeId;
    auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
    ctx.sessionToken = mgr.GetSession(ctx.name);
    if (session.state == UBS_MEM_MIGRATE_STATE_FAILED) {
        cleanupReason = session.failReason == UBSE_OK ? MIGRATE_FAIL_CONVERGE : session.failReason;
        UBSE_LOG_WARN << "Handle late migrate export result for cleanup, name=" << ctx.name
                      << ", targetNodeId=" << ctx.dstNodeId << ", reason=" << cleanupReason.value();
    }
    ctx.oldObj = exports.GetResource(ctx.srcNodeId, ctx.name);
    if (ctx.oldObj == nullptr) {
        cleanupReason = MIGRATE_FAIL_OLD_OBJ_LOST;
        UBSE_LOG_ERROR << "Migrate export result requires cleanup: old export ledger not found, name=" << ctx.name
                       << ", sourceNodeId=" << ctx.srcNodeId << ", targetNodeId=" << ctx.dstNodeId;
        mgr.SetRebuildRequired(ctx.name, true);
        mgr.FinishSession(ctx.name, UBS_MEM_MIGRATE_STATE_FAILED, MIGRATE_FAIL_OLD_OBJ_LOST);
        return;
    }
    ctx.newExportObj = result;
    ctx.exportConfirmed = true;
    const auto& obj = ctx.newExportObj;
    if (cleanupReason.has_value() || session.cancelRequested || obj.status.errCode != UBSE_OK) {
        const auto reason = session.cancelRequested ? UBSE_OK : cleanupReason.value_or(MIGRATE_FAIL_OBMM_EXPORT);
        UBSE_LOG_WARN << "Clean up confirmed migrate export, name=" << ctx.name << ", targetNodeId=" << ctx.dstNodeId
                      << ", cancelRequested=" << session.cancelRequested << ", reason=" << reason
                      << ", exportErrCode=" << FormatRetCode(obj.status.errCode);
        StartExportCleanup(ctx, reason);
        return;
    }
    // 块数不一致时保留新导出供核对；块描述无效且尚未切换时允许清理。
    if (obj.status.exportObmmInfo.size() != ctx.oldObj->status.exportObmmInfo.size()) {
        UBSE_LOG_ERROR << "Migrate export block count mismatch, name=" << ctx.name
                       << ", oldBlocks=" << ctx.oldObj->status.exportObmmInfo.size()
                       << ", newBlocks=" << obj.status.exportObmmInfo.size();
        FailMigration(ctx, MIGRATE_FAIL_LAYOUT, ExportCleanup::PRESERVE);
        return;
    }
    if (!PrepareImportNodes(ctx)) {
        FailMigration(ctx, MIGRATE_FAIL_LAYOUT, ExportCleanup::ALLOW_DELETE);
        return;
    }
    const auto decision = mgr.TryStartCopy(ctx.name, ctx.sessionToken);
    if (decision != UbseMemShmMigrateSessionManager::CopyStartResult::STARTED) {
        UBSE_LOG_WARN << "Cannot start block migration: copy checkpoint rejected, name=" << ctx.name
                      << ", targetNodeId=" << ctx.dstNodeId;
        if (decision == UbseMemShmMigrateSessionManager::CopyStartResult::CANCELED) {
            StartExportCleanup(ctx, UBSE_OK);
        }
        return;
    }
    StartBlockMigration(std::move(ctx));
}

} // namespace migrate_detail

void UbseMemShmMigrateService::NewExportResult(const UbseMemShareBorrowExportObj& result)
{
    UBSE_LOG_INFO << "Master received migrate business result, name=" << result.req.name << ", operation=new_export, "
                  << MigrateExportLog(result);
    if (const auto ret = CommitExportRsp(result); ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Migrate export ledger update failed, name=" << result.req.name
                       << ", ret=" << FormatRetCode(ret);
        auto& mgr = UbseMemShmMigrateSessionManager::GetInstance();
        mgr.SetRebuildRequired(result.req.name, true);
        mgr.FinishSession(result.req.name, UBS_MEM_MIGRATE_STATE_FAILED, MIGRATE_FAIL_CONVERGE);
        return;
    }
    auto nameLock = LoggingLockGuard(result.req.name);
    UbseMemShmMigrateSession session{};
    if (!ReadNewExportMigrationSession(result, session)) {
        return;
    }
    ContinueNewExportMigration(result, session);
}

bool IsSameExportResource(const UbseMemShareBorrowExportObj& left, const UbseMemShareBorrowExportObj& right)
{
    if (left.migrateInfos != right.migrateInfos) {
        return false;
    }
    if (left.status.exportObmmInfo.size() != right.status.exportObmmInfo.size()) {
        return false;
    }
    return std::equal(left.status.exportObmmInfo.begin(), left.status.exportObmmInfo.end(),
                      right.status.exportObmmInfo.begin(),
                      [](const auto& l, const auto& r) { return l.memId == r.memId; });
}

namespace migrate_detail {
void ReceiveOperationResult(const UbseMemMigrateOpResp& result, std::initializer_list<MigrationOperation> operations,
                            const UbseMemMigrateBeginExportResp* exportResult = nullptr)
{
    const auto& info = result.rpcInfo;
    UBSE_LOG_INFO << "Master received migrate business result, name=" << info.name << ", indexId=" << info.blockIndex
                  << ", nodeId=" << info.srcNodeId << ", resultRet=" << FormatRetCode(result.errCode)
                  << ", committed=" << result.committed << ", rollback=" << info.rollback;
    auto nameLock = LoggingLockGuard(info.name);
    std::shared_ptr<MigrationTask> task;
    {
        std::lock_guard<std::mutex> lock(g_migrateTasksMutex);
        const auto it = g_migrateTasks.find(info.name);
        if (it == g_migrateTasks.end()) {
            UBSE_LOG_WARN << "Migrate result has no task, name=" << info.name << ", indexId=" << info.blockIndex
                          << ", nodeId=" << info.srcNodeId;
            return;
        }
        task = it->second;
    }
    bool done = false;
    std::shared_ptr<MigrationStep> step;
    {
        std::lock_guard<std::mutex> lock(task->mutex);
        step = task->step;
        if (!IsCurrentMigration(task->ctx) || task->finished || !step ||
            step->state != MigrationStepState::WAITING_RESULT ||
            std::find(operations.begin(), operations.end(), step->operation) == operations.end()) {
            UBSE_LOG_WARN << "Migrate result inactive or wrong stage, name=" << info.name
                          << ", indexId=" << info.blockIndex
                          << ", expectedOperation=" << (step ? MigrationOperationName(step->operation) : "none")
                          << ", expectedIndexId=" << (step ? step->blockIndex : task->blockIndex)
                          << ", finished=" << task->finished << ", nodeId=" << info.srcNodeId;
            return;
        }
        const auto node = std::find_if(step->nodeResults.begin(), step->nodeResults.end(), [&](const auto& item) {
            return item.state == NodeResultState::WAITING && MatchesMigrateRequest(item.request, info);
        });
        if (node == step->nodeResults.end() || info.srcNodeId.empty()) {
            UBSE_LOG_WARN << "Duplicate/late migrate result or identity mismatch, name=" << info.name
                          << ", indexId=" << info.blockIndex << ", expected={" << MigrationStepLog(*step)
                          << "}, actual={" << MigrateRpcLog(info) << "}";
            return;
        }
        done = ApplyMigrationNodeResult(task, step, std::distance(step->nodeResults.begin(), node), UBSE_OK, result,
                                        exportResult);
    }
    if (done) {
        DispatchMigrationCompletion(task, step);
    }
}
} // namespace migrate_detail

void UbseMemShmMigrateService::BeginImportResult(const UbseMemMigrateOpResp& result)
{
    ReceiveOperationResult(result, {MigrationOperation::BEGIN_IMPORT});
}

void UbseMemShmMigrateService::BeginExportResult(const UbseMemMigrateBeginExportResp& result)
{
    UbseMemMigrateOpResp response{};
    response.rpcInfo = result.rpcInfo;
    response.errCode = result.errCode;
    ReceiveOperationResult(response, {MigrationOperation::BEGIN_EXPORT}, &result);
}

void UbseMemShmMigrateService::CopyResult(const UbseMemMigrateOpResp& result)
{
    ReceiveOperationResult(result, {MigrationOperation::COPY});
}

void UbseMemShmMigrateService::EndResult(const UbseMemMigrateOpResp& result)
{
    ReceiveOperationResult(result, {MigrationOperation::END_IMPORT, MigrationOperation::ROLLBACK_IMPORT,
                                    MigrationOperation::END_EXPORT, MigrationOperation::ROLLBACK_EXPORT});
}

} // namespace ubse::mem::controller
