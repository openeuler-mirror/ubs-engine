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

#include "over_commit_msg_handler.h"

#include <climits>

#include "ubse_logger.h"
#include "mempooling_message.h"
#include "mp_configuration.h"
#include "mp_json_util.h"
#include "mp_smap_controller.h"
#include "mp_smap_helper.h"
#include "numa_info.h"
#include "over_commit_fault_management_handler.h"
#include "over_commit_mem_migrate_trans_msg.h"
#include "over_commit_msg.h"
#include "over_commit_serializer.h"
#include "page_file_helper.h"
#include "set_smap_remote_numa_info_trans_msg.h"
#include "smap_remote_process_query_trans_msg.h"
#include "smap_remove_trans_msg.h"
#include "vm_mem_migrate_strategy.h"

namespace mempooling::over_commit {
using namespace ubse::log;
using namespace ubse::com;
using namespace mempooling::smap;
using namespace turbo::rmrs;
const int SMAP_QUERY_PID_NUM = 40;
constexpr int SMAP_OK = 0;
constexpr int SMAP_ERROR = 1;

void LogMigrationDetails(const std::vector<MemBorrowInfoWithSrc>& memBorrowInfoWithSrcs,
                         const std::vector<MemMigrateResult>& memMigrateResults)
{
    std::ostringstream oss;
    for (const auto& memBorrowInfo : memBorrowInfoWithSrcs) {
        oss << memBorrowInfo.ToString() << R"(,)";
    }
    std::ostringstream oss2;
    for (const auto& memMigrateResult : memMigrateResults) {
        oss2 << memMigrateResult.ToString() << R"(,)";
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MsgHandler][Migrate]"
        << " memBorrowInfoWithSrcs=" << oss.str() << ", memMigrateResults=" << oss2.str() << ".";
}

MpResult OverCommitMsgHandler::MigrateLocalHandler(const outinterface::SrcMemoryBorrowParam& srcMemoryBorrowParam,
                                                   const std::vector<MemBorrowInfoWithSrc> memBorrowInfoWithSrcs,
                                                   const std::vector<MemMigrateResult>& memMigrateResults)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] MigrateLocalHandler start.";
    LogMigrationDetails(memBorrowInfoWithSrcs, memMigrateResults);
    if (memMigrateResults.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler]  MemMigrateResults is empty.";
        return MEM_POOLING_ERROR;
    }
    VMMemMigrateStrategy s;
    std::vector<pid_t> pids;
    for (auto memRes : memMigrateResults) {
        (void)pids.emplace_back(memRes.pid);
    }
    bool needRebalance = false;
    if (memBorrowInfoWithSrcs.empty()) {
        needRebalance = true;
    }
    auto ret = MEM_POOLING_OK;
    if (needRebalance) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[Rebalance] Start Rebalance.";
        // 关闭冷热页流动
        int retSmap = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 0, 0);
        if (MEM_POOLING_OK != static_cast<MpResult>(retSmap)) {
            return MEM_POOLING_ERROR;
        }
        ret = s.Rebalance(srcMemoryBorrowParam.srcNid, srcMemoryBorrowParam.srcNumaId, pids,
                          memMigrateResults.front().maxRatio);
        // 打开冷热页流动
        retSmap = MpSmapHelper::SmapEnableProcessMigrateHelper(pids.data(), pids.size(), 1, 0);
        if (MEM_POOLING_OK != static_cast<MpResult>(retSmap)) {
            return MEM_POOLING_ERROR;
        }
    } else {
        ret = NormMigrate(memMigrateResults, memBorrowInfoWithSrcs, srcMemoryBorrowParam.srcNumaId);
    }
    if (ret == MEM_POOLING_SMAP_PARTIAL_SUCCESS) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] Migrate pids partial failed.";
        return ret;
    }
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] MigrateLocalHandler failed.";
        return ret;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] MigrateLocalHandler Success.";
    return MEM_POOLING_OK;
}

MpResult OverCommitMsgHandler::NormMigrate(const std::vector<MemMigrateResult>& memMigrateResults,
                                           const std::vector<MemBorrowInfoWithSrc>& memBorrowInfoWithSrcs,
                                           int16_t srcNumaId)
{
    auto ret = MEM_POOLING_OK;

    // 修改大页
    if (MpConfiguration::GetInstance().GetPageType() == PageType::PAGE_2M) {
        ret = PageFileHelper::AllocateHugePages(memBorrowInfoWithSrcs);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] Failed to AllocateHugePages.";
            return MEM_POOLING_ERROR;
        }
    }

    // 限制借用大小
    ret = MpSmapHelper::SetSmapRemoteNumaInfo(srcNumaId, memBorrowInfoWithSrcs);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] Failed to SetSmapRemoteNumaInfo.";
        return MEM_POOLING_ERROR;
    }

    // 检查removePid和smapEnable数据库
    CheckAndExecutePersistence();

    // 冷数据迁移
    auto ratio = memMigrateResults.front().maxRatio;
    ret = MpSmapHelper::MigrateOutInOverCommit(memMigrateResults, ratio);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] Failed to MigrateOutInOverCommit.";
        return ret;
    }
    return ret;
}

void OverCommitMsgHandler::CheckAndExecutePersistence()
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] Start to CheckAndExecutePersistence.";
    // 检查SmapEnable持久化数据库，若存在待enable的numaId，则执行SmapEnable
    CheckAndExecuteSmapEnable();

    // 检查RemovePids持久化数据库，若存在待remove的pid，则执行RemovePids
    CheckAndExecuteRemovePid();
}

void OverCommitMsgHandler::CheckAndExecuteSmapEnable()
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] Start to CheckAndExecuteSmapEnable.";
    // 检查smapEnable数据库
    std::vector<int16_t> smapEnableCompletedList;
    auto ret = SmapEnableCompleted::Instance().Query(smapEnableCompletedList);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] Failed to Query smapEnableCompletedList.";
        return;
    }
    if (smapEnableCompletedList.empty()) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MsgHandler] smapEnableCompletedList is empty, no need to execute SmapEnable.";
        return;
    }

    // 执行smapEnable
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[PluginInit][SmapEnableCompleted] smapEnableCompletedList.size=" << smapEnableCompletedList.size()
        << ", Start to execute SmapEnable.";
    int successCount = 0;
    for (auto& numaId : smapEnableCompletedList) {
        EnableNodeMsg enableMsg;
        enableMsg.nid = static_cast<int>(numaId);
        enableMsg.enable = SMAP_ENABLE_NUMA;
        ret = SmapEnableNumaProcess(enableMsg);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[PluginInit][SmapEnableCompleted] SmapEnableNumaProcess failed, numaId = " << numaId << ".";
            continue;
        }
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[PluginInit][SmapEnableCompleted] SmapEnableNumaProcess success, numaId = " << numaId
            << ", Start to Remove this numaId.";
        ret = SmapEnableCompleted::Instance().Remove(numaId);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[PluginInit][SmapEnableCompleted] Remove failed, numaId = " << numaId << ".";
            continue;
        }
        successCount++;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[PluginInit][SmapEnableCompleted] SmapEnableNumaProcess finished, totalCount = "
        << smapEnableCompletedList.size() << ", successCount = " << successCount << ".";
}

void OverCommitMsgHandler::CheckAndExecuteRemovePid()
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] Start to CheckAndExecuteRemovePid.";
    // 检查removePid数据库
    std::unordered_map<uint16_t, std::unordered_set<pid_t>> removePidCompletedList;
    auto ret = RemovePidCompleted::Instance().Query(removePidCompletedList);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] Failed to Query removePidList.";
        return;
    }
    if (removePidCompletedList.empty()) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MsgHandler] removePidCompletedList is empty, no need to execute RemovePids.";
        return;
    }

    // 执行removePids
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MsgHandler] removePidCompletedList.size = " << removePidCompletedList.size() << ".";
    int successCount = 0;
    for (const auto& [numaId, pids] : removePidCompletedList) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MsgHandler] Start to RemovePids, numaId = " << numaId << ", pids.size = " << pids.size() << ".";
        std::vector<pid_t> pidsVec(pids.begin(), pids.end());
        ret = RemoveLocalHandler(numaId, pidsVec);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MsgHandler] Failed to RemovePids, numaId = " << numaId << ".";
            continue;
        }
        ret = RemovePidCompleted::Instance().Remove(numaId, pidsVec);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MsgHandler] Failed to RemovePid, numaId = " << numaId << ".";
            continue;
        }
        successCount++;
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MsgHandler] RemovePid success, numaId = " << numaId << ".";
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MsgHandler] CheckAndExecuteRemovePids finished, totalCount = " << removePidCompletedList.size()
        << ", successCount = " << successCount << ".";
}

MpResult OverCommitMsgHandler::SetSmapRemoteNumaHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] SetSmapRemoteNumaHandler start.";
    if (req.data == nullptr || req.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] SetSmapRemoteNumaHandler req.data is null.";
        return MEM_POOLING_ERROR;
    }
    ResponseInfoSimpo response;
    SetSmapRemoteNumaInfoTransMsg setSmapRemoteNumaInfoTransMsg{};
    RmrsInStream builder(req.data, req.len);
    builder >> setSmapRemoteNumaInfoTransMsg;
    const auto [srcMemoryBorrowParam, memBorrowInfos] = setSmapRemoteNumaInfoTransMsg.GetSetSmapRemoteNumaInfoTrans();

    const auto SetSmapRemoteNumaInfo = SmapModule::GetSetSmapRemoteNumaInfo();
    if (SetSmapRemoteNumaInfo == nullptr) {
        SetResponse(response, MEM_POOLING_ERROR, "SetSmapRemoteNumaHandler failed.", resp);
        return MEM_POOLING_ERROR;
    }

    for (const auto& [presentNumaIds, borrowSize] : memBorrowInfos) {
        RemoteNumaInfo remoteNumaInfo{
            .srcNid = srcMemoryBorrowParam.srcNumaId, .destNid = presentNumaIds, .size = borrowSize >> KB2MB};
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MsgHandler] setSmapRemoteNumaInfoMsg=" << remoteNumaInfo.ToString() << ".";
        auto ret = SetSmapRemoteNumaInfo(&remoteNumaInfo);
        if (ret != SMAP_OK) {
            SetResponse(response, ret, "SetSmapRemoteNumaHandler failed.", resp);
            return MEM_POOLING_ERROR;
        } else {
            SetResponse(response, MEM_POOLING_OK, "SetSmapRemoteNumaHandler success.", resp);
        }
    }
    return MEM_POOLING_OK;
}

MpResult OverCommitMsgHandler::SetSmapRemoteNumaLocalHandler(const outinterface::SrcMemoryBorrowParam& srcParam,
                                                             const std::vector<MemBorrowInfo>& memBorrowInfos)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] SetSmapRemoteNumaLocalHandler start.";
    const auto SetSmapRemoteNumaInfo = SmapModule::GetSetSmapRemoteNumaInfo();
    if (SetSmapRemoteNumaInfo == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] GetSetSmapRemoteNumaInfo failed.";
        return MEM_POOLING_ERROR;
    }

    for (const auto& [presentNumaIds, borrowSize] : memBorrowInfos) {
        RemoteNumaInfo remoteNumaInfo{
            .srcNid = srcParam.srcNumaId, .destNid = presentNumaIds, .size = borrowSize >> KB2MB};
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MsgHandler] setSmapRemoteNumaInfoMsg=" << remoteNumaInfo.ToString() << ".";
        auto ret = SetSmapRemoteNumaInfo(&remoteNumaInfo);
        if (ret != SMAP_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MsgHandler] SetSmapRemoteNumaInfo failed. ret=" << ret << ".";
            return MEM_POOLING_ERROR;
        } else {
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] SetSmapRemoteNumaInfo success.";
        }
    }
    return MEM_POOLING_OK;
}

MpResult OverCommitMsgHandler::RemoveHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] RemoveHandler start.";
    if (req.data == nullptr || req.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] RemoveHandler req.data is null.";
        return MEM_POOLING_ERROR;
    }
    ResponseInfoSimpo response;
    SmapRemoveTransMsg smapRemoveTransMsg{};
    RmrsInStream builder(req.data, req.len);
    builder >> smapRemoveTransMsg;
    const auto [pids] = smapRemoveTransMsg.GetSmapRemoveTrans();

    const auto smapRemove = SmapModule::GetSmapRemoveFunc();
    if (smapRemove == nullptr) {
        SetResponse(response, MEM_POOLING_ERROR, "SmapRemove failed.", resp);
        return MEM_POOLING_ERROR;
    }
    RemoveMsg removeMsg{};
    if (pids.size() > MAX_NR_REMOVE_MP) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MsgHandler] Pids size exceeds the limit, size=" << pids.size() << ".";
        return MEM_POOLING_ERROR;
    }
    removeMsg.count = static_cast<int>(pids.size());
    for (size_t i = 0; i < static_cast<size_t>(removeMsg.count); ++i) {
        RemovePayload tmp{};
        tmp.pid = pids[i];
        removeMsg.payload[i] = tmp;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] removeMsg=" << removeMsg.ToString() << ".";
    auto ret = smapRemove(&removeMsg, static_cast<int>(MpConfiguration::GetInstance().GetMpSceneType()));
    if (ret != SMAP_OK) {
        SetResponse(response, ret, "SmapRemove failed.", resp);
    } else {
        SetResponse(response, MEM_POOLING_OK, "SmapRemove success.", resp);
    }
    return MEM_POOLING_OK;
}

MpResult OverCommitMsgHandler::RemoveLocalHandler(const uint16_t presentNumaId, const std::vector<pid_t>& pids)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] RemoveLocalHandler start.";
    const auto smapRemove = SmapModule::GetSmapRemoveFunc();
    if (smapRemove == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] GetSmapRemoveFunc failed.";
        return MEM_POOLING_ERROR;
    }
    RemoveMsg removeMsg{};
    if (pids.size() > MAX_NR_REMOVE_MP) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MsgHandler] Pids size exceeds the limit, size=" << pids.size() << ".";
        return MEM_POOLING_ERROR;
    }
    removeMsg.count = static_cast<int>(pids.size());
    for (size_t i = 0; i < static_cast<size_t>(removeMsg.count); ++i) {
        RemovePayload tmp{};
        tmp.pid = pids[i];
        if (MpConfiguration::GetInstance().GetMpSceneType() == MpSceneType::VIRTUAL_SCENE &&
            MpConfiguration::GetInstance().GetMultiNumaScene() == true) {
            tmp.count = 1;
            tmp.nid[0] = presentNumaId;
        } else {
            tmp.count = 0;
            tmp.nid[0] = presentNumaId;
        }
        removeMsg.payload[i] = tmp;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] removeMsg=" << removeMsg.ToString() << ".";
    auto ret = smapRemove(&removeMsg, static_cast<int>(MpConfiguration::GetInstance().GetMpSceneType()));
    if (ret != SMAP_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] SmapRemoveLocal failed.";
        return MEM_POOLING_ERROR;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] SmapRemoveLocal success.";
    return MEM_POOLING_OK;
}

MpResult OverCommitMsgHandler::ProcessQueryHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    if (req.data == nullptr || req.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] ProcessQueryHandler req.data is null.";
        return MEM_POOLING_ERROR;
    }
    ResponseInfoSimpo response;
    SmapRemoteProcessQueryTransMsg smapRemoteProcessQueryMsg{};
    RmrsInStream builder(req.data, req.len);
    builder >> smapRemoteProcessQueryMsg;
    const auto [numaIds] = smapRemoteProcessQueryMsg.GetSmapRemoteProcessQueryTrans();
    const auto smapQueryProcessConfig = SmapModule::GetSmapGetRemoteProcessesFunc();
    if (smapQueryProcessConfig == nullptr) {
        SetResponse(response, MEM_POOLING_ERROR, "SmapQuery failed.", resp);
        return MEM_POOLING_ERROR;
    }
    JSON_MAP numa2pidMap;
    auto ret = MEM_POOLING_OK;
    auto totalRet = MEM_POOLING_OK;
    for (size_t i = 0; i < numaIds.size(); i++) {
        ProcessPayload processPayload[SMAP_QUERY_PID_NUM];
        int retLen = 0;
        ret = smapQueryProcessConfig(numaIds[i], processPayload, SMAP_QUERY_PID_NUM, &retLen);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OSTurboSmap][MsgHandler] Query config failed.";
            totalRet = MEM_POOLING_ERROR;
            break;
        }
        std::vector<std::string> pids;
        for (int j = 0; j < retLen; j++) {
            if (processPayload[j].scanType == 1) {
                (void)pids.emplace_back(std::to_string(processPayload[j].pid));
            }
        }
        std::string str;
        if (!JsonUtil::RackMemConvertVector2JsonStr(pids, str)) {
            SetResponse(response, MEM_POOLING_ERROR, "SmapQuery convert json failed.", resp);
            return MEM_POOLING_ERROR;
        }
        numa2pidMap[std::to_string(numaIds[i])] = str;
    }
    if (totalRet != MEM_POOLING_OK) {
        SetResponse(response, totalRet, "SmapRemove failed.", resp);
    } else {
        std::string str2;
        if (!JsonUtil::RackMemConvertMap2JsonStr(numa2pidMap, str2)) {
            SetResponse(response, MEM_POOLING_ERROR, "SmapQuery convert json map failed.", resp);
            return MEM_POOLING_ERROR;
        }
        SetResponse(response, MEM_POOLING_OK, str2, resp);
    }
    return MEM_POOLING_OK;
}

MpResult OverCommitMsgHandler::ProcessQueryLocalHandler(const std::vector<uint32_t>& numaIds,
                                                        std::string& numa2pidMapJson, bool isReturn)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] ProcessQueryLocalHandler start.";
    const auto smapQueryProcessConfig = SmapModule::GetSmapGetRemoteProcessesFunc();
    if (smapQueryProcessConfig == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[OSTurboSmap][MsgHandler] GetSmapGetRemoteProcessesFunc failed.";
        return MEM_POOLING_ERROR;
    }
    JSON_MAP numa2pidMap;
    auto ret = MEM_POOLING_OK;
    auto totalRet = MEM_POOLING_OK;
    for (size_t i = 0; i < numaIds.size(); i++) {
        ProcessPayload processPayload[SMAP_QUERY_PID_NUM];
        int retLen = 0;
        ret = smapQueryProcessConfig(numaIds[i], processPayload, SMAP_QUERY_PID_NUM, &retLen);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OSTurboSmap][MsgHandler] Query config failed.";
            totalRet = MEM_POOLING_ERROR;
            break;
        }
        std::vector<std::string> pids;
        for (int j = 0; j < retLen; j++) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[OSTurboSmap][MsgHandler] ProcessQuery, j=" << j << ",pid=" << processPayload[j].pid
                << ", scanType=" << processPayload[j].scanType << ", isReturn=" << isReturn << ".";
            if (isReturn && (processPayload[j].scanType == 1 || processPayload[j].scanType == 0)) {
                (void)pids.emplace_back(std::to_string(processPayload[j].pid));
            }

            if (!isReturn && processPayload[j].scanType == 1) {
                (void)pids.emplace_back(std::to_string(processPayload[j].pid));
            }
        }
        std::string str;
        if (!JsonUtil::RackMemConvertVector2JsonStr(pids, str)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] SmapQueryLocal convert json failed.";
            return MEM_POOLING_ERROR;
        }
        numa2pidMap[std::to_string(numaIds[i])] = str;
    }
    if (totalRet != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] SmapQueryProcessByLocal failed.";
        return MEM_POOLING_ERROR;
    } else {
        if (!JsonUtil::RackMemConvertMap2JsonStr(numa2pidMap, numa2pidMapJson)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] SmapQueryLocal convert json map failed.";
            return MEM_POOLING_ERROR;
        }
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MsgHandler] SmapQueryLocal success.";
    }
    return MEM_POOLING_OK;
}

void OverCommitMsgHandler::SetResponse(ResponseInfoSimpo& response, const MpResult& retCode, const std::string& msg,
                                       UbseByteBuffer& resBuffer)
{
    response.SetResponseInfo(retCode, msg);
    RmrsOutStream builder;
    builder << response;

    resBuffer.data = builder.GetBufferPointer();
    resBuffer.len = builder.GetSize();
    resBuffer.freeFunc = DefaultFreeFunc;
}

uint32_t NumaBindTypeNotify(UbseByteBuffer& buffer)
{
    return OverCommitStorage::Instance().PutNumaBindTypeRawData(buffer);
}

uint32_t NumaBindTypeGetData(UbseByteBuffer& data)
{
    return OverCommitStorage::Instance().GetNumaBindTypeRawData(data, true);
}

uint32_t InitOverCommitReg()
{
    // 超分场景 agent节点处理setSmapRemoteNumaInfo
    UbseComEndpoint endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_SET_SMAP_REMOTE_NUMA_INFO};
    auto ret = UbseRegRpcService(endpoint, over_commit::OverCommitMsgHandler::SetSmapRemoteNumaHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "SetSmapRemoteNumaHandler reg failed res: " << ret;
        return ret;
    }

    // 超分场景 agent节点处理SmapRemove
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_SMAP_REMOVE};
    ret = UbseRegRpcService(endpoint, over_commit::OverCommitMsgHandler::RemoveHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "RemoveHandler reg failed res: " << ret;
    }

    // 超分场景 agent节点处理SmapQuery
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_SMAP_PROCESS_QUERY};
    ret = UbseRegRpcService(endpoint, over_commit::OverCommitMsgHandler::ProcessQueryHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "ProcessQueryHandler reg failed res: " << ret;
    }

    // 超分场景memID故障处理 agent节点查询远端numa VM的numainfo
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_OVER_COMMIT_MEMID_FAULT_GET_VM_INFO};
    ret = UbseRegRpcService(endpoint, over_commit::OverCommitFaultManagementHandler::GetVmNumaInfoMapRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "GetVmNumaInfoMapRecvHandler reg failed res: " << ret;
    }

    // 超分场景memID故障处理 agent节点查询localNuma VM的numainfo
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_OVER_COMMIT_MIGRATE_GET_LOCAL_VM_INFO};
    ret = UbseRegRpcService(endpoint, over_commit::OverCommitMsg::GetVmNumaInfoMapRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "OverCommitMsgHandler GetVmNumaInfoMapRecvHandler reg failed res: " << ret;
    }

    // 超分场景memID故障处理 agent节点进行大页设置、remoteNumaInfo设置和 VM迁移
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_OVER_COMMIT_MEM_ID_FAULT_EXECUTE};
    ret = UbseRegRpcService(endpoint, over_commit::OverCommitFaultManagementHandler::MemIdExecuteRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "MemIdExecuteRecvHandler reg failed res: " << ret;
    }

    // 超分场景memID故障处理 agent节点内存迁回+内存归还
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_OVER_COMMIT_MEM_ID_FAULT_RETURN_EXECUTE};
    ret = UbseRegRpcService(endpoint, over_commit::OverCommitFaultManagementHandler::MemIdReturnExecuteRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "MemIdReturnExecuteRecvHandler reg failed res: " << ret;
    }

    // 超分场景memID故障处理 agent节点停止冷热流动
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_SMAP_PROCESS_MIGRATE_DISABLE};
    ret = UbseRegRpcService(endpoint,
                            over_commit::OverCommitFaultManagementHandler::DisableSmapProcessMigrateRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "DisableSmapProcessMigrateRecvHandler reg failed res: " << ret;
    }

    // 超分场景memID故障处理 agent节点开启冷热流动
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_SMAP_PROCESS_MIGRATE_ENABLE};
    ret =
        UbseRegRpcService(endpoint, over_commit::OverCommitFaultManagementHandler::EnableSmapProcessMigrateRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "EnableSmapProcessMigrateRecvHandler reg failed res: " << ret;
    }

    // 超分场景memID故障处理 agent节点内存归还
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_OVER_COMMIT_MEM_ID_FAULT_DIRECTLY_RETURN_EXECUTE};
    ret = UbseRegRpcService(endpoint,
                            over_commit::OverCommitFaultManagementHandler::MemIdReturnDirectlyExecuteRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "MemIdReturnDirectlyExecuteRecvHandler reg failed res: " << ret;
    }

    // 虚机超分场景故障处理 在借入节点上借用内存、迁出pid、归还内存
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_OVER_COMMIT_FAULT_NUMA_PROCESS};
    ret = UbseRegRpcService(endpoint, over_commit::OverCommitFaultManagementHandler::FaultNumaProcessRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "FaultNumaProcessRecvHandler reg failed res: " << ret;
    }

    // 单numa超分场景故障处理在节点侧进行借用
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_OVER_COMMIT_FAULT_HANDLE_MEM_BORROW};
    ret = UbseRegRpcService(endpoint, over_commit::OverCommitFaultManagementHandler::FaultHandleMemBorrowRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "FaultHandleMemBorrowRecvHandler reg failed res: " << ret;
    }

    // 简化版单numa超分场景故障处理在节点侧进行借用
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_OVER_COMMIT_FAULT_NUMA_PROCESS_SIMPLIFIED};
    ret = UbseRegRpcService(endpoint,
                            over_commit::OverCommitFaultManagementHandler::SimplifiedFaultNumaProcessRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "SimplifiedFaultNumaProcessRecvHandler reg failed res: " << ret;
    }

    // 查询节点NumaBindType RPC handler
    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_GET_NUMA_BIND_TYPE_FROM_NODE};
    ret = UbseRegRpcService(endpoint, over_commit::OverCommitMsg::GetNumaBindTypeRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "GetNumaBindTypeRecvHandler reg failed res: " << ret;
    }

    return ret;
}

uint32_t InitExportReg()
{
    // 容器采集 agent节点进行容器NUMA信息采集
    UbseComEndpoint endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_PID_NUMA_INFO_COLLECT};
    auto ret = UbseRegRpcService(endpoint, PidNumaInfoCollectRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "PidNumaInfoCollectRecvHandler reg failed res: " << ret;
        return ret;
    }

    endpoint = {.moduleId = MP_MODULE_CODE, .serviceId = OPCODE_NUMA_MEM_INFO_COLLECT};
    ret = UbseRegRpcService(endpoint, over_commit::OverCommitMsg::NumaMemInfoCollectRecvHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "NumaMemInfoCollectRecvHandler reg failed res: " << ret;
        return ret;
    }
    return ret;
}

uint32_t PidNumaInfoCollectRecvHandler(const UbseByteBuffer& req, UbseByteBuffer& resp)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[PidNumaInfoCollect] PidNumaInfoCollectRecvHandler start.";
    if (req.data == nullptr || req.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[PidNumaInfoCollect] PidNumaInfoCollectRecvHandler req.data is null.";
        return MEM_POOLING_ERROR;
    }
    turbo::rmrs::PidNumaInfoCollectParam pidNumaInfoCollectParam;
    turbo::rmrs::PidNumaInfoCollectResult pidNumaInfoCollectResult;
    RmrsInStream builderIn(req.data, req.len);
    builderIn >> pidNumaInfoCollectParam;

    auto ret = MempoolingMessage::rmrsPidNumaInfoCollect(pidNumaInfoCollectParam, pidNumaInfoCollectResult);
    if (ret != IPC_OK) {
        MpResult errCode = MEM_POOLING_ERROR;
        if (ret == IPC_BAD_CONNECT) {
            errCode = MEM_POOLING_TURBO_CONNECT_ERROR;
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[PidNumaInfoCollect] Ipc connect failed, ret=" << ret << ".";
        } else {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[PidNumaInfoCollect] OSTurboFunctionCaller failed, ret=" << ret << ".";
        }

        mempooling::over_commit::PidNumaInfoCollectResult pidNumaInfoCollectResult;
        std::vector<mempooling::RmrsPidInfo> errPidInfoList;
        mempooling::RmrsPidInfo errPidInfo;
        errPidInfo.pid = -1;
        errPidInfoList.push_back(errPidInfo);
        pidNumaInfoCollectResult.pidInfoList = errPidInfoList;
        RmrsOutStream builder;
        builder << pidNumaInfoCollectResult;
        resp.data = builder.GetBufferPointer();
        resp.len = builder.GetSize();
        resp.freeFunc = [](uint8_t* data) {
            delete[] data;
        };
        return errCode;
    }

    RmrsOutStream builderOut;
    builderOut << pidNumaInfoCollectResult;
    resp.data = builderOut.GetBufferPointer();
    resp.len = builderOut.GetSize();
    resp.freeFunc = [](uint8_t* data) {
        delete[] data;
    };
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[PidNumaInfoCollect] PidNumaInfoCollect sucess.";
    return MEM_POOLING_OK;
}

uint32_t PidNumaInfoCollectHandler(const turbo::rmrs::PidNumaInfoCollectParam& pidNumaInfoCollectParam,
                                   turbo::rmrs::PidNumaInfoCollectResult& pidNumaInfoCollectResult)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[PidNumaInfoCollect] PidNumaInfoCollectHandler start.";

    auto ret = MempoolingMessage::rmrsPidNumaInfoCollect(pidNumaInfoCollectParam, pidNumaInfoCollectResult);
    if (ret != IPC_OK) {
        MpResult errCode = MEM_POOLING_ERROR;
        if (ret == IPC_BAD_CONNECT) {
            errCode = MEM_POOLING_TURBO_CONNECT_ERROR;
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[PidNumaInfoCollect] Ipc connect failed, ret=" << ret << ".";
        } else {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[PidNumaInfoCollect] OSTurboFunctionCaller failed, ret=" << ret << ".";
        }
        return errCode;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[PidNumaInfoCollect] PidNumaInfoCollect sucess.";
    return MEM_POOLING_OK;
}

void PidNumaInfoCollectRpcResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[PidNumaInfoCollectRpcResHandler] Ctx or respData is null.";
        return;
    }
    turbo::rmrs::PidNumaInfoCollectResult result;
    turbo::rmrs::PidNumaInfoCollectResult* pidNumaInfoCollectResult =
        static_cast<turbo::rmrs::PidNumaInfoCollectResult*>(ctx);
    if (resCode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "send error, res=" << resCode << ".";
    } else {
        RmrsInStream inBuilder(respData.data, respData.len);
        inBuilder >> result;
    }
    *pidNumaInfoCollectResult = result;
}

} // namespace mempooling::over_commit