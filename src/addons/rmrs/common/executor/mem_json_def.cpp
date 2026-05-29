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

#include "mem_json_def.h"

#include <iostream>
#include <regex>
#include <sstream>
#include <string>

namespace mempooling {
using namespace ubse::utils;
std::string RackCreateResourceWaterBorrowAttr::ToJson()
{
    JSON_VEC lenderLocVec;
    JSON_VEC lenderSizeVec;
    for (const auto& item : this->waterMallocAttr.lenderLocs) {
        (void)lenderLocVec.emplace_back(item.nodeId + "/" + std::to_string(item.socketId) + "/" +
                                        std::to_string(item.numaId));
    }

    for (auto item : this->waterMallocAttr.lenderSizes) {
        (void)lenderSizeVec.emplace_back(std::to_string(item));
    }

    JSON_STR jArrLenderLocStr;
    JSON_STR jArrLenderSizeStr;

    if (!JsonUtil::RackMemConvertVector2JsonStr(lenderLocVec, jArrLenderLocStr) ||
        !JsonUtil::RackMemConvertVector2JsonStr(lenderSizeVec, jArrLenderSizeStr)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Convert vector to json string failed.";
        return "";
    }

    JSON_MAP waterMallocAttrMap;
    (void)waterMallocAttrMap.emplace("srcNid", this->waterMallocAttr.srcNid);
    (void)waterMallocAttrMap.emplace("srcSocket", std::to_string(this->waterMallocAttr.srcSocket));
    (void)waterMallocAttrMap.emplace("srcNuma", std::to_string(this->waterMallocAttr.srcNuma));
    (void)waterMallocAttrMap.emplace("dstNodeNum", std::to_string(this->waterMallocAttr.dstNodeNum));
    (void)waterMallocAttrMap.emplace("lenderNumaSize", std::to_string(this->waterMallocAttr.lenderNumaSize));
    (void)waterMallocAttrMap.emplace("lenderLocs", jArrLenderLocStr);
    (void)waterMallocAttrMap.emplace("lenderSize", jArrLenderSizeStr);
    JSON_STR jsonWaterMallocAttrStr;
    if (!JsonUtil::RackMemConvertMap2JsonStr(waterMallocAttrMap, jsonWaterMallocAttrStr)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Convert map to json string failed.";
        return "";
    }
    JSON_MAP strMap;
    (void)strMap.emplace("numaPresentId", std::to_string(this->numaPresentId));
    (void)strMap.emplace("type", this->type);
    (void)strMap.emplace("name", this->name);
    (void)strMap.emplace("size", std::to_string(this->size));
    (void)strMap.emplace("highWatermark", std::to_string(this->highWatermark));
    (void)strMap.emplace("lowWatermark", std::to_string(this->lowWatermark));
    (void)strMap.emplace("perfLevel", std::to_string(this->perfLevel));
    (void)strMap.emplace("waterMallocAttr", jsonWaterMallocAttrStr);

    JSON_STR res;
    if (!JsonUtil::RackMemConvertMap2JsonStr(strMap, res)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Convert map to json string failed.";
        return "";
    }
    return res;
}

bool RackCreateResourceWaterBorrowAttr::ParseJsonString(const std::string& jsonString, JSON_MAP& strMap)
{
    (void)strMap.emplace("numaPresentId", "");
    (void)strMap.emplace("type", "");
    (void)strMap.emplace("name", "");
    (void)strMap.emplace("size", "");
    (void)strMap.emplace("highWatermark", "");
    (void)strMap.emplace("lowWatermark", "");
    (void)strMap.emplace("perfLevel", "");
    (void)strMap.emplace("waterMallocAttr", "");
    if (!JsonUtil::RackMemConvertJsonStr2Map(jsonString, strMap)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Convert json string to map failed.";
        return false;
    }

    return true;
}

bool RackCreateResourceWaterBorrowAttr::ParseBasicAttributes(JSON_MAP& strMap)
{
    int64_t pLev;
    if (!StrToULong(strMap["size"], this->size) || !StrToULong(strMap["highWatermark"], this->highWatermark) ||
        !StrToULong(strMap["lowWatermark"], this->lowWatermark) ||
        ConvertStrToLong(strMap["numaPresentId"], this->numaPresentId) || ConvertStrToLong(strMap["perfLevel"], pLev)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Convert string to int failed.";
        return false;
    }
    this->type = strMap["type"];
    this->name = strMap["name"];
    this->perfLevel = static_cast<PerfLevel>(pLev);

    return true;
}

bool RackCreateResourceWaterBorrowAttr::ParseWaterMallocAttr(JSON_MAP& strMap, JSON_MAP& waterMallocAttrMap)
{
    (void)waterMallocAttrMap.emplace("srcNid", "");
    (void)waterMallocAttrMap.emplace("srcSocket", "");
    (void)waterMallocAttrMap.emplace("srcNuma", "");
    (void)waterMallocAttrMap.emplace("dstNodeNum", "");
    (void)waterMallocAttrMap.emplace("lenderNumaSize", "");
    (void)waterMallocAttrMap.emplace("lenderLocs", "");
    (void)waterMallocAttrMap.emplace("lenderSize", "");
    if (!JsonUtil::RackMemConvertJsonStr2Map(strMap["waterMallocAttr"], waterMallocAttrMap)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Convert json string to map failed.";
        return false;
    }
    this->waterMallocAttr.srcNid = waterMallocAttrMap["srcNid"];
    int64_t tmpSkt;
    int64_t tmpNuma;
    int64_t tmpDstNum;
    int64_t lenderNumaSize0;
    if (ConvertStrToLong(waterMallocAttrMap["srcSocket"], tmpSkt) ||
        ConvertStrToLong(waterMallocAttrMap["srcNuma"], tmpNuma) ||
        ConvertStrToLong(waterMallocAttrMap["lenderNumaSize"], lenderNumaSize0) ||
        ConvertStrToLong(waterMallocAttrMap["dstNodeNum"], tmpDstNum)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Convert string to int failed.";
        return false;
    }
    this->waterMallocAttr.srcSocket = static_cast<int>(tmpSkt);
    this->waterMallocAttr.srcNuma = static_cast<int>(tmpNuma);
    this->waterMallocAttr.dstNodeNum = static_cast<int>(tmpDstNum);
    this->waterMallocAttr.lenderNumaSize = static_cast<int>(lenderNumaSize0);

    return true;
}

bool RackCreateResourceWaterBorrowAttr::ParseVecOfWaterMallocAttr(JSON_MAP& waterMallocAttrMap)
{
    JSON_VEC lenderLocsVec;
    JSON_VEC lenderSizeVec;
    if (!JsonUtil::RackMemConvertJsonStr2Vec(waterMallocAttrMap["lenderLocs"], lenderLocsVec) ||
        !JsonUtil::RackMemConvertJsonStr2Vec(waterMallocAttrMap["lenderSize"], lenderSizeVec)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Convert json string to vector failed.";
        return false;
    }
    this->waterMallocAttr.lenderLocs.resize(lenderLocsVec.size());
    for (size_t i = 0; i < lenderLocsVec.size(); ++i) {
        std::vector<std::string> vecTmp;
        auto item = lenderLocsVec[0];
        Split(item, "/", vecTmp);

        if (vecTmp.size() < 3U) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] Get lender location error.";
            return false;
        }
        this->waterMallocAttr.lenderLocs[i].nodeId = vecTmp[0];
        int64_t tmpSck;
        if (ConvertStrToLong(vecTmp[1U], tmpSck) ||
            ConvertStrToLong(vecTmp[2U], this->waterMallocAttr.lenderLocs[i].numaId)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] Convert string to int failed.";
            return false;
        }
        this->waterMallocAttr.lenderLocs[i].socketId = static_cast<int>(tmpSck);
    }

    this->waterMallocAttr.lenderSizes.resize(lenderLocsVec.size());
    for (size_t i = 0; i < lenderSizeVec.size(); ++i) {
        auto item = lenderSizeVec[0];
        if (!StrToULong(item, this->waterMallocAttr.lenderSizes[i])) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemBorrow][MemBorrowExecute] Convert string to int failed.";
            return false;
        }
    }

    return true;
}

bool RackCreateResourceWaterBorrowAttr::FromJson(const std::string& jsonString)
{
    bool ret = false;
    JSON_MAP strMap;
    JSON_MAP waterMallocAttrMap;

    ret = ParseJsonString(jsonString, strMap);
    if (!ret) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][MemBorrowExecute] Parse json string failed.";
        return ret;
    }
    ret = ParseBasicAttributes(strMap);
    if (!ret) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Parse basic attributes failed.";
        return ret;
    }
    ret = ParseWaterMallocAttr(strMap, waterMallocAttrMap);
    if (!ret) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Parse water malloc attributes failed.";
        return ret;
    }
    ret = ParseVecOfWaterMallocAttr(waterMallocAttrMap);
    if (!ret) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][MemBorrowExecute] Parse vector of water malloc attributes failed.";
        return ret;
    }

    return true;
}

} // namespace mempooling