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

#ifndef RMRS_RESOURCE_QUERY_H
#define RMRS_RESOURCE_QUERY_H

#include <sstream>
#include <vector>

#include "ubse_logger.h"
#include "exporter.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "mp_module.h"
#include "numa_info.h"
#include "turbo_rmrs_interface.h"

namespace mempooling {
using namespace ubse::log;
struct MetaNumaInfo {
    uint16_t numaId{};      // numaId
    uint64_t numaUsedMem{}; // 该numaId上使用的内存
    bool isLocalNuma{true}; // 是否本地numa
    int socketId{-1};       // numaId对应的socketId

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "numaId:" << numaId << ",";
        oss << "numaUsedMem:" << numaUsedMem << " BYTE,";
        oss << "isLocalNuma:" << isLocalNuma << ",";
        oss << "socketId:" << socketId;
        oss << "}";
        return oss.str();
    }
};

struct RmrsPidInfo {
    pid_t pid{};                               // 进程id
    std::vector<uint16_t> localNumaIds{};      // 本地numaId集合
    uint64_t totalLocalUsedMem{};              // 本地numa上使用的总内存大小
    uint64_t totalRemoteUsedMem{};             // 远端numa使用的总内存大小
    uint16_t remoteNumaId{};                   // 远端numaId（仅在容器场景下有效）
    std::vector<MetaNumaInfo> metaNumaInfos{}; // pid进程元信息集合

    std::string ToString() const
    {
        std::ostringstream oss;
        oss << "{";
        oss << "pid:" << pid;
        oss << ",localNumaIds:[";
        for (size_t i = 0; i < localNumaIds.size(); ++i) {
            if (i)
                oss << ", ";
            oss << localNumaIds[i];
        }
        oss << "]";

        oss << ",totalLocalUsedMem:" << totalLocalUsedMem << " BYTE";
        oss << ",totalRemoteUsedMem:" << totalRemoteUsedMem << " BYTE";
        oss << ",remoteNumaId:" << remoteNumaId;

        oss << ",metaNumaInfos:[";
        for (size_t i = 0; i < metaNumaInfos.size(); ++i) {
            if (i)
                oss << ", ";
            oss << metaNumaInfos[i].ToString();
        }
        oss << "]";
        oss << "}";
        return oss.str();
    }
};

MpResult HelpGetVmInfoListOnNode(std::vector<mempooling::exportV2::VmDomainInfo>& vmDomainInfos);
MpResult HelpGetNumaInfoListOnNode(std::vector<mempooling::exportV2::NumaInfo>& numaInfos);

class ResourceQuery {
public:
    static MpResult HelpGetContainerPidNumaInfo(const std::string& srcNid, const std::vector<pid_t>& pids,
                                                std::vector<RmrsPidInfo>& pidInfos);
    static MpResult HelpGetNumaMemInfoCollect(const std::string& srcNid, const int& numaId,
                                              mempooling::NumaMetaData& numaInfo);
    static MpResult HelpGetContainerPidNumaInfoByLocalNode(const std::string& srcNid, const std::vector<pid_t>& pidList,
                                                           std::vector<RmrsPidInfo>& pidInfos);
    static MpResult ConvertMetaNumaInfos(std::vector<turbo::rmrs::MetaNumaInfo> metaNumaInfos,
                                         mempooling::RmrsPidInfo& pidInfo);
    static MpResult FilterValidPidListByLocalNode(std::vector<pid_t>& pidList);
    static MpResult FilterValidPidListRpc(const std::string& srcNid, std::vector<pid_t>& pidList);

private:
    static std::string addTag;
    const std::string tagPrefix = "rack_mgr_ip";
};

MpResult ExportV2Init();
MpResult ExportV2DeInit();

class MpExportSubModule : public MpSubModule {
public:
    MpResult Init() override
    {
        if (MpConfiguration::GetInstance().GetMpSceneType() == MpSceneType::VIRTUAL_SCENE) {
            auto res = ExportV2Init(); // 采集功能初始化
            if (res != MEM_POOLING_OK) {
                UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Failed to init export.";
                return MEM_POOLING_ERROR;
            }
        }
        return MEM_POOLING_OK;
    }
    void DeInit() override
    {
        auto res = ExportV2DeInit();
        if (res != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Failed to deinit export.";
            return;
        }
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "Export DeInit success.";
        return;
    }
    const std::string Name() override
    {
        return "Export";
    }
};

} // namespace mempooling

#endif // RMRS_RESOURCE_QUERY_H
