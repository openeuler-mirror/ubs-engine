/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 
 * UBS RMRS is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "over_commit_ucache_strategy.h"

#include "mempooling_message.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "rmrs_serialize.h"
#include "ubse_com.h"
#include "ubse_logger.h"

namespace mempooling::over_commit {
using namespace ubse::log;
using namespace ubse::com;
using namespace mempooling::message;
using namespace rmrs;
using namespace rmrs::serialize;

static std::map<std::string, float> g_ucacheMemRaitoMap{}; // NodeId -> UCache Mem Ratio
static float g_localUCacheMemRaito{};                      // local UCache Mem Ratio

namespace {
constexpr float MAX_UCACHE_RATIO = 0.5;
constexpr float MIN_UCACHE_RATIO = 0.0;
} // namespace

static void FreeMemory(uint8_t *data)
{
    delete[] data;
}

void GenUCacheMigrationStrategy(int16_t localNumaId, const std::vector<pid_t> &pids,
                                const std::vector<uint16_t> &remoteNumaIds, const std::string &nodeId,
                                UCacheMigrationStrategyParam &ucacheStrategy)
{
    ucacheStrategy.localNumaId = localNumaId;
    ucacheStrategy.ucacheUsageRatio = GetUcacheUsageRatio(nodeId);
    for (const auto pid : pids) {
        ucacheStrategy.pids.push_back(pid);
    }
    for (const auto id : remoteNumaIds) {
        ucacheStrategy.remoteNumaIds.push_back(id);
    }
    if (ucacheStrategy.pids.empty() || ucacheStrategy.remoteNumaIds.empty()) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[Mem_migrate][ucache] ucacheMigrationStrategy is empty";
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[Mem_migrate][ucache] UCacheMigrationStrategy is :" << ucacheStrategy.ToString();
}

uint32_t SendUCacheMigrationStrategy(UCacheMigrationStrategyParam &ucacheStrategy, const std::string &srcNid)
{
    uint32_t result = MEM_POOLING_OK;
    UbseComEndpoint endpoint = {
        .moduleId = MP_MODULE_CODE, .serviceId = OPCODE_OVER_COMMIT_UCACHE_SEND_MIGRTIAON_STRATEGY, .address = srcNid};
    RmrsOutStream builder;
    builder << ucacheStrategy;
    UbseByteBuffer reqData = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    UbseRpcSend(endpoint, reqData, &result, UCacheSendMigrationStrategyResHandler);
    if (result != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Mem_migrate][ucache] Send UCache MigrationStrategy failed, result:" << result;
    }
    delete[] reqData.data;
    return result;
}

void CheckAndStopUCacheMigration(const std::string &srcNid)
{
    uint32_t result = MEM_POOLING_OK;
    // 实际无需入参
    ResCode blank{};
    RmrsOutStream builder;
    builder << blank;
    UbseComEndpoint endpoint = {
        .moduleId = MP_MODULE_CODE, .serviceId = OPCODE_OVER_COMMIT_UCACHE_STOP_MIGRTIAON, .address = srcNid};
    UbseByteBuffer reqData = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    UbseRpcSend(endpoint, reqData, &result, UCacheStopMigrationResHandler);
    delete[] reqData.data;
    if (result != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Mem_migrate][ucache] Send CheckAndStopUCacheMigration failed, result:" << result;
    }
}

void UpdateUcacheUsageRatio(const std::vector<pid_t> &pids, uint64_t borrowMemKB, const std::string &nodeId)
{
    UbseComEndpoint endpoint = {
        .moduleId = MP_MODULE_CODE, .serviceId = OPCODE_OVER_COMMIT_UPDATE_UCACHE_RATIO, .address = nodeId};
    MigrationInfoParam param{};
    param.pids = pids;
    param.borrowMemKB = borrowMemKB;
    RmrsOutStream builder;
    builder << param;
    UCacheRatioRes res{};
    UbseByteBuffer reqData = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    UbseRpcSend(endpoint, reqData, &res, UpdateUCacheRatioResHandler);
    delete[] reqData.data;
    if (res.resCode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Mem_migrate][ucache] Send UpdateUcacheUsageRatio failed, res=" << res.resCode
            << ". UcacheUsageRatio=0, nodeId=" << nodeId << ".";
        g_ucacheMemRaitoMap[nodeId] = 0;
        return;
    }
    g_ucacheMemRaitoMap[nodeId] = res.ucacheUsageRatio;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[Mem_migrate][ucache] UpdateUcacheUsageRatio succeed, UcacheUsageRatio=" << res.ucacheUsageRatio
        << ", nodeId=" << nodeId << ".";
    return;
}

float GetUcacheUsageRatio(const std::string &nodeId)
{
    if (!MpConfiguration::GetInstance().GetUcacheEnable()) {
        return 0.0;
    }
    if (g_ucacheMemRaitoMap.find(nodeId) != g_ucacheMemRaitoMap.end()) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Mem_migrate][ucache] GetUcacheUsageRatio succeed, nodeId=" << nodeId
            << ", ratio=" << g_ucacheMemRaitoMap[nodeId] << ".";
        return g_ucacheMemRaitoMap[nodeId];
    }
    UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[Mem_migrate][ucache] GetUcacheUsageRatio failed, nodeId=" << nodeId << ".";
    return 0.0;
}

void UpdateLocalUcacheUsageRatio(float ratio)
{
    if (ratio >= MIN_UCACHE_RATIO && ratio <= MAX_UCACHE_RATIO) {
        g_localUCacheMemRaito = ratio;
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Mem_migrate][ucache] UpdateLocalUcacheUsageRatio succeed, g_localUCacheMemRaito="
            << g_localUCacheMemRaito << ".";
        return;
    }
    UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[Mem_migrate][ucache] UpdateLocalUcacheUsageRatio failed, "
                                                        "ratio is invalid, ratio="
                                                     << ratio << ". Set g_localUCacheMemRaito to default value 0.";
    g_localUCacheMemRaito = 0;
    return;
}

float GetLocalUcacheUsageRatio()
{
    if (!MpConfiguration::GetInstance().GetUcacheEnable()) {
        return 0.0;
    }
    if (g_localUCacheMemRaito >= MIN_UCACHE_RATIO && g_localUCacheMemRaito <= MAX_UCACHE_RATIO) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[Mem_migrate][ucache] GetLocalUcacheUsageRatio succeed, g_localUCacheMemRaito=" << g_localUCacheMemRaito
            << ".";
        return g_localUCacheMemRaito;
    }
    UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[Mem_migrate][ucache] GetLocalUcacheUsageRatio failed, "
                                                        "g_localUCacheMemRaito is invalid, g_localUCacheMemRaito="
                                                     << g_localUCacheMemRaito << ". Set to default value 0.";
    return 0.0;
}

uint32_t InitUCacheOverCommitReg()
{
    UbseComEndpoint endpoint;
    uint32_t ret;

    // UCache融合超分场景，UCache迁移策略执行
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_OVER_COMMIT_UCACHE_SEND_MIGRTIAON_STRATEGY};
    ret = UbseRegRpcService(endpoint, UCacheSendMigrationStrategyRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[UCache] OverCommitMsgHandler UCacheSendMigrationStrategyRecvHandler reg failed res: " << ret;
        return ret;
    }

    // UCache融合超分场景，UCache迁移停止
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_OVER_COMMIT_UCACHE_STOP_MIGRTIAON};
    ret = UbseRegRpcService(endpoint, UCacheStopMigrationRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[UCache] OverCommitMsgHandler UCacheStopMigrationRecvHandler reg failed res: " << ret;
        return ret;
    }

    // UCache融合超分场景，UCache迁移比例获取
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_OVER_COMMIT_UPDATE_UCACHE_RATIO};
    ret = UbseRegRpcService(endpoint, UpdateUCacheRatioRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[UCache] OverCommitMsgHandler UpdateUCacheRatioRecvHandler reg failed res: " << ret;
        return ret;
    }

    return MEM_POOLING_OK;
}

bool ValidateBuffer(const UbseByteBuffer &req)
{
    if (req.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Empty request buffer!";
        return false;
    }
    if (req.data != nullptr && req.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Invalid request buffer!";
        return false;
    }
    return true;
}

void UCacheSendMigrationStrategyRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[over-commit][ucache] Call turbo UCacheMigrateStrategy start.";
    if (!ValidateBuffer(req)) {
        return;
    }
    UCacheMigrationStrategyParam param;
    ResCode result;
    RmrsInStream builderIn(req.data, req.len);
    builderIn >> param;
    if (!builderIn.Check()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[over-commit][ucache] Failed to deserialize UCacheMigrationStrategyParam.";
        return;
    }
    uint32_t ret = MempoolingMessage::rmrsUCacheMigrateStrategy(param, result);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][ucache] UBTurboRMRSAgentUCacheMigrateStrategy ipc send error, ret=" << ret << ".";
    }
    RmrsOutStream builderOut;
    builderOut << result;
    resp.data = builderOut.GetBufferPointer();
    resp.len = builderOut.GetSize();
    resp.freeFunc = FreeMemory;
}

void UCacheStopMigrationRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[over-commit][ucache] Call turbo UCacheStopMigration start.";
    ResCode result;
    uint32_t ret = MempoolingMessage::rmrsUCacheMigrateStop(result);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][ucache] UBTurboRMRSAgentUCacheMigrateStop ipc send error, ret=" << ret << ".";
    }
    RmrsOutStream builderOut;
    builderOut << result;
    resp.data = builderOut.GetBufferPointer();
    resp.len = builderOut.GetSize();
    resp.freeFunc = FreeMemory;
}

void UCacheSendMigrationStrategyResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[over-commit][ucache] Ctx or respData is null.";
        return;
    }
    if (resCode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[over-commit][ucache] UCacheSendMigrationStrategy send error " << resCode;
        return;
    }
    if (!ValidateBuffer(respData)) {
        return;
    }
    ResCode result{};
    RmrsInStream builderIn(respData.data, respData.len);
    if (!builderIn.Check()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[over-commit][ucache] Failed to deserialize respData to ResCode.";
        return;
    }
    builderIn >> result;
    ResCode *ucacheSendMigrationStrategyResult = static_cast<ResCode *>(ctx);
    *ucacheSendMigrationStrategyResult = result;
}

void UCacheStopMigrationResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][ucache] Ctx or respData is null.";
        return;
    }
    if (resCode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[over-commit][ucache] UCacheStopMigration send error " << resCode;
        return;
    }
    if (!ValidateBuffer(respData)) {
        return;
    }
    ResCode result{};
    RmrsInStream builderIn(respData.data, respData.len);
    builderIn >> result;
    if (!builderIn.Check()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[over-commit][ucache] Failed to deserialize respData to ResCode.";
        return;
    }
    ResCode *ucacheStopMigrationResult = static_cast<ResCode *>(ctx);
    *ucacheStopMigrationResult = result;
}

void UpdateUCacheRatioRecvHandler(const UbseByteBuffer &req, UbseByteBuffer &resp)
{
    if (!ValidateBuffer(req)) {
        return;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[over-commit][ucache] Call turbo UpdateUCacheRatioRecvHandler start.";
    MigrationInfoParam param;
    UCacheRatioRes result;
    RmrsInStream builderIn(req.data, req.len);
    builderIn >> param;
    if (!builderIn.Check()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[over-commit][ucache] Failed to deserialize reqdata to MigrationInfoParam.";
        return;
    }
    uint32_t ret = MempoolingMessage::rmrsUpdateUCacheRatio(param, result);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][ucache] UBTurboRMRSAgentUpdateUCacheRatio ipc send error, ret=" << ret << ".";
    }
    RmrsOutStream builderOut;
    builderOut << result;
    resp.data = builderOut.GetBufferPointer();
    resp.len = builderOut.GetSize();
    resp.freeFunc = FreeMemory;
    over_commit::UpdateLocalUcacheUsageRatio(result.ucacheUsageRatio);
}

void UpdateUCacheRatioResHandler(void *ctx, const UbseByteBuffer &respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][ucache] Ctx or respData is null.";
        return;
    }
    if (resCode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OverCommit][ucache] UpdateUCacheRatio send error " << resCode;
        return;
    }
    if (!ValidateBuffer(respData)) {
        return;
    }
    UCacheRatioRes result{};
    RmrsInStream builderIn(respData.data, respData.len);
    builderIn >> result;
    if (!builderIn.Check()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[over-commit][ucache] Failed to deserialize respData to UCacheRatioRes.";
        return;
    }
    UCacheRatioRes *updateUCacheRatioResult = static_cast<UCacheRatioRes *>(ctx);
    *updateUCacheRatioResult = result;
}

} // namespace mempooling::over_commit