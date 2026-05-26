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

#ifndef UBSE_MANAGER_UBSE_MEM_CONTROLLER_RAS_HANDLER_H
#define UBSE_MANAGER_UBSE_MEM_CONTROLLER_RAS_HANDLER_H

#include <cstdint>

#include "ubse_mem_controller_def.h"
#include "ubse_mmi_interface.h"
#include "ubse_ras.h"
#include "ubse_thread_pool_module.h"

namespace ubse::mem::controller {
using namespace ubse::ras;
using namespace ubse::common::def;
using namespace ubse::task_executor;

class UbseMemFaultManager {
public:
    static UbseResult InitMemFaultManager();

    static UbseResult DeInitMemFaultManager();

    static UbseResult ReportSingleImportDebt(const std::string& targetNodeId,
                                             const def::ShareHandleInfoVec& shareHandleInfoVec,
                                             const def::NumaHandleInfoVec& numaHandleInfoVec,
                                             const def::FdHandleInfoVec& fdHandleInfoVec);

private:
    static uint32_t PanicRebootFaultEventHandler(std::string& eventId, std::string& eventMessage);

    static uint32_t BmcFaultHandler(ALARM_FAULT_TYPE alarmFaultEvent, const std::string& faultInfo);

    static uint32_t BmcFaultTimerHandler();

    static void BmcFaultAgentsHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);

    static UbseResult MemReportWhenExportNodeOnFault(ALARM_FAULT_TYPE faultType, std::string& faultId);

    static void SingleImportDebtNotifyHandler(const UbseByteBuffer& req, UbseByteBuffer& resp);

    static uint32_t MemFaultHandler(ALARM_FAULT_TYPE alarmFaultEvent, std::string faultInfo);

    static UbseResult SendMemFaultMessageByType(const std::string& memType, uint64_t memId, const std::string& memName,
                                                const adapter_plugins::mmi::UbseUdsInfo& udsInfo,
                                                ubse::adapter_plugins::mmi::UbMemFaultType type);

    static UbseResult CreateTaskExecutor(const std::string& name);

    static UbseResult RemoveTaskExecutor(const std::string& name);

    static UbseTaskExecutorPtr executorPtr;
};

} // namespace ubse::mem::controller

#endif // UBSE_MANAGER_UBSE_MEM_CONTROLLER_RAS_HANDLER_H