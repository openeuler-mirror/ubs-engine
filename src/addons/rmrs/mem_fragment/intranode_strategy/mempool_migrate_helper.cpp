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

#include "mempool_migrate_helper.h"
#include "mempool_migrate_module.h"
#include "mempooling_message.h"
#include "mp_mem_json_util.h"
#include "rmrs_serialize.h"
#include "turbo_def.h"
#include "ubse_com.h"

namespace mempooling {
using namespace ubse::com;
using namespace rmrs::serialize;
using namespace mempooling::migrate;

MpResult ValidBorrowIdPidMap(std::map<std::string, std::set<BorrowIdInfo>> borrowIdsPidsMap)
{
    if (borrowIdsPidsMap.empty()) {
        return MEM_POOLING_OK;
    }
    auto it = borrowIdsPidsMap.begin();
    const auto &baseSet = it->second;
    ++it;
    for (; it != borrowIdsPidsMap.end(); ++it) {
        if (it->second != baseSet) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemRollback] BorrowIds own pids not equals, borrowId= " << it->first << ".";
            return MEM_POOLING_ERROR;
        }
    }
    return MEM_POOLING_OK;
}

MpResult FilterBorrowIds(const std::vector<std::string> &borrowIdsList,
                         std::map<std::string, std::set<BorrowIdInfo>> &borrowIdsPidsMap,
                         std::map<std::string, std::set<BorrowIdInfo>> &validBorrowIdsPidsMap)
{
    size_t existCount = 0;
    for (const std::string &borrowId : borrowIdsList) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemRollback] Input borrowId: " << borrowId << ".";
        if (borrowIdsPidsMap.find(borrowId) != borrowIdsPidsMap.end()) {
            validBorrowIdsPidsMap[borrowId] = borrowIdsPidsMap[borrowId];
            existCount++;
        } else {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MemRollback] BorrowId not exist in name2pid map, borrowId: " << borrowId << ".";
        }
    }
    if (existCount != 0 && existCount != borrowIdsList.size()) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemRollback] Valid borrowId count not match, existCount: " << existCount
            << ", borrowIdsList size: " << borrowIdsList.size() << ".";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult GetRollBackBorrowIdPid(const std::string &nodeId, RollBackBorrowIdPid &entry,
                                std::vector<std::string> borrowIdsList)
{
    // 持久化处取borrowId与pid的映射
    std::map<std::string, std::set<BorrowIdInfo>> borrowIdsPidsMap;
    MpResult res = mempooling::Name2VmInfo::Instance().Query(nodeId, borrowIdsPidsMap);
    if (res != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemRollback] Get borrow pid map faild " << res << ".";
        return MEM_POOLING_ERROR;
    }
    // 取borrowIdsPidsMap和borrowIdsList交集作为待处理的borrowId
    std::map<std::string, std::set<BorrowIdInfo>> validBorrowIdsPidsMap;
    res = FilterBorrowIds(borrowIdsList, borrowIdsPidsMap, validBorrowIdsPidsMap);
    if (res != MEM_POOLING_OK) {
        return MEM_POOLING_ERROR;
    }
    if (ValidBorrowIdPidMap(validBorrowIdsPidsMap)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemRollback] Valid borrowId same batch failed.";
        return MEM_POOLING_ERROR;
    }
    if (validBorrowIdsPidsMap.empty()) {
        // 手动填充borrow映射
        for (const std::string &borrowId : borrowIdsList) {
            (void)validBorrowIdsPidsMap[borrowId].insert(BorrowIdInfo{-1, 0});
            UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemRollback] Manual fill pid " << borrowId << ".";
        }
    }

    std::vector<std::string> borrowIdList;
    std::vector<BorrowIdInfo> pidList;
    for (auto borrowIdPidsEntry : validBorrowIdsPidsMap) {
        if (pidList.empty()) {
            for (auto &item : borrowIdPidsEntry.second) {
                pidList.push_back(item);
            }
        }
        borrowIdList.push_back(borrowIdPidsEntry.first);
    }
    entry.borrowIdList = borrowIdList;
    entry.pidList = pidList;
    return MEM_POOLING_OK;
}

MpResult PersistentBorrowIdPid(std::string &nodeId, RollBackBorrowIdPid &entry)
{
    std::map<std::string, std::set<BorrowIdInfo>> queryMap;
    if (mempooling::Name2VmInfo::Instance().Query(nodeId, queryMap)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "GetName2VmInfo faild ";
        return MEM_POOLING_ERROR;
    }
    std::map<std::string, std::set<BorrowIdInfo>> borrowIdsPidsMap;
    std::set<BorrowIdInfo> pidSet;
    for (std::string borrowId : entry.borrowIdList) {
        if (!pidSet.empty()) {
            borrowIdsPidsMap[borrowId] = pidSet;
            continue;
        }
        auto itMap = queryMap.find(borrowId);
        if (itMap == queryMap.end()) {
            continue;
        }
        for (auto itSet = itMap->second.begin(); itSet != itMap->second.end(); itSet++) {
            if (std::find(entry.pidList.begin(), entry.pidList.end(), *itSet) != entry.pidList.end()) {
                (void)pidSet.insert(BorrowIdInfo{itSet->pid, itSet->oriSize});
            }
        }
        borrowIdsPidsMap[borrowId] = pidSet;
    }
    if (mempooling::Name2VmInfo::Instance().Update(nodeId, borrowIdsPidsMap)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemRollback] Update borrow pid map faild.";
        return MEM_POOLING_ERROR;
    }
    return MEM_POOLING_OK;
}

MpResult RpcMemBorrowRollback(std::string nodeId, const std::vector<std::string> &borrowIdsList)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemRollback] Master to invoke the slave MemBorrow Rollback.";
    RollBackBorrowIdPid inEntry;
    if (GetRollBackBorrowIdPid(nodeId, inEntry, borrowIdsList)) {
        return MEM_POOLING_ERROR;
    }
    MemBorrowRollbackParam param = {nodeId, borrowIdsList, inEntry};
    RollBackForOutEntry rollBackForOutEntry;
    MpResult res =
        migrate::MempoolMigrateExecute::MemBorrowRollbackImpl(param.borrowIdList, param.entry, rollBackForOutEntry);
    if (MEM_POOLING_OK != res) {
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemRollback] Recv MemBorrowRollbackRpc res: " << res << ".";
        return res;
    }
    if (rollBackForOutEntry.errorCode != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MemRollback] MemBorrow Rollback failed, error code is " << rollBackForOutEntry.errorCode << ".";
        return rollBackForOutEntry.errorCode;
    }
    RollBackBorrowIdPid outEntry;
    if (!MempoolMigrateModule::FreeMemAndPersistent(rollBackForOutEntry.validBorrowIdSet,
                                                    rollBackForOutEntry.curBorrowIdsPidsMap, outEntry)) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemRollback] FreeMemAndPersistent failed.";
        return MEM_POOLING_ERROR;
    }

    if (PersistentBorrowIdPid(nodeId, outEntry)) {
        return MEM_POOLING_ERROR;
    }
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE) << "[MemRollback] MemBorrow Rollback success.";
    return MEM_POOLING_OK;
}

MpResult MigrateExecuteInStandbyToMasterEvent(const pid_t pid, const std::string borrowInNode,
                                              const std::string remoteNumaId)
{
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MpElection][StandbyToMaster][MigrateExecute] Begin to migrate vm-" << pid << " back to local.";

    MpResult retRpc = MEM_POOLING_ERROR;
    MpResult retMigrate = MEM_POOLING_OK;
    // 迁出执行失败后，最多重试6次
    for (int i = 1; i <= MAX_RETRY_TIMES; ++i) {
        std::vector<VMMigrateOutParam> vmInfoList;
        std::vector<std::string> borrowIdList;
        (void)vmInfoList.emplace_back(VMMigrateOutParam{pid, 0, static_cast<uint16_t>(std::stoul(remoteNumaId))});
        MigrateExecuteParam migrateExecuteParam = {vmInfoList, MIGRATEOUT_TIMEOUT, borrowIdList};
        retMigrate = MempoolMigrateExecute::MigrateExecuteImpl(
            migrateExecuteParam.vmInfoList, migrateExecuteParam.waitingTime, migrateExecuteParam.borrowIdList);
        if (retMigrate == MEM_POOLING_OK) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpElection][StandbyToMaster][MigrateExecute] Migrate back vm-" << pid << " back to local success.";
            return MEM_POOLING_OK;
        }
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpElection][StandbyToMaster][MigrateExecute] Migrate vms back to local failed, error code is "
            << retMigrate << ", starting No." << i << " retry.";
        sleep(RETRY_SLEEP_TIME_SEC * i);
    }
    UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MpElection][StandbyToMaster][MigrateExecute] After retrying" << MAX_RETRY_TIMES << " times, "
        << "migrate back vm-" << pid << " back to local failed.";

    return retMigrate;
}

MpResult RemoveVmInfosCompletedWithRetry(const pid_t pid)
{
    UBSE_LOGGER_INFO(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MpElection][StandbyToMaster][MigrateExecute] Begin to remove vmInfos in database.";

    MpResult ret = MEM_POOLING_OK;
    // 迁出执行失败后，最多重试6次
    for (int i = 1; i <= MAX_RETRY_TIMES; ++i) {
        ret = VmInfosCompleted::Instance().Remove(pid);
        if (ret == MEM_POOLING_OK) {
            UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpElection][StandbyToMaster][MigrateExecute] Remove vmInfos in database success.";
            return MEM_POOLING_OK;
        }
        UBSE_LOGGER_WARN(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpElection][StandbyToMaster][MigrateExecute] Remove vmInfos in database failed, error code is " << ret
            << ", starting No." << i << " retry.";
        sleep(RETRY_SLEEP_TIME_SEC * i);
    }

    UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MpElection][StandbyToMaster][MigrateExecute] After retrying " << MAX_RETRY_TIMES
        << " times, remove vmInfos in database failed.";

    return ret;
}

MpResult RollBackMigratedVmsInStandbyToMasterEvent()
{
    std::unordered_map<pid_t, std::string> pidListNeedToMigrateBack;
    MpResult ret = VmInfosCompleted::Instance().Query(pidListNeedToMigrateBack);
    if (ret != MEM_POOLING_OK) {
        UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpElection][StandbyToMaster][MigrateExecute] GetVmInfosCompletedMap failed.";
        return ret;
    }
    UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
        << "[MpElection][StandbyToMaster][MigrateExecute] The number of vms need to migrate back is "
        << pidListNeedToMigrateBack.size() << ".";
    for (const auto &pair : pidListNeedToMigrateBack) {
        if (pair.second.empty()) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpElection][StandbyToMaster][MigrateExecute] The value of key " << pair.first << " is empty.";
            continue;
        }
        std::string borrowInNode, remoteNumaId;
        std::istringstream stream(pair.second);
        // 分割字符串
        if (!std::getline(stream, borrowInNode, '-')) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpElection][StandbyToMaster][MigrateExecute] No delimiter '-' found in the data, which key is "
                << pair.first << "and value is " << pair.second << ".";
            continue;
        }
        if (!std::getline(stream, remoteNumaId)) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpElection][StandbyToMaster][MigrateExecute] The string does not "
                   "contain exactly one delimiter '-'. Too few parts. key is "
                << pair.first << "and value is " << pair.second << ".";
            continue;
        }
        UBSE_LOGGER_DEBUG(MP_MODULE_NAME, MP_MODULE_CODE)
            << "[MpElection][StandbyToMaster][MigrateExecute] Pid is " << pair.first
            << ", borrowInNode is : " << borrowInNode << ", remoteNumaId is : " << remoteNumaId << ".";
        ret = MigrateExecuteInStandbyToMasterEvent(pair.first, borrowInNode, remoteNumaId);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpElection][StandbyToMaster][MigrateExecute] Migrate back pid-" << pair.first << " failed.";
        }
        ret = RemoveVmInfosCompletedWithRetry(pair.first);
        if (ret != MEM_POOLING_OK) {
            UBSE_LOGGER_ERROR(MP_MODULE_NAME, MP_MODULE_CODE)
                << "[MpElection][StandbyToMaster][MigrateExecute] Remove vmInfo in database failed, pid is "
                << pair.first << ".";
        }
    }
    return MEM_POOLING_OK;
}

} // namespace mempooling
