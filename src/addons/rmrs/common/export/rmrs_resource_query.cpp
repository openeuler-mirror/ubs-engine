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

#include "rmrs_resource_query.h"

#include <rmrs_serialize.h>
#include <securec.h>
#include "common_delete_func.h"
#include "mempooling_message.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "mp_json_util.h"
#include "mp_string_util.h"
#include "numa_memInfo_send.h"
#include "over_commit_msg_handler.h"
#include "over_commit_serializer.h"
#include "ubse_com.h"

namespace mempooling {
using namespace ubse::log;
using namespace turbo::rmrs;

const std::string SUB_MODULE_NAME = "[ResourceQuery] ";
#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)

MpResult HelpGetVmInfoListOnNode(std::vector<mempooling::exportV2::VmDomainInfo> &vmDomainInfos)
{
    try {
        return mempooling::exportV2::Exporter::GetVmInfoImmediately(vmDomainInfos);
    } catch (const std::exception &e) {
        LOG_ERROR << "HelpGetVmInfoListOnNode caught std::exception from Exporter: " << e.what();
        return MEM_POOLING_ERROR;
    } catch (...) {
        LOG_ERROR << "HelpGetVmInfoListOnNode caught unknown exception from Exporter";
        return MEM_POOLING_ERROR;
    }
}

MpResult HelpGetNumaInfoListOnNode(std::vector<mempooling::exportV2::NumaInfo> &numaInfos)
{
    try {
        return mempooling::exportV2::Exporter::GetNumaInfoImmediately(numaInfos);
    } catch (const std::exception &e) {
        LOG_ERROR << "HelpGetNumaInfoListOnNode caught std::exception from Exporter: " << e.what();
        return MEM_POOLING_ERROR;
    } catch (...) {
        LOG_ERROR << "HelpGetNumaInfoListOnNode caught unknown exception from Exporter";
        return MEM_POOLING_ERROR;
    }
}

MpResult FillNumaInfo(mempooling::NumaMetaData &numaInfo, JSON_MAP numaInfoStrMap)
{
    LOG_INFO << "[FillNumaInfo] FillNumaInfo start.";
    
    auto iter = numaInfoStrMap.find("MemTotal");
    if (iter == numaInfoStrMap.end()) {
        LOG_ERROR << "[NumaMemInfoCollect] Json no MemTotal.";
        return MEM_POOLING_ERROR;
    }
    if (MpStringUtil::SafeStoull(iter->second, numaInfo.memTotal)) {
        LOG_ERROR << "[NumaMemInfoCollect] Stoull memTotal failed, str=" << iter->second << ".";
        return MEM_POOLING_ERROR;
    }

    iter = numaInfoStrMap.find("MemFree");
    if (iter == numaInfoStrMap.end()) {
        LOG_ERROR << "[NumaMemInfoCollect] Json no MemFree.";
        return MEM_POOLING_ERROR;
    }
    if (MpStringUtil::SafeStoull(iter->second, numaInfo.memFree)) {
        LOG_ERROR << "[NumaMemInfoCollect] Stoull memFree failed, str=" << iter->second << ".";
        return MEM_POOLING_ERROR;
    }

    iter = numaInfoStrMap.find("HugePages_Total");
    if (iter == numaInfoStrMap.end()) {
        LOG_ERROR << "[NumaMemInfoCollect] Json no HugePages_Total.";
        return MEM_POOLING_ERROR;
    }
    if (MpStringUtil::SafeStoull(iter->second, numaInfo.hugePageTotal)) {
        LOG_ERROR << "[NumaMemInfoCollect] Stoull hugePageTotal failed, str=" << iter->second << ".";
        return MEM_POOLING_ERROR;
    }

    iter = numaInfoStrMap.find("HugePages_Free");
    if (iter == numaInfoStrMap.end()) {
        LOG_ERROR << "[NumaMemInfoCollect] Json no HugePages_Free.";
        return MEM_POOLING_ERROR;
    }
    if (MpStringUtil::SafeStoull(iter->second, numaInfo.hugePageFree)) {
        LOG_ERROR << "[NumaMemInfoCollect] Stoull hugePageFree failed, str=" << iter->second << ".";
        return MEM_POOLING_ERROR;
    }
    iter = numaInfoStrMap.find("SocketId");
    if (iter == numaInfoStrMap.end()) {
        LOG_ERROR << "[NumaMemInfoCollect] Json no SocketId.";
        return MEM_POOLING_ERROR;
    }
    if (MpStringUtil::SafeStou16(iter->second, numaInfo.socketId)) {
        LOG_ERROR << "[NumaMemInfoCollect] Stoull socketId failed, str=" << iter->second << ".";
        return MEM_POOLING_ERROR;
    }

    LOG_INFO << "[FillNumaInfo] FillNumaInfo end.";

    return MEM_POOLING_OK;
}

MpResult ResourceQuery::HelpGetNumaMemInfoCollect(const std::string &srcNid, const int &numaId,
                                                  mempooling::NumaMetaData &numaInfo)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[GetNumaMemInfoCollect] Master invoke the slave to collect pid numa info numaid = " << numaId << ".";
    MpResult ret = MEM_POOLING_OK;
    auto numaMemInfoSend = over_commit::NumaMemInfoSend(srcNid, numaId);
    ret = numaMemInfoSend.SendMsg();
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[NumaMemInfoCollect] Send collect msg failed.";
        return MEM_POOLING_ERROR;
    }
    JSON_MAP numaInfoStrMap;
    static const std::vector<std::string> requiredKeys = {"MemTotal", "MemFree", "HugePages_Total", "HugePages_Free",
                                                          "SocketId"};
    for (auto const &str : requiredKeys) {
        numaInfoStrMap[str] = {};
    }
    if (!JsonUtil::RackMemConvertJsonStr2Map(numaMemInfoSend.resJson_, numaInfoStrMap)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[NumaMemInfoCollect] "
                                                             "Numa mem Info json failed.";
        return MEM_POOLING_ERROR;
    }

    ret = FillNumaInfo(numaInfo, numaInfoStrMap);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[NumaMemInfoCollect] FillNumaInfo failed.";
        return MEM_POOLING_ERROR;
    }
    return ret;
}

MpResult ResourceQuery::ConvertMetaNumaInfos(std::vector<turbo::rmrs::MetaNumaInfo> metaNumaInfos,
                                             mempooling::RmrsPidInfo &pidInfo)
{
    for (auto &metaNumaInfo : metaNumaInfos) {
        mempooling::MetaNumaInfo out{metaNumaInfo.numaId, metaNumaInfo.numaUsedMem, metaNumaInfo.isLocalNuma,
                                     metaNumaInfo.socketId};
        pidInfo.metaNumaInfos.push_back(out);
    }

    return MEM_POOLING_OK;
}

MpResult ResourceQuery::HelpGetContainerPidNumaInfo(const std::string &srcNid, const std::vector<pid_t> &pidList,
                                                    std::vector<RmrsPidInfo> &pidInfos)
{
    if (pidList.empty()) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[PidNumaInfoCollect] PidList id empty, skip collect.";
        return MEM_POOLING_OK;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[PidNumaInfoCollect] Master to invoke the slave to collect pid numa info.";
    MpResult ret = MEM_POOLING_OK;
    ubse::com::UbseComEndpoint endpoint_ms = {
        .moduleId = MP_MODULE_CODE, .serviceId = message::OPCODE_PID_NUMA_INFO_COLLECT, .address = srcNid};
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[PidNumaInfoCollect] Endpoint, srcNid=" << srcNid << ".";
    over_commit::PidNumaInfoCollectParam param(pidList);
    rmrs::serialize::RmrsOutStream builder;
    builder << param;
    UbseByteBuffer reqData = {
        .data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = DefaultFreeFunc};

    turbo::rmrs::PidNumaInfoCollectResult pidNumaInfoCollectResult;
    ret = UbseRpcSend(endpoint_ms, reqData, &pidNumaInfoCollectResult,
                      mempooling::over_commit::PidNumaInfoCollectRpcResHandler);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[PidNumaInfoCollect][Memfabric] PidNumaInfoCollect RpcSend failed. ret=" << ret << ".";
        return ret;
    }
    if (pidNumaInfoCollectResult.pidInfoList.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[PidNumaInfoCollect] Failed, Collect RmrsPidInfo empty.";
        return MEM_POOLING_ERROR;
    }
    if (pidNumaInfoCollectResult.pidInfoList.front().pid == -1) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[PidNumaInfoCollect] Call OSTurbo failed.";
        return MEM_POOLING_ERROR;
    }
    // vector 转成内部PidInfo
    for (auto &pidInfo : pidNumaInfoCollectResult.pidInfoList) {
        RmrsPidInfo innerPidInfo;
        innerPidInfo.pid = pidInfo.pid;
        innerPidInfo.totalLocalUsedMem = pidInfo.totalLocalUsedMem;
        innerPidInfo.totalRemoteUsedMem = pidInfo.totalRemoteUsedMem;
        ConvertMetaNumaInfos(pidInfo.metaNumaInfos, innerPidInfo);
        for (auto &localNumaId : pidInfo.localNumaIds) {
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[PidNumaInfoCollect] Fill outerPidInfo pid=" << pidInfo.pid << ", localNumaId=" << localNumaId
                << ".";
            innerPidInfo.localNumaIds.push_back(localNumaId);
        }
        pidInfos.push_back(innerPidInfo);
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[PidNumaInfoCollect] Container PidNumaInfoCollect success.";
    return MEM_POOLING_OK;
}

MpResult ResourceQuery::HelpGetContainerPidNumaInfoByLocalNode(const std::string &srcNid,
                                                               const std::vector<pid_t> &pidList,
                                                               std::vector<RmrsPidInfo> &pidInfos)
{
    if (pidList.empty()) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[PidNumaInfoCollect] PidList id empty, skip collect.";
        return MEM_POOLING_OK;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[PidNumaInfoCollect] LocalNode start to collect pid numa info.";
    turbo::rmrs::PidNumaInfoCollectParam pidNumaInfoCollectParam(pidList);
    turbo::rmrs::PidNumaInfoCollectResult pidNumaInfoCollectResult;

    auto ret = PidNumaInfoCollectHandler(pidNumaInfoCollectParam, pidNumaInfoCollectResult);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[PidNumaInfoCollect] PidNumaInfoCollectHandler failed. ret=" << ret << ".";
        return ret;
    }

    if (pidNumaInfoCollectResult.pidInfoList.empty()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[PidNumaInfoCollect] Failed, Collect RmrsPidInfo empty.";
        return MEM_POOLING_ERROR;
    }

    // vector 转成内部PidInfo
    for (auto &pidInfo : pidNumaInfoCollectResult.pidInfoList) {
        RmrsPidInfo innerPidInfo;
        innerPidInfo.pid = pidInfo.pid;
        innerPidInfo.totalLocalUsedMem = pidInfo.totalLocalUsedMem;
        innerPidInfo.totalRemoteUsedMem = pidInfo.totalRemoteUsedMem;
 
        innerPidInfo.metaNumaInfos.clear();
        innerPidInfo.metaNumaInfos.reserve(pidInfo.metaNumaInfos.size());
        for (const auto &meta : pidInfo.metaNumaInfos) {
            mempooling::MetaNumaInfo dst;
            dst.numaId = meta.numaId;
            dst.numaUsedMem = meta.numaUsedMem;
            dst.isLocalNuma = meta.isLocalNuma;
            dst.socketId = meta.socketId;
            if (!meta.isLocalNuma && meta.numaUsedMem > 0) {
                // 仅在容器场景下有意义
                innerPidInfo.remoteNumaId = meta.numaId;
            }
 
            innerPidInfo.metaNumaInfos.push_back(dst);
        }
        for (auto &localNumaId : pidInfo.localNumaIds) {
            innerPidInfo.localNumaIds.push_back(localNumaId);
        }
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[PidNumaInfoCollect] Collect RmrsPidInfo: " << innerPidInfo.ToString() << ".";
        pidInfos.push_back(innerPidInfo);
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[PidNumaInfoCollect] Container PidNumaInfoCollect success.";
    return MEM_POOLING_OK;
}

MpResult ExportV2Init()
{
    auto res = mempooling::exportV2::Exporter::Init();
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[ExportInit] Failed to init exporter, res=" << res << ".";
        return res;
    }
    return MEM_POOLING_OK;
}

MpResult ExportV2DeInit()
{
    mempooling::exportV2::Exporter::Shutdown();
    return MEM_POOLING_OK;
}

} // namespace mempooling
