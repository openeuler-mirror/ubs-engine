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

#ifndef UBSE_MEM_AGENT_TASK_MANAGER_H
#define UBSE_MEM_AGENT_TASK_MANAGER_H

#include <map>

#include "ubse_common_def.h"
#include "ubse_error.h"
#include "ubse_mem_controller_def.h"
#include "ubse_mmi_interface.h"

namespace ubse::mem::controller {
using namespace ubse::common::def;
using namespace ubse::adapter_plugins::mmi;

enum class MemResourceType {
    FD_EXPORT,
    NUMA_EXPORT,
    SHARE_EXPORT,
    ADDR_EXPORT,
    FD_IMPORT,
    NUMA_IMPORT,
    SHARE_IMPORT,
    ADDR_IMPORT
};

class UbseMemAgentTaskManager {
public:
    static uint32_t GenerateTaskId();

    /* taskId必须通过函数生成 */
    template <typename T> static UbseResult AddTaskObj(uint32_t taskId, const T &obj);

    static UbseResult DeleteTaskObj(uint32_t taskId);

    template <typename T>
    static UbseResult GetTaskObj(const std::string &name, const std::string &importNodeId, T &returnObj);

    static uint32_t GetTaskId();

private:
    static UbseResult AddFdExportTask(uint32_t taskId, const UbseMemFdBorrowExportObj &obj);

    static UbseResult AddNumaExportTask(uint32_t taskId, const UbseMemNumaBorrowExportObj &obj);

    static UbseResult AddShareExportTask(uint32_t taskId, const UbseMemShareBorrowExportObj &obj);

    static UbseResult AddAddrExportTask(uint32_t taskId, const UbseMemAddrBorrowExportObj &obj);

    static UbseResult AddFdImportTask(uint32_t taskId, const UbseMemFdBorrowImportObj &obj);

    static UbseResult AddNumaImportTask(uint32_t taskId, const UbseMemNumaBorrowImportObj &obj);

    static UbseResult AddShareImportTask(uint32_t taskId, const UbseMemShareBorrowImportObj &obj);

    static UbseResult AddAddrImportTask(uint32_t taskId, const UbseMemAddrBorrowImportObj &obj);

    static bool DeleteFdExportTask(uint32_t delTaskId);

    static bool DeleteNumaExportTask(uint32_t delTaskId);

    static bool DeleteShareExportTask(uint32_t delTaskId);

    static bool DeleteAddrExportTask(uint32_t delTaskId);

    static bool DeleteFdImportTask(uint32_t delTaskId);

    static bool DeleteNumaImportTask(uint32_t delTaskId);

    static bool DeleteShareImportTask(uint32_t delTaskId);

    static bool DeleteAddrImportTask(uint32_t delTaskId);

    static UbseResult GetFdExportTaskObj(const std::string &name, const std::string &importNodeId,
        UbseMemFdBorrowExportObj &returnObj);

    static UbseResult GetNumaExportTaskObj(const std::string &name, const std::string &importNodeId,
        UbseMemNumaBorrowExportObj &returnObj);

    static UbseResult GetShareExportTaskObj(const std::string &name, UbseMemShareBorrowExportObj &returnObj);

    static UbseResult GetAddrExportTaskObj(const std::string &name, const std::string &importNodeId,
        UbseMemAddrBorrowExportObj &returnObj);

    static UbseResult GetFdImportTaskObj(const std::string &name, UbseMemFdBorrowImportObj &returnObj);

    static UbseResult GetNumaImportTaskObj(const std::string &name, UbseMemNumaBorrowImportObj &returnObj);

    static UbseResult GetShareImportTaskObj(const std::string &name, UbseMemShareBorrowImportObj &returnObj);

    static UbseResult GetAddrImportTaskObj(const std::string &name, UbseMemAddrBorrowImportObj &returnObj);

    static std::map<std::string, std::map<uint32_t, UbseMemFdBorrowExportObj>> fdExportTaskObjMap;
    static std::map<std::string, std::map<uint32_t, UbseMemNumaBorrowExportObj>> numaExportTaskObjMap;
    static std::map<std::string, std::map<uint32_t, UbseMemShareBorrowExportObj>> shareExportTaskObjMap;
    static std::map<std::string, std::map<uint32_t, UbseMemAddrBorrowExportObj>> addrExportTaskObjMap;
    static std::map<std::string, std::map<uint32_t, UbseMemFdBorrowImportObj>> fdImportTaskObjMap;
    static std::map<std::string, std::map<uint32_t, UbseMemNumaBorrowImportObj>> numaImportTaskObjMap;
    static std::map<std::string, std::map<uint32_t, UbseMemShareBorrowImportObj>> shareImportTaskObjMap;
    static std::map<std::string, std::map<uint32_t, UbseMemAddrBorrowImportObj>> addrImportTaskObjMap;
    static uint32_t taskId;
};

template <typename T> UbseResult UbseMemAgentTaskManager::AddTaskObj(uint32_t taskId, const T &obj)
{
    if constexpr (std::is_same_v<T, UbseMemFdBorrowExportObj>) {
        return AddFdExportTask(taskId, obj);
    }
    if constexpr (std::is_same_v<T, UbseMemNumaBorrowExportObj>) {
        return AddNumaExportTask(taskId, obj);
    }
    if constexpr (std::is_same_v<T, UbseMemShareBorrowExportObj>) {
        return AddShareExportTask(taskId, obj);
    }
    if constexpr (std::is_same_v<T, UbseMemAddrBorrowExportObj>) {
        return AddAddrExportTask(taskId, obj);
    }
    if constexpr (std::is_same_v<T, UbseMemFdBorrowImportObj>) {
        return AddFdImportTask(taskId, obj);
    }
    if constexpr (std::is_same_v<T, UbseMemNumaBorrowImportObj>) {
        return AddNumaImportTask(taskId, obj);
    }
    if constexpr (std::is_same_v<T, UbseMemShareBorrowImportObj>) {
        return AddShareImportTask(taskId, obj);
    }
    if constexpr (std::is_same_v<T, UbseMemAddrBorrowImportObj>) {
        return AddAddrImportTask(taskId, obj);
    }
    return UBSE_ERROR;
}

template <typename T>
UbseResult UbseMemAgentTaskManager::GetTaskObj(const std::string &name, const std::string &importNodeId, T &returnObj)
{
    if constexpr (std::is_same_v<T, UbseMemFdBorrowExportObj>) {
        return GetFdExportTaskObj(name, importNodeId, returnObj);
    }
    if constexpr (std::is_same_v<T, UbseMemNumaBorrowExportObj>) {
        return GetNumaExportTaskObj(name, importNodeId, returnObj);
    }
    if constexpr (std::is_same_v<T, UbseMemShareBorrowExportObj>) {
        return GetShareExportTaskObj(name, returnObj);
    }
    if constexpr (std::is_same_v<T, UbseMemAddrBorrowExportObj>) {
        return GetAddrExportTaskObj(name, importNodeId, returnObj);
    }
    if constexpr (std::is_same_v<T, UbseMemFdBorrowImportObj>) {
        return GetFdImportTaskObj(name, returnObj);
    }
    if constexpr (std::is_same_v<T, UbseMemNumaBorrowImportObj>) {
        return GetNumaImportTaskObj(name, returnObj);
    }
    if constexpr (std::is_same_v<T, UbseMemShareBorrowImportObj>) {
        return GetShareImportTaskObj(name, returnObj);
    }
    if constexpr (std::is_same_v<T, UbseMemAddrBorrowImportObj>) {
        return GetAddrImportTaskObj(name, returnObj);
    }
    return UBSE_ERROR;
}
}

#endif // UBSE_MEM_AGENT_TASK_MANAGER_H
