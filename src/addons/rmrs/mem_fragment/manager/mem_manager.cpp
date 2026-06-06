
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

#include "mem_manager.h"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>

#include <rapidjson/document.h>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <vector>
#include "ubse_def.h"
#include "ubse_error.h"
#include "ubse_logger.h"
#include "ubse_mem_controller.h"
#include "ubse_node.h"
#include "ubse_pointer_process.h"
#include "ubse_storage.h"
#include "mempooling_return_module.h"
#include "mp_configuration.h"
#include "mp_error.h"
#include "mp_json_util.h"
#include "mp_mem_json_util.h"
#include "mp_parse_util.h"
#include "mp_sync_data_helper.h"
#include "over_commit_msg_handler.h"
#include "rmrs_serialize.h"
#include "securec.h"

namespace mempooling {
using namespace ubse::log;
using namespace ubse::storage;
using namespace ubse::nodeController;
using namespace rmrs::serialize;
using namespace ubse::mem::controller;

const std::string MEM_CONFIG_FILE = "/usr/local/softbus/ctrlbus/conf/plugin_mem_master.conf";
const std::string KEYPREFIX_REDIRECT = "/rack.mp.memmanager.redirect";
const std::string KEYPREFIX_NAME2VMINFO = "/rack.mp.memmanager.name_2_vm_info";
const std::string KEYPREFIX_BORROWID_COMPLETED = "_borrow_id_completed";
const std::string KEYPREFIX_VMINFO_COMPLETED = "_vm_info_completed";
const std::string KEYPREFIX_COMMON = "mempooling";
const std::string KEYPREFIX_SMAPENABLE_COMPLETED = "_smap_enable_completed";
const std::string KEYPREFIX_REMOVEPID_COMPLETED = "_remove_pid_completed";
const std::string KEYPREFIX_FAULT_PROCESS_BORROWID = "_fault_process_borrowid";
const std::string KEYPREFIX_BORROWED_DECISION = "_borrowed_decision";

const int HEADER_LENGTH = 4;
const int TIMESTAMP_OFFSET = 2;
constexpr int ONE_HUNDRD_PERCENT = 100;
constexpr uint64_t KB_TO_BYTES = 1024;
constexpr uint64_t MB_TO_KBYTES = 1024;
constexpr uint64_t HUGE_PAGE_FREE_NUM_TO_BYTES = 2 * 1024 * 1024;
constexpr uint64_t PAGE_64K_BYTES = 64 * 1024;
constexpr uint64_t HUGE_PAGE_NUM_4K_TO_KB = 2 * 1024; // 4k标准页场景下pmd配置为2M大页可以用于被借出
constexpr uint64_t HUGE_PAGE_NUM_64K_TO_KB = 512 * 1024; // 64k标准页场景下pmd配置为512M大页可以用于被借出
constexpr uint64_t NUM_TO_RATIO = 100;
constexpr uint16_t TIMEOUT_CYCLES_LIMIT = 300; // 超时周期上限

#define LOG_DEBUG UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_ERROR UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_INFO UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
#define LOG_WARN UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)

MpResult BorrowRecordHelper::Init()
{
    MpResult ret = UpdateBorrowRecords();
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "Failed to init gBorrowRecords, memManager init failed.";
        return ret;
    }
    LOG_DEBUG << "MemManager init success. gBorrowRecords size: " << gBorrowRecords.size();
    for (const auto& record : gBorrowRecords) {
        LOG_DEBUG << "" << record.ToString();
    }
    return MEM_POOLING_OK;
}

MpResult MemManager::GetNodeMemInfo(const std::string& nodeId, NodeMemInfo& outInfo) const
{
    std::lock_guard<std::mutex> lock(mtx);
    MpResult ret = MemManager::Instance().InitBorrowableInfo();
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "Init borrowableInfo failed.";
        return ret;
    }
    auto it = nodeMemMap.find(nodeId);
    if (it == nodeMemMap.end()) {
        LOG_ERROR << "The nodeMemInfo of node" << nodeId << "does not exist";
        return MEM_POOLING_ERROR;
    }

    outInfo = it->second;
    return MEM_POOLING_OK;
}

MpResult MemManager::GetNodeMemMap(std::unordered_map<std::string, NodeMemInfo>& outMap) const
{
    std::lock_guard<std::mutex> lock(mtx);
    auto start = std::chrono::steady_clock::now();
    MpResult ret = MemManager::Instance().InitBorrowableInfo();
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "Init borrowableInfo failed.";
        return ret;
    }
    auto cost = std::chrono::steady_clock::now() - start;
    constexpr auto MAX_EXEC_TIME = std::chrono::seconds(TIMESTAMP_OFFSET * TIMEOUT_CYCLES_LIMIT);
    if (cost > MAX_EXEC_TIME) {
        LOG_ERROR << "[MemBorrow][MemBorrowStrategy] InitBorrowableInfo execution timeout"
                  << ", cost_sec=" << std::chrono::duration_cast<std::chrono::seconds>(cost).count();
        return MEM_POOLING_ERROR;
    }
    outMap = nodeMemMap;
    return MEM_POOLING_OK;
}

MpResult AntiNode::BuildSyncAntiNode(const UbseByteBuffer& buffer, UbseByteBuffer& syncData)
{
    // 直接构造 syncData，避免 GetAntiNodeRawData 再去读
    syncData.len = buffer.len;
    syncData.data = new (std::nothrow) uint8_t[syncData.len];
    if (syncData.data == nullptr) {
        LOG_ERROR << "[MemBorrow][AntiAffinity] The anti-affinity information syncData field is empty.";
        syncData.len = 0;
        return MEM_POOLING_ERROR;
    }
    errno_t err = memcpy_s(syncData.data, syncData.len, buffer.data, buffer.len);
    if (err != EOK) {
        LOG_ERROR << "[MemBorrow][AntiAffinity] Failed to populate the anti-affinity syncData field.";
        SafeDeleteArray(syncData.data);
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult AntiNode::Update(const std::map<std::string, std::vector<std::string>>& nodeAntiMap)
{
    std::unique_lock<std::mutex> lock(mtxAnti);
    UbseByteBuffer antiData;
    MpUpdateAntiNodeParam tempNodeAntiMap;
    tempNodeAntiMap.nodeAntiAffinityMap = nodeAntiMap;
    std::string tempNodeAntiMapStr = tempNodeAntiMap.ToJson();
    antiData.len = tempNodeAntiMapStr.length();
    antiData.data = new (std::nothrow) uint8_t[antiData.len];
    if (antiData.data == nullptr) {
        LOG_ERROR << "[MemBorrow][AntiAffinity] The anti-affinity information data field is empty.";
        antiData.len = 0;
        return MEM_POOLING_ERROR;
    }
    antiData.freeFunc = [](uint8_t* data) {
        delete[] data;
    };
    errno_t err = memcpy_s(antiData.data, antiData.len, tempNodeAntiMapStr.c_str(), tempNodeAntiMapStr.length());
    if (err != EOK) {
        SafeDeleteArray(antiData.data);
        return MEM_POOLING_ERROR;
    }
    MpResult ret = UbseStoragePutData("mempooling", "_antiNode", &antiData);
    if (ret != MEM_POOLING_OK) {
        SafeDeleteArray(antiData.data);
        LOG_ERROR << "[MemBorrow][AntiAffinity] Failed to store the anti-affinity information to ubse.";
        return ret;
    }
    antiData.freeFunc(antiData.data);
    antiData.data = nullptr;
    antiNodeParam = tempNodeAntiMap;

    UbseByteBuffer syncData;
    SyncUpdateAntiNodeParam syncParam{"mempooling", "_antiNode", nodeAntiMap};
    RmrsOutStream builder;
    builder << syncParam;
    syncData.len = builder.GetSize();
    syncData.data = builder.GetBufferPointer();
    std::vector<std::string> nodeIdList;
    // 查询主节点
    UbseRoleInfo masterRole;
    ret = ubse::election::UbseGetMasterInfo(masterRole);
    if (ret != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][AntiAffinity] UbseGetMasterInfo failed.";
        delete[] syncData.data;
        return MEM_POOLING_ERROR;
    }
    nodeIdList.push_back(masterRole.nodeId);
    ret = sync::MpSyncDataHelper::Instance().SyncDataToNode(nodeIdList, message::OPCODE_SYNC_DATA_TO_NODE, syncData);
    delete[] syncData.data;
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][AntiAffinity] SyncDataToNode failed.";
        return MEM_POOLING_ERROR;
    }
    lock.unlock();
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemBorrow][AntiAffinity] Update antinode success";
    return MEM_POOLING_OK;
}

MpResult AntiNode::Query(const std::string& srcNid, std::vector<std::string>& antiNodeMemVec)
{
    std::lock_guard<std::mutex> lock(mtxAnti);

    if (antiNodeParam.nodeAntiAffinityMap.empty()) {
        uint32_t ret = AntiDataReload();
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "Failed to reload anti data";
            return ret;
        }
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemBorrow][AntiAffinity] Recover antidata from ubse database success.";
    }

    if (antiNodeParam.nodeAntiAffinityMap.find(srcNid) != antiNodeParam.nodeAntiAffinityMap.end()) {
        antiNodeMemVec = antiNodeParam.nodeAntiAffinityMap[srcNid];
    } else {
        antiNodeMemVec = {};
    }
    return MEM_POOLING_OK;
}

MpResult AntiNode::GetRawData(UbseByteBuffer& buffer, bool needLock)
{
    LOG_DEBUG << "[MemBorrow][AntiAffinity] GetAntiNodeRawData start.";
    std::unique_lock<std::mutex> locker(mtxAnti, std::defer_lock);
    if (needLock) {
        locker.lock();
    }
    UbseByteBuffer ctx{};
    MpResult ret = UbseStorageQueryData("mempooling", "_antiNode", &ctx, LoadDataBase);
    if (ret != MEM_POOLING_OK) {
        if (ctx.data != nullptr) {
            delete[] ctx.data;
        }
        return MEM_POOLING_OK;
    }
    if (ctx.len == 0) {
        return MEM_POOLING_OK;
    }
    buffer.data = ctx.data;
    buffer.len = ctx.len;
    LOG_DEBUG << "[MemBorrow][AntiAffinity] GetAntiNodeRawData end.";
    return MEM_POOLING_OK;
}

MpResult AntiNode::PutRawData(UbseByteBuffer& buffer)
{
    LOG_DEBUG << "PutAntiNodeRawData start.";
    std::lock_guard<std::mutex> locker(mtxAnti);
    MpResult ret = UbseStoragePutData("mempooling", "_antiNode", &buffer);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "UbseStoragePutData failed.";
        return MEM_POOLING_ERROR;
    }
    // 先将数据做处理
    if (buffer.data == nullptr) {
        LOG_ERROR << "buffer.data is nullptr.";
        return MEM_POOLING_ERROR;
    }
    std::string jsonStr(static_cast<char*>(static_cast<void*>(buffer.data)), buffer.len);
    MpUpdateAntiNodeParam param;
    bool flag = param.FromJson(jsonStr);
    if (!flag) {
        LOG_ERROR << "antinodeParam deserialization  failed.";
        return MEM_POOLING_ERROR;
    }
    SetAntiNodeParam(param);
    LOG_DEBUG << "PutAntiNodeRawData end.";
    return MEM_POOLING_OK;
}

MpResult BorrowIdRedirection::Update(const std::string key, const std::string value)
{
    LOG_DEBUG << "[PersistentStore][BorrowIdRedirection] Update of redirection borrowId started.";
    std::unique_lock<std::mutex> lock(mtxBorrowIdRedirect);
    borrowIdRedirectionMap[key] = value;
    UbseByteBuffer buffer{};
    RmrsOutStream builder;
    builder << borrowIdRedirectionMap;
    buffer.len = builder.GetSize();
    buffer.data = builder.GetBufferPointer();
    auto ret = UbseStoragePutData("mempooling", "_borrowidredirection", &buffer);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][BorrowIdRedirection] Failed to store redirection borrowId data to RackManager "
                     "database, old borrowId ="
                  << key << ", new borrowId =" << value << ", ret " << ret;
        delete[] buffer.data;
        return MEM_POOLING_ERROR;
    }
    LOG_DEBUG << "[PersistentStore][BorrowIdRedirection] Successfully stored redirection borrowId data to RackManager "
                 "database, old borrowId ="
              << key << ", new borrowId =" << value << ", ret " << ret;
    SafeDeleteArray(buffer.data);
    lock.unlock();

    LOG_DEBUG << "[PersistentStore][BorrowIdRedirection] Update of redirection borrowId finished.";

    return MEM_POOLING_OK;
}

MpResult BorrowIdsCompleted::Update(const std::string borrowId)
{
    LOG_DEBUG << "[PersistentStore][BorrowIdsCompleted] Update of completed borrowIds started, borrow_id=" << borrowId
              << ".";

    std::unique_lock<std::mutex> lock(mtxBorrowIdsCompleted);
    RmrsOutStream builder;
    LOG_DEBUG << "[PersistentStore] [BorrowIdsCompleted] Old BorrowIdsCompleted size = " << borrowIdsCompleted.size();
    for (const auto& item : borrowIdsCompleted) {
        LOG_DEBUG << "[PersistentStore] [BorrowIdsCompleted] Old borrowIdsCompleted  = " << item;
    }
    (void)borrowIdsCompleted.insert(borrowId);
    builder << borrowIdsCompleted;
    LOG_DEBUG << "[PersistentStore] [BorrowIdsCompleted] New BorrowIdsCompleted size = " << borrowIdsCompleted.size();
    for (const auto& item : borrowIdsCompleted) {
        LOG_DEBUG << "[PersistentStore] [BorrowIdsCompleted] New borrowIdsCompleted  = " << item;
    }
    UbseByteBuffer buffer = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_BORROWID_COMPLETED, &buffer);
    if (ret != MEM_POOLING_OK) {
        SafeDeleteArray(buffer.data);
        LOG_ERROR << "[PersistentStore][BorrowIdsCompleted] Failed to store "
                     "completed borrowIds to mxe database, ret="
                  << ret << ".";
        return MEM_POOLING_ERROR;
    }
    LOG_DEBUG << "[PersistentStore][BorrowIdsCompleted] Successfully stored "
                 "completed borrowIds data to mxe database.";
    SafeDeleteArray(buffer.data);
    lock.unlock();

    LOG_DEBUG << "[PersistentStore][BorrowIdsCompleted] Update of completed borrowIds finished.";
    return MEM_POOLING_OK;
}

MpResult SmapEnableCompleted::Update(const int16_t numaId)
{
    LOG_DEBUG << "[PersistentStore][SmapEnableCompleted] Update of completed smapEnable started, numaId=" << numaId
              << ".";

    std::unique_lock<std::mutex> lock(mtxSmapEnableCompleted);
    RmrsOutStream builder;
    LOG_DEBUG << "[PersistentStore] [SmapEnableCompleted] Old smapEnableCompleted size = " << smapEnableCompleted.size()
              << ".";
    for (const auto& item : smapEnableCompleted) {
        LOG_DEBUG << "[PersistentStore] [SmapEnableCompleted] Old smapEnableCompleted  = " << item;
    }
    smapEnableCompleted.insert(numaId);
    builder << smapEnableCompleted;
    LOG_DEBUG << "[PersistentStore] [SmapEnableCompleted] New smapEnableCompleted size = " << smapEnableCompleted.size()
              << ".";
    for (const auto& item : smapEnableCompleted) {
        LOG_DEBUG << "[PersistentStore] [SmapEnableCompleted] New smapEnableCompleted  = " << item << ".";
    }
    UbseByteBuffer buffer = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_SMAPENABLE_COMPLETED, &buffer);
    if (ret != MEM_POOLING_OK) {
        SafeDeleteArray(buffer.data);
        LOG_ERROR << "[PersistentStore][SmapEnableCompleted] Failed to store "
                     "completed smapEnable to mxe database, ret="
                  << ret << ".";
        return MEM_POOLING_ERROR;
    }
    LOG_DEBUG << "[PersistentStore][SmapEnableCompleted] Successfully stored "
                 "completed smapEnable data to mxe database.";
    SafeDeleteArray(buffer.data);
    lock.unlock();

    LOG_DEBUG << "[PersistentStore][SmapEnableCompleted] Update of completed smapEnable numaId finished.";
    return MEM_POOLING_OK;
}

MpResult FaultHandleBorrowedDecision::Update(const uint16_t numaId, const BorrowedDecision& decision)
{
    LOG_DEBUG << "[FaultHandleBorrowedDecision] FaultHandleBorrowedDecision Update for numaId=" << numaId << ".";

    std::unique_lock<std::mutex> lock(mtxBorrowedDecision);
    // 覆盖，这里直接替换该numaId对应的所有决策
    borrowedDecisionMap[numaId] = decision;

    // 持久化
    RmrsOutStream builder;
    builder << borrowedDecisionMap;
    UbseByteBuffer buffer = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_BORROWED_DECISION, &buffer);
    SafeDeleteArray(buffer.data);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultHandleBorrowedDecision] Failed to store borrowed decisions, ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    lock.unlock();
    LOG_DEBUG << "[FaultHandleBorrowedDecision] Update success.";
    return MEM_POOLING_OK;
}

MpResult BorrowIdInFaultProcess::Update(const std::string borrowId)
{
    LOG_DEBUG << "[PersistentStore][BorrowIdInFaultProcess] Update of borrowId in fault started, numaId=" << borrowId
              << ".";

    std::unique_lock<std::mutex> lock(mtxBorrowIdInFaultProcess);
    RmrsOutStream builder;
    LOG_DEBUG << "[PersistentStore] [BorrowIdInFaultProcess] Old BorrowIdInFaultProcess size = "
              << borrowIdInFaultProcess.size() << ".";
    for (const auto& item : borrowIdInFaultProcess) {
        LOG_DEBUG << "[PersistentStore] [BorrowIdInFaultProcess] Old BorrowIdInFaultProcess  = " << item;
    }
    borrowIdInFaultProcess.insert(borrowId);
    builder << borrowIdInFaultProcess;
    LOG_DEBUG << "[PersistentStore] [BorrowIdInFaultProcess] New borrowIdInFaultProcess size = "
              << borrowIdInFaultProcess.size() << ".";
    for (const auto& item : borrowIdInFaultProcess) {
        LOG_DEBUG << "[PersistentStore] [BorrowIdInFaultProcess] New borrowIdInFaultProcess  = " << item << ".";
    }
    UbseByteBuffer buffer = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_FAULT_PROCESS_BORROWID, &buffer);
    if (ret != MEM_POOLING_OK) {
        SafeDeleteArray(buffer.data);
        LOG_ERROR << "[PersistentStore][BorrowIdInFaultProcess] Failed to store "
                     "fault process borrowId to mxe database, ret="
                  << ret << ".";
        return MEM_POOLING_ERROR;
    }
    LOG_DEBUG << "[PersistentStore][BorrowIdInFaultProcess] Successfully stored "
                 "fault process borrowId data to mxe database.";
    SafeDeleteArray(buffer.data);
    lock.unlock();

    LOG_DEBUG << "[PersistentStore][BorrowIdInFaultProcess] Update of fault process borrowId finished.";
    return MEM_POOLING_OK;
}

MpResult RemovePidCompleted::Update(const uint16_t numaId, const std::vector<pid_t> pids)
{
    for (auto pid : pids) {
        LOG_DEBUG << "[PersistentStore][RemovePidCompleted] Update of completed removePid started, pid=" << pid
                  << ", numaId=" << numaId << ".";
    }

    std::unique_lock<std::mutex> lock(mtxRemovePidCompleted);

    LOG_DEBUG << "[PersistentStore][RemovePidCompleted] Old map size=" << removePidCompleted.size() << ".";
    for (const auto& kv : removePidCompleted) {
        for (auto pid : kv.second) {
            LOG_DEBUG << "[PersistentStore][RemovePidCompleted] OldEntry numa=" << kv.first << " pid=" << pid << ".";
        }
    }

    auto& setRef = removePidCompleted[numaId];
    for (auto pid : pids) {
        setRef.insert(pid);
    }

    RmrsOutStream builder;
    builder << removePidCompleted;
    LOG_DEBUG << "[PersistentStore] [RemovePidCompleted]  New map size=" << removePidCompleted.size() << ".";
    for (const auto& kv : removePidCompleted) {
        for (auto pid : kv.second) {
            LOG_DEBUG << "[PersistentStore][RemovePidCompleted] NewEntry numa=" << kv.first << " pid=" << pid << ".";
        }
    }
    UbseByteBuffer buffer = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_REMOVEPID_COMPLETED, &buffer);
    if (ret != MEM_POOLING_OK) {
        SafeDeleteArray(buffer.data);
        LOG_ERROR << "[PersistentStore][RemovePidCompleted] Failed to store "
                     "completed removePid to mxe database, ret="
                  << ret << ".";
        return MEM_POOLING_ERROR;
    }
    LOG_DEBUG << "[PersistentStore][RemovePidCompleted] Successfully stored "
                 "completed removePid data to mxe database.";
    SafeDeleteArray(buffer.data);
    lock.unlock();

    LOG_DEBUG << "[PersistentStore][RemovePidCompleted] Update of completed removePid finished.";
    return MEM_POOLING_OK;
}

MpResult SmapEnableCompleted::Remove(const int16_t numaId)
{
    LOG_DEBUG << "[PersistentStore][SmapEnableCompleted] Remove SmapEnableCompleted start, numaId=" << numaId << ".";

    std::unique_lock<std::mutex> lock(mtxSmapEnableCompleted);
    LOG_DEBUG << "[PersistentStore] [SmapEnableCompleted] Old smapEnableCompleted size = "
              << smapEnableCompleted.size();
    for (const auto& item : smapEnableCompleted) {
        LOG_DEBUG << "[PersistentStore] [SmapEnableCompleted] Old smapEnableCompleted  = " << item << ".";
    }
    auto it = smapEnableCompleted.find(numaId);
    if (it != smapEnableCompleted.end()) {
        smapEnableCompleted.erase(numaId);
    }
    LOG_DEBUG << "[PersistentStore] [SmapEnableCompleted] New smapEnableCompleted size = "
              << smapEnableCompleted.size();
    for (const auto& item : smapEnableCompleted) {
        LOG_DEBUG << "[PersistentStore] [SmapEnableCompleted] New smapEnableCompleted  = " << item << ".";
    }
    RmrsOutStream builder;
    builder << smapEnableCompleted;
    UbseByteBuffer buffer = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_SMAPENABLE_COMPLETED, &buffer);
    if (ret != MEM_POOLING_OK) {
        SafeDeleteArray(buffer.data);
        LOG_ERROR << "[PersistentStore][SmapEnableCompleted] Remove smapEnable numaId by scbus failed.";
        return MEM_POOLING_ERROR;
    }
    SafeDeleteArray(buffer.data);
    lock.unlock();

    LOG_DEBUG << "[PersistentStore][SmapEnableCompleted] Remove smapEnable numaId ended.";
    return MEM_POOLING_OK;
}

MpResult FaultHandleBorrowedDecision::Remove(const uint16_t numaId)
{
    LOG_DEBUG << "[FaultHandleBorrowedDecision] Remove for numaId=" << numaId << ".";

    std::unique_lock<std::mutex> lock(mtxBorrowedDecision);
    auto it = borrowedDecisionMap.find(numaId);
    if (it == borrowedDecisionMap.end()) {
        LOG_DEBUG << "[FaultHandleBorrowedDecision] numaId=" << numaId << " not found, nothing to remove.";
        return MEM_POOLING_OK;
    }
    borrowedDecisionMap.erase(it);

    // 持久化
    RmrsOutStream builder;
    builder << borrowedDecisionMap;
    UbseByteBuffer buffer = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_BORROWED_DECISION, &buffer);
    SafeDeleteArray(buffer.data);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultHandleBorrowedDecision] Failed to store after remove, ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    lock.unlock();
    LOG_DEBUG << "[FaultHandleBorrowedDecision] Remove success.";
    return MEM_POOLING_OK;
}

MpResult BorrowIdInFaultProcess::Remove(const std::string borrowId)
{
    LOG_DEBUG << "[PersistentStore][BorrowIdInFaultProcess] Remove borrowIdInFaultProcess start, numaId=" << borrowId
              << ".";

    std::unique_lock<std::mutex> lock(mtxBorrowIdInFaultProcess);
    LOG_DEBUG << "[PersistentStore] [BorrowIdInFaultProcess] Old borrowIdInFaultProcess size = "
              << borrowIdInFaultProcess.size();
    for (const auto& item : borrowIdInFaultProcess) {
        LOG_DEBUG << "[PersistentStore] [BorrowIdInFaultProcess] Old borrowIdInFaultProcess  = " << item << ".";
    }
    auto it = borrowIdInFaultProcess.find(borrowId);
    if (it != borrowIdInFaultProcess.end()) {
        borrowIdInFaultProcess.erase(borrowId);
    }
    LOG_DEBUG << "[PersistentStore] [BorrowIdInFaultProcess] New borrowIdInFaultProcess size = "
              << borrowIdInFaultProcess.size();
    for (const auto& item : borrowIdInFaultProcess) {
        LOG_DEBUG << "[PersistentStore] [BorrowIdInFaultProcess] New borrowIdInFaultProcess  = " << item << ".";
    }
    RmrsOutStream builder;
    builder << borrowIdInFaultProcess;
    UbseByteBuffer buffer = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_FAULT_PROCESS_BORROWID, &buffer);
    if (ret != MEM_POOLING_OK) {
        SafeDeleteArray(buffer.data);
        LOG_ERROR << "[PersistentStore][BorrowIdInFaultProcess] Remove fault process borrowId by scbus failed.";
        return MEM_POOLING_ERROR;
    }
    SafeDeleteArray(buffer.data);
    lock.unlock();

    LOG_DEBUG << "[PersistentStore][BorrowIdInFaultProcess] Remove fault process borrowId ended.";
    return MEM_POOLING_OK;
}

MpResult BorrowIdInFaultProcess::Clear()
{
    LOG_DEBUG << "[PersistentStore][BorrowIdInFaultProcess] Clear borrowIdInFaultProcess start.";

    std::unique_lock<std::mutex> lock(mtxBorrowIdInFaultProcess);
    LOG_DEBUG << "[PersistentStore] [BorrowIdInFaultProcess] Old borrowIdInFaultProcess size = "
              << borrowIdInFaultProcess.size();
    for (const auto& item : borrowIdInFaultProcess) {
        LOG_DEBUG << "[PersistentStore] [BorrowIdInFaultProcess] Old borrowIdInFaultProcess  = " << item << ".";
    }

    borrowIdInFaultProcess.clear();
    LOG_DEBUG << "[PersistentStore] [BorrowIdInFaultProcess] New borrowIdInFaultProcess size = "
              << borrowIdInFaultProcess.size();
    RmrsOutStream builder;
    builder << borrowIdInFaultProcess;
    UbseByteBuffer buffer = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_FAULT_PROCESS_BORROWID, &buffer);
    if (ret != MEM_POOLING_OK) {
        SafeDeleteArray(buffer.data);
        LOG_ERROR << "[PersistentStore][BorrowIdInFaultProcess] Clear fault process borrowId by scbus failed.";
        return MEM_POOLING_ERROR;
    }
    SafeDeleteArray(buffer.data);
    lock.unlock();

    LOG_DEBUG << "[PersistentStore][BorrowIdInFaultProcess] Clear fault process borrowId ended.";
    return MEM_POOLING_OK;
}

MpResult RemovePidCompleted::Remove(const uint16_t numaId, const std::vector<pid_t> pids)
{
    for (auto pid : pids) {
        LOG_DEBUG << "[PersistentStore][RemovePidCompleted] Remove start"
                  << " numa=" << numaId << " pid=" << pid << ".";
    }
    std::unique_lock<std::mutex> lock(mtxRemovePidCompleted);

    LOG_DEBUG << "[PersistentStore][RemovePidCompleted] Old map size=" << removePidCompleted.size() << ".";
    auto it = removePidCompleted.find(numaId);
    if (it != removePidCompleted.end()) {
        for (auto pid : pids) {
            if (it->second.erase(pid)) {
                LOG_DEBUG << "[PersistentStore][RemovePidCompleted] Removed pid=" << pid << " numa=" << numaId << ".";
            }
        }

        if (it->second.empty()) {
            // key=numa对应的value=pids为空时，就把这个key也从map中删除掉
            removePidCompleted.erase(it);
            LOG_DEBUG << "[PersistentStore][RemovePidCompleted] Erased empty numa=" << numaId << ".";
        }
    }

    LOG_DEBUG << "[PersistentStore][RemovePidCompleted] New map size=" << removePidCompleted.size() << ".";
    for (const auto& kv : removePidCompleted) {
        for (auto pid : kv.second) {
            LOG_DEBUG << "[PersistentStore][RemovePidCompleted] NewEntry numa=" << kv.first << " pid=" << pid << ".";
        }
    }

    RmrsOutStream builder;
    builder << removePidCompleted;
    UbseByteBuffer buffer = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_REMOVEPID_COMPLETED, &buffer);
    if (ret != MEM_POOLING_OK) {
        SafeDeleteArray(buffer.data);
        LOG_ERROR << "[PersistentStore][RemovePidCompleted] Remove RemovePid by scbus failed.";
        return MEM_POOLING_ERROR;
    }
    SafeDeleteArray(buffer.data);
    lock.unlock();

    LOG_DEBUG << "[PersistentStore][RemovePidCompleted] Remove RemovePid ended.";
    return MEM_POOLING_OK;
}

MpResult BorrowIdsCompleted::Remove(const std::string borrowId)
{
    LOG_DEBUG << "[PersistentStore][BorrowIdsCompleted] RemoveBorrowIdCompleted start, borrow_id=" << borrowId << ".";

    std::unique_lock<std::mutex> lock(mtxBorrowIdsCompleted);
    LOG_DEBUG << "[PersistentStore] [BorrowIdsCompleted] Old BorrowIdsCompleted size = " << borrowIdsCompleted.size();
    for (const auto& item : borrowIdsCompleted) {
        LOG_DEBUG << "[PersistentStore] [BorrowIdsCompleted] Old borrowIdsCompleted  = " << item;
    }
    auto it = borrowIdsCompleted.find(borrowId);
    if (it != borrowIdsCompleted.end()) {
        borrowIdsCompleted.erase(borrowId);
    }
    LOG_DEBUG << "[PersistentStore] [BorrowIdsCompleted] New BorrowIdsCompleted size = " << borrowIdsCompleted.size();
    for (const auto& item : borrowIdsCompleted) {
        LOG_DEBUG << "[PersistentStore] [BorrowIdsCompleted] New borrowIdsCompleted  = " << item;
    }
    RmrsOutStream builder;
    builder << borrowIdsCompleted;
    UbseByteBuffer buffer = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_BORROWID_COMPLETED, &buffer);
    if (ret != MEM_POOLING_OK) {
        SafeDeleteArray(buffer.data);
        LOG_ERROR << "[PersistentStore][BorrowIdsCompleted] Remove borrowId by scbus failed.";
        return MEM_POOLING_ERROR;
    }
    SafeDeleteArray(buffer.data);
    lock.unlock();

    LOG_DEBUG << "[PersistentStore][BorrowIdsCompleted] Remove ended.";
    return MEM_POOLING_OK;
}

MpResult VmInfosCompleted::Update(const pid_t pid, std::string remoteNumaId, std::string borrowInNode)
{
    LOG_DEBUG << "[PersistentStore][VmInfosCompleted] Update of completed vmInfos started, pid=" << pid << ".";

    std::unique_lock<std::mutex> lock(mtxVmInfosCompleted);

    std::string value = borrowInNode + "-" + remoteNumaId;
    LOG_DEBUG << "[PersistentStore] [VmInfosCompleted] Old VmInfosCompleted size = " << vmMap.size();
    for (const auto& item : vmMap) {
        LOG_DEBUG << "[PersistentStore] [VmInfosCompleted] Old pid  = " << item.first << ", value = " << item.second
                  << ".";
    }
    vmMap[pid] = value;
    LOG_DEBUG << "[PersistentStore] [VmInfosCompleted] New VmInfosCompleted size = " << vmMap.size();
    for (const auto& item : vmMap) {
        LOG_DEBUG << "[PersistentStore] [VmInfosCompleted] New pid  = " << item.first << ", value = " << item.second
                  << ".";
    }

    RmrsOutStream builder;
    builder << vmMap;
    UbseByteBuffer buffer = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_VMINFO_COMPLETED, &buffer);
    if (ret != MEM_POOLING_OK) {
        SafeDeleteArray(buffer.data);
        LOG_ERROR << "[PersistentStore][VmInfosCompleted] Failed to store "
                     "completed vmInfos data to RackManager database, ret="
                  << ret << ".";
        return MEM_POOLING_ERROR;
    }
    LOG_DEBUG
        << "[PersistentStore][VmInfosCompleted] Successfully stored completed vmInfos data to RackManager database.";
    SafeDeleteArray(buffer.data);
    lock.unlock(); // 在持久化之后，数据同步之前释放mempooling的数据锁，避免与rackManager的服务锁形成死锁

    LOG_DEBUG << "[PersistentStore][VmInfosCompleted] Update of completed vmInfos finished.";

    return MEM_POOLING_OK;
}

MpResult VmInfosCompleted::Remove(const pid_t pid)
{
    std::string pidStr = std::to_string(pid);

    LOG_DEBUG << "[PersistentStore][VmInfosCompleted] Removal of completed vmInfos started, pid=" << pidStr << ".";

    std::unique_lock<std::mutex> lock(mtxVmInfosCompleted);
    LOG_DEBUG << "[PersistentStore] [VmInfosCompleted] Old VmInfosCompleted size = " << vmMap.size();
    for (const auto& item : vmMap) {
        LOG_DEBUG << "[PersistentStore] [VmInfosCompleted] Old pid  = " << item.first << ", value = " << item.second
                  << ".";
    }
    auto it = vmMap.find(pid);
    if (it != vmMap.end()) {
        vmMap.erase(pid);
    }
    LOG_DEBUG << "[PersistentStore] [VmInfosCompleted] New VmInfosCompleted size = " << vmMap.size();
    for (const auto& item : vmMap) {
        LOG_DEBUG << "[PersistentStore] [VmInfosCompleted] New pid  = " << item.first << ", value = " << item.second
                  << ".";
    }
    RmrsOutStream builder;
    builder << vmMap;
    UbseByteBuffer buffer = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_VMINFO_COMPLETED, &buffer);
    if (ret != MEM_POOLING_OK) {
        SafeDeleteArray(buffer.data);
        LOG_ERROR << "[PersistentStore][VmInfosCompleted] Delete completed vmInfos data by RackManager failed.";
        return MEM_POOLING_ERROR;
    }
    SafeDeleteArray(buffer.data);
    lock.unlock();

    LOG_DEBUG << "[PersistentStore][VmInfosCompleted] Removal of completed vmInfos finished.";

    return MEM_POOLING_OK;
}

MpResult BorrowIdRedirection::Query(const std::string key, std::string& value)
{
    LOG_DEBUG << "[PersistentStore][BorrowIdRedirection] GetBorrowIdRedirection start.";
    std::lock_guard<std::mutex> lock(mtxBorrowIdRedirect);
    auto it = borrowIdRedirectionMap.find(key);
    if (it != borrowIdRedirectionMap.end()) {
        value = it->second;
        LOG_INFO << "[PersistentStore][BorrowIdRedirection] GetBorrowIdRedirection from memory.";
        return MEM_POOLING_OK;
    }
    LOG_DEBUG << "[PersistentStore][BorrowIdRedirection] GetBorrowIdRedirection finished.";
    return MEM_POOLING_OK;
}

void GetBorrowIdCompletedValue(const std::string& keyPrefix, const std::string& key, const UbseByteBuffer& buff,
                               void* ctx)
{
    if (ctx == nullptr || buff.data == nullptr || buff.len == 0) {
        LOG_WARN << "Ctx or respData is null.";
        return;
    }
    std::unordered_set<std::string>& borrowIdsCompleted = *(static_cast<std::unordered_set<std::string>*>(ctx));
    RmrsInStream builder(buff.data, buff.len);
    builder >> borrowIdsCompleted;
    return;
}

MpResult BorrowIdsCompleted::Query(std::vector<std::string>& borrowIdsCompletedList)
{
    std::lock_guard<std::mutex> lock(mtxBorrowIdsCompleted);
    std::unordered_set<std::string> borrowIdsCompleted;
    auto ret = UbseStorageQueryData(KEYPREFIX_COMMON, KEYPREFIX_BORROWID_COMPLETED, &borrowIdsCompleted,
                                    GetBorrowIdCompletedValue);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][BorrowIdsCompleted] UbseStorageQueryData by scbus failed.";
        return MEM_POOLING_ERROR;
    }

    for (const auto& borrowId : borrowIdsCompleted) {
        borrowIdsCompletedList.push_back(borrowId);
    }
    return MEM_POOLING_OK;
}

void GetSmapEnableCompletedValue(const std::string& keyPrefix, const std::string& key, const UbseByteBuffer& buff,
                                 void* ctx)
{
    if (ctx == nullptr) {
        LOG_ERROR << "GetSmapEnableCompletedValue: ctx is null!";
        return;
    }
    std::unordered_set<int16_t>& smapEnableCompleted = *(static_cast<std::unordered_set<int16_t>*>(ctx));
    RmrsInStream builder(buff.data, buff.len);
    builder >> smapEnableCompleted;
    return;
}

MpResult SmapEnableCompleted::Query(std::vector<int16_t>& smapEnableCompletedList)
{
    std::lock_guard<std::mutex> lock(mtxSmapEnableCompleted);
    std::unordered_set<int16_t> smapEnableCompleted;
    auto ret = UbseStorageQueryData(KEYPREFIX_COMMON, KEYPREFIX_SMAPENABLE_COMPLETED, &smapEnableCompleted,
                                    GetSmapEnableCompletedValue);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][SmapEnableCompleted] UbseStorageQueryData by scbus failed.";
        return MEM_POOLING_ERROR;
    }

    for (const auto& numaId : smapEnableCompleted) {
        smapEnableCompletedList.push_back(numaId);
    }
    return MEM_POOLING_OK;
}

void FaultHandleBorrowedDecision::ToString()
{
    std::ostringstream oss;
    oss << "BorrowedDecisionMap{size=" << borrowedDecisionMap.size();
    for (const auto& kv : borrowedDecisionMap) {
        oss << ", numa" << kv.first << ":" << kv.second.ToString();
    }
    oss << "}";
    LOG_DEBUG << "[FaultHandleBorrowedDecision] Print BorrowedDecisionMap," << oss.str() << ".";
}

MpResult FaultHandleBorrowedDecision::Query(BorrowedDecision& decision, const uint16_t numaId)
{
    LOG_DEBUG << "[FaultHandleBorrowedDecision] Query for numaId=" << numaId << ".";

    std::lock_guard<std::mutex> lock(mtxBorrowedDecision);
    auto it = borrowedDecisionMap.find(numaId);
    if (it == borrowedDecisionMap.end()) {
        LOG_DEBUG << "[FaultHandleBorrowedDecision] No borrowed decision found for numaId=" << numaId << ".";
        return MEM_POOLING_ERROR;
    }
    decision = it->second;
    LOG_DEBUG << "[FaultHandleBorrowedDecision] Query success, decision=" << decision.ToString() << ".";
    return MEM_POOLING_OK;
}

MpResult FaultHandleBorrowedDecision::QueryAll(std::vector<BorrowedDecision>& decisionList)
{
    LOG_DEBUG << "[FaultHandleBorrowedDecision] Query all borrowed decisions.";
    std::lock_guard<std::mutex> lock(mtxBorrowedDecision);

    LOG_DEBUG << "[FaultHandleBorrowedDecision] borrowedDecisionMap.size=" << borrowedDecisionMap.size() << ".";
    if (borrowedDecisionMap.size() == 0) {
        LOG_DEBUG << "[FaultHandleBorrowedDecision] No borrowed decision found.";
        return MEM_POOLING_ERROR;
    }

    for (const auto& pair : borrowedDecisionMap) {
        LOG_DEBUG << "[FaultHandleBorrowedDecision] numaId=" << pair.first
                  << " get one borrowedDecision=" << pair.second.ToString() << ".";
        decisionList.push_back(pair.second);
    }
    return MEM_POOLING_OK;
}

void GetFaultProcessBorrowIdValue(const std::string& keyPrefix, const std::string& key, const UbseByteBuffer& buff,
                                  void* ctx)
{
    if (ctx == nullptr) {
        LOG_ERROR << "GetSmapEnableCompletedValue: ctx is null!";
        return;
    }
    std::unordered_set<std::string>& borrowIdInFaultProcess = *(static_cast<std::unordered_set<std::string>*>(ctx));
    RmrsInStream builder(buff.data, buff.len);
    builder >> borrowIdInFaultProcess;
    return;
}

MpResult BorrowIdInFaultProcess::Query(std::vector<std::string>& borrowIdInFaultProcessList)
{
    std::lock_guard<std::mutex> lock(mtxBorrowIdInFaultProcess);
    std::unordered_set<std::string> borrowIdInFaultProcess;
    auto ret = UbseStorageQueryData(KEYPREFIX_COMMON, KEYPREFIX_FAULT_PROCESS_BORROWID, &borrowIdInFaultProcess,
                                    GetFaultProcessBorrowIdValue);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][SmapEnableCompleted] UbseStorageQueryData by scbus failed.";
        return MEM_POOLING_ERROR;
    }

    for (const auto& borrowId : borrowIdInFaultProcess) {
        borrowIdInFaultProcessList.push_back(borrowId);
    }
    return MEM_POOLING_OK;
}

void GetRemovePidCompletedValue(const std::string& keyPrefix, const std::string& key, const UbseByteBuffer& buff,
                                void* ctx)
{
    if (ctx == nullptr) {
        LOG_ERROR << "[PersistentStore][RemovePidCompleted] ctx is null!";
        return;
    }

    auto& removePidCompleted = *(static_cast<std::unordered_map<uint16_t, std::unordered_set<pid_t>>*>(ctx));
    RmrsInStream builder(buff.data, buff.len);
    builder >> removePidCompleted;
    LOG_DEBUG << "[PersistentStore][RemovePidCompleted] Loaded map size=" << removePidCompleted.size() << ".";
}

MpResult RemovePidCompleted::Query(std::unordered_map<uint16_t, std::unordered_set<pid_t>>& removePidCompletedList)
{
    LOG_DEBUG << "[PersistentStore][RemovePidCompleted] Query start.";
    std::lock_guard<std::mutex> lock(mtxRemovePidCompleted);

    removePidCompletedList.clear();
    removePidCompletedList = removePidCompleted;

    LOG_DEBUG << "[PersistentStore][RemovePidCompleted] Query result size=" << removePidCompletedList.size() << ".";
    for (const auto& [numaId, pidSet] : removePidCompletedList) {
        LOG_DEBUG << "[PersistentStore][RemovePidCompleted] Query success, numa=" << numaId
                  << " pidCount=" << pidSet.size() << ".";

        for (auto pid : pidSet) {
            LOG_DEBUG << "[PersistentStore][RemovePidCompleted] Query success, numa=" << numaId << ", pid=" << pid
                      << ".";
        }
    }

    LOG_DEBUG << "[PersistentStore][RemovePidCompleted] Query end.";
    return MEM_POOLING_OK;
}

void GetVmInfosCompletedValue(const std::string& keyPrefix, const std::string& key, const UbseByteBuffer& buff,
                              void* ctx)
{
    if (ctx == nullptr || buff.data == nullptr || buff.len == 0) {
        LOG_WARN << "Ctx or respData is null.";
        return;
    }
    std::unordered_map<pid_t, std::string>& vmMap = *(static_cast<std::unordered_map<pid_t, std::string>*>(ctx));
    RmrsInStream builder(buff.data, buff.len);
    builder >> vmMap;
    return;
}

MpResult VmInfosCompleted::Query(std::unordered_map<pid_t, std::string>& vmInfosCompletedMap)
{
    std::lock_guard<std::mutex> lock(mtxVmInfosCompleted);
    std::unordered_map<pid_t, std::string> vmMap;
    auto ret = UbseStorageQueryData(KEYPREFIX_COMMON, KEYPREFIX_VMINFO_COMPLETED, &vmMap, GetVmInfosCompletedValue);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][VmInfosCompleted] RackStorageQueryAllData failed.";
        return MEM_POOLING_ERROR;
    }

    for (const auto& vmInfo : vmMap) {
        vmInfosCompletedMap[vmInfo.first] = vmInfo.second;
    }
    return MEM_POOLING_OK;
}

MpResult BorrowIdRedirection::PutRawData(UbseByteBuffer& buffer)
{
    LOG_DEBUG << "[PersistentStore][BorrowIdRedirection] PutBorrowIdRedirectionRawData start.";
    std::lock_guard<std::mutex> locker(mtxBorrowIdRedirect);
    RmrsInStream builderIn(buffer.data, buffer.len);
    borrowIdRedirectionMap.clear();
    builderIn >> borrowIdRedirectionMap;
    auto ret = UbseStoragePutData("mempooling", "_borrowidredirection", &buffer);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][BorrowIdRedirection] RackStorageQueryAllData failed.";
        return MEM_POOLING_ERROR;
    }
    LOG_DEBUG << "[PersistentStore][BorrowIdRedirection] PutBorrowIdRedirectionRawData finished.";
    return MEM_POOLING_OK;
}

MpResult BorrowIdsCompleted::PutRawData(UbseByteBuffer& data)
{
    LOG_DEBUG << "[PersistentStore][BorrowIdsCompleted] PutBorrowIdsCompletedRawData start.";

    std::lock_guard<std::mutex> locker(mtxBorrowIdsCompleted);
    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_BORROWID_COMPLETED, &data);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][BorrowIdsCompleted] UbseStoragePutData failed.";
        return MEM_POOLING_ERROR;
    }
    borrowIdsCompleted.clear();
    RmrsInStream builder(data.data, data.len);
    builder >> borrowIdsCompleted;
    LOG_DEBUG << "[PersistentStore][BorrowIdsCompleted] PutBorrowIdsCompletedRawData end.";

    return MEM_POOLING_OK;
}

MpResult SmapEnableCompleted::PutRawData(UbseByteBuffer& data)
{
    LOG_DEBUG << "[PersistentStore][SmapEnableCompleted] PutSmapEnableCompletedRawData start.";

    std::lock_guard<std::mutex> locker(mtxSmapEnableCompleted);
    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_SMAPENABLE_COMPLETED, &data);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][SmapEnableCompleted] UbseStoragePutData failed.";
        return MEM_POOLING_ERROR;
    }
    smapEnableCompleted.clear();
    RmrsInStream builder(data.data, data.len);
    builder >> smapEnableCompleted;
    LOG_DEBUG << "[PersistentStore][SmapEnableCompleted] PutSmapEnableCompletedRawData end.";

    return MEM_POOLING_OK;
}

MpResult FaultHandleBorrowedDecision::PutRawData(UbseByteBuffer& data)
{
    LOG_DEBUG << "[FaultHandleBorrowedDecision] PutRawData start.";

    std::lock_guard<std::mutex> locker(mtxBorrowedDecision);
    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_BORROWED_DECISION, &data);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[FaultHandleBorrowedDecision] UbseStoragePutData failed, ret=" << ret << ".";
        return MEM_POOLING_ERROR;
    }
    // 重新加载到内存
    borrowedDecisionMap.clear();
    RmrsInStream builder(data.data, data.len);
    builder >> borrowedDecisionMap;
    LOG_DEBUG << "[FaultHandleBorrowedDecision] PutRawData success, loaded " << borrowedDecisionMap.size()
              << " entries.";
    return MEM_POOLING_OK;
}

MpResult BorrowIdInFaultProcess::PutRawData(UbseByteBuffer& data)
{
    LOG_DEBUG << "[PersistentStore][BorrowIdInFaultProcess] PutBorrowIdInFaultProcessRawData start.";

    std::lock_guard<std::mutex> locker(mtxBorrowIdInFaultProcess);
    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_FAULT_PROCESS_BORROWID, &data);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][BorrowIdInFaultProcess] UbseStoragePutData failed.";
        return MEM_POOLING_ERROR;
    }
    borrowIdInFaultProcess.clear();
    RmrsInStream builder(data.data, data.len);
    builder >> borrowIdInFaultProcess;
    LOG_DEBUG << "[PersistentStore][BorrowIdInFaultProcess] PutBorrowIdInFaultProcessRawData end.";

    return MEM_POOLING_OK;
}

MpResult RemovePidCompleted::PutRawData(UbseByteBuffer& data)
{
    LOG_DEBUG << "[PersistentStore][RemovePidCompleted] PutRemovePidCompletedRawData start.";

    std::lock_guard<std::mutex> locker(mtxRemovePidCompleted);
    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_REMOVEPID_COMPLETED, &data);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][RemovePidCompleted] UbseStoragePutData failed.";
        return MEM_POOLING_ERROR;
    }
    removePidCompleted.clear();
    RmrsInStream builder(data.data, data.len);
    builder >> removePidCompleted;
    LOG_DEBUG << "[PersistentStore][RemovePidCompleted] PutRemovePidCompletedRawData end.";

    return MEM_POOLING_OK;
}

MpResult VmInfosCompleted::PutRawData(UbseByteBuffer& data)
{
    LOG_DEBUG << "[PersistentStore][VmInfosCompleted] Put VmInfosCompleted start.";

    std::lock_guard<std::mutex> locker(mtxVmInfosCompleted);

    auto ret = UbseStoragePutData(KEYPREFIX_COMMON, KEYPREFIX_VMINFO_COMPLETED, &data); // 覆盖写
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][PutVmInfosCompletedRawData] UbseStoragePutData failed.";
        return MEM_POOLING_ERROR;
    }
    vmMap.clear();
    RmrsInStream builder(data.data, data.len);
    builder >> vmMap;
    LOG_DEBUG << "[PersistentStore][VmInfosCompleted] Put VmInfosCompleted end.";

    return MEM_POOLING_OK;
}

MpResult BorrowIdRedirection::GetRawData(UbseByteBuffer& buffer)
{
    LOG_DEBUG << "[PersistentStore][BorrowIdRedirection] GetBorrowIdRedirectionRawData start.";
    std::lock_guard<std::mutex> locker(mtxBorrowIdRedirect);
    RmrsOutStream builder;
    builder << borrowIdRedirectionMap;
    buffer.data = builder.GetBufferPointer();
    if (buffer.data == nullptr) {
        LOG_ERROR << "[PersistentStore][BorrowIdRedirection] GetRawData failed.";
        return MEM_POOLING_ERROR;
    }
    buffer.len = builder.GetSize();
    LOG_DEBUG << "[PersistentStore][BorrowIdRedirection] GetBorrowIdRedirectionRawData finished.";
    return MEM_POOLING_OK;
}

MpResult BorrowIdsCompleted::GetRawData(UbseByteBuffer& data, bool needLock)
{
    LOG_DEBUG << "[PersistentStore][BorrowIdsCompleted] GetBorrowIdsCompletedRawData start.";

    std::unique_lock<std::mutex> locker(mtxBorrowIdsCompleted, std::defer_lock);
    if (needLock) {
        locker.lock();
    }
    std::vector<std::string> vec;
    std::unordered_set<std::string> borrowIdsCompleted;
    auto ret = UbseStorageQueryData(KEYPREFIX_COMMON, KEYPREFIX_BORROWID_COMPLETED, &borrowIdsCompleted,
                                    GetBorrowIdCompletedValue);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][BorrowIdsCompleted] RackStorageQueryAllData by scbus fail.";
    }

    if (borrowIdsCompleted.empty()) {
        data.len = 1;
        data.data = new (std::nothrow) uint8_t[data.len];
        if (data.data == nullptr) {
            LOG_ERROR << "[PersistentStore][BorrowIdsCompleted] new data failed.";
            return MEM_POOLING_ERROR;
        }
        data.data[0] = ' '; // 数据清空标致
        LOG_DEBUG << "[PersistentStore][BorrowIdsCompleted] The data of keyPrefix=" << KEYPREFIX_BORROWID_COMPLETED
                  << " is empty.";
        return MEM_POOLING_OK;
    }
    RmrsOutStream builder;
    builder << borrowIdsCompleted;
    data = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    if (data.data == nullptr) {
        LOG_ERROR << "[PersistentStore][BorrowIdsCompleted] GetRawData failed.";
        return MEM_POOLING_ERROR;
    }

    LOG_DEBUG << "[PersistentStore][BorrowIdsCompleted] GetBorrowIdsCompletedRawData end.";
    return MEM_POOLING_OK;
}

MpResult SmapEnableCompleted::GetRawData(UbseByteBuffer& data, bool needLock)
{
    LOG_DEBUG << "[PersistentStore][SmapEnableCompleted] GetSmapEnableCompletedRawData start.";
    std::unique_lock<std::mutex> locker(mtxSmapEnableCompleted, std::defer_lock);
    if (needLock) {
        locker.lock();
    }

    std::unordered_set<int16_t> smapEnableCompleted;
    auto ret = UbseStorageQueryData(KEYPREFIX_COMMON, KEYPREFIX_SMAPENABLE_COMPLETED, &smapEnableCompleted,
                                    GetSmapEnableCompletedValue);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][SmapEnableCompleted] RackStorageQueryAllData by scbus fail.";
    }

    if (smapEnableCompleted.empty()) {
        data.len = 1;
        data.data = new uint8_t[data.len];
        data.data[0] = ' '; // 数据清空标致
        LOG_DEBUG << "[PersistentStore][SmapEnableCompleted] The data of keyPrefix=" << KEYPREFIX_SMAPENABLE_COMPLETED
                  << " is empty.";
        return MEM_POOLING_OK;
    }
    RmrsOutStream builder;
    builder << smapEnableCompleted;
    data = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    LOG_DEBUG << "[PersistentStore][SmapEnableCompleted] GetSmapEnableCompletedRawData end.";
    return MEM_POOLING_OK;
}

MpResult FaultHandleBorrowedDecision::GetRawData(UbseByteBuffer& data, bool needLock)
{
    LOG_DEBUG << "[FaultHandleBorrowedDecision] GetRawData start.";

    std::unique_lock<std::mutex> locker(mtxBorrowedDecision, std::defer_lock);
    if (needLock) {
        locker.lock();
    }

    if (borrowedDecisionMap.empty()) {
        data.len = 1;
        data.data = new uint8_t[data.len];
        data.data[0] = ' '; // 数据清空标致
        data.freeFunc = [](uint8_t* p) {
            delete[] p;
        };
        LOG_DEBUG << "[FaultHandleBorrowedDecision] The data of keyPrefix=" << KEYPREFIX_BORROWED_DECISION
                  << " is empty.";
        return MEM_POOLING_OK;
    }

    RmrsOutStream builder;
    builder << borrowedDecisionMap;
    data = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    LOG_DEBUG << "[FaultHandleBorrowedDecision] GetBorrowedDecisionRawData end.";
    return MEM_POOLING_OK;
}

MpResult BorrowIdInFaultProcess::GetRawData(UbseByteBuffer& data, bool needLock)
{
    LOG_DEBUG << "[PersistentStore][BorrowIdInFaultProcess] GetBorrowIdInFaultProcessRawData start.";
    std::unique_lock<std::mutex> locker(mtxBorrowIdInFaultProcess, std::defer_lock);
    if (needLock) {
        locker.lock();
    }

    std::unordered_set<std::string> borrowIdInFaultProcess;
    auto ret = UbseStorageQueryData(KEYPREFIX_COMMON, KEYPREFIX_FAULT_PROCESS_BORROWID, &borrowIdInFaultProcess,
                                    GetFaultProcessBorrowIdValue);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][BorrowIdInFaultProcess] RackStorageQueryAllData by scbus fail.";
    }

    if (borrowIdInFaultProcess.empty()) {
        data.len = 1;
        data.data = new uint8_t[data.len];
        data.data[0] = ' '; // 数据清空标致
        LOG_DEBUG << "[PersistentStore][BorrowIdInFaultProcess] The data of keyPrefix="
                  << KEYPREFIX_FAULT_PROCESS_BORROWID << " is empty.";
        return MEM_POOLING_OK;
    }
    RmrsOutStream builder;
    builder << borrowIdInFaultProcess;
    data = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    LOG_DEBUG << "[PersistentStore][BorrowIdInFaultProcess] GetBorrowIdInFaultProcessRawData end.";
    return MEM_POOLING_OK;
}

MpResult RemovePidCompleted::GetRawData(UbseByteBuffer& data, bool needLock)
{
    LOG_DEBUG << "[PersistentStore][RemovePidCompleted] GetRemovePidCompletedRawData start.";

    std::unique_lock<std::mutex> locker(mtxRemovePidCompleted, std::defer_lock);
    if (needLock) {
        locker.lock();
    }

    // 直接从缓存removePidCompleted中取
    if (removePidCompleted.empty()) {
        data.len = 1;
        data.data = new uint8_t[data.len];
        data.data[0] = ' '; // 数据清空标致
        LOG_DEBUG << "[PersistentStore][RemovePidCompleted] The data of keyPrefix=" << KEYPREFIX_REMOVEPID_COMPLETED
                  << " is empty.";
        return MEM_POOLING_OK;
    }
    RmrsOutStream builder;
    builder << removePidCompleted;
    data = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};

    LOG_DEBUG << "[PersistentStore][RemovePidCompleted] GetRemovePidCompletedRawData end.";
    return MEM_POOLING_OK;
}

MpResult VmInfosCompleted::GetRawData(UbseByteBuffer& data, bool needLock)
{
    LOG_DEBUG << "[PersistentStore][VmInfosCompleted] GetVmInfosCompletedRawData start.";

    std::unique_lock<std::mutex> locker(mtxVmInfosCompleted, std::defer_lock);
    if (needLock) {
        locker.lock();
    }
    std::unordered_map<pid_t, std::string> vmMap;
    auto ret = UbseStorageQueryData(KEYPREFIX_COMMON, KEYPREFIX_VMINFO_COMPLETED, &vmMap, GetVmInfosCompletedValue);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][VmInfosCompleted] RackStorageQueryAllData fail.";
    }

    if (vmMap.empty()) {
        data.len = 1;
        data.data = new (std::nothrow) uint8_t[data.len];
        if (data.data == nullptr) {
            LOG_ERROR << "[PersistentStore][VmInfosCompleted] new data failed.";
            return MEM_POOLING_ERROR;
        }
        data.data[0] = ' '; // 数据清空标致
        LOG_DEBUG << "[PersistentStore][VmInfosCompleted] The data of keyPrefix(" << KEYPREFIX_VMINFO_COMPLETED
                  << ") is empty.";
        return MEM_POOLING_OK;
    }

    RmrsOutStream builder;
    builder << vmMap;
    data = {.data = builder.GetBufferPointer(), .len = builder.GetSize(), .freeFunc = nullptr};
    if (data.data == nullptr) {
        LOG_ERROR << "[PersistentStore][VmInfosCompleted] GetRawData failed.";
        return MEM_POOLING_ERROR;
    }
    LOG_DEBUG << "[PersistentStore][VmInfosCompleted] GetVmInfosCompletedRawData end.";
    return MEM_POOLING_OK;
}

MpResult BorrowIdRedirection::Remove(const std::string key)
{
    LOG_DEBUG << "[PersistentStore][BorrowIdRedirection] RemoveBorrowIdRedirection start.";
    std::unique_lock<std::mutex> lock(mtxBorrowIdRedirect);
    auto it = borrowIdRedirectionMap.find(key);
    if (it == borrowIdRedirectionMap.end()) {
        LOG_INFO << "[PersistentStore][BorrowIdRedirection] Key=" << key << " has been removed.";
        return MEM_POOLING_OK;
    }
    borrowIdRedirectionMap.erase(it);
    RmrsOutStream builder;
    builder << borrowIdRedirectionMap;
    UbseByteBuffer buffer;
    buffer.data = builder.GetBufferPointer();
    buffer.len = builder.GetSize();
    auto ret = UbseStoragePutData("mempooling", "_borrowidredirection", &buffer);
    delete[] buffer.data;
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][BorrowIdRedirection] UbseStoragePutData failed.";
        return MEM_POOLING_ERROR;
    }
    lock.unlock();
    LOG_DEBUG << "[PersistentStore][BorrowIdRedirection] RemoveBorrowIdRedirection finished.";
    return MEM_POOLING_OK;
}

MpResult MemRequestHelper::ParseMemIdArray(const rapidjson::Value& doc, BorrowRecord& record)
{
    auto ret = MEM_POOLING_ERROR;
    if (!doc.HasMember("lentMemId") || !doc.HasMember("borrowMemId")) {
        LOG_ERROR << "[MemLedger] [BorrowRecords] Failed to get memId array, no member named lentMemId or borrowMemId.";
        return ret;
    }
    const rapidjson::Value& lentMemId = doc["lentMemId"];
    const rapidjson::Value& borrowMemId = doc["borrowMemId"];
    if (!lentMemId.IsArray() || !borrowMemId.IsArray()) {
        LOG_ERROR << "[MemLedger] [BorrowRecords] Failed to get memId array, parament is not array.";
        return ret;
    }
    auto lentMemIdSize = lentMemId.Size();
    auto borrowMemIdSize = borrowMemId.Size();

    for (rapidjson::SizeType i = 0; i < lentMemIdSize; ++i) {
        if (!(lentMemId[i].IsUint64())) {
            LOG_ERROR << "[MemLedger] [BorrowRecords] Failed to get array item[" << i
                      << "], lentMemId=" << JsonUtil::PrintJsonString(lentMemId[i]) << ".";
            return ret;
        }
        record.lentMemId.push_back(static_cast<uint64_t>(lentMemId[i].GetUint64()));
    }

    for (rapidjson::SizeType i = 0; i < borrowMemIdSize; ++i) {
        if (!(borrowMemId[i].IsUint64())) {
            LOG_ERROR << "[MemLedger] [BorrowRecords] Failed to get array item[" << i
                      << "], borrowMemId=" << JsonUtil::PrintJsonString(borrowMemId[i]) << ".";
            return ret;
        }
        record.borrowMemId.push_back(static_cast<uint64_t>(borrowMemId[i].GetUint64()));
    }
    return MEM_POOLING_OK;
}

MpResult MemRequestHelper::ParseLentNumaArray(const rapidjson::Value& doc, std::vector<LentNuma>& lentNumaVec)
{
    auto ret = MEM_POOLING_ERROR;
    if (!doc.HasMember("lentNuma")) {
        LOG_ERROR << "[MemLedger] [BorrowRecords] Failed to get memId array, no member named lentNuma.";
        return ret;
    }
    const rapidjson::Value& lentNumaInfos = doc["lentNuma"];
    if (!lentNumaInfos.IsArray()) {
        LOG_ERROR << "[MemLedger] [BorrowRecords] Failed to get lentNuma array, parament is not array.";
        return ret;
    }
    for (rapidjson::SizeType i = 0; i < lentNumaInfos.Size(); ++i) {
        const rapidjson::Value& lentNumaInfo = lentNumaInfos[i];
        LentNuma lentNuma;
        uint16_t numaId;
        uint64_t lentSize;
        bool typeCheckRes = JsonUtil::GetJsonUint16Value(lentNumaInfo, "numaId", numaId);
        typeCheckRes &= JsonUtil::GetJsonUint64Value(lentNumaInfo, "lentSize", lentSize);
        if (!typeCheckRes) {
            LOG_ERROR << "Failed to parse lentNuma item[" << i << "].";
            return ret;
        }
        lentNuma.numaId = numaId;
        lentNuma.lentSize = static_cast<uint64_t>(lentSize / KB_TO_BYTES);
        lentNumaVec.push_back(lentNuma);
    }
    return MEM_POOLING_OK;
}

MpResult MemRequestHelper::ParseBorrowRecordFields(const rapidjson::Value& doc, BorrowRecord& record)
{
    auto ret = MEM_POOLING_ERROR;
    std::string collectName;
    uint64_t collectSize;
    std::string collectLentNode;
    uint16_t collectLentSocketId;
    std::string collectBorrowNode;
    int16_t collectBorrowLocalNuma;
    int16_t collectBorrowRemoteNuma;

    bool typeCheckRes = JsonUtil::GetJsonStringValueWithoutCheck(doc, "name", collectName);
    typeCheckRes &= JsonUtil::GetJsonUint64Value(doc, "size", collectSize);
    typeCheckRes &= JsonUtil::GetJsonStringValueWithoutCheck(doc, "lentNode", collectLentNode);
    typeCheckRes &= JsonUtil::GetJsonUint16Value(doc, "lentSocketId", collectLentSocketId);
    typeCheckRes &= JsonUtil::GetJsonStringValueWithoutCheck(doc, "borrowNode", collectBorrowNode);
    typeCheckRes &= JsonUtil::GetJsonInt16Value(doc, "borrowLocalNuma", collectBorrowLocalNuma);
    typeCheckRes &= JsonUtil::GetJsonInt16Value(doc, "borrowRemoteNuma", collectBorrowRemoteNuma);
    if (!typeCheckRes) {
        LOG_ERROR << "[MemLedger] [BorrowRecords] Failed to parse record fields.";
        return ret;
    }
    record.name = collectName;
    record.size = static_cast<uint64_t>(collectSize / KB_TO_BYTES);
    record.lentNode = collectLentNode;
    record.lentSocketId = collectLentSocketId;
    record.borrowNode = collectBorrowNode;
    record.borrowLocalNuma = collectBorrowLocalNuma;
    record.borrowRemoteNuma = collectBorrowRemoteNuma;
    return MEM_POOLING_OK;
}

MpResult BorrowRecordHelper::GenBorrowRecords(const rapidjson::Value& doc, std::vector<BorrowRecord>& borrowRecords)
{
    auto ret = MEM_POOLING_ERROR;
    if (!doc.IsArray()) {
        LOG_ERROR << "[MemLedger] [BorrowRecords] Failed to get record array, parament is not array.";
        return ret;
    }
    borrowRecords.clear();
    for (rapidjson::SizeType i = 0; i < doc.Size(); ++i) {
        const rapidjson::Value& borrowRecordInfo = doc[i];
        BorrowRecord record;
        ret = MemRequestHelper::ParseLentNumaArray(borrowRecordInfo, record.lentNuma);
        ret |= MemRequestHelper::ParseMemIdArray(borrowRecordInfo, record);
        ret |= MemRequestHelper::ParseBorrowRecordFields(borrowRecordInfo, record);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "[MemLedger] [BorrowRecords] Failed to parse record item[" << i << "].";
            return ret;
        }
        borrowRecords.push_back(record);
    }
    return MEM_POOLING_OK;
}

MpResult BorrowRecordHelper::GetDebtInfosWithRetry(std::vector<UbseNumaMemoryDebtInfo>& debtInfos)
{
    LOG_DEBUG << "[MemLedger][BorrowRecords] GetDebtInfosWithRetry start.";
    constexpr int maxRetryTimes = 30;
    constexpr int sleepSeconds = 1;
    int curRetryTimes = 0;

    while (curRetryTimes < maxRetryTimes) {
        debtInfos.clear();
        UbseResult ret = UbseGetNumaMemDebtInfo(debtInfos);
        if (ret == UBSE_ERR_INTERNAL || (ret != UBSE_OK && debtInfos.empty())) {
            LOG_WARN << "[MemLedger][BorrowRecords] UbseGetNumaMemDebtInfo failed, retry=" << (curRetryTimes + 1)
                     << ", ret=" << static_cast<uint32_t>(ret) << ".";
            std::this_thread::sleep_for(std::chrono::seconds(sleepSeconds));
            curRetryTimes++;
            continue;
        }

        // 成功
        break;
    }

    if (curRetryTimes >= maxRetryTimes) {
        LOG_ERROR << "[MemLedger][BorrowRecords] UbseGetNumaMemDebtInfo failed after max retry.";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

MpResult BorrowRecordHelper::GetValidDebtInfosWithRetry(std::vector<UbseNumaMemoryDebtInfo>& debtInfos)
{
    LOG_DEBUG << "[MemLedger][BorrowRecords] GetDebtInfosWithRetry start.";
    constexpr int maxRetryTimes = 30;
    constexpr int sleepSeconds = 1;
    int curRetryTimes = 0;

    while (curRetryTimes < maxRetryTimes) {
        debtInfos.clear();
        UbseResult ret = UbseGetNumaMemDebtInfo(debtInfos);

        bool needRetry = false;

        // 内部错误 或者 非内部错误但是账本为空，都需要进行重试
        if (ret == UBSE_ERR_INTERNAL) {
            needRetry = true;
        } else if (ret != UBSE_OK && debtInfos.empty()) {
            needRetry = true;
            LOG_WARN << "[GetValidDebtInfosWithRetry] GetDebtInfos Failed, ret=" << ret
                     << ", debtInfos is empty, need Retry.";
        } else if (ret != UBSE_OK && !debtInfos.empty()) {
            bool hasInvalidRemoteNuma =
                std::any_of(debtInfos.begin(), debtInfos.end(),
                            [](const UbseNumaMemoryDebtInfo& info) { return info.remoteNumaId < 0; });
            LOG_WARN << "[GetValidDebtInfosWithRetry] GetDebtInfos Failed, ret=" << ret
                     << ", has remoteNumaId < 0, need Retry.";
            if (hasInvalidRemoteNuma) {
                needRetry = true;
            }
        }

        if (needRetry) {
            LOG_WARN << "[MemLedger][BorrowRecords] UbseGetNumaMemDebtInfo retry=" << (curRetryTimes + 1)
                     << ", ret=" << static_cast<uint32_t>(ret);
            std::this_thread::sleep_for(std::chrono::seconds(sleepSeconds));
            curRetryTimes++;
            continue;
        }

        break; // 成功
    }

    if (curRetryTimes >= maxRetryTimes) {
        LOG_ERROR << "[MemLedger][BorrowRecords] UbseGetNumaMemDebtInfo failed after max retry.";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

// isFilter为默认参数，为标志位表示是否是filter函数调用，默认值为false
MpResult BorrowRecordHelper::UpdateBorrowRecords(bool isFilter)
{
    std::vector<UbseNumaMemoryDebtInfo> debtInfos;
    MpResult ret = MEM_POOLING_OK;
    if (isFilter) {
        // filterAndSort函数无需校验remoteNumaId
        ret = GetDebtInfosWithRetry(debtInfos);
    } else {
        ret = GetValidDebtInfosWithRetry(debtInfos);
    }

    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[MemLedger][BorrowRecords] GetDebtInfosWithRetry failed.";
        return MEM_POOLING_ERROR;
    }
    gBorrowRecords.clear();
    for (auto& debtInfo : debtInfos) {
        BorrowRecord record;
        record.name = debtInfo.name;
        record.username = debtInfo.username;
        record.uid = debtInfo.uid;
        record.size = debtInfo.size / KB_TO_BYTES;
        record.lentNode = debtInfo.lentNodeId;
        record.lentMemId = debtInfo.lentMemId;
        // lentSocketIdList加上空校验
        if (debtInfo.lentSocketIdList.size() == 0 || debtInfo.borrowSocketIdList.size() == 0) {
            LOG_ERROR << "[MemLedger] [BorrowRecords] SocketIdList is empty.";
            return MEM_POOLING_ERROR;
        }
        record.lentSocketId = debtInfo.lentSocketIdList[0];
        record.borrowSocketId = debtInfo.borrowSocketIdList[0];
        size_t n = std::min(debtInfo.lentNumaIdList.size(), debtInfo.lentNumaSizeList.size());
        for (size_t i = 0; i < n; ++i) {
            LentNuma ln;
            ln.numaId = static_cast<uint16_t>(debtInfo.lentNumaIdList[i]);
            ln.lentSize = debtInfo.lentNumaSizeList[i];
            record.lentNuma.push_back(ln);
        }
        record.borrowNode = debtInfo.borrowNodeId;
        errno_t res = memcpy_s(&record.borrowLocalNuma, sizeof(record.borrowLocalNuma), debtInfo.usrInfo,
                               sizeof(record.borrowLocalNuma));
        if (res != EOK) {
            LOG_ERROR << "[MemLedger] [BorrowRecords] memcpy_s failed.";
        }
        record.borrowRemoteNuma = static_cast<int16_t>(debtInfo.remoteNumaId);
        record.borrowMemId = debtInfo.borrowMemId;
        gBorrowRecords.push_back(record);
    }
    for (auto& record : gBorrowRecords) {
        LOG_DEBUG << "[MemLedger] [BorrowRecords] Collected borrowRecords: " << record.ToString() << ".";
    }
    return MEM_POOLING_OK;
}

// 更新传入的nodeId以及集群中可见的nodeId的账本信息
MpResult BorrowRecordHelper::UpdateBorrowRecordsWithFragmentFault(std::string nodeId)
{
    // 目前架构能查到的所有的节点都可以借用 查询所有节点
    std::vector<std::string> allNodeIdList = MpConfiguration::GetInstance().GetNodeIds();
    if (allNodeIdList.empty()) {
        return MEM_POOLING_ERROR;
    }

    // 查找nodeId是否在列表中，不存在则添加
    auto iter = std::find(allNodeIdList.begin(), allNodeIdList.end(), nodeId);
    if (iter == allNodeIdList.end()) {
        allNodeIdList.push_back(nodeId);
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[FaultManager] NodeId: " << nodeId << " not in cluster node list, add it to update list.";
    }

    gBorrowRecordsFragmentFault.clear();
    for (std::string nodeId : allNodeIdList) {
        std::vector<UbseNumaMemoryDebtInfo> debtInfos;
        auto ret = UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos);
        if (ret == UBSE_ERR_INTERNAL) {
            LOG_ERROR << "[MemLedger] [BorrowRecords][FaultManager] UbseGetNumaMemDebtInfoWithNode failed, ret="
                      << static_cast<uint32_t>(ret) << ".";
            return MEM_POOLING_ERROR;
        }
        std::vector<BorrowRecord> recordVec;
        for (auto& debtInfo : debtInfos) {
            BorrowRecord record;
            // 最小化修改：调用子函数完成转换
            if (!ConvertDebtToRecord(debtInfo, record)) {
                return MEM_POOLING_ERROR;
            }
            if (record.borrowRemoteNuma < 0) {
                continue;
            }
            recordVec.push_back(record);
        }
        for (auto& record : recordVec) {
            LOG_DEBUG << "[MemLedger] [BorrowRecords][FaultManager] Collected borrowRecords: " << record.ToString()
                      << ".";
        }
        gBorrowRecordsFragmentFault[nodeId] = recordVec;
    }

    return MEM_POOLING_OK;
}

bool BorrowRecordHelper::ConvertDebtToRecord(const UbseNumaMemoryDebtInfo& debtInfo, BorrowRecord& outRecord)
{
    outRecord.name = debtInfo.name;
    outRecord.username = debtInfo.username;
    outRecord.uid = debtInfo.uid;
    outRecord.size = debtInfo.size / KB_TO_BYTES;
    outRecord.lentNode = debtInfo.lentNodeId;
    outRecord.lentMemId = debtInfo.lentMemId;

    // lentSocketIdList加上空校验
    if (debtInfo.lentSocketIdList.empty() || debtInfo.borrowSocketIdList.empty()) {
        LOG_ERROR << "[MemLedger] [BorrowRecords] SocketIdList is empty.";
        return false;
    }
    outRecord.lentSocketId = debtInfo.lentSocketIdList[0];
    outRecord.borrowSocketId = debtInfo.borrowSocketIdList[0];

    size_t n = std::min(debtInfo.lentNumaIdList.size(), debtInfo.lentNumaSizeList.size());
    for (size_t i = 0; i < n; ++i) {
        LentNuma ln;
        ln.numaId = static_cast<uint16_t>(debtInfo.lentNumaIdList[i]);
        ln.lentSize = debtInfo.lentNumaSizeList[i];
        outRecord.lentNuma.push_back(ln);
    }

    outRecord.borrowNode = debtInfo.borrowNodeId;
    errno_t res = memcpy_s(&outRecord.borrowLocalNuma, sizeof(outRecord.borrowLocalNuma), debtInfo.usrInfo,
                           sizeof(outRecord.borrowLocalNuma));
    if (res != EOK) {
        LOG_ERROR << "[MemLedger] [BorrowRecords][FaultManager] memcpy_s failed.";
        return false;
    }

    outRecord.borrowRemoteNuma = static_cast<int16_t>(debtInfo.remoteNumaId);
    outRecord.borrowMemId = debtInfo.borrowMemId;

    // 非法条目日志
    if (outRecord.borrowRemoteNuma < 0) {
        LOG_ERROR << "[MemLedger] [BorrowRecords][FaultManager] BorrowRemoteNuma is invalid, record: "
                  << outRecord.ToString() << ".";
    }
    return true;
}

MpResult BorrowRecordHelper::GetFragmentFaultBorrowRecords(std::string nodeId,
                                                           std::vector<BorrowRecord>& fragMentFaultBorrowRecords)
{
    fragMentFaultBorrowRecords = gBorrowRecordsFragmentFault[nodeId];
    return MEM_POOLING_OK;
}

MpResult BorrowRecordHelper::UpdateBorrowRecordsAllWithFault()
{
    std::vector<UbseNumaMemoryDebtInfo> debtInfos;
    auto ret = UbseGetNumaMemDebtInfo(debtInfos);
    if (ret == UBSE_ERR_INTERNAL) {
        LOG_ERROR << "[MemLedger] [BorrowRecords] UbseGetNumaMemDebtInfo failed, ret=" << static_cast<uint32_t>(ret)
                  << ".";
        return MEM_POOLING_ERROR;
    }
    gBorrowRecords.clear();
    for (auto& debtInfo : debtInfos) {
        BorrowRecord record;
        record.name = debtInfo.name;
        record.username = debtInfo.username;
        record.uid = debtInfo.uid;
        record.size = debtInfo.size / KB_TO_BYTES;
        record.lentNode = debtInfo.lentNodeId;
        record.lentMemId = debtInfo.lentMemId;
        // lentSocketIdList加上空校验
        if (debtInfo.lentSocketIdList.size() == 0 || debtInfo.borrowSocketIdList.size() == 0) {
            LOG_ERROR << "[MemLedger] [BorrowRecords] SocketIdList is empty.";
            return MEM_POOLING_ERROR;
        }
        record.lentSocketId = debtInfo.lentSocketIdList[0];
        record.borrowSocketId = debtInfo.borrowSocketIdList[0];
        size_t n = std::min(debtInfo.lentNumaIdList.size(), debtInfo.lentNumaSizeList.size());
        for (size_t i = 0; i < n; ++i) {
            LentNuma ln;
            ln.numaId = static_cast<uint16_t>(debtInfo.lentNumaIdList[i]);
            ln.lentSize = debtInfo.lentNumaSizeList[i];
            record.lentNuma.push_back(ln);
        }
        record.borrowNode = debtInfo.borrowNodeId;
        errno_t res = memcpy_s(&record.borrowLocalNuma, sizeof(record.borrowLocalNuma), debtInfo.usrInfo,
                               sizeof(record.borrowLocalNuma));
        if (res != EOK) {
            LOG_ERROR << "[MemLedger] [BorrowRecords] memcpy_s failed.";
        }
        record.borrowRemoteNuma = static_cast<int16_t>(debtInfo.remoteNumaId);
        record.borrowMemId = debtInfo.borrowMemId;
        gBorrowRecords.push_back(record);
    }
    for (auto& record : gBorrowRecords) {
        LOG_DEBUG << "[MemLedger] [BorrowRecords] Collected borrowRecords: " << record.ToString() << ".";
    }
    return MEM_POOLING_OK;
}

MpResult BorrowRecordHelper::UpdateBorrowRecordsWithFault(const std::string nodeId,
                                                          std::vector<UbseNumaMemoryDebtInfo>& debtInfos)
{
    constexpr int maxRetryTimes = 30;
    constexpr int sleepSeconds = 1;
    int curRetryTimes = 0;

    while (curRetryTimes < maxRetryTimes) {
        debtInfos.clear();
        auto ret = UbseGetNumaMemDebtInfoWithNode(nodeId, debtInfos);
        if (ret != UBSE_OK) {
            LOG_WARN << "[MemLedger][BorrowRecords] UbseGetNumaMemDebtInfo failed, retry=" << (curRetryTimes + 1)
                     << ", ret=" << static_cast<uint32_t>(ret) << ".";
            std::this_thread::sleep_for(std::chrono::seconds(sleepSeconds));
            curRetryTimes++;
            continue;
        }

        // 成功
        break;
    }

    if (curRetryTimes >= maxRetryTimes) {
        LOG_ERROR << "[MemLedger][BorrowRecords] UbseGetNumaMemDebtInfo failed after max retry.";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

MpResult BorrowRecordHelper::CollectBorrowRecordsWithFault(const std::string nodeId,
                                                           std::vector<BorrowRecord>& borrowRecords)
{
    LOG_DEBUG << "[MemLedger] [BorrowRecords] Start to collect borrow records of node_id=" << nodeId.c_str() << ".";
    std::vector<UbseNumaMemoryDebtInfo> debtInfos;
    auto ret = BorrowRecordHelper::Instance().UpdateBorrowRecordsWithFault(nodeId, debtInfos);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[MemLedger] [BorrowRecords] CollectBorrowRecords failed when update BorrowRecords.";
        return ret;
    }

    for (auto& debtInfo : debtInfos) {
        BorrowRecord record;
        record.name = debtInfo.name;
        record.username = debtInfo.username;
        record.uid = debtInfo.uid;
        record.size = debtInfo.size / KB_TO_BYTES;
        record.lentNode = debtInfo.lentNodeId;
        record.lentMemId = debtInfo.lentMemId;
        // lentSocketIdList加上空校验
        if (debtInfo.lentSocketIdList.size() == 0 || debtInfo.borrowSocketIdList.size() == 0) {
            LOG_ERROR << "[MemLedger] [BorrowRecords] SocketIdList is empty.";
            return MEM_POOLING_ERROR;
        }
        record.lentSocketId = debtInfo.lentSocketIdList[0];
        record.borrowSocketId = debtInfo.borrowSocketIdList[0];
        size_t n = std::min(debtInfo.lentNumaIdList.size(), debtInfo.lentNumaSizeList.size());
        for (size_t i = 0; i < n; ++i) {
            LentNuma ln;
            ln.numaId = static_cast<uint16_t>(debtInfo.lentNumaIdList[i]);
            ln.lentSize = debtInfo.lentNumaSizeList[i];
            record.lentNuma.push_back(ln);
        }
        record.borrowNode = debtInfo.borrowNodeId;
        errno_t res = memcpy_s(&record.borrowLocalNuma, sizeof(record.borrowLocalNuma), debtInfo.usrInfo,
                               sizeof(record.borrowLocalNuma));
        if (res != EOK) {
            LOG_ERROR << "[MemLedger] [BorrowRecords] memcpy_s failed.";
        }
        record.borrowRemoteNuma = static_cast<int16_t>(debtInfo.remoteNumaId);
        record.borrowMemId = debtInfo.borrowMemId;
        borrowRecords.push_back(record);
    }

    LOG_DEBUG << "[MemLedger] [BorrowRecords] Collect borrow records with nodeId=" << nodeId.c_str()
              << " finished, get " << borrowRecords.size() << " records.";
    for (auto& record : borrowRecords) {
        LOG_DEBUG << "[MemLedger] [BorrowRecords] Collected borrowRecords: " << record.ToString() << ".";
    }

    return MEM_POOLING_OK;
}

MpResult BorrowRecordHelper::CollectBorrowRecords(const std::string nodeId, std::vector<BorrowRecord>& borrowRecords)
{
    auto ret = BorrowRecordHelper::Instance().UpdateBorrowRecords();
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[MemLedger] [BorrowRecords] CollectBorrowRecords failed when update gBorrowRecords.";
        return ret;
    }
    LOG_DEBUG << "[MemLedger] [BorrowRecords] Start to collect borrow records of node_id=" << nodeId.c_str() << ".";
    borrowRecords.clear();
    for (const auto& record : gBorrowRecords) {
        if (record.borrowNode == nodeId || record.lentNode == nodeId) {
            borrowRecords.push_back(record);
        }
    }
    LOG_DEBUG << "[MemLedger] [BorrowRecords] Collect borrow records finished, get " << borrowRecords.size()
              << " records.";
    for (auto& record : borrowRecords) {
        LOG_DEBUG << "[MemLedger] [BorrowRecords] Collected borrowRecord: " << record.ToString() << ".";
    }
    return MEM_POOLING_OK;
}

MpResult BorrowRecordHelper::CollectBorrowRecordsOnlyBorrowIn(const std::string nodeId, const int& numaId,
                                                              std::vector<BorrowRecord>& borrowRecords)
{
    LOG_DEBUG << "[MemLedger][BorrowRecords] CollectBorrowRecordsOnlyBorrowIn start, nodeId=" << nodeId
              << ", numaId=" << numaId << ".";
    auto ret = CollectBorrowRecords(nodeId, borrowRecords);
    if (ret != MEM_POOLING_OK) {
        return ret;
    }
    uint16_t socketId = 0;
    if (numaId != -1 && MemManager::GetSocketId(nodeId, numaId, socketId) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    std::vector<BorrowRecord> borrowRecordsBorrowIn;
    std::unordered_map<std::string, BorrowItem> timeoutNodeInfo;
    MemReturnManager::Instance().QueryAll(timeoutNodeInfo);
    std::unordered_set<std::string> timeoutBorrowIdSet;
    // 排除归还超时的borrowId
    for (const auto& [borrowId, _] : timeoutNodeInfo) {
        LOG_DEBUG << "[MemLedger][BorrowRecords] borrowId=" << borrowId << " is timeout, need to be ignored.";
        timeoutBorrowIdSet.insert(borrowId);
    }
    for (auto& record : borrowRecords) {
        if (record.borrowNode == nodeId && !timeoutBorrowIdSet.count(record.name) &&
            (numaId == -1 ||
             MemManager::Instance().JudgeSampPlane(nodeId, socketId, record.lentNode, record.lentSocketId))) {
            LOG_DEBUG << "[MemLedger][BorrowRecords] borrowRecordsBorrowIn=" << record.ToString() << " will be moved.";
            borrowRecordsBorrowIn.push_back(record);
        }
    }
    borrowRecords = std::move(borrowRecordsBorrowIn);
    return MEM_POOLING_OK;
}

MpResult BorrowRecordHelper::CollectBorrowRecordsAll(std::vector<BorrowRecord>& borrowRecords, bool isFault,
                                                     bool isFilter)
{
    LOG_INFO << "[MemLedger] [BorrowRecords] Collect all borrowRecords, isFault=" << isFault << ".";
    MpResult ret = MEM_POOLING_OK;
    if (isFault) {
        ret = UpdateBorrowRecordsAllWithFault();
    } else {
        ret = UpdateBorrowRecords(isFilter);
    }

    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[MemLedger] [BorrowRecords] Collect All BorrowRecords failed when updateBorrowRecords.";
        return ret;
    }
    borrowRecords.clear();
    for (const auto& record : gBorrowRecords) {
        borrowRecords.push_back(record);
    }
    return MEM_POOLING_OK;
}

MpResult BorrowRecordHelper::GetBorrowIdByNumaId(std::vector<std::string>& borrowIds, const uint16_t numaId,
                                                 const std::string nodeId)
{
    LOG_DEBUG << "[MemLedger] [BorrowRecords] Start to get borrowIds by NumaId: " << numaId << "on Node: " << nodeId
              << ".";
    std::vector<BorrowRecord> borrowRecords;
    auto ret = CollectBorrowRecords(nodeId, borrowRecords);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[MemLedger] [BorrowRecords] CollectBorrowRecords failed.";
        return ret;
    }
    for (const auto& record : borrowRecords) {
        if (record.borrowRemoteNuma == numaId) {
            borrowIds.push_back(record.name);
        }
    }
    if (borrowIds.empty()) {
        LOG_WARN << "[MemLedger] [BorrowRecords] No borrowIds found by numaId: " << numaId
                 << " on node: " << nodeId.c_str() << ".";
    }
    LOG_DEBUG << "[MemLedger] [BorrowRecords] Get " << borrowIds.size() << " borrowIds by NumaId: " << numaId << ".";
    for (const auto& borrowId : borrowIds) {
        LOG_DEBUG << "[MemLedger] [BorrowRecords] NumaId: " << numaId << ", borrowId: " << borrowId << ".";
    }
    return MEM_POOLING_OK;
}

MpResult MemReturnManager::Update(const std::string& borrowId, BorrowItem& value)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[PersistentStore][MemReturnManager] Update of return timeout borrowId= " << borrowId << " start.";
    std::unique_lock<std::shared_mutex> lock(mtxMemReturnManager);
    borrowCache[borrowId] = value;
    UbseByteBuffer buffer;
    RmrsOutStream builder;
    builder << borrowCache;
    buffer.data = builder.GetBufferPointer();
    buffer.len = builder.GetSize();
    auto ret = UbseStoragePutData("mempooling", "_returnrecords", &buffer);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[PersistentStore][MemReturnManager] Update of return timeout borrowId failed, borrowId=" << borrowId;
        delete[] buffer.data;
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[PersistentStore][MemReturnManager] Update of return timeout borrowId successfully.";
    lock.unlock();
    delete[] buffer.data;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[PersistentStore][MemReturnManager] Update of return timeout borrowId finished.";
    return MEM_POOLING_OK;
}

MpResult MemReturnManager::Remove(const std::string& borrowId)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[PersistentStore][MemReturnManager] Remove return timeout record start, borrowId=" << borrowId;
    std::unique_lock<std::shared_mutex> lock(mtxMemReturnManager);
    auto it = borrowCache.find(borrowId);
    if (it == borrowCache.end()) {
        UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[PersistentStore][MemReturnManager] BorrowId=" << borrowId << " has been removed.";
        return MEM_POOLING_OK;
    }
    borrowCache.erase(it);
    RmrsOutStream builder;
    builder << borrowCache;
    UbseByteBuffer buffer;
    buffer.data = builder.GetBufferPointer();
    buffer.len = builder.GetSize();
    auto ret = UbseStoragePutData("mempooling", "_returnrecords", &buffer);
    if (ret != MEM_POOLING_OK) {
        delete[] buffer.data;
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[PersistentStore][MemReturnManager] UbseStoragePutData failed.";
        return MEM_POOLING_ERROR;
    }
    lock.unlock();
    delete[] buffer.data;
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[PersistentStore][MemReturnManager] Remove return timeout record finished.";
    return MEM_POOLING_OK;
}

MpResult MemReturnManager::Query(const std::string& borrowId, BorrowItem& value)
{
    std::shared_lock<std::shared_mutex> lock(mtxMemReturnManager);

    if (borrowCache.find(borrowId) != borrowCache.end()) {
        value = borrowCache[borrowId];
    } else {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[PersistentStore][MemReturnManager] BorrowId=" << borrowId << " is not exist in return timeout records";
    }
    return MEM_POOLING_OK;
}

MpResult MemReturnManager::QueryAll(std::unordered_map<std::string, BorrowItem>& borrowCacheAll)
{
    std::shared_lock<std::shared_mutex> lock(mtxMemReturnManager);
    borrowCacheAll = borrowCache;
    return MEM_POOLING_OK;
}

MpResult MemReturnManager::GetRawData(UbseByteBuffer& buffer)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[PersistentStore][MemReturnManager] Get borrowCache start.";
    std::shared_lock<std::shared_mutex> lock(mtxMemReturnManager);
    RmrsOutStream builder;
    builder << borrowCache;
    buffer.data = builder.GetBufferPointer();
    if (buffer.data == nullptr) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[PersistentStore][MemReturnManager] GetRawData failed.";
        return MEM_POOLING_ERROR;
    }
    buffer.len = builder.GetSize();
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[PersistentStore][MemReturnManager] GetBorrowIdRedirectionRawData finished.";
    return MEM_POOLING_OK;
}

MpResult MemReturnManager::PutRawData(UbseByteBuffer& buffer)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[PersistentStore][MemReturnManager] Put borrowCache start.";
    std::unique_lock<std::shared_mutex> lock(mtxMemReturnManager);
    RmrsInStream builderIn(buffer.data, buffer.len);
    borrowCache.clear();
    builderIn >> borrowCache;
    auto ret = UbseStoragePutData("mempooling", "_returnrecords", &buffer);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[PersistentStore][MemReturnManager] UbseStoragePutData failed.";
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[PersistentStore][MemReturnManager] Put borrowCache finished.";
    return MEM_POOLING_OK;
}

bool MemReturnManager::IsAllReturned(const std::string& srcNid, const uint16_t& srcRemoteNumaId)
{
    std::shared_lock<std::shared_mutex> lock(mtxMemReturnManager);
    for (const auto& kv : borrowCache) {
        const BorrowItem& item = kv.second;
        if (item.srcNid == srcNid && item.srcRemoteNumaId == srcRemoteNumaId) {
            return false;
        }
    }
    return true;
}

MpResult Name2VmInfo::Update(const std::string& nodeId, std::map<std::string, std::set<BorrowIdInfo>>& value)
{
    std::unique_lock<std::mutex> lock(mtx);
    LOG_DEBUG << "[PersistentStore][Name2Pids] Update operation.";
    vmInfoData[nodeId] = value;
    RmrsOutStream builder;
    builder << vmInfoData;
    UbseByteBuffer buffer;
    buffer.data = builder.GetBufferPointer();
    buffer.len = builder.GetSize();
    LOG_DEBUG << "[PersistentStore][Name2Pids] UbseStoragePutData start.";
    auto ret = UbseStoragePutData("mempooling", "_name2vminfo", &buffer);
    delete[] buffer.data;
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PersistentStore][Name2Pids] UbseStoragePutData failed.";
        return MEM_POOLING_ERROR;
    };
    lock.unlock(); // 在持久化之后，数据同步之前释放mempooling的数据锁，避免与rackManager的服务锁形成死锁
    return MEM_POOLING_OK;
}

MpResult Name2VmInfo::Query(const std::string& nodeId, std::map<std::string, std::set<BorrowIdInfo>>& value)
{
    std::lock_guard<std::mutex> lock(mtx);
    if (vmInfoData.find(nodeId) != vmInfoData.end()) {
        value = vmInfoData[nodeId];
    }
    return MEM_POOLING_OK;
}

MpResult Name2VmInfo::GetRawData(UbseByteBuffer& buffer)
{
    LOG_DEBUG << "[PersistentStore][Name2Pids] Start.";
    std::lock_guard<std::mutex> locker(mtx);
    RmrsOutStream builder;
    builder << vmInfoData;
    buffer.data = builder.GetBufferPointer();
    if (buffer.data == nullptr) {
        LOG_ERROR << "[PersistentStore][Name2Pids] GetRawData failed.";
        return MEM_POOLING_ERROR;
    }
    buffer.len = builder.GetSize();
    LOG_DEBUG << "[PersistentStore][Name2Pids] End.";
    return MEM_POOLING_OK;
}

MpResult Name2VmInfo::PutRawData(UbseByteBuffer& buffer)
{
    LOG_DEBUG << "[PersistentStore][Name2Pids] PutName2VmInfoRawData start.";
    std::lock_guard<std::mutex> locker(mtx);
    auto ret = UbseStoragePutData("mempooling", "_name2vminfo", &buffer);
    if (ret != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    RmrsInStream builderIn(buffer.data, buffer.len);
    vmInfoData.clear();
    builderIn >> vmInfoData;
    LOG_DEBUG << "[PersistentStore][Name2Pids] PutName2VmInfoRawData end.";
    return MEM_POOLING_OK;
}

void MemManager::UpdateNodeMemMap(const std::unordered_map<std::string, NodeMemoryInfoWithReservedMem>& srcMap)
{
    nodeMemMap.clear();
    for (const auto& [nodeId, info] : srcMap) {
        auto& dst = nodeMemMap[nodeId];
        dst.totalReservedMem = info.reservedMem * KB_TO_BYTES;
        dst.totalBorrowableMem = (info.reservedMem - info.lentMemory - info.sharedMem) * KB_TO_BYTES;
        dst.totalLentMem = info.lentMemory * KB_TO_BYTES;
        dst.timestamp = info.timestamp;

        for (const auto& numa : info.numaMemInfo) {
            NumaMemInfo mem{};
            mem.numaId = numa.numaId;
            mem.socketId = numa.socketId;
            mem.reservedMem = numa.reservedMem * KB_TO_BYTES;
            mem.lentMem = numa.lentMem * KB_TO_BYTES;
            mem.borrowableMem = (numa.reservedMem - numa.lentMem - numa.sharedMem) * KB_TO_BYTES;
            mem.memFree = numa.memFree * KB_TO_BYTES;
            mem.vmMemFree = numa.vmMemFree * KB_TO_BYTES;
            (void)dst.localnumaMemInfo.emplace_back(mem);
        }
    }
    for (const auto& [nodeId, memInfo] : nodeMemMap) {
        LOG_DEBUG << "[MemManager] NodeId=" << nodeId << ".";
        LOG_DEBUG << "[MemManager] " << memInfo.ToString();
    }
}

void GetAllNodeInfoImmediatelyResHandler(void* ctx, const UbseByteBuffer& respData, uint32_t resCode)
{
    if (ctx == nullptr || respData.data == nullptr || respData.len == 0) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE) << "Ctx or respData is null.";
        return;
    }
    std::map<std::string, std::vector<mempooling::exportV2::NumaInfo>>& nodeInfoMap =
        *(static_cast<std::map<std::string, std::vector<mempooling::exportV2::NumaInfo>>*>(ctx));
    RmrsInStream builder(respData.data, respData.len);
    builder >> nodeInfoMap;
    return;
}

void FillNodeMemoryInfoWithReservedMem(
    const std::unordered_map<std::string, std::vector<mempooling::exportV2::NumaInfoWithReservedMem>>&
        nodeInfoMapWithReservedMem,
    std::unordered_map<std::string, NodeMemoryInfoWithReservedMem>& nodeMemoryInfoWithReservedMemList)
{
    long basePageSize = MpConfiguration::GetInstance().GetBasePageSize();
    uint64_t hugePageNumToKb = (basePageSize == PAGE_64K_BYTES) ? HUGE_PAGE_NUM_64K_TO_KB : HUGE_PAGE_NUM_4K_TO_KB;
    for (const auto& kv : nodeInfoMapWithReservedMem) {
        const std::string& nodeId = kv.first;
        const auto& numaInfos = kv.second;

        if (numaInfos.empty()) {
            continue; // 没有NUMA信息则跳过
        }

        NodeMemoryInfoWithReservedMem nodeInfo;
        nodeInfo.nodeId = nodeId;
        nodeInfo.timestamp = numaInfos.front().timestamp;
        nodeInfo.socketId = numaInfos.front().metaData.socketId;
        nodeInfo.totalMemory = 0;
        nodeInfo.usedMemory = 0;
        nodeInfo.freeMemory = 0;
        nodeInfo.lentMemory = 0;
        nodeInfo.reservedMem = 0;
        nodeInfo.sharedMem = 0;
        nodeInfo.canBorrowMem = 0;
        nodeInfo.numaMemInfo.clear();

        for (const auto& numa : numaInfos) {
            RackNumaMemInfo rack;

            rack.numaId = numa.metaData.numaId;
            rack.socketId = numa.metaData.socketId;
            rack.memTotal = numa.metaData.memTotal;
            rack.memFree = numa.metaData.memFree;
            rack.memUsed = rack.memTotal - rack.memFree;
            auto it = numa.metaData.numaPageInfo.find(hugePageNumToKb);
            if (it != numa.metaData.numaPageInfo.end()) {
                rack.vmMemTotal = it->second.hugePageTotal * hugePageNumToKb;
                rack.vmMemFree = it->second.hugePageFree * hugePageNumToKb;
            } else {
                rack.vmMemTotal = 0;
                rack.vmMemFree = 0;
            }
            rack.vmUsedMem = rack.vmMemTotal - rack.vmMemFree;

            rack.reservedMem = numa.reservedMem;
            rack.lentMem = numa.lentMem;
            rack.sharedMem = numa.sharedMem;
            rack.canBorrowMem = numa.borrowableMem;

            nodeInfo.numaMemInfo.push_back(rack);
            // 聚合NUMA信息
            nodeInfo.totalMemory += rack.memTotal;
            nodeInfo.usedMemory += rack.memUsed;
            nodeInfo.freeMemory += rack.memFree;
            nodeInfo.lentMemory += rack.lentMem;
            nodeInfo.reservedMem += rack.reservedMem;
            nodeInfo.sharedMem += rack.sharedMem;
            nodeInfo.canBorrowMem += rack.canBorrowMem;
        }
        // 填入结果map
        nodeMemoryInfoWithReservedMemList[nodeId] = std::move(nodeInfo);
    }
}

MpResult MemManager::InitBorrowableInfo()
{
    std::map<std::string, std::vector<mempooling::exportV2::NumaInfo>> nodeInfoMap;
    UbseRoleInfo roleInfo;
    if (UbseGetMasterInfo(roleInfo) != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "UbseGetMasterInfo failed.";
        return MEM_POOLING_ERROR;
    }
    UbseComEndpoint endpoint = {
        .moduleId = MP_MODULE_CODE, .serviceId = message::OPCODE_GET_ALL_NODEINFO, .address = roleInfo.nodeId};
    UbseByteBuffer reqData = {.data = new (std::nothrow) uint8_t[1], .len = 1, .freeFunc = nullptr};
    // 用同步接口，回调函数里记录结果
    uint32_t retRpc = UbseRpcSend(endpoint, reqData, &nodeInfoMap, GetAllNodeInfoImmediatelyResHandler);
    delete[] reqData.data;
    if (retRpc != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "UbseRpcSend failed , get all nodeInfo failed.";
        return MEM_POOLING_ERROR;
    }
    for (auto& [nodeId, numaList] : nodeInfoMap) {
        LOG_DEBUG << "[MemManager][nodeInfoMap] NodeId=" << nodeId;
        for (const auto& numaInfo : numaList) {
            LOG_DEBUG << "[MemManager][nodeInfoMap]" << numaInfo.ToString();
        }
    }
    long basePageSize = MpConfiguration::GetInstance().GetBasePageSize();
    uint64_t hugePageNumToKb = (basePageSize == PAGE_64K_BYTES) ? HUGE_PAGE_NUM_64K_TO_KB : HUGE_PAGE_NUM_4K_TO_KB;
    std::vector<UbseNodeNumaInfo> numaNodeInfoList;
    UbseResult ret = UbseGetAllNodeNumaInfo(numaNodeInfoList);
    if (ret != UBSE_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "UbseGetAllNodeNumaInfo failed.";
        return MEM_POOLING_ERROR;
    }
    std::unordered_map<std::string, std::vector<mempooling::exportV2::NumaInfoWithReservedMem>>
        nodeInfoMapWithReservedMem;

    // 1. 构建辅助索引 (nodeId + "_" + numaId) → 保留相关信息
    std::unordered_map<std::string, ReservedInfo> reservedInfoMap;
    reservedInfoMap.reserve(numaNodeInfoList.size());
    for (const auto& node : numaNodeInfoList) {
        std::string key = node.nodeId + "_" + std::to_string(node.numaId);
        reservedInfoMap[key] = {node.mReservedMemRatio, node.memLent, node.memShared};
    }
    // 2. 遍历 nodeInfoMap，构建带 reservedMem 的新表
    for (const auto& pair : nodeInfoMap) {
        const std::string& nodeKey = pair.first;
        const auto& numaInfoVec = pair.second;

        std::vector<mempooling::exportV2::NumaInfoWithReservedMem> extendedVec;
        extendedVec.reserve(numaInfoVec.size());

        for (const auto& info : numaInfoVec) {
            const auto& meta = info.metaData;
            std::string key = meta.nodeId + "_" + std::to_string(meta.numaId);
            uint64_t reservedMem = 0;
            uint64_t borrowableMem = 0;
            uint64_t lentMem = 0;
            uint64_t sharedMem = 0;
            auto it = reservedInfoMap.find(key);
            if (it != reservedInfoMap.end()) {
                const auto& ri = it->second;
                // 基础保留内存 = 大页数量 × 2MB × 比例
                lentMem = ri.memLent / MB_TO_KBYTES;
                sharedMem = ri.memShared / MB_TO_KBYTES;
                auto ix = meta.numaPageInfo.find(hugePageNumToKb);
                if (ix != meta.numaPageInfo.end()) {
                    reservedMem = ix->second.hugePageTotal * hugePageNumToKb * ri.reservedRatio / NUM_TO_RATIO;
                    borrowableMem =
                        std::min(reservedMem - lentMem - sharedMem, ix->second.hugePageFree * hugePageNumToKb);
                } else {
                    reservedMem = 0;
                    borrowableMem = 0;
                }
            }
            (void)extendedVec.emplace_back(info, reservedMem, borrowableMem, lentMem, sharedMem);
        }
        (void)nodeInfoMapWithReservedMem.emplace(nodeKey, std::move(extendedVec));
    }
    std::unordered_map<std::string, NodeMemoryInfoWithReservedMem> nodeMemoryInfoWithReservedMemList;
    FillNodeMemoryInfoWithReservedMem(nodeInfoMapWithReservedMem, nodeMemoryInfoWithReservedMemList);
    for (auto& [nodeId, info] : nodeMemoryInfoWithReservedMemList) {
        LOG_DEBUG << "[MemManager] NodeId=" << nodeId << ".";
        LOG_DEBUG << "[MemManager] " << info.ToString();
    }
    UpdateNodeMemMap(nodeMemoryInfoWithReservedMemList);
    return MEM_POOLING_OK;
}

MpResult BorrowRecordHelper::CollectBorrowableInfo(const std::string& nodeId,
                                                   NodeMemoryInfoWithReservedMem& nodeMemoryInfoWithReservedMem)
{
    std::map<std::string, std::vector<mempooling::exportV2::NumaInfo>> nodeInfoMap;
    UbseRoleInfo roleInfo;
    if (UbseGetMasterInfo(roleInfo) != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "UbseGetMasterInfo failed.";
        return MEM_POOLING_ERROR;
    }
    UbseComEndpoint endpoint = {
        .moduleId = MP_MODULE_CODE, .serviceId = message::OPCODE_GET_ALL_NODEINFO, .address = roleInfo.nodeId};
    UbseByteBuffer reqData = {.data = new (std::nothrow) uint8_t[1], .len = 1, .freeFunc = nullptr};
    // 用同步接口，回调函数里记录结果
    uint32_t retRpc = UbseRpcSend(endpoint, reqData, &nodeInfoMap, GetAllNodeInfoImmediatelyResHandler);
    delete[] reqData.data;
    if (retRpc != 0) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "UbseRpcSend failed , get all nodeInfo failed.";
        return MEM_POOLING_ERROR;
    }
    // 然后获取所有节点memory_info
    std::vector<UbseNodeNumaInfo> numaNodeInfoList;
    UbseResult ret = UbseGetAllNodeNumaInfo(numaNodeInfoList);
    if (ret != UBSE_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "UbseGetAllNodeNumaInfo failed.";
        return MEM_POOLING_ERROR;
    }
    std::unordered_map<std::string, std::vector<mempooling::exportV2::NumaInfoWithReservedMem>>
        nodeInfoMapWithReservedMem;

    // 1. 构建辅助索引 (nodeId + "_" + numaId) → 保留相关信息
    std::unordered_map<std::string, ReservedInfo> reservedInfoMap;
    reservedInfoMap.reserve(numaNodeInfoList.size());
    for (const auto& node : numaNodeInfoList) {
        std::string key = node.nodeId + "_" + std::to_string(node.numaId);
        reservedInfoMap[key] = {node.mReservedMemRatio, node.memLent, node.memShared};
    }
    long basePageSize = MpConfiguration::GetInstance().GetBasePageSize();
    uint64_t hugePageNumToKb = (basePageSize == PAGE_64K_BYTES) ? HUGE_PAGE_NUM_64K_TO_KB : HUGE_PAGE_NUM_4K_TO_KB;
    // 2. 遍历 nodeInfoMap，构建带 reservedMem 的新表
    for (const auto& pair : nodeInfoMap) {
        const std::string& nodeKey = pair.first;
        const auto& numaInfoVec = pair.second;

        std::vector<mempooling::exportV2::NumaInfoWithReservedMem> extendedVec;
        extendedVec.reserve(numaInfoVec.size());

        for (const auto& info : numaInfoVec) {
            const auto& meta = info.metaData;
            std::string key = meta.nodeId + "_" + std::to_string(meta.numaId);
            uint64_t reservedMem = 0;
            uint64_t borrowableMem = 0;
            uint64_t lentMem = 0;
            uint64_t sharedMem = 0;
            auto it = reservedInfoMap.find(key);
            if (it != reservedInfoMap.end()) {
                const auto& ri = it->second;
                // 基础保留内存 = 大页数量 × 2MB × 比例
                lentMem = ri.memLent / MB_TO_KBYTES;
                sharedMem = ri.memShared / MB_TO_KBYTES;
                auto ix = meta.numaPageInfo.find(hugePageNumToKb);
                if (ix != meta.numaPageInfo.end()) {
                    reservedMem = ix->second.hugePageTotal * hugePageNumToKb * ri.reservedRatio / NUM_TO_RATIO;
                    borrowableMem =
                        std::min(reservedMem - lentMem - sharedMem, ix->second.hugePageFree * hugePageNumToKb);
                } else {
                    reservedMem = 0;
                    borrowableMem = 0;
                }
            }
            (void)extendedVec.emplace_back(info, reservedMem, borrowableMem, lentMem, sharedMem);
        }
        (void)nodeInfoMapWithReservedMem.emplace(nodeKey, std::move(extendedVec));
    }
    std::unordered_map<std::string, NodeMemoryInfoWithReservedMem> nodeMemoryInfoWithReservedMemList;
    FillNodeMemoryInfoWithReservedMem(nodeInfoMapWithReservedMem, nodeMemoryInfoWithReservedMemList);
    MemManager::Instance().UpdateNodeMemMap(nodeMemoryInfoWithReservedMemList);
    auto ix = nodeMemoryInfoWithReservedMemList.find(nodeId);
    if (ix == nodeMemoryInfoWithReservedMemList.end()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << " [MemLedger][BorrowableMem] Can't find borrowable info of nodeId=" << nodeId;
        return MEM_POOLING_ERROR;
    }
    nodeMemoryInfoWithReservedMem = ix->second;
    LOG_DEBUG << "[MemLedger][BorrowableMem] Parsed memory-info = " << nodeMemoryInfoWithReservedMem.ToString();
    return MEM_POOLING_OK;
}

MpResult MemManager::GetCanBorrowMemFromUb(RackNumaMemInfo numaMemInfo, uint64_t& canBorrowMem)
{
    uint64_t calMemFree = numaMemInfo.reservedMem - numaMemInfo.sharedMem - numaMemInfo.lentMem;
    uint64_t hugePageFreeMem = numaMemInfo.vmMemFree;
    canBorrowMem = std::min(hugePageFreeMem, calMemFree);
    LOG_DEBUG << "NumaId = " << numaMemInfo.numaId << ", canBorrowMem = " << canBorrowMem;
    return MEM_POOLING_OK;
}

MpResult MemManager::ResolveUbBorrowableInfoList(NodeMemoryInfoWithReservedMem nodeMemoryInfoWithReservedMem,
                                                 std::vector<NodeMemoryInfoWithReservedMem>& nodeMemoryInfoList)
{
    std::set<uint16_t> socketIdSet;
    for (auto& numaMemInfo : nodeMemoryInfoWithReservedMem.numaMemInfo) {
        (void)socketIdSet.insert(numaMemInfo.socketId);
        if (GetCanBorrowMemFromUb(numaMemInfo, numaMemInfo.canBorrowMem)) {
            return MEM_POOLING_ERROR;
        }
    }
    for (uint16_t socketId : socketIdSet) {
        NodeMemoryInfoWithReservedMem socketMem(nodeMemoryInfoWithReservedMem);
        socketMem.socketId = socketId;
        socketMem.reservedMem = 0;
        socketMem.sharedMem = 0;
        socketMem.lentMemory = 0;
        socketMem.numaMemInfo.clear();
        for (auto& numaMemInfo : nodeMemoryInfoWithReservedMem.numaMemInfo) {
            if (numaMemInfo.socketId == socketId) {
                socketMem.reservedMem += numaMemInfo.reservedMem;
                socketMem.sharedMem += numaMemInfo.sharedMem;
                socketMem.lentMemory += numaMemInfo.lentMem;
                socketMem.canBorrowMem += numaMemInfo.canBorrowMem;
                (void)socketMem.numaMemInfo.emplace_back(numaMemInfo);
            }
        }
        (void)nodeMemoryInfoList.emplace_back(socketMem);
    }
    return MEM_POOLING_OK;
}

void MemManager::ResolveHccsBorrowableInfoList(NodeMemoryInfoWithReservedMem& nodeMemoryInfoWithReservedMem)
{
    nodeMemoryInfoWithReservedMem.canBorrowMem =
        std::min(nodeMemoryInfoWithReservedMem.freeMemory, nodeMemoryInfoWithReservedMem.reservedMem -
                                                               nodeMemoryInfoWithReservedMem.sharedMem -
                                                               nodeMemoryInfoWithReservedMem.lentMemory);
    for (RackNumaMemInfo& numaMemInfo : nodeMemoryInfoWithReservedMem.numaMemInfo) {
        numaMemInfo.canBorrowMem =
            std::min(numaMemInfo.memFree, numaMemInfo.reservedMem - numaMemInfo.sharedMem - numaMemInfo.lentMem);
    }
}

MpResult BorrowRecordHelper::CollectBorrowableInfoList(const std::vector<std::string>& nodeId,
                                                       std::vector<NodeMemoryInfoWithReservedMem>& nodeMemoryInfoList)
{
    for (auto& node : nodeId) {
        NodeMemoryInfoWithReservedMem nodeMemoryInfoWithReservedMem;
        auto ret = CollectBorrowableInfo(node, nodeMemoryInfoWithReservedMem);
        if (ret != MEM_POOLING_OK) {
            nodeMemoryInfoList.clear();
            return MEM_POOLING_ERROR;
        }
#ifdef UB_ENVIRONMENT
        LOG_INFO << "[FaultManager] [FaultLentNode]UB CollectBorrowableInfoList";
        LOG_DEBUG << "[FaultManager] [FaultLentNode] NodeId = " << node;
        if (MemManager::ResolveUbBorrowableInfoList(nodeMemoryInfoWithReservedMem, nodeMemoryInfoList)) {
            return MEM_POOLING_ERROR;
        }
#else
        LOG_INFO << "[FaultManager] [FaultLentNode]HCCS CollectBorrowableInfoList";
        MemManager::ResolveHccsBorrowableInfoList(nodeMemoryInfoWithReservedMem);
        (void)nodeMemoryInfoList.emplace_back(nodeMemoryInfoWithReservedMem);
#endif
    }
    return MEM_POOLING_OK;
}

uint32_t GeneratePerNodeNumaSocketMap(const std::vector<MemNodeData>& memNodeDataVec,
                                      std::map<std::string, std::map<int, uint16_t>>& numaSocketMap)
{
    for (const auto& memNodeData : memNodeDataVec) {
        uint16_t socketId;
        try {
            socketId = std::stoi(memNodeData.socket.socketId);
        } catch (const std::invalid_argument& e) {
            LOG_ERROR << "Invalid argument, socket_id=" << memNodeData.socket.socketId;
            return MEM_POOLING_ERROR;
        } catch (const std::out_of_range& e) {
            LOG_ERROR << "Out of range, socket_id=" << memNodeData.socket.socketId;
            return MEM_POOLING_ERROR;
        }
        for (const auto& numa : memNodeData.socket.numas) {
            int numaId;
            try {
                numaId = std::stoi(numa.numaId);
            } catch (const std::invalid_argument& e) {
                LOG_ERROR << "Invalid argument, numa_id=" << numa.numaId;
                return MEM_POOLING_ERROR;
            } catch (const std::out_of_range& e) {
                LOG_ERROR << "Out of range, numa_id=" << numa.numaId;
                return MEM_POOLING_ERROR;
            }
            LOG_DEBUG << "memNodeData.NodeId = " << memNodeData.nodeId << " ,memNodeData.socketId = " << socketId
                      << "memNodeData.numaId = " << numaId;
            numaSocketMap[memNodeData.nodeId][numaId] = socketId;
        }
    }
    return MEM_POOLING_OK;
}

void PrintNumaSocketMap(const std::map<std::string, std::map<int, uint16_t>>& numaSocketMap)
{
    for (auto& pair1 : numaSocketMap) {
        for (const auto& pair2 : pair1.second) {
            LOG_DEBUG << "Node_id=" << pair1.first << "numa_id=" << pair2.first << " socket_id=" << pair2.second;
        }
    }
}

bool GetNumaSocket(const std::map<std::string, std::map<int, uint16_t>>& numaSocketMap, const std::string& nodeId,
                   int numaId, uint16_t& socketId)
{
    auto outerIt = numaSocketMap.find(nodeId);
    if (outerIt == numaSocketMap.end()) {
        LOG_ERROR << "Node_id=" << nodeId << " does not exist";
        return false;
    }

    const auto& innerMap = outerIt->second;
    auto innerIt = innerMap.find(numaId);
    if (innerIt == innerMap.end()) {
        LOG_ERROR << "NumaId : " << numaId << " in nodeId : " << nodeId << " does not exist";
        return false;
    }

    socketId = innerIt->second;
    return true;
}

MpResult MemManager::GenerateNumaSocketMap(std::map<std::string, std::map<int, uint16_t>>& numaSocketMap)
{
    numaSocketMap.clear();
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    uint32_t ret = UbseMemGetTopologyInfo(nodeTopology);
    if (ret != 0) {
        LOG_ERROR << "Get topo from rack failed!";
        return MEM_POOLING_ERROR;
    }
    for (const auto& pair : nodeTopology) {
        ret = GeneratePerNodeNumaSocketMap(pair.second, numaSocketMap);
        if (ret != MEM_POOLING_OK) {
            LOG_ERROR << "Generate per node numa socket map failed!";
            return MEM_POOLING_ERROR;
        }
    }
    PrintNumaSocketMap(numaSocketMap);
    return MEM_POOLING_OK;
}

MpResult MemManager::GetSocketId(const std::string& nodeId, const int& numaId, uint16_t& socketId)
{
    std::map<std::string, std::map<int, uint16_t>> numaSocketMap;
    MpResult ret = GenerateNumaSocketMap(numaSocketMap);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "Generate numa socket map failed!";
        return ret;
    }
    if (!GetNumaSocket(numaSocketMap, nodeId, numaId, socketId)) {
        LOG_ERROR << "Get socketId failed, the input node_id=" << nodeId << ", the input numa_id=" << numaId;
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

bool MemManager::JudgeSampPlane(const std::string& srcNid, const uint16_t& srcSocketId, const std::string& dstNid,
                                const uint16_t& dstSocketId)
{
    std::unordered_map<std::string, std::vector<MemNodeData>> nodeTopology;
    uint32_t ret = UbseMemGetTopologyInfo(nodeTopology);
    if (ret != 0) {
        LOG_ERROR << "Get topo from rack failed!";
        return false;
    }
    std::string srcNidAndSocketId = srcNid + "-" + std::to_string(srcSocketId);
    auto it = nodeTopology.find(srcNidAndSocketId);
    if (it == nodeTopology.end()) {
        LOG_ERROR << "[MemBorrow][MemBorrowStrategy] Can't find " << srcNidAndSocketId << " in nodeTopology";
        return false;
    }
    std::vector<MemNodeData> foundNodeData = it->second;
    std::unordered_map<std::string, std::unordered_set<std::string>> nodeToSocketSet;
    for (const auto& foundNode : foundNodeData) {
        (void)nodeToSocketSet[foundNode.nodeId].insert(foundNode.socket.socketId);
    }
    std::string socketStr = std::to_string(dstSocketId);

    auto ix = nodeToSocketSet.find(dstNid);
    if (ix == nodeToSocketSet.end()) {
        return false; // key不存在
    }
    // 在对应的set里查找socketStr
    const auto& socketSet = ix->second;
    return socketSet.find(socketStr) != socketSet.end();
}

void LoadDataBase(const std::string& keyPrefix, const std::string& key, const UbseByteBuffer& buff, void* ctx)
{
    if (ctx == nullptr) {
        LOG_ERROR << "Ctx ptr is null.";
        return;
    }

    if (buff.data == nullptr) {
        LOG_ERROR << "The ptr of data is null.";
        return;
    }

    if (buff.len == 0) {
        LOG_WARN << "The len of buff is invalid.";
        return;
    }

    UbseByteBuffer& value = *(static_cast<UbseByteBuffer*>(ctx));
    value.len = buff.len;
    value.data = new (std::nothrow) uint8_t[value.len];
    if (value.data == nullptr) {
        LOG_ERROR << "New data failed.";
        return;
    }
    if (memcpy_s(value.data, value.len, buff.data, value.len) != 0) {
        LOG_ERROR << "Memcpy_s failed.";
        delete[] value.data;
        value.data = nullptr;
        return;
    }
    return;
}

uint32_t AntiDataReload()
{
    // 初始化时重新加载反亲和数据到缓存
    UbseByteBuffer antiData{};
    MpResult ret = UbseStorageQueryData("mempooling", "_antiNode", &antiData, LoadDataBase);
    if (ret != MEM_POOLING_OK) {
        if (antiData.data != nullptr) {
            delete[] antiData.data;
        }
        LOG_ERROR << "UbseStorageQueryData antiNode failed.";
        return MEM_POOLING_ERROR;
    }
    if (antiData.len == 0 || antiData.data == nullptr) {
        LOG_DEBUG << "queryed anti data is empty.";
        return MEM_POOLING_OK;
    }
    std::string jsonStr(static_cast<char*>(static_cast<void*>(antiData.data)), antiData.len);
    MpUpdateAntiNodeParam param;
    bool flag = param.FromJson(jsonStr);
    if (!flag) {
        delete[] antiData.data;
        LOG_ERROR << "antinodeParam deserialization  failed.";
        return MEM_POOLING_ERROR;
    }
    mempooling::AntiNode::Instance().SetAntiNodeParam(param);
    delete[] antiData.data;
    return MEM_POOLING_OK;
}

void ResetAndDeleteBuffer(UbseByteBuffer& buffer)
{
    if (buffer.data != nullptr) {
        delete[] buffer.data;
        buffer.data = nullptr;
        buffer.len = 0;
    }
    return;
}

uint32_t SmapEnableCompletedInit(UbseByteBuffer& buffer)
{
    MpResult ret = UbseStorageQueryData(KEYPREFIX_COMMON, KEYPREFIX_SMAPENABLE_COMPLETED, &buffer, LoadDataBase);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PluginInit][SmapEnableCompleted] Failed to query database.";
        ResetAndDeleteBuffer(buffer);
        return MEM_POOLING_ERROR;
    }

    ret = mempooling::SmapEnableCompleted::Instance().PutRawData(buffer);
    ResetAndDeleteBuffer(buffer);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PluginInit][SmapEnableCompleted] Failed to init SmapEnable data.";
        return MEM_POOLING_ERROR;
    }

    std::vector<int16_t> numaIds;
    ret = SmapEnableCompleted::Instance().Query(numaIds);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PluginInit][SmapEnableCompleted] Query failed.";
        return MEM_POOLING_ERROR;
    }

    if (numaIds.size() != 0) {
        LOG_DEBUG << "[PluginInit][SmapEnableCompleted] numaIds.size=" << numaIds.size()
                  << ", Start to execute SmapEnable.";
        for (auto& numaId : numaIds) {
            EnableNodeMsg enableMsg;
            enableMsg.nid = static_cast<int>(numaId);
            enableMsg.enable = SMAP_ENABLE_NUMA;
            auto retSmap = SmapEnableNumaProcess(enableMsg);
            if (retSmap != MEM_POOLING_OK) {
                LOG_WARN << "[PluginInit][SmapEnableCompleted] SmapEnableNumaProcess failed, numaId = " << numaId
                         << ", ret=" << retSmap << ".";
            }
        }
    }

    return MEM_POOLING_OK;
}

uint32_t FaultHandleBorrowedDecisionInit(UbseByteBuffer& buffer)
{
    MpResult ret = UbseStorageQueryData(KEYPREFIX_COMMON, KEYPREFIX_BORROWED_DECISION, &buffer, LoadDataBase);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PluginInit][FaultHandleBorrowedDecision] Failed to query database.";
        ResetAndDeleteBuffer(buffer);
        return MEM_POOLING_ERROR;
    }

    ret = mempooling::FaultHandleBorrowedDecision::Instance().PutRawData(buffer);
    ResetAndDeleteBuffer(buffer);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PluginInit][FaultHandleBorrowedDecision] Failed to init FaultHandleBorrowedDecision data.";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

uint32_t BorrowIdInFaultProcessInit()
{
    MpResult retBorrowIdInFaultProcess = BorrowIdInFaultProcess::Instance().Clear();
    if (retBorrowIdInFaultProcess != MEM_POOLING_OK) {
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemReturn] Clear fault process borrowId failed. ret=" << retBorrowIdInFaultProcess << ".";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

uint32_t RemovePidCompletedInit(UbseByteBuffer& buffer)
{
    MpResult ret = UbseStorageQueryData(KEYPREFIX_COMMON, KEYPREFIX_REMOVEPID_COMPLETED, &buffer, LoadDataBase);
    if (ret != MEM_POOLING_OK) {
        ResetAndDeleteBuffer(buffer);
        LOG_ERROR << "[PluginInit][RemovePidCompleted] Failed to query database.";
        return MEM_POOLING_ERROR;
    }

    ret = mempooling::RemovePidCompleted::Instance().PutRawData(buffer);
    ResetAndDeleteBuffer(buffer);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PluginInit][RemovePidCompleted] Failed to init RemovePid data.";
        return MEM_POOLING_ERROR;
    }

    std::unordered_map<uint16_t, std::unordered_set<pid_t>> numa2RemovePidMap;
    ret = RemovePidCompleted::Instance().Query(numa2RemovePidMap);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PluginInit][RemovePidCompleted] Query failed.";
        return MEM_POOLING_ERROR;
    }

    LOG_DEBUG << "[PluginInit][RemovePidCompleted] RemovePidCompleted.size=" << numa2RemovePidMap.size()
              << ", Start to RemovePid.";
    if (numa2RemovePidMap.size() != 0) {
        for (const auto& [numaId, pidSet] : numa2RemovePidMap) {
            std::vector<pid_t> pids(pidSet.begin(), pidSet.end());
            ret = OverCommitMsgHandler::RemoveLocalHandler(numaId, pids);
            if (ret != MEM_POOLING_OK) {
                LOG_WARN << "[PluginInit][RemovePidCompleted] Remove pids failed, numaId=" << numaId << ", ret=" << ret
                         << ".";
            }
        }
    }

    return MEM_POOLING_OK;
}

uint32_t Name2VmInfoInit(UbseByteBuffer& buffer)
{
    MpResult ret = UbseStorageQueryData("mempooling", "_name2vminfo", &buffer, LoadDataBase);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PluginInit] Failed to init borrowidredirection.";
        ResetAndDeleteBuffer(buffer);
        return MEM_POOLING_ERROR;
    }

    ret = mempooling::Name2VmInfo::Instance().PutRawData(buffer);
    ResetAndDeleteBuffer(buffer);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PluginInit] Failed to init name2vminfo.";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

uint32_t BorrowIdRedirectionInit(UbseByteBuffer& buffer)
{
    MpResult ret = UbseStorageQueryData("mempooling", "_borrowidredirection", &buffer, LoadDataBase);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PluginInit] Failed to query database.";
        ResetAndDeleteBuffer(buffer);
        return MEM_POOLING_ERROR;
    }

    ret = mempooling::BorrowIdRedirection::Instance().PutRawData(buffer);
    ResetAndDeleteBuffer(buffer);
    if (ret != MEM_POOLING_OK) {
        LOG_ERROR << "[PluginInit] Failed to init borrowidredirection.";
        return MEM_POOLING_ERROR;
    }

    return MEM_POOLING_OK;
}

uint32_t DataReloadInit()
{
    UbseByteBuffer buffer;

    // 初始化Name2VmInfo记录
    if (Name2VmInfoInit(buffer) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }

    // 初始化BorrowIdRedirection记录
    if (BorrowIdRedirectionInit(buffer) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }

    // 初始化SmapEnable记录，如果存在需enable的numaId则执行SmapEnable操作
    if (SmapEnableCompletedInit(buffer) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }

    // 初始化BorrowedDecisionMap
    if (FaultHandleBorrowedDecisionInit(buffer) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }

    // 初始化RemovePid记录，如果存在需remove的pid则执行remove操作
    if (RemovePidCompletedInit(buffer) != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }

    // 清空BorrowIdInFaultProcess记录
    if (BorrowIdInFaultProcessInit() != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }

    MpResult ret = UbseStorageQueryData("mempooling", "_returnrecords", &buffer, LoadDataBase);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[PluginInit] Failed to query database.";
        ResetAndDeleteBuffer(buffer);
        return MEM_POOLING_ERROR;
    }

    ret = mempooling::MemReturnManager::Instance().PutRawData(buffer);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[PluginInit] Failed to init borrowCache.";
        ResetAndDeleteBuffer(buffer);
        return MEM_POOLING_ERROR;
    }
    if (buffer.len != 0 && !mempooling::MemReturnScanner::Instance().start()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[PluginInit] Failed to start return scanner.";
        ResetAndDeleteBuffer(buffer);
        return MEM_POOLING_ERROR;
    }
    ResetAndDeleteBuffer(buffer);
    return MEM_POOLING_OK;
}

} // namespace mempooling