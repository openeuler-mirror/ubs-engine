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

#include "mp_memory_info.h"
#include "ubse_logger.h"
#include "mp_error.h"

namespace mempooling {
using namespace ubse::log;

uint64_t StringToKB(std::string& str)
{
    if (str.empty()) {
        return 0;
    }
    size_t unitPos = str.find_first_not_of("0123456789."); // 找到单位起始位置
    if (unitPos == std::string::npos || unitPos == 0) {
        return 0; // 无单位，返回 0
    }
    double value = 0.0;
    try {
        value = std::stod(str.substr(0, unitPos));
    } catch (const std::exception&) {
        return 0;
    }
    std::string unit = str.substr(unitPos); // 提取单位部分
    if (unit == "KB") {
        return static_cast<uint64_t>(value);
    }
    if (unit == "MB") {
        return static_cast<uint64_t>(value * KB);
    }
    if (unit == "GB") {
        return static_cast<uint64_t>(value * MB);
    }
    if (unit == "TB") {
        return static_cast<uint64_t>(value * TB);
    }
    return 0; // 未知单位，返回 0
}

bool NodeMemoryInfoList::ParseNodeMemoryInfoMap(const JSON_VEC& nodeMemoryInfoListVec, const int& i,
                                                JSON_MAP& nodeMemoryInfoMap)
{
    for (const auto& key : {"timestamp", "nodeId", "totalMemory", "usedMemory", "freeMemory", "borrowedMemory",
                            "lentMemory", "numaMemInfo", "borrowedAndLentInfo"}) {
        (void)nodeMemoryInfoMap.emplace(key, "");
    }
    if (!JsonUtil::RackMemConvertJsonStr2Map(nodeMemoryInfoListVec[i], nodeMemoryInfoMap)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "RackMemConvertJsonStr2Vec error.";
        return MEM_POOLING_ERROR;
    }
    uint64_t tmpTimeStamp;
    uint64_t tmpTotalMemory;
    uint64_t tmpUsedMemory;
    uint64_t tmpFreeMemory;
    uint64_t tmpBorrowedMemory;
    uint64_t tmpLentMemory;
    if (!StrToULong(nodeMemoryInfoMap["timestamp"], tmpTimeStamp) ||
        !StrToULong(std::to_string(StringToKB(nodeMemoryInfoMap["totalMemory"])), tmpTotalMemory) ||
        !StrToULong(std::to_string(StringToKB(nodeMemoryInfoMap["usedMemory"])), tmpUsedMemory) ||
        !StrToULong(std::to_string(StringToKB(nodeMemoryInfoMap["freeMemory"])), tmpFreeMemory) ||
        !StrToULong(std::to_string(StringToKB(nodeMemoryInfoMap["lentMemory"])), tmpLentMemory)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Str to nodeMemoryInfoMap error.";
        return MEM_POOLING_ERROR;
    }
    this->nodeMemoryInfoList[i].timestamp = static_cast<time_t>(tmpTimeStamp);
    this->nodeMemoryInfoList[i].nodeId = nodeMemoryInfoMap["nodeId"];
    this->nodeMemoryInfoList[i].totalMemory = static_cast<uint64_t>(tmpTotalMemory);
    this->nodeMemoryInfoList[i].usedMemory = static_cast<uint64_t>(tmpUsedMemory);
    this->nodeMemoryInfoList[i].freeMemory = static_cast<uint64_t>(tmpFreeMemory);
    this->nodeMemoryInfoList[i].lentMemory = static_cast<uint64_t>(tmpLentMemory);
    return MEM_POOLING_OK;
}

bool NodeMemoryInfoList::ParseNumaMemInfoMap(const JSON_VEC& numaMemInfoVec, const int& i, const int& j,
                                             JSON_MAP& numaMemInfoMap)
{
    static const std::vector<std::string> keys = {"numaId",      "socketId",   "memTotal",  "memFree",
                                                  "memUsed",     "vmMemTotal", "vmMemFree", "vmMemUsed",
                                                  "reservedMem", "lentMem",    "sharedMem"};
    for (const auto& key : keys) {
        (void)numaMemInfoMap.emplace(key, "");
    }
    if (!JsonUtil::RackMemConvertJsonStr2Map(numaMemInfoVec[j], numaMemInfoMap)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "RackMemConvertJsonStr2Vec error.";
        return MEM_POOLING_ERROR;
    }
    uint32_t tmpNumaId;
    uint32_t tmpSocketId;
    uint64_t tmpMemTotal;
    uint64_t tmpMemFree;
    uint64_t tmpMemUsed;
    uint64_t tmpVmMemTotal;
    uint64_t tmpVmMemFree;
    uint64_t tmpVmUsedMem;
    uint64_t tmpReservedMem;
    uint64_t tmpLentMem;
    uint64_t tmpSharedMem;
    if (!StrToUint(numaMemInfoMap["numaId"], tmpNumaId) || !StrToUint(numaMemInfoMap["socketId"], tmpSocketId) ||
        !StrToULong(std::to_string(StringToKB(numaMemInfoMap["memTotal"])), tmpMemTotal) ||
        !StrToULong(std::to_string(StringToKB(numaMemInfoMap["memFree"])), tmpMemFree) ||
        !StrToULong(std::to_string(StringToKB(numaMemInfoMap["memUsed"])), tmpMemUsed) ||
        !StrToULong(std::to_string(StringToKB(numaMemInfoMap["vmMemTotal"])), tmpVmMemTotal) ||
        !StrToULong(std::to_string(StringToKB(numaMemInfoMap["vmMemFree"])), tmpVmMemFree) ||
        !StrToULong(std::to_string(StringToKB(numaMemInfoMap["vmMemUsed"])), tmpVmUsedMem) ||
        !StrToULong(std::to_string(StringToKB(numaMemInfoMap["reservedMem"])), tmpReservedMem) ||
        !StrToULong(std::to_string(StringToKB(numaMemInfoMap["lentMem"])), tmpLentMem) ||
        !StrToULong(std::to_string(StringToKB(numaMemInfoMap["sharedMem"])), tmpSharedMem)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Str to numaMemInfoMap error.";
        return MEM_POOLING_ERROR;
    }
    this->nodeMemoryInfoList[i].numaMemInfo[j].numaId = tmpNumaId;
    this->nodeMemoryInfoList[i].numaMemInfo[j].socketId = tmpSocketId;
    this->nodeMemoryInfoList[i].numaMemInfo[j].memTotal = tmpMemTotal;
    this->nodeMemoryInfoList[i].numaMemInfo[j].memFree = tmpMemFree;
    this->nodeMemoryInfoList[i].numaMemInfo[j].memUsed = tmpMemUsed;
    this->nodeMemoryInfoList[i].numaMemInfo[j].vmMemTotal = tmpVmMemTotal;
    this->nodeMemoryInfoList[i].numaMemInfo[j].vmMemFree = tmpVmMemFree;
    this->nodeMemoryInfoList[i].numaMemInfo[j].vmUsedMem = tmpVmUsedMem;
    this->nodeMemoryInfoList[i].numaMemInfo[j].reservedMem = tmpReservedMem;
    this->nodeMemoryInfoList[i].numaMemInfo[j].lentMem = tmpLentMem;
    this->nodeMemoryInfoList[i].numaMemInfo[j].sharedMem = tmpSharedMem;
    return MEM_POOLING_OK;
}

bool NodeMemoryInfoList::CreateNodeMemoryInfoListVec(const std::string& jsonString, JSON_MAP& nodeMemoryInfoListMAP,
                                                     JSON_VEC& nodeMemoryInfoListVec)
{
    (void)nodeMemoryInfoListMAP.emplace("nodeMemoryInfoList", "");
    if (!JsonUtil::RackMemConvertJsonStr2Map(jsonString, nodeMemoryInfoListMAP)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "RackMemConvertJsonStr2Map error.";
        return MEM_POOLING_ERROR;
    }
    if (!JsonUtil::RackMemConvertJsonStr2Vec(nodeMemoryInfoListMAP["nodeMemoryInfoList"], nodeMemoryInfoListVec)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "RackMemConvertJsonStr2Vec error.";
        return MEM_POOLING_ERROR;
    }
    this->nodeMemoryInfoList.resize(nodeMemoryInfoListVec.size());
    return MEM_POOLING_OK;
}

bool NodeMemoryInfoList::CreateNumaMemInfoVec(JSON_VEC& numaMemInfoVec, const int& i, JSON_MAP& nodeMemoryInfoMap)
{
    if (!JsonUtil::RackMemConvertJsonStr2Vec(nodeMemoryInfoMap["numaMemInfo"], numaMemInfoVec)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "RackMemConvertJsonStr2Vec error.";
        return MEM_POOLING_ERROR;
    }
    this->nodeMemoryInfoList[i].numaMemInfo.resize(numaMemInfoVec.size());
    for (size_t j = 0; j < numaMemInfoVec.size(); ++j) {
        JSON_MAP numaMemInfoMap;
        auto ret = ParseNumaMemInfoMap(numaMemInfoVec, i, j, numaMemInfoMap);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "ParseNumaMemInfoMap error.";
            return MEM_POOLING_ERROR;
        }
    }
    return MEM_POOLING_OK;
}

bool NodeMemoryInfoList::FromJson(const std::string& jsonString)
{
    JSON_MAP nodeMemoryInfoListMAP;
    JSON_VEC nodeMemoryInfoListVec;
    auto ret = CreateNodeMemoryInfoListVec(jsonString, nodeMemoryInfoListMAP, nodeMemoryInfoListVec);
    std::string resBody = jsonString;
    resBody.erase(std::remove_if(resBody.begin(), resBody.end(), ::isspace), resBody.end());
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "CreateVec error, resBody = " << resBody;
        return MEM_POOLING_ERROR;
    }
    for (size_t i = 0; i < nodeMemoryInfoListVec.size(); ++i) {
        JSON_MAP nodeMemoryInfoMap;
        auto ret = ParseNodeMemoryInfoMap(nodeMemoryInfoListVec, i, nodeMemoryInfoMap);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "ParseNodeMemoryInfoMap error, resBody = " << resBody;
            return MEM_POOLING_ERROR;
        }
        JSON_VEC numaMemInfoVec;
        ret = CreateNumaMemInfoVec(numaMemInfoVec, i, nodeMemoryInfoMap);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "CreateNumaMemInfoVec error, resBody = " << resBody;
            return MEM_POOLING_ERROR;
        }
    }
    return MEM_POOLING_OK;
}

std::string NodeMemoryInfoWithReservedMem::ToString()
{
    std::stringstream ss;
    struct tm tmStruct;
    char timeBuffer[20];
    localtime_r(&timestamp, &tmStruct);
    size_t result = strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &tmStruct);
    if (result == 0) {
        return "";
    }
    ss << "timestamp: " << timeBuffer;
    ss << ", nodeId: " << nodeId;
    ss << ", totalMemory: " << totalMemory << "kB";
    ss << ", usedMemory: " << usedMemory << "kB";
    ss << ", freeMemory: " << freeMemory << "kB";
    ss << ", lentMemory: " << lentMemory << "kB";
    ss << ", numaMemInfo: [";
    for (size_t i = 0; i < numaMemInfo.size(); ++i) {
        if (i > 0) {
            ss << ", ";
        }
        ss << numaMemInfo[i].ToString();
    }
    ss << "]";
    ss << ", reservedMem: " << reservedMem << "kB";
    ss << ", sharedMem: " << sharedMem << "kB";
    return ss.str();
}

} // namespace mempooling
