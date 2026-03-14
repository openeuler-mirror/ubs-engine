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

#include "over_commit_storage.h"
#include "ubse_def.h"
#include "ubse_logger.h"
#include "ubse_storage.h"
#include "mp_configuration.h"
#include "mp_json_util.h"
#include "mp_string_util.h"
#include "mp_sync_data_helper.h"
#include "ubse_pointer_process.h"
#include "rmrs_serialize.h"
#include "securec.h"
#include "ubse_election.h"
namespace mempooling {
using namespace ubse::log;
using namespace ubse::storage;
using namespace rmrs::serialize;
const std::string TAG_DB = "[OverCommit][PersistentStore][NumaBindType]";
const std::string KEYPREFIX = "over_commit_";
const std::string KEYPREFIX_BINDTYPE = "numa_bind_type";
const std::string KEYPREFIX_WATERMARK = "water_mark";
const std::string HIGH_WATER_MAKR = "high_water";
const std::string LOW_WATER_MAKR = "low_water";
const int HIGH_WATER_MARK_THRESHOLD = 100;

static void GetRawData(const std::string &keyPrefix, const std::string &key, const UbseByteBuffer &buff, void *ctx)
{
    if (ctx == nullptr || buff.len == 0 || buff.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Get Raw Data failed.";
        return;
    }
    UbseByteBuffer &data = *(static_cast<UbseByteBuffer *>(ctx));
    data.data = new (std::nothrow) uint8_t[buff.len];
    if (data.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[GetRawData] Failed to allocate memory, size=" << buff.len << ".";
        return;
    }
    data.len = buff.len;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "Get Raw Data len=" << buff.len << ".";
    memcpy_s(data.data, buff.len, buff.data, buff.len);
    return;
}

std::string BindTypeToStr(const NumaBindType value)
{
    if (BIND_TYPE_TO_STR_MAP.find(value) == BIND_TYPE_TO_STR_MAP.end()) {
        return "BIND_INVALID";
    } else {
        return BIND_TYPE_TO_STR_MAP.at(value);
    }
}

MpResult OverCommitStorage::BindTypeMapToStr(std::string &str)
{
    JSON_MAP map;
    for (auto &pair : mNodeNumaBindMap) {
        auto iter = BIND_TYPE_TO_STR_MAP.find(pair.second);
        if (iter == BIND_TYPE_TO_STR_MAP.end()) {
            return MEM_POOLING_ERROR;
        } else {
            map[pair.first] = iter->second;
        }
    }
    return MEM_POOLING_OK;
}

static NumaBindType StrToBindType(const std::string &value)
{
    auto iter = STR_TO_BIND_TYPE_MAP.find(value);
    if (iter == STR_TO_BIND_TYPE_MAP.end()) { // not found
        return NumaBindType::BIND_INVALID;
    } else {
        return iter->second;
    }
}

static void GetBindTypePair(const std::string &keyPrefix, const std::string &key, const UbseByteBuffer &buff, void *ctx)
{
    if (ctx == nullptr || buff.data == nullptr || buff.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Get Bind Type Pair failed.";
        return;
    }
    std::map<std::string, std::string> &map = *(static_cast<std::map<std::string, std::string> *>(ctx));

    RmrsInStream builder(buff.data, buff.len);
    builder >> map;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "Get keyPrefix-" << keyPrefix << "key" << key << " from database, len= " << buff.len << ".";

    return;
}

static void GetWaterMarkPair(const std::string &keyPrefix, const std::string &key, const UbseByteBuffer &buff,
                             void *ctx)
{
    if (ctx == nullptr || buff.data == nullptr || buff.len == 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "Get Water Mark Pair failed.";
        return;
    }
    std::unordered_map<std::string, uint16_t> &map = *(static_cast<std::unordered_map<std::string, uint16_t> *>(ctx));
    std::string dataStr(buff.data, buff.data + buff.len);

    JSON_MAP resStrMap;
    resStrMap[HIGH_WATER_MAKR] = {};
    resStrMap[LOW_WATER_MAKR] = {};

    JsonUtil::RackMemConvertJsonStr2Map(dataStr, resStrMap);
    if (MpStringUtil::SafeStou16(resStrMap[HIGH_WATER_MAKR], map[HIGH_WATER_MAKR])) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Convert high water mark to uint16_t failed, value=" << resStrMap[HIGH_WATER_MAKR] << ".";
    }
    if (MpStringUtil::SafeStou16(resStrMap[LOW_WATER_MAKR], map[LOW_WATER_MAKR])) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "Convert high water mark to uint16_t failed, value=" << resStrMap[HIGH_WATER_MAKR] << ".";
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "WaterPair=" << dataStr << " from database.";

    return;
}

MpResult ClearData(const std::vector<std::string> &keyList)
{
    for (auto &key : keyList) {
        auto ret = UbseStorageDeleteData(KEYPREFIX_BINDTYPE, key);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << TAG_DB << "UbseStorageDeleteData failed, key=" << key << ", retCode=" << ret << ".";
            return MEM_POOLING_ERROR;
        }
    }
    return MEM_POOLING_OK;
}

MpResult OverCommitStorage::PutNumaBindTypeRawData(UbseByteBuffer &data)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[PersistentStore][NumaBindType] Put NumaBindType start.";

    std::lock_guard<std::mutex> locker(mtx);
    auto ret = UbseStoragePutData(KEYPREFIX, KEYPREFIX_BINDTYPE, &data);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG_DB << "UbseStoragePutData failed keyPrefix=" << KEYPREFIX_BINDTYPE << ", ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG_DB << "Put NumaBindType end.";

    return MEM_POOLING_OK;
}

MpResult OverCommitStorage::GetNumaBindTypeRawData(UbseByteBuffer &data, bool needLock)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[PersistentStore][VmInfosCompleted] GetNumaBindTypeRawData start.";

    std::unique_lock<std::mutex> locker(mtx, std::defer_lock);
    if (needLock) {
        locker.lock();
    }

    auto ret = UbseStorageQueryData(KEYPREFIX, KEYPREFIX_BINDTYPE, &data, GetRawData);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG_DB << "RackStorageQueryAllData fail.";
        return MEM_POOLING_ERROR;
    }

    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG_DB << " GetNumaBindTypeRawData end.";
    return MEM_POOLING_OK;
}

MpResult OverCommitStorage::UpdateBindTypeDB(const std::string &nodeId, const NumaBindType value)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << TAG_DB << "UpdateBindTypeDB start.";
    std::string valueStr = BindTypeToStr(value);
    auto iter = mNodeNumaBindMap.find(nodeId);
    // map中保存的值和输入的值相等 不入库
    if ((iter != mNodeNumaBindMap.end()) && (iter->second == value)) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG_DB << "BindType has been set. NodeId=" << nodeId << ", bindType=" << valueStr << ".";
        return MEM_POOLING_OK;
    }
    // 更新内存的值
    std::unique_lock<std::mutex> lock(mtx);
    mNodeNumaBindMap[nodeId] = value;
    // 如果当前nodeId无绑定类型 或 绑定类型变更，则入库并更新内存值
    JSON_MAP jsonMap;
    for (auto &pair : mNodeNumaBindMap) {
        jsonMap[pair.first] = BindTypeToStr(pair.second);
    }
    // 2. 持久化更新到rack数据库中
    UbseByteBuffer buffer{};
    RmrsOutStream outBuilder;
    outBuilder << jsonMap;
    size_t size = outBuilder.GetSize();
    buffer.data = outBuilder.GetBufferPointer();
    buffer.len = size;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG_DB << "bind type size=" << buffer.len << ".";
    auto ret = UbseStoragePutData(KEYPREFIX, KEYPREFIX_BINDTYPE, &buffer);
    delete[] buffer.data;
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG_DB << "UbseStoragePutData failed keyPrefix=" << KEYPREFIX_BINDTYPE << ", key=" << nodeId
            << ", bind type=" << valueStr << ", ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }

    UbseByteBuffer syncData;
    ret = GetNumaBindTypeRawData(syncData, false);
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG_DB << "Get rawdata size=" << syncData.len << ".";
    if (ret != MEM_POOLING_OK) {
        delete[] syncData.data;
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG_DB << "Get rawdata failed ret " << ret << ".";
        return MEM_POOLING_ERROR;
    }
    lock.unlock();
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG_DB << "UbseStoragePutData success, keyPrefix=" << KEYPREFIX_BINDTYPE << ", key=" << nodeId
        << ", bind type=" << valueStr << ".";
    // 3. 数据同步到主节点的数据库中
    std::vector<std::string> nodeIdList;
    // 查询主节点
    ubse::election::UbseRoleInfo masterRole;
    ret = ubse::election::UbseGetMasterInfo(masterRole);
    if (ret != 0) {
        delete[] syncData.data;
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG_DB << "UbseGetMasterInfo failed.";
        return MEM_POOLING_ERROR;
    }
    nodeIdList.push_back(masterRole.nodeId);
    ret = sync::MpSyncDataHelper::Instance().SyncDataToNode(nodeIdList, message::OPCODE_SYNC_BIND_TYPE_DATA_TO_NODE,
                                                            syncData);
    delete[] syncData.data;
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG_DB << "UpdateBindTypeDB SyncDataToNode failed.";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG_DB << "UpdateBindTypeDB SyncDataToNode success.";
    return MEM_POOLING_OK;
}

MpResult OverCommitStorage::SelectBindTypeDB()
{
    std::lock_guard<std::mutex> lock(mtx);
    mNodeNumaBindMap.clear();
    JSON_MAP jsonMap;
    auto ret = UbseStorageQueryData(KEYPREFIX, KEYPREFIX_BINDTYPE, static_cast<void *>(&jsonMap), GetBindTypePair);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[PersistentStore][VmInfosCompleted] RackStorageQueryAllData fail, key=" << KEYPREFIX_BINDTYPE << ".";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "jsonMap size=" << jsonMap.size() << ".";

    for (auto &pair : jsonMap) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "nodeId="
            << pair.first << "bindType=" << pair.second << ".";
        mNodeNumaBindMap[pair.first] = StrToBindType(pair.second);
    }

    return MEM_POOLING_OK;
}

MpResult OverCommitStorage::GetNumaBindType(const std::string &nodeId, NumaBindType &value)
{
    auto iter = mNodeNumaBindMap.find(nodeId);
    if (iter == mNodeNumaBindMap.end()) {
        SelectBindTypeDB();
    }
    iter = mNodeNumaBindMap.find(nodeId);
    if (iter == mNodeNumaBindMap.end()) {
        value = NumaBindType::BIND_INVALID;
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG_DB << " GetNumaBindType failed, can not find nodeId=" << nodeId << " in DB.";
        return MEM_POOLING_ERROR;
    }

    value = iter->second;
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG_DB << " GetNumaBindType success, nodeId=" << nodeId << ", value=" << BindTypeToStr(value) << ".";
    return MEM_POOLING_OK;
}
MpResult OverCommitStorage::UpdateWaterMark(const uint16_t highWaterMark, const uint16_t lowWaterMark)
{
    MpResult ret = MEM_POOLING_ERROR;
    JSON_MAP strMap;
    strMap[HIGH_WATER_MAKR] = std::to_string(highWaterMark);
    strMap[LOW_WATER_MAKR] = std::to_string(lowWaterMark);
    JSON_STR value;
    JsonUtil::RackMemConvertMap2JsonStr(strMap, value);
    // 2. 持久化更新到rack数据库中
    UbseByteBuffer buffer{};
    size_t size = value.size();
    buffer.data = new (std::nothrow) uint8_t[size];
    if (buffer.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Failed to allocate memory, size=" << size << ".";
        return MEM_POOLING_ERROR;
    }
    buffer.len = size;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << TAG_DB << "waterMark size=" << size << ".";
    auto mcpyRet = memcpy_s(buffer.data, size, value.c_str(), size);
    if (mcpyRet != EOK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG_DB << "Memcpy_s failed, ret=" << mcpyRet << ", value=" << value << ", size=" << size << " .";
        SafeDeleteArray(buffer.data);
        return MEM_POOLING_ERROR;
    }
    ret = UbseStoragePutData(KEYPREFIX, KEYPREFIX_WATERMARK, &buffer);
    SafeDeleteArray(buffer.data);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG_DB << "UbseStoragePutData failed keyPrefix=" << KEYPREFIX << ", key=" << KEYPREFIX_WATERMARK
            << ", ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult OverCommitStorage::GetWaterMark(uint16_t &highWaterMark, uint16_t &lowWaterMark)
{
    std::unordered_map<std::string, uint16_t> map;
    auto ret = UbseStorageQueryData(KEYPREFIX, KEYPREFIX_WATERMARK, static_cast<void *>(&map), GetWaterMarkPair);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG_DB << "Query Water Mark Error.";
        return MEM_POOLING_ERROR;
    }
    auto iter = map.find(HIGH_WATER_MAKR);
    if (iter == map.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG_DB << "Query HIGH_WATER_MAKR Mark Error.";
        return MEM_POOLING_ERROR;
    }
    highWaterMark = iter->second;
    iter = map.find(LOW_WATER_MAKR);
    if (iter == map.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << TAG_DB << "Query LOW_WATER_MAKR Mark Error.";
        return MEM_POOLING_ERROR;
    }
    lowWaterMark = iter->second;
    if (highWaterMark > HIGH_WATER_MARK_THRESHOLD || highWaterMark <= lowWaterMark) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[SetWaterMark] WaterMark is invalid.";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult OverCommitStorage::UpdateUint16(const std::string &keyPrefix, const std::string &key, const uint16_t &value)
{
    UbseByteBuffer buffer{};
    auto highWaterMarkStr = std::to_string(value);
    size_t size = highWaterMarkStr.size();
    buffer.data = new (std::nothrow) uint8_t[size];
    if (buffer.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[Failed to allocate memory, size=" << size << ".";
        return MEM_POOLING_ERROR;
    }
    auto mcpyRet = memcpy_s(buffer.data, size, highWaterMarkStr.c_str(), size);
    if (mcpyRet != EOK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
        << TAG_DB << "Memcpy_s failed, ret=" << mcpyRet << ", value=" << highWaterMarkStr << ", size=" << size << " .";
        SafeDeleteArray(buffer.data);
        return MEM_POOLING_ERROR;
    }

    buffer.len = size;
    auto ret = UbseStoragePutData(keyPrefix, key, &buffer);
    // 释放申请的内存
    SafeDeleteArray(buffer.data);

    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << TAG_DB << "UbseStoragePutData failed keyPrefix=" << KEYPREFIX_WATERMARK << ", key="
            << HIGH_WATER_MAKR << ", bind type=" << highWaterMarkStr << ", ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

std::unordered_map<std::string, NumaBindType>& OverCommitStorage::getNodeNumaBindMap()
{
    return mNodeNumaBindMap;
}
} // namespace mempooling