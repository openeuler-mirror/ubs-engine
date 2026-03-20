/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.

 * UBS uCache is licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *      http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "deserialize.h"

#include <regex>
#include "master_task_controller.h"
#include "securec.h"
#include "turbo_ucache_interface.h"
#include "ubse_com.h"
#include "ucache_config.h"
#include "ucache_error.h"
#include "ucache_serialize.h"
#include "ubse_error.h"


namespace ucache {
namespace deserialize {

using namespace ucache;
using namespace master;
using namespace ubse::log;
using namespace ubse::com;
using namespace ucache::serialize;
using namespace turbo::ucache;

std::vector<BorrowMemInfo> Deserialize::borrowMemInfos;

const int KB_TO_BYTE = 1024;

uint32_t Deserialize::ParseBorrowLendMemInfo(const std::vector<UbseNumaMemoryDebtInfo> &extInfos)
{
    borrowMemInfos.reserve(extInfos.size());
    for (const auto &ext : extInfos) {
        BorrowMemInfo info{};
        info.name = ext.name;
        info.size = ext.size;
        info.lentNodeId = ext.lentNodeId;
        info.lentNumaInfos.reserve(ext.lentNumaIdList.size());
        for (size_t i = 0; i < ext.lentNumaIdList.size(); i++) {
            LentNumaInfo lentNumaInfo{ext.lentNumaIdList[i], ext.lentNumaSizeList[i]};
            info.lentNumaInfos.emplace_back(lentNumaInfo);
        }
        info.borrowNodeId = ext.borrowNodeId;
        errno_t err =
            memcpy_s(&info.borrowLocalNuma, sizeof(info.borrowLocalNuma), ext.usrInfo, sizeof(info.borrowLocalNuma));
        if (err != 0) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "memcpy_s for borrowLocalNuma failed.";
            return EXEC_MEM_BORROW_ERROR;
        }
        info.borrowRemoteNuma = ext.remoteNumaId;
        info.lentMemId = ext.lentMemId;
        info.borrowMemId = ext.borrowMemId;
        borrowMemInfos.emplace_back(std::move(info));
    }
    return UCACHE_OK;
}

uint32_t Deserialize::GetBorrowMemInfo(const std::string &nodeId, std::vector<BorrowMemInfo> &borrowMem)
{
    borrowMemInfos.clear();
    std::vector<UbseNumaMemoryDebtInfo> debtInfos{};
    UbseResult result{};
    if (nodeId != "") {
        result = UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos);
        if (result != UBSE_OK) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "UbseGetNumaMemDebtInfoWithNode failed, result=" << static_cast<uint32_t>(result)
                << ", node=" << nodeId << ".";
            return UBSE_API_ERROR;
        }
    } else {
        result = UbseGetNumaMemDebtInfo(debtInfos);
        if (result != UBSE_OK) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "UbseGetNumaMemDebtInfo failed, result=" << static_cast<uint32_t>(result) << ".";
            return UBSE_API_ERROR;
        }
    }

    uint32_t ret = ParseBorrowLendMemInfo(debtInfos);
    if (ret != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "Parse BorrowLendMemInfo failed, ret=" << ret << ".";
        return ret;
    }

    borrowMem = borrowMemInfos;
    return UCACHE_OK;
}

uint32_t Deserialize::GetNodeMemInfo(std::vector<UbseNodeNumaInfo> &numaNodeInfoList)
{
    UbseResult result = UbseGetAllNodeNumaInfo(numaNodeInfoList);
    if (result != UBSE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "UbseGetAllNodeNumaInfo failed. result=" << static_cast<uint32_t>(result) << ".";
        return UBSE_API_ERROR;
    }
    return UCACHE_OK;
}

uint32_t DispatchCollectTask(ResourceQueryType qType, const std::string &nodeId, TaskResponse &tResp)
{
    TaskRequest tReq{};
    {
        UCacheOutStream out;
        out << qType;
        tReq.payload = out.Str();
        tReq.type = TaskType::COLLECT_RESOURCE;
    }
    UBSE_LOGGER_DEBUG(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
        << "Start to dispatch CollectTask to agent, destNode=" << nodeId
        << ", queryType=" << ResourceQueryTypeToString(qType) << ".";
    uint32_t ret = DispatchTask(tReq, tResp, nodeId);
    if (ret != UCACHE_OK || tResp.resCode != UCACHE_OK) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
            << "DispatchCollectTask failed, queryType=" << ResourceQueryTypeToString(qType) << ", rpcRet=" << ret
            << ", collectRes=" << tResp.resCode << ".";
        return UCACHE_ERR;
    }
    if (tResp.timestamp == 0) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Node " << nodeId << " timestamp is 0!\
            This node may not be able to communicate.";
        return UCACHE_ERR;
    }
    return UCACHE_OK;
}

bool ValidateDockerId(const std::string &id)
{
    const size_t fullDockerIdLen = 64;
    const size_t shortDockerIdLen = 12;
    if (id.length() != fullDockerIdLen && id.length() != shortDockerIdLen) {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](unsigned char c) { return std::isxdigit(c); });
}

bool ValidateDockerInfos(const std::map<std::string, CgroupInfos> &dockerInfos)
{
    for (const auto &[dockerId, info] : dockerInfos) {
        if (!ValidateDockerId(dockerId)) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Invalid numaId, dockerId=" << dockerId << ".";
            return false;
        }
        if (!info.Validate()) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Invalid dockerInfo, dockerId=" << dockerId << ", "
                << "dockerinfo=" << info.ToString() << ".";
            return false;
        }
    }
    return true;
}

uint32_t GetCgroupInfoInNode(const std::string &nodeId, std::map<std::string, CgroupInfos> &dockerInfos,
                             std::map<std::string, uint64_t> &timeStamps)
{
    TaskResponse tResp{};
    uint32_t ret = DispatchCollectTask(ResourceQueryType::CGROUP_INFO, nodeId, tResp);
    if (ret != UCACHE_OK) {
        return ret;
    }
    timeStamps[nodeId] = tResp.timestamp;
    UCacheInStream in(tResp.payload);
    in >> dockerInfos;
    if (!in.Check()) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to deserialize payload to dockerInfos.";
        return UCACHE_ERR;
    }
    if (!ValidateDockerInfos(dockerInfos)) {
        UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Invalid dockerInfos.";
        return UCACHE_ERR;
    }
    return UCACHE_OK;
}

uint32_t Deserialize::GetCgroupInfos(
    std::queue<std::map<std::string, uint64_t>> &timeStampsCgroup,
    std::queue<std::map<std::string, std::map<std::string, CgroupInfos>>> &cgroupInfosQueue)
{
    std::vector<std::string> nodeIds = UcacheConfig::GetInstance().GetNodeIds();
    std::map<std::string, std::map<std::string, CgroupInfos>> cgroupInfos;
    std::map<std::string, uint64_t> timeStamps;
    for (auto const &nodeId : nodeIds) {
        std::map<std::string, CgroupInfos> dockerInfos;
        auto ret = GetCgroupInfoInNode(nodeId, dockerInfos, timeStamps);
        if (ret != UCACHE_OK) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Get DockerInfo in Node " << nodeId << " failed.";
            return UCACHE_ERR;
        }
        cgroupInfos[nodeId] = dockerInfos;
    }
    if (cgroupInfosQueue.size() > 1) {
        cgroupInfosQueue.pop();
        timeStampsCgroup.pop();
    }
    cgroupInfosQueue.push(cgroupInfos);
    timeStampsCgroup.push(timeStamps);
    return UCACHE_OK;
}

bool ValidateNodeStr(const std::string &name)
{
    const size_t nodeStrLen = 4;
    if (name.length() <= nodeStrLen || name.find("node") != 0) {
        return false;
    }
    std::string numPart = name.substr(nodeStrLen);
    return !numPart.empty() && std::all_of(numPart.begin(), numPart.end(), ::isdigit);
}

bool ValidateNodeInfos(const std::map<std::string, NodeInfo> &numaInfos)
{
    for (const auto &[numaId, numainfo] : numaInfos) {
        if (!ValidateNodeStr(numaId)) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Invalid numaId, numaId=" << numaId << ".";
            return false;
        }
        if (!numainfo.Validate()) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Invalid numaInfo, numaId=" << numaId << ", "
                                                                      << "numainfo=" << numainfo.ToString() << ".";
            return false;
        }
    }
    return true;
}

uint32_t Deserialize::GetNumaInfos(std::map<std::string, std::map<std::string, NodeInfo>> &nodeInfos)
{
    nodeInfos.clear();
    std::vector<std::string> nodeIds = UcacheConfig::GetInstance().GetNodeIds();
    for (auto const &nodeId : nodeIds) {
        std::map<std::string, NodeInfo> numaInfos{};
        TaskResponse tResp{};
        uint32_t ret = DispatchCollectTask(ResourceQueryType::NUMA_INFO, nodeId, tResp);
        if (ret != UCACHE_OK) {
            return ret;
        }
        UCacheInStream in(tResp.payload);
        in >> numaInfos;
        if (!in.Check()) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Failed to deserialize payload to numaInfos.";
            return UCACHE_ERR;
        }
        if (!ValidateNodeInfos(numaInfos)) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE) << "Invalid numaInfos.";
            return UCACHE_ERR;
        }
        nodeInfos[nodeId] = numaInfos;
    }
    return UCACHE_OK;
}

uint32_t Deserialize::GetMemWaterMark(std::map<std::string, uint64_t> &memWaterMarkInfos)
{
    memWaterMarkInfos.clear();
    std::vector<std::string> nodeIds = UcacheConfig::GetInstance().GetNodeIds();
    for (auto const &nodeId : nodeIds) {
        MemWatermarkInfo memWaterMark{};
        TaskResponse tResp{};
        uint32_t ret = DispatchCollectTask(ResourceQueryType::MEM_WATERMARK, nodeId, tResp);
        if (ret != UCACHE_OK) {
            return ret;
        }
        UCacheInStream in(tResp.payload);
        in >> memWaterMark;
        if (!in.Check()) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Failed to deserialize payload to memWaterMark.";
            return UCACHE_ERR;
        }
        if (!memWaterMark.Validate()) {
            UBSE_LOGGER_ERROR(UCACHE_MODULE_NAME, UCACHE_MODULE_CODE)
                << "Invalid memWaterMark, minFreeKBytes=" << memWaterMark.minFreeKBytes << ".";
            return UCACHE_ERR;
        }
        memWaterMarkInfos[nodeId] = memWaterMark.minFreeKBytes;
    }
    return UCACHE_OK;
}
} // namespace deserialize
} // namespace ucache
