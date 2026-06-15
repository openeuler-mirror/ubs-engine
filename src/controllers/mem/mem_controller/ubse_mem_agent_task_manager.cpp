/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 * ubs-engine is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
  *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include "ubse_mem_agent_task_manager.h"

#include <ubse_com_module.h>

#include "ubse_com_base.h"
#include "ubse_error.h"
#include "ubse_logger_module.h"
#include "ubse_mem_controller_api_common.h"
#include "ubse_mem_util.h"
#include "ubse_pointer_process.h"
#include "lock/ubse_lock.h"

namespace ubse::mem::controller {
UBSE_DEFINE_THIS_MODULE("ubse");
using namespace utils;
using namespace log;
using namespace com;
using namespace message;

const std::string TASK_MANGER_LOG_PREFIX = "[Task Manager] ";

std::map<std::string, std::map<uint32_t, UbseMemFdBorrowExportObj>> UbseMemAgentTaskManager::fdExportTaskObjMap{};
std::map<std::string, std::map<uint32_t, UbseMemNumaBorrowExportObj>> UbseMemAgentTaskManager::numaExportTaskObjMap{};
std::map<std::string, std::map<uint32_t, UbseMemShareBorrowExportObj>> UbseMemAgentTaskManager::shareExportTaskObjMap{};
std::map<std::string, std::map<uint32_t, UbseMemAddrBorrowExportObj>> UbseMemAgentTaskManager::addrExportTaskObjMap{};
std::map<std::string, std::map<uint32_t, UbseMemFdBorrowImportObj>> UbseMemAgentTaskManager::fdImportTaskObjMap{};
std::map<std::string, std::map<uint32_t, UbseMemNumaBorrowImportObj>> UbseMemAgentTaskManager::numaImportTaskObjMap{};
std::map<std::string, std::map<uint32_t, UbseMemShareBorrowImportObj>> UbseMemAgentTaskManager::shareImportTaskObjMap{};
std::map<std::string, std::map<uint32_t, UbseMemAddrBorrowImportObj>> UbseMemAgentTaskManager::addrImportTaskObjMap{};
uint32_t UbseMemAgentTaskManager::taskId = 0;

std::map<MemResourceType, std::shared_ptr<ReadWriteLock>> memResourceLockMap = {
    {MemResourceType::FD_EXPORT, SafeMakeShared<ReadWriteLock>()},
    {MemResourceType::NUMA_EXPORT, SafeMakeShared<ReadWriteLock>()},
    {MemResourceType::SHARE_EXPORT, SafeMakeShared<ReadWriteLock>()},
    {MemResourceType::ADDR_EXPORT, SafeMakeShared<ReadWriteLock>()},
    {MemResourceType::FD_IMPORT, SafeMakeShared<ReadWriteLock>()},
    {MemResourceType::NUMA_IMPORT, SafeMakeShared<ReadWriteLock>()},
    {MemResourceType::SHARE_IMPORT, SafeMakeShared<ReadWriteLock>()},
    {MemResourceType::ADDR_IMPORT, SafeMakeShared<ReadWriteLock>()}};
ReadWriteLock taskIdLock;

uint32_t UbseMemAgentTaskManager::GenerateTaskId()
{
    taskIdLock.LockWrite();
    const auto res = taskId++;
    taskIdLock.UnLock();
    return res;
}

template <typename T, MemResourceType ResourceType>
UbseResult AddTask(uint32_t taskId, const T& obj, std::map<std::string, std::map<uint32_t, T>>& taskObjMap)
{
    // 校验taskId
    taskIdLock.LockRead();
    if (taskId > UbseMemAgentTaskManager::GetTaskId()) {
        UBSE_LOG_ERROR << TASK_MANGER_LOG_PREFIX << "TaskId is wrong, " << FormatRetCode(UBSE_ERROR);
        taskIdLock.UnLock();
        return UBSE_ERROR;
    }
    taskIdLock.UnLock();

    const std::string name = obj.req.name;
    memResourceLockMap[ResourceType]->LockWrite();
    // 资源已存在
    if (taskObjMap.find(name) != taskObjMap.end() && taskObjMap[name].find(taskId) != taskObjMap[name].end()) {
        UBSE_LOG_ERROR << TASK_MANGER_LOG_PREFIX << "Resource is exist, name=" << name << ", taskId=" << taskId << ", "
                       << FormatRetCode(UBSE_ERROR);
        memResourceLockMap[ResourceType]->UnLock();
        return UBSE_ERROR;
    }

    // 写入资源
    taskObjMap[name][taskId] = obj;
    memResourceLockMap[ResourceType]->UnLock();
    return UBSE_OK;
}

UbseResult UbseMemAgentTaskManager::DeleteTaskObj(uint32_t taskId)
{
    if (DeleteFdExportTask(taskId)) {
        return UBSE_OK;
    }
    if (DeleteNumaExportTask(taskId)) {
        return UBSE_OK;
    }
    if (DeleteShareExportTask(taskId)) {
        return UBSE_OK;
    }
    if (DeleteAddrExportTask(taskId)) {
        return UBSE_OK;
    }
    if (DeleteFdImportTask(taskId)) {
        return UBSE_OK;
    }
    if (DeleteNumaImportTask(taskId)) {
        return UBSE_OK;
    }
    if (DeleteShareImportTask(taskId)) {
        return UBSE_OK;
    }
    if (DeleteAddrImportTask(taskId)) {
        return UBSE_OK;
    }
    UBSE_LOG_WARN << TASK_MANGER_LOG_PREFIX << "Delete task failed, can't find taskId=" << taskId << ".";
    return UBSE_ERROR;
}

uint32_t UbseMemAgentTaskManager::GetTaskId()
{
    return taskId;
}

UbseResult UbseMemAgentTaskManager::AddFdExportTask(uint32_t taskId, const UbseMemFdBorrowExportObj& obj)
{
    return AddTask<UbseMemFdBorrowExportObj, MemResourceType::FD_EXPORT>(taskId, obj, fdExportTaskObjMap);
}

UbseResult UbseMemAgentTaskManager::AddNumaExportTask(uint32_t taskId, const UbseMemNumaBorrowExportObj& obj)
{
    return AddTask<UbseMemNumaBorrowExportObj, MemResourceType::NUMA_EXPORT>(taskId, obj, numaExportTaskObjMap);
}

UbseResult UbseMemAgentTaskManager::AddShareExportTask(uint32_t taskId, const UbseMemShareBorrowExportObj& obj)
{
    return AddTask<UbseMemShareBorrowExportObj, MemResourceType::SHARE_EXPORT>(taskId, obj, shareExportTaskObjMap);
}

UbseResult UbseMemAgentTaskManager::AddAddrExportTask(uint32_t taskId, const UbseMemAddrBorrowExportObj& obj)
{
    return AddTask<UbseMemAddrBorrowExportObj, MemResourceType::ADDR_EXPORT>(taskId, obj, addrExportTaskObjMap);
}

UbseResult UbseMemAgentTaskManager::AddFdImportTask(uint32_t taskId, const UbseMemFdBorrowImportObj& obj)
{
    return AddTask<UbseMemFdBorrowImportObj, MemResourceType::FD_IMPORT>(taskId, obj, fdImportTaskObjMap);
}

UbseResult UbseMemAgentTaskManager::AddNumaImportTask(uint32_t taskId, const UbseMemNumaBorrowImportObj& obj)
{
    return AddTask<UbseMemNumaBorrowImportObj, MemResourceType::NUMA_IMPORT>(taskId, obj, numaImportTaskObjMap);
}

UbseResult UbseMemAgentTaskManager::AddShareImportTask(uint32_t taskId, const UbseMemShareBorrowImportObj& obj)
{
    return AddTask<UbseMemShareBorrowImportObj, MemResourceType::SHARE_IMPORT>(taskId, obj, shareImportTaskObjMap);
}

UbseResult UbseMemAgentTaskManager::AddAddrImportTask(uint32_t taskId, const UbseMemAddrBorrowImportObj& obj)
{
    return AddTask<UbseMemAddrBorrowImportObj, MemResourceType::ADDR_IMPORT>(taskId, obj, addrImportTaskObjMap);
}

template <typename T, MemResourceType ResourceType>
bool DeleteTask(uint32_t taskId, std::map<std::string, std::map<uint32_t, T>>& taskObjMap)
{
    memResourceLockMap[ResourceType]->LockWrite();
    for (auto it = taskObjMap.begin(); it != taskObjMap.end();) {
        if (it->second.find(taskId) != it->second.end()) {
            UBSE_LOG_INFO << TASK_MANGER_LOG_PREFIX << "Delete taskId=" << taskId << " ,name=" << it->first
                          << ", type=" << static_cast<uint32_t>(ResourceType) << ".";
            it->second.erase(taskId);
            if (it->second.empty()) {
                it = taskObjMap.erase(it); // 更新迭代器
            } else {
                ++it; // 继续下一个元素
            }
            memResourceLockMap[ResourceType]->UnLock();
            return true;
        } else {
            ++it; // 继续下一个元素
        }
    }
    memResourceLockMap[ResourceType]->UnLock();
    return false;
}

bool UbseMemAgentTaskManager::DeleteFdExportTask(uint32_t delTaskId)
{
    return DeleteTask<UbseMemFdBorrowExportObj, MemResourceType::FD_EXPORT>(delTaskId, fdExportTaskObjMap);
}

bool UbseMemAgentTaskManager::DeleteNumaExportTask(uint32_t delTaskId)
{
    return DeleteTask<UbseMemNumaBorrowExportObj, MemResourceType::NUMA_EXPORT>(delTaskId, numaExportTaskObjMap);
}

bool UbseMemAgentTaskManager::DeleteShareExportTask(uint32_t delTaskId)
{
    return DeleteTask<UbseMemShareBorrowExportObj, MemResourceType::SHARE_EXPORT>(delTaskId, shareExportTaskObjMap);
}

bool UbseMemAgentTaskManager::DeleteAddrExportTask(uint32_t delTaskId)
{
    return DeleteTask<UbseMemAddrBorrowExportObj, MemResourceType::ADDR_EXPORT>(delTaskId, addrExportTaskObjMap);
}

bool UbseMemAgentTaskManager::DeleteFdImportTask(uint32_t delTaskId)
{
    return DeleteTask<UbseMemFdBorrowImportObj, MemResourceType::FD_IMPORT>(delTaskId, fdImportTaskObjMap);
}

bool UbseMemAgentTaskManager::DeleteNumaImportTask(uint32_t delTaskId)
{
    return DeleteTask<UbseMemNumaBorrowImportObj, MemResourceType::NUMA_IMPORT>(delTaskId, numaImportTaskObjMap);
}

bool UbseMemAgentTaskManager::DeleteShareImportTask(uint32_t delTaskId)
{
    return DeleteTask<UbseMemShareBorrowImportObj, MemResourceType::SHARE_IMPORT>(delTaskId, shareImportTaskObjMap);
}

bool UbseMemAgentTaskManager::DeleteAddrImportTask(uint32_t delTaskId)
{
    return DeleteTask<UbseMemAddrBorrowImportObj, MemResourceType::ADDR_IMPORT>(delTaskId, addrImportTaskObjMap);
}

template <typename T, MemResourceType ResourceType>
UbseResult GetTaskObjTemplate(const std::string& name, const std::string& importNodeId, T& returnObj,
                              const std::map<std::string, std::map<uint32_t, T>>& taskObjMap)
{
    memResourceLockMap[ResourceType]->LockRead();
    std::string key = importNodeId.empty() ? name : GenerateExportObjKey(name, importNodeId);
    if (taskObjMap.find(key) == taskObjMap.end() || taskObjMap.at(key).empty()) {
        UBSE_LOG_WARN << TASK_MANGER_LOG_PREFIX << "name=" << name << ", importNodeId=" << importNodeId
                      << " is not in task manager.";
        memResourceLockMap[ResourceType]->UnLock();
        return UBSE_ERROR;
    }

    returnObj = taskObjMap.at(key).rbegin()->second;
    memResourceLockMap[ResourceType]->UnLock();
    return UBSE_OK;
}

UbseResult UbseMemAgentTaskManager::GetFdExportTaskObj(const std::string& name, const std::string& importNodeId,
                                                       UbseMemFdBorrowExportObj& returnObj)
{
    return GetTaskObjTemplate<UbseMemFdBorrowExportObj, MemResourceType::FD_EXPORT>(name, importNodeId, returnObj,
                                                                                    fdExportTaskObjMap);
}

UbseResult UbseMemAgentTaskManager::GetNumaExportTaskObj(const std::string& name, const std::string& importNodeId,
                                                         UbseMemNumaBorrowExportObj& returnObj)
{
    return GetTaskObjTemplate<UbseMemNumaBorrowExportObj, MemResourceType::NUMA_EXPORT>(name, importNodeId, returnObj,
                                                                                        numaExportTaskObjMap);
}

UbseResult UbseMemAgentTaskManager::GetShareExportTaskObj(const std::string& name,
                                                          UbseMemShareBorrowExportObj& returnObj)
{
    return GetTaskObjTemplate<UbseMemShareBorrowExportObj, MemResourceType::SHARE_EXPORT>(name, "", returnObj,
                                                                                          shareExportTaskObjMap);
}

UbseResult UbseMemAgentTaskManager::GetAddrExportTaskObj(const std::string& name, const std::string& importNodeId,
                                                         UbseMemAddrBorrowExportObj& returnObj)
{
    return GetTaskObjTemplate<UbseMemAddrBorrowExportObj, MemResourceType::ADDR_EXPORT>(name, importNodeId, returnObj,
                                                                                        addrExportTaskObjMap);
}

UbseResult UbseMemAgentTaskManager::GetFdImportTaskObj(const std::string& name, UbseMemFdBorrowImportObj& returnObj)
{
    return GetTaskObjTemplate<UbseMemFdBorrowImportObj, MemResourceType::FD_IMPORT>(name, "", returnObj,
                                                                                    fdImportTaskObjMap);
}

UbseResult UbseMemAgentTaskManager::GetNumaImportTaskObj(const std::string& name, UbseMemNumaBorrowImportObj& returnObj)
{
    return GetTaskObjTemplate<UbseMemNumaBorrowImportObj, MemResourceType::NUMA_IMPORT>(name, "", returnObj,
                                                                                        numaImportTaskObjMap);
}

UbseResult UbseMemAgentTaskManager::GetShareImportTaskObj(const std::string& name,
                                                          UbseMemShareBorrowImportObj& returnObj)
{
    return GetTaskObjTemplate<UbseMemShareBorrowImportObj, MemResourceType::SHARE_IMPORT>(name, "", returnObj,
                                                                                          shareImportTaskObjMap);
}

UbseResult UbseMemAgentTaskManager::GetAddrImportTaskObj(const std::string& name, UbseMemAddrBorrowImportObj& returnObj)
{
    return GetTaskObjTemplate<UbseMemAddrBorrowImportObj, MemResourceType::ADDR_IMPORT>(name, "", returnObj,
                                                                                        addrImportTaskObjMap);
}
} // namespace ubse::mem::controller
