/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2026-2026. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "ubse_mem_controller_migrate_api.h"

#include <unistd.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ios>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <utility>

#include "ubse_com_module.h"
#include "ubse_context.h"
#include "ubse_election.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_common_utils.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_debt_ledger.h"
#include "ubse_mem_decoder_utils.h"
#include "ubse_mem_migrate_helper.h"
#include "ubse_mem_util.h"
#include "ubse_mmi_interface.h"
#include "ubse_node_controller.h"
#include "ubse_obmm_utils.h"
#include "ubse_str_util.h"
#include "../logging_lock_guard.h"
#include "adapter_plugins/mti/ubse_mti_interface.h"
#include "api/ubse_mem_controller_share_api.h"
#include "message/ubse_mem_auto_serial.h"
#include "message/ubse_mem_callback_message.h"
#include "ubse_auto_simpo.hpp"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace ubse::log;
using namespace ubse::nodeController;
using namespace ubse::common::def;
using namespace ubse::context;
using namespace ubse::com;
using namespace ubse::mem::util;
using namespace ubse::mem::controller::debt;
using namespace ubse::adapter_plugins::mmi;
using namespace ubse::utils;
using namespace migrate_detail;

namespace migrate_detail {
namespace {
enum class AgentOperation
{
    BEGIN_IMPORT,
    BEGIN_EXPORT,
    COPY,
    END_IMPORT,
    END_EXPORT
};

const char* AgentOperationName(AgentOperation operation)
{
    switch (operation) {
        case AgentOperation::BEGIN_IMPORT:
            return "begin_import";
        case AgentOperation::BEGIN_EXPORT:
            return "begin_export";
        case AgentOperation::COPY:
            return "copy";
        case AgentOperation::END_IMPORT:
            return "end_import";
        case AgentOperation::END_EXPORT:
            return "end_export";
    }
    return "unknown";
}

// 所有块操作使用同一锁键；不同块不竞争资源级name锁。
// '/'不是合法资源名字符，避免与普通资源锁重名。
std::string MigrateBlockLockKey(const UbseMemMigrateRpcInfo& info)
{
    return "ShmMigrateBlock/" + info.name + "/" + std::to_string(info.blockIndex);
}

// 校验只发生在name+blockIndex锁内的执行入口。内部执行函数消费此处取得的同一账本快照。
// 仅在本次块锁内使用；context 指向 local 快照中已匹配的当前块。
struct CheckedMigrateBlock {
    ConstImportObjPtr local;
    const UbseMemMigrateSwitchContext* context = nullptr;
    bool committed = false;

    bool IsCommitConfirmed() const
    {
        return committed || (context && context->endDecision.value_or(false));
    }
};

// 公共输入校验：实际执行时确认控制者身份，再校验请求字段与end方向。
uint32_t CheckMigrateRequestIdentity(const UbseMemMigrateRpcInfo& info, const std::string& requestMasterNodeId,
                                     AgentOperation operation, uint32_t success)
{
    std::string masterNodeId;
    if (ubse::election::UbseGetMasterNodeId(masterNodeId) != UBSE_OK || requestMasterNodeId.empty() ||
        masterNodeId != requestMasterNodeId) {
        UBSE_LOG_WARN << "name=" << info.name << ", indexId=" << info.blockIndex
                      << ", operation=" << AgentOperationName(operation)
                      << ", Reject migrate request from inactive Master, requestMasterNodeId=" << requestMasterNodeId
                      << ", masterNodeId=" << masterNodeId;
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    const bool ending = operation == AgentOperation::END_IMPORT || operation == AgentOperation::END_EXPORT;
    const bool rollbackEnd = ending && success == 0;
    if (info.name.empty() || info.srcNodeId != GetCurNodeId() || info.exportMemId == 0 ||
        !ShmMigrateIsExport(info.migrateInfo) || ShmMigrateOrigin(info.migrateInfo) == 0 ||
        ShmMigrateIndex(info.migrateInfo) != info.blockIndex || success > 1 || info.rollback != rollbackEnd) {
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << info.blockIndex
                       << ", operation=" << AgentOperationName(operation)
                       << ", Invalid migrate request identity, nodeId=" << info.srcNodeId;
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

// 无匹配上下文：确认旧操作已收尾，且请求对应账本中的下一块/下一轮；仅允许begin或回退。
uint32_t CheckMigrateWithoutContext(const UbseMemMigrateRpcInfo& info, AgentOperation operation, bool commitEnd,
                                    const UbseMemShareBorrowImportObj& local)
{
    const auto& context = local.switchContext;
    const auto b = info.blockIndex;
    // 当前上下文尚有 gap 或待提交资源时，不得被下一块/下一轮覆盖。
    if (context && (context->blockIndex >= local.exportObmmInfo.size() || context->target || !context->endDecision ||
                    local.exportObmmInfo[context->blockIndex].memIdStatus == UB_MEM_SWITCHING)) {
        UBSE_LOG_WARN << "name=" << info.name << ", indexId=" << b << ", operation=" << AgentOperationName(operation)
                      << ", Migrate previous block unresolved, pendingIndex=" << context->blockIndex;
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    const auto version = ShmMigrateVersion(info.migrateInfo);
    const auto nextVersion = (ShmMigrateVersion(local.migrateInfos[b]) + 1U) & 0xfU;
    const bool previousBlockCommitted = b == 0 ||
                                        local.migrateInfos[b - 1] == MakeShmMigrateInfo(version, b - 1, 0, false);
    if (!context && local.exportObmmInfo[b].memIdStatus == UB_MEM_SWITCHING && version == nextVersion &&
        previousBlockCommitted &&
        (operation == AgentOperation::COPY || operation == AgentOperation::END_IMPORT ||
         operation == AgentOperation::END_EXPORT)) {
        UbseMemMigrateBlockData actual;
        const auto ret =
            UbseMmiInterface::GetInstance().ReadShmMigrationBlock(local.status.importResults[b].memId, info, actual);
        // 重启丢失上下文时，只对实际gap放行copy；实际已提交时只允许补end(1)后的账本。
        if (ret == UBSE_ERR_MIGRATE_IN_PROGRESS || (ret == UBSE_OK && commitEnd)) {
            return UBSE_OK;
        }
        return ret == UBSE_OK ? UBSE_ERR_MIGRATE_NOT_ALLOWED : ret;
    }
    if (local.exportObmmInfo[b].memIdStatus != UB_MEM_HEALTHY || version != nextVersion || !previousBlockCommitted) {
        UBSE_LOG_WARN << "name=" << info.name << ", indexId=" << b << ", operation=" << AgentOperationName(operation)
                      << ", Migrate block is not next in ledger, memIdStatus=" << local.exportObmmInfo[b].memIdStatus
                      << ", localMigrateInfo=" << local.migrateInfos[b];
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    if (operation == AgentOperation::COPY || commitEnd) {
        UBSE_LOG_WARN << "name=" << info.name << ", indexId=" << b << ", operation=" << AgentOperationName(operation)
                      << ", Migrate operation missing pending block";
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    return UBSE_OK;
}

// 有匹配上下文：已决定结束或发生失败后关闭begin/copy入口，end只允许沿既定方向执行。
uint32_t CheckMigrateWithContext(const UbseMemMigrateRpcInfo& info, AgentOperation operation, uint32_t success,
                                 const CheckedMigrateBlock& block)
{
    const auto& local = block.local;
    const auto* context = block.context;
    const auto b = info.blockIndex;
    const bool ending = operation == AgentOperation::END_IMPORT || operation == AgentOperation::END_EXPORT;
    const bool commitEnd = ending && success != 0;
    const bool conflictsWithDecision = context->endDecision && (!ending || *context->endDecision != commitEnd);
    const bool actionAfterFailure = !ending && local->status.errCode != UBSE_OK;
    if (conflictsWithDecision || actionAfterFailure) {
        UBSE_LOG_WARN << "name=" << info.name << ", indexId=" << b << ", operation=" << AgentOperationName(operation)
                      << ", Migrate block already closing, endDecision="
                      << (context->endDecision ? (*context->endDecision ? "commit" : "rollback") : "none")
                      << ", success=" << success << ", errCode=" << FormatRetCode(local->status.errCode);
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    return UBSE_OK;
}

uint32_t CheckMigrateRequest(const UbseMemMigrateRpcInfo& info, const std::string& requestMasterNodeId,
                             AgentOperation operation, uint32_t success, CheckedMigrateBlock& block)
{
    auto& local = block.local;
    UBSE_LOG_INFO << "name=" << info.name << ", indexId=" << info.blockIndex
                  << ", operation=" << AgentOperationName(operation) << ", Agent executing migrate request, "
                  << MigrateRpcLog(info) << ", requestMasterNodeId=" << requestMasterNodeId
                  << ", localNodeId=" << GetCurNodeId() << ", success=" << success;
    if (const auto ret = CheckMigrateRequestIdentity(info, requestMasterNodeId, operation, success); ret != UBSE_OK) {
        return ret;
    }
    const bool ending = operation == AgentOperation::END_IMPORT || operation == AgentOperation::END_EXPORT;
    const bool commitEnd = ending && success != 0;
    const bool rollbackEnd = ending && success == 0;

    // 读取一次本地账本并校验块布局；后续执行继续使用同一快照。
    local = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowImportObj>().GetResource(GetCurNodeId(),
                                                                                                   info.name);
    const auto b = info.blockIndex;
    if (!local || b >= local->status.importResults.size() || b >= local->exportObmmInfo.size() ||
        b >= local->migrateInfos.size() || local->status.importResults[b].memId == 0 ||
        local->exportObmmInfo[b].desc.length == 0 || local->status.state != UBSE_MEM_IMPORT_SUCCESS) {
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << b << ", operation=" << AgentOperationName(operation)
                       << ", Migrate local import block missing or invalid, nodeId=" << GetCurNodeId();
        return UBSE_ERR_NOT_EXIST;
    }
    block.committed = IsMigrateBlockCommitted(*local, info);
    const auto& context = local->switchContext;
    block.context = context && MatchesSwitchContext(*context, info) ? &*context : nullptr;
    // 已提交块优先处理幂等请求，禁止再次copy或回退。
    if (block.committed) {
        if (operation == AgentOperation::COPY || rollbackEnd) {
            UBSE_LOG_WARN << "name=" << info.name << ", indexId=" << b
                          << ", operation=" << AgentOperationName(operation)
                          << ", Reject copy/abort of committed block";
            return UBSE_ERR_MIGRATE_NOT_ALLOWED;
        }
        return UBSE_OK;
    }
    // 按是否存在本次请求的上下文，分别校验新操作准入或已有操作的结束决定。
    const auto ret = block.context ? CheckMigrateWithContext(info, operation, success, block) :
                                     CheckMigrateWithoutContext(info, operation, commitEnd, *local);
    if (ret != UBSE_OK) {
        return ret;
    }

    // copy和提交必须处于切换中；begin与回退允许尚未进入gap的准备状态。
    const bool requiresSwitching = operation == AgentOperation::COPY || commitEnd;
    if (requiresSwitching && local->exportObmmInfo[b].memIdStatus != UB_MEM_SWITCHING) {
        UBSE_LOG_WARN << "name=" << info.name << ", indexId=" << b << ", operation=" << AgentOperationName(operation)
                      << ", Migrate block is not switching, memIdStatus=" << local->exportObmmInfo[b].memIdStatus;
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    return UBSE_OK;
}

bool SaveContext(const UbseMemMigrateRpcInfo& info, const UbseMemMigrateSwitchContext& context,
                 uint32_t error = UBSE_OK, bool switching = false)
{
    return UpdateMigrateImport(GetCurNodeId(), info.name, [&](auto& obj) {
        obj.switchContext = context;
        obj.status.errCode = error;
        if (switching) {
            obj.exportObmmInfo[info.blockIndex].memIdStatus = UB_MEM_SWITCHING;
        }
        return true;
    });
}

uint32_t CloseFailedOperation(const UbseMemMigrateRpcInfo& info, UbseMemMigrateSwitchContext& context, uint32_t ret)
{
    context.endDecision = false;
    if (!SaveContext(info, context, ret)) {
        return UBSE_ERR_INTERNAL;
    }
    UBSE_LOG_ERROR << "Migrate block operation failed; await rollback, name=" << info.name
                   << ", indexId=" << info.blockIndex << ", ret=" << FormatRetCode(ret);
    return ret;
}

// begin同步返回失败: 回退内核调用前预置的SWITCHING标记，
// 否则end(0)会误入内核关窗路径去关闭不存在的间隙(ESTALE)，导致master误判回退失败、资源误保留。
void RollbackPreparedSwitching(const UbseMemMigrateRpcInfo& info)
{
    UpdateMigrateImport(GetCurNodeId(), info.name, [&](auto& obj) {
        if (info.blockIndex < obj.exportObmmInfo.size() &&
            obj.exportObmmInfo[info.blockIndex].memIdStatus == UB_MEM_SWITCHING) {
            obj.exportObmmInfo[info.blockIndex].memIdStatus = UB_MEM_HEALTHY;
        }
        return true;
    });
}

// 临时方案：旧 decoder 暂存到最后一块提交或回退收尾；回退同时清理目标 decoder。
void CleanupMigrateDecoder(const UbseMemMigrateRpcInfo& info, const UbseMemShareBorrowImportObj& local, bool success)
{
    using ubse::adapter_plugins::mti::mami::UbseMamiMemWithdraw;
    const ubse::adapter_plugins::mti::mami::UbseMamiMemImportResult* entry = nullptr;
    auto socketId = local.algoResult.attachSocketId;
    if (success && info.blockIndex < local.status.decoderResult.size() &&
        local.status.decoderResult[info.blockIndex].hpa != 0) {
        entry = &local.status.decoderResult[info.blockIndex];
    } else if (!success && local.switchContext && local.switchContext->target && local.switchContext->target->decoder) {
        entry = &*local.switchContext->target->decoder;
        socketId = local.switchContext->target->targetLayout.attachSocketId;
    }
    std::optional<UbseMamiMemWithdraw> current;
    if (entry != nullptr) {
        std::pair<uint32_t, uint32_t> chipDie{};
        if (decoder::utils::MemDecoderUtils::GetChipAndDieId(socketId, chipDie) == UBSE_OK) {
            current = UbseMamiMemWithdraw{
                chipDie.first, chipDie.second, entry->marId,
                decoder::utils::MemDecoderUtils::GetDecoderIdByPrivData(local.req.ubseMemPrivData), entry->handle};
        } else {
            UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << info.blockIndex
                           << ", operation=end, Migrate decoder location unavailable; defer to cycle cleanup, socketId="
                           << socketId;
        }
    }
    std::vector<UbseMamiMemWithdraw> withdrawals;
    UpdateMigrateImport(GetCurNodeId(), info.name, [&](auto& obj) {
        if (success && current) {
            obj.deferredMigrateDecoders.push_back(*current);
        }
        if (!success || info.blockIndex + 1 == local.status.importResults.size()) {
            withdrawals.swap(obj.deferredMigrateDecoders);
        }
        return true;
    });
    if (!success && current) {
        withdrawals.push_back(*current);
    }
    for (const auto& withdraw : withdrawals) {
        UBSE_LOG_INFO << "name=" << info.name << ", indexId=" << info.blockIndex
                      << ", operation=end, Migrate decoder cleanup, success=" << success
                      << ", ubpuId=" << withdraw.ubpuId << ", iouId=" << withdraw.iouId << ", marId=" << withdraw.marId
                      << ", decoderId=" << static_cast<uint32_t>(withdraw.decoderIdx) << ", handle=" << withdraw.handle
                      << ", nodeId=" << info.srcNodeId;
        const auto ret = ubse::adapter_plugins::mti::UbseMtiInterface::GetInstance().DeleteDecoderEntry(withdraw);
        if (ret != UBSE_OK) {
            UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << info.blockIndex
                           << ", operation=end, Migrate decoder cleanup failed; defer to cycle cleanup, success="
                           << success << ", ret=" << FormatRetCode(ret);
        }
    }
}

uint32_t BuildImportSwitch(const UbseMemShareBorrowImportObj& blockObj, const UbseMemMigrateRpcInfo& info,
                           uint64_t memId, size_t blockCount, UbseMemShmSwitchObj& target)
{
    const auto& block = blockObj.exportObmmInfo.front();
    const auto& decoder = blockObj.status.decoderResult.front();
    target.name = info.name;
    target.importNodeId = GetCurNodeId();
    target.oldImportMemId = memId;
    target.newPa = decoder.hpa;
    target.newScna = block.desc.scna;
    target.newDcna = block.desc.dcna;
    target.baseDist = 40;
    FillEid16FromU32(target.newSeid, block.desc.seid);
    FillEid16FromU32(target.newDeid, block.desc.deid);
    ubse::mmi::UbMemPrivData priv{};
    ubse::mmi::CopyUbMemPrivData(priv, blockObj.req.ubseMemPrivData);
    priv.mar_id = static_cast<uint16_t>(decoder.marId);
    ubse::mmi::UbseMemLocalObmmCustomMeta meta{};
    const auto ret = ubse::mmi::GetCustomMetaFromShmImportObj(blockObj, meta);
    if (ret != UBSE_OK) {
        return ret;
    }
    meta.migrateInfo = ShmMigrateImportInfo(info.migrateInfo);
    meta.memidCount = blockCount;
    meta.exportMemid = info.exportMemId;
    meta.decoderResult = decoder;
    ubse::mmi::ObmmEncodedMeta encoded{};
    uint32_t length{};
    const auto encodeRet = ubse::mmi::EncodeObmmCustomMeta(meta, priv, encoded, length);
    if (encodeRet != UBSE_OK) {
        UBSE_LOG_ERROR << "Build import switch priv failed, name=" << info.name << ", memId=" << memId
                       << ", ret=" << encodeRet;
        return encodeRet;
    }
    target.priv.assign(encoded.begin(), encoded.begin() + length);
    return UBSE_OK;
}

uint32_t BeginImport(const UbseMemMigrateSwitchBeginImportReq& req, const UbseMemShareBorrowImportObj& local,
                     UbseMemMigrateOpResp& result)
{
    const auto& info = req.rpcInfo;
    auto context = MakeSwitchContext(info);
    context.target = UbseMemMigrateBlockData{req.targetBlock, std::nullopt, req.targetLayout};
    // 复用现有 decoder 和 priv 路径，但工作对象只有当前块，生命周期止于本次调用。
    UbseMemShareBorrowImportObj work{};
    work.req = local.req;
    work.importNodeId = local.importNodeId;
    work.req.trustRingData = req.trustRingData;
    work.algoResult = req.targetLayout;
    work.exportObmmInfo = {req.targetBlock};
    std::pair<uint32_t, uint32_t> chipDie{};
    std::shared_lock decoderLock(GetDecoderImportMutex());
    if (decoder::utils::MemDecoderUtils::GetChipAndDieId(req.targetLayout.attachSocketId, chipDie) != UBSE_OK) {
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << info.blockIndex
                       << ", operation=begin_import, Migrate decoder local topology missing, attachSocketId="
                       << req.targetLayout.attachSocketId << ", nodeId=" << info.srcNodeId;
        return CloseFailedOperation(info, context, UBSE_ERR_INTERNAL);
    }
    auto ret = RealImportDecoder(chipDie, work);
    if (ret == UBSE_OK && work.status.decoderResult.size() == 1) {
        context.target->decoder = work.status.decoderResult.front();
        UBSE_LOG_INFO << "name=" << info.name << ", indexId=" << info.blockIndex
                      << ", operation=begin_import, Migrate decoder created, ubpuId=" << chipDie.first
                      << ", iouId=" << chipDie.second << ", marId=" << context.target->decoder->marId
                      << ", handle=" << context.target->decoder->handle;
    }
    if (ret != UBSE_OK || !context.target->decoder || context.target->decoder->hpa == 0) {
        UBSE_LOG_ERROR << "Migrate decoder result invalid, resultCount=" << work.status.decoderResult.size()
                       << ", hasAddress=" << (context.target->decoder && context.target->decoder->hpa != 0)
                       << ", ret=" << FormatRetCode(ret);
        return CloseFailedOperation(info, context, ret != UBSE_OK ? ret : UBSE_ERR_INTERNAL);
    }
    UbseMemShmSwitchObj target{};
    ret = BuildImportSwitch(work, info, local.status.importResults[info.blockIndex].memId,
                            local.status.importResults.size(), target);
    if (ret != UBSE_OK) {
        return CloseFailedOperation(info, context, ret);
    }
    // decoder 锁覆盖创建到落账，周期清理不会采集到尚未登记的新 handle。
    // 内核调用前一次保存完整 target 和 SWITCHING；准备失败由 CloseFailedOperation 保存。
    if (!SaveContext(info, context, UBSE_OK, true)) {
        return UBSE_ERR_INTERNAL;
    }
    ret = UbseMmiInterface::GetInstance().ShmSwitchBeginImportExecutor(target);
    if (ret != UBSE_OK) {
        RollbackPreparedSwitching(info);
        return CloseFailedOperation(info, context, ret);
    }
    result.block = context.target;
    return UBSE_OK;
}

void PrepareExportEnd(UbseMemShareBorrowExportObj& obj, uint32_t b, bool success)
{
    // begin 尚未产生当前块时，回退无需清除该块；提交路径已由入口检查索引。
    if (b >= obj.status.exportObmmInfo.size()) {
        return;
    }
    auto& block = obj.status.exportObmmInfo[b];
    if (block.memIdStatus == UB_MEM_SWITCHING) {
        block.memIdStatus = UB_MEM_HEALTHY;
    }
    if (!success) {
        block.desc = {};
    } else if (b + 1 == obj.migrateInfos.size()) {
        obj.status.state = UBSE_MEM_EXPORT_SUCCESS;
        obj.status.expectState = UBSE_MEM_EXPORT_SUCCESS;
    }
}

// 只修改准备副本；设备 end 成功后才发布结束决定。
void PrepareEndContext(const UbseMemMigrateRpcInfo& info, bool success, UbseMemShareBorrowImportObj& obj)
{
    auto context = obj.switchContext && MatchesSwitchContext(*obj.switchContext, info) ? *obj.switchContext :
                                                                                         MakeSwitchContext(info);
    context.endDecision = success;
    context.target.reset();
    obj.switchContext = std::move(context);
    obj.status.errCode = UBSE_OK;
}

uint32_t EndBlockDevice(const UbseMemMigrateRpcInfo& info, bool success, const CheckedMigrateBlock& block)
{
    const auto& local = *block.local;
    if (success && block.IsCommitConfirmed()) {
        return UBSE_OK; // 已确认end(1)，只重试尚未完成的回读与落账。
    }
    if (success && !block.context) {
        UbseMemMigrateBlockData actual;
        if (UbseMmiInterface::GetInstance().ReadShmMigrationBlock(local.status.importResults[info.blockIndex].memId,
                                                                  info, actual) == UBSE_OK) {
            return UBSE_OK; // end已生效，但重启前尚未落账。
        }
    }
    if (local.exportObmmInfo[info.blockIndex].memIdStatus != UB_MEM_SWITCHING) {
        return UBSE_OK; // begin 未进入切换态时，回退只需清理账本。
    }
    UbseMemShmSwitchObj device{};
    device.name = info.name;
    device.importNodeId = GetCurNodeId();
    device.oldImportMemId = local.status.importResults[info.blockIndex].memId;
    const auto ret = UbseMmiInterface::GetInstance().ShmSwitchEndExecutor(device, success);
    if (ret != UBSE_OK) {
        auto context = block.context ? *block.context : MakeSwitchContext(info);
        if (!success) {
            context.endDecision = false;
        }
        SaveContext(info, context, ret);
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << info.blockIndex
                       << ", Migrate end unconfirmed; device cleanup still required, success=" << success
                       << ", ret=" << FormatRetCode(ret) << ", nodeId=" << info.srcNodeId;
    }
    return ret;
}

// 导入端只更新当前导入块及最后一块的来源布局。
void PublishImportEnd(const UbseMemMigrateRpcInfo& info, bool success, const UbseMemShareBorrowImportObj& prepared,
                      const CheckedMigrateBlock& block)
{
    UpdateMigrateImport(GetCurNodeId(), info.name, [&](auto& obj) {
        if (success) {
            ApplyEndSuccess(obj, info, *block.context->target);
        } else {
            ApplyEndRollback(obj, info.blockIndex);
        }
        obj.switchContext = prepared.switchContext;
        obj.status.errCode = UBSE_OK;
        return true;
    });
}

// 导出端同时维护导出块与 self-import；最后一块发布准备好的身份转换结果。
void PublishExportEnd(const UbseMemMigrateRpcInfo& info, bool success, const UbseMemShareBorrowImportObj& prepared,
                      const ConstExportObjPtr& exp)
{
    const auto nodeId = GetCurNodeId();
    const auto b = info.blockIndex;
    if (exp) {
        UpdateMigrateExport(nodeId, info.name, [&](auto& obj) {
            PrepareExportEnd(obj, b, success);
            return true;
        });
    }
    UpdateMigrateImport(nodeId, info.name, [&](auto& obj) {
        if (success) {
            obj.exportObmmInfo[b] = prepared.exportObmmInfo[b];
            obj.migrateInfos[b] = prepared.migrateInfos[b];
            if (b < obj.status.decoderResult.size()) {
                obj.status.decoderResult[b] = {};
            }
            if (b + 1 == exp->migrateInfos.size()) {
                obj.status.decoderResult.clear();
                obj.algoResult.exportNumaInfos = prepared.algoResult.exportNumaInfos;
                obj.req.baseNodeId = prepared.req.baseNodeId;
                obj.req.lenderInfo.nodeId = prepared.req.lenderInfo.nodeId;
                obj.realExe = prepared.realExe;
                obj.req.trustRingData = prepared.req.trustRingData;
                obj.status.state = prepared.status.state;
                obj.status.expectState = prepared.status.expectState;
            }
        } else {
            ApplyEndRollback(obj, b);
        }
        obj.switchContext = prepared.switchContext;
        obj.status.errCode = UBSE_OK;
        return true;
    });
    if (!success && exp && b == 0) {
        // 首块回退删除临时导出；后续块回退保留前面已经提交的块。
        UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().RemoveResource(nodeId, info.name);
    }
}

// 恢复提交先保存已提交事实，再回读设备；回读失败也不得重新copy或回退。
uint32_t CommitAndReadRecoveryBlock(const UbseMemMigrateRpcInfo& info, const CheckedMigrateBlock& block,
                                    UbseMemMigrateOpResp& result, UbseMemMigrateBlockData& actual)
{
    if (const auto ret = EndBlockDevice(info, true, block); ret != UBSE_OK) {
        return ret;
    }
    result.committed = true;
    auto context = MakeSwitchContext(info);
    context.endDecision = true;
    if (!SaveContext(info, context)) {
        return UBSE_ERR_INTERNAL;
    }
    return UbseMmiInterface::GetInstance().ReadShmMigrationBlock(
        block.local->status.importResults[info.blockIndex].memId, info, actual);
}

uint32_t EndImportBlock(const UbseMemMigrateRpcInfo& info, bool success, const CheckedMigrateBlock& block,
                        UbseMemMigrateOpResp& result)
{
    result.committed = block.committed;
    if (block.committed) {
        result.block = ReadMigrateBlock(*block.local, info.blockIndex);
        return UBSE_OK;
    }
    auto prepared = *block.local;
    if (success && (!block.context || !block.context->target)) {
        UbseMemMigrateBlockData actual;
        const auto ret = CommitAndReadRecoveryBlock(info, block, result, actual);
        if (ret != UBSE_OK || !actual.decoder || actual.targetLayout.exportNumaInfos.empty()) {
            return ret == UBSE_OK ? UBSE_ERR_INTERNAL : ret;
        }
        PrepareEndContext(info, true, prepared);
        actual.targetLayout.blockSize = prepared.algoResult.blockSize;
        const bool updated = UpdateMigrateImport(GetCurNodeId(), info.name, [&](auto& obj) {
            obj.status.decoderResult.resize(obj.status.importResults.size());
            ApplyEndSuccess(obj, info, actual);
            obj.switchContext = prepared.switchContext;
            obj.status.errCode = UBSE_OK;
            return true;
        });
        if (!updated) {
            return UBSE_ERR_INTERNAL;
        }
        result.block = actual;
        CleanupMigrateDecoder(info, *block.local, true);
        return UBSE_OK;
    }
    if (success) {
        const auto& target = *block.context->target;
        if (!target.decoder || (info.blockIndex + 1 == prepared.status.importResults.size() &&
                                target.targetLayout.exportNumaInfos.empty())) {
            UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << info.blockIndex
                           << ", Cannot prepare end_import: pending block or layout missing";
            return UBSE_ERR_INTERNAL;
        }
        ApplyEndSuccess(prepared, info, target);
    } else {
        ApplyEndRollback(prepared, info.blockIndex);
    }
    PrepareEndContext(info, success, prepared);
    auto resultBlock = ReadMigrateBlock(prepared, info.blockIndex);
    if (const auto ret = EndBlockDevice(info, success, block); ret != UBSE_OK) {
        return ret;
    }
    PublishImportEnd(info, success, prepared, block);
    result.committed = success;
    result.block = std::move(resultBlock);
    CleanupMigrateDecoder(info, *block.local, success);
    return UBSE_OK;
}

uint32_t EndExportBlock(const UbseMemMigrateRpcInfo& info, bool success, const CheckedMigrateBlock& block,
                        const ConstExportObjPtr& exp, UbseMemMigrateOpResp& result)
{
    result.committed = block.committed;
    if (block.committed) {
        result.block = ReadMigrateBlock(*block.local, info.blockIndex);
        return UBSE_OK;
    }
    auto prepared = *block.local;
    auto effectiveExport = exp;
    const bool recoveryCommit = success && (!block.context || block.context->endDecision == true);
    if (recoveryCommit) {
        UbseMemMigrateBlockData actual;
        const auto ret = CommitAndReadRecoveryBlock(info, block, result, actual);
        if (ret != UBSE_OK || actual.decoder) {
            return ret == UBSE_OK ? UBSE_ERR_INTERNAL : ret;
        }
        auto restored = *exp;
        restored.status.exportObmmInfo[info.blockIndex] = actual.exportInfo;
        effectiveExport = std::make_shared<UbseMemShareBorrowExportObj>(std::move(restored));
    }
    if (success) {
        auto preparedExport = *effectiveExport;
        PrepareExportEnd(preparedExport, info.blockIndex, true);
        // 在副本上校验并准备 self-import 更新及最后一块的身份转换，设备成功后才发布。
        if (!CommitSelfImportBlock(prepared, preparedExport, info.blockIndex)) {
            return UBSE_ERR_INTERNAL;
        }
    } else {
        ApplyEndRollback(prepared, info.blockIndex);
    }
    PrepareEndContext(info, success, prepared);
    auto resultBlock = ReadMigrateBlock(prepared, info.blockIndex);
    // 恢复路径已完成设备提交；正常路径在准备好账本后执行end。
    if (!recoveryCommit) {
        if (const auto ret = EndBlockDevice(info, success, block); ret != UBSE_OK) {
            return ret;
        }
    }
    if (recoveryCommit) {
        UpdateMigrateExport(GetCurNodeId(), info.name, [&](auto& obj) {
            obj.status.exportObmmInfo[info.blockIndex] = effectiveExport->status.exportObmmInfo[info.blockIndex];
            return true;
        });
    }
    PublishExportEnd(info, success, prepared, effectiveExport);
    result.committed = success;
    result.block = std::move(resultBlock);
    CleanupMigrateDecoder(info, *block.local, success);
    return UBSE_OK;
}

// 块结果固定回到发起请求的 Master，发送重试不会改投新 Master。
template <typename Message>
uint32_t AgentSendMigrateMessage(const std::string& requestMasterNodeId, UbseMemBorrowCallbackOpCode opCode,
                                 const Message& data)
{
    auto comModule = UbseContext::GetInstance().GetModule<UbseComModule>();
    if (comModule == nullptr) {
        UBSE_LOG_ERROR << "Migrate result communication module unavailable, opCode=" << static_cast<uint16_t>(opCode);
        return UBSE_ERROR_NULLPTR;
    }
    ubse::simpo::util::UbseAutoSimpoPtr<Message> request = new (std::nothrow)
        ubse::simpo::util::UbseAutoSimpo<Message>();
    UbseBaseMessagePtr response = new (std::nothrow) UbseMemCallbackMessage();
    if (request == nullptr || response == nullptr) {
        UBSE_LOG_ERROR << "Migrate result allocation failed, opCode=" << static_cast<uint16_t>(opCode);
        return UBSE_ERROR_NULLPTR;
    }
    request->SetUbseMesgInfo(data);
    SendParam param(requestMasterNodeId, static_cast<uint16_t>(UbseModuleCode::UBSE_MEM_BORROW),
                    static_cast<uint16_t>(opCode));
    auto ret = UBSE_ERROR;
    // 最多上报5次，失败间隔1s，累计休眠最多5s（不含RPC调用耗时）。
    // Master普通阶段超时后会进入清理，额外长时间重发无意义
    const uint32_t maxRetryTimes = SEND_RETRY_TIMES;
    for (uint32_t attempt = 0; attempt < maxRetryTimes; ++attempt) {
        if (requestMasterNodeId.empty()) { // 普通新建导出沿用原有上报路由；块操作始终绑定请求发起 Master。
            std::string masterNodeId;
            ret = ubse::election::UbseGetMasterNodeId(masterNodeId);
            if (ret == UBSE_OK) {
                param.SetRemoteId(masterNodeId);
                ret = comModule->RpcSend(param, request, response);
            }
        } else {
            ret = comModule->RpcSend(param, request, response);
        }
        if (ret == UBSE_OK) {
            return ret;
        }
        UBSE_LOG_WARN << "Report migrate result failed, requestMasterNodeId=" << param.GetRemoteId()
                      << ", attempt=" << attempt + 1 << ", opCode=" << static_cast<uint16_t>(opCode)
                      << ", ret=" << FormatRetCode(ret);
        sleep(SEND_RETRY_DURATION);
    }
    return ret;
}

// 重复begin只返回已登记的当前块，不再次调用内核。
uint32_t ReadExistingBeginExport(const UbseMemMigrateRpcInfo& info, const CheckedMigrateBlock& block,
                                 const ConstExportObjPtr& existing, UbseMemMigrateBeginExportResp& result)
{
    const auto b = info.blockIndex;
    if (existing && b < existing->status.exportObmmInfo.size() && b < existing->migrateInfos.size() &&
        existing->migrateInfos[b] == info.migrateInfo && existing->status.exportObmmInfo[b].desc.addr != 0) {
        result.block = existing->status.exportObmmInfo[b];
        UBSE_LOG_WARN << "name=" << info.name << ", indexId=" << info.blockIndex
                      << ", operation=begin_export, Duplicate begin reuses export block, committed=" << block.committed
                      << ", nodeId=" << info.srcNodeId;
        return UBSE_OK;
    }
    UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << b
                   << ", operation=begin_export, Duplicate begin export missing target block";
    return UBSE_ERR_MIGRATE_NOT_ALLOWED;
}

// 公共准入已保证本地导入块有效；这里只校验导出模板及已有导出账本与本次请求的一致性。
uint32_t CheckBeginExportTemplate(const UbseMemMigrateBeginExportReq& req, const UbseMemShareBorrowImportObj& local,
                                  const ConstExportObjPtr& existing)
{
    const auto& info = req.rpcInfo;
    const auto b = info.blockIndex;
    const auto& target = req.exportObj;
    if (target.req.name != info.name || target.algoResult.exportNumaInfos.empty() ||
        target.algoResult.exportNumaInfos.front().nodeId != GetCurNodeId() || b >= target.migrateInfos.size() ||
        target.migrateInfos[b] != info.migrateInfo || local.status.importResults[b].memId != info.exportMemId ||
        (existing && (b >= existing->migrateInfos.size() || existing->migrateInfos[b] != info.migrateInfo))) {
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << b
                       << ", operation=begin_export, Invalid begin export template";
        return UBSE_ERROR_INVAL;
    }
    return UBSE_OK;
}

// 根据已校验的调度布局构造设备参数；失败收尾由调用方统一处理。
uint32_t BuildBeginExportDevice(const UbseMemMigrateBeginExportReq& req, const UbseMemShareBorrowImportObj& local,
                                UbseMemShmSwitchExportObj& device)
{
    const auto& info = req.rpcInfo;
    const auto b = info.blockIndex;
    const auto& target = req.exportObj;
    uint32_t localEid = 0;
    if (UbseNodeController::GetInstance().GetLocalEidBySocket(target.algoResult.exportNumaInfos.front().socketId,
                                                              localEid) != UBSE_OK) {
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << b
                       << ", operation=begin_export, Local EID lookup failed, socketId="
                       << target.algoResult.exportNumaInfos.front().socketId;
        return UBSE_ERR_INTERNAL;
    }
    device.importMemId = local.status.importResults[b].memId;
    device.blockIndex = b;
    device.exportObj = target;
    device.blockLength = local.exportObmmInfo[b].desc.length;
    FillEid16FromU32(device.deid, localEid);
    return UBSE_OK;
}

template <typename Response>
void ReportBlockResult(const UbseMemMigrateRpcInfo& info, const std::string& requestMasterNodeId,
                       UbseMemBorrowCallbackOpCode op, Response& result)
{
    result.rpcInfo = info;
    if (result.errCode != UBSE_OK) {
        UBSE_LOG_ERROR << "Agent migrate operation failed, name=" << info.name << ", indexId=" << info.blockIndex
                       << ", opCode=" << static_cast<uint16_t>(op) << ", ret=" << FormatRetCode(result.errCode);
    } else {
        UBSE_LOG_INFO << "Agent migrate operation completed, name=" << info.name << ", indexId=" << info.blockIndex
                      << ", opCode=" << static_cast<uint16_t>(op);
    }
    const auto ret = AgentSendMigrateMessage(requestMasterNodeId, op, result);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Report migrate result exhausted retries, name=" << info.name
                       << ", indexId=" << info.blockIndex << ", opCode=" << static_cast<uint16_t>(op)
                       << ", ret=" << FormatRetCode(ret);
    }
}
} // namespace
} // namespace migrate_detail

uint32_t UbseMemShmMigrateService::SwitchBeginImport(const UbseMemMigrateSwitchBeginImportReq& req,
                                                     const std::string& requestMasterNodeId,
                                                     UbseMemMigrateOpResp& result)
{
    const auto& info = req.rpcInfo;
    auto lock = LoggingLockGuard(MigrateBlockLockKey(info));
    UBSE_LOG_INFO << "name=" << info.name << ", indexId=" << info.blockIndex
                  << ", operation=begin_import, Migrate begin import request";
    CheckedMigrateBlock block;
    const auto& local = block.local;
    const auto ret = CheckMigrateRequest(info, requestMasterNodeId, AgentOperation::BEGIN_IMPORT, 1, block);
    if (ret != UBSE_OK) {
        return ret;
    }
    if (block.committed) {
        result.committed = true;
        result.block = ReadMigrateBlock(*local, info.blockIndex);
        UBSE_LOG_WARN << "name=" << info.name << ", indexId=" << info.blockIndex
                      << ", operation=begin_import, Duplicate begin reuses committed import block";
        return UBSE_OK;
    }
    if (block.context) {
        if (block.context->target && block.context->target->decoder &&
            local->exportObmmInfo[info.blockIndex].memIdStatus == UB_MEM_SWITCHING) {
            result.block = block.context->target;
            UBSE_LOG_WARN << "name=" << info.name << ", indexId=" << info.blockIndex
                          << ", operation=begin_import, Duplicate begin reuses pending decoder";
            return UBSE_OK;
        }
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << info.blockIndex
                       << ", operation=begin_import, Duplicate begin missing pending import data";
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    if (req.oldExportMemId != local->exportObmmInfo[info.blockIndex].memId || req.oldExportMemId == 0 ||
        req.targetBlock.memId != info.exportMemId || req.targetBlock.desc.addr == 0 ||
        req.targetBlock.desc.length != local->exportObmmInfo[info.blockIndex].desc.length ||
        req.targetLayout.exportNumaInfos.empty() || req.targetLayout.importNumaInfos.empty() ||
        info.blockIndex >= local->status.decoderResult.size() || req.trustRingData.lendSignedDatas.size() > 1) {
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << info.blockIndex
                       << ", operation=begin_import, Invalid begin import target block";
        return UBSE_ERROR_INVAL;
    }
    auto beginRet = BeginImport(req, *local, result);
    UBSE_LOG_INFO << "name=" << info.name << ", indexId=" << info.blockIndex
                  << ", operation=begin_import, Migrate begin import completed";
    return beginRet;
}

uint32_t UbseMemShmMigrateService::SwitchBeginExport(const UbseMemMigrateBeginExportReq& req,
                                                     const std::string& requestMasterNodeId,
                                                     UbseMemMigrateBeginExportResp& result)
{
    UBSE_LOG_INFO << "name=" << req.rpcInfo.name << ", indexId=" << req.rpcInfo.blockIndex
                  << ", operation=begin_export, Migrate begin export request";
    const auto& info = req.rpcInfo;
    const auto b = info.blockIndex;
    auto lock = LoggingLockGuard(MigrateBlockLockKey(info));
    CheckedMigrateBlock block;
    const auto& local = block.local;
    const auto check = CheckMigrateRequest(info, requestMasterNodeId, AgentOperation::BEGIN_EXPORT, 1, block);
    if (check != UBSE_OK) {
        return check;
    }
    auto& exports = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>();
    const auto existing = exports.GetResource(GetCurNodeId(), info.name);
    if (block.committed || block.context) {
        return ReadExistingBeginExport(info, block, existing, result);
    }
    if (const auto ret = CheckBeginExportTemplate(req, *local, existing); ret != UBSE_OK) {
        return ret;
    }
    if (!existing) {
        exports.PutResource(GetCurNodeId(), info.name, req.exportObj);
    }
    auto context = MakeSwitchContext(info);
    UbseMemShmSwitchExportObj device{};
    if (const auto ret = BuildBeginExportDevice(req, *local, device); ret != UBSE_OK) {
        return CloseFailedOperation(info, context, ret);
    }
    if (!SaveContext(info, context, UBSE_OK, true)) {
        return UBSE_ERR_INTERNAL;
    }
    ubse_mem_obmm_mem_desc desc{};
    const auto ret = UbseMmiInterface::GetInstance().ShmSwitchBeginExportExecutor(device, desc);
    if (ret != UBSE_OK) {
        RollbackPreparedSwitching(info);
        return CloseFailedOperation(info, context, ret);
    }
    if (desc.addr == 0) {
        UBSE_LOG_ERROR << "Migrate begin_export returned empty address, name=" << info.name << ", indexId=" << b;
        RollbackPreparedSwitching(info);
        return CloseFailedOperation(info, context, UBSE_ERROR_INVAL);
    }
    result.block = {device.importMemId, desc, UB_MEM_SWITCHING};
    if (!UpdateMigrateExport(GetCurNodeId(), info.name, [&](auto& obj) {
            obj.status.exportObmmInfo.resize(b + 1);
            obj.status.exportObmmInfo[b] = result.block;
            return true;
        })) {
        return CloseFailedOperation(info, context, UBSE_ERR_INTERNAL);
    }
    UBSE_LOG_INFO << "name=" << info.name << ", indexId=" << info.blockIndex
                  << ", operation=begin_export, Migrate begin export completed";
    return UBSE_OK;
}

uint32_t UbseMemShmMigrateService::DataCopy(const UbseMemMigrateDataCopyReq& req,
                                            const std::string& requestMasterNodeId)
{
    const auto& info = req.rpcInfo;
    auto lock = LoggingLockGuard(MigrateBlockLockKey(info));
    CheckedMigrateBlock block;
    const auto& local = block.local;
    const auto check = CheckMigrateRequest(info, requestMasterNodeId, AgentOperation::COPY, 1, block);
    if (check != UBSE_OK) {
        return check;
    }
    // 不缓存 copy 结果；每次有效请求都重新映射并实际拷贝。
    const auto ret = UbseMmiInterface::GetInstance().ShmMigrateCopyExecutor(
        local->status.importResults[info.blockIndex].memId, local->exportObmmInfo[info.blockIndex].desc.length,
        local->req.ubseMemPrivData.cacheableFlag != 0);
    if (ret != UBSE_OK) {
        auto context = block.context ? *block.context : MakeSwitchContext(info);
        return CloseFailedOperation(info, context, ret);
    }
    return UBSE_OK;
}

uint32_t UbseMemShmMigrateService::SwitchEndImport(const UbseMemMigrateSwitchEndImportReq& req,
                                                   const std::string& requestMasterNodeId, UbseMemMigrateOpResp& result)
{
    const auto& info = req.rpcInfo;
    auto lock = LoggingLockGuard(MigrateBlockLockKey(info));
    CheckedMigrateBlock block;
    const auto& local = block.local;
    const auto ret = CheckMigrateRequest(info, requestMasterNodeId, AgentOperation::END_IMPORT, req.success, block);
    // 回包保留已确认的设备提交事实；block.committed 仍只表示正式账本已提交。
    result.committed = block.IsCommitConfirmed();
    if (ret != UBSE_OK) {
        if (block.committed) {
            result.block = ReadMigrateBlock(*local, info.blockIndex);
        }
        return ret;
    }
    const bool hasTarget = block.context && block.context->target.has_value();
    if (info.blockIndex >= local->status.decoderResult.size() && hasTarget) {
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << info.blockIndex
                       << ", operation=end_import, End import missing decoder slot";
        return UBSE_ERR_INTERNAL;
    }
    if (!block.committed && req.success != 0 && hasTarget && !block.context->target->decoder) {
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << info.blockIndex
                       << ", operation=end_import, Commit missing pending resources";
        SaveContext(info, *block.context, UBSE_ERR_INTERNAL);
        return UBSE_ERR_INTERNAL;
    }
    return EndImportBlock(info, req.success != 0, block, result);
}

// end_export 账本身份校验：在 name+blockIndex 锁内逐项核对本地导入结果与账本导出快照，
// 任一项失败时打印对应日志并返回 false。exp 为空仅允许回滚（success == 0）路径。
bool CheckExportLedgerIdentity(const UbseMemMigrateRpcInfo& info, const ConstImportObjPtr& local, uint32_t success,
                               const ConstExportObjPtr& exp)
{
    const auto b = info.blockIndex;
    if (local->status.importResults[b].memId != info.exportMemId) {
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << b
                       << ", operation=end_export, End export import result memId mismatch, expectMemId="
                       << info.exportMemId << ", actualMemId=" << local->status.importResults[b].memId;
        return false;
    }
    if (success == 1 && !exp) {
        // end(1)时, 导出账本不能为空
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << b
                       << ", operation=end_export, End export success without ledger resource";
        return false;
    }
    if (exp && b >= exp->migrateInfos.size() && success != 0) {
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << b
                       << ", ledgerMigrateInfoCount=" << exp->migrateInfos.size()
                       << ", operation=end_export, End export ledger migrate info index out of range";
        return false;
    }
    if (exp && b < exp->migrateInfos.size() && exp->migrateInfos[b] != info.migrateInfo) {
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << b
                       << ", operation=end_export, End export ledger migrate info mismatch";
        return false;
    }
    if (exp && b < exp->status.exportObmmInfo.size() && exp->status.exportObmmInfo[b].memId != info.exportMemId) {
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << b
                       << ", operation=end_export, End export ledger obmm info memId mismatch, expectMemId="
                       << info.exportMemId << ", actualMemId=" << exp->status.exportObmmInfo[b].memId;
        return false;
    }
    if (exp && b >= exp->status.exportObmmInfo.size() && success != 0) {
        UBSE_LOG_ERROR << "name=" << info.name << ", indexId=" << b
                       << ", ledgerObmmInfoCount=" << exp->status.exportObmmInfo.size()
                       << ", operation=end_export, End export success without ledger obmm info";
        return false;
    }
    return true;
}

uint32_t UbseMemShmMigrateService::SwitchEndExport(const UbseMemMigrateEndExportReq& req,
                                                   const std::string& requestMasterNodeId, UbseMemMigrateOpResp& result)
{
    const auto& info = req.rpcInfo;
    auto lock = LoggingLockGuard(MigrateBlockLockKey(info));
    CheckedMigrateBlock block;
    const auto& local = block.local;
    const auto ret = CheckMigrateRequest(info, requestMasterNodeId, AgentOperation::END_EXPORT, req.success, block);
    result.committed = block.IsCommitConfirmed();
    if (ret != UBSE_OK) {
        if (block.committed) {
            result.block = ReadMigrateBlock(*local, info.blockIndex);
        }
        return ret;
    }
    const auto exp = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(
        GetCurNodeId(), info.name);
    if (!CheckExportLedgerIdentity(info, local, req.success, exp)) {
        return UBSE_ERR_MIGRATE_NOT_ALLOWED;
    }
    return EndExportBlock(info, req.success != 0, block, exp, result);
}

void UbseMemShmMigrateService::Export(const UbseMemShareBorrowExportObj& obj, UbseMemShareBorrowExportObj& rsp)
{
    rsp = obj;
    auto lock = LoggingLockGuard(obj.req.name);
    const auto local = UbseMemDebtLedger::GetInstance().GetDebtMap<UbseMemShareBorrowExportObj>().GetResource(
        GetCurNodeId(), obj.req.name);
    if (local && (local->status.state == UBSE_MEM_EXPORT_SUCCESS || local->status.state == UBSE_MEM_EXPORT_DESTROYED)) {
        rsp = *local;
        // 复用实际设备结果，回包关联本次请求；不改本地账本中的创建记录。
        rsp.req.requestId = obj.req.requestId;
        rsp.req.requestNodeId = obj.req.requestNodeId;
        return;
    }
    rsp.status.errCode = ShareExportExecute(rsp, GetCurNodeId());
}

void UbseMemShmMigrateService::ProcessExportRequest(const UbseMemShareBorrowExportObj& req)
{
    UBSE_LOG_INFO << "Agent executing migrate request, name=" << req.req.name
                  << ", operation=new_export, localNodeId=" << GetCurNodeId() << ", " << MigrateExportLog(req);
    UbseMemShareBorrowExportObj result{};
    Export(req, result);
    UBSE_LOG_INFO << "Agent migrate operation completed, name=" << result.req.name
                  << ", operation=new_export, localNodeId=" << GetCurNodeId() << ", state=" << result.status.state
                  << ", resultRet=" << FormatRetCode(result.status.errCode);
    const auto ret =
        AgentSendMigrateMessage("", UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_NEW_EXPORT_RESULT, result);
    if (ret != UBSE_OK) {
        UBSE_LOG_ERROR << "Report export result failed, name=" << req.req.name << ", ret=" << FormatRetCode(ret);
    }
}

void UbseMemShmMigrateService::ProcessSwitchBeginImportRequest(const UbseMemMigrateSwitchBeginImportReq& req,
                                                               const std::string& requestMasterNodeId)
{
    UbseMemMigrateOpResp result{};
    result.errCode = SwitchBeginImport(req, requestMasterNodeId, result);
    ReportBlockResult(req.rpcInfo, requestMasterNodeId,
                      UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_SWITCH_BEGIN_IMPORT_RESULT, result);
}

void UbseMemShmMigrateService::ProcessSwitchBeginExportRequest(const UbseMemMigrateBeginExportReq& req,
                                                               const std::string& requestMasterNodeId)
{
    UbseMemMigrateBeginExportResp result{};
    result.errCode = SwitchBeginExport(req, requestMasterNodeId, result);
    ReportBlockResult(req.rpcInfo, requestMasterNodeId,
                      UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_SWITCH_BEGIN_EXPORT_RESULT, result);
}

void UbseMemShmMigrateService::ProcessCopyRequest(const UbseMemMigrateDataCopyReq& req,
                                                  const std::string& requestMasterNodeId)
{
    UbseMemMigrateOpResp result{};
    result.errCode = DataCopy(req, requestMasterNodeId);
    ReportBlockResult(req.rpcInfo, requestMasterNodeId, UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_COPY_RESULT,
                      result);
}

void UbseMemShmMigrateService::ProcessSwitchEndImportRequest(const UbseMemMigrateSwitchEndImportReq& req,
                                                             const std::string& requestMasterNodeId)
{
    UbseMemMigrateOpResp result{};
    result.errCode = SwitchEndImport(req, requestMasterNodeId, result);
    ReportBlockResult(req.rpcInfo, requestMasterNodeId, UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_END_RESULT,
                      result);
}

void UbseMemShmMigrateService::ProcessSwitchEndExportRequest(const UbseMemMigrateEndExportReq& req,
                                                             const std::string& requestMasterNodeId)
{
    UbseMemMigrateOpResp result{};
    result.errCode = SwitchEndExport(req, requestMasterNodeId, result);
    ReportBlockResult(req.rpcInfo, requestMasterNodeId, UbseMemBorrowCallbackOpCode::UBSE_MEM_SHM_MIGRATE_END_RESULT,
                      result);
}
} // namespace ubse::mem::controller
