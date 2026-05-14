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

#include "collect_util.h"
#include "ubse_com.h"
#include "ubse_logger.h"
#include "mempooling_message.h"
#include "mp_configuration.h"
#include "mp_json_util.h"
#include "mp_string_util.h"
#include "over_commit_msg_handler.h"
#include "rmrs_resource_query.h"
#include "smap_query_process_send.h"
namespace mempooling::over_commit {
using namespace ubse::log;
using namespace ubse::com;
using namespace mempooling::message;

#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)

MpResult CollectUtil::GetRemoteVmPids(const std::string& nodeID, const std::vector<uint32_t>& remoteNumaIds,
                                      std::unordered_map<std::uint16_t, std::vector<pid_t>>& res)
{
    outinterface::SrcMemoryBorrowParam srcParam{.srcNid = nodeID};
    auto smapQueryProcessSend = SmapQueryProcessSend(srcParam, remoteNumaIds);
    auto ret = smapQueryProcessSend.SendMsg();
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[MemReturn][ProcessQuery] Process remote numa failed.";
        return MEM_POOLING_ERROR;
    }
    JSON_MAP numaId2PidStrMap;
    for (auto numaId : remoteNumaIds) {
        (void)numaId2PidStrMap.emplace(std::to_string(numaId), "");
    }

    if (!JsonUtil::RackMemConvertJsonStr2Map(smapQueryProcessSend.pidJson_, numaId2PidStrMap)) {
        LOG_ERROR << "[MemReturn][ProcessQuery] Process remote numa json failed.";
        return MEM_POOLING_ERROR;
    }

    pid_t pid = 0;
    uint32_t numaId = 0;
    for (const auto& [numaIdStr, pidsStr] : numaId2PidStrMap) {
        JSON_VEC strvec;
        if (!JsonUtil::RackMemConvertJsonStr2Vec(pidsStr, strvec)) {
            LOG_ERROR << "[MemReturn][ProcessQuery] Process remote numa json vec failed.";
            return MEM_POOLING_ERROR;
        }
        std::vector<pid_t> pidVec;
        for (const auto& s : strvec) {
            ret = MpStringUtil::SafeStopid(s, pid);
            if (ret != MEM_POOLING_OK) {
                LOG_ERROR << "[MemReturn][ProcessQuery] Process remote numa pid failed, s=" << s << ", ret=" << ret
                          << ".";
                return MEM_POOLING_ERROR;
            }
            pidVec.emplace_back(pid);
        }
        ret = MpStringUtil::SafeStoul(numaIdStr, numaId);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "[MemReturn][ProcessQuery] Process remote numa id failed, numaIdStr=" << numaIdStr
                      << ", ret=" << ret << ".";
            return MEM_POOLING_ERROR;
        }
        res[numaId] = pidVec;
    }
    return MEM_POOLING_OK;
}

MpResult CollectUtil::GetRemoteVmPidsByLocal(const std::vector<uint32_t>& remoteNumaIds,
                                             std::unordered_map<std::uint16_t, std::vector<pid_t>>& res, bool isReturn)
{
    std::string numa2pidMapJson;
    auto ret = OverCommitMsgHandler::ProcessQueryLocalHandler(remoteNumaIds, numa2pidMapJson, isReturn);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][ProcessQuery] Process remote numa failed.";
        return MEM_POOLING_ERROR;
    }
    JSON_MAP numaId2PidStrMap;
    for (auto numaId : remoteNumaIds) {
        (void)numaId2PidStrMap.emplace(std::to_string(numaId), "");
    }

    if (!JsonUtil::RackMemConvertJsonStr2Map(numa2pidMapJson, numaId2PidStrMap)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][ProcessQuery] "
                                                             "Process remote numa json failed.";
        return MEM_POOLING_ERROR;
    }

    pid_t pid = 0;
    uint32_t numaId = 0;
    for (const auto& [numaIdStr, pidsStr] : numaId2PidStrMap) {
        JSON_VEC strvec;
        if (!JsonUtil::RackMemConvertJsonStr2Vec(pidsStr, strvec)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][ProcessQuery] "
                                                                 "Process remote numa json vec failed.";
            return MEM_POOLING_ERROR;
        }
        std::vector<pid_t> pidVec;
        for (const auto& s : strvec) {
            if (MpStringUtil::SafeStopid(s, pid)) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][ProcessQuery] "
                                                                     "Process remote numa pid failed.";
                return MEM_POOLING_ERROR;
            }
            pidVec.emplace_back(pid);
        }
        if (MpStringUtil::SafeStoul(numaIdStr, numaId)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemReturn][ProcessQuery] "
                                                                 "Process remote numa id failed.";
            return MEM_POOLING_ERROR;
        }
        res[numaId] = pidVec;
    }
    return MEM_POOLING_OK;
}

MpResult CollectUtil::GetNumaMemInfos(const std::string& nodeId, const std::set<int16_t>& numaIds,
                                      std::map<int, mempooling::NumaMetaData>& numaMemInfos)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommitCollect][NumaMemInfo] "
                                                        "Query GetNumaMemInfos Start.";
    MpResult ret = MEM_POOLING_OK;
    for (auto numaId : numaIds) {
        NumaMetaData numaMetaData;
        ret = ResourceQuery::HelpGetNumaMemInfoCollect(nodeId, numaId, numaMetaData);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommitCollect][NumaMemInfo] "
                                                                 "Query GetNumaMemInfos Error.";
            return ret;
        }
        numaMemInfos[numaId] = numaMetaData;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[OverCommitCollect][NumaMemInfo] "
                                                        "Query GetNumaMemInfos End.";
    return ret;
}

} // namespace mempooling::over_commit